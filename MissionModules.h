#pragma once

#include <Arduino.h>

enum class MissionModuleState { IDLE, PLAYING, WON, LOST };

// Additive hardware service: the existing native M5Dial reader remains owned
// by the sketch. This service only handles the external Port A/B modules.
class MissionModules {
 public:
  void begin();
  void update();
  bool pollExternalNfc(String& uidOut);
  bool externalNfcReady() const { return _nfcReady; }
  void printStatus() const;
  void scanPortAI2c() const;
  void setState(MissionModuleState state);
  void setTargetReached(bool reached);

 private:
  bool _nfcReady = false;
  MissionModuleState _state = MissionModuleState::IDLE;
  bool _targetReached = false;
  bool _targetPulseVisible = false;
  unsigned long _lastTargetPulseAt = 0;

  void renderLeds();
};