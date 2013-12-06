#include "KeyValueStore.h"
#include "Master.h"
#include "Replica.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/concurrency/PlatformThreadFactory.h>

#include <string>
#include <memory>
#include <list>
#include "db.h"
#include "schema.h"

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::apache::thrift::concurrency;

using boost::shared_ptr;

using namespace  ::tpc;

class CReplica
{
public:
   CReplica(int port)
       : _socket(new TSocket("localhost", port))
       , _transport(new TFramedTransport(_socket))
       , _protocol(new TBinaryProtocol(_transport))
       , _client(_protocol)
   {
      _transport->open();
   }

   
   ~CReplica()
   {
      _transport->close();
   }

   ReplicaClient* get()
   {
      return &_client;
   }

private:
   CReplica(); // Delete!
   CReplica(const CReplica&); // Delete!
   boost::shared_ptr<TSocket> _socket;
   boost::shared_ptr<TTransport> _transport;
   boost::shared_ptr<TProtocol> _protocol;
   ReplicaClient _client;
};

class KeyValueStoreHandler : virtual public KeyValueStoreIf {
public:
   static const char* _dbname;
  KeyValueStoreHandler() 
   
  {
    db Db;
    int rc = Db.init(_dbname);
    if (rc != SQLITE_OK) {
       TpcException ex;
       ex.msg = Db.errmsg();
       ex.x = rc;
       throw ex;
    }

    Db.setup();

  }

  void put(const std::string& key, const std::string& value) {
    printf("put Key:'%s' Value:'%s'\n", key.c_str(), value.c_str());
    db Db;
    CReplica replica1(9100);
    CReplica replica2(9200);

    int rc = Db.init(_dbname);
    int transactionId = 0;

    bool commit = true;

    if (rc == SQLITE_OK) {
       rc = Db.prepareSet(key, value, transactionId);
    }

    if (rc == SQLITE_OK) {
       try {
          replica1.get()->prepareSet(transactionId,key,value);
          replica2.get()->prepareSet(transactionId,key,value);
       }
       catch (TpcException ex) {
          commit = false;
       }
    }

    if (rc == SQLITE_OK) {
       if (commit) {
          rc = Db.commitSet(transactionId);
       }
       else{
          rc = Db.abort(transactionId);
       }

    }

    if (rc == SQLITE_OK) {
       if (commit) {
          replica1.get()->commitSet(transactionId);
          replica2.get()->commitSet(transactionId);
       }
       else{
          replica1.get()->abort(transactionId);
          replica2.get()->abort(transactionId);
       }

    }

    if (rc != SQLITE_OK) {
       TpcException ex;
       ex.msg = Db.errmsg();
       ex.x = rc;
       throw ex;
    }

    return;
  }

  void erase(const std::string& key) {
    printf("erase: %s\n", key.c_str());
    db Db; 
    CReplica replica1(9100);
    CReplica replica2(9200);
    int transactionId = 0;
    int rc = Db.init(_dbname);
    bool commit = true;

    if (rc == SQLITE_OK) {
       rc = Db.prepareDel(key, transactionId);
    }

    if (rc == SQLITE_OK) {
       try {
          replica1.get()->prepareDel(transactionId,key);
          replica2.get()->prepareDel(transactionId,key);
       }
       catch (TpcException ex) {
          commit = false;
       }
    }

    if (rc == SQLITE_OK) {
       if (commit) {
          rc = Db.commitDel(transactionId);
       }
       else {
          rc = Db.abort(transactionId);
       }
    }

    if (rc == SQLITE_OK) {
       if (commit) {
          replica1.get()->commitDel(transactionId);
          replica2.get()->commitDel(transactionId);
       }
    }

    if (rc != SQLITE_OK) {
       TpcException ex;
       ex.msg = Db.errmsg();
       ex.x = rc;
       throw ex;
    }
  }

  void get(std::string& _return, const std::string& key) {
    printf("get: %s\n", key.c_str());
    db Db;
    int rc = Db.init(_dbname);

    if (rc == SQLITE_OK) {
       rc = Db.get(key,_return);
    }

    if (rc != SQLITE_OK) {
       TpcException ex;
       ex.msg = Db.errmsg();
       ex.x = rc;
       throw ex;
    }
  }

};

const char* KeyValueStoreHandler::_dbname = "master.sqlite3";

