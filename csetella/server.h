// Server!

#pragma once

#include "message.h"

#include <map>
#include <queue>
#include <set>

typedef std::map<int, std::shared_ptr<Connection>  > ConnectionMap;
typedef std::map<MessageID, Connection* > RoutingMap;
typedef std::map<MessageID, unsigned int> MessageTimeoutMap;
typedef std::queue< std::pair<MessageID, unsigned int> > MessageTimeoutQueue;
typedef std::queue< std::pair<unsigned int, unsigned short int> > AddressQueue;
typedef std::set< std::pair<unsigned int, unsigned short int> > AddressSet;
typedef std::set< std::pair<unsigned int, unsigned short int> > OldAddresses;
typedef std::map< std::pair<unsigned int, unsigned short int>, std::basic_string<char> > ReplyMap;

#include "connection.h"

class Server
{
public:
   Server();
   ~Server();
   void run(char* localAddr, unsigned short int localPort, char* remoteAddr = nullptr, unsigned short int remotePort = 0); // run the server

private:
   void RebuildArray( int* pcapfd, struct pollfd *apfd); // Rebulld the poll array from internal data structures
   void processPing(const MessageID& messageID, std::shared_ptr<Message> pMessage);
   void processPong(const MessageID& messageID, std::shared_ptr<Message> pMessage);
   void processQuery(const MessageID& messageID, std::shared_ptr<Message>  pMessage);
   void processReply(const MessageID& messageID, std::shared_ptr<Message> pMessage);
   void closeConnection(std::shared_ptr<Connection> pCon);
   unsigned int getTime();
   void timeoutMessages();
   void discoverAddress(unsigned int addr, unsigned short int port);
   void processMessages();
   void connect(unsigned int addr, unsigned short int port);
   void ping();
   void connectToNodes();
   void query();
   void reconnect(); // reconnect to the remote adress if we are not connected
   void printStatus();

   ConnectionMap _connectionMap; // map of file descriptors to Connection objects
   MessageQueue _receivedMessages; // a FIFO queue of received messages
   RoutingMap _routingMap; // a map of message ID's to the connectionthey were first seen on. 
   MessageTimeoutMap _timeoutMap; // a map of messsage id's to the last time the message was recieved;
   MessageTimeoutQueue _timeoutQueue; // a queue of <MessageID, unsigned int> pairs that we'll use to check when a message times out
   AddressQueue _addressQueue; // container full of address, port pairs!
   AddressSet _addressSet; 
   OldAddresses _oldAddresses; // addresses not to connect to
   ReplyMap _replyMap; // set of currently visible replies

   unsigned int _localAddr; //local address in network order
   unsigned short int _localPort; 
   unsigned int _remoteAddr;
   unsigned short int _remotePort;
   unsigned int _lastQuery;
   unsigned int _lastPing;
   unsigned int _lastAddress;
   unsigned int _lastOldAddress;
   unsigned int _lastStatus; 
};
