// SQL strings for the DB Schema

#include "schema.h"

const char* kvtable_sql = 
   "Create Table IF NOT EXISTS KeyValue ("
   "   Key TEXT PRIMARY KEY ASC,"
   "   Value TEXT NOT NULL)";


const char* status_sql = 
   "Create Table IF NOT EXISTS Status("
   "  StatusID INTEGER PRIMARY KEY ASC,"
   "  Name TEXT NOT NULL UNIQUE)";

const char* populate_status_sql=
   "INSERT OR IGNORE INTO Status (StatusID, Name) Values (0, \"Prepare\");"
   "INSERT OR IGNORE INTO Status (StatusID, Name) Values (1, \"Committed\");";

const char* transactiontype_sql = 
   "Create Table IF NOT EXISTS TransactionType("
   "  TransactionTypeID INTEGER PRIMARY KEY ASC,"
   "  Name TEXT UNIQUE)";

const char* populate_transactiontype_sql = 
   "INSERT OR IGNORE INTO TransactionType (TransactionTypeID, Name) Values (0, \"Set\");"
   "INSERT OR IGNORE INTO TransactionType (TransactionTypeID, Name) Values (1, \"Delete\");";
   
const char* walog_sql = 
   "Create Table IF NOT EXISTS WriteAheadLog ("
   "  TransactionID INTEGER PRIMARY KEY AUTOINCREMENT,"
   "  TransactionTypeID INTEGER,"
   "  Key TEXT NOT NULL,"
   "  Value TEXT,"
   "  StatusID INTEGER NOT NULL,"
   "  TimeStamp INTEGER NOT NULL,"
   "  FOREIGN KEY(TransactionTypeID) REFERENCES TransactionType(TransactionTypeID),"
   "  FOREIGN KEY(StatusID) REFERENCES Status(StatusID)"
   "  )";

const char* lockTable_sql =
   "Create Table IF NOT EXISTS LockTable ("
   "  Key TEXT PRIMARY KEY ASC,"
   "  TransactionID INTEGER UNIQUE NOT NULL,"
   "  FOREIGN KEY(TransactionID) REFERENCES WriteAheadLog(TransactionID)"
   ")";

const char* prepare_set_sql =
   "BEGIN TRANSACTION;"
   "INSERT INTO WriteAheadLog (TransactionTypeID, Key, Value, StatusID, TimeStamp)"
   "   Values (0, ?1, ?2, 0, ?3);"
   "INSERT INTO LockTable (Key, TransactionId) Values (?1, ?2);"
   "END TRANSACTION;";

const char* commit_set_sql = 
   "BEGIN TRANSACTION;"
   "DELETE FROM LockTable WHERE Key = (SELECT Key FROM WriteAheadLog WHERE TransactionID = ?1);"
   "INSERT OR REPLACE INTO KeyValue (Key, Value) SELECT Key, Value FROM WriteAheadLog WHERE TransactionId = ?1;"
   "UPDATE OR IGNORE WriteAheadLog SET StatusID = 1 WHERE TransactionID = ?1;"
   "END TRANSACTION;";

const char* prepare_del_sql = 
   "BEGIN TRANSACTION;"
   "INSERT INTO WriteAheadLog (TransactionTypeID, Key, StatusID, TimeStamp) "
   "   VALUES (1, ?1, 0, ?2);"
   "INSERT INTO LockTable (Key, TransactionID) Values (?1, ?2);"
   "END TRANSACTION;";

const char* commit_del_sql = 
   "BEGIN TRANSACTION;"
   "DELETE FROM LockTable WHERE Key = (SELECT Key FROM WriteAheadLog WHERE TransactionID = ?1);"
   "DELETE FROM KeyValue WHERE Key = (SELECT Key FROM WriteAheadLog WHERE TransactionID = ?1);"
   "UPDATE OR IGNORE WriteAheadLog SET StatusID = 1 WHERE TransactionID = ?1;"
   "END TRANSACTION";

const char* abort_sql = 
   "BEGIN TRANSACTION;"
   "DELETE FROM LockTable WHERE TransactionID = ?1;"
   "DELETE FROM WriteAheadLog Where TransactionID = ?1;"
   "END TRANSACTION";

const char* get_sql = 
    "SELECT KeyValue.Value, LockTable.Key"
    " FROM KeyValue LEFT OUTER JOIN LockTable"
    "  ON KeyValue.Key = LockTable.Key"
    " WHERE KeyValue.Key = ?1;";


const char* r_walog_sql = 
   "Create Table IF NOT EXISTS WriteAheadLog ("
   "  TransactionID INTEGER PRIMARY KEY ASC,"
   "  TransactionTypeID INTEGER NOT NULL,"
   "  Key TEXT NOT NULL,"
   "  Value TEXT,"
   "  StatusID INTEGER NOT NULL,"
   "  TimeStamp INTEGER NOT NULL,"
   "  FOREIGN KEY(TransactionTypeID) REFERENCES TransactionType(TransactionTypeID),"
   "  FOREIGN KEY(StatusID) REFERENCES Status(StatusID)"
   "  )";
    
const char* r_prepare_del_sql = 
   "BEGIN TRANSACTION;"
   "INSERT INTO LockTable (Key, TransactionId) Values (?1, ?2);"
   "INSERT INTO WriteAheadLog (Key, TransactionId, TransactionTypeID, StatusID, TimeStamp) "
   "   VALUES (?1, ?2, 1, 0, ?3);"
   "END TRANSACTION;";

const char* r_prepare_set_sql =
   "BEGIN TRANSACTION;"
   "INSERT INTO LockTable (Key, TransactionID) Values (?1, ?2);"
   "INSERT INTO WriteAheadLog (TransactionID, TransactionTypeID, Key, Value, StatusID, TimeStamp)"
   "   Values (?1, 0, ?2, ?3, 0, ?4);"
   "END TRANSACTION;";

const char* query_transactionid_sql =
   "SELECT TransactionTypeID, StatusID FROM WriteAheadLog WHERE TransactionID = ?1";

const char* get_old_transactionid_sql = 
   "SELECT TransactionID FROM WriteAheadLog Where StatusID = 0 AND TimeStamp < ?1";



