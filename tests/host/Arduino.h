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
  String(int value) : std::string(std::to_string(value)) {}
  String(unsigned value) : std::string(std::to_string(value)) {}
  String(long value) : std::string(std::to_string(value)) {}
  String(unsigned long value) : std::string(std::to_string(value)) {}
  bool isEmpty() const { return empty(); }
  using std::string::replace;
  void remove(size_t start) { erase(start); }
  String substring(size_t start, size_t end = std::string::npos) const { return substr(start, end == std::string::npos ? end : end - start); }
  int lastIndexOf(char ch, size_t end) const { auto pos = rfind(ch, end); return pos == npos ? -1 : pos; }
  void trim() { auto start = find_first_not_of(" \n\r\t"); if (start == npos) { clear(); return; } erase(0, start); erase(find_last_not_of(" \n\r\t") + 1); }
  void replace(const String& from, const String& to) { size_t pos = 0; while ((pos = find(from, pos)) != npos) { std::string::replace(pos, from.size(), to); pos += to.size(); } }
};
inline unsigned long fakeMillis = 1;
inline unsigned long millis() { return fakeMillis; }
inline void delay(unsigned long ms) { fakeMillis += ms; }
struct SerialStub {
  template<typename... Args> void printf(const char*, Args...) const {}
  void println(const String&) const {}
};
inline SerialStub Serial;
