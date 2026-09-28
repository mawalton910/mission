# Automatic mission dial setup

Change `DEVICE_GAME_ID` in your local, ignored `secrets.h`. The sketch downloads the game's setup on first boot and saves it in flash. Normal boots and mission resets reuse that setup. A different game ID requires a new download; a full factory reset erases the cache and downloads again at the next boot. An ordinary firmware upload preserves the cache unless you enable **Erase All Flash**.

The device must be assigned to an enabled widget in that same game in Creator. A firmware game ID cannot override the server's widget assignment. Wi-Fi credentials, the device's unique authentication key, and the server CA certificate are provisioned once; they cannot be downloaded through a connection that needs those credentials to exist first.

## What comes from the backend

| Setting | Source / behavior |
| --- | --- |
| POI names, database IDs and physical tag UUIDs | Every POI in the assigned game; replaces `LOCATION_NAMES` and `POI_LOCATIONS` in normal builds. Scanning the POI's actual tag counts its visit. No separate weapon/security/vehicle/money tags are needed. |
| Game name and ID | Assigned game record. |
| Faction IDs, names and colors | Game faction records. `factionName(id)` supplies a cached display lookup. The old `FACTION_FALLBACK_*` arrays in `secrets.h` were unused by this sketch and remain unused; there is no second manual faction list to maintain. |
| Story NPC IDs, names and mission-card UUIDs | Active StoryGiver/StaticGiver contacts. The selected widget's existing `story_mission.npc_id` or equivalent story assignment identifies the default contact. A game with exactly one eligible contact can select it automatically. |
| Several story NPCs without a widget assignment | Scan the desired NPC card once, as before. The dial cannot infer which of several faction contacts the operator intended. That assignment is remembered. |
| Current mission, ordered POIs, round duration, mission name and rewards | Continue to come from the existing live story-round and reward APIs. These are not frozen in the initial setup cache. |

Admin/reset/completion/OTA/Wi-Fi preset cards remain local control cards. There is currently no authoritative game field defining those privileged roles, so arbitrary game loot keywords never become administration commands. Hardware pins, enabled modules, display theme, sound files, network credentials and firmware/OTA settings also stay local. `SafeCrackMiniGame` and `MissionModules` remain hardware/local logic; their source has no additional event catalog to download.

`REMOTE_GAME_CONFIGURATION = true` in `Config.h` is the default. The old POI table is retained only for an explicitly chosen legacy build (`false`), never as a silent fallback when a download fails. Remote mode uses real game POIs even when developer mode is enabled; it does not substitute the old GURU_HOME tags.

## First setup and game changes

1. Assign the provisioned IoT device to exactly one enabled game widget in Creator.
2. Ensure each POI has the UUID of its **actual physical tag**. Setting a UUID in the database does not program a different UID into a physical NFC tag.
3. Set `DEVICE_GAME_ID` in `secrets.h`, compile, and upload for `esp32:esp32:crabik_slot_esp32_s3` (8 MB default partition with SPIFFS).
4. Boot on venue Wi-Fi. The dial synchronizes its clock, authenticates over HTTPS, downloads and validates the setup, and saves it before accepting play.
5. If a default NPC is available, the existing NPC lookup links its mission card. Otherwise, scan the desired NPC card once.

On a game change, old badge/session progress, NPC assignment and saved POI indices are cleared after the new catalog is successfully saved. Wi-Fi and operational mode are retained during that switch. A failed download blocks play and offers a button retry; it never uses the old game's tags. Admin and Wi-Fi configuration remain accessible.

Game setup is intentionally immutable between initial download and factory reset. Editing POI names, tags or NPC defaults in Creator does not update an already cached dial. Full-reset that dial to fetch the new catalog. A live mission referring to an uncached or changed POI is rejected rather than matched by a similar name.

## Cache and transport

