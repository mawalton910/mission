#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

constexpr int MAX_GAME_POIS = 128;
constexpr size_t MAX_GAME_CONFIG_BYTES = 32768;
struct GamePoi { String id, name, uuid; };
struct GameFaction { String id, name, color; };
struct GameNpc { String id, name, uuid; };

class GameConfiguration {
 public:
  bool load(const String& gameId);
  bool download(const String& gameId);
  bool clear();
  bool ready() const { return valid; }
  void invalidate(const String& reason) { valid = false; lastError = reason; }
  const String& error() const { return lastError; }
  String binding() const { return gameId + ":" + revision; }
  const String& name() const { return gameName; }
  int count() const { return pois.size(); }
  String poiName(int index) const;
  int findUuid(const String& uuid) const;
  int findId(const String& id) const;
  String npcIdForTag(const String& uuid) const;
  String defaultNpcTag() const;
  String factionName(const String& id) const;
  void printStatus() const;
  static String normalizeUid(const String& value);

 private:
  bool valid = false;
  String gameId, gameName, revision, defaultNpcId, lastError;
  std::vector<GamePoi> pois;
  std::vector<GameFaction> factions;
  std::vector<GameNpc> npcs;
  bool parse(const String& json, const String& expectedGame);
  bool readCache(const char* path, const String& expectedGame);
  bool saveCache(const String& json, const String& expectedGame);
};

extern GameConfiguration gameConfiguration;
// Local control/admin cards must never become visit tags through remote configuration.
const char* reservedGameTagRole(const String& canonicalUid);
int missionLocationCount();
String missionLocationName(int index);
int missionLocationForTag(const String& tag);
