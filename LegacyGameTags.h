#pragma once
// Archived local game tables. Remote mode compiles only empty placeholders.
// Use Creator for current-game POIs, NPC UUIDs and completion cards.
#if !REMOTE_GAME_CONFIGURATION
// Mission complete tags
// EF96E264 belongs to DNR Medical Center in Heist 2, not mission completion.
const String COMPLETE_TAGS[] = {
    " 6F 14 E4 64",
    " 04 F5 E8 2A 0A 12 90",
    " 04 2E D6 2A 0A 12 91",
    " 1D 51 BD 02 81 00 00",
    " 1D 5A D4 02 81 00 00",
    " 51 54 9F 3A",
    " 91 FE AB 3A",
    " 11 2E AB 3A",
    " 41 19 AE 3A",
    " 61 AD A5 3A",
    " 21 02 AE 3A"
};
const int NUM_COMPLETE_TAGS = sizeof(COMPLETE_TAGS) / sizeof(COMPLETE_TAGS[0]);

const LocationInfo POI_LOCATIONS[] = {
    { LOCATION_NAMES[0],  // Allens Corner Store // Belle Isle Outpost // Ashline Underpass
      {" DF 6D E0 64", "", ""},
      {" 52 D3 BB 29", "", ""},
      {" 1D BE 2E 03 81 00 00", "", ""},
      {" 1D 6C A1 02 81 00 00", "", ""} },
    { LOCATION_NAMES[1],  // Grizzly Gas // Brush Park // Belle Spire Broadcast Mast
      {" 43 76 E7 02", "", ""},
      {" 91 E3 81 9F", "", ""},
      {" A1 F6 80 9F", "", ""},
      {" 73 63 82 9F", "", ""} },
    { LOCATION_NAMES[2],  // Loki-Eleven // Campus Martius // Blacktop Bazaar
      {" E3 CB 1D FF", "", ""},
      {" 25 B6 80 9F", "", ""},
      {" D7 0C 82 9F", "", ""},
      {" 75 20 82 9F", "", ""} },
    { LOCATION_NAMES[3],  // PredStop // Capital Hill // Charter Street Bunker
      {" 5F 61 E6 64", "", ""},
      {" 5C E7 19 DA", "", ""},
      {" E2 FC 80 9F", "", ""},
      {" 8C 23 83 9F", "", ""} },
    { LOCATION_NAMES[4],  // Wicks Country Fuels // Dequindre Greenway // Civic Archive Tube Station
      {" 53 0F BE FE", "", ""},
      {" 04 69 93 2A 0A 12 90", "", ""},
      {" 1D D1 D0 02 81 00 00", "", ""},
      {" 1D 2C 95 02 81 00 00", "", ""} },
    { LOCATION_NAMES[5],  // Overbay Autos // Eastern Market // Corktown Rail Depot
      {" 04 AD 70 D5 5F 61 80", "", ""},
      {" 92 47 81 9F", "", ""},
      {" 94 9A 81 9F", "", ""},
      {" E2 C7 80 9F", "", ""} },
    { LOCATION_NAMES[6],  // Bank of Angry Gremlin // Fenkell Frontlines // Eden Intake Pump House
      {" 04 39 48 D8 5F 61 80", "", ""},
      {" 83 C1 C0 FE", "", ""},
      {" 1D 0F DA 02 81 00 00", "", ""},
      {" 1D 0C ED 02 81 00 00", "", ""} },
    { LOCATION_NAMES[7],  // Rookie Country Club // Fisher Beacon // Eden Overflow Culvert
      {" 6F B6 CA 64", "", ""},
      {" 43 01 FC 02", "", ""},
      {" 1D 04 30 03 81 00 00", "", ""},
      {" 1D 57 FC 02 81 00 00", "", ""} },
    { LOCATION_NAMES[8],  // The Nella Lounge // Greektown Alleyways // Engine Chapel
      {" 04 00 36 D5 5F 61 80", "", ""},
      {" 1D 36 1E 03 81 00 00", "", ""},
      {" 1D 3C A8 02 81 00 00", "", ""},
      {" 1D 76 A1 03 81 00 00", "", ""} },
    { LOCATION_NAMES[9],  // Capital Hill Private Security // Guardian Plaza // Gate 313 Relay
      {" 1D E4 3D 03 81 00 00", "", ""},
      {" E3 38 1B FF", "", ""},
      {" 1D 20 44 03 81 00 00", "", ""},
      {" 1D 33 A0 03 81 00 00", "", ""} },
    { LOCATION_NAMES[10],  // Stayback Mine // Iron Depot // Greenwater Quarantine Yard
      {" B3 58 95 0F", "", ""},
      {" C3 8B E6 02", "", ""},
      {" 1D 56 E9 02 81 00 00", "", ""},
      {" 1D CB E7 02 81 00 00", "", ""} },
    { LOCATION_NAMES[11],  // Saint Lawrence Cathedral // Masonic Stronghold // Mercy Row Clinic
      {" 9F 32 23 64", "", ""},
      {" 04 18 49 2A 0A 12 91", "", ""},
      {" 1D 50 DA 02 81 00 00", "", ""},
      {" 1D BE BB 03 81 00 00", "", ""} },
    { LOCATION_NAMES[12],  // Armageddon Power Station // Midtown Commons // Motorworks Yard
      {" 43 54 60 13", "", ""},
      {" EE 2A 81 9F", "", ""},
      {" F7 AE 81 9F", "", ""},
      {" E6 02 83 9F", "", ""} },
    { LOCATION_NAMES[13],  // DNR Hospital // Motown Archives // New Detroit Exchange
      {" E3 3A F3 02", "", ""},
      {" 04 57 75 D5 5F 61 80", "", ""},
      {" 1D 5B 94 02 81 00 00", "", ""},
      {" 1D 32 2F 03 81 00 00", "", ""} },
    { LOCATION_NAMES[14],  // A.O.A.T. Army Surplus // New Center // Old Municipal Power House
      {" 13 C1 E8 02", "", ""},
      {" F4 E1 80 9F", "", ""},
      {" 6A 18 82 9F", "", ""},
      {" CA AF 82 9F", "", ""} },
    { LOCATION_NAMES[15],  // BUT DID YOU STRIKE // Old Fort Bastion // Red Brick Social Hall
      {" 3F 87 DA 64", "", ""},
      {" 1F 63 1B 64", "", ""},
      {" 1D B3 F5 02 81 00 00", "", ""},
      {" 1D 6E 97 02 81 00 00", "", ""} },
    { LOCATION_NAMES[16],  // Princess Substation 7 // Packard Ruins // Riverglass Canal Lock
      {" 04 45 C0 2A 0A 12 90", "", ""},
      {" 73 F5 E8 02", "", ""},
      {" 1D C6 81 03 81 00 00", "", ""},
      {" 1D AC F0 02 81 00 00", "", ""} },
    { LOCATION_NAMES[17],  // Turf War Arena // Renaissance Hub // Service Tunnel Nine
      {" 6F 3B D5 64", "", ""},
      {" 3F D6 CD 64", "", ""},
      {" 1D 7F 3A 03 81 00 00", "", ""},
      {" 1D A3 99 03 81 00 00", "", ""} },
    { LOCATION_NAMES[18],  // Gatormaille Crossing Prawn Shop // Riverfront Plaza // Sherwood Civic Tower
      {" EF 6A F4 64", "", ""},
      {" 1B 90 81 9F", "", ""},
      {" 44 56 81 9F", "", ""},
      {" 2B EB 82 9F", "", ""} },
    { LOCATION_NAMES[19],  // SHEro's Taproom // Spirits Watch // Union Tunnel Mouth
      {" 23 D1 74 05", "", ""},
      {" AF 7A 1F 64", "", ""},
      {" 1D 5C ED 02 81 00 00", "", ""},
      {" 1D 65 A7 03 81 00 00", "", ""} },
    { LOCATION_NAMES[20],  // Hall of High Scores // TechTown // Vault 313 Surface Gate
      {" 33 23 66 E8", "", ""},
      {" 05 18 81 9F", "", ""},
      {" 66 F2 80 9F", "", ""},
      {" 79 CB 81 9F", "", ""} },
    { LOCATION_NAMES[21],  // Meadman Pulse Tower // Tower of Pages // Water Board Annex
      {" 04 51 58 2A 0A 12 91", "", ""},
      {" A3 DF EA 02", "", ""},
      {" 1D D3 D0 02 81 00 00", "", ""},
      {" 1D 60 C4 02 81 00 00", "", ""} },
    { LOCATION_NAMES[22],  // GGs Inc. // Waters Edge // Rusted Overpass Checkpoint
      {" 04 5D 5A D6 5F 61 80", "", ""},
      {" F3 64 B4 FE", "", ""},
      {" 1D 6B DA 02 81 00 00", "", ""},
      {" 1D 69 AC 02 81 00 00", "", ""} },
    { LOCATION_NAMES[23],  // Laceys Lounge // West Village // Market Checkpoint
      {" FF BC D4 64", "", ""},
      {" 55 64 82 9F", "", ""},
      {" F7 29 81 9F", "", ""},
      {" B3 4C 81 9F", "", ""} },
    // === DEV LOCATION ===
    { LOCATION_NAMES[24],  // GURU_HOME
      {" FF 38 DC 64", " 04 F2 D2 7D CC 2A 81", ""},
      {" 53 FC 04 03", " 04 74 D3 7F CC 2A 81", ""},
      {" F3 21 E9 02", " 04 8C F2 7F CC 2A 81", ""},
      {" EF B2 D7 64", " 86 10 EA AD", ""} }
};

