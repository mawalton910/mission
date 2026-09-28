#include <Arduino.h>
#include <ArduinoJson.h>
#include <M5Dial.h>
#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>
#include <vector>
#define private public
#include "../GameConfiguration.h"
#include "../CheckpointDial.h"
#undef private
#include "../GameConfiguration.cpp"
#include "../CheckpointDial.cpp"
const char* reservedGameTagRole(const String&) { return nullptr; }
std::vector<String> calls;
bool failFinish = false, failStart = false, rejectCrew = false, failReceiptWrite = false;
std::vector<String> finishCrews;
const String runId(64, 'a');
bool GameConfiguration::requestAction(const char*, JsonObjectConst payload, String& response) {
  String operation = payload["operation"] | "";
  calls.push_back(operation);
  if (operation == "finish") { String crew; serializeJson(payload["player_uuids"], crew); finishCrews.push_back(crew); }
  if ((operation == "finish" && failFinish) || (operation == "start" && failStart)) { lastError = "Test network outage"; return false; }
  DynamicJsonDocument result(8000);
  result["ok"] = true; result["run_id"] = runId;
  if (operation == "finish") { result["settled"] = !rejectCrew; result["paid"] = !rejectCrew; result["difficulty"] = "Easy"; if (rejectCrew) { result["crew_editable"] = true; result["message"] = "Invalid badge; scan it out"; } }
  else {
    result["roster_policy"] = payload["roster_policy"] | "starting_crew";
    result["device"]["resolved_game_id"] = "111111111111111111111111";
    result["source"]["npc_name"] = "Select Few Fixer";
    result["mission"]["name"] = "Heist Circuit";
    result["round"]["duration_ms"] = 900000;
    for (int i = 0; i < 4; ++i) { auto row = result["pois"].to<JsonArray>().createNestedObject(); (void)row; break; }
    result.remove("pois");
    JsonArray pois = result.createNestedArray("pois");
    for (int i = 0; i < 4; ++i) { auto row = pois.createNestedObject(); row["id"] = gameConfiguration.pois[i].id; row["uuid"] = gameConfiguration.pois[i].uuid; row["name"] = gameConfiguration.pois[i].name; }
    JsonArray tiers = result.createNestedArray("tiers");
    auto tier = tiers.createNestedObject(); tier["difficulty"] = "Easy"; tier["visits"] = 1;
    auto reward = tier["player_rewards"].to<JsonArray>().createNestedObject(); reward["name"] = "Heist Cash"; reward["amount"] = 10;
    tier.createNestedArray("faction_rewards");
  }
  response.clear(); serializeJson(result, response);
  if (operation == "finish" && failReceiptWrite) storage.rejectWrite = true;
  return true;
}
void screenshot(const char* name) { std::ofstream f(std::string("/out/") + name + ".svg"); f << M5Dial.Display.svg.str() << "</g></svg>"; }
int main() {
  gameConfiguration.valid = true;
  gameConfiguration.gameName = "Heist 2";
  gameConfiguration.npcs.push_back({"000000000000000000000004", "Fixer", "FAAC1307"});
  gameConfiguration.completionTags.push_back({"Turn in", "FAAC1309"});
  for (int i = 0; i < 4; ++i) gameConfiguration.pois.push_back({String(24, char('1'+i)), i ? String("Mission stop ")+String(i+1) : String("Armageddon Power Station"), String("ABCD000")+String(i+1)});
  auto online = [](){ return true; }; int disconnects = 0; auto disconnect = [&](){ disconnects++; };
  CheckpointDial dial; dial.begin("111111111111111111111111"); dial.update(); screenshot("badge");
  // The NPC starts immediately, before any player has checked in.
  dial.tag("FAAC1307", online, disconnect); assert(dial.hasUnpaidRun()); assert(calls.size()==1 && calls.back()=="start");
  assert(dial.saved["roster"].size()==0);
  dial.tag("AABBCCDD", online, disconnect); assert(dial.saved["roster"].size()==1);
  dial.update(); screenshot("mission");
  dial.tag("ABCD0001", online, disconnect); assert(dial.visits()==1); assert(calls.size()==1);
  dial.tag("ABCD0001", online, disconnect); assert(dial.visits()==1);
  dial.tag("DEADBEEF", online, disconnect); assert(dial.visits()==1);
  assert(dial.saved["roster"].size()==2 && calls.size()==1); // Late helper joins offline.
  dial.tag("AABBCCDD", online, disconnect); assert(dial.saved["roster"].size()==1);
  assert(dial.saved["roster"][0].as<String>()=="DEADBEEF"); // Original player checks out.
  fakeMillis += 10000; dial.press(); dial.update(); screenshot("reward");
  storage.rejectWrite = true;
  dial.tag("DEADBEEF", online, disconnect); assert(dial.saved["roster"].size()==1);
  dial.tag("AABBCCDD", online, disconnect); assert(dial.saved["roster"].size()==1);
  dial.tag("ABCD0002", online, disconnect); assert(dial.visits()==1);
  size_t noWriteCalls=calls.size(); dial.tag("FAAC1307",online,disconnect);
  assert(calls.size()==noWriteCalls && !(dial.saved["settlement_pending"] | false));
  storage.rejectWrite=false;
  rejectCrew=true; dial.tag("FAAC1307", online, disconnect);
  assert(dial.hasUnpaidRun() && !(dial.saved["settlement_pending"] | false));
  assert(finishCrews.back()=="[\"DEADBEEF\"]");
  dial.tag("DEADBEEF", online, disconnect); assert(dial.saved["roster"].size()==0);
  size_t noCrewCalls=calls.size(); dial.tag("FAAC1307", online, disconnect); assert(calls.size()==noCrewCalls);
  dial.tag("AABBCCDD", online, disconnect); rejectCrew=false;
  failFinish=true; dial.tag("FAAC1307", online, disconnect); assert(dial.hasUnpaidRun()); assert(calls.back()=="finish");
  assert(dial.saved["settlement_pending"] | false);
  dial.tag("DEADBEEF", online, disconnect); dial.tag("AABBCCDD", online, disconnect);
  dial.tag("ABCD0002", online, disconnect);
  assert(dial.saved["roster"].size()==1 && dial.visits()==1); // Ambiguous payout freezes exact request.
  assert(!dial.reset()); assert(dial.visits()==1);
  failFinish=false; failReceiptWrite=true;
  dial.tag("FAAC1307",online,disconnect);
  assert(dial.hasUnpaidRun() && (dial.saved["settlement_pending"] | false));
  assert(calls.back()=="finish"); // A lost receipt write cannot start the next mission.
  storage.rejectWrite=false; failReceiptWrite=false;
  CheckpointDial reboot; reboot.begin("111111111111111111111111"); assert(reboot.hasUnpaidRun()); assert(reboot.visits()==1); assert(reboot.frozen);
  reboot.tag("ABCD0002", online, disconnect); assert(reboot.visits()==1);
  fakeMillis += 10000; reboot.update(); screenshot("recovered");
  failFinish=false; failStart=true;
  reboot.tag("FAAC1309", online, disconnect); assert(!reboot.hasUnpaidRun());
  assert(calls[calls.size()-2]=="finish" && calls.back()=="start");
  assert(finishCrews.back()=="[\"AABBCCDD\"]");
  String pending = reboot.saved["request_id"] | "";
  size_t before = calls.size(); reboot.tag("FAAC1307", online, disconnect);
  assert(calls.size()==before+1 && calls.back()=="start"); assert(String(reboot.saved["request_id"] | "")==pending);
  fakeMillis += 10000; reboot.update(); screenshot("next-round");
  storage.rejectWrite = true;
  reboot.tag("AABBCCDD", online, disconnect);
  assert(reboot.saved["roster"].size()==1 && String(reboot.saved["request_id"] | "")==pending);
  storage.rejectWrite = false;
  reboot.tag("AABBCCDD", online, disconnect);
  assert(reboot.saved["roster"].size()==0 && reboot.saved["last_paid"].isNull());
  assert(String(reboot.saved["request_id"] | "")==pending); // Crew cannot change the shared NPC assignment.
  failStart=false; reboot.tag("FAAC1307", online, disconnect);
  assert(reboot.hasUnpaidRun() && reboot.saved["roster"].size()==0);
  for (int i=0;i<9;++i) reboot.tag(String("AABB000")+String(i),online,disconnect);
  assert(reboot.saved["roster"].size()==8);
  CheckpointDial crewRecovery; crewRecovery.begin("111111111111111111111111");
  assert(crewRecovery.saved["roster"].size()==8);
  crewRecovery.tag("AABB0000",online,disconnect); assert(crewRecovery.saved["roster"].size()==7); // Expired/recovered run still allows crew edits.
  crewRecovery.saved["roster_policy"]="starting_crew";
  crewRecovery.tag("AABB0001",online,disconnect); assert(crewRecovery.saved["roster"].size()==7); // Legacy assignment is not silently changed.
  CheckpointDial different; different.begin("222222222222222222222222"); assert(!different.hasUnpaidRun()); assert(different.saved["roster"].size()==0);
  different.saved["large_reward_fixture"] = String(5000, 'x'); assert(different.persist());
  CheckpointDial large; large.begin("222222222222222222222222"); assert(large.saved["large_reward_fixture"].as<String>().length()==5000);
  storage.files[RUN_SLOTS[(large.sequence + 1) % 2]] = "torn file";
  CheckpointDial recovered; recovered.begin("222222222222222222222222"); assert(recovered.storageReady); assert(recovered.saved["large_reward_fixture"].as<String>().length()==5000);
  CheckpointDial neverStarted; assert(neverStarted.reset(true));
  CheckpointDial afterFactory; afterFactory.begin("222222222222222222222222");
  assert(afterFactory.storageReady && afterFactory.saved["roster"].size()==0);
  assert(afterFactory.saved["large_reward_fixture"].isNull());
  std::cout << "Checkpoint tests passed: NPC-first start, offline crew joins/leaves, final-roster payout, rejected-crew correction, lost-response lock, durable progress, retries, legacy runs, crew limit, power recovery and game isolation.\n";
}
