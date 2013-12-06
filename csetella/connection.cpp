// Connection
#include <stdlib.h>
#include <stdio.h>
#include <memory>
#include "connection.h"
#include "message.h"
#include "sock.h"
#include "log.h"

Connection::Connection(std::unique_ptr<ConnectionSocket> &pSocket)
   : _pConSock(nullptr)
   , _messagesSent(0)
   , _messagesRecv(0)
{
   _pConSock.swap(pSocket);
}

Connection::~Connection()
{

}

const ConnectionSocket* Connection::getSocket() const
{
   return _pConSock.get();
}

unsigned int Connection::getWriteQueueSize() const
{
   return _writeQueue.size();
}

void Connection::read(MessageQueue* pMessageQueue){
   unsigned int cch = 0;
   unsigned int count = 0;
   do {
      // read the messages and put any completed messages on the message queue. 
      if (_pReadMessage.get() == nullptr) {
         _pReadMessage = std::shared_ptr<Message>(new Message);
         _pReadMessage->setConnection(this);
      }

      cch = _pConSock->read(_pReadMessage->getCurPos(), _pReadMessage->getCch());

      if (cch > 0) {
         _pReadMessage->updatePos(cch);
          if (_pReadMessage->getMessageState() == MESSAGE_STATE_COMPLETED_READ) {
            g_log.logRecvMsg(this, _pReadMessage.get());
            pMessageQueue->push_back(_pReadMessage);
            ++_messagesRecv;
            _pReadMessage = nullptr;
         }
      }
      ++count;
      if (count >= MAX_READS) {
         break;
      }
   }  while (cch > 0); 

}

void Connection::queueWrite(std::shared_ptr<Message> pMessage)
{

   pMessage->prepareForWriting();
   _writeQueue.push_back(pMessage);
   if (_writeQueue.size() > 75) {
      char ch[128];
      sprintf(ch, "write queue depth: %lu", _writeQueue.size());
      g_log.writeln(ch);
   }
}

void Connection::write(){
   // write messages from the message queue;
   unsigned int cch = 0;
   do {
      if (!_writeQueue.empty()) {
         std::shared_ptr<Message> pMsg = _writeQueue.front();

         if (_writeQueue.size() > 100) {
            // we're 100 messages behind. close the connection
            g_log.err("Not keepng up with writes!");
         }
         else if (pMsg->getCch() == 0) {
            // stuck messag!
            g_log.err("Stuck Message Queue");
            _writeQueue.pop_front();
         }

         cch = _pConSock->write(pMsg->getCurPos(), pMsg->getCch());
         if (cch > 0) {
            pMsg->updatePos(cch);
            if (pMsg->getMessageState() == MESSAGE_STATE_COMPLETED_WRITE) {
               _writeQueue.pop_front();
               ++_messagesSent;
            }
         }
      }
   }
   while (cch > 0 && !_writeQueue.empty()); 
}

void Connection::printStatus(const ReplyMap& replyMap)
{
   unsigned int addr = getSocket()->getAddr();
   unsigned short int port = getSocket()->getPort();
   unsigned char* puch = reinterpret_cast<unsigned char*>(&addr);
   std::string reply;
   auto addrpair = std::make_pair(addr, port);
   if (replyMap.count(addrpair) > 0) {
      reply = replyMap.at(addrpair);
   }
   printf("%3u.%3u.%3u.%3u %5u %10lu %10lu %s\n", puch[0],puch[1],puch[2],puch[3],port, _messagesSent, _messagesRecv, reply.c_str());
}


