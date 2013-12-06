//
// event base server
//

#include <signal.h>
#include <poll.h>
#include <string.h>

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <sys/time.h>

#include "sock.h"
#include "server.h"
#include "connection.h"
#include "log.h"
#include "output.h"

// discover and connect nodes every 120 seconds
// up this to like 15 minutes when we run for real

#define INITIAL_TTL 5
#define CONNECTED_NODE_COUNT 3
#define OLD_ADDRESS_INTERVAL 900
#define ADDRESS_INTERVAL 60
#define PING_INTERVAL 30
#define QUERY_INTERVAL 60
#define TIMEOUT_INTERVAL 15
#define STATUS_INTERVAL 20

extern int g_continue;

using namespace std;
#define MAXREQUESTS 128

Server::Server() 
   : _localAddr(0)
   , _localPort(0) 
   , _remoteAddr(0)
   , _remotePort(0)
   , _lastQuery(0)
   , _lastPing(0)
   , _lastAddress(0)
   , _lastOldAddress(0)
   , _lastStatus(0)
{

}

Server::~Server()
{

}

void Server::run(char* localAddr, unsigned short int localPort, char* remoteAddr, unsigned short int remotePort)
{
    ListenSocket lsock(localPort, SOCK_NONBLOCK);
    lsock.Listen();

    _localAddr = ConnectionSocketFactory::stringToAddr(localAddr);
    _localPort = localPort;
    MessageID::setSeed( (static_cast<unsigned int>(_localAddr) << 16) | static_cast<unsigned int>(localPort));

    if (remoteAddr != nullptr) {
       _remoteAddr = ConnectionSocketFactory::stringToAddr(remoteAddr);
    }
    _remotePort = remotePort;
    
    struct pollfd apfd[MAXREQUESTS];
    int capfd = 1;
    memset(apfd, 0, sizeof(apfd));

    apfd[0].fd = lsock.GetFd();
    apfd[0].events = POLLIN;

    while(g_continue) {
        // Wait for I/O to be ready
        int cevents = poll(apfd,capfd,250);

        // now cevents tells us how many events there are
        while (cevents > 0) {
            
            // check the listening socket first, but only if we've got more room in our array
            if (apfd[0].revents) {
                if (cevents < MAXREQUESTS) {
                   try {
                       std::unique_ptr<ConnectionSocket> pConSock(lsock.Accept());

                       g_log.write("Incomming Connection: ");
                       g_log.logConAddr(pConSock.get());
                       g_log.writeln("");

                       std::shared_ptr<Connection> pCon(new Connection(pConSock));
                       auto thepair = std::make_pair(pCon->getSocket()->getFd(),  pCon);
                       _connectionMap.insert(thepair);

                       // We've got a new connection, let's issue a ping & to celebrate
                       ping();
                       query();
                   }
                   catch (const char* ch) {
                      //g_log.writeln(ch);
                   }
                }
                cevents--;
            }
            
            // now check the rest
            for(int i = 1; ((i < capfd) && (cevents > 0)); i++) {
                
                // check if we have events received for the file descriptor
                if (apfd[i].revents) {
                    // we found one, decrement the count of events.  This allows for early termination of the loop
                    cevents--;
                    std::shared_ptr<Connection> pCon = _connectionMap[apfd[i].fd];
                    if (apfd[i].revents & POLLIN) {
                       //Read!
                       try {
                          pCon->read(&_receivedMessages);
                       }
                       catch (const char* ch) {
                          //g_log.writeln(ch);
                          closeConnection(pCon);
                       }
                    }

                    if (apfd[i].revents & POLLOUT) {
                       // Write!
                       try {
                          pCon->write();
                       }
                       catch (const char* ch) {
                          //g_log.writeln(ch);
                          closeConnection(pCon);
                       }
                    }
                }
            }
        }
     
        processMessages();

        timeoutMessages();

        if (_connectionMap.empty()) {
           reconnect();
        }

        connectToNodes();

         // bootstrap!
         {
            unsigned int time = getTime();

            if (time - _lastPing > PING_INTERVAL) {
               ping();
               _lastPing = time;
            }

            if (time - _lastQuery > QUERY_INTERVAL) {
               _replyMap.clear();
               query();
               _lastQuery = time;
            }

            if (time - _lastAddress > ADDRESS_INTERVAL) 
            {
               while (!_addressQueue.empty()) {
                  _addressQueue.pop();
               }
               _addressSet.clear();
               _lastAddress = time;

               // reconnect to remote address if it isn't in there:
               reconnect();

            }

            if (time - _lastOldAddress > OLD_ADDRESS_INTERVAL) {
              _oldAddresses.clear();

              // add ourselves
              _oldAddresses.insert(std::make_pair(_localAddr, _localPort));

               // Add all of the existing connections
               for (auto it = _connectionMap.begin(); it != _connectionMap.end(); ++it) {
                  unsigned int addr = it->second->getSocket()->getAddr();
                  unsigned short int port = it->second->getSocket()->getPort();
                  _oldAddresses.insert(std::make_pair(addr, port));
               }

              _lastOldAddress = time;
            }

            if (time - _lastStatus > STATUS_INTERVAL) {
               printStatus();
               _lastStatus = time;
            }

        }

        // now rebuild the array and go aroun again
        RebuildArray(&capfd, apfd);
    }
}

