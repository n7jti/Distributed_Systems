// message class
#pragma once

#include <stdint.h>
#include <memory>
#include <deque>

class Connection;

class MessageID
{
public:
   MessageID(); // default constructor
   MessageID(const unsigned char* messageID); // regular construcor
   MessageID(const MessageID& other); // copy constructor
   ~MessageID();

   MessageID & operator= (const MessageID& other); // assignement operator
   bool operator<(const MessageID& other ) const; // comparison operator
   bool operator==(const MessageID& other ) const; // logical equal operator
   void copyToChar(unsigned char* pChar) const;
   void copyFromChar(unsigned char* pChar);
   static void setSeed(unsigned int seed);

private:
   uint64_t _id[2];

   static unsigned int _seed;
   static unsigned int _count;
};

#define MESSAGE_HEADER_LENGTH 23
#define MESSAGE_ID_LENGTH 16

enum MessageType
{
   MESSAGE_TYPE_PING = 0,
   MESSAGE_TYPE_PONG = 1,
   MESSAGE_TYPE_QUERY = 2,
   MESSAGE_TYPE_REPLY = 3
};

enum MessageState
{
   MESSAGE_STATE_READING_HEADER,
   MESSAGE_STATE_READING_PAYLOAD,
   MESSAGE_STATE_COMPLETED_READ,
   MESSAGE_STATE_WRITING_HEADER,
   MESSAGE_STATE_WRITING_PAYLOAD,
   MESSAGE_STATE_COMPLETED_WRITE
};


class Message
{
public:
   Message();
   Message(const Message& other); // copy constructor
   ~Message();

   MessageID getMessageId() const;
   void setMessageId(const MessageID& messageID);

   MessageType getMessageType() const;
   void setMessageType(MessageType messageType);

   unsigned char getTTL() const;
   void setTTL(unsigned char ttl);

   unsigned char getHops() const;
   void setHops(unsigned char hops);

   unsigned int getPayloadLength() const;
   void setPayLoadLength(unsigned int payloadLength);

   MessageState getMessageState() const;
   void setMessageState(MessageState state);

   unsigned char* getCurPos();
   unsigned int getCch() const;
   void updatePos(unsigned int cch);

   Connection* getConnection() const;
   void setConnection(Connection* pConnection);

   unsigned short int getPort() const;
   void setPort(unsigned short int port);

   unsigned int getAddr() const;
   void setAddr(unsigned int addr);

   const char* getTextBlock() const;
   void setTextBlock(const char *text, unsigned int cchText);

   void prepareForWriting();

   static const char* _textBlock; // the text block for our replies;
   static unsigned int _cchTextBlock; // the size of the text block

private:
   void updateState();
   void validateHeader();
   void validatePayload();

   unsigned char _pRawHeader[23];
   std::unique_ptr<unsigned char[]> _pRawPayload;
   unsigned char* _pPos; // Read or write possition
   unsigned int _cch; // characters left in the buffer
   MessageState _state;
   Connection *_pConnection; // the connection the message was received on.
};

typedef std::deque<std::shared_ptr<Message> > MessageQueue;
