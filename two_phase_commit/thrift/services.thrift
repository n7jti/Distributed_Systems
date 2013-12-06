namespace cpp tpc

exception TpcException
{
  1:string msg,
  2:optional i32 x,
}

service KeyValueStore{
   void put(1:string key, 2:string value) throws (1:TpcException te),
   void erase(1:string key) throws (1:TpcException te),
   string get(1:string key) throws (1:TpcException te)
}

service Master{
   void query(1:i32 transactionId) throws (1:TpcException te)
}

service Replica{
   void prepareSet(1:i32 transactionId, 2:string key, 3:string value) throws (1:TpcException te),
   void commitSet(1:i32 transactionId) throws (1:TpcException te),
   void prepareDel(1:i32 transactionId, 2:string key) throws (1:TpcException te),
   void commitDel(1:i32 transactionId) throws (1:TpcException te),
   void abort(1:i32 transactionId) throws (1:TpcException te)
}
