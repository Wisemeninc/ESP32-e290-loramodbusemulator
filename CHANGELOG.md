# Changelog

All notable changes to the ESP32-e290-loramodbusemulator project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.53] - 2025-11-20

### Added
- **Display Enhancements**: Improved E-Ink display information density
  - AP Mode: Now shows AP SSID suffix (e.g., "AP 2 clients A1B2")
  - LoRaWAN: Shows all enabled profile DevEUIs in compact format (e.g., "..7CEB/..9AB2")
  - Allows quick identification of device and active profiles at a glance

### Changed
- Display Manager: Added `ap_ssid` parameter to `update()` function
- LoRaWAN Handler: Added `getEnabledDevEUIs()` method to retrieve enabled profiles

## [1.52] - 2025-11-20

### Changed
- **Code Refactoring**: Major architectural overhaul to improve maintainability and stability
  - Split monolithic `main.cpp` into separate components:
    - `SF6Emulator`: Encapsulates sensor simulation logic
    - `WebServerManager`: Manages HTTPS server and request handlers
    - `web_pages.h`: Stores HTML templates in PROGMEM
  - Reduced `main.cpp` size from >3000 lines to ~200 lines
  - Improved global state management

### Fixed
- **SSL Crash**: Fixed `StoreProhibited` panic in `HTTPS_Server_Generic` library
  - Moved web server to a dedicated FreeRTOS task pinned to Core 0
  - Increased web server task stack size to 8KB to handle SSL operations safely
- **LoRaWAN Auto-Rotation**: Restored and fixed multi-profile auto-rotation logic
  - Correctly cycles through enabled profiles
  - Staggers uplinks by 1 minute when multiple profiles are active
  - Ensures nonces are saved to NVS to prevent replay attacks
- **Web Interface**: Fixed issues with WiFi scanning and LoRaWAN configuration pages
- **Registers Page**: Now displays all Holding (0-11) and Input (0-8) registers

## [1.51] - 2025-11-19

### Added
- **Per-Profile Payload Format Selection**: Each profile can now use a different payload format
  - Four payload formats available: Adeunis Modbus SF6, Cayenne LPP, Raw Modbus Registers, Custom
  - Dropdown selector in web UI for easy format configuration
  - Payload format displayed in profile overview table
  - Format persisted in NVS with profile data
  - Automatic migration for existing profiles (default to Adeunis)
  - Serial output shows selected format and decoded values
  - Perfect for simulating heterogeneous device fleets

- **Payload Format Implementations**:
  - **Adeunis Modbus SF6** (10 bytes): Original format with SF6-specific encoding
  - **Cayenne LPP** (12 bytes): Standard IoT format compatible with most platforms
  - **Raw Modbus Registers** (10 bytes): Direct register dump for debugging
  - **Custom Format** (13 bytes): IEEE 754 floats for high-precision applications

- **Web UI Enhancements**:
  - "Payload Format" column in profile overview table
  - Dropdown menu in profile edit forms
  - Format description and usage hints
  - Confirmation message showing selected format after save
  - Responsive table design with horizontal scroll

- **Documentation**:
  - `PAYLOAD_FORMAT_SELECTION.md`: Technical reference with decoder examples
  - `WEB_UI_PAYLOAD_SELECTION.md`: User guide for web interface usage
  - JavaScript decoder examples for all formats
  - ChirpStack integration guide for multi-format scenarios

### Changed
- Firmware version bumped to 151 (v1.51)
- Profile update handler now processes `payload_type` parameter
- Profile overview table now has 6 columns (added Payload Format)
- `LoRaProfile` struct size increased by 1 byte (PayloadType field)

### Fixed
- Raw Modbus payload builder corrected to use available InputRegisters fields only
- Payload size calculation now uses returned value instead of sizeof()

### Technical
- New enum: `PayloadType` with 4 values (0-3)
- New array: `PAYLOAD_TYPE_NAMES[]` for format display names
- Four payload builder functions in `lorawan_handler.cpp`
- `sendUplink()` refactored with switch statement for format selection
- NVS validation ensures payload_type is within valid range
- Backwards compatible with v1.50 profile data

## [1.50] - 2025-11-19

### Added
- **Startup Uplink Sequence**: Device now sends initial uplinks from all enabled profiles on boot
  - Each enabled profile joins network and sends startup announcement
  - 10-second delay between profiles to allow RX windows to complete
  - Ensures all devices are registered and visible in network server immediately
  - Returns to initial profile for normal auto-rotation operation
  - Serial output shows progress for each profile
  - Total startup time: ~17 seconds per enabled profile
  - Provides immediate device visibility in ChirpStack/network server

### Fixed
- **Per-Profile Nonce Storage**: Critical fix for multi-profile DevNonce management
  - Each profile now maintains its own DevNonce sequence independently
  - Prevents "duplicate nonce" join rejections when rotating profiles
  - NVS keys: `nonces_0` through `nonces_3` (one per profile)
  - DevNonce persists correctly across profile switches and reboots
  - Essential for proper auto-rotation operation with multiple devices
  - Fixes issue where all profiles shared single nonce storage

