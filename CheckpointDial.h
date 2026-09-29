#pragma once
#include <ArduinoJson.h>
#include <functional>

class CheckpointDial {
 public:
  void begin(const String& game);
  void tag(const String& raw, const std::function<bool()>& connect, const std::function<void()>& disconnect);
  void update();
  void press();
  bool hasUnpaidRun() const;
  bool reset(bool factory = false);
  void redraw() { dirty = true; }
 private:
  uint32_t sequence = 0;
  DynamicJsonDocument saved{16384};
  String gameId;
  bool dirty = true, frozen = false, storageReady = true;
  int page = 0;
  long encoder = 0;
  unsigned long tick = 0, lastSave = 0, noticeUntil = 0, lastScroll = 0;
  String notice, noticeLabel;
  bool persist();
  void show(const String& label, const String& message, unsigned long duration = 4000);
  void checkpoint(const String& tag, const std::function<bool()>& connect, const std::function<void()>& disconnect);
  bool start(const String& tag);
  bool finish(const String& tag);
  void toggleBadge(const String& tag);
  int visits() const;
  JsonObjectConst tier() const;
  void render();
};
extern CheckpointDial checkpointDial;
