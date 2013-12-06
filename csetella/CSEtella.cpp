#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "server.h"
#include "log.h"
#include "output.h"

using namespace std;

int g_continue = 1;
Log g_log;
Output g_output;

void sig_int(int signum)
{
   g_log.write(">>>>>>>>>> Exiting! <<<<<<<<<<\n");
   g_continue = 0;
}

void usage(const char* zero)
{
   printf("%s <logPrefix> <localaddress> <local port> [<remote address> <remote port>]\n", zero);
}

int main (int argc, char *argv[])
{
   signal(SIGINT, sig_int);
   signal(SIGHUP, sig_int);
   signal(SIGTERM, sig_int);

   Server server;
   char logName[255];

   if (argc >= 2) {
      sprintf(logName,"%s_log.log",argv[1]);
      g_log.open(logName);

      sprintf(logName,"%s_replyLog.log", argv[1]);
      g_output.open(logName);
   }

   if (argc == 4) {
      server.run(argv[2], atoi(argv[3]));
   }
   else if (argc == 6) {
      server.run(argv[2], static_cast<unsigned short int>(atoi(argv[3])), argv[4], static_cast<unsigned short int>(atoi(argv[5])));
   }
   else {
      usage(argv[0]);
   }

   return(0);
}

