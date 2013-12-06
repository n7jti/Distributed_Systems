
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include "message.h"
#include "log.h"
#include "connection.h"
#include "sock.h"

unsigned int MessageID::_seed = 0;
unsigned int MessageID::_count = 0;

const char* Message::_textBlock = "Alan Ludwig -- alanlu [at] uw.edu";
unsigned int Message::_cchTextBlock = strlen(Message::_textBlock);

MessageID::MessageID()
{
   uint64_t id[2];

   id[0] = _seed;
   id[1] = _count;

   unsigned char* ch = reinterpret_cast<unsigned char*>(id);
   this->copyFromChar(ch);
   ++_count;
}

// regular constructor
MessageID::MessageID(const unsigned char* messageID)
{
   memcpy(_id, messageID, 16);
}

// copy constructor
MessageID::MessageID(const MessageID& other)
{
   memcpy(_id, other._id, 16);
}

MessageID::~MessageID()
{

}

// assignement operator
MessageID & MessageID::operator= (const MessageID& other)
{
   memcpy(_id, other._id, 16);

   return *this;
}

// comparison operator
bool MessageID::operator<(const MessageID& other ) const
{
   return (memcmp(_id, other._id, sizeof(_id)) < 0);
}

bool MessageID::operator==(const MessageID& other ) const
{
   return (memcmp(_id, other._id, sizeof(_id)) == 0);
}

void MessageID::copyToChar(unsigned char* pChar) const
{
   memcpy(pChar, _id, sizeof(_id));
}

void MessageID::copyFromChar(unsigned char* pChar)
{
   memcpy(_id, pChar, sizeof(_id));
}

void MessageID::setSeed(unsigned int seed)
{
   _seed = seed;
   /* initialize random seed: */
   srand (time(NULL));
   _count = rand();
}

Message::Message()
   : _pPos (_pRawHeader)
   , _cch(sizeof(_pRawHeader))
   , _state(MESSAGE_STATE_READING_HEADER)
   , _pConnection(nullptr)
{
   memset(_pRawHeader,0, sizeof(_pRawHeader));
   setMessageId(MessageID());
}

Message::Message(const Message& other)
   : _pPos(nullptr)
   , _cch(other._cch)
   , _state(other._state)
   , _pConnection(other._pConnection)
{
   // copy over the header
   memcpy(_pRawHeader, other._pRawHeader, 23);

   // Allocate space for the payload
   unsigned int payloadLength = getPayloadLength();
   if (payloadLength > 0) {
      setPayLoadLength(payloadLength);

      // copy over the payload
      memcpy(_pRawPayload.get(), other._pRawPayload.get(), payloadLength);
   }

   if (_state == MESSAGE_STATE_READING_HEADER) {
      _pPos = _pRawHeader + (sizeof(_pRawHeader) - _cch);
   }
   else if (_state == MESSAGE_STATE_READING_PAYLOAD) {
      _pPos = _pRawPayload.get() + (payloadLength - _cch);
   }

}

Message::~Message()
{

}

MessageID Message::getMessageId() const
{
   return MessageID(static_cast<const unsigned char*>(_pRawHeader));
}

void Message::setMessageId(const MessageID& messageID)
{
   messageID.copyToChar(_pRawHeader);
}

MessageType Message::getMessageType() const
{
   return static_cast<MessageType>(_pRawHeader[16]);
}


void Message::setMessageType(MessageType messageType)
{
   _pRawHeader[16] = messageType;
}

unsigned char Message::getTTL() const
{
   return static_cast<unsigned char>(_pRawHeader[17]);
}

void Message::setTTL(unsigned char ttl)
{
   _pRawHeader[17] = ttl;
}

unsigned char Message::getHops() const
{
   return static_cast<unsigned char>(_pRawHeader[18]);
}

void Message::setHops(unsigned char hops)
{
   _pRawHeader[18] = hops;
}

unsigned int Message::getPayloadLength() const
{
   return ntohl(*reinterpret_cast<const unsigned int *>(&_pRawHeader[19]));
}


void Message::setPayLoadLength(unsigned int payloadLength)
{
   *reinterpret_cast<unsigned int*>(&_pRawHeader[19]) = htonl(payloadLength);
   std::unique_ptr<unsigned char[]> payload(new unsigned char[payloadLength]);
   memset(payload.get(), 0, payloadLength);
   _pRawPayload.swap(payload);
}

MessageState Message::getMessageState() const
{
   return _state;
}

void Message::setMessageState(MessageState state)
{
   _state = state;
}

unsigned char* Message::getCurPos()
{
   return _pPos;
}

unsigned int Message::getCch() const
{
   return _cch;
}

void Message::updatePos(unsigned int cch)
{
   _pPos += cch;
   _cch -= cch;
   updateState();
}

