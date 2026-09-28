// Runs the actual firmware parser/cache code against an in-memory flash filesystem.
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#define private public
#include "../GameConfiguration.h"
#undef private
#include "../GameConfiguration.cpp"
const char* reservedGameTagRole(const String& tag) { return tag == "DEADBEEF" ? "mission completion" : nullptr; }

int main(int argc, char** argv) {
  assert(argc == 2);
  {
    DynamicJsonDocument small(0);
    assert(!readConfigurationJson(small, "{\"ok\":true}"));
    assert(small["ok"].as<bool>());
    assert(small.capacity() == 4096); // No unconditional 64 KiB heap allocation.
    String dense = "[0";
    for (int i = 1; i < 1024; ++i) dense += ",0";
    dense += "]";
    DynamicJsonDocument grown(0);
    assert(!readConfigurationJson(grown, dense));
    assert(grown.size() == 1024 && grown.capacity() > 4096 && grown.capacity() <= 65536);
    String excessive = "[0";
    for (int i = 1; i < 5000; ++i) excessive += ",0";
    excessive += "]";
    DynamicJsonDocument bounded(0);
    assert(readConfigurationJson(bounded, excessive) == DeserializationError::NoMemory);
    assert(bounded.capacity() == 65536);
    assert(readConfigurationJson(small, "{\"ok\":") == DeserializationError::IncompleteInput);
  }
  std::ifstream input(argv[1]); std::ostringstream buffer; buffer << input.rdbuf();
  const String json = buffer.str();
  const String game = "111111111111111111111111";
  const String other = "222222222222222222222222";
  GameConfiguration config;
  assert(!config.load(game));
  assert(config.parse(json, game));
  assert(config.count() == 2);
  assert(config.poiName(0) == "New POI");
  assert(config.findUuid(" 04 a1:b2-c3 45 67 89 ") == 0);
  assert(config.findUuid("04?A1B2C3456789") == -1);
  assert(config.findId("000000000000000000000001") == 0);
  assert(config.factionName("000000000000000000000003") == "Select Few");
  assert(config.defaultNpcTag() == "FAAC1307");
  assert(config.saveCache(json, game));
  GameConfiguration reboot;
  assert(reboot.load(game)); // Offline boot reads exactly the saved catalog.
  assert(reboot.binding() == config.binding());
  assert(!reboot.load(other) && !reboot.ready()); // Never reuse another event.
  assert(!config.parse(json.substr(0, json.length() - 6), game));
  auto replace = [&](std::string from, std::string to) {
    String changed = json; auto pos = changed.find(from); assert(pos != std::string::npos); changed.replace(pos, from.size(), to); return changed;
  };
  assert(!config.parse(replace("AABBCCDD", "04A1B2C3456789"), game));
  assert(!config.parse(replace("AABBCCDD", "DEADBEEF"), game));
  assert(config.error().find("DEADBEEF") != std::string::npos);
  assert(config.error().find("mission completion") != std::string::npos);
  assert(config.error().find("POI:") != std::string::npos);
  assert(!config.parse(replace("FAAC1307", "DEADBEEF"), game));
  assert(config.error().find("NPC:") != std::string::npos);
  assert(config.error().find("DEADBEEF") != std::string::npos);
  assert(!config.parse(replace("AABBCCDD", "RF111111"), game));
  assert(!config.parse(replace("\"schema_version\":1", "\"schema_version\":2"), game));
  assert(!config.parse(String(MAX_GAME_CONFIG_BYTES + 1, 'x'), game));
  storage.rejectWrite = true;
  assert(!config.saveCache(json, game) && !config.ready());
  storage.rejectWrite = false;
  assert(reboot.load(game)); // Failed write does not destroy last committed setup.
  storage.rejectRename = true;
  assert(!config.saveCache(json, game));
  storage.rejectRename = false;
  assert(reboot.load(game));
  assert(SPIFFS.rename("/mission_game.json", "/mission_game.bak"));
  assert(reboot.load(game)); // Power loss between the two renames recovers the backup.
  storage.files["/mission_game.json"] = storage.files["/mission_game.bak"];
  storage.files["/mission_game.json"].back() ^= 1;
  assert(reboot.load(game)); // Corrupt primary recovers verified backup.
  SPIFFS.remove("/mission_game.bak");
  assert(!reboot.load(game) && !reboot.ready()); // Corrupt cache alone cannot run a mission.
  assert(config.saveCache(json, game));
  assert(config.saveCache(replace(game, other), other));
  assert(reboot.load(other)); // A successful game switch replaces the previous catalog.
  assert(!reboot.load(game));
  assert(config.clear() && !config.ready());
  assert(!reboot.load(other)); // Factory reset forces download.
  storage.unavailable = true;
  assert(!config.clear());
  std::cout << "Firmware parser, cache, game-switch and recovery checks passed\n";
}
