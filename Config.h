#pragma once
#include <Arduino.h>
#include "secrets.h"

// ===== FIRMWARE VERSION =====
#define FIRMWARE_VERSION "POIStory v26.5.4"

// Normal use: change only DEVICE_GAME_ID in your ignored secrets.h.
// False is an explicit legacy/offline-table build, never an automatic fallback.
#define REMOTE_GAME_CONFIGURATION true

// ===== OPERATIONAL MODES =====
// Device can operate in two modes, switchable via admin menu
enum OperationalMode {
    MODE_MISSION_WIDGET = 0,       // Shared NPC story mission using the assigned tag
    MODE_RELAY = 1,                // Preserve the original persisted Relay value
    MODE_STORY_MISSION_WIDGET = 2  // Manual mission-token scan compatibility mode
};

enum UITheme {
  THEME_GENERAL = 0,
  THEME_VAULT = 1
};

// Keep THEME_GENERAL for the existing look.
// THEME_VAULT applies the Fallout-inspired orange palette and animation accents.
const UITheme ACTIVE_UI_THEME = THEME_GENERAL;

// Set true for a firmware build dedicated to the shared NPC story-round dial.
// False preserves the current local/random mission behavior.
const bool STORY_MODE_ON_BOOT = false;

// ===== ADMIN CONFIGURATION =====
#define ADMIN_BUTTON_HOLD_TIME 10000     // Hold button for 10 seconds to enter admin
#define ADMIN_EXIT_TIMEOUT 30000        // Auto-exit after 30s inactivity
#define ADMIN_MENU_ITEMS 11             // Device Info, Mission, Story Round, Relay, Dev Mode, Log, Scan Tag, NPC Tag, Safe Crack, Exit
#define ADMIN_MENU_SCROLL_DELAY 100     // Scroll delay in ms

// ===== OPTIONAL EXTERNAL MODULES =====
// Port A: Unit NFC/RFID2 (I2C). Port B: Unit RGB (SK6812 data on GPIO2).
// The RGB Unit and Unit AudioPlayer both use Port B, so only one may be enabled.
#ifndef ENABLE_EXTERNAL_NFC
#define ENABLE_EXTERNAL_NFC 1
#endif
#ifndef ENABLE_UNIT_RGB
#define ENABLE_UNIT_RGB 1
#endif
#if ENABLE_UNIT_AUDIO && ENABLE_UNIT_RGB
#error "Port B cannot run Unit AudioPlayer and Unit RGB simultaneously. Disable one module."
#endif

// ===== DEVICE CONFIGURATION =====
// Using values from secrets.h
// const String DEVICE_SERIAL and DEVICE_MAC defined in secrets.h

// ===== WIFI CONFIGURATION =====
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;
const int WIFI_CONNECT_TIMEOUT = 10000; // ms
const int WIFI_RETRY_DELAY = 500; // ms

// ===== SERIAL LOGGING =====
enum SerialLogLevel {
  SERIAL_LOG_BASIC = 0,
  SERIAL_LOG_VERBOSE = 1,
  SERIAL_LOG_FULL = 2
};

// Increase/decrease this to control runtime serial verbosity globally.
const int SERIAL_LOG_LEVEL = SERIAL_LOG_FULL;

// ===== API CONFIGURATION =====
// Mission Mode endpoints
const String API_ENDPOINT = String(MISSION_API_ENDPOINT);
const String REWARD_ENDPOINT = String(MISSION_REWARD_ENDPOINT);
const String WHATISIT_ENDPOINT = String(WHATISIT_ENDPOINT_URL);

// Completion payload mode toggle:
// false = legacy payload (no game_id)
// true  = new payload (includes game_id)
const bool COMPLETION_INCLUDE_GAME_ID = false;

const int HTTP_TIMEOUT = 5000; // ms
const int MAX_HTTP_RETRIES = 3;

// ===== MISSION TIMEOUT CONFIGURATION =====
const int MISSION_TIMEOUT_MIN = 15;  // Configurable: mission duration in minutes
const unsigned long MISSION_TIMEOUT_MS = (unsigned long)MISSION_TIMEOUT_MIN * 60UL * 1000UL;

// ===== GAME DIFFICULTY =====
// Selected by players on the difficulty screen (after badge confirmation).
// Controls location count:  EASY=1, MEDIUM=2, HARD=3, EXTREME=4
// Also controls Safe Cracker visual mode: EASY→SC_EASY, MEDIUM/HARD→SC_NORMAL, EXTREME→SC_EXTREME
enum GameDifficulty { GAME_EASY = 0, GAME_MEDIUM, GAME_HARD, GAME_EXTREME };

