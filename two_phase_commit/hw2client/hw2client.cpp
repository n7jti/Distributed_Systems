#include "KeyValueStore.h"

#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace tpc;

void usage()
{
   printf("Usage: \n");
   printf("  hw2client get <key>/t/tPrints the value for the given key\n");
   printf("  hw2client del <key>/t/tDeletes the value for the given key\n");
   printf("  hw2client set <key> <value>/tSets the value for the given key\n");
}

int main(int argc, char **argv) {
  boost::shared_ptr<TSocket> socket(new TSocket("localhost", 9090));
  boost::shared_ptr<TTransport> transport(new TFramedTransport(socket));
  boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

  KeyValueStoreClient client(protocol);
  transport->open();

  try {

     if (argc < 3) {
        usage();
     }
     else if (argc == 3) {
        if  (0 == strcmp("get", argv[1])) {
           // Get
           std::string value;
           client.get(value, argv[2]);
           printf("%s\n", value.c_str());
        }
        else if (0 == strcmp("del", argv[1])) {
           // Delete
           client.erase(argv[2]);
        }
        else
        {
           usage();
        }
     }
     else if (argc == 4) {
        if (0 == strcmp("set", argv[1])) {
           // Set
           client.put(argv[2], argv[3]);
        }
        else {
           usage();
        }
     }
     else {
        usage();
     }
  }
  catch (TpcException ex) {
     printf("%d, %s\n", ex.x, ex.msg.c_str());
  }



  transport->close();

  return 0;
}
