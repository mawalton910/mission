#pragma once
#include "Arduino.h"
#include <map>
#include <algorithm>
#define FILE_READ "r"
#define FILE_WRITE "w"
struct StorageStub {
  std::map<std::string, std::string> files;
  bool unavailable = false, rejectWrite = false, rejectRename = false;
};
inline StorageStub storage;
class File {
  std::string path;
  size_t position = 0;
  bool opened = false;
 public:
  File(const char* p, bool valid) : path(p), opened(valid) {}
  explicit operator bool() const { return opened; }
  size_t size() const { return storage.files[path].size(); }
  size_t readBytes(char* output, size_t count) {
    auto& data = storage.files[path];
    count = std::min(count, data.size() - position);
    memcpy(output, data.data() + position, count); position += count; return count;
  }
  String readString() { return storage.files[path].substr(position); }
  size_t write(const uint8_t* data, size_t count) {
    if (storage.rejectWrite) return 0;
    storage.files[path].append(reinterpret_cast<const char*>(data), count); return count;
  }
  size_t print(const String& value) { return write(reinterpret_cast<const uint8_t*>(value.c_str()), value.length()); }
  void flush() {}
  void close() { opened = false; }
};
struct SpiffsStub {
  bool begin(bool) { return !storage.unavailable; }
  bool exists(const char* path) { return storage.files.count(path) > 0; }
  File open(const char* path, const char* mode) {
    if (storage.unavailable) return File(path, false);
    if (strcmp(mode, FILE_WRITE) == 0) storage.files[path] = "";
    return File(path, exists(path));
  }
  bool remove(const char* path) { return !storage.unavailable && storage.files.erase(path) > 0; }
  bool rename(const char* from, const char* to) {
    if (storage.rejectRename || !exists(from) || exists(to)) return false;
    storage.files[to] = std::move(storage.files[from]); storage.files.erase(from); return true;
  }
};
inline SpiffsStub SPIFFS;
