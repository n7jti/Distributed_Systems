//
// csock.cpp
//

// impementation file for network sockets

#include <unistd.h>     // misc Unix functions
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>

#include <iostream>
#include <string>

#include "sock.h"
#include "log.h"

ConnectionSocket* ConnectionSocketFactory::connect(unsigned int addr, unsigned short int port, int flags)
{
   int sockfd = 0;
   struct sockaddr_in serv_addr;

   sockfd = socket(AF_INET, SOCK_STREAM | flags, 0);
   if (sockfd < 0) {
      g_log.err("Could not create socket");
   }

   memset(&serv_addr, '0', sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_port = htons(port);
   memcpy(&serv_addr.sin_addr, &addr, sizeof(addr));

   if (::connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
       if (errno == EINPROGRESS) {
           struct pollfd pfd;
           pfd.fd = sockfd;
           pfd.events = POLLOUT;
           pfd.revents = 0;
           int cevents = poll(&pfd, 1, 2000);
           if (cevents == 0) {
              // we timed out!
              close(sockfd);
              throw("Timed out connecting");
           }
       }
       else {
          g_log.err("Failed to connect");
       }
   }

   ConnectionSocket *pConSock = nullptr;
   try {
      pConSock = new ConnectionSocket(sockfd, SOCK_STREAM | flags);
   }
   catch (char *ch) {
      close(sockfd);
      throw ch;
   }

   return pConSock;
}

unsigned int ConnectionSocketFactory::stringToAddr(char* addr)
{
   unsigned int out = 0;
   if (inet_pton(AF_INET, addr, &out) <= 0 ){
      g_log.err("Failed to inet_pton");
   }
   return out;
}

// Constructor for a network socket
ConnectionSocket::ConnectionSocket(int conn_s, int flags) :
    _conn_s(conn_s),
    _flags(flags),
    _addr(0),
    _port(0)
{
   // This is ugly, but simpler than the alternative
   union {
       struct sockaddr sa;
       struct sockaddr_in sa4;
       struct sockaddr_storage sas;
   } address;
   socklen_t size = sizeof(address);

   // Assume the file descriptor is in the var 'fd':
   if (getpeername(_conn_s, &address.sa, &size) < 0) {
       int err = errno;
       char buff[128];
       sprintf(buff, "Error getpeername: %u", err);
       g_log.err(buff);
   }

   if (address.sa4.sin_family == AF_INET) {
       // IP address now in address.sa4.sin_addr, port in address.sa4.sin_port
      _addr = address.sa4.sin_addr.s_addr;
      _port = ntohs(address.sa4.sin_port);
   } else {
       g_log.err("Socket Family not recognized");
   }
   
}


// desructor, note it automatically closes the socket
ConnectionSocket::~ConnectionSocket(){
    if (_conn_s > 0) {
        close(_conn_s);
        _conn_s = 0;
    }
}

// read in up to maxline characers from the socket. Use with non-blocking sockets
ssize_t ConnectionSocket::read(void* buffer, size_t maxlen) const {
    ssize_t rc = ::read(_conn_s, buffer, maxlen);
    if(rc < 0) {
        if((errno == EINTR) || 
           errno == EAGAIN) {
            rc = 0;
        }
        else {
            int err = errno;
            char buff[128];
            sprintf(buff, "Error, read failed: %u", err);
            g_log.err(buff);
        }
    }
    return rc;
}

// write out up to maxlen characters.  Use with non-blocking sockets
ssize_t ConnectionSocket::write(const void *vptr, size_t maxlen) const {
   //ssize_t rc = ::write(_conn_s, vptr, maxlen);
   ssize_t rc = ::send(_conn_s, vptr, maxlen, MSG_NOSIGNAL);
   if (rc < 0) {
      if((errno == EINTR) || 
        (errno == EAGAIN)) {
         rc = 0;
      }
      else{
         static char chBuf[128];
         memset(chBuf,0,sizeof(chBuf));
         unsigned int addr = getAddr();
         unsigned short int port = getPort();
         unsigned char* puch = reinterpret_cast<unsigned char*>(&addr);
         sprintf(chBuf,"ERROR: write failed %u %u.%u.%u.%u %u:  ", errno, puch[0],puch[1],puch[2],puch[3], port);
         g_log.write(chBuf);
         g_log.err(chBuf);
      }
   }
   return rc;
}

int ConnectionSocket::getFd() const{
    return _conn_s;
}

unsigned int ConnectionSocket::getAddr() const{
    return _addr;
}

unsigned short int ConnectionSocket::getPort() const{
    return _port;
}


// Constructor for the listener socket
ListenSocket::ListenSocket(unsigned short int port, int flags) :
    _list_s(0),
    _port(port),
    _flags(flags)
{
    memset(&_servaddr, 0, sizeof(_servaddr));
    _servaddr.sin_family = AF_INET;
    _servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    _servaddr.sin_port = htons(_port);
}


// destructor for the listener socket.  Note that the destructor closes the socket
ListenSocket::~ListenSocket(){
    if (_list_s > 0) {
        close(_list_s);
        _list_s = 0;
    }
}

// Bind and listen
void ListenSocket::Listen(){
    int ret = 0;
    const int LISTENQ = 127;

    _list_s = socket(AF_INET, SOCK_STREAM | _flags, 0);
    if (_list_s == -1) {
        g_log.err("ERROR: Could not create listening socket");
    }
    
    ret  = ::bind(_list_s, reinterpret_cast<const sockaddr*>(&_servaddr), sizeof(_servaddr));
    if (ret) {
        g_log.err("ERROR: Failed to bind");
    }

    ret = listen(_list_s, LISTENQ);
    if (ret < 0) {
       g_log.err("ERROR: Could not listen.");
    }

}

// accept the connection and return a connection socket
ConnectionSocket* ListenSocket::Accept(void)
{
    int conn_s;
    ConnectionSocket *pConnectionSocket = nullptr;
    conn_s = accept4(_list_s, NULL, NULL, _flags);
    if(conn_s < 0) {
       g_log.err("ERROR: accept failed");
    }
    else
    {
       try {
          pConnectionSocket = new ConnectionSocket(conn_s);
       }
       catch (const char* ch) {
          close(conn_s);
          throw ch;
       }
    }
    return(pConnectionSocket);
}


// get the file descriptor of the connection socket
int ListenSocket::GetFd(void){
    return _list_s;
}