void Server::RebuildArray(int* pcapfd, struct pollfd *apfd)
{
   apfd[0].revents = 0;

    // look up the file descriptor in the map
    auto mIter = _connectionMap.begin();
    int cRequests = 1;
    while (mIter != _connectionMap.end()) {
        apfd[cRequests].fd = mIter->second->getSocket()->getFd();
        apfd[cRequests].revents = 0;

        // inidcate what events we are looking for.  We are always ready for input.  If there is
        // a write queue we're looking to write our output as well. 
        unsigned short int events = POLLIN;
        if (mIter->second->getWriteQueueSize() > 0) {
           events |= POLLOUT;
        }
        apfd[cRequests].events = events;
        ++mIter;
        ++cRequests;
    }
    *pcapfd = cRequests; 
}

void Server::processMessages()
{
   while (!_receivedMessages.empty()) {
      std::shared_ptr<Message> pMessage = _receivedMessages.front();
      unsigned int time = getTime();
      MessageID id(pMessage->getMessageId());

      _timeoutMap.insert(std::make_pair(id, time));
      _timeoutQueue.push(std::make_pair(id, time));

      switch (pMessage->getMessageType()) {
      case MESSAGE_TYPE_PING:
         processPing(id, pMessage);
         break;
      case MESSAGE_TYPE_PONG:
         processPong(id, pMessage);
         break;
      case MESSAGE_TYPE_QUERY:
         processQuery(id, pMessage);
         break;
      case MESSAGE_TYPE_REPLY:
         processReply(id, pMessage);
         break;
      default:
         g_log.err("Unrecognized Message Type");
      }

      _receivedMessages.pop_front();
   }

}

void Server::processPing(const MessageID& messageID, std::shared_ptr<Message> pMessage)
{
   unsigned int count = _routingMap.count(messageID);
   if (count == 0) {
      // First time we've seen this mesage, note it in the messageMap so we know where to send the replies
      _routingMap.insert(std::make_pair(messageID, pMessage->getConnection()));
      
      //Now forward pings to all the other connections, if there is anything left in the TTL
      if (pMessage->getTTL() > 0) {
         for (auto iter = _connectionMap.begin(); iter != _connectionMap.end(); ++iter) {
            Connection* pCon = iter->second.get();
            if (pCon != pMessage->getConnection()) {
               std::shared_ptr<Message> pNextMessage(new Message(*pMessage));
               // Update TTL and Hops
               pNextMessage->setTTL(pNextMessage->getTTL()-1);
               pNextMessage->setHops(pNextMessage->getHops()+1);
               
               // Queue the message to write
               g_log.logSendMsg(pCon, pNextMessage.get());
               pCon->queueWrite(pNextMessage);
            }
         }
      }

      std::shared_ptr<Message> pPongMessage(new Message);
      pPongMessage->setMessageId(pMessage->getMessageId());
      pPongMessage->setMessageType(MESSAGE_TYPE_PONG);
      pPongMessage->setTTL(pMessage->getHops() + pMessage->getTTL());
      pPongMessage->setPayLoadLength(6);
      pPongMessage->setPort(_localPort);
      pPongMessage->setAddr(_localAddr);

      auto pCon = pMessage->getConnection();
      // queue the PONG reply
      g_log.logSendMsg(pCon, pPongMessage.get());
      pCon->queueWrite(pPongMessage);
         
   }
   else {
      g_log.write("Ignoring: ");
      g_log.logRecvMsg(pMessage->getConnection(), pMessage.get());
   }
}

