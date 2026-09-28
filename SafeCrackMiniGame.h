#pragma once

#include <Arduino.h>

#include "MissionModules.h"

class SafeCrackMiniGame {
 public:
  explicit SafeCrackMiniGame(MissionModules& modules) : _modules(modules) {}

  void begin();
  bool handleBadge(const String& badgeUid);
  void handleConfirm();
  void update();
  bool isFinished() const { return _state == State::FINISHED; }
  String badgeUid() const { return _badgeUid; }

 private:
  enum class State { WAIT_BADGE, PLAYING, RESULT, FINISHED };

  static constexpr int COMBINATION_POSITIONS = 40;
  static constexpr int COMBINATION_DIGITS = 3;
  static constexpr unsigned long TIME_LIMIT_MS = 120000UL;
  static constexpr unsigned long WRONG_GUESS_PENALTY_MS = 3000UL;
  static constexpr unsigned long RESULT_HOLD_MS = 6000UL;

  MissionModules& _modules;
  State _state = State::WAIT_BADGE;
  String _badgeUid;
  uint8_t _target[COMBINATION_DIGITS] = {};
  bool _clockwise[COMBINATION_DIGITS] = {};
  bool _locked[COMBINATION_DIGITS] = {};
  int _digitIndex = 0;
  int _position = 0;
  int _lastPosition = -1;
  int _stepsInDirection = 0;
  bool _rotationCleared = false;
  bool _won = false;
  unsigned long _startedAt = 0;
  unsigned long _resultAt = 0;

  void startGame();
  void finish(bool won);
  void render() const;
  void renderBadgePrompt() const;
  void renderResult() const;
  int wrapPosition(long raw) const;
  int circularDistance(int first, int second) const;
  void updatePosition();
};