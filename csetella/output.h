// output.h
#pragma once


class Output
{
public:
   Output();
   ~Output();
   void open(const char* filename);
   void addReply(const char* textBlock, unsigned int cch);
private:
   void read();
   void write(const std::string&);

   std::set< std::string > _replies;
   int _fd;
};

extern Output g_output;