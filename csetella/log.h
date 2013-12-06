//
// The Log Class
//
class Message;
class ConnectionSocket;
class Connection;

class Log
{

public:
   Log();
   ~Log();
   void open(const char* filename);
   void err(const char *msg);
   void write(const char *msg, unsigned int cchMsg);
   void write(const char *msg);
   void writeln(const char *msg, unsigned int cchMsg);
   void writeln(const char *msg);
   void logSendMsg(const Connection *pCon, const Message* pMsg);
   void logRecvMsg(const Connection *pCon, const Message* pMsg);
   void logConAddr(ConnectionSocket *pCon);

private:
   Log(const Log& other); //delete
   void logMsg(const Message* pMsg);
   int _fd;
};

extern Log g_log;
