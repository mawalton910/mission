# Mission dial setup and offline runs (v26.5.0)

Set `DEVICE_GAME_ID` in your ignored `secrets.h`. Assign that provisioned device to an enabled widget in the same game. Wi-Fi, device authentication, TLS, hardware options and privileged control cards remain local.

## Creator setup

Open **Settings → Missions & meters → Mission dial setup**.

- Set each POI's actual physical tag UUID in its POI editor. The setup panel lists these automatically.
- Add an **NPC / mission card** with its UUID, name and StoryGiver/StaticGiver contact. Save the setup. A Loot card in the same game may supply this physical UUID; the explicit NPC mapping gives it its mission role without claiming that Loot record.
- Existing native NPC mission-card links are automatic; do not duplicate them here.
- Optional **Mission completion cards** finish runs at connected checkpoints. The original NPC card already starts and finishes its mission, so a separate completion card is not required.
- Configure the NPC's round POI selection and four **Mission Difficulties** (Easy, Medium, Hard, Extreme), including player/faction inventory templates. These templates supply rewards.
- Start an active game round before receiving a mission. Boot setup works without an active round; a new assignment needs an active round and four valid POIs.

## Player flow

1. Boot at venue Wi-Fi. First boot downloads setup; later boots use flash.
2. Scan player badges (up to eight), then the NPC mission card. Rescan a badge before departure to remove it.
3. Wait for **Assignment saved: 4 POIs**. The dial saves the assignment/reward manifest and turns Wi-Fi off.
4. Visit assigned POIs and scan their tags offline. **STOP SAVED** means the visit was persisted. Turn the dial to browse stops; press to preview rewards. One through four unique visits earn Easy through Extreme.
5. Return to the same NPC and scan its card. The dial submits visits and saves the server's settlement receipt. The highest reached tier pays once; lower tiers do not stack. Player rewards go to the crew; faction rewards go once to the lead player's faction, subject to configured eligibility and limits.
6. Only after confirmed settlement does it fetch the next available assignment using the same connection. A player can claim once per NPC per round. **PAID / NEXT RUN** may ask you to return next round; it does not mean the completed payment failed.

A zero-visit run closes without rewards. A return after round end can settle the original run. Time expiry stops new visits. A reboot preserves visits but freezes further offline progress until return to the NPC, preventing a restarted clock from extending play.

This four-POI dial flow uses the NPC's round assignment and Mission Difficulty rewards. It does not replay arbitrary phone story-chain actions or grant their separate completion rewards.

## RFID2 on Port A

The external reader now uses the WS1850S/MFRC522-compatible protocol at **0x28**, on `M5.Ex_I2C` (Port A), like the Buy Station. The built-in Dial reader remains available. The older Unit NFC/ST25R3916 driver (0x50) is not RFID2.

Look for these Serial messages:

```text
[RFID2] Port A SDA=... SCL=... VersionReg=...
[Modules] Unit NFC/RFID2 on Port A: ready
[RFID2:Port A] Card scanned: ...
[MISSION] BADGE ADDED: ...
[MISSION] Assignment saved: 4 POIs. Visit offline, then return to ...
[MISSION] STOP SAVED: ...
[MISSION] Settlement confirmed: Easy rewards paid
```

`audioPlayerReady=false` means the optional external audio player is unavailable; it does not block missions.

Hardware references: [M5Stack RFID2](https://docs.m5stack.com/en/unit/rfid2), [M5Stack Unit NFC](https://docs.m5stack.com/en/unit/Unit_NFC). Driver attribution is in `src/rfid2/README.md`.

## Caching and recovery

- Boot schema 1 / profile 2 contains POIs, factions/colors, story NPCs, mission-card aliases, completion cards and difficulty summaries. Upgrading an old profile downloads the expanded setup once.
- Normal boots reuse setup. **Admin → Refresh Game Setup** downloads Creator edits while idle. It refuses during an unpaid run; failed refresh retains the prior valid profile.
- Changing the compiled game ID requires new setup and clears incompatible game progress. A failed download cannot enable the old game's tags.
- `CONFIG` prints status. `RESET` and `NPCCLEAR` clear an idle crew but refuse to discard an unpaid run. They retain game setup.
- `FACTORYRESET` and the full-reset card intentionally erase setup/session state and saved Wi-Fi overrides, then reboot. Do not use them with unsettled visits. Compiled credentials remain.
- Setup has a verified SPIFFS cache. Progress uses two alternating length/CRC-checked SPIFFS files; a torn write recovers the last committed copy. Unreadable storage blocks play.
- Retries keep the same request/run ID. Backend transactions and unique claim records prevent duplicate settlement. A replacement assignment cannot start before payout confirmation is durable.
- Changed or uncached assignment POI tags are rejected; refresh setup while idle.
- Remote mode ignores legacy local completion cards and downloads event completion cards from Creator. Admin/reset/OTA/Wi-Fi cards remain local and reserved.
- No field internet or persistent WebSocket is needed. Boot and checkpoint calls use HMAC challenge-authenticated HTTPS actions `missionGameConfiguration` and `missionCheckpoint`.

`REMOTE_GAME_CONFIGURATION = false` retains the explicit legacy path. Relay and safe-crack are separate modes. Do not publish local credentials or device-specific compiled binaries.

## Files and build

`GameConfiguration` handles the catalog and authenticated transport; `CheckpointDial` handles crew, offline visits, journal and settlement ordering; `DialScreen.h` renders the compact display; `MissionModules.cpp` and `src/rfid2` handle Port A; the main sketch connects boot/scans/admin to those components.

Compile for `esp32:esp32:crabik_slot_esp32_s3`, with the 8 MB default SPIFFS partition. Ordinary uploads preserve storage unless **Erase All Flash** is enabled. Physical reader and screen behavior still require testing on the uploaded device.
