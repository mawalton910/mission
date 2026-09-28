#include "SafeCrackMiniGame.h"

#include <M5Dial.h>
#include <math.h>

namespace {
constexpr uint16_t COLOR_PANEL = 0x10A2;
constexpr uint16_t COLOR_BRASS = 0xC680;
constexpr uint16_t COLOR_DIM = 0x6320;

void pointOnCircle(int centerX, int centerY, float radius, float degrees, int& xOut, int& yOut) {
  float radians = (degrees - 90.0f) * (float)M_PI / 180.0f;
  xOut = centerX + (int)(radius * cosf(radians));
  yOut = centerY + (int)(radius * sinf(radians));
}
}  // namespace

void SafeCrackMiniGame::begin() {
  _state = State::WAIT_BADGE;
  _badgeUid = "";
  _modules.setState(MissionModuleState::IDLE);
  renderBadgePrompt();
}

bool SafeCrackMiniGame::handleBadge(const String& badgeUid) {
  if (_state != State::WAIT_BADGE || badgeUid.length() == 0) return false;
  _badgeUid = badgeUid;
  _badgeUid.replace(" ", "");
  _badgeUid.toUpperCase();
  Serial.printf("[SAFE CRACK] Badge captured: %s\n", _badgeUid.c_str());
  startGame();
  return true;
}

void SafeCrackMiniGame::startGame() {
  _digitIndex = 0;
  _position = 0;
  _lastPosition = -1;
  _stepsInDirection = 0;
  _rotationCleared = false;
  _won = false;
  for (int index = 0; index < COMBINATION_DIGITS; index++) {
    _target[index] = random(0, COMBINATION_POSITIONS);
    _clockwise[index] = (index % 2) == 0;
    _locked[index] = false;
  }
  _startedAt = millis();
  M5Dial.Encoder.write(0);
  M5Dial.Speaker.tone(800, 100);
  _modules.setState(MissionModuleState::PLAYING);
  _state = State::PLAYING;
}

void SafeCrackMiniGame::handleConfirm() {
  if (_state != State::PLAYING) return;
  const bool positionCorrect = circularDistance(_position, _target[_digitIndex]) == 0;
  if (!positionCorrect || !_rotationCleared) {
    _startedAt -= WRONG_GUESS_PENALTY_MS;
    M5Dial.Speaker.tone(140, 220);
    return;
  }

  _locked[_digitIndex++] = true;
  M5Dial.Speaker.tone(1400, 70);
  _stepsInDirection = 0;
  _rotationCleared = false;
  if (_digitIndex >= COMBINATION_DIGITS) finish(true);
}

void SafeCrackMiniGame::finish(bool won) {
  _won = won;
  _resultAt = millis();
  _state = State::RESULT;
  _modules.setState(won ? MissionModuleState::WON : MissionModuleState::LOST);
  M5Dial.Speaker.tone(won ? 1200 : 180, won ? 120 : 400);
  Serial.printf("[SAFE CRACK] %s badge=%s duration=%lums\n",
                won ? "won" : "lost", _badgeUid.c_str(), millis() - _startedAt);
}

void SafeCrackMiniGame::update() {
  if (_state == State::WAIT_BADGE) {
    renderBadgePrompt();
    return;
  }
  if (_state == State::PLAYING) {
    if (millis() - _startedAt >= TIME_LIMIT_MS) finish(false);
    else updatePosition();
    _modules.setTargetReached(_state == State::PLAYING &&
                              circularDistance(_position, _target[_digitIndex]) == 0 &&
                              _rotationCleared);
    if (_state == State::PLAYING) render();
    return;
  }
  if (_state == State::RESULT) {
    renderResult();
    if (millis() - _resultAt >= RESULT_HOLD_MS) {
      _modules.setState(MissionModuleState::IDLE);
      _state = State::FINISHED;
    }
  }
}

int SafeCrackMiniGame::wrapPosition(long raw) const {
  long position = raw % COMBINATION_POSITIONS;
  return position < 0 ? position + COMBINATION_POSITIONS : position;
}

int SafeCrackMiniGame::circularDistance(int first, int second) const {
  int distance = abs(first - second);
  return distance > COMBINATION_POSITIONS - distance ? COMBINATION_POSITIONS - distance : distance;
}

void SafeCrackMiniGame::updatePosition() {
  int nextPosition = wrapPosition(M5Dial.Encoder.read());
  if (_lastPosition < 0) _lastPosition = nextPosition;
  int delta = nextPosition - _lastPosition;
  if (delta > COMBINATION_POSITIONS / 2) delta -= COMBINATION_POSITIONS;
  if (delta < -COMBINATION_POSITIONS / 2) delta += COMBINATION_POSITIONS;
  if (delta != 0) {
    const int wantedDirection = _clockwise[_digitIndex] ? 1 : -1;
    if ((delta > 0 ? 1 : -1) == wantedDirection) _stepsInDirection += abs(delta);
    else _stepsInDirection = 0;
    if (_stepsInDirection >= COMBINATION_POSITIONS) _rotationCleared = true;
    const int distance = circularDistance(nextPosition, _target[_digitIndex]);
    M5Dial.Speaker.tone(250 + (20 - distance) * (20 - distance) * 4, 12);
  }
  _position = nextPosition;
  _lastPosition = nextPosition;
}