const int TOTAL_LOCATIONS = sizeof(POI_LOCATIONS) / sizeof(POI_LOCATIONS[0]);

// Mission card tags - ALL cards trigger HEIST missions (category selection removed)
// Add your physical mission card RFID tags here
const String MISSION_CARD_TAGS[] = {
    " EE 57 67 5B", " 3E BB 01 0C",
  " 4F 74 CA 64", " B3 DC 00 03", " 1F 59 D2 64",
  " CE D5 0E 0C", " 6F 8F 17 64",
    // Add more mission card tags below as needed
    " 04 81 14 2A 0A 12 90", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", ""
};
const int NUM_MISSION_CARD_TAGS = sizeof(MISSION_CARD_TAGS) / sizeof(MISSION_CARD_TAGS[0]);

// ===== FREE ROAM MISSION CARD TAGS =====
// Free Roam missions: 4 random POIs are selected.  Players visit any subset and scan
// any resource they choose at each POI.  Each resource TYPE can only be collected once.
// Difficulty is determined by how many POIs are visited before scanning the completion card.
const String FREE_ROAM_MISSION_TAGS[] = {
    " B3 DC 00 03",  // FREE ROAM card 1
    // Add more Free Roam cards here as needed
};
const int NUM_FREE_ROAM_MISSION_TAGS = sizeof(FREE_ROAM_MISSION_TAGS) / sizeof(FREE_ROAM_MISSION_TAGS[0]);

#else
const String COMPLETE_TAGS[] = { "" };
const int NUM_COMPLETE_TAGS = 0;
const LocationInfo POI_LOCATIONS[1] = {};
const int TOTAL_LOCATIONS = 0;
const String MISSION_CARD_TAGS[] = { "" };
const int NUM_MISSION_CARD_TAGS = 0;
const String FREE_ROAM_MISSION_TAGS[] = { "" };
const int NUM_FREE_ROAM_MISSION_TAGS = 0;
#endif