// ===== BADGE COOLDOWN CONFIGURATION =====
const int BADGE_COOLDOWN_MIN = 5;  // Minutes before a completed badge can be reused
const unsigned long BADGE_COOLDOWN_MS = (unsigned long)BADGE_COOLDOWN_MIN * 60UL * 1000UL;

// ===== WIFI CONFIG TAGS =====
// Special tags that trigger WiFi configuration mode
// Format: First tag triggers config mode, subsequent tags contain SSID and password
extern const char* WIFI_CONFIG_TAGS[];
extern const int NUM_WIFI_CONFIG_TAGS;

// ===== WIFI PRESETS (Lookup Table) =====
// Maps NFC tag UIDs to human-readable WiFi credentials
// To add a preset: Scan tag in admin mode to get UID, then add entry here
struct WiFiCredential {
    const char* tagUID;      // NFC tag UID (space-delimited hex)
    const char* ssid;        // Human-readable WiFi SSID
    const char* password;    // Human-readable WiFi password
};

extern const WiFiCredential WIFI_PRESETS[];
extern const int NUM_WIFI_PRESETS;

// ===== DISPLAY CONFIGURATION ====
const int ERROR_DISPLAY_TIME = 2000; // ms
const int SUCCESS_DISPLAY_TIME = 2000; // ms

// ===== DISPLAY TEXT (centralized UI copy) =====
namespace DisplayText {
  // Usage reference (function names in mission_M5.ino)
  // Splash: SPLASH_LINE1/2, SPLASH_PRODUCT, SPLASH_VERSION -> displaySplashScreen()
  // Errors: BADGE_USED -> WAIT_FOR_BADGE branch; SAME_SESSION/USE_DIFFERENT_MISSION -> WAIT_FOR_MISSION_CARD reuse guard
  // Errors: INVALID_MISSION_CARD -> WAIT_FOR_MISSION_CARD invalid tag; VISIT_LOCATIONS_FIRST -> completion tag guard
  // Networking errors: WIFI_FAILED -> sendCombinedCompletionRequest()
  // State restore/reset: RESUMING_MISSION -> badge-restore flow; BADGE_RESET -> resetBadgeForReuse(); FULL_RESET/MEMORY_CLEARED/RESETTING -> fullReset()/reset()
  // Mission prompts: TAP_TO_COMPLETE/REWARD_LABEL -> update() completion prompt; TIMEOUT_TITLE/TIMEOUT_ACTION -> timeout hold screen; COMPLETION_TITLE/COMPLETION_SUBTITLE -> locked-difficulty prompt
  // Timer overlay: TIMER_TITLE/TIMER_HURRY -> update() timer overlay sprite
  // Log viewer: LOG_TITLE/LOG_FOOTER -> displayLogFile()
  // Success: MISSION_PASSED/RESPECT_PLUS -> drawMissionPassedScreen()
  // Networking status: SENDING_TO_SERVER -> sendCombinedCompletionRequest() send animation
  // Admin: ADMIN_EXIT -> displayAdminMode(); ADMIN_DEV_MODE_ON/OFF -> admin dev mode toggle
    // Splash screen
    constexpr char SPLASH_LINE1[] = "GURU";                 // Splash screen top line
    constexpr char SPLASH_LINE2[] = "GAMES";                // Splash screen bottom line
    constexpr char SPLASH_PRODUCT[] = "GURU GAMES";     // Splash product name
    constexpr char SPLASH_VERSION[] = "MISSION WIDGET";                 // Splash version label

    // WiFi configuration
    constexpr char WIFI_CONFIG_MODE[] = "WiFi Config Mode";     // WiFi config mode title
    constexpr char WIFI_SCAN_SSID[] = "Scan WiFi Preset Tag";   // Prompt for WiFi preset tag (lookup table)
    constexpr char WIFI_SAVED[] = "WiFi Saved";                 // Success message

    // Errors and warnings
    constexpr char BADGE_USED[] = "Badge Used";                  // Badge already used error
    constexpr char SAME_SESSION[] = "Same Session";              // Consecutive session block
    constexpr char USE_DIFFERENT_MISSION[] = "Use New Mission"; // Mission reuse warning
    constexpr char INVALID_MISSION_CARD[] = "Invalid Mission Card";   // Invalid mission card
    constexpr char VISIT_LOCATIONS_FIRST[] = "Visit 1+ Locations";    // Require progress warning
    constexpr char WIFI_FAILED[] = "WiFi Failed";                    // WiFi failure

    // Session state transitions
    constexpr char RESUMING_MISSION[] = "Resuming Mission";       // Restore mission after reset
    constexpr char BADGE_RESET[] = "Badge Reset";                 // Badge reset info
    constexpr char FULL_RESET[] = "FULL RESET";                   // Full reset notice
    constexpr char MEMORY_CLEARED[] = "Memory Cleared";           // After full reset
    constexpr char RESETTING[] = "Resetting...";                  // Generic reset progress

