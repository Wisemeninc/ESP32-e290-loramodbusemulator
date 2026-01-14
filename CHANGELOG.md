# Changelog

All notable changes to the ESP32-e290-loramodbusemulator project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.86] - 2026-01-14

### Fixed
- **Enhanced OTA Watchdog Protection**: Added comprehensive watchdog resets throughout entire OTA process
  - Watchdog resets after each GitHub API call
  - Watchdog resets before/after JSON parsing operations
  - Watchdog resets during asset search and URL preparation
  - Watchdog resets before download HTTP connection and GET request
  - Added esp_task_wdt_delete(NULL) before task deletion to properly unregister from watchdog
  - Prevents timeout during long SSL handshakes in download phase

## [1.85] - 2026-01-14

### Changed
- Version bump to test OTA update functionality with watchdog fixes

## [1.84] - 2026-01-14

### Fixed
- **OTA Watchdog Timeout**: Added frequent watchdog resets during GitHub API requests
  - OTA task now resets watchdog before/after SSL handshake operations
  - Added watchdog resets around HTTP GET calls that trigger SSL connections
  - Added timeouts for SSL client (15s) and HTTP client (15s)
  - Prevents watchdog timeout during long SSL handshake operations
  - Fixes crash: "Task watchdog got triggered" error in OTATask

## [1.83] - 2026-01-14

### Changed
- Version bump for release

## [1.68] - 2025-01-13

### Added
- OTA firmware updates from private GitHub repository
- GitHub Personal Access Token configuration in config.h
- Real-time update progress tracking with web UI
- Semantic version comparison for update checking
- GitHub API integration with secure token authentication

### Changed
- Simplified OTA web interface (removed token configuration form)
- GitHub token now configured exclusively via config.h hardcoded value

### Fixed
- PAYLOAD_TYPE_NAMES array order to match PayloadType enum indices
- Version comment mismatch in config.h (now correctly shows v1.68)

## [1.67] - 2025-01-13

### Added
- Complete OTA manager implementation
- GitHub API client with SSL certificate handling
- Web routes for OTA check, start, status, and configuration
- Progress monitoring with JSON status API

### Fixed
- String concatenation bugs in URL construction (6+ locations)
- Progress calculation overflow showing incorrect percentages
- DNS resolution and SSL certificate verification issues

## [1.66] - 2025-01-13

### Added
- Initial OTA update feature framework
- GitHub repository integration for firmware distribution
- Web UI foundations for OTA management

## [1.65] - 2025-11-23

### Improved
- **Display Uptime Format**: Enhanced with multi-unit detail for better readability
  - Under 1 hour: Shows minutes and seconds (e.g., "15m 30s")
  - 1 hour to 1 day: Shows hours and minutes (e.g., "5h 23m")
  - 1 day or more: Shows days, hours, and minutes (e.g., "3d 5h 23m")
  - Heap display moved further right to accommodate additional detail
  - More informative at a glance without taking up extra space

## [1.64] - 2025-11-23

