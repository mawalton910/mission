#pragma once
#include <Arduino.h>

// Fixed-size UID records are read from flash, not kept as thousands of Strings.
class BadgeCatalog {
 public:
  bool load(const String& game);
  bool refresh(const String& game);
  bool contains(const String& tag) const;
  bool clear();
  bool ready() const { return valid; }
  const String& error() const { return lastError; }
 private:
  String gameId, lastError;
  bool valid = false;
  bool read(const char* path, const String& game, const String& wanted, bool& found) const;
};
extern BadgeCatalog badgeCatalog;
