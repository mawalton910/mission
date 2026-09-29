# Mission dial setup and offline runs (v26.5.4)

Set `DEVICE_GAME_ID` in your ignored `secrets.h`. Assign that provisioned device to an enabled widget in the same game. Wi-Fi, device authentication, TLS, hardware options and privileged control cards remain local.

## Creator setup

Open **Settings → Missions & meters → Mission dial setup**.

- Set each POI's actual physical tag UUID in its POI editor. The setup panel lists these automatically.
- In the **NPC editor → Profile & visibility → NPC UUID**, enter the contact's physical UUID or use **Scan with relay reader**, then save the NPC. Its name and mission card appear automatically in Mission dial setup; do not enter a second card there. A Loot record in the same game may already use that physical tag; the NPC mapping gives it its mission role without claiming Loot.
- Existing legacy mission-card aliases are preserved, while new primary NPC cards come from the NPC editor.
- Optional **Mission completion cards** finish runs at connected checkpoints. Add them in Mission dial setup using the relay-reader scanner or a typed UUID. The original NPC card already starts and finishes its mission, so a separate completion card is not required.
- Configure the NPC's round POI selection and four **Mission Difficulties** (Easy, Medium, Hard, Extreme), including player/faction inventory templates. These templates supply rewards.
- Start an active game round before receiving a mission. Boot setup works without an active round; a new assignment needs an active round and four valid POIs.

## Player flow

1. Boot at venue Wi-Fi. First boot downloads setup; later boots use flash.
2. Scan the NPC mission card to start. No player badge is required first. Up to eight players can check in before or during the mission; scan a badge again to check it out. The NPC downloads an ID-only list of this game's valid badges before the field run. Crew changes use that list offline and are saved on the dial. Unknown cards are rejected without changing the crew. A newly linked badge needs a checkpoint refresh before joining.
3. Wait for **Assignment saved: 4 POIs**. The dial saves the assignment/reward manifest and turns Wi-Fi off.
4. Visit assigned POIs and scan their tags offline. **STOP SAVED** means the visit was persisted. Turn the dial to browse stops, rewards and the checked-in crew; pressing cycles stops → reward → crew. One through four unique visits earn Easy through Extreme. Helpers can join after earlier stops and receive the same earned tier.
5. Return to the same NPC and scan its card. Everyone still checked in receives the full earned tier; checked-out players receive nothing. The highest reached tier pays once; lower tiers do not stack. Faction rewards go once to the first remaining crew member's faction, subject to configured eligibility and limits. At least one player must be checked in to collect an earned reward.
6. The dial saves the exact final crew before submitting payment. A lost connection or receipt freezes crew and stops until the same NPC confirms the result. An explicit **CHECK YOUR CREW** rejection means no payout committed: scan an invalid/ineligible badge out (or have staff fix its game link), then retry the NPC. Only cached game badge IDs can check in offline. The server revalidates the final crew at completion, including game membership, disabled badges and reward eligibility.
7. Only after confirmed settlement does it fetch the current NPC assignment using the same connection. Checked-in badges carry into that next run and can leave or join normally. A player can still claim only once per NPC per round, even by joining another crew. Already-paid players must check out before another same-round crew can collect, or wait for the next round.

A zero-visit run closes without rewards. A return after round end can settle the original run. Time expiry stops new visits. A reboot preserves visits but freezes further offline progress until return to the NPC, preventing a restarted clock from extending play.

This four-POI dial flow uses the NPC's round assignment and Mission Difficulty rewards. It does not replay arbitrary phone story-chain actions or grant their separate completion rewards.

## RFID2 on Port A

The external reader now uses the WS1850S/MFRC522-compatible protocol at **0x28**, on `M5.Ex_I2C` (Port A), like the Buy Station. The built-in Dial reader remains available. The older Unit NFC/ST25R3916 driver (0x50) is not RFID2.

Look for these Serial messages:

