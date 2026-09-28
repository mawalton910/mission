#include "GameConfiguration.h"
#ifndef MISSION_CONFIG_HOST_TEST
#include "Config.h"
#include "MissionDeviceAuth.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
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
#ifndef MISSION_CONFIG_HOST_TEST
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
bool response(HTTPClient& http, String& text, size_t max) {
  if (http.getSize() > static_cast<int>(max)) return false;
  LimitedResponse output(max);
  if (http.writeToStream(&output) < 0 || output.exceeded) return false;
  text = std::move(output.body);
  return true;
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
  DynamicJsonDocument doc(65536);
  if (deserializeJson(doc, json.c_str()) || doc.overflowed()) return false;
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

#ifndef MISSION_CONFIG_HOST_TEST
bool GameConfiguration::requestAction(const char* action, JsonObjectConst actionPayload, String& responseBody) {
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
  WiFiClientSecure tls;
  tls.setCACert(ROOT_CA_PEM);
  tls.setHandshakeTimeout(10);
  HTTPClient http;
  http.setTimeout(10000);
  String challengeUrl = base + "/challenge?mac_address=" + urlEncode(DEVICE_MAC_ADDR) + "&serial_number=" + urlEncode(DEVICE_SERIAL_NUM);
  lastError = "Device connection failed";
  if (!http.begin(tls, challengeUrl)) return false;
  int status = http.GET();
  String body;
  bool received = status == 200 && response(http, body, 2048);
  http.end();
  if (!received) { lastError = "Device challenge failed: " + String(status); return false; }
  DynamicJsonDocument request(4096);
  {
    DynamicJsonDocument challenge(2048);
    if (deserializeJson(challenge, body)) return false;
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
  if (!signedRequest) { lastError = "Provision device key first"; return false; }
  String payload; serializeJson(request, payload);
  if (!http.begin(tls, base + "/action")) return false;
  http.addHeader("Content-Type", "application/json");
  status = http.POST(payload);
  received = status > 0 && response(http, body, MAX_GAME_CONFIG_BYTES + 1024);
  http.end();
  if (!received) { lastError = "Config download failed: " + String(status); return false; }

  {
    DynamicJsonDocument result(65536);
    if (deserializeJson(result, body) || result.overflowed()) { lastError = "Invalid server response"; return false; }
    if (status != 200 || !(result["ok"] | false) || !(result["body"]["ok"] | false)) {
      const char* message = result["body"]["message"] | "";
      if (!message[0]) message = result["message"] | "Game setup request failed";
      lastError = message;
      return false;
    }
    serializeJson(result["body"], responseBody);
  }
  return true;
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
    DynamicJsonDocument result(65536);
    if (deserializeJson(result, body)) return false;
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
