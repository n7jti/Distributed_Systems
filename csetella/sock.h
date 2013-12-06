///
//
// csock -- RAII sockets class
//

#pragma once

// RAII wrapper for sockets.  

#include <sys/socket.h> // socket definitions
#include <sys/types.h>  // socket types
#include <arpa/inet.h>  // inet (3) funcitons
#include <string>

//
// class Connection Socket
// wraps a network socket connection
class ConnectionSocket;

class ConnectionSocketFactory{
public: 
   static ConnectionSocket* connect(unsigned int addr, unsigned short int port, int flags = 0);
   static unsigned int stringToAddr(char* addr);
};

class ConnectionSocket{
public:
    ConnectionSocket(int conn_s, int flags = 0); // constructor for a connection socket wrapper.  
    ~ConnectionSocket();

    ssize_t read(void* vptr, size_t maxlen) const; //Does a single read up to maxlen chars.
    ssize_t write(const void* vptr, size_t maxlen) const; // Does a single write up to maxlen chars

    int getFd() const; // get the wraped file descriptor
    unsigned int getAddr() const;
    unsigned short int getPort() const;

private:
    int _conn_s; // the wrapped file descriptor
    int _flags; // the construction flags. Used to make it non-blocking
    unsigned int _addr;
    short _port;
};


//
// class CListenSocket
// a network listner socket

class ListenSocket{
public:
    ListenSocket(unsigned short int port, int flags = 0); // constructor.  Passes flags to the create sockets functions
    ~ListenSocket();  
    void Listen(void);  // make the listen call on the underlying socket. Also does the bind
    ConnectionSocket* Accept(void); // Call accept and return a wrapped network socket
    int GetFd(); // get the file descriptor for the socket

private:
    int _list_s; // listening socket
    unsigned short int _port; // port number
    int _flags; // flags passed to socket creation functions
    struct sockaddr_in _servaddr; // socket address structure
    std::string _reply;
};
