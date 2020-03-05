#include <Arduino.h>
#include "Url.h"

Url::Url(const String& url_s) {
  // check for ://
  int index = url_s.indexOf("://");
  if (index < 0) {
    ESP_LOGV(TAG, "failed to parse protocol");
  }

  this->Protocol = url_s.substring(0, index);
  this->Protocol.toLowerCase();

  this->Path = url_s.substring(index + 3); // after ://

  index = this->Path.indexOf('/'); // looking for the first /
  this->Host = this->Path.substring(0, index);
  this->Path.remove(0,index); // removing host part

  // remove Authorization part from host
  index = this->Host.indexOf('@');
  if (index >= 0) {
    // auth info
    this->Host.remove(0, index + 1); // remove auth part including @
  }

  // get port
  index = this->Host.indexOf(':');
  //TODO(gmasse): does not support IPv6 like [1234:abcd::1234]:8888
  if (index >= 0) {
    this->Port = this->Host.substring(index + 1);
    this->Host.remove(index, this->Port.length() + 1); // remove : + port
  }

  // get query
  index = this->Path.indexOf('?');
  if (index >= 0) {
    this->Query = this->Path.substring(index + 1);
    this->Path.remove(index);
  }
}
