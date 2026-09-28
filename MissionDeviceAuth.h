#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <mbedtls/md.h>

namespace MissionDeviceAuth {
static String challengeNonce;

static inline bool decodeHex(const char* input, uint8_t* output, size_t length) {
  if (!input || strlen(input) != length * 2) return false;
  for (size_t i = 0; i < length; i++) {
    char pair[3] = { input[i * 2], input[i * 2 + 1], 0 };
    char* end = nullptr;
    const long value = strtol(pair, &end, 16);
    if (!end || *end || value < 0 || value > 255) return false;
    output[i] = static_cast<uint8_t>(value);
  }
  return true;
}

static inline String hex(const uint8_t* input, size_t length) {
  static const char* digits = "0123456789abcdef";
  String output;
  output.reserve(length * 2);
  for (size_t i = 0; i < length; i++) {
    output += digits[(input[i] >> 4) & 15];
    output += digits[input[i] & 15];
  }
  return output;
}

static inline void acceptChallenge(JsonVariantConst frame) {
  if (String(frame["type"] | "") == "authChallenge") challengeNonce = String(frame["auth"]["nonce"] | "");
}

static inline bool ready() {
  return challengeNonce.length() == 48 && strlen(configuredDeviceKeyId()) && strlen(configuredDeviceSecretHex()) == 64;
}

static inline bool appendProof(JsonObject payload, const String& mac, const String& serial, const String& firmware) {
  if (!ready()) return false;
  uint8_t secret[32], nonce[16], digest[32];
  if (!decodeHex(configuredDeviceSecretHex(), secret, sizeof(secret))) return false;
  esp_fill_random(nonce, sizeof(nonce));
  const String clientNonce = hex(nonce, sizeof(nonce));
  String normalizedMac = mac;
  normalizedMac.toUpperCase();
  normalizedMac.replace("-", ":");
  const String canonical = String("rke-iot-auth-v1\n") + challengeNonce + "\n" + clientNonce + "\n" + normalizedMac + "\n" + serial + "\n" + firmware;
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  const bool ok = info && mbedtls_md_hmac(info, secret, sizeof(secret), reinterpret_cast<const unsigned char*>(canonical.c_str()), canonical.length(), digest) == 0;
  memset(secret, 0, sizeof(secret));
  if (!ok) return false;
  JsonObject auth = payload["auth"].to<JsonObject>();
  auth["version"] = 1;
  auth["keyId"] = configuredDeviceKeyId();
  auth["clientNonce"] = clientNonce;
  auth["proof"] = hex(digest, sizeof(digest));
  return true;
}

static inline void clearChallenge() { challengeNonce = ""; }
}
