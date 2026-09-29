#include "GameConfiguration.h"
#ifndef MISSION_CONFIG_HOST_TEST
#include "Config.h"
#include "MissionDeviceAuth.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#endif
#include <SPIFFS.h>
#include <time.h>

GameConfiguration gameConfiguration;
namespace {
const char* CONFIG_CACHE_FILE = "/mission_game.json";
const char* CONFIG_BACKUP_FILE = "/mission_game.bak";
const char* CONFIG_PENDING_FILE = "/mission_game.tmp";
constexpr uint32_t MAGIC = 0x524B4531;
uint32_t checksum(const String& value) {
  uint32_t crc = 0xffffffffU;
  for (size_t i = 0; i < value.length(); ++i) {
    crc ^= static_cast<uint8_t>(value[i]);
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
  }
  return ~crc;
}
bool hexId(const String& value, size_t length) {
  if (value.length() != length) return false;
  for (size_t i = 0; i < length; ++i) if (!isxdigit(static_cast<unsigned char>(value[i]))) return false;
  return true;
}
bool validName(const String& value) { return value.length() > 0 && value.length() <= 96; }
// Most event catalogs need only a small JSON pool. A fixed 64 KiB allocation
// can fail on the ESP32-S3 even with plenty of total free (fragmented) heap.
// Grow only when the parsed document actually needs it, keeping the same cap.
DeserializationError readConfigurationJson(DynamicJsonDocument& doc, const String& json) {
  for (size_t capacity = 4096; capacity <= 65536; capacity *= 2) {
    DynamicJsonDocument candidate(capacity);
    DeserializationError error = deserializeJson(candidate, json.c_str(), json.length());
    if (error != DeserializationError::NoMemory || capacity == 65536) {
      doc = std::move(candidate);
      return error;
    }
  }
  return DeserializationError::NoMemory;
}
#if !defined(MISSION_CONFIG_HOST_TEST) || defined(MISSION_TRANSPORT_HOST_TEST)
String urlEncode(const String& value) {
  String result;
  for (size_t i = 0; i < value.length(); ++i) {
    unsigned char c = value[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.') result += char(c);
    else { char encoded[4]; snprintf(encoded, sizeof(encoded), "%%%02X", c); result += encoded; }
  }
  return result;
}
// Bounds the response even when the server uses chunked transfer without Content-Length.
class LimitedResponse : public Stream {
 public:
  String body;
  size_t limit;
  bool exceeded = false;
  explicit LimitedResponse(size_t max) : limit(max) {}
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* data, size_t size) override {
    if (body.length() + size > limit) { exceeded = true; return 0; }
    if (!body.concat(reinterpret_cast<const char*>(data), size)) return 0;
    return size;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};
constexpr unsigned REQUEST_ATTEMPTS = 3;
bool transientStatus(int status) {
  return status == -1 || status == -2 || status == -3 || status == -4 || status == -5 || status == -7 || status == -11 ||
         status == 408 || status == 500 || status == 502 || status == 503 || status == 504;
}
// These actions are read-only, or use a durable ID to recover the same server
// result. Never replay an arbitrary action after an ambiguous POST failure.
bool replaySafe(const char* action, JsonObjectConst payload) {
  if (!strcmp(action, "missionGameConfiguration")) return true;
  if (strcmp(action, "missionCheckpoint")) return false;
  const String operation = payload["operation"] | "";
  return operation == "badge_catalog" ||
         (operation == "start" && hexId(payload["request_id"] | "", 32)) ||
         (operation == "finish" && hexId(payload["run_id"] | "", 64));
}
struct HttpResponse {
  int status = 0;
  bool received = false, retryable = false, oversized = false;
  String body;
};
HttpResponse exchange(const String& url, const String* payload, size_t max,
                      const char* action, const char* operation, unsigned attempt) {
  // Each request owns a fresh connection. Release TLS before parsing/signing,
  // and never carry a failed socket into the next challenge or action.
  HttpResponse result;
  WiFiClientSecure tls;
  tls.setCACert(ROOT_CA_PEM);
  tls.setHandshakeTimeout(10);
  HTTPClient http;
  http.setReuse(false);
  http.setConnectTimeout(10000);
  http.setTimeout(10000);
  int readError = 0;
  if (http.begin(tls, url)) {
    if (payload) http.addHeader("Content-Type", "application/json");
    result.status = payload ? http.POST(*payload) : http.GET();
    if (result.status > 0) {
      result.oversized = http.getSize() > static_cast<int>(max);
      if (!result.oversized) {
        LimitedResponse output(max);
        readError = http.writeToStream(&output);
        result.oversized = output.exceeded;
        result.received = readError >= 0 && !output.exceeded;
        if (result.received) result.body = std::move(output.body);
      }
    }
  }
  result.retryable = !result.oversized && (transientStatus(result.status) ||
    (result.status == 200 && !result.received && transientStatus(readError)));
  if (result.status != 200 || !result.received) {
    char tlsDetail[120] = {};
    const int tlsError = tls.lastError(tlsDetail, sizeof(tlsDetail));
    Serial.printf("[NET] %s action=%s op=%s attempt=%u/%u HTTP=%d (%s) read=%d TLS=%d (%s) RSSI=%d heap=%u largest=%u oversized=%s\n",
      payload ? "action" : "challenge", action, operation, attempt, REQUEST_ATTEMPTS,
      result.status, result.status < 0 ? HTTPClient::errorToString(result.status).c_str() : "HTTP response",
      readError, tlsError, tlsDetail, int(WiFi.RSSI()), unsigned(ESP.getFreeHeap()),
      unsigned(ESP.getMaxAllocHeap()), result.oversized ? "yes" : "no");
  }
  http.end();
  tls.stop();
  return result;
}
void pauseBeforeRetry(unsigned attempt) {
  Serial.printf("[NET] Retrying checkpoint connection (%u/%u)\n", attempt + 1, REQUEST_ATTEMPTS);
  delay(attempt * 500);
}
#endif
}

String GameConfiguration::normalizeUid(const String& raw) {
  String value;
  for (size_t i = 0; i < raw.length(); ++i) {
    unsigned char c = raw[i];
    if (isspace(c) || c == ':' || c == '-') continue;
    if (!isxdigit(c)) return "";
    value += char(toupper(c));
  }
  return value.length() >= 4 && value.length() <= 20 && value.length() % 2 == 0 ? value : String();
}

bool GameConfiguration::parse(const String& json, const String& expectedGame) {
  valid = false;
  lastError = "Invalid game configuration";
  if (json.length() > MAX_GAME_CONFIG_BYTES) return false;
  DynamicJsonDocument doc(0);
  if (readConfigurationJson(doc, json) || doc.overflowed()) return false;
  String returnedGame = doc["game"]["id"] | "";
  String returnedName = doc["game"]["name"] | "";
  String returnedRevision = doc["revision"] | "";
  if ((doc["profile_version"] | 0) != 2) { lastError = "Mission profile update needed"; return false; }
  if ((doc["schema_version"] | 0) != 1 || !hexId(returnedGame, 24) || returnedGame != expectedGame ||
      !validName(returnedName) || !hexId(returnedRevision, 64)) return false;
  if (!doc["pois"].is<JsonArray>() || !doc["factions"].is<JsonArray>() || !doc["npcs"].is<JsonArray>()) return false;
  JsonArray locations = doc["pois"];
  if (locations.size() < 1 || locations.size() > MAX_GAME_POIS || doc["factions"].size() > 32 || doc["npcs"].size() > 32) return false;
  std::vector<GamePoi> nextPois;
  for (JsonObject row : locations) {
    GamePoi poi{row["id"] | "", row["name"] | "", normalizeUid(row["uuid"] | "")};
    if (!hexId(poi.id, 24) || !validName(poi.name) || poi.uuid.isEmpty()) return false;
    if (const char* role = reservedGameTagRole(poi.uuid)) {
      lastError = String("POI: ") + poi.name + ". Tag " + poi.uuid + " is also a " + role + " card.";
      return false;
    }
    for (const auto& previous : nextPois) if (previous.uuid == poi.uuid || previous.id == poi.id) return false;
    nextPois.push_back(poi);
  }
  std::vector<GameFaction> nextFactions;
  for (JsonObject row : doc["factions"].as<JsonArray>()) {
    GameFaction faction{row["id"] | "", row["name"] | "", row["color"] | ""};
    if (!hexId(faction.id, 24) || !validName(faction.name) || faction.color.length() > 7) return false;
    for (const auto& previous : nextFactions) if (previous.id == faction.id) return false;
    nextFactions.push_back(faction);
  }
  std::vector<GameNpc> nextNpcs;
  String assigned = doc["default_npc_id"] | "";
  bool assignmentFound = assigned.isEmpty();
  for (JsonObject row : doc["npcs"].as<JsonArray>()) {
    GameNpc npc{row["id"] | "", row["name"] | "", normalizeUid(row["uuid"] | "")};
    if (!hexId(npc.id, 24) || !validName(npc.name) || npc.uuid.isEmpty()) return false;
    if (const char* role = reservedGameTagRole(npc.uuid)) {
      lastError = String("NPC: ") + npc.name + ". Tag " + npc.uuid + " is also a " + role + " card.";
      return false;
    }
    for (const auto& previous : nextNpcs) if (previous.id == npc.id || previous.uuid == npc.uuid) return false;
    for (const auto& poi : nextPois) if (poi.uuid == npc.uuid) return false;
    if (npc.id == assigned) assignmentFound = true;
    nextNpcs.push_back(npc);
  }
  if (!assignmentFound) return false;
  if (!doc["mission_tags"].is<JsonArray>() || !doc["completion_tags"].is<JsonArray>() ||
      doc["mission_tags"].size() > 32 || doc["completion_tags"].size() > 32) return false;
  std::vector<GameNpc> nextMissionTags;
  std::vector<GameCard> nextCompletionTags;
  std::vector<String> known;
  for (const auto& poi : nextPois) known.push_back(poi.uuid);
  for (const auto& npc : nextNpcs) known.push_back(npc.uuid);
  for (const char* key : {"mission_tags", "completion_tags"}) {
    for (JsonObject row : doc[key].as<JsonArray>()) {
      String cardName = row["name"] | "", tag = normalizeUid(row["uuid"] | "");
      if (!validName(cardName) || tag.isEmpty()) return false;
      if (const char* role = reservedGameTagRole(tag)) { lastError = cardName + ": tag is also a " + role + " card"; return false; }
      for (const auto& previous : known) if (previous == tag) return false;
      known.push_back(tag);
      if (strcmp(key, "mission_tags") == 0) {
        String npcId = row["npc_id"] | "", npcName = row["npc_name"] | "";
        if (!hexId(npcId, 24) || !validName(npcName)) return false;
        nextMissionTags.push_back({npcId, npcName, tag});
      } else nextCompletionTags.push_back({cardName, tag});
    }
  }
  pois = std::move(nextPois); factions = std::move(nextFactions); npcs = std::move(nextNpcs);
  missionTags = std::move(nextMissionTags); completionTags = std::move(nextCompletionTags);
  gameId = returnedGame; gameName = returnedName; revision = returnedRevision; defaultNpcId = assigned;
  valid = true; lastError = "";
  return true;
}

bool GameConfiguration::readCache(const char* path, const String& expectedGame) {
  File file = SPIFFS.open(path, FILE_READ);
  if (!file) return false;
  uint32_t header[3] = {};
  if (file.readBytes(reinterpret_cast<char*>(header), sizeof(header)) != sizeof(header) || header[0] != MAGIC ||
      header[2] > MAX_GAME_CONFIG_BYTES || file.size() != sizeof(header) + header[2]) { file.close(); return false; }
  String json = file.readString();
  file.close();
  return json.length() == header[2] && checksum(json) == header[1] && parse(json, expectedGame);
}

bool GameConfiguration::load(const String& expectedGame) {
  valid = false;
  if (!SPIFFS.begin(false)) { lastError = "Storage unavailable"; return false; }
  if (readCache(CONFIG_CACHE_FILE, expectedGame) || readCache(CONFIG_BACKUP_FILE, expectedGame)) {
    Serial.println("[CONFIG] Using saved game configuration");
    return true;
  }
  lastError = "Game setup needs download";
  return false;
}

bool GameConfiguration::saveCache(const String& json, const String& expectedGame) {
  File file = SPIFFS.open(CONFIG_PENDING_FILE, FILE_WRITE);
  if (!file) { lastError = "Cannot write game setup"; valid = false; return false; }
  uint32_t header[] = {MAGIC, checksum(json), static_cast<uint32_t>(json.length())};
  bool ok = file.write(reinterpret_cast<uint8_t*>(header), sizeof(header)) == sizeof(header) && file.print(json) == json.length();
  file.flush(); file.close();
  if (ok) ok = readCache(CONFIG_PENDING_FILE, expectedGame);
  if (ok) {
    // Keep the last committed file until its replacement is completely written and verified.
    if (SPIFFS.exists(CONFIG_BACKUP_FILE)) ok = SPIFFS.remove(CONFIG_BACKUP_FILE);
    if (ok && SPIFFS.exists(CONFIG_CACHE_FILE)) ok = SPIFFS.rename(CONFIG_CACHE_FILE, CONFIG_BACKUP_FILE);
    if (ok) ok = SPIFFS.rename(CONFIG_PENDING_FILE, CONFIG_CACHE_FILE);
  }
  if (!ok) { valid = false; lastError = "Game setup was not saved"; return false; }
  SPIFFS.remove(CONFIG_BACKUP_FILE);
  return true;
}

bool GameConfiguration::clear() {
  valid = false; pois.clear(); factions.clear(); npcs.clear(); missionTags.clear(); completionTags.clear();
  if (!SPIFFS.begin(false)) { lastError = "Storage unavailable"; return false; }
  bool ok = true;
  for (const char* path : {CONFIG_CACHE_FILE, CONFIG_BACKUP_FILE, CONFIG_PENDING_FILE}) if (SPIFFS.exists(path) && !SPIFFS.remove(path)) ok = false;
  if (!ok) lastError = "Could not erase game setup";
  return ok;
}

#if !defined(MISSION_CONFIG_HOST_TEST) || defined(MISSION_TRANSPORT_HOST_TEST)
bool GameConfiguration::requestAction(const char* action, JsonObjectConst actionPayload, String& responseBody) {
  responseBody = "";
  lastError = "";
  String endpoint = STORY_ROUND_ENDPOINT;
  int slash = endpoint.indexOf('/', 8);
  if (!endpoint.startsWith("https://") || slash < 0) { lastError = "HTTPS server required"; return false; }
  String base = endpoint.substring(0, slash) + "/iot/universal-dial";
  // TLS must verify the certificate; synchronize the clock before the first secure request.
  if (time(nullptr) < 1704067200) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    unsigned long start = millis();
    while (time(nullptr) < 1704067200 && millis() - start < 10000) delay(50);
  }
  if (time(nullptr) < 1704067200) { lastError = "Clock sync failed; retry"; return false; }
  const String challengeUrl = base + "/challenge?mac_address=" + urlEncode(DEVICE_MAC_ADDR) + "&serial_number=" + urlEncode(DEVICE_SERIAL_NUM);
  const bool canReplay = replaySafe(action, actionPayload);
  const char* operation = actionPayload["operation"] | "setup";
  for (unsigned attempt = 1; attempt <= REQUEST_ATTEMPTS; ++attempt) {
    String body;
    int status = 0;
    {
      // A used challenge is never reused, even if its POST receipt was lost.
      MissionDeviceAuth::clearChallenge();
      HttpResponse challengeResponse = exchange(challengeUrl, nullptr, 2048, action, operation, attempt);
      if (!challengeResponse.received || challengeResponse.status != 200) {
        lastError = challengeResponse.oversized ? "Device challenge too large" :
          challengeResponse.status <= 0 ? "Server connection failed; scan again near WiFi" :
          !challengeResponse.received ? "Device challenge interrupted; scan again near WiFi" :
          "Device challenge failed: HTTP " + String(challengeResponse.status);
        if (challengeResponse.retryable && attempt < REQUEST_ATTEMPTS) { pauseBeforeRetry(attempt); continue; }
        return false;
      }
      String payload;
      {
        DynamicJsonDocument request(4096);
        {
          DynamicJsonDocument challenge(2048);
          if (deserializeJson(challenge, challengeResponse.body) ||
              String(challenge["type"] | "") != "authChallenge" ||
              !hexId(challenge["auth"]["nonce"] | "", 48)) {
            lastError = "Invalid device challenge; scan again";
            return false;
          }
          MissionDeviceAuth::acceptChallenge(challenge.as<JsonVariantConst>());
          request["challenge_nonce"] = challenge["auth"]["nonce"];
        }
        request["mac_address"] = DEVICE_MAC_ADDR;
        request["serial_number"] = DEVICE_SERIAL_NUM;
        String firmware = FIRMWARE_VERSION; firmware.trim();
        request["firmware_version"] = firmware;
        request["action"] = action;
        request["payload"].set(actionPayload);
        bool signedRequest = MissionDeviceAuth::appendProof(request.as<JsonObject>(), DEVICE_MAC_ADDR, DEVICE_SERIAL_NUM, firmware);
        MissionDeviceAuth::clearChallenge();
        if (!signedRequest || request.overflowed()) { lastError = "Could not sign device request"; return false; }
        serializeJson(request, payload);
      }
      challengeResponse.body = "";
      HttpResponse reply = exchange(base + "/action", &payload, MAX_GAME_CONFIG_BYTES + 1024, action, operation, attempt);
      if (canReplay && reply.retryable && attempt < REQUEST_ATTEMPTS) { pauseBeforeRetry(attempt); continue; }
      if (!reply.received) {
        lastError = reply.oversized ? "Server response too large" : "Server response not confirmed; scan again near WiFi";
        return false;
      }
      status = reply.status;
      body = std::move(reply.body);
    } // Destroy HTTP/TLS and signing buffers before allocating the JSON pool.
    DynamicJsonDocument result(0);
    DeserializationError jsonError = readConfigurationJson(result, body);
    if (jsonError || result.overflowed()) {
      // Report transport/parser metadata only; never log signed requests or credentials.
      Serial.printf("[NET] Response parse failed: HTTP=%d bytes=%u error=%s overflow=%s heap=%u largest=%u\n",
                    status, unsigned(body.length()), jsonError.c_str(), result.overflowed() ? "yes" : "no",
                    unsigned(ESP.getFreeHeap()), unsigned(ESP.getMaxAllocHeap()));
      lastError = String("Server response: ") + jsonError.c_str() + " (HTTP " + String(status) + ")";
      return false;
    }
    if (status != 200 || !(result["ok"] | false) || !(result["body"]["ok"] | false)) {
      const char* message = result["body"]["message"] | "";
      if (!message[0]) message = result["message"] | "Server request failed";
      lastError = message;
      return false;
    }
    serializeJson(result["body"], responseBody);
    lastError = "";
    return true;
  }
  return false;
}

