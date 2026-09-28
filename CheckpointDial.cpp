#include "CheckpointDial.h"
#include "GameConfiguration.h"
#include "DialScreen.h"
#include <esp_system.h>
#include <SPIFFS.h>

CheckpointDial checkpointDial;

namespace {
const char* RUN_SLOTS[] = { "/dial_run_a.dat", "/dial_run_b.dat" };
uint32_t runChecksum(const String& value) {
  uint32_t crc = 0xffffffffU;
  for (size_t i = 0; i < value.length(); ++i) {
    crc ^= static_cast<uint8_t>(value[i]);
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
  }
  return ~crc;
}
bool readRunSlot(const char* path, String& json, uint32_t& sequence) {
  File file = SPIFFS.open(path, FILE_READ);
  if (!file) return false;
  uint32_t header[4] = {};
  bool valid = file.readBytes(reinterpret_cast<char*>(header), sizeof(header)) == sizeof(header) &&
    header[0] == 0x524B4532 && header[2] <= 24576 && file.size() == sizeof(header) + header[2];
  if (valid) json = file.readString();
  file.close();
  if (!valid || json.length() != header[2] || runChecksum(String(header[3]) + json) != header[1]) return false;
  sequence = header[3]; return true;
}
}

bool CheckpointDial::persist() {
  if (saved.overflowed()) return false;
  String json; serializeJson(saved, json);
  // Large reward manifests exceed NVS string limits. Alternate verified files
  // so a torn write always leaves the previous committed progress recoverable.
  uint32_t next = sequence + 1;
  const char* path = RUN_SLOTS[next % 2];
  File file = SPIFFS.open(path, FILE_WRITE);
  if (!file || json.length() > 24576) return false;
  uint32_t header[] = {0x524B4532, runChecksum(String(next) + json), static_cast<uint32_t>(json.length()), next};
  bool ok = file.write(reinterpret_cast<uint8_t*>(header), sizeof(header)) == sizeof(header) && file.print(json) == json.length();
  file.flush(); file.close();
  String check; uint32_t checkedSequence = 0;
  if (!ok || !readRunSlot(path, check, checkedSequence) || check != json || checkedSequence != next) return false;
  sequence = next; return true;
}
void CheckpointDial::begin(const String& game) {
  gameId = game;
  if (!SPIFFS.begin(false)) { storageReady = false; return; }
  String json;
  for (const char* path : RUN_SLOTS) {
    String candidate; uint32_t candidateSequence = 0;
    if (readRunSlot(path, candidate, candidateSequence) && candidateSequence >= sequence) { sequence = candidateSequence; json = candidate; }
  }
  if (!json.length() && (SPIFFS.exists(RUN_SLOTS[0]) || SPIFFS.exists(RUN_SLOTS[1]))) { storageReady = false; return; }
  if (json.length() && (deserializeJson(saved, json) || saved.overflowed())) { storageReady = false; return; }
  if (!json.length() || String(saved["game_id"] | "") != gameId) {
    saved.clear(); saved["game_id"] = gameId;
    saved.createNestedArray("roster"); storageReady = persist();
  }
  // A reset must never restart the clock or silently discard offline progress.
  if (hasUnpaidRun()) { frozen = true; saved["frozen"] = true; persist(); }
  encoder = M5Dial.Encoder.read(); tick = millis(); lastSave = tick;
  dirty = true;
}
bool CheckpointDial::hasUnpaidRun() const { return String(saved["run_id"] | "").length() && !(saved["paid"] | false); }
bool CheckpointDial::reset(bool factory) {
  if (hasUnpaidRun() && !factory) { show("RUN SAVED", "Return to your NPC before resetting."); return false; }
  if (factory) {
    // Also works when boot configuration failed before begin() read the journal.
    // Erase both generations so an old run cannot return after a factory reset.
    if (!SPIFFS.begin(false)) return false;
    for (const char* path : RUN_SLOTS) if (SPIFFS.exists(path) && !SPIFFS.remove(path)) return false;
    sequence = 0;
  }
  saved.clear(); saved["game_id"] = gameId; saved.createNestedArray("roster");
  frozen = false; page = 0; dirty = true;
  storageReady = persist();
  if (factory && storageReady) storageReady = persist();
  return storageReady;
}
void CheckpointDial::show(const String& label, const String& message, unsigned long duration) {
  noticeLabel = label; notice = message; noticeUntil = millis() + duration; dirty = true;
  Serial.println("[MISSION] " + label + ": " + message);
  DialScreen::message(label, message, hasUnpaidRun() ? "Progress saved on this dial" : "Scan at the NPC checkpoint");
}
int CheckpointDial::visits() const { return saved["visited"].size(); }
JsonObjectConst CheckpointDial::tier() const {
  JsonObjectConst best;
  for (JsonObjectConst row : saved["tiers"].as<JsonArrayConst>())
    if ((row["visits"] | 99) <= visits() && (best.isNull() || row["visits"].as<int>() > best["visits"].as<int>())) best = row;
  return best;
}
void CheckpointDial::tag(const String& raw, const std::function<bool()>& connect, const std::function<void()>& disconnect) {
  if (!storageReady) { show("STORAGE ERROR", "Saved data cannot be read. See event staff."); return; }
  update();
  const String tag = GameConfiguration::normalizeUid(raw);
  if (tag.isEmpty()) { show("TAG NOT READ", "Scan the card again."); return; }
  bool npc = !gameConfiguration.npcIdForTag(tag).isEmpty();
  bool completion = gameConfiguration.isCompletionTag(tag);
  // A cached run keeps its original checkpoint if Creator edits the catalog later.
  if (hasUnpaidRun() && String(saved["npc_uuid"] | "") == tag) npc = true;
  if (npc || completion) { checkpoint(tag, connect, disconnect); return; }
  if (hasUnpaidRun()) {
    if (frozen || (saved["frozen"] | false)) { show("RETURN TO NPC", "Your earned progress is saved. Scan your NPC to finish."); return; }
    bool assigned = false;
    for (JsonObjectConst poi : saved["pois"].as<JsonArrayConst>()) if (String(poi["uuid"] | "") == tag) assigned = true;
    if (!assigned) { show("NOT A MISSION STOP", "Rotate the dial to see your assigned POIs."); return; }
    for (JsonVariantConst previous : saved["visited"].as<JsonArrayConst>()) if (previous.as<String>() == tag) { show("ALREADY VISITED", "This stop is already saved."); return; }
    saved["visited"].as<JsonArray>().add(tag);
    if (!persist()) { saved["visited"].as<JsonArray>().remove(visits() - 1); show("STORAGE ERROR", "Visit was not saved. Scan again."); return; }
    page = 0;
    show("STOP SAVED", visits() == 4 ? "All four visited. Return to your NPC." : String(visits()) + " of 4 complete", 1400);
    return;
  }
  if (gameConfiguration.findUuid(tag) >= 0) { show("START AT YOUR NPC", "Scan player badges, then the NPC mission card."); return; }
  // Until a manifest is accepted there are no earned visits to lose. Changing
  // the crew may abandon an uncertain empty assignment, never a paid run.
  saved.remove("request_id");
  JsonArray roster = saved["roster"].as<JsonArray>();
  for (unsigned i = 0; i < roster.size(); ++i) if (roster[i].as<String>() == tag) {
    roster.remove(i); persist(); show("PLAYER REMOVED", String(roster.size()) + " badges ready", 1200); return;
  }
  if (roster.size() >= 8) { show("CREW FULL", "Up to eight badges can join."); return; }
  roster.add(tag);
  if (!persist()) { roster.remove(roster.size() - 1); show("STORAGE ERROR", "Badge was not saved. Try again."); return; }
  show("BADGE ADDED", String(roster.size()) + " ready. Scan your NPC to start.", 1500);
}
void CheckpointDial::checkpoint(const String& tag, const std::function<bool()>& connect, const std::function<void()>& disconnect) {
  String source = hasUnpaidRun() ? String(saved["npc_uuid"] | "") : tag;
  if (hasUnpaidRun() && tag != source && !gameConfiguration.isCompletionTag(tag)) { show("RETURN TO YOUR NPC", "Finish at the NPC where this run started."); return; }
  if (!hasUnpaidRun() && gameConfiguration.isCompletionTag(tag)) { show("NO ACTIVE RUN", "Scan a player badge and an NPC mission card."); return; }
  if (!saved["roster"].size()) { show("ADD YOUR CREW", "Scan player badges first, then this NPC card."); return; }
  DialScreen::message("AT THE CHECKPOINT", "Connecting", "Keep the dial here");
  if (!connect()) { disconnect(); show("NO CONNECTION", "Progress is safe. Stay at the checkpoint and scan again."); return; }
  if (hasUnpaidRun() && !finish(tag)) { disconnect(); return; }
  start(source);
  disconnect();
  tick = millis(); lastSave = tick;
}
bool CheckpointDial::finish(const String& tag) {
  DynamicJsonDocument payload(2048);
  payload["game_id"] = gameId; payload["operation"] = "finish";
  payload["run_id"] = saved["run_id"]; payload["checkpoint_uuid"] = tag;
  payload["visited_uuids"].set(saved["visited"]);
  String response;
  DialScreen::message("CONFIRMING REWARDS", "Sending saved progress", "Do not leave the checkpoint yet");
  if (!gameConfiguration.requestAction("missionCheckpoint", payload.as<JsonObjectConst>(), response)) { show("PAYMENT NOT CONFIRMED", gameConfiguration.error(), 7000); return false; }
  DynamicJsonDocument receipt(4096);
  if (deserializeJson(receipt, response) || !(receipt["settled"] | false) || receipt["run_id"].as<String>() != saved["run_id"].as<String>()) { show("RETRY CHECKPOINT", "No payment receipt received. Progress kept."); return false; }
  saved["paid"] = true;
  saved["reward_paid"] = receipt["paid"] | false;
  if (!persist()) { saved["paid"] = false; show("STORAGE ERROR", "Scan again to recover your payment receipt."); return false; }
  DialScreen::message((receipt["paid"] | false) ? "REWARDS CONFIRMED" : "RUN CLOSED", (receipt["paid"] | false) ? String(receipt["difficulty"] | "Mission") + " paid" : "No POIs visited. No rewards paid.", "Getting the next assignment");
  Serial.println(String("[MISSION] Settlement confirmed: ") + String(receipt["difficulty"] | "None") + ((receipt["paid"] | false) ? " rewards paid" : " no rewards"));
  delay(1500);
  return true;
}
bool CheckpointDial::start(const String& source) {
  // This write follows a durable payment receipt, never an uncertain response.
  if (!String(saved["request_id"] | "").length() || (saved["paid"] | false)) {
    DynamicJsonDocument next(16384);
    next["game_id"] = gameId; next["roster"].set(saved["roster"]);
    next["last_paid"] = (saved["reward_paid"] | false) || (saved["last_paid"] | false);
    next["npc_uuid"] = source;
    char requestId[33]; snprintf(requestId, sizeof(requestId), "%08lx%08lx%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random());
    next["request_id"] = requestId;
    saved.set(next);
    if (!persist()) { show("STORAGE ERROR", "Cannot save the next request. Scan again."); return false; }
  }
  if (!persist()) { show("STORAGE ERROR", "Cannot save the request. Scan again."); return false; }
  DynamicJsonDocument payload(2048);
  payload["game_id"] = gameId; payload["operation"] = "start";
  payload["request_id"] = saved["request_id"]; payload["npc_uuid"] = saved["npc_uuid"];
  payload["player_uuids"].set(saved["roster"]);
  String response;
  DialScreen::message("GETTING YOUR MISSION", "Downloading four POIs", "The field run works offline");
  if (!gameConfiguration.requestAction("missionCheckpoint", payload.as<JsonObjectConst>(), response)) { show((saved["last_paid"] | false) ? "PAID / NEXT RUN" : "MISSION NOT STARTED", gameConfiguration.error(), 8000); return false; }
  DynamicJsonDocument result(16384);
  if (deserializeJson(result, response) || result.overflowed() || !(result["ok"] | false) || result["pois"].size() != 4 || result["run_id"].as<String>().length() != 64) { show("MISSION NOT SAVED", "Invalid assignment. Scan the NPC to retry."); return false; }
  if (result["device"]["resolved_game_id"].as<String>() != gameId) { show("WRONG GAME", "Check the widget assignment in Creator."); return false; }
  // Verify identities, never guess a tag by the displayed POI name.
  for (JsonObject poi : result["pois"].as<JsonArray>()) {
    String uuid = GameConfiguration::normalizeUid(poi["uuid"] | "");
    int index = gameConfiguration.findUuid(uuid);
    if (index < 0 || index != gameConfiguration.findId(poi["id"] | "")) { show("UPDATE GAME SETUP", "A POI tag changed. Refresh setup before starting."); return false; }
    poi["uuid"] = uuid;
  }
  saved["run_id"] = result["run_id"]; saved["pois"].set(result["pois"]);
  saved["tiers"].set(result["tiers"]); saved["players"].set(result["players"]);
  saved["mission_name"] = result["mission"]["name"];
  saved["npc_name"] = result["source"]["npc_name"];
  saved["remaining_ms"] = result["round"]["duration_ms"] | (15UL * 60000UL);
  saved["paid"] = false; saved["frozen"] = false; saved.createNestedArray("visited");
  if (!persist()) { saved.remove("run_id"); show("MISSION NOT SAVED", "Storage write failed. Scan the NPC to retry."); return false; }
  frozen = false; page = 0; noticeUntil = 0; dirty = true;
  Serial.println("[MISSION] Assignment saved: 4 POIs. Visit offline, then return to " + String(saved["npc_name"] | "your NPC"));
  return true;
}
void CheckpointDial::press() { page = page < 4 ? 4 : 0; dirty = true; noticeUntil = 0; }
void CheckpointDial::update() {
  unsigned long now = millis();
  long current = M5Dial.Encoder.read();
  if (abs(current - encoder) >= 2) { page = (page + (current > encoder ? 1 : 4)) % 5; encoder = current; dirty = true; noticeUntil = 0; }
  if (hasUnpaidRun() && !frozen && now - tick >= 1000) {
    unsigned long remaining = saved["remaining_ms"] | 0UL, elapsed = now - tick;
    saved["remaining_ms"] = remaining > elapsed ? remaining - elapsed : 0UL;
    tick = now; dirty = true;
    if (remaining <= elapsed) { frozen = true; saved["frozen"] = true; }
    if (frozen || now - lastSave >= 5000) { if (!persist()) show("STORAGE ERROR", "Return to the NPC. Keep the dial powered."); lastSave = now; }
  }
  if (noticeUntil && (long)(now - noticeUntil) < 0) return;
  if (noticeUntil) { noticeUntil = 0; dirty = true; }
  if (dirty) { dirty = false; render(); }
}
void CheckpointDial::render() {
  if (!storageReady) { DialScreen::message("STORAGE ERROR", "Saved data cannot be read.", "See event staff before resetting"); return; }
  if (!hasUnpaidRun()) {
    if (String(saved["request_id"] | "").length()) { DialScreen::message((saved["last_paid"] | false) ? "REWARDS CONFIRMED" : "CREW SAVED", "Scan your NPC to get the next available mission.", "Internet needed only here"); return; }
    DialScreen::message(gameConfiguration.name(), saved["roster"].size() ? String(saved["roster"].size()) + " badges ready. Scan your NPC." : "Scan your player badge", "Add crew badges, then scan NPC"); return;
  }
  uint16_t color = DialScreen::accent();
  String faction = saved["players"][0]["faction_color"] | "";
  if (faction.length() == 7 && faction[0] == '#') { uint32_t rgb = strtoul(faction.c_str() + 1, nullptr, 16); color = M5Dial.Display.color565(rgb >> 16, rgb >> 8, rgb); }
  unsigned long seconds = (saved["remaining_ms"] | 0UL) / 1000;
  char clock[18]; snprintf(clock, sizeof(clock), "%lu:%02lu  OFFLINE", seconds / 60, seconds % 60);
  DialScreen::base(frozen ? "RETURN TO NPC" : String(clock), color);
  JsonObjectConst earned = tier();
  DialScreen::line(String(visits()) + "/4 visited  |  " + String(earned["difficulty"] | "No reward yet"), 65, 1, DialScreen::muted());
  if (page == 4) {
    DialScreen::line("EXPECTED REWARD", 89, 1, DialScreen::accent());
    DialScreen::line(earned["difficulty"] | "Visit a POI", 113, 2);
    String preview;
    JsonArrayConst rewards = earned["player_rewards"].as<JsonArrayConst>();
    if (rewards.size()) preview = String(rewards[0]["amount"].as<int>()) + " x " + String(rewards[0]["name"] | "item");
    else preview = "Faction rewards only";
    DialScreen::wrap(earned.isNull() ? "Rewards unlock as you visit." : preview, 140, 1, 2);
    DialScreen::line(String(rewards.size()) + " player / " + String(earned["faction_rewards"].size()) + " faction items", 173, 1, DialScreen::muted());
  } else {
    JsonObjectConst poi = saved["pois"][page];
    bool done = false; for (JsonVariantConst tag : saved["visited"].as<JsonArrayConst>()) if (tag.as<String>() == poi["uuid"].as<String>()) done = true;
    DialScreen::line(String(done ? "VISITED" : "SCAN AT THIS POI") + "  " + String(page + 1) + "/4", 91, 1, done ? DialScreen::accent() : TFT_WHITE);
    DialScreen::wrap(poi["name"] | "Mission stop", 120, 2, 3);
  }
  for (int i = 0; i < 4; ++i) {
    bool done = false; for (JsonVariantConst tag : saved["visited"].as<JsonArrayConst>()) if (tag.as<String>() == saved["pois"][i]["uuid"].as<String>()) done = true;
    if (done) M5Dial.Display.fillCircle(90 + i * 20, 191, 4, color); else M5Dial.Display.drawCircle(90 + i * 20, 191, 4, DialScreen::muted());
  }
  DialScreen::line(frozen || visits() == 4 ? "Return to NPC to collect" : "Turn: stops / Push: loot", 212, 1, DialScreen::muted(), 155);
}