void SafeCrackMiniGame::renderBadgePrompt() const {
  M5Dial.Display.fillScreen(TFT_BLACK);
  M5Dial.Display.setTextDatum(MC_DATUM);
  M5Dial.Display.setTextColor(COLOR_BRASS);
  M5Dial.Display.setTextSize(2);
  M5Dial.Display.drawString("SAFE CRACK", 120, 76);
  M5Dial.Display.setTextColor(TFT_WHITE);
  M5Dial.Display.setTextSize(1);
  M5Dial.Display.drawString("SCAN BADGE TO BEGIN", 120, 112);
  M5Dial.Display.setTextColor(TFT_DARKGREY);
  M5Dial.Display.drawString("Native reader or Port A NFC/RFID2", 120, 142);
}

void SafeCrackMiniGame::render() const {
  const int centerX = 120;
  const int centerY = 120;
  const unsigned long elapsed = millis() - _startedAt;
  const unsigned long remainingMs = elapsed >= TIME_LIMIT_MS ? 0 : TIME_LIMIT_MS - elapsed;
  const int distance = circularDistance(_position, _target[_digitIndex]);
  const uint16_t targetColor = distance == 0 && _rotationCleared ? TFT_GREEN :
                               distance <= 3 ? TFT_ORANGE : TFT_CYAN;

  M5Dial.Display.fillScreen(TFT_BLACK);
  M5Dial.Display.fillCircle(centerX, centerY, 80, COLOR_PANEL);
  M5Dial.Display.drawCircle(centerX, centerY, 108, COLOR_BRASS);
  for (int index = 0; index < COMBINATION_POSITIONS; index++) {
    int innerX, innerY, outerX, outerY;
    pointOnCircle(centerX, centerY, index % 5 == 0 ? 94 : 99, index * 9.0f, innerX, innerY);
    pointOnCircle(centerX, centerY, 106, index * 9.0f, outerX, outerY);
    M5Dial.Display.drawLine(innerX, innerY, outerX, outerY, index % 5 == 0 ? COLOR_BRASS : COLOR_DIM);
  }
  int pointerX, pointerY;
  pointOnCircle(centerX, centerY, 88, _position * 9.0f, pointerX, pointerY);
  M5Dial.Display.drawLine(centerX, centerY, pointerX, pointerY, targetColor);
  M5Dial.Display.fillCircle(pointerX, pointerY, 3, COLOR_BRASS);

  M5Dial.Display.setTextDatum(MC_DATUM);
  M5Dial.Display.setTextColor(TFT_WHITE, COLOR_PANEL);
  M5Dial.Display.setTextSize(4);
  char positionText[4];
  snprintf(positionText, sizeof(positionText), "%02d", _position);
  M5Dial.Display.drawString(positionText, centerX, centerY - 10);
  M5Dial.Display.setTextSize(1);
  M5Dial.Display.setTextColor(targetColor, COLOR_PANEL);
  M5Dial.Display.drawString(_clockwise[_digitIndex] ? "TURN RIGHT" : "TURN LEFT", centerX, centerY + 20);
  M5Dial.Display.drawString(_rotationCleared ? "PRESS TO SET" : "MAKE ONE FULL TURN", centerX, centerY + 38);

  for (int index = 0; index < COMBINATION_DIGITS; index++) {
    int x = centerX - 20 + index * 20;
    if (_locked[index]) M5Dial.Display.fillCircle(x, 28, 5, COLOR_BRASS);
    else M5Dial.Display.drawCircle(x, 28, 5, index == _digitIndex ? TFT_WHITE : TFT_DARKGREY);
  }
  char timeText[8];
  snprintf(timeText, sizeof(timeText), "%lu:%02lu", remainingMs / 60000UL, (remainingMs / 1000UL) % 60UL);
  M5Dial.Display.setTextColor(TFT_WHITE);
  M5Dial.Display.drawString(timeText, centerX, 214);
}

void SafeCrackMiniGame::renderResult() const {
  M5Dial.Display.fillScreen(TFT_BLACK);
  M5Dial.Display.setTextDatum(MC_DATUM);
  M5Dial.Display.setTextSize(3);
  M5Dial.Display.setTextColor(_won ? TFT_GREEN : TFT_RED);
  M5Dial.Display.drawString(_won ? "CRACKED" : "LOCKED", 120, 104);
  M5Dial.Display.setTextSize(1);
  M5Dial.Display.setTextColor(TFT_WHITE);
  M5Dial.Display.drawString(_won ? "Vault opened" : "Time expired", 120, 132);
}