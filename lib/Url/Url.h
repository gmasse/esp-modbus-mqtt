/*
 Url.h - URL parsing class headers
 Copyright (C) 2020 Germain Masse

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef LIB_URL_URL_H_
#define LIB_URL_URL_H_

#include <Arduino.h>

class Url {
 public:
  explicit Url(const String& url_s);  // omitted copy, ==, accessors, ...
  String Query, Path, Protocol, Host, Port;

// private:
//  void Parse(const String& url_s);
};

#endif  // LIB_URL_URL_H_