    // Mission flow prompts
    constexpr char TAP_TO_COMPLETE[] = "Tap to Complete";     // Completion prompt header
    constexpr char REWARD_LABEL[] = "Reward:";                // Reward label
    constexpr char TIMEOUT_TITLE[] = "TIME OUT";              // Timeout title
    constexpr char TIMEOUT_ACTION[] = "RETURN TO BASE";        // Timeout action line
    constexpr char COMPLETION_TITLE[] = "SCAN";               // Completion title when locked
    constexpr char COMPLETION_SUBTITLE[] = "MISSION COMPLETION"; // Completion subtitle when locked
    constexpr char RETURN_TO_BASE[] = "RETURN TO BASE";        // Post-timeout field instruction
    constexpr char NO_REWARD[] = "NO REWARD";                  // Timeout without gained difficulty
    constexpr char CLAIM_REWARD[] = "CLAIM";                   // Timeout with gained difficulty

    // Timer overlay
    constexpr char TIMER_TITLE[] = "TIME LEFT";               // Timer overlay header
    constexpr char TIMER_HURRY[] = "HURRY!";                  // Timer critical warning

    // Log viewer
    constexpr char LOG_TITLE[] = "=== MISSION LOG ===";       // Log viewer title
    constexpr char LOG_FOOTER[] = "(Press button to close)";  // Log viewer footer

    // Success / reward visuals
    constexpr char MISSION_PASSED[] = "MISSION PASSED!";      // Success screen title
    constexpr char RESPECT_PLUS[] = "RESPECT +";              // Success screen subtitle

    // Networking
    constexpr char SENDING_TO_SERVER[] = "Sending to Server!"; // Send animation

    // Admin mode
    constexpr char ADMIN_EXIT[] = "(Hold 3s to go back)";       // Admin navigation instruction
    constexpr char ADMIN_DEV_MODE_ON[] = "DEV MODE: ON";        // Dev mode enabled
    constexpr char ADMIN_DEV_MODE_OFF[] = "DEV MODE: OFF";      // Dev mode disabled

    // Helpers
    inline String rewardLine(const String& difficulty) {
        return String(REWARD_LABEL) + " " + difficulty;
    }
}

// ===== COLORS (RGB565 format for TFT display) =====
const uint16_t COLOR_ERROR = 0xF800;      // Red
const uint16_t COLOR_SUCCESS = 0x07E0;    // Green
const uint16_t COLOR_WARNING = 0xFD20;    // Orange
const uint16_t COLOR_INFO = 0x8410;       // Gray
const uint16_t COLOR_PROCESSING = 0x001F; // Blue
const uint16_t COLOR_NONE = 0x000C;       // Dark Blue
const uint16_t COLOR_EXTREME = 0x8010;    // Purple

// ===== RESOURCE COLORS (RGB565 format for TFT display) =====
// Use these for text highlighting - convert RGB888 to RGB565
const uint16_t RESOURCE_COLOR_WEAPONS = 0xF460;    // Red (R:31, G:0, B:0)
const uint16_t RESOURCE_COLOR_SECURITY = 0x07FF;   // Yellow (R:31, G:63, B:0)
const uint16_t RESOURCE_COLOR_VEHICLE = 0xFFE0;  // White
const uint16_t RESOURCE_COLOR_MONEY = 0x07EF;      // Green (R:0, G:63, B:0)

// ===== RFID TAG CONFIGURATION =====
// Full reset tags (clears all data including completed badges)
const String FULL_RESET_TAGS[] = {
    " 3F 09 E5 64", " EF 23 18 64", " 04 20 83 D8 5F 61 80", " 0F A3 1A 64", " 9B CE 40 55",
};
const int NUM_FULL_RESET_TAGS = sizeof(FULL_RESET_TAGS) / sizeof(FULL_RESET_TAGS[0]);

// Regular reset tag
const String RESET_TAG[] = {
  " EF 23 18 99",
  " 1D A4 DD 02 81 00 00",
  " 1D E3 B9 03 81 00 00",
  " 04 D6 46 2A 0A 12 90",
  " 04 B3 45 2A 0A 12 90",
  " 04 F6 64 14 4E 61 80",
  " 04 C7 BD BB 2E 61 80",
  " 04 A5 24 15 4E 61 81",
  " 04 43 5B 14 4E 61 80",
  " 04 73 04 14 4E 61 80",
  " 04 29 10 14 4E 61 80"
};
const int NUM_RESET_TAGS = sizeof(RESET_TAG) / sizeof(RESET_TAG[0]);

