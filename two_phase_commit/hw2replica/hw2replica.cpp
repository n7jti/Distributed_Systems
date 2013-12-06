#include "Replica.h"
#include "Master.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/concurrency/PlatformThreadFactory.h>

#include "db.h"

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::apache::thrift::concurrency;

using boost::shared_ptr;

using namespace  ::tpc;

class ReplicaHandler : virtual public ReplicaIf {
 public:
  ReplicaHandler() {
    // Your initialization goes here
  }

  void prepareSet(const int32_t transactionId, const std::string& key, const std::string& value){
    printf("prepareSet TransactionId:%d Key:%s Value:%s \n",
           transactionId,
           key.c_str(),
           value.c_str());

    db Db;
    int rc = Db.init(_dbname);

    if (rc == SQLITE_OK) {
       rc = Db.replicaPrepareSet(transactionId,key,value);
    }

    if (rc != SQLITE_OK) {
       TpcException ex;
       ex.msg = Db.errmsg();
       ex.x = rc;
       throw ex;
    }
  }

  void commitSet(const int32_t transactionId) {
      printf("commitSet transactionId: %d\n", transactionId);

      db Db;
      int rc = Db.init(_dbname);

      if (rc == SQLITE_OK) {
         rc = Db.commitSet(transactionId);
      }

      if (rc != SQLITE_OK) {
         TpcException ex;
         ex.msg = Db.errmsg();
         ex.x = rc;
         throw ex;
      }
  }

  void prepareDel(const int32_t transactionId, const std::string& key) {
    printf("prepareDel TransactionId:%d Key:%s \n",
           transactionId,
           key.c_str());

    db Db;
    int rc = Db.init(_dbname);

    if (rc == SQLITE_OK) {
       rc = Db.replicaPrepareDel(transactionId,key);
    }

    if (rc != SQLITE_OK) {
       TpcException ex;
       ex.msg = Db.errmsg();
       ex.x = rc;
       throw ex;
    }
  }

  void commitDel(const int32_t transactionId) {
     printf("commitDel transactionId: %d\n", transactionId);

      db Db;
      int rc = Db.init(_dbname);

      if (rc == SQLITE_OK) {
         Db.commitDel(transactionId);
      }

      if (rc != SQLITE_OK) {
         TpcException ex;
         ex.msg = Db.errmsg();
         ex.x = rc;
         throw ex;
      }
  }

  void abort(const int32_t transactionId) {
      db Db;
      int rc = Db.init(_dbname);

      if (rc == SQLITE_OK) {
         Db.abort(transactionId);
      }

      if (rc != SQLITE_OK) {
         TpcException ex;
         ex.msg = Db.errmsg();
         ex.x = rc;
         throw ex;
      }
  }

   static const char* _dbname;

};

const char* ReplicaHandler::_dbname = "uninitialized.sqlite3";

void usage()
{
   printf("Usage: hw2Replica <replica#>\n");
}

class CMaster
{
public:
   CMaster()
       : _socket(new TSocket("localhost", 9091))
       , _transport(new TFramedTransport(_socket))
       , _protocol(new TBinaryProtocol(_transport))
       , _client(_protocol)
   {
      _transport->open();
   }

   ~CMaster()
   {
      _transport->close();
   }

   MasterClient* get()
   {
      return &_client;
   }

private:
   boost::shared_ptr<TSocket> _socket;
   boost::shared_ptr<TTransport> _transport;
   boost::shared_ptr<TProtocol> _protocol;
   MasterClient _client;
};

void recover(int timeStamp)
{
   // scope so that the DB gets destroyed on each loop
   db Db;
   int rc = Db.init(ReplicaHandler::_dbname);
   try {
      CMaster master;
      std::list<int> tidList;
      if (rc == SQLITE_OK) {
         rc = Db.queryUncertain(timeStamp, tidList);
      }

      for (auto i = tidList.begin(); i != tidList.end(); ++i) {
         printf("Query: %d\n", *i);
         master.get()->query(*i);
      }
   }
   catch (apache::thrift::transport::TTransportException ex) {
      printf("Master not found\n");
   }
   catch (TpcException ex)
   {
      printf("Application Exception %d, %s\n", ex.x, ex.msg.c_str());
   }
}


void *TimeoutThreadProc(void* ptr)
{
   while (true) {
      sleep(5);
      recover(db::gettime() - 10);
   }

   return nullptr;
}

int main(int argc, char **argv) {
  pthread_t timeoutThread = {0};
  int port = -1;

  if (argc < 2)
  {
     usage();
     return -1;
  }

  int replicaNumber = atoi(argv[1]);

  switch (replicaNumber) {
  case 1:
     port = 9100;
     ReplicaHandler::_dbname = "replica1.sqlite3";
     break;
  case 2:
     port = 9200;
     ReplicaHandler::_dbname = "replica2.sqlite3";
     break;
  case 3:
     port = 9300;
     ReplicaHandler::_dbname = "replica3.sqlite3";
     break;
  default:
     usage();
     return -1;
  }

  shared_ptr<ReplicaHandler> handler(new ReplicaHandler());
  shared_ptr<TProcessor> processor(new ReplicaProcessor(handler));


  shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
  shared_ptr<TTransportFactory> transportFactory(new TFramedTransportFactory());
  shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

  // using thread pool with maximm 8 threads to handle incomming requests
  shared_ptr<ThreadManager> threadManager = ThreadManager::newSimpleThreadManager(8);
  shared_ptr<PlatformThreadFactory> threadFactory = shared_ptr<PlatformThreadFactory>( new PlatformThreadFactory());
  threadManager->threadFactory(threadFactory);
  threadManager->start();

  TThreadPoolServer server(processor, 
                           serverTransport,
                           transportFactory,
                           protocolFactory, 
                           threadManager);

  {
     db Db;
     Db.init(ReplicaHandler::_dbname);
     Db.replicaSetup();
  }

  int rc = 0;
  rc = pthread_create(&timeoutThread, nullptr, TimeoutThreadProc, nullptr);
  if (0 != rc) {
     fprintf(stderr, "Failed to create Timeout Thread\n");
  }

  if (rc == 0) {
     server.serve();
  }
  return rc;
}



