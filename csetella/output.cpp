//
// output.cpp
//

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <memory>
#include <set>
#include <string>
#include "output.h"

Output::Output()
   :_fd(-1)
{
}

Output::~Output(){
   if (_fd > 0) {
      close(_fd);
   }
}

void Output::open(const char* filename){
   _fd = ::open(filename, O_CREAT | O_APPEND | O_RDWR, S_IRUSR | S_IWUSR | S_IWGRP | S_IWGRP | S_IROTH | S_IWOTH);
   if (_fd < 0) {
      throw "Failed to create file!";
   }
   read();
}

void Output::addReply(const char* textBlock, unsigned int cch){
   std::string str(textBlock, cch);
   if (_replies.count(str) == 0) {
      _replies.insert(str);
      write(str);
   }
}

void Output::read(){
   int ret = 0;
   int cch = 0;
   char chCount[5];
   char chTextBlock[4096];

   memset(chCount, 0, sizeof(chCount));
   memset(chTextBlock, 0, sizeof(chTextBlock));

   _replies.clear();
   ::lseek(_fd,0,SEEK_SET);


   ret = ::read(_fd, chCount, 4);
   while (ret > 0) {
      cch = atoi(chCount);
      ret = ::read(_fd, chTextBlock, cch);
      if (ret < cch) {
         throw "Invalid File: not enough characters";
      }
      _replies.insert(std::string(chTextBlock,cch));

      memset(chCount, 0,sizeof(5));
      ret = ::read(_fd, chCount, 4);
   }
}

void Output::write(const std::string& str){
   char chCount[5];
   int ret = 0;
   memset(chCount, 0, sizeof(chCount));
   sprintf(chCount, "%4lu", str.size());
   ret = ::write(_fd, chCount, 4);
   if (ret < 0) {
      throw "Output Write Failed!";
   }
   ret = ::write(_fd, str.data(), str.size());
   if (ret < 0) {
      throw "Output Write Failed";
   }

}
