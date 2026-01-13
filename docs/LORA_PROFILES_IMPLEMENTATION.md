# LoRaWAN Multi-Profile Implementation

## Overview
The ESP32-e290-loramodbusemulator now supports 4 independent LoRaWAN device profiles, allowing the device to emulate multiple LoRa devices with different credentials. Each profile can be enabled/disabled and configured via the web interface.

## Features Implemented

### 1. Profile Data Structure
- **Location**: `src/config.h`
- **Structure**: `LoRaProfile`
  - `name[33]` - Profile name (32 chars + null terminator)
  - `devEUI` - Device EUI (64-bit, MSB format)
  - `joinEUI` - Join EUI / AppEUI (64-bit, MSB format)
  - `appKey[16]` - 128-bit Application Key
  - `nwkKey[16]` - 128-bit Network Key
  - `enabled` - Boolean flag for enable/disable state
- **Constant**: `MAX_LORA_PROFILES = 4`

### 2. LoRaWAN Handler Enhancements
- **Location**: `src/lorawan_handler.h` and `src/lorawan_handler.cpp`
- **New Methods**:
  - `loadProfiles()` - Load all 4 profiles from NVS
  - `saveProfiles()` - Save all 4 profiles to NVS
  - `initializeDefaultProfiles()` - Generate default profiles on first boot
  - `generateProfile(index, name)` - Generate unique credentials for a profile
  - `setActiveProfile(index)` - Switch to a different profile (requires enabled)
  - `getActiveProfileIndex()` - Get current active profile index (0-3)
  - `getProfile(index)` - Get pointer to specific profile
  - `updateProfile(index, profile)` - Update profile credentials
  - `toggleProfileEnabled(index)` - Enable/disable a profile
  - `printProfile(index)` - Print profile details to serial console

### 3. NVS Storage
- **Namespace**: `lorawan_prof` (separate from old `lorawan` namespace)
- **Keys**:
  - `has_profiles` - Boolean flag indicating profiles exist
  - `active_idx` - Active profile index (0-3)
  - `prof0` to `prof3` - Binary storage of each profile structure
- **Migration**: Old credentials are preserved in `lorawan` namespace for backward compatibility

### 4. Web Interface
- **New Pages**:
  - `/lorawan/profiles` - Main profile management page
    - Overview table showing all 4 profiles
    - Status indicators (ENABLED/DISABLED, ACTIVE badge)
    - Enable/Disable buttons
    - Activate button (switches active profile with device restart)
    - Edit forms for each profile
  - `/lorawan/profile/update` - POST handler for updating profile credentials
  - `/lorawan/profile/toggle` - GET handler for enabling/disabling profiles
  - `/lorawan/profile/activate` - GET handler for activating a profile (with restart)

- **Updated Pages**:
  - `/lorawan` - Now shows active profile information
  - All navigation bars include "Profiles" link

### 5. Initialization Behavior

#### First Boot
1. No profiles exist in NVS
2. `initializeDefaultProfiles()` is called
3. Profile 0 is generated with unique credentials and enabled
4. Profiles 1-3 are generated with unique credentials and disabled
5. Active profile index set to 0
6. All profiles saved to NVS

#### Subsequent Boots
1. Profiles loaded from NVS
2. Active profile index loaded from NVS
3. Active profile credentials copied to legacy fields (for backward compatibility)
4. Device uses active profile for LoRaWAN join

### 6. Profile Management Rules
- **Enable/Disable**:
  - Any profile can be enabled or disabled
  - Exception: Cannot disable the currently active profile
  
- **Activation**:
  - Only enabled profiles can be activated
  - Activating a profile triggers device restart
  - After restart, device joins network with new profile credentials
  
- **Editing**:
  - All profiles can be edited at any time
  - Changes to active profile take effect immediately (for display)
  - LoRaWAN join uses credentials at boot time

### 7. Credential Generation
Each profile gets unique credentials:
- **JoinEUI**: Random 64-bit value
- **DevEUI**: Based on ESP32 MAC address + profile index + random bytes for uniqueness
- **AppKey**: Random 128-bit value
- **NwkKey**: Copy of AppKey (for LoRaWAN 1.0.x compatibility)

This ensures each profile can be registered as a separate device on the LoRaWAN network.

## Usage Guide

### Accessing Profile Management
1. Connect to device WiFi AP or access via client IP
2. Navigate to: `https://stationsdata.local/lorawan/profiles`
3. Default credentials: `admin` / `admin`

### Managing Profiles

#### Enable a Profile
1. Click "Enable" button next to disabled profile
2. Profile becomes available for activation

#### Disable a Profile
1. Click "Disable" button next to enabled profile
2. Cannot disable the active profile