void Server::processPong(const MessageID& messageID, std::shared_ptr<Message> pMessage)
{
   discoverAddress(pMessage->getAddr(), pMessage->getPort());
   if (_routingMap.count(messageID) != 0) {
      auto pCon = _routingMap.at(messageID);
      if (pCon != nullptr) {
         // We have routing info for this message!
         if (pMessage->getTTL() > 0) {
            std::shared_ptr<Message> pNextMessage(new Message(*pMessage));
            pNextMessage->setTTL(pMessage->getTTL()-1);
            pNextMessage->setHops(pMessage->getHops()+1);
            g_log.logSendMsg(pCon, pNextMessage.get());
            pCon->queueWrite(pNextMessage);
         }
      }
   }
   else {
      g_log.writeln("No routing information for this pong!");
   }


}

void Server::processQuery(const MessageID& messageID, std::shared_ptr<Message> pMessage)
{
   if (_routingMap.count(messageID) == 0) {
      // First time we've seen this mesage, note it in the messageMap so we know where to send the replies
      _routingMap.insert(std::make_pair(messageID, pMessage->getConnection()));

      //Now forward Query to all the other connections, if there is anything left in the TTL
      if (pMessage->getTTL() > 0) {

         for (auto iter = _connectionMap.begin(); iter != _connectionMap.end(); ++iter) {
            Connection* pCon = iter->second.get();
            if (pCon != pMessage->getConnection()) {
               std::shared_ptr<Message> pNextMessage(new Message(*pMessage));
               // Update TTL and Hops
               pNextMessage->setTTL(pNextMessage->getTTL()-1);
               pNextMessage->setHops(pNextMessage->getHops()+1);
               
               // Queue the message to write
               g_log.logSendMsg(pCon, pNextMessage.get());
               pCon->queueWrite(pNextMessage);
            }
         }
      }

      std::shared_ptr<Message> pReplyMessage(new Message);
      pReplyMessage->setMessageId(pMessage->getMessageId());
      pReplyMessage->setMessageType(MESSAGE_TYPE_REPLY);
      pReplyMessage->setTTL(pMessage->getHops() + pMessage->getTTL());
      pReplyMessage->setPayLoadLength(6 + Message::_cchTextBlock);
      pReplyMessage->setPort(_localPort);
      pReplyMessage->setAddr(_localAddr);
      pReplyMessage->setTextBlock(Message::_textBlock, Message::_cchTextBlock);

      // queue the REPLY
      auto pCon = pMessage->getConnection();
      g_log.logSendMsg(pCon, pReplyMessage.get());
      pCon->queueWrite(pReplyMessage);
         
   }
   else {
      g_log.write("Ignoring: ");
      g_log.logRecvMsg(pMessage->getConnection(), pMessage.get());
   }
}

void Server::processReply(const MessageID& messageID, std::shared_ptr<Message> pMessage)
{
   g_output.addReply(pMessage->getTextBlock(), pMessage->getPayloadLength() - 6);
   std::string sreply(pMessage->getTextBlock(), pMessage->getPayloadLength()- 6);

   _replyMap.insert(std::make_pair( std::make_pair(pMessage->getAddr(), pMessage->getPort()), sreply ));

   if (_routingMap.count(messageID) != 0) {
      auto pCon = _routingMap.at(messageID);
      if (pCon != nullptr) {
         if (pMessage->getTTL() > 0){
            std::shared_ptr<Message> pNextMessage(new Message(*pMessage));
            pNextMessage->setTTL(pMessage->getTTL()-1);
            pNextMessage->setHops(pMessage->getHops()+1);
            g_log.logSendMsg(pCon, pNextMessage.get());
            pCon->queueWrite(pNextMessage);
         }
      }
   }
   else {
      g_log.writeln("No routing information for this reply!");
   }

}

