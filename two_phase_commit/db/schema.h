// SQL strings for the DB Schema

#pragma once

// Shared Schema
extern const char* kvtable_sql;
extern const char* status_sql;
extern const char* populate_status_sql;
extern const char* transactiontype_sql;
extern const char* populate_transactiontype_sql;
extern const char* lockTable_sql;
extern const char* abort_sql;
extern const char* get_sql;
extern const char* commit_set_sql;
extern const char* commit_del_sql;

// Master Schema
extern const char* walog_sql;
extern const char* prepare_set_sql;
extern const char* prepare_del_sql;
extern const char* query_transactionid_sql;
extern const char* get_old_transactionid_sql;

// Replica Schema
extern const char* r_walog_sql;
extern const char* r_prepare_set_sql;
extern const char* r_prepare_del_sql;



