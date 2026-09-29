// g++ -std=c++17 -DMISSION_CONFIG_HOST_TEST -Itests/host -I<ArduinoJson/src> ...
#define MISSION_TRANSPORT_HOST_TEST
#include "host/MissionTransport.h"
#include "../GameConfiguration.cpp"
#include <iostream>
const char* reservedGameTagRole(const String&) { return nullptr; }

String challenge(char nonce) {
  return String("{\"type\":\"authChallenge\",\"auth\":{\"nonce\":\"") + String(48, nonce) + "\"}}";
}
const String success = "{\"ok\":true,\"body\":{\"ok\":true,\"settled\":true}}";
void script(std::initializer_list<NetworkReply> replies) {
  assert(activeClients == 0);
  networkReplies = replies; networkCursor = 0; stoppedClients = 0;
  postedRequests.clear(); requestedUrls.clear();
  MissionDeviceAuth::clearChallenge(); MissionDeviceAuth::signAvailable = true;
}
void consumed() {
  assert(networkCursor == networkReplies.size());
  assert(stoppedClients == networkCursor && activeClients == 0);
  assert(MissionDeviceAuth::challengeNonce.empty());
}
int main() {
  GameConfiguration config;
  DynamicJsonDocument payload(2048);
  payload["game_id"] = "111111111111111111111111";
  payload["operation"] = "badge_catalog";
  String result = "stale response";
  // The reported failure: badge download/next challenge fails once, then recovers.
  script({{false, -1, ""}, {false, 200, challenge('a')}, {true, 200, success}});
  assert(config.requestAction("missionCheckpoint", payload.as<JsonObjectConst>(), result));
  consumed(); assert(postedRequests.size() == 1 && config.error().empty());
  assert(result.find("settled") != String::npos);

  script({{false, -1, ""}, {false, -1, ""}, {false, -1, ""}});
  auto before = millis();
  assert(!config.requestAction("missionCheckpoint", payload.as<JsonObjectConst>(), result));
  consumed(); assert(postedRequests.empty() && result.empty() && millis() - before == 1500);
  assert(config.error().find("Server connection failed") != String::npos);

  script({{false, 200, "", -11}, {false, 200, "", -11}, {false, 200, "", -11}});
  assert(!config.requestAction("missionCheckpoint", payload.as<JsonObjectConst>(), result)); consumed();
  assert(config.error().find("interrupted") != String::npos);

  // A missing receipt must preserve the exact finish request and use new proofs.
  payload["operation"] = "finish"; payload["run_id"] = String(64, 'f');
  payload["player_uuids"].to<JsonArray>().add("AABBCCDD");
  payload["visited_uuids"].to<JsonArray>().add("11223344");
  script({{false, 200, challenge('a')}, {true, 200, "", -11},
          {false, 200, challenge('b')}, {true, 200, success}});
  assert(config.requestAction("missionCheckpoint", payload.as<JsonObjectConst>(), result));
  consumed(); assert(postedRequests.size() == 2);
  DynamicJsonDocument first(4096), second(4096);
  deserializeJson(first, postedRequests[0]); deserializeJson(second, postedRequests[1]);
  String firstPayload, secondPayload;
  serializeJson(first["payload"], firstPayload); serializeJson(second["payload"], secondPayload);
  assert(firstPayload == secondPayload);
  assert(first["challenge_nonce"].as<String>() != second["challenge_nonce"].as<String>());
  assert(first["auth"]["proof"].as<String>() != second["auth"]["proof"].as<String>());

  // Same durable start ID through a failed POST, including a gateway outage.
  payload.clear(); payload["operation"] = "start"; payload["request_id"] = String(32, 'a');
  script({{false, 200, challenge('a')}, {true, -1, ""},
          {false, 200, challenge('b')}, {true, 503, "Service Unavailable"},
          {false, 200, challenge('c')}, {true, 200, success}});
  assert(config.requestAction("missionCheckpoint", payload.as<JsonObjectConst>(), result)); consumed();
  for (const String& posted : postedRequests) {
    deserializeJson(first, posted); assert(first["payload"]["request_id"].as<String>() == String(32, 'a'));
  }

  // No retries for server validation/auth rejection, or a rolled-back crew rejection.
  for (int status : {400, 401, 403, 404, 409, 429}) {
    script({{false, 200, challenge('a')}, {true, status, "{\"ok\":false,\"message\":\"Check game setup\"}"}});
    assert(!config.requestAction("missionCheckpoint", payload.as<JsonObjectConst>(), result)); consumed();
    assert(config.error() == "Check game setup");
  }
  script({{false, 200, challenge('a')}, {true, 200, "{\"ok\":true,\"body\":{\"ok\":true,\"settled\":false,\"crew_editable\":true}}"}});
  assert(config.requestAction("missionCheckpoint", payload.as<JsonObjectConst>(), result)); consumed();
  script({{false, 403, "denied"}});
  assert(!config.requestAction("missionCheckpoint", payload.as<JsonObjectConst>(), result)); consumed();

  // Unknown/missing-ID mutations must not be replayed after an uncertain action.
  payload.remove("request_id");
  script({{false, 200, challenge('a')}, {true, -11, ""}});
  assert(!config.requestAction("missionCheckpoint", payload.as<JsonObjectConst>(), result)); consumed();
  script({{false, 200, challenge('a')}, {true, -11, ""}});
  assert(!config.requestAction("unknownRewardAction", payload.as<JsonObjectConst>(), result)); consumed();

  // Read-only setup is retryable. Bounded readers reject oversized fixed/chunked bodies.
  script({{false, 503, "down"}, {false, 200, challenge('a')}, {true, 200, success}});
  assert(config.requestAction("missionGameConfiguration", payload.as<JsonObjectConst>(), result)); consumed();
  for (int length : {-1, 40000}) {
    script({{false, 200, challenge('a')}, {true, 200, String(40000, 'x'), 0, length}});
    assert(!config.requestAction("missionGameConfiguration", payload.as<JsonObjectConst>(), result)); consumed();
    assert(config.error() == "Server response too large");
  }
  script({{false, 200, "{}"}});
  assert(!config.requestAction("missionGameConfiguration", payload.as<JsonObjectConst>(), result)); consumed();
  assert(config.error().find("Invalid device challenge") != String::npos);
  script({{false, 200, challenge('a')}}); MissionDeviceAuth::signAvailable = false;
  assert(!config.requestAction("missionGameConfiguration", payload.as<JsonObjectConst>(), result)); consumed();
  std::cout << "Mission transport tests passed\n";
}
