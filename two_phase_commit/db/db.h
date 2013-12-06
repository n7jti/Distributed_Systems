#include <string>
#include <sqlite3.h>
#include <sys/time.h>
#include <list>

#define DBERROR_LOCKED 1000



class db
{
public: 
   db();
   ~db();

   int init(const std::string &dbName);
   int exec(const std::string &sql);
   int setup();
   int replicaSetup();
   int get(const std::string &key, std::string &value);
   int prepareSet(const std::string &key, const std::string &value, int &transactionId);
   int commitSet(int transactionId);
   int prepareDel(const std::string &key, int &transactionId);
   int commitDel(int transactionId);
   int abort(int transactionId);
   int rollback();
   int replicaPrepareSet(int transactionId, const std::string &key, const std::string &value);
   int replicaPrepareDel(int transactionId, const std::string &key);
   int queryTransaction(const int transactionId, int &transactionTypeId, int &statusId);
   int queryUncertain(const int, std::list<int> &tidList);

   const char* errmsg() { return _errMsg; };
   static int gettime();

private:
   void copyErrMsg();
   static int callback(void *pDb, int argc, char **argv, char **azColName);
   sqlite3 *_pDb;
   const char* _errMsg;
};
