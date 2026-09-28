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
#include "src/rfid2/PortARfid2.h"
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
PortARfid2 rfid2(0x28);
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
  M5.Ex_I2C.begin();
  if (M5.Ex_I2C.scanID(0x28)) {
    rfid2.PCD_Init();
    uint8_t version = rfid2.PCD_ReadRegister(PortARfid2::VersionReg);
    _nfcReady = version != 0x00 && version != 0xFF;
    Serial.printf("[RFID2] Port A SDA=%d SCL=%d VersionReg=0x%02X\n", M5.Ex_I2C.getSDA(), M5.Ex_I2C.getSCL(), version);
  }
  Serial.printf("[Modules] Unit NFC/RFID2 on Port A: %s\n", _nfcReady ? "ready" : "not found");
  // Keep the full bus diagnostic opt-in (Serial: I2C). The reader probe above
  // is all normal startup needs before drawing the mission screen.
#endif
}

void MissionModules::update() {
#if MISSION_NFC_AVAILABLE
  // RFID2 is polled directly; no background NFC engine is needed.
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
  // Match M5Unified's safe scan range. Probing reserved addresses 0x00-0x07
  // can halt the ESP32-S3 before the display or Serial loop is initialized.
  for (uint8_t address = 0x08; address < 0x78; address++) {
    if (M5.Ex_I2C.scanID(address)) {
      Serial.printf(" 0x%02X", address);
      found++;
    }
    delay(1);
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
  uint8_t atqa[2] = {0, 0}, size = sizeof(atqa);
  uint8_t status = rfid2.PICC_RequestA(atqa, &size);
  if (status != PortARfid2::STATUS_OK && status != PortARfid2::STATUS_COLLISION) return false;
  if (!rfid2.PICC_ReadCardSerial()) return false;
  uidOut = "";
  for (uint8_t index = 0; index < rfid2.uid.size; index++) {
    char byteText[4]; snprintf(byteText, sizeof(byteText), " %02X", rfid2.uid.uidByte[index]);
    uidOut += byteText;
  }
  rfid2.PICC_HaltA(); rfid2.PCD_StopCrypto1();
  Serial.printf("[RFID2:Port A] Card scanned:%s\n", uidOut.c_str());
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