### Technical
- Per-profile nonce keys: `nonces_X` and `has_nonces_X` in `lorawan` namespace
- Startup sequence iterates through enabled profiles with automatic skip of disabled ones
- State restoration returns to initial profile after startup sequence
- Documentation added: `STARTUP_UPLINK_SEQUENCE.md`, `PER_PROFILE_NONCE_MANAGEMENT.md`

## [1.49] - 2025-11-19

### Added
- **Auto-Rotation Feature**: Automatically cycle through multiple enabled LoRaWAN profiles
  - Rotates to next enabled profile before each uplink transmission
  - Automatically re-joins network with new profile credentials
  - Persistent auto-rotation setting stored in NVS
  - Only rotates through enabled profiles (skips disabled ones)
  - Requires at least 2 enabled profiles to function
- **Auto-Rotation Web UI**: Enhanced profiles page with rotation controls
  - Status section showing current rotation state (ENABLED/DISABLED)
  - Enabled profile count display
  - Toggle checkbox to enable/disable auto-rotation
  - Checkbox disabled when insufficient profiles (< 2 enabled)
  - Warning message when auto-rotation unavailable
- **Auto-Rotation API**: New endpoint for toggling rotation setting
  - `GET /lorawan/auto-rotate?enabled=1` - Enable rotation
  - `GET /lorawan/auto-rotate?enabled=0` - Disable rotation
  - Returns success/error status
- **Display Feedback**: E-ink display updates during rotation
  - "Profile rotated" - When switching profiles
  - "Joined with new profile" - After successful join
  - "Join failed after rotation" - If OTAA join fails
- **Backend Methods**: New LoRaWANHandler methods for rotation management
  - `setAutoRotation(bool)` - Enable/disable rotation with NVS persistence
  - `getAutoRotation()` - Get current rotation state
  - `rotateToNextProfile()` - Switch to next enabled profile
  - `getNextEnabledProfile()` - Find next enabled profile in sequence
  - `getEnabledProfileCount()` - Count total enabled profiles
- **Startup Profile Summary**: Terminal now displays all profile configurations on boot
  - Shows all 4 profiles with status (ENABLED/DISABLED)
  - Displays DevEUI, JoinEUI for each profile (full hex values)
  - **Full AppKey and NwkKey display** (32 hex characters) for ChirpStack configuration
  - Keys shown without "0x" prefix - copy directly into ChirpStack
  - Indicates currently active profile
  - Shows auto-rotation status and enabled profile count
  - Displays rotation schedule when auto-rotation enabled

### Changed
- **Staggered Uplink Timing**: When auto-rotation enabled with multiple profiles
  - Each profile sends every 5 minutes (maintains 5-minute interval per device)
  - Profiles are staggered 1 minute apart (e.g., Profile 0 at 0min, Profile 1 at 1min, etc.)
  - Per-profile uplink tracking ensures accurate timing for each device
  - Minimum 1-minute gap enforced between any two uplinks
- Main loop now performs profile rotation after sending uplink
- Profile rotation triggers automatic network re-join with new credentials
- Uplink is skipped if join fails after profile rotation

### Technical
- NVS key `auto_rotate` added to `lorawan_prof` namespace
- Rotation algorithm wraps around from last profile to first
- Profiles page route remains `/lorawan/profiles` with enhanced UI
- Documentation added: `AUTO_ROTATION_FEATURE.md`, `TIMING_STRATEGY.md`, `CHIRPSTACK_CONFIG_GUIDE.md`, `TERMINAL_OUTPUT_SAMPLE.md`

## [1.48] - 2025-11-19

### Added
- **Multi-Profile LoRaWAN Device Emulation**: Support for 4 independent LoRaWAN device profiles
  - Each profile has unique DevEUI, JoinEUI, AppKey, and NwkKey credentials
  - Profiles can be individually enabled/disabled
  - One profile can be active at a time
  - Profile switching triggers device restart to join with new credentials
- **Profile Management Web Interface**: New `/lorawan/profiles` page
  - Overview table showing all 4 profiles with status indicators
  - ACTIVE badge for currently active profile
  - Enable/Disable toggle buttons for each profile
  - Activate button to switch active profile (requires enabled status)
  - In-place credential editor for each profile (name, DevEUI, JoinEUI, AppKey, NwkKey)
- **Profile Persistence**: All profiles stored in NVS (Non-Volatile Storage)
  - New `lorawan_prof` namespace for profile data
  - Profiles survive reboots and power cycles
  - Active profile index persisted across restarts
- **Automatic Profile Initialization**: 
  - On first boot, Profile 0 is auto-generated and enabled
  - Profiles 1-3 are auto-generated but disabled by default
  - Each profile gets unique MAC-based DevEUI for device identification