bool GameConfiguration::download(const String& expectedGame) {
  valid = false;
  if (!hexId(expectedGame, 24)) { lastError = "Check DEVICE_GAME_ID"; return false; }
  DynamicJsonDocument payload(128);
  payload["game_id"] = expectedGame;
  String body;
  if (!requestAction("missionGameConfiguration", payload.as<JsonObjectConst>(), body)) return false;
  String configJson;
  {
    DynamicJsonDocument result(0);
    if (readConfigurationJson(result, body)) return false;
    serializeJson(result["configuration"], configJson);
  }
  body = "";
  if (!parse(configJson, expectedGame) || !saveCache(configJson, expectedGame)) return false;
  Serial.println("[CONFIG] Downloaded and saved game configuration");
  return true;
}

#endif

String GameConfiguration::poiName(int index) const { return valid && index >= 0 && index < count() ? pois[index].name : String(); }
int GameConfiguration::findUuid(const String& raw) const {
  const String tag = normalizeUid(raw);
  if (valid && !tag.isEmpty()) for (int i = 0; i < count(); ++i) if (pois[i].uuid == tag) return i;
  return -1;
}
int GameConfiguration::findId(const String& id) const {
  if (valid) for (int i = 0; i < count(); ++i) if (pois[i].id == id) return i;
  return -1;
}
String GameConfiguration::npcIdForTag(const String& raw) const {
  const String tag = normalizeUid(raw);
  if (valid) for (const auto& npc : npcs) if (npc.uuid == tag) return npc.id;
  if (valid) for (const auto& npc : missionTags) if (npc.uuid == tag) return npc.id;
  return "";
}
String GameConfiguration::npcNameForTag(const String& raw) const {
  const String tag = normalizeUid(raw);
  if (valid) for (const auto& npc : npcs) if (npc.uuid == tag) return npc.name;
  if (valid) for (const auto& npc : missionTags) if (npc.uuid == tag) return npc.name;
  return "";
}
bool GameConfiguration::isCompletionTag(const String& raw) const {
  const String tag = normalizeUid(raw);
  if (valid) for (const auto& card : completionTags) if (card.uuid == tag) return true;
  return false;
}
String GameConfiguration::defaultNpcTag() const {
  if (valid) for (const auto& npc : npcs) if (npc.id == defaultNpcId) return npc.uuid;
  return "";
}
String GameConfiguration::factionName(const String& id) const {
  if (valid) for (const auto& faction : factions) if (faction.id == id) return faction.name;
  return "";
}
void GameConfiguration::printStatus() const {
  Serial.printf("[CONFIG] ready=%s game=%s name=%s POIs=%u factions=%u missionCards=%u completionCards=%u\n", valid ? "yes" : "no", gameId.c_str(), gameName.c_str(), unsigned(pois.size()), unsigned(factions.size()), unsigned(npcs.size() + missionTags.size()), unsigned(completionTags.size()));
  if (!lastError.isEmpty()) Serial.println("[CONFIG] " + lastError);
}

#ifndef MISSION_CONFIG_HOST_TEST
int missionLocationCount() { return REMOTE_GAME_CONFIGURATION ? (gameConfiguration.ready() ? gameConfiguration.count() : 0) : TOTAL_LOCATIONS; }
String missionLocationName(int index) {
  if (REMOTE_GAME_CONFIGURATION) return gameConfiguration.poiName(index);
  return index >= 0 && index < TOTAL_LOCATIONS ? String(LOCATION_NAMES[index]) : String();
}
int missionLocationForTag(const String& raw) {
  if (REMOTE_GAME_CONFIGURATION) return gameConfiguration.findUuid(raw);
  const String tag = GameConfiguration::normalizeUid(raw);
  if (tag.isEmpty()) return -1;
  for (int i = 0; i < TOTAL_LOCATIONS; ++i) {
    const LocationInfo& loc = POI_LOCATIONS[i];
    const String* sets[] = {loc.weaponTags, loc.securityTags, loc.vehicleTags, loc.moneyTags};
    for (const auto* set : sets) for (int j = 0; j < 3; ++j) if (GameConfiguration::normalizeUid(set[j]) == tag) return i;
  }
  return -1;
}
#endif
