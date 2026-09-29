#pragma once
// Scripted network boundary for testing the production requestAction function.
// Authentication is a test double; no provisioned device credentials are loaded.
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cassert>
#include <vector>

constexpr char STORY_ROUND_ENDPOINT[] = "https://example.test/iot/story-round";
constexpr char DEVICE_MAC_ADDR[] = "00:00:00:00:00:00";
constexpr char DEVICE_SERIAL_NUM[] = "TEST-DIAL";
constexpr char FIRMWARE_VERSION[] = "test-version";
constexpr char ROOT_CA_PEM[] = "test-ca";
inline void configTime(int, int, const char*, const char*) {}
struct { int RSSI() { return -79; } } WiFi;
struct { unsigned getFreeHeap() { return 200000; } unsigned getMaxAllocHeap() { return 100000; } } ESP;

class Stream {
 public:
  virtual ~Stream() = default;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t*, size_t) = 0;
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  virtual void flush() = 0;
};
struct NetworkReply {
  bool post;
  int status;
  String body;
  int readError = 0;
  int contentLength = -1;
};
inline std::vector<NetworkReply> networkReplies;
inline std::vector<String> postedRequests, requestedUrls;
inline size_t networkCursor = 0;
inline unsigned activeClients = 0, stoppedClients = 0;
class WiFiClientSecure {
 public:
  bool verified = false, stopped = false;
  WiFiClientSecure() { assert(++activeClients == 1); }
  ~WiFiClientSecure() { assert(stopped); --activeClients; }
  void setCACert(const char* ca) { assert(!strcmp(ca, ROOT_CA_PEM)); verified = true; }
  void setHandshakeTimeout(unsigned timeout) { assert(timeout == 10); }
  int lastError(char* output, size_t size) { snprintf(output, size, "test transport failure"); return -123; }
  void stop() { if (!stopped) ++stoppedClients; stopped = true; }
};
class HTTPClient {
 public:
  NetworkReply reply{};
  bool reuseDisabled = false;
  void setReuse(bool reuse) { assert(!reuse); reuseDisabled = true; }
  void setConnectTimeout(int timeout) { assert(timeout == 10000); }
  void setTimeout(int timeout) { assert(timeout == 10000); }
  bool begin(WiFiClientSecure& client, const String& url) {
    assert(client.verified && reuseDisabled); requestedUrls.push_back(url); return true;
  }
  void addHeader(const char*, const char*) {}
  int consume(bool post) {
    assert(networkCursor < networkReplies.size()); reply = networkReplies[networkCursor++];
    assert(reply.post == post); return reply.status;
  }
  int GET() { return consume(false); }
  int POST(const String& payload) { postedRequests.push_back(payload); return consume(true); }
  int getSize() { return reply.contentLength; }
  int writeToStream(Stream* output) {
    if (reply.readError < 0) return reply.readError;
    return output->write(reinterpret_cast<const uint8_t*>(reply.body.c_str()), reply.body.length()) == reply.body.length() ? int(reply.body.length()) : -10;
  }
  void end() {}
  static String errorToString(int) { return "test connection error"; }
};
namespace MissionDeviceAuth {
inline String challengeNonce;
inline bool signAvailable = true;
inline void clearChallenge() { challengeNonce = ""; }
inline bool acceptChallenge(JsonVariantConst frame) {
  challengeNonce = frame["auth"]["nonce"].as<String>(); return true;
}
inline bool appendProof(JsonObject request, const String&, const String&, const String&) {
  assert(challengeNonce.length() == 48);
  if (!signAvailable) return false;
  request["auth"]["proof"] = String("test-proof-") + challengeNonce;
  return true;
}
}
