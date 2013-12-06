#include<db.h>
#include<schema.h>
#include<string.h>

using namespace std;

const char* db_lockedMsg = "Key is Locked!";

db::db()
   : _pDb(nullptr)
   , _errMsg(nullptr)
{
}


db::~db()
{
   if (_pDb) {
      sqlite3_close(_pDb);
      _pDb = nullptr;
   }

   if (_errMsg != nullptr) {
      delete[] _errMsg;
      _errMsg = nullptr;
   }
}

int db::init(const string& dbName)
{
   int rc = sqlite3_open(dbName.c_str(), &_pDb);

   if (rc != SQLITE_OK) {
     copyErrMsg();
     fprintf(stderr, "Can't open database: %s\n", _errMsg);
     sqlite3_close(_pDb);
     _pDb = nullptr;
   }

  return rc;
}


int db::callback(void *pdb, int argc, char **argv, char **azColName)
{
   int i;
   for (i=0; i < argc; i++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");

   return 0;
}

int db::exec(const string &sql)
{
   char *zErrMsg = 0;
   int rc = sqlite3_exec(_pDb, sql.c_str(), db::callback, 0, &zErrMsg);
   if ( rc != SQLITE_OK) {
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
   }
   return rc;
}

static void freeText(void* cstr)
{
   delete[] reinterpret_cast<char*>(cstr);
}

int allocTextAndBind(const string& str, sqlite3_stmt *pStatement, int index)
{
   int rc = SQLITE_OK;
   char* pc = new char[str.length() + 1];
   if (pc == nullptr) {
      rc = SQLITE_NOMEM;
   }

   if (rc == SQLITE_OK) {
      strcpy(pc, str.c_str());
      rc = sqlite3_bind_text(
               pStatement,
               index,
               pc,
               str.length(),
               freeText);
   }

   return rc;
}



int db::get(const string &key, string &value)
{
   sqlite3_stmt *pGetStatement = nullptr;
   value.clear();
   
   int rc = sqlite3_prepare_v2(
      _pDb,
      get_sql,
      strlen(get_sql),
      &pGetStatement,
      nullptr);

   if (rc == SQLITE_OK) {
      rc = allocTextAndBind(key,pGetStatement,1);
   }

   bool locked = false;
   if (rc == SQLITE_OK) {
      rc = sqlite3_step(pGetStatement);
      if (rc == SQLITE_ROW) {
         rc = SQLITE_OK;
         const char* pc = reinterpret_cast<const char*>(sqlite3_column_text(pGetStatement, 1));
         if (pc != nullptr) {
            locked = true; 
         }
         else
         {
            pc = reinterpret_cast<const char*>(sqlite3_column_text(pGetStatement, 0));
            if (pc != nullptr) {
               value.assign(pc);
            }
         }
      }
      else if (rc == SQLITE_DONE) {
         rc = SQLITE_OK;
         //printf("No Value Found for %s\n",key.c_str());
      }
   }

   if (rc == SQLITE_OK) {
      rc = sqlite3_finalize(pGetStatement);
   }

   if (rc != SQLITE_OK) {
      copyErrMsg();
      fprintf(stderr, "Failed Get: %s\n", _errMsg);
   }

   if (locked) {
      rc = DBERROR_LOCKED; // bigger than any sqllite error
      _errMsg = db_lockedMsg;
   }
   return rc;
}

int db::prepareSet(const std::string &key, const std::string &value, int &transactionId)
{
   sqlite3_stmt *pSetStatement = nullptr;
   const char* pSet = prepare_set_sql;
   transactionId = -1;

   int rc = SQLITE_OK;
   int count = 0;
   while (*pSet != '\0') {
      ++count;
      rc = sqlite3_prepare_v2(
         _pDb,
         pSet,
         strlen(pSet),
         &pSetStatement,
         &pSet);

      if (rc == SQLITE_OK) {
         switch (count) {
         case 2: // Second SQL Statement
               rc = allocTextAndBind(key, pSetStatement, 1);
               if (rc == SQLITE_OK) {
                  rc = allocTextAndBind(value, pSetStatement, 2);
               }
               if (rc == SQLITE_OK) {
                  rc = sqlite3_bind_int(pSetStatement, 3, gettime());
               }
            break;
         case 3: // 3rd SQL Statement
               rc = allocTextAndBind(key, pSetStatement, 1);
               if (rc == SQLITE_OK) {
                  rc = sqlite3_bind_int(pSetStatement, 2, transactionId);
               }
            break;
         }
      }

      if (rc == SQLITE_OK) {
         rc = sqlite3_step(pSetStatement);
         if (rc == SQLITE_DONE) {
            rc = SQLITE_OK;
         }
      }

      if (rc == SQLITE_OK && count == 2) {
         transactionId = sqlite3_last_insert_rowid(_pDb);
      }

      if (rc == SQLITE_OK) {
         rc = sqlite3_finalize(pSetStatement);
      }

      if (rc != SQLITE_OK) {
         break;
      }
   }

   if (rc != SQLITE_OK) {
      copyErrMsg();
      fprintf(stderr, "Failed Prepare Set: %s\n", _errMsg);
      (void) rollback();
   }

   return rc;
}

int db::commitSet(int transactionId)
{
   sqlite3_stmt *pSetStatement = nullptr;
   const char *sql = commit_set_sql;

   int rc = SQLITE_OK;
   while (*sql != '\0') {
      rc = sqlite3_prepare_v2(
         _pDb,
         sql,
         strlen(commit_set_sql),
         &pSetStatement,
         &sql);

      int argCount = sqlite3_bind_parameter_count(pSetStatement);

      if ( (rc == SQLITE_OK) && (argCount >= 1)) {
         rc = sqlite3_bind_int(
                  pSetStatement,
                  1,
                  transactionId);
      }

      if (rc == SQLITE_OK) {
         rc = sqlite3_step(pSetStatement);
         if (rc == SQLITE_DONE) {
            rc = SQLITE_OK;
         }
      }

      if (rc == SQLITE_OK) {
         rc = sqlite3_finalize(pSetStatement);
      }

      if (rc != SQLITE_OK) {
         break;
      }
   }

   if (rc == SQLITE_OK) {
      transactionId = sqlite3_last_insert_rowid(_pDb);
   }

   if (rc != SQLITE_OK) {
      copyErrMsg();
      fprintf(stderr, "Failed Commit Set: %s\n", _errMsg);
      (void) rollback();
   }

   return rc;
}

int db::prepareDel(const std::string &key, int &transactionId)
{
   sqlite3_stmt *pDelStatement = nullptr;
   const char * sql = prepare_del_sql;
   transactionId = -1;
   int rc = SQLITE_OK;

   int count = 0;
   while (*sql != '\0') {
      ++count;
      rc = sqlite3_prepare_v2(
         _pDb,
         sql,
         strlen(sql),
         &pDelStatement,
         &sql);

      if (rc == SQLITE_OK) {
         switch (count) {
         case 2: // Second SQL Statement
               rc = allocTextAndBind(key, pDelStatement, 1);
               if (rc == SQLITE_OK) {
                  rc = sqlite3_bind_int(pDelStatement, 2, gettime());
               }
            break;
         case 3: // 3rd SQL Statement
               rc = allocTextAndBind(key, pDelStatement, 1);
               if (rc == SQLITE_OK) {
                  rc = sqlite3_bind_int(pDelStatement, 2, transactionId);
               }
            break;
         }
      }

      if (rc == SQLITE_OK) {
         rc = sqlite3_step(pDelStatement);
         if (rc == SQLITE_DONE) {
            rc = SQLITE_OK;
         }
      }

      if (rc == SQLITE_OK && count == 2) {
         transactionId = sqlite3_last_insert_rowid(_pDb);
      }
         
      if (rc == SQLITE_OK) {
         rc = sqlite3_finalize(pDelStatement);
      }

      if (rc != SQLITE_OK) {
         break;
      }
   }

   if (rc != SQLITE_OK) {
      copyErrMsg();
      fprintf(stderr, "Failed Prepare Delete: %s\n", _errMsg);
      (void) rollback();
   }

   return rc;
}

int db::commitDel(int transactionId)
{
   sqlite3_stmt *pCommitDelStatement = nullptr;
   const char* sql = commit_del_sql;

   int rc = SQLITE_OK;
   while (*sql != '\0'){
      rc = sqlite3_prepare_v2(
         _pDb,
         sql,
         strlen(commit_del_sql),
         &pCommitDelStatement,
         &sql);

      int argCount = sqlite3_bind_parameter_count(pCommitDelStatement);

      if (argCount >= 1) {
         if (rc == SQLITE_OK) {
            rc = sqlite3_bind_int(
                     pCommitDelStatement,
                     1,
                     transactionId);
         }
      }

      if (rc == SQLITE_OK) {
         rc = sqlite3_step(pCommitDelStatement);
         if (rc == SQLITE_DONE) {
            rc = SQLITE_OK;
         }
      }

      if (rc == SQLITE_OK) {
         rc = sqlite3_finalize(pCommitDelStatement);
      }

      if (rc != SQLITE_OK) {
         break;
      }
   }

   if (rc != SQLITE_OK) {
      copyErrMsg();
      fprintf(stderr, "Failed Commit Delete: %s\n", _errMsg);
      (void) rollback();
   }

   return rc;
}

int db::abort(int transactionId)
{
   sqlite3_stmt *pAbortStatement = nullptr;
   const char *sql = abort_sql;

   int rc = SQLITE_OK;
   while (*sql != '\0') {

      rc = sqlite3_prepare_v2(
         _pDb,
         sql,
         strlen(abort_sql),
         &pAbortStatement,
         &sql);

      int argCount = sqlite3_bind_parameter_count(pAbortStatement);

      if (argCount >= 1) {
         if (rc == SQLITE_OK) {
            rc = sqlite3_bind_int(
                     pAbortStatement,
                     1,
                     transactionId);
         }
      }

      if (rc == SQLITE_OK) {
         rc = sqlite3_step(pAbortStatement);
         if (rc == SQLITE_DONE) {
            rc = SQLITE_OK;
         }
      }

      if (rc == SQLITE_OK) {
         rc = sqlite3_finalize(pAbortStatement);
      }

      if (rc != SQLITE_OK) {
         break;
      }
   }

   if (rc != SQLITE_OK) {
      copyErrMsg();
      fprintf(stderr, "Failed Abort: %s\n", _errMsg);
      (void) rollback();
   }

   return rc;
}

int db::rollback()
{
   sqlite3_stmt *pRollback = nullptr;
   const char *sql = "ROLLBACK;";

   int rc = sqlite3_prepare_v2(
      _pDb,
      sql,
      strlen(abort_sql),
      &pRollback,
      nullptr);

   if (rc == SQLITE_OK) {
      rc = sqlite3_step(pRollback);
      if (rc == SQLITE_DONE) {
         rc = SQLITE_OK;
      }
   }

   if (rc == SQLITE_OK) {
      rc = sqlite3_finalize(pRollback);
   }

   if (rc != SQLITE_OK) {
      copyErrMsg();
      fprintf(stderr, "Failed ROLLBACK: %s\n", _errMsg);
   }

   return rc;
}

void db::copyErrMsg()
{
   const char* src = sqlite3_errmsg(_pDb);

   if (_errMsg != nullptr) {
      delete[] _errMsg;
      _errMsg = nullptr;
   }

   char* str = new char[strlen(src)+1];
   strcpy(str, src);
   _errMsg = str;
}

int db::setup()
{
   int rc;

    // Create the tables
    rc = exec(kvtable_sql);
    rc = exec(status_sql);
    rc = exec(transactiontype_sql);
    rc = exec(walog_sql);
    rc = exec(lockTable_sql);

    // Populate the domains
    rc = exec(populate_status_sql);
    rc = exec(populate_transactiontype_sql);
    rc += 1;

    return SQLITE_OK;
}

int db::replicaSetup()
{
   int rc = SQLITE_OK;

   // Create the tables
   rc = exec(kvtable_sql);
   rc = exec(status_sql);
   rc = exec(transactiontype_sql);
   rc = exec(r_walog_sql);
   rc = exec(lockTable_sql);

   // Populate the domains
   rc = exec(populate_status_sql);
   rc = exec(populate_transactiontype_sql);
   rc += 1;
   return SQLITE_OK;
}

int db::replicaPrepareSet(int transactionId, const std::string &key, const std::string &value)
{
   sqlite3_stmt *pSetStatement = nullptr;
   const char* pSet = r_prepare_set_sql;

   int rc = SQLITE_OK;

   int count = 0;
   while (*pSet != '\0') {
      ++count;
      rc = sqlite3_prepare_v2(
         _pDb,
         pSet,
         strlen(pSet),
         &pSetStatement,
         &pSet);

      if (rc == SQLITE_OK) {
         switch (count) {
         case 2:
            rc = allocTextAndBind(key, pSetStatement, 1);
            if (rc == SQLITE_OK) {
               rc = sqlite3_bind_int(pSetStatement, 2, transactionId);
            }
            break;
         case 3:
            rc = sqlite3_bind_int(pSetStatement, 1, transactionId);
            if (rc == SQLITE_OK) {
               rc = allocTextAndBind(key, pSetStatement, 2);
            }
            if (rc == SQLITE_OK) {
               rc = allocTextAndBind(value, pSetStatement, 3);
            }
            if (rc == SQLITE_OK) {
               rc = sqlite3_bind_int(pSetStatement, 4, gettime());
            }
            break;
         }
      }

      if (rc == SQLITE_OK) {
         rc = sqlite3_step(pSetStatement);
         if (rc == SQLITE_DONE) {
            rc = SQLITE_OK;
         }
      }

      if (rc == SQLITE_OK) {
         rc = sqlite3_finalize(pSetStatement);
      }

      if (rc != SQLITE_OK) {
         break;
      }
   }

   if (rc != SQLITE_OK) {
      copyErrMsg();
      fprintf(stderr, "Failed Replica Prepare Set: %s\n", _errMsg);
      (void) rollback();
   }

   return rc;
}
 
int db::replicaPrepareDel(int transactionId, const std::string &key)
{
   sqlite3_stmt *pDelStatement = nullptr;
   const char * sql = r_prepare_del_sql;
   int rc = SQLITE_OK;

   int count = 0;
   while (*sql != '\0') {
      ++count;
      rc = sqlite3_prepare_v2(
         _pDb,
         sql,
         strlen(sql),
         &pDelStatement,
         &sql);

      if (rc == SQLITE_OK) {
         switch (count) {
         case 2:
            rc = allocTextAndBind(key, pDelStatement, 1);
            if (rc == SQLITE_OK) {
               rc = sqlite3_bind_int(pDelStatement, 2, transactionId);
            }
            break;
         case 3:
            rc = allocTextAndBind(key, pDelStatement, 1);
            if (rc == SQLITE_OK) {
               rc = sqlite3_bind_int(pDelStatement, 2, transactionId);
            }
            if (rc == SQLITE_OK) {
               rc = sqlite3_bind_int(pDelStatement, 3, gettime());
            }
            break;
         }
      }

      if (rc == SQLITE_OK) {
         rc = sqlite3_step(pDelStatement);
         if (rc == SQLITE_DONE) {
            rc = SQLITE_OK;
         }
      }

      if (rc == SQLITE_OK) {
         rc = sqlite3_finalize(pDelStatement);
      }
      if (rc != SQLITE_OK) {
         break;
      }
   }

   if (rc != SQLITE_OK) {
      copyErrMsg();
      fprintf(stderr, "Failed Replica Prepare Delete: %s\n", _errMsg);
      (void) rollback();
   }

   return rc;
}

int db::queryTransaction(const int transactionId, int &transactionTypeId, int &statusId)
{
   sqlite3_stmt *pStatement = nullptr;
   const char * sql = query_transactionid_sql;
   int rc = SQLITE_OK;
   transactionTypeId = -1;
   statusId = -1;

   rc = sqlite3_prepare_v2(
         _pDb,
         sql,
         strlen(sql),
         &pStatement,
         &sql);


   if (rc == SQLITE_OK) {
      rc = sqlite3_bind_int(pStatement, 1, transactionId);
   }

   if (rc == SQLITE_OK) {
      rc = sqlite3_step(pStatement);
      if (rc == SQLITE_ROW) {
         rc = SQLITE_OK;
         transactionTypeId = sqlite3_column_int(pStatement, 0);
         statusId = sqlite3_column_int(pStatement, 1);
      }
      else if (rc == SQLITE_DONE) {
         rc = SQLITE_OK;
      }
   }

   if (rc == SQLITE_OK) {
      rc = sqlite3_finalize(pStatement);
   }

   if (rc != SQLITE_OK) {
      copyErrMsg();
      fprintf(stderr, "Failed Replica Query Transaction: %s\n", _errMsg);
   }
   
   return rc;
}

int db::gettime()
{
   int rc = 0;
   struct timeval tv;
   struct timezone tz;
   if (0 == gettimeofday(&tv, &tz)){
      rc = tv.tv_sec;
   }
   return rc;
}

int db::queryUncertain(const int timeStamp, std::list<int> &tidList)
{

   sqlite3_stmt *pStatement = nullptr;
   const char * sql = get_old_transactionid_sql;
   int rc = SQLITE_OK;

   rc = sqlite3_prepare_v2(
         _pDb,
         sql,
         strlen(sql),
         &pStatement,
         &sql);


   if (rc == SQLITE_OK) {
      rc = sqlite3_bind_int(pStatement, 1, timeStamp);
   }

   if (rc == SQLITE_OK) {
      rc = sqlite3_step(pStatement);
      while (rc == SQLITE_ROW) {
         tidList.push_back(sqlite3_column_int(pStatement, 0));
         rc = sqlite3_step(pStatement);
      }
      if (rc == SQLITE_DONE) {
         rc = SQLITE_OK;
      }
   }

   if (rc == SQLITE_OK) {
      rc = sqlite3_finalize(pStatement);
   }

   if (rc != SQLITE_OK) {
      copyErrMsg();
      fprintf(stderr, "Failed Query Uncertain: %s\n", _errMsg);
      (void) rollback();
   }

   return rc;
}
