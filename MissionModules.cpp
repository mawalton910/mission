#include "MissionModules.h"

#include "Config.h"
#include <M5Unified.h>
#include <Wire.h>

#if ENABLE_UNIT_RGB && __has_include(<FastLED.h>)
#include <FastLED.h>
#define MISSION_RGB_AVAILABLE 1
#else
#define MISSION_RGB_AVAILABLE 0
#endif

#if ENABLE_EXTERNAL_NFC
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <M5Utility.h>
#include <wiring/m5_unit_unified_wiring.hpp>
#include <vector>
#define MISSION_NFC_AVAILABLE 1
#else
#define MISSION_NFC_AVAILABLE 0
#endif

namespace {
#if MISSION_RGB_AVAILABLE
constexpr uint8_t PORT_B_RGB_PIN = 2;
constexpr uint8_t RGB_LED_COUNT = 3;
CRGB rgbLeds[RGB_LED_COUNT];

void fillLeds(const CRGB& color) {
  fill_solid(rgbLeds, RGB_LED_COUNT, color);
  FastLED.show();
}
#endif

#if MISSION_NFC_AVAILABLE
m5::unit::UnitUnified units;
m5::unit::UnitNFC unitNfc;
m5::nfc::NFCLayerA nfcA(unitNfc);
#endif
}  // namespace

void MissionModules::begin() {
#if MISSION_RGB_AVAILABLE
  FastLED.addLeds<SK6812, PORT_B_RGB_PIN, GRB>(rgbLeds, RGB_LED_COUNT);
  FastLED.setBrightness(96);
  renderLeds();
  Serial.println("[Modules] Unit RGB ready on Port B (GPIO2)");
#elif ENABLE_UNIT_RGB
  Serial.println("[Modules] Unit RGB unavailable: install FastLED");
#endif

#if MISSION_NFC_AVAILABLE
  _nfcReady = m5::unit::wiring::addI2C(units, unitNfc) && units.begin();
  Serial.printf("[Modules] Unit NFC/RFID2 on Port A: %s\n", _nfcReady ? "ready" : "not found");
  scanPortAI2c();
#endif
}

void MissionModules::update() {
#if MISSION_NFC_AVAILABLE
  if (_nfcReady) units.update();
#endif
  if (_state == MissionModuleState::PLAYING && _targetReached &&
      millis() - _lastTargetPulseAt >= 70) {
    _targetPulseVisible = !_targetPulseVisible;
    _lastTargetPulseAt = millis();
    renderLeds();
  }
}

void MissionModules::printStatus() const {
#if MISSION_NFC_AVAILABLE
  Serial.printf("[Modules] Port A NFC/RFID2: %s\n", _nfcReady ? "ready and polling" : "not detected");
#else
  Serial.println("[Modules] Port A NFC/RFID2: disabled by ENABLE_EXTERNAL_NFC=0");
#endif

#if MISSION_RGB_AVAILABLE
  Serial.println("[Modules] Port B RGB: ready on GPIO2");
#elif ENABLE_UNIT_RGB
  Serial.println("[Modules] Port B RGB: support unavailable (install FastLED)");
#else
  Serial.println("[Modules] Port B RGB: disabled by ENABLE_UNIT_RGB=0");
#endif
}

void MissionModules::scanPortAI2c() const {
#if MISSION_NFC_AVAILABLE
  const int sda = M5.getPin(m5::pin_name_t::port_a_sda);
  const int scl = M5.getPin(m5::pin_name_t::port_a_scl);
  int found = 0;
  Serial.printf("[Modules] Port A I2C scan: SDA=%d SCL=%d addresses=", sda, scl);
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf(" 0x%02X", address);
      found++;
    }
  }
  if (found == 0) Serial.print(" none");
  Serial.println();
#else
  Serial.println("[Modules] Port A I2C scan unavailable because NFC library support is not compiled");
#endif
}

bool MissionModules::pollExternalNfc(String& uidOut) {
#if MISSION_NFC_AVAILABLE
  if (!_nfcReady) return false;
  std::vector<m5::nfc::a::PICC> piccs;
  if (!nfcA.detect(piccs) || piccs.empty()) return false;

  uidOut = "";
  const auto& picc = piccs.front();
  for (uint8_t index = 0; index < picc.size; index++) {
    char byteText[4];
    snprintf(byteText, sizeof(byteText), " %02X", picc.uid[index]);
    uidOut += byteText;
  }
  nfcA.deactivate();
  Serial.printf("[NFC:Port A] Card scanned:%s\n", uidOut.c_str());
  return uidOut.length() > 0;
#else
  (void)uidOut;
  return false;
#endif
}

void MissionModules::setState(MissionModuleState state) {
  _state = state;
  _targetReached = false;
  _targetPulseVisible = false;
  _lastTargetPulseAt = millis();
  renderLeds();
}

void MissionModules::setTargetReached(bool reached) {
  if (_targetReached == reached) return;
  _targetReached = reached;
  _targetPulseVisible = reached;
  _lastTargetPulseAt = millis();
  renderLeds();
}

void MissionModules::renderLeds() {
#if MISSION_RGB_AVAILABLE
  switch (_state) {
    case MissionModuleState::IDLE: fillLeds(CRGB::Blue); break;
    case MissionModuleState::PLAYING:
      fillLeds(_targetReached && _targetPulseVisible ? CRGB::Red : CRGB::Orange);
      break;
    case MissionModuleState::WON: fillLeds(CRGB::Green); break;
    case MissionModuleState::LOST: fillLeds(CRGB::Red); break;
  }
#endif
}