void Server::connect(unsigned int addr, unsigned short int port)
{
   char buf[128];
   unsigned char* puch = reinterpret_cast<unsigned char*>(&addr);
   sprintf(buf,"Connecting To:%u.%u.%u.%u %u\n", puch[0],puch[1],puch[2],puch[3], port);
   g_log.write(buf);

   try {
      std::unique_ptr<ConnectionSocket> pConSock(ConnectionSocketFactory::connect(addr, port, SOCK_NONBLOCK));
      std::shared_ptr<Connection> pCon(new Connection(pConSock));
      _connectionMap.insert(std::make_pair(pCon->getSocket()->getFd(), pCon));
      _oldAddresses.insert(std::make_pair(pCon->getSocket()->getAddr(), pCon->getSocket()->getPort()));
      pConSock = nullptr;
   }
   catch (const char* ch) {
      g_log.writeln(ch);
   }
}

void Server::ping()
{
   // Use a single message id for all packets in the ping.  
   std::shared_ptr<Message> pMsg(new Message());
   pMsg->setMessageType(MESSAGE_TYPE_PING);
   pMsg->setTTL(INITIAL_TTL);

   // Send the ping on all connected nodes
   for (auto it = _connectionMap.begin(); it != _connectionMap.end(); ++it) {
      //Queue a Ping
      std::shared_ptr<Message> pPing(new Message(*pMsg));
      pPing->setMessageType(MESSAGE_TYPE_PING);   

      g_log.logSendMsg(it->second.get(), pPing.get());
      _routingMap.insert(std::make_pair(pPing->getMessageId(), nullptr));
      it->second->queueWrite(pPing);
   }
}

void Server::closeConnection(std::shared_ptr<Connection> pCon)
{
   // Remove the connection from the connection map
   int fd = pCon->getSocket()->getFd();  // The FD associated with this socket;
   _connectionMap.erase(fd);

   // Remove the associated message to connection entries
   // Dump them in a set and detete them after we're done iterating
   std::set<MessageID> messageSet;
   for (auto it = _routingMap.begin(); it != _routingMap.end(); ++it) {
      if (it->second == pCon.get()) 
      {
         // We have a winner!
         messageSet.insert(it->first);
      }
   }

   // Now iterate the set and delete all the messages
   for (auto it = messageSet.begin(); it != messageSet.end(); ++it) {
      // Remove it from the message map
      _routingMap.erase(*it);
   }

   // Remove it from the _recievedMessageQueue
   for (auto it2 = _receivedMessages.begin(); it2 != _receivedMessages.end(); ) {

      if( messageSet.count((*it2)->getMessageId()) > 0 ) {
         it2 = _receivedMessages.erase(it2);
      }
      else if ( (*it2)->getConnection() == pCon.get() ) {
         it2 =  _receivedMessages.erase(it2);
      }
      else {
         ++it2;
      }

   }
   

   // remove the connection from the map
   _connectionMap.erase(pCon->getSocket()->getFd());

   // add the address to the "old addresses" to age out. 
   _oldAddresses.insert(std::make_pair(pCon->getSocket()->getAddr(), pCon->getSocket()->getPort()));

}

unsigned int Server::getTime(){
   struct timeval tv = {0};
   int ret = gettimeofday(&tv, nullptr);
   if (ret < 0) {
      g_log.err("Failed to get time!");
   }
   return static_cast<unsigned int>(tv.tv_sec);
}