void Message::updateState()
{
   switch (_state) {
   case MESSAGE_STATE_READING_HEADER:
      if (_cch == 0) {
         validateHeader();
         unsigned int payloadLen = getPayloadLength();
         if (payloadLen > 0) {
            setPayLoadLength(payloadLen);
            _cch = payloadLen;
            _pPos = _pRawPayload.get();
            _state = MESSAGE_STATE_READING_PAYLOAD;
         }
         else{
            // _cch is already zero
            _pPos = nullptr; // defensively set to null
            _state = MESSAGE_STATE_COMPLETED_READ;
         }
      }
      break;
   case MESSAGE_STATE_READING_PAYLOAD:
      if (_cch == 0) {
         // _cch is already zero
         _pPos = nullptr; 
         _state = MESSAGE_STATE_COMPLETED_READ;
      }
      break;
   case MESSAGE_STATE_COMPLETED_READ:
      break;
   case MESSAGE_STATE_WRITING_HEADER:
      if (_cch == 0) {
         unsigned int payloadLen = getPayloadLength();
         if (payloadLen > 0) {
            _cch = payloadLen;
            _pPos = _pRawPayload.get();
            _state = MESSAGE_STATE_WRITING_PAYLOAD;
         }
         else{
            validatePayload();
            _state = MESSAGE_STATE_COMPLETED_WRITE;
         }
      }
      break;
   case MESSAGE_STATE_WRITING_PAYLOAD:
      if (_cch == 0) {
         _pPos = nullptr;
         _state = MESSAGE_STATE_COMPLETED_WRITE;
      }
      break;
   case MESSAGE_STATE_COMPLETED_WRITE:
      break;
   default:
      g_log.err("State Not Recognized!");
   }
}

Connection* Message::getConnection() const
{
   return _pConnection;
}

void Message::setConnection(Connection *pConnection)
{
   _pConnection = pConnection;
}

unsigned short int Message::getPort() const
{
   return ntohs(*reinterpret_cast<unsigned short int*>(_pRawPayload.get()));
}

void Message::setPort(unsigned short int port)
{
   *reinterpret_cast<unsigned short int*>(_pRawPayload.get()) = htons(port);
}

unsigned int Message::getAddr() const
{
   return *reinterpret_cast<unsigned int*>(_pRawPayload.get() + 2);
}

void Message::setAddr(unsigned int addr)
{
   *reinterpret_cast<unsigned int*>(_pRawPayload.get() + 2) = addr;
}

void Message::prepareForWriting()
{
   _cch = sizeof(_pRawHeader);
   _pPos = _pRawHeader;
   _state = MESSAGE_STATE_WRITING_HEADER;

}

const char* Message::getTextBlock() const
{
   return reinterpret_cast<char*>(_pRawPayload.get() + 6);
}

void Message::setTextBlock(const char *text, unsigned int cchText)
{
   if (cchText < getPayloadLength() - 6) {
      g_log.err("Too Much Text");
   }
   memcpy(_pRawPayload.get() + 6, text, std::min(cchText, getPayloadLength() - 6));
}

void Message::validateHeader()
{

   unsigned int addr = 0;
   unsigned short int port= 0;

   if (_pConnection != nullptr) {
      addr = _pConnection->getSocket()->getAddr();
      port = _pConnection->getSocket()->getPort();
   }
   unsigned char* puch = reinterpret_cast<unsigned char*>(&addr);

   static char buf[128];
   if(getMessageType() > MESSAGE_TYPE_REPLY) {
      sprintf(buf, "Invalid Message Header: Unrecognized Message Type - %u %3u.%3u.%3u.%3u %5u", getMessageType(),puch[0],puch[1],puch[2],puch[3],port);
      g_log.err(buf);
   }

   if (getTTL() > 50) {
      sprintf(buf, "Invalid Message Header: TTL over 50 - %u %3u.%3u.%3u.%3u %5u", getTTL(), puch[0],puch[1],puch[2],puch[3],port);
      g_log.err(buf);
   }

   if (getHops() > 50) {
      sprintf(buf, "Invalid Message Header: Hops over 50 - %u %3u.%3u.%3u.%3u %5u", getHops(), puch[0],puch[1],puch[2],puch[3],port);
      g_log.err(buf);
   }

   if (getPayloadLength() > 1000) {
      // payload longer than 4k
      sprintf(buf, "Invalid Message Header: Payload length over 1k - %u %3u.%3u.%3u.%3u %5u", getPayloadLength(), puch[0],puch[1],puch[2],puch[3],port);
      g_log.err(buf);
   }

   if (getMessageType() == MESSAGE_TYPE_PONG && getPayloadLength() != 6) {
      // incorrect payload size
      sprintf(buf, "Invalid payload length for PONG: - %u %3u.%3u.%3u.%3u %5u", getPayloadLength(), puch[0],puch[1],puch[2],puch[3],port);
      g_log.err(buf);
   }

   if (getMessageType() == MESSAGE_TYPE_REPLY && getPayloadLength() <= 6) {
      // incorrect payload size
      sprintf(buf, "Invalid payload length for REPLY: - %u %3u.%3u.%3u.%3u %5u", getPayloadLength(), puch[0],puch[1],puch[2],puch[3],port);
      g_log.err(buf);
   }

}

void Message::validatePayload(){
   unsigned int addr = 0;
   unsigned short int port= 0;

   if (_pConnection != nullptr) {
      const ConnectionSocket *pSocket = _pConnection->getSocket();
      if (pSocket != nullptr) {
         addr = pSocket->getAddr();
         port = pSocket->getPort();
      }
   }

   unsigned char* puch = reinterpret_cast<unsigned char*>(&addr);

   static char buf[128];

   if (getMessageType() == MESSAGE_TYPE_REPLY) {
      const char* pch = getTextBlock();
      unsigned int cch = getPayloadLength() - 6;
      while (cch > 0) {
         if (*pch < 32 || *pch > 126) {
            sprintf(buf, "Invalid payload Unprintable ascii character: %u %3u.%3u.%3u.%3u %5u", *pch, puch[0],puch[1],puch[2],puch[3],port);
            g_log.err(buf);
         }
         ++pch;
         --cch;
      }
   }
}
