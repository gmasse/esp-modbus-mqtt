#ifndef URL_H_INCLUDED__
#define URL_H_INCLUDED__

#include <Arduino.h>

class Url {
 public:
  Url(const String& url_s); // omitted copy, ==, accessors, ...
  String Query, Path, Protocol, Host, Port;

// private:
//  void Parse(const String& url_s);
};

#endif
