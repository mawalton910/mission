#pragma once
#include <string>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <cstdint>
class String : public std::string {
 public:
  using std::string::string;
  String(const std::string& value) : std::string(value) {}
  bool isEmpty() const { return empty(); }
};
struct SerialStub {
  template<typename... Args> void printf(const char*, Args...) const {}
  void println(const String&) const {}
};
inline SerialStub Serial;