- Existing authenticated transport: HTTPS `/iot/universal-dial/challenge`, then `/iot/universal-dial/action` with action `missionGameConfiguration` and payload `{ "game_id": "..." }`. The same action is available over the already authenticated Universal Dial WebSocket envelope.
- Each download uses a new HMAC-SHA256 challenge proof and verifies the TLS server certificate. No device secrets are sent or stored in the configuration response. No WebSocket connection or periodic polling is needed for this once-per-setup download.
- Schema version 1; at most 128 POIs, 32 factions, 32 story contacts, 96 UTF-8 bytes per display name, and a 32 KB serialized catalog. Invalid/missing/duplicate physical tags, control-tag collisions, oversized data, and wrong-game responses are rejected.
- SPIFFS cache has a length and CRC check. A temporary file is validated before replacement; an interrupted replacement can recover the previous committed file for the same game. Cache revision binds saved mission indices to the exact catalog.
- Cached setup and visit scans work offline. Fetching a new round mission, resolving a new NPC assignment, and paying rewards still requires the backend. Cached faction names do not authorize payouts.

## Controls and diagnosis

- `CONFIG` on Serial prints the cached game name, ID and catalog counts, without credentials.
- `FACTORYRESET` on Serial, or an existing full-reset card, clears cached setup and session storage, then reboots. It also clears stored Wi-Fi overrides; compiled venue credentials and the provisioned device key remain in the firmware.
- A normal `RESET` keeps the downloaded catalog and saved Wi-Fi.
- On **GAME SETUP NEEDED / PRESS TO RETRY**, read the Serial `[CONFIG]` message: fix the indicated game assignment, tag, device provisioning, Wi-Fi, clock or storage issue, then press the dial button. The Wi-Fi config tag remains usable while setup is blocked.

Firmware v26.4.3 also shows the failure reason on the dial. A tag collision identifies the POI/NPC name, UUID, and conflicting local control role. The full message remains available with `CONFIG` on Serial if it is too long for the round display.

Heist 2's DNR Medical Center uses `EF96E264`. That UID was removed from the old `COMPLETE_TAGS` list at the operator's request; it remains a POI tag and no longer completes a mission. The other completion cards remain configured. A scan of the current live catalog against all local control-card groups found no remaining overlaps.

The Wi-Fi hostname combines `WIFI_HOSTNAME_PREFIX` and `DEVICE_SERIAL_NUM`, for example `mission-GGInteractiveWidget1`. It is a router label, not the game selector. `DEVICE_GAME_ID` and the server's widget assignment determine the game. Reset reason 11 is the ESP32 core's USB reset reason and is now labeled `USB` instead of `UNKNOWN`.

Do not publish the local `secrets.h` or device-specific compiled binary. The credentials in that file are not part of the source release.

## File audit

| File | Result |
| --- | --- |
| `Config.h` | Enables automatic setup. The old POI/resource-tag table is used only when remote setup is explicitly disabled. Device control cards, screen styling, local timing, and hardware options remain here. |
| `secrets.h` | `DEVICE_GAME_ID` is the event selector. `STORY_NPC_ID` is ignored in remote mode. The faction fallback arrays have no callers. Network settings, endpoint addresses, device authentication and the TLS root certificate remain provisioned locally. |
| `arduino_m5dial_mission.ino` | Loads or downloads setup before accepting play, binds saved progress to the catalog, optionally selects the widget's story contact, and matches live mission POIs using both database ID and UUID. |
| `GameConfiguration.h/.cpp` | New validated catalog, RFID/name lookup, bounded authenticated download, persistent cache, recovery and factory-reset handling. |
| `MissionDeviceAuth.h` | Uses the existing server challenge-proof protocol; credentials are never supplied by the game catalog. |
| `FreeRoamMission.h/.cpp` | Uses downloaded locations for assignment, scanning and display; supports catalogs up to 128 POIs while retaining the existing mission length. |
| `StateManager.h/.cpp` | Catalog binding clears incompatible saved mission indices and NPC assignment on game changes. Normal resets retain the catalog binding and venue Wi-Fi. |
| `Logger.h`, `OTAUpdate.h` | Existing flash initialization/logging and firmware update flow remain. Uploading with Erase All Flash intentionally removes the catalog. |
| `MissionBase.h`, `MissionModules.h/.cpp`, `SafeCrackMiniGame.h/.cpp` | Mission interface and local hardware/minigame behavior; no additional faction or POI catalog to fetch. Existing local NFC/RGB/safe-crack work is preserved. |
| `AudioIndex.h`, `Sounds.h`, `GuruLogo.h` | Local audio/art assets, not event configuration. |
