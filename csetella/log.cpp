//
// Log
//

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "log.h"
#include "message.h"
#include "sock.h"
#include "connection.h"



Log::Log()
   : _fd(-1)
{
   
}

Log::~Log()
{
   ::close(_fd);
   _fd = -1;
}

void Log::open(const char* filename)
{
   _fd = ::open(filename, O_CREAT | O_APPEND | O_WRONLY, S_IRUSR | S_IWUSR | S_IWGRP | S_IWGRP | S_IROTH | S_IWOTH);
   if (_fd < 0) {
      throw "Failed to create file!";
   }
}

void Log::write(const char* msg)
{
   ssize_t cch = strlen(msg);
   write(msg, cch);
}


#ifdef LOGGING_ENABLED
void Log::write(const char* msg, unsigned int cch)
{
   ssize_t bytesWritten = 0;
#ifdef LOG_TO_STDOUT
   if(::write(1,msg,cch) < 0)
   {
      printf("Failed to write!");
      fflush(stdout);
      throw "Failed to write!";
   }
#endif
   while (bytesWritten < cch) {
      int ret = ::write(_fd, msg + bytesWritten, cch-bytesWritten);
      if (ret < 0) {
         printf("Failed to Write: %u",errno);
         throw "Failed to write";
      }
      bytesWritten += ret;
   }

#ifdef LOG_TO_STDOUT
   fsync(_fd);
#endif
}
#else
void Log::write(const char*, unsigned int)
{
 
}
#endif

void Log::writeln(const char* msg)
{
   write(msg);
   write("\n");
}

void Log::writeln(const char *msg, unsigned int cch)
{
   write(msg,cch);
   write("\n");
}

void Log::err(const char *msg)
{
   writeln(msg);
   printf("%s\n", msg);
   throw msg;
}

void Log::logMsg(const Message* pMsg)
{
   char chBuf[128];

   // MessageID
   MessageID id(pMsg->getMessageId());
   uint64_t iMsgId[2];
   id.copyToChar(reinterpret_cast<unsigned char*>(iMsgId));
   sprintf(chBuf,"0x%016lx%016lx ",iMsgId[0],iMsgId[1]);
   write(chBuf);

   // Type
   MessageType mt = pMsg->getMessageType();
   switch (mt) {
   case MESSAGE_TYPE_PING:
      write("PING ");
      break;
   case MESSAGE_TYPE_PONG:
      write("PONG ");
      break;
   case MESSAGE_TYPE_QUERY:
      write("QUERY ");
      break;
   case MESSAGE_TYPE_REPLY:
      write("REPLY ");
      break;
   default:
      write("UNKN ");
   }

   // TTL
   sprintf(chBuf, "%u ", pMsg->getTTL());
   write(chBuf);

   // HOPS
   sprintf(chBuf, "%u ", pMsg->getHops());
   write(chBuf);

   // PayloadLength
   sprintf(chBuf, "%u ", pMsg->getPayloadLength());

   if (mt == MESSAGE_TYPE_PONG || mt == MESSAGE_TYPE_REPLY) {

      sprintf(chBuf,"%u ", pMsg->getPort());
      write(chBuf);

      unsigned int addr = pMsg->getAddr();
      unsigned char* puch = reinterpret_cast<unsigned char*>(&addr);
      sprintf(chBuf,"%u.%u.%u.%u ", puch[0],puch[1],puch[2],puch[3]);
      write(chBuf);

      if (mt == MESSAGE_TYPE_REPLY) {
         write(pMsg->getTextBlock(), pMsg->getPayloadLength() - 6);
      }
   }

   writeln("");
}

void Log::logSendMsg(const Connection *pCon, const Message* pMsg)
{
   write("Send to ");

   char chBuf[128];

   unsigned int addr = pCon->getSocket()->getAddr();
   unsigned short int port = pCon->getSocket()->getPort();
   unsigned char* puch = reinterpret_cast<unsigned char*>(&addr);
   sprintf(chBuf,"%u.%u.%u.%u %u:  ", puch[0],puch[1],puch[2],puch[3], port);
   write(chBuf);

   logMsg(pMsg);
}

void Log::logRecvMsg(const Connection *pCon, const Message* pMsg)
{
   write("Recv from ");

   char chBuf[128];

   unsigned int addr = pCon->getSocket()->getAddr();
   unsigned short int port = pCon->getSocket()->getPort();
   unsigned char* puch = reinterpret_cast<unsigned char*>(&addr);
   sprintf(chBuf,"%u.%u.%u.%u %u:  ", puch[0],puch[1],puch[2],puch[3], port);
   write(chBuf);

   logMsg(pMsg);
}

void Log::logConAddr(ConnectionSocket *pCon)
{
   char buf[128];
   unsigned int addr = pCon->getAddr();
   unsigned short int port = pCon->getPort();
   unsigned char* puch = reinterpret_cast<unsigned char*>(&addr);
   sprintf(buf,"%u.%u.%u.%u %u", puch[0],puch[1],puch[2],puch[3],port);
   g_log.write(buf);
}