void *KeyValueStoreThreadProc(void* ptr)
{
  int port = 9090;
  shared_ptr<KeyValueStoreHandler> handler(new KeyValueStoreHandler());
  shared_ptr<TProcessor> processor(new KeyValueStoreProcessor(handler));

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
  server.serve();
  return 0;
}

class MasterHandler : virtual public MasterIf {
public:
   MasterHandler() {
      db Db;
      int rc = Db.init(_dbname);
      if (rc != SQLITE_OK) {
         TpcException ex;
         ex.msg = Db.errmsg();
         ex.x = rc;
         throw ex;
      }
  }

   void query(const int32_t transactionId) {
      printf("Query TransactionId: %d\n", transactionId);
      db Db;
      int rc = Db.init(_dbname);
      int transactionTypeId = -1;
      int statusId = -1;
      
      if (rc == SQLITE_OK) {
         rc = Db.queryTransaction(transactionId, transactionTypeId, statusId);
      }



      CReplica replica1(9100);
      CReplica replica2(9200);

      if (rc == SQLITE_OK) {

         if (statusId == -1) {
            // This should never be possible!
            replica1.get()->abort(transactionId);
            replica2.get()->abort(transactionId);
         }
         else if (statusId == 0) {
            // Not yet committed, but in the system. We choose to abort now
            rc = Db.abort(transactionId);
            if (rc == SQLITE_OK) {
               replica1.get()->abort(transactionId);
               replica2.get()->abort(transactionId);
            }
         }
         else{
            // committed!
            switch (transactionTypeId) {
            case -1: // ABORT -- This shouldn't be possibe!
                  replica1.get()->abort(transactionId);
                  replica2.get()->abort(transactionId);
               break;
            case 0: // SET
                  replica1.get()->commitSet(transactionId);
                  replica2.get()->commitSet(transactionId);
               break;
            case 1: // DELETE
                  replica1.get()->commitDel(transactionId);
                  replica2.get()->commitDel(transactionId);
               break;
            }
         }
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

const char* MasterHandler::_dbname = "master.sqlite3";

void *MasterThreadProc(void* ptr)
{
  int port = 9091;
  shared_ptr<MasterHandler> handler(new MasterHandler());
  shared_ptr<TProcessor> processor(new MasterProcessor(handler));

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
  server.serve();

  return 0;
}

void recover(int timeStamp)
{
   // scope so that the DB gets destroyed on each loop
   db Db;
   int rc = Db.init(KeyValueStoreHandler::_dbname);
   try {
      CReplica replica1(9100);
      CReplica replica2(9100);
      std::list<int> tidList;
      if (rc == SQLITE_OK) {
         rc = Db.queryUncertain(timeStamp, tidList);
      }

      for (auto i = tidList.begin(); i != tidList.end(); ++i) {
         printf("Abort: %d\n", *i);
         rc = Db.abort(*i);
         if (rc == SQLITE_OK) {
            replica1.get()->abort(*i);
            replica2.get()->abort(*i);
         }
      }
   }
   catch (apache::thrift::transport::TTransportException ex) {
      printf("Replica not found\n");
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
   pthread_t masterThread = {0};
   pthread_t kvstoreThread = {0};
   pthread_t timeoutThread = {0};

   int rc = 0;
   rc = pthread_create(&masterThread, nullptr, MasterThreadProc, nullptr);
   if (0 != rc) {
      fprintf(stderr, "Failed to create masterThread\n");
   }

   rc = pthread_create(&kvstoreThread, nullptr, KeyValueStoreThreadProc, nullptr);
   if (0 != rc) {
      fprintf(stderr, "Failed to create kvstoreThread\n");
   }

  rc = 0;
  rc = pthread_create(&timeoutThread, nullptr, TimeoutThreadProc, nullptr);
  if (0 != rc) {
     fprintf(stderr, "Failed to create Timeout Thread\n");
  }

   rc = pthread_join(masterThread, NULL);
   if (0 != rc) {
      fprintf(stderr, "Failed to join masterThread\n");
   }

   rc = pthread_join(kvstoreThread, NULL);
   if (0 != rc) {
      fprintf(stderr, "Failed to join masterThread\n");
   }

   rc = pthread_join(timeoutThread, NULL);
   if (0 != rc) {
      fprintf(stderr, "Failed to join timeoutThread\n");
   }

}

