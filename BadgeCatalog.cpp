#include "BadgeCatalog.h"
#include "GameConfiguration.h"
#include <SPIFFS.h>
#include <cstring>

BadgeCatalog badgeCatalog;
namespace {
constexpr uint32_t BADGE_MAGIC = 0x42444731;
constexpr size_t MAX_BADGES = 10000;
constexpr size_t UID_BYTES = 11; // Length byte + up to ten actual UID bytes.
const char* BADGE_FILE = "/mission_badges.dat";
const char* BADGE_BACKUP = "/mission_badges.bak";
const char* BADGE_PENDING = "/mission_badges.tmp";
uint32_t badgeCrc(uint32_t crc, const uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
  }
  return crc;
}
bool encodeBadge(const String& tag, uint8_t (&bytes)[UID_BYTES]) {
  String canonical = GameConfiguration::normalizeUid(tag);
  if (canonical.isEmpty() || canonical != tag) return false;
  memset(bytes, 0, UID_BYTES); bytes[0] = tag.length() / 2;
  for (unsigned i = 0; i < bytes[0]; ++i) bytes[i + 1] = strtoul(tag.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
  return true;
}
}
bool BadgeCatalog::read(const char* path, const String& game, const String& wanted, bool& found) const {
  found = false;
  File file = SPIFFS.open(path, FILE_READ);
  if (!file) return false;
  uint32_t magic = 0; char storedGame[25] = {};
  const size_t headerSize = sizeof(magic) + 24, footerSize = sizeof(uint32_t);
  size_t size = file.size();
  bool ok = game.length() == 24 && size >= headerSize + footerSize &&
    (size - headerSize - footerSize) % UID_BYTES == 0 &&
    (size - headerSize - footerSize) / UID_BYTES <= MAX_BADGES &&
    file.readBytes(reinterpret_cast<char*>(&magic), sizeof(magic)) == sizeof(magic) && magic == BADGE_MAGIC &&
    file.readBytes(storedGame, 24) == 24 && String(storedGame) == game;
  uint32_t crc = badgeCrc(0xffffffffU, reinterpret_cast<uint8_t*>(storedGame), 24);
  uint8_t target[UID_BYTES] = {};
  bool search = encodeBadge(wanted, target);
  size_t left = ok ? size - headerSize - footerSize : 0;
  uint8_t block[UID_BYTES * 32];
  while (ok && left) {
    size_t count = left < sizeof(block) ? left : sizeof(block);
    ok = file.readBytes(reinterpret_cast<char*>(block), count) == count;
    if (!ok) break;
    crc = badgeCrc(crc, block, count);
    for (size_t i = 0; i < count; i += UID_BYTES) {
      if (block[i] < 2 || block[i] > 10) ok = false;
      if (search && !memcmp(block + i, target, UID_BYTES)) found = true;
    }
    left -= count;
  }
  uint32_t expected = 0;
  ok = ok && file.readBytes(reinterpret_cast<char*>(&expected), sizeof(expected)) == sizeof(expected) && expected == ~crc;
  file.close();
  if (!ok) found = false;
  return ok;
}
bool BadgeCatalog::load(const String& game) {
  gameId = game; valid = false;
  if (!SPIFFS.begin(false)) return false;
  bool ignored = false;
  valid = read(BADGE_FILE, game, "", ignored) || read(BADGE_BACKUP, game, "", ignored);
  return valid;
}
bool BadgeCatalog::contains(const String& tag) const {
  if (!valid) return false;
  bool found = false;
  if (read(BADGE_FILE, gameId, tag, found)) return found;
  return read(BADGE_BACKUP, gameId, tag, found) && found;
}
bool BadgeCatalog::clear() {
  valid = false; gameId = "";
  if (!SPIFFS.begin(false)) return false;
  for (const char* path : {BADGE_FILE, BADGE_BACKUP, BADGE_PENDING}) if (SPIFFS.exists(path) && !SPIFFS.remove(path)) return false;
  return true;
}
bool BadgeCatalog::refresh(const String& game) {
  if (gameId != game) load(game);
  lastError = "Badge list could not be saved. Retry at the NPC.";
  File file = SPIFFS.open(BADGE_PENDING, FILE_WRITE);
  if (!file || game.length() != 24) return false;
  uint32_t magic = BADGE_MAGIC;
  bool ok = file.write(reinterpret_cast<uint8_t*>(&magic), sizeof(magic)) == sizeof(magic) &&
    file.print(game) == 24;
  uint32_t crc = badgeCrc(0xffffffffU, reinterpret_cast<const uint8_t*>(game.c_str()), 24);
  int offset = 0, total = -1;
  String revision, previous;
  while (ok) {
    DynamicJsonDocument request(512);
    request["game_id"] = game; request["operation"] = "badge_catalog";
    request["offset"] = offset; if (offset) request["revision"] = revision;
    String response;
    if (!gameConfiguration.requestAction("missionCheckpoint", request.as<JsonObjectConst>(), response)) { lastError = gameConfiguration.error(); ok = false; break; }
    DynamicJsonDocument page(8192);
    if (deserializeJson(page, response) || page.overflowed() || !(page["ok"] | false) ||
        String(page["game_id"] | "") != game || !page["offset"].is<int>() || page["offset"].as<int>() != offset ||
        !page["total"].is<int>() || !page["uids"].is<const char*>() || String(page["revision"] | "").length() != 64) { ok = false; break; }
    int returnedTotal = page["total"].as<int>();
    if (returnedTotal < 0 || returnedTotal > int(MAX_BADGES)) { lastError = "Badge list exceeds dial capacity (10000)."; ok = false; break; }
    if (!offset) { total = returnedTotal; revision = String(page["revision"] | ""); }
    if (total != returnedTotal || revision != String(page["revision"] | "")) { ok = false; break; }
    String tags = page["uids"] | "";
    int start = 0, count = 0;
    while (ok && start < int(tags.length())) {
      int end = tags.indexOf('\n', start); if (end < 0) end = tags.length();
      String tag = tags.substring(start, end); uint8_t bytes[UID_BYTES];
      if (++count > 256 || !encodeBadge(tag, bytes) || (previous.length() && tag <= previous)) { ok = false; break; }
      ok = file.write(bytes, sizeof(bytes)) == sizeof(bytes);
      crc = badgeCrc(crc, bytes, sizeof(bytes)); previous = tag; start = end + 1;
    }
    offset += count;
    if (!ok || offset > total) { ok = false; break; }
    if (page["next_offset"].isNull()) { ok = offset == total; break; }
    if (count != 256 || !page["next_offset"].is<int>() || page["next_offset"].as<int>() != offset || offset >= total) { ok = false; break; }
  }
  crc = ~crc;
  if (ok) ok = file.write(reinterpret_cast<uint8_t*>(&crc), sizeof(crc)) == sizeof(crc);
  file.flush(); file.close();
  bool ignored = false;
  if (ok) ok = read(BADGE_PENDING, game, "", ignored);
  if (ok && SPIFFS.exists(BADGE_BACKUP)) ok = SPIFFS.remove(BADGE_BACKUP);
  if (ok && SPIFFS.exists(BADGE_FILE)) ok = SPIFFS.rename(BADGE_FILE, BADGE_BACKUP);
  if (ok) ok = SPIFFS.rename(BADGE_PENDING, BADGE_FILE);
  if (!ok) return false;
  gameId = game; valid = true; lastError = "";
  Serial.println("[BADGES] Saved " + String(total) + " game badge IDs (" + String(total * UID_BYTES + 32) + " bytes). Field check-in is offline.");
  return true;
}