void Server::timeoutMessages(){
   // We're going to timeout messages after two minutes
   unsigned int now = getTime();
   while (!_timeoutQueue.empty()){
      MessageID id(_timeoutQueue.front().first);
      unsigned int time = _timeoutQueue.front().second;
      if (now - time > TIMEOUT_INTERVAL) {
         // This entry in the queue is expired.  
         _timeoutQueue.pop();

         // Now, Check to see if this was the last time we've seen this timestamp
         if (_timeoutMap.count(id) > 0) {
            unsigned int lastTime = _timeoutMap.at(id);
            if (lastTime <= time) {
               // Yup, we haven't recieved this messageId in the last two minutes
               _timeoutMap.erase(id);
               _routingMap.erase(id);
            }
         }
         else
         {
            // for some reason, this message isn't in our map, so go ahead and delete the routing info
            _routingMap.erase(id);
         }
      }
      else{
         // Nothing to timeout!
         break;
      }
   }
}

void Server::discoverAddress(unsigned int addr, unsigned short int port)
{
   auto thepair = std::make_pair(addr, port);
   if (_oldAddresses.count(thepair) == 0) {
      if (_addressSet.count( thepair ) == 0)
      {
         _addressQueue.push( thepair );
         _addressSet.insert( thepair );

      }
   }   
}

void Server::connectToNodes(){
   // We want to connect to our CONNECTED_NODE_COUNT, but we'll preserve the connections that we've got
   if ( (_connectionMap.size() < CONNECTED_NODE_COUNT) && (_addressQueue.size() > 0) ){

      unsigned int addr = _addressQueue.front().first;
      unsigned short int port = _addressQueue.front().second;

      try {
         connect(addr, port);
      }
      catch (const char* ch) {
         g_log.writeln(ch);
      }
      _oldAddresses.insert(std::make_pair(addr,port));
      _addressSet.erase(_addressQueue.front());
      _addressQueue.pop();
      
   }
}

void Server::query()
{
   // Use a single message id for all packets in the ping.  
   std::shared_ptr<Message> pMsg(new Message());
   pMsg->setMessageType(MESSAGE_TYPE_QUERY);
   pMsg->setTTL(INITIAL_TTL);

   // Send the ping on all connected nodes
   for (auto it = _connectionMap.begin(); it != _connectionMap.end(); ++it) {
      //Queue the message
      std::shared_ptr<Message> pQuery(new Message(*pMsg));

      g_log.logSendMsg(it->second.get(), pQuery.get());
      _routingMap.insert(std::make_pair(pQuery->getMessageId(), nullptr));
      it->second->queueWrite(pQuery);
   }
}

void Server::reconnect()
{
   bool found = false;
   for (auto it = _connectionMap.begin(); it != _connectionMap.end(); ++it) {
      unsigned int addr = it->second->getSocket()->getAddr();
      unsigned short int port = it->second->getSocket()->getPort();

      if (addr == _remoteAddr && port == _remotePort) {
         found = true;
         break;
      }
   }

   if (!found) {
      if (_remoteAddr != 0) {
        try {
           connect(_remoteAddr, _remotePort);
        }
        catch (const char* ch) {
           g_log.write("Failed To Connect: ");
           char buf[128];
           unsigned char* puch = reinterpret_cast<unsigned char*>(&_remoteAddr);
           sprintf(buf,"%u.%u.%u.%u %u\n", puch[0],puch[1],puch[2],puch[3],_remotePort);
           g_log.write(buf);
        }
      }
   }

}

void Server::printStatus()
{
   printf("\nVisible Nodes: %lu\n", _replyMap.size());
   for (auto it = _replyMap.begin(); it != _replyMap.end(); ++it) {
      unsigned int addr = it->first.first;
      unsigned short int port = it->first.second;
      unsigned char* puch = reinterpret_cast<unsigned char*>(&addr);
      printf("%3u.%3u.%3u.%3u %5u %s\n", puch[0],puch[1],puch[2],puch[3],port, it->second.c_str());
   }

   printf("\nConnections: %lu\n", _connectionMap.size());

   for (auto it = _connectionMap.begin(); it != _connectionMap.end(); ++it) {
      it->second->printStatus(_replyMap);
   }
}

