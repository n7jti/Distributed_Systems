// Conection.h

#pragma once
#include <memory>
#include "message.h"
#include "server.h"

#define MAX_READS 5

class ConnectionSocket;

class Connection
{
public:
   Connection(std::unique_ptr<ConnectionSocket> &pSocket);
   ~Connection();
   const ConnectionSocket* getSocket() const;
   //void queueMessage(Message *pMessage);
   unsigned int getWriteQueueSize() const;

   void read(MessageQueue* pMessageQueue);
   void queueWrite(std::shared_ptr<Message> pMessage);
   void write();
   void printStatus(const ReplyMap& replyMap);
private:
   Connection(); // delete
   std::unique_ptr<ConnectionSocket> _pConSock;  // the connection socket associated with this connection

   MessageQueue _writeQueue;
   std::shared_ptr<Message> _pReadMessage;
   unsigned long int _messagesSent;
   unsigned long int _messagesRecv;

};
