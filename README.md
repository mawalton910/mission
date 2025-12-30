# Mission M5 - Location-Based Story Mission System
**Current Version**: POIStory v25.8.2

A location-based mission tracking system built on the M5Stack Dial (ESP32-S3) for immersive story-driven gameplay in live-action role-playing events.

## 🎮 What It Does

The Mission M5 device creates dynamic, location-based story missions for LARP players:

### For Players
- **Story-Driven Missions**: Receive narrative missions with specific objectives
- **Location Tracking**: Visit real-world locations to complete mission objectives
- **Mission Categories**: Choose from Heist, Police, Hacking, Job, Vehicle, Drug, Medical, or Community missions
- **Dynamic Narratives**: Each mission generates unique storylines with category-specific themes
- **Progress Tracking**: 30-minute timer with real-time countdown and location visit tracking
- **Respect Rewards**: Earn difficulty-based rewards based on locations completed

### For Game Masters
- **Device Monitoring**: View device information, firmware version, and WiFi status
- **Mode Switching**: Toggle between Mission, Buy Station, and Relay modes
- **Mission Logs**: Review all mission events with timestamps
- **Remote Updates**: Push firmware updates to all devices simultaneously
- **Integrated Systems**: Buy Station for loot and Relay for badge management

## 🔧 Hardware

- **M5Stack Dial** (ESP32-S3) with built-in RFID reader
- 240x240 round touchscreen display
- Rotary encoder with button
- WiFi connectivity for server integration

## 📦 Mission System Features

### Mission Categories
Eight distinct mission types, each with unique narrative themes:
- **Heist**: High-stakes robbery operations
- **Police**: Law enforcement missions
- **Hacking**: Digital infiltration tasks
- **Job**: Professional assignments
- **Vehicle**: Transportation-based missions
- **Drug**: Pharmaceutical operations
- **Medical**: Healthcare-related tasks
- **Community**: Social service missions

### Mission Structure
Each mission includes:
- **Story Generation**: Dynamic narrative based on selected category
- **4 Random Locations**: Visit real-world locations to progress
- **30-Minute Timer**: Complete mission before time expires
- **Difficulty Levels**: Easy, Medium, Hard, or Extreme based on completion
- **Respect Rewards**: Dynamic rewards calculated from difficulty
- **Badge History**: Track completed missions per player

## 🎯 How Players Use It

1. **Scan Your Badge** → System identifies you and checks mission history
2. **Select Mission Type** → Scan a mission card (prevents consecutive duplicate missions)
3. **Read Your Story** → Device generates a unique narrative with 4 location objectives
4. **Navigate the Story** → Use rotary dial to scroll through mission pages
5. **Visit Locations** → Travel to real-world sites and scan location RFID tags
6. **Track Progress** → Press button to view countdown timer during mission
7. **Complete Mission** → Scan completion card when objectives are done
8. **Earn Respect** → Server awards difficulty-based rewards automatically

## 🕐 Mission Timer System

- **30-Minute Countdown**: Starts when mission begins
- **On-Demand Display**: Press button to see MM:SS remaining
- **Warning System**: Red "HURRY!" alert when under 60 seconds
- **Timeout Handling**:
  - No progress: Mission can be cleared with badge-reset or completion
  - Some progress: Locks current difficulty level, allows submission

## 🏆 Difficulty & Rewards

Difficulty is earned based on locations visited:
- **Easy**: 1 location visited
- **Medium**: 2 locations visited  
- **Hard**: 3 locations visited
- **Extreme**: All 4 locations visited

Respect rewards scale with difficulty and are automatically calculated and distributed by the server.

## 🛠️ Administrative Features

Game masters can access admin mode by scanning an authorized badge:
- View device serial, MAC address, firmware version, and WiFi IP
- Switch between Mission Widget, Buy Station, and Relay modes
- Review mission logs with timestamps (press button in badge-wait state)
- Update firmware remotely
- Monitor device status in real-time

## 📡 Over-the-Air Updates

Devices support remote firmware updates for seamless version management:
- Scan a designated trigger card to start update
- Device automatically downloads and installs new firmware
- Progress displayed with percentage and download statistics
- Automatic reboot after successful installation
- All devices can be updated simultaneously

## 🎮 Game Integration

The system integrates with backend servers to:
- Track player badge data and mission history
- Record mission completions with difficulty levels
- Calculate and distribute respect rewards
- Prevent consecutive duplicate missions per player
- Synchronize game state across all devices

## 🔐 Security & Anti-Abuse

- Physical RFID badge required for all missions
- Consecutive use prevention (different player must complete between same badge+mission pairs)
- Badge history tracking
- Session timeout protection
- Location verification
- Server-side validation of all submissions

## 🙏 Acknowledgments

Built with M5Stack Dial hardware, ESP32-S3 platform, and the Arduino framework.

---

**Current Version**: POIStory v25.8.2
**Last Updated**: December 30, 2025