// Badge reset tags (allows re-scanning of badge)
const String BADGE_RESET_TAGS[] = {
    " 04 49 53 CA 02 11 90", " 04 46 45 2A 0A 12 90"  // Replace with your actual badge reset tags
};
const int NUM_BADGE_RESET_TAGS = sizeof(BADGE_RESET_TAGS) / sizeof(BADGE_RESET_TAGS[0]);

// Scrub mission tags (marks badges on cooldown, clears session, no server call)
const String SCRUB_MISSION_TAGS[] = {
    " AF 74 D3 64"
};
const int NUM_SCRUB_MISSION_TAGS = sizeof(SCRUB_MISSION_TAGS) / sizeof(SCRUB_MISSION_TAGS[0]);

// Admin badge tags (enters admin mode for tag inspection)
const String ADMIN_BADGE_TAGS[] = {
  " 23 85 9F FE",
  " BF 1D 2A 40",
  " 4F 51 D2 64",
  " 43 D6 AC FE",
  " CD C0 AA 82",
  " F3 5D 25 03",
  " 13 F0 2F 04",
  " D1 AC 67 A9",
  " D3 57 1F FF",
  " 1F 69 D9 64",
  " 63 10 2F FF",
  " CB F5 15 55",
  " 04 12 07 D7 5F 61 80",
  " 6E 64 59 5B"
  // Add more admin badge UIDs here, one per line.
  // Example: " AA BB CC DD",
};
const int NUM_ADMIN_BADGE_TAGS = sizeof(ADMIN_BADGE_TAGS) / sizeof(ADMIN_BADGE_TAGS[0]);

// Mission reset tags (allows re-scanning of mission card)
// Mission reset tags removed — not used in current flow

// (No cooldown tags - feature removed)

// ===== RESOURCE CONFIGURATION =====
// Mission resource types (each location can have these)
enum ResourceType {
    RESOURCE_WEAPONS = 0,
    RESOURCE_SECURITY,
    RESOURCE_VEHICLE,
    RESOURCE_MONEY,
    RESOURCE_COUNT
};

// Status icon types for loud-mode UI feedback
enum StatusIconType {
  ICON_SUCCESS,
  ICON_ERROR,
  ICON_WARNING,
  ICON_INFO
};

// ===== DEVELOPMENT MODE =====
extern bool devMode;  // When true, all missions use GURU_HOME only

// ===== LOCATION CONFIGURATION =====

// ========================================
// EASY EDIT: Location Names (25 total)
// ========================================
// Just change the names here - keep the same order!
// Indexes 0-23 are production locations (alphabetical)
// Index 24 is GURU_HOME (dev location)
// NOTE: Actual definition is in POIStoryDualSendNoSoundv4.ino
extern const char* LOCATION_NAMES[];

// ========================================
// Location Structure (RFID Tags)
// ========================================
struct LocationInfo {
    const char* name;
    const String weaponTags[3];      // 3 RFID tags for WEAPONS resource
    const String securityTags[3];    // 3 RFID tags for SECURITY resource
    const String vehicleTags[3];     // 3 RFID tags for VEHICLE resource
    const String moneyTags[3];       // 3 RFID tags for MONEY resource
};

// Old game-specific tables are archived separately and disabled in remote mode.
#include "LegacyGameTags.h"
const int REQUIRED_LOCATIONS = 4;

// Validate location tags - for debugging
inline bool validateTags() {
    // Validate location resource tags (12 per location: 3 tags each for WEAPONS, SECURITY, VEHICLE, MONEY)
    for (int i = 0; i < TOTAL_LOCATIONS; i++) {
        const LocationInfo& loc = POI_LOCATIONS[i];
        
        // Check all 4 resource types, each with 3 tag slots
        const String* tagArrays[4] = { loc.weaponTags, loc.securityTags, loc.vehicleTags, loc.moneyTags };
        const char* resourceNames[4] = { "WEAPONS", "SECURITY", "VEHICLE", "MONEY" };
        
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 3; k++) {
                const String& tag = tagArrays[j][k];
                if (tag.isEmpty()) continue; // Skip empty tag slots
                
                if (tag[0] != ' ') {
                    Serial.println("ERROR: Invalid " + String(resourceNames[j]) + " tag " + String(k+1) + " format at location " + String(i) + " (" + String(loc.name) + ") - must start with space");
                    return false;
                }
                if (tag.length() < 6 || tag.length() > 25) {
                    Serial.println("ERROR: Invalid " + String(resourceNames[j]) + " tag " + String(k+1) + " length at location " + String(i) + " (" + String(loc.name) + ")");
                    return false;
                }
            }
        }
    }
    return true;
}