```text
[RFID2] Port A SDA=... SCL=... VersionReg=...
[Modules] Unit NFC/RFID2 on Port A: ready
[RFID2:Port A] Card scanned: ...
[MISSION] CHECKED IN: ...
[MISSION] CHECKED OUT: ...
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
- Runs begun with older firmware retain their original fixed crew through settlement. Flexible check-in starts with the next v26.5.1 assignment; updating firmware cannot silently replace a saved run's recipients.
- Changed or uncached assignment POI tags are rejected; refresh setup while idle.
- Remote mode ignores legacy local completion cards and downloads event completion cards from Creator. Admin/reset/OTA/Wi-Fi cards remain local and reserved.
- No field internet or persistent WebSocket is needed. Boot and checkpoint calls use HMAC challenge-authenticated HTTPS actions `missionGameConfiguration` and `missionCheckpoint`.

### Checkpoint connection failures

`Device challenge failed: -1` on older firmware means the HTTPS connection failed before an HTTP response arrived; it is not a badge or mission rejection. A successful badge download followed by this error means the next connection failed. Wi-Fi association alone does not guarantee the server connection will succeed.

From v26.5.4, transient connection/time-out/gateway failures get up to three attempts, with fresh verified TLS connections and 0.5/1-second pauses. Challenge retries always happen before an action; action retries are limited to setup/catalog reads and mission starts/finishes with durable request/run IDs. Each action retry obtains a new authentication challenge and retains the exact mission payload. Server validation/authentication rejections are not retried automatically. Certificate checks remain enabled.

Look for `[NET]` lines identifying `challenge` or `action`, the operation, attempt, HTTP/stream/TLS error, RSSI, free heap and largest allocation. They contain no signed requests or credentials. If all attempts fail, remain at the connected checkpoint and scan the NPC again. Saved progress and any uncertain final payout remain protected. A weak signal such as -79 dBm warrants retrying closer to the access point; the TLS diagnostic helps distinguish connection trouble from certificate/clock problems.

`REMOTE_GAME_CONFIGURATION = false` retains the explicit legacy path. Relay and safe-crack are separate modes. Do not publish local credentials or device-specific compiled binaries.

## Files and build

`GameConfiguration` handles the catalog and authenticated transport; `CheckpointDial` handles crew, offline visits, journal and settlement ordering; `DialScreen.h` renders the compact display; `MissionModules.cpp` and `src/rfid2` handle Port A; the main sketch connects boot/scans/admin to those components.

Compile for `esp32:esp32:crabik_slot_esp32_s3`, with the 8 MB default SPIFFS partition. Ordinary uploads preserve storage unless **Erase All Flash** is enabled. Physical reader and screen behavior still require testing on the uploaded device.

## Offline badge identification and storage

A hexadecimal tag ID alone is not proof of a player badge. Scan routing handles locally reserved controls first, then the saved mission's NPC, configured NPC/mission and completion cards, assigned POIs and other known POIs. Only then can a card reach crew check-in, where it must exist in the game's badge cache. An unknown card displays **BADGE NOT RECOGNIZED** and is not added. With no verified cache, the dial displays **SCAN NPC FIRST**. An older invalid crew entry can still be scanned out, unless payment is uncertain and the exact crew is locked.

At every connected NPC checkpoint, after any prior reward settlement is confirmed and before requesting the next assignment, the dial refreshes badge IDs. No player names, accounts, inventories or photos are downloaded. Disabled badges, duplicate/ambiguous IDs, malformed IDs and tags assigned to this game's POIs/NPCs/completion cards are excluded. A changed list halfway through a paginated download is rejected and can be retried. A failed download never silently replaces the prior cache or starts another mission.

Storage is 32 bytes plus **11 bytes per badge**, regardless of 4-, 7- or 10-byte UID length. A backup is retained; a temporary third file exists during refresh. For 1,000 badges, that is about 22 KB normally or 33 KB during refresh; for 5,000, about 110 KB normally or 165 KB while refreshing. The configured 8 MB partition layout reserves 1.5 MiB for SPIFFS (shared with setup, missions and logs). The limit is 10,000 game badges; insufficient space fails visibly. Membership checks stream through 352-byte blocks from flash. One download page uses a bounded 8 KB JSON buffer, rather than keeping all badge records in RAM.

Factory reset clears all badge-cache generations. Changing games rejects caches with a different game ID. Cached validity can become stale during an offline run; the server always checks current eligibility before paying.

## Which hard-coded tag lists remain?

- **Keep local in Config.h:** `ADMIN_BADGE_TAGS`, `FULL_RESET_TAGS`, `RESET_TAG`, `BADGE_RESET_TAGS`, and `SCRUB_MISSION_TAGS`. Keep only the control cards you actually use. These cards are reserved and must not also be POIs, NPCs or player badges.
- **Keep local in secrets.h / OTAUpdate.h:** device authentication, Wi-Fi credentials/preset tags, OTA update controls, and `DEVICE_GAME_ID`. Do not publish credentials.
- **Configure in Creator:** POI UUIDs, NPC UUIDs/mission card aliases, extra completion cards, faction names/colors, difficulty/reward rules.
- **Archived and inactive in remote mode:** `COMPLETE_TAGS`, `POI_LOCATIONS`, `MISSION_CARD_TAGS`, `FREE_ROAM_MISSION_TAGS` are now in `LegacyGameTags.h`; old `LOCATION_NAMES` in the sketch are also compiled out. With `REMOTE_GAME_CONFIGURATION=true`, no old event UIDs from these arrays enter the firmware. You no longer need to remove individual values. Keep the placeholder declarations/file so legacy code still compiles. Setting remote mode false explicitly restores the archived legacy tables.

POI browsing now needs four encoder counts per page and enforces a 300 ms pause. Fast movement is consumed during that pause, so pages do not keep jumping after the dial stops.