- **Profile Safety Features**:
  - Cannot disable currently active profile
  - Only enabled profiles can be activated
  - Profile credentials validated before saving
- **Enhanced LoRaWAN Status Page**: Updated `/lorawan` page
  - Shows currently active profile information
  - "Manage All Profiles" button linking to profiles page
  - Active profile indicator in credentials section
- **Navigation Updates**: Added "Profiles" link to all page navigation bars

### Changed
- **LoRaWANHandler Class**: Extended with profile management methods
  - `loadProfiles()` - Load all profiles from NVS
  - `saveProfiles()` - Save all profiles to NVS
  - `initializeDefaultProfiles()` - Generate default profiles
  - `generateProfile(index, name)` - Generate unique credentials
  - `setActiveProfile(index)` - Switch active profile
  - `getActiveProfileIndex()` - Get current active profile
  - `getProfile(index)` - Access specific profile
  - `updateProfile(index, profile)` - Update profile credentials
  - `toggleProfileEnabled(index)` - Enable/disable profile
  - `printProfile(index)` - Debug output for profile
- **Startup Sequence**: Modified to load and use active profile
  - Profiles loaded before LoRaWAN initialization
  - Active profile credentials used for OTAA join
  - Profile info printed to serial console
- **Configuration Structure**: Added `LoRaProfile` struct to `config.h`
  - 67 bytes per profile (name, credentials, enabled flag)
  - ~270 bytes total NVS usage for all profiles

### Technical Details
- **NVS Storage**:
  - Namespace: `lorawan_prof`
  - Keys: `has_profiles`, `active_idx`, `prof0` through `prof3`
- **Profile Structure**:
  ```c
  struct LoRaProfile {
      char name[33];           // Profile name (32 chars + null)
      uint64_t devEUI;         // Device EUI (MSB)
      uint64_t joinEUI;        // Join EUI / AppEUI (MSB)
      uint8_t appKey[16];      // 128-bit AppKey (MSB)
      uint8_t nwkKey[16];      // 128-bit NwkKey (MSB)
      bool enabled;            // Profile enabled/disabled
  };
  ```
- **Web Endpoints**:
  - `GET /lorawan/profiles` - Profile management page
  - `POST /lorawan/profile/update` - Update profile credentials
  - `GET /lorawan/profile/toggle` - Enable/disable profile
  - `GET /lorawan/profile/activate` - Activate profile (with restart)

### Documentation
- Added `LORA_PROFILES_IMPLEMENTATION.md` with complete feature documentation
  - Usage guide
  - Technical implementation details
  - Example use cases
  - Troubleshooting guide

## [1.47] - 2025-11-18

### Added
- **Factory Reset Feature**: Added comprehensive factory reset in security settings
  - Erases all NVS settings (Modbus, Auth, WiFi, SF6, LoRaWAN)
  - Restores device to factory defaults
  - Automatic device reboot after reset
  - Confirmation dialog to prevent accidental resets
- **Factory Reset Web UI**: New section in `/security` page
  - Red danger zone styling
  - List of all settings that will be reset
  - Double confirmation required (browser confirm + button click)

### Changed
- Updated security page layout with factory reset section
- Added warning messages about factory reset consequences

## Previous Versions

### [1.46] - 2025-11
- Debug logging controls for HTTPS and authentication
- Enhanced security settings page

### [1.45] - 2025-11
- Authentication enable/disable via web interface
- Emergency authentication re-enable endpoint

### [1.44] - 2025-11
- HTTP to HTTPS redirect server
- mDNS support (stationsdata.local)

### [1.43] - 2025-11
- WiFi client mode support
- WiFi configuration persistence
- Network scanning functionality

### [1.42] - 2025-11
- Modbus TCP support
- TCP/RTU register synchronization
- Web interface for Modbus TCP enable/disable

### [1.41] - 2025-11
- LoRaWAN OTAA join support
- DevNonce persistence
- Uplink/downlink tracking

### [1.40] - 2025-11
- Initial LoRaWAN integration
- SX1262 radio support
- EU868 region configuration

### [1.30] - 2025-11
- SF6 sensor emulation
- Manual SF6 value control via web interface
- NVS persistence for SF6 values

### [1.20] - 2025-11
- E-Ink display support (Vision Master E290)
- Display rotation configuration
- Status display on E-Ink

### [1.10] - 2025-11
- HTTPS web server
- Basic authentication
- Web-based configuration

### [1.00] - 2025-11
- Initial release
- Modbus RTU slave implementation
- ESP32-S3 support
- WiFi AP mode
- Basic web interface

---

## Version Numbering

Version numbers follow the format: `MMmm`
- `MM` = Major version (00-99)
- `mm` = Minor version (00-99)

Examples:
- `100` = v1.00
- `111` = v1.11
- `148` = v1.48
- `203` = v2.03

Display format: `v(FIRMWARE_VERSION/100).(FIRMWARE_VERSION%100)`