#### Switch Active Profile
1. Ensure target profile is enabled
2. Click "Activate" button
3. Confirm the restart prompt
4. Device restarts and joins with new profile

#### Edit Profile Credentials
1. Scroll to profile edit form
2. Modify:
   - Profile Name (friendly identifier)
   - JoinEUI (16 hex characters)
   - DevEUI (16 hex characters)  
   - AppKey (32 hex characters)
   - NwkKey (32 hex characters)
3. Click "Save Profile X"
4. Credentials saved to NVS immediately
5. If editing active profile, restart required for LoRaWAN join

### Registering Profiles on Network Server
Each profile can be registered as a separate device:

1. Access `/lorawan/profiles` page
2. Copy credentials for desired profile
3. Register on network server (TTN, ChirpStack, etc.) as new device:
   - Device EUI: from DevEUI field
   - Application EUI: from JoinEUI field
   - App Key: from AppKey field
   - Network Key: from NwkKey field (if required)
4. Enable the profile on device
5. Activate the profile
6. Device will restart and join as that device

## Technical Details

### Profile Storage Format
```c
struct LoRaProfile {
    char name[33];           // Profile name
    uint64_t devEUI;         // Device EUI (MSB)
    uint64_t joinEUI;        // Join EUI (MSB)
    uint8_t appKey[16];      // 128-bit AppKey (MSB)
    uint8_t nwkKey[16];      // 128-bit NwkKey (MSB)
    bool enabled;            // Enabled flag
};
```
Total size per profile: ~67 bytes
Total NVS usage: ~270 bytes for all profiles + metadata

### Profile Index in NVS
Active profile index stored as single byte (`active_idx`) in `lorawan_prof` namespace.

### Backward Compatibility
Legacy credential fields (joinEUI, devEUI, appKey, nwkKey) in LoRaWANHandler are still populated from the active profile, ensuring existing code continues to work.

## Example Use Cases

### 1. Multi-Tenant Deployment
Deploy one device that can represent different tenant devices by switching profiles.

### 2. Testing Multiple Device Types
Test LoRaWAN network behavior with different device credentials without reflashing.

### 3. Failover Configuration
Pre-configure backup profiles with different network server credentials for redundancy.

### 4. Device Emulation Lab
Use one physical device to emulate up to 4 different LoRa devices for development/testing.

## Serial Console Output

### Profile Loading
```
>>> Loading LoRaWAN profiles from NVS...
    Loaded Profile 0: Profile 0 (enabled)
    Loaded Profile 1: Profile 1 (disabled)
    Loaded Profile 2: Profile 2 (disabled)
    Loaded Profile 3: Profile 3 (disabled)
>>> Active profile index: 0
```

### Profile Switching
```
>>> Setting active profile to 2: Test Device
>>> Active profile updated
```

### Profile Generation
```
>>> Generating new LoRaWAN credentials...
    Generated JoinEUI: 0x0123456789ABCDEF
    Generated DevEUI: 0xABCDEF0123456789 (MAC-based)
    Generated AppKey: 0123456789ABCDEF0123456789ABCDEF
```

## Files Modified
1. `src/config.h` - Added LoRaProfile structure and MAX_LORA_PROFILES
2. `src/lorawan_handler.h` - Added profile management method declarations
3. `src/lorawan_handler.cpp` - Implemented profile management logic
4. `src/main.cpp` - Added web UI handlers and updated setup()

## Future Enhancements (Not Implemented)
- Import/export profiles via JSON
- Profile templates for common network servers
- Automatic profile rotation on schedule
- Profile-specific uplink intervals
- Multi-region profile support

## Testing Checklist
- [x] Profile initialization on first boot
- [x] Profile loading on subsequent boots
- [x] Enable/disable profile toggle
- [x] Active profile cannot be disabled
- [x] Profile activation triggers restart
- [x] Profile credentials persist across reboots
- [x] Web UI displays correct profile status
- [x] Profile editing saves to NVS
- [x] Active profile indicator shows correctly
- [x] Multiple profiles can be registered on network server

## Troubleshooting

### Profile not activating
- Check profile is enabled first
- Verify profile credentials are valid hexadecimal
- Check serial console for error messages

### Device not joining after profile switch
- Verify profile is registered on network server
- Check network server credentials match exactly
- Ensure gateway is in range and online
- Review serial console for join failure details

### Profiles reset to defaults
- Check NVS is not corrupted (factory reset if needed)
- Verify power supply is stable during profile saves
- Review serial console during boot for NVS errors

## Version History
- **v1.68** - Current version
  - Fixed PAYLOAD_TYPE_NAMES array order bug
  - Documentation updates
  - OTA firmware update feature added
- **v1.47** - Initial multi-profile implementation
  - 4 profiles supported
  - Web UI for profile management
  - NVS persistence
  - Profile enable/disable/activate