### Added
- **Dark Mode Support**: Configurable dark mode for both web interface and E-Ink display
  - `WEB_DARK_MODE` in config.h: true = dark theme, false = light theme
  - `DISPLAY_DARK_MODE` in config.h: true = inverted display (white on black), false = normal (black on white)
  - Web interface: Dark theme with dark backgrounds (#1a1a1a, #2d2d2d) and light text (#e0e0e0)
  - Display: Inverted e-ink rendering in dark mode (white text on black background)
  - Consistent dark/light theming across entire user interface
  - Compile-time configuration via config.h (no runtime toggle)

### Changed
- **Web Interface**: Completely rewritten HTML header generation for dynamic theming
  - Removed static HTML_HEADER constant from web_pages.h
  - Added dynamic `printHTMLHeader()` method that generates theme-specific CSS
  - Separate color schemes for light and dark modes
  - All 7 web pages now use dynamic header generation

### Improved
- **Display Rendering**: Better contrast and readability in both light and dark modes
  - Background and foreground colors now use BG_COLOR/FG_COLOR macros
  - Consistent theming across startup screen, status display, and WiFi credentials

## [1.63] - 2025-11-23

### Changed
- **Modbus Uptime Register**: Expanded from 16-bit to 32-bit to prevent overflow
  - Previous: 16-bit uptime rolled over after 18.2 hours (65,535 seconds)
  - Current: 32-bit uptime supports ~136 years (4,294,967,295 seconds)
  - Uptime now spans registers 2-3 (low word/high word)
  - All subsequent holding registers shifted by one (now at addresses 4-12)
  - Total holding registers increased from 12 to 13

### Improved
- **Display Uptime Format**: Intelligent formatting based on duration
  - Less than 1 hour: shows seconds (e.g., "3456s")
  - 1 hour to 1 day: shows hours (e.g., "18h")
  - 1 day or more: shows days (e.g., "45d")
  - Optimizes display space while maintaining readability

### Updated
- **Documentation**: Updated README.md with corrected register addresses and 32-bit uptime examples
- **Web Interface**: Updated register display to show 32-bit uptime value
- **Python Examples**: Updated to read 13 registers and correctly combine 32-bit uptime

## [1.62] - 2025-11-22

### Added
- **LoRaWAN DevNonce Reset**: Added functionality to reset DevNonce counters to resolve nonce misalignment
  - New `resetNonces()` method clears saved DevNonce counters for all profiles
  - Web interface endpoint `/lorawan/reset-nonces` (requires authentication)
  - Reset button added to LoRaWAN configuration page with troubleshooting guidance
  - Resolves persistent join failures caused by DevNonce counter mismatch with network server
  - Useful when device nonce gets out of sync after development/testing cycles

### Fixed
- **CRITICAL: LoRaWAN DevNonce Synchronization**: Save DevNonce after EVERY join attempt, not just successful ones
  - **Root cause**: Device only saved DevNonce after successful joins. If join attempts failed and device rebooted, DevNonce counter would reset to old value. Meanwhile, network server may have seen those failed attempts and incremented its expected DevNonce, causing persistent join failures.
  - **Symptom**: Error -1116 (RADIOLIB_ERR_NO_JOIN_ACCEPT) during device operation, even with correct credentials and good signal
  - **Solution**: Now saves DevNonce to NVS after every join attempt (successful or failed) to keep counter synchronized with network server
  - **Impact**: Eliminates "DevNonce too low" rejections caused by counter misalignment during normal operation
  - **Recovery**: Use new DevNonce reset function if device is already out of sync

## [1.61] - 2025-11-22

### Added
- **LoRaWAN Nonce Verification**: Added write verification for DevNonce persistence
  - Verifies nonce data is written correctly to NVS after each save
  - Automatic retry up to 3 times if verification fails
  - Read-back comparison to ensure data integrity
  - Critical warning if all retry attempts fail
  - Ensures DevNonce reliably persists across reboots

## [1.60] - 2025-11-21

### Security
- **CRITICAL FIX**: Added authentication checks to 5 unprotected web endpoints
  - `/sf6/update` - SF6 sensor value modification now requires authentication
  - `/sf6/reset` - SF6 reset now requires authentication
  - `/security/enable` - Authentication enable/disable now requires authentication
  - `/factory-reset` - Factory reset now requires authentication
  - `/reboot` - Device reboot now requires authentication
  - Previously these endpoints were accessible without credentials (security vulnerability)
  - All 20 functional web endpoints now properly protected with HTTP Basic Auth

## [1.59] - 2025-11-20

### Changed
- **Display Initialization**: Display now clears immediately on boot before any other initialization
  - Display initializes first in setup() before LoRaWAN and other components
  - Clears screen to blank white to remove any previous content or ghosting
  - Startup screen shown after all components are initialized
  - Ensures clean display state on every boot

## [1.58] - 2025-11-20

### Changed
- **WiFi AP SSID**: Changed from "ESP32-Modbus-Config-XXXX" to "stationsdata-XXXX"
  - More concise and professional naming convention
  - XXXX = last 4 hex digits of device MAC address
  - Updated code in wifi_manager.cpp
  - Updated display to show full SSID instead of truncated version
  - Updated all documentation references

## [1.57] - 2025-11-20

### Fixed
- **NVS Preferences**: Eliminated `nvs_open failed: NOT_FOUND` errors on first boot
  - Changed all `preferences.begin()` calls from read-only to read-write mode
  - NVS namespaces now automatically created if they don't exist
  - Fixed: auth_manager, sf6_emulator, wifi_manager, lorawan_handler, main loop
  - Prevents harmless but confusing error messages in serial console

## [1.56] - 2025-11-20

### Added
- **Modbus Input Registers**: Populated registers 4-8 with device information
  - Register 4: Modbus Slave ID
  - Register 5: Serial Number (High) - First 2 bytes of MAC address
  - Register 6: Serial Number (Low) - Last 2 bytes of MAC address
  - Register 7: Firmware Version (156 for v1.56)
  - Register 8: Crystal Frequency (4000 = 40.00 MHz)

## [1.55] - 2025-11-20

### Changed
- **E-Ink Display**: Improved startup screen with device info instead of test pattern
  - Now shows device name, firmware version, and initialization status
  - Removed unnecessary boxes and diagonal lines
- **Display Colors**: Changed from inverted (white-on-black) to normal (black-on-white)
  - More natural appearance and better readability
  - Lower power consumption with less black pixels

## [1.54] - 2025-11-20

### Changed
- **E-Ink Display**: Enabled partial refresh mode to eliminate flicker during updates
  - Display now uses fast partial refresh (no black/white inversion flicker)
  - Automatic full refresh every 10 updates to prevent ghosting
  - Much faster display updates with better user experience

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
