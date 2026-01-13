# Auto-Rotation Feature Documentation

## Overview
The auto-rotation feature allows the ESP32 device to automatically cycle through multiple enabled LoRaWAN profiles during uplink transmissions. Each uplink will use a different enabled profile, effectively emulating multiple LoRa devices in sequence.

## Features
- ✅ Automatic profile switching before each uplink transmission
- ✅ Only rotates through enabled profiles (disabled profiles are skipped)
- ✅ Persistent setting stored in NVS (survives reboots)
- ✅ Web UI toggle with real-time status display
- ✅ Automatic re-join with new profile credentials
- ✅ Requires at least 2 enabled profiles to function
- ✅ Display feedback showing current profile

## How It Works

### 1. Backend Logic (`lorawan_handler.cpp`)
The LoRaWANHandler class implements the following methods:

```cpp
void setAutoRotation(bool enabled);           // Enable/disable auto-rotation
bool getAutoRotation() const;                  // Get current rotation state
uint8_t getNextEnabledProfile() const;         // Find next enabled profile
bool rotateToNextProfile();                    // Switch to next profile
int getEnabledProfileCount() const;            // Count enabled profiles
```

**Rotation Algorithm:**
- Starts from current active profile index
- Searches forward (wrapping around) for next enabled profile
- Skips disabled profiles automatically
- Returns to first enabled profile after last one

### 2. Web Interface (`main.cpp`)
Access the feature at: `https://stationsdata.local/lorawan/profiles`

**UI Elements:**
- Auto-Rotation status section (green background)
- Current state: ENABLED/DISABLED
- Enabled profile count display
- Toggle checkbox (disabled if < 2 enabled profiles)
- Warning message when insufficient profiles

**JavaScript Handler:**
```javascript
function toggleAutoRotate(enabled) {
  fetch('/lorawan/auto-rotate?enabled=' + (enabled ? '1' : '0'))
    .then(r => r.text())
    .then(() => location.reload());
}
```

### 3. Main Loop Integration (`main.cpp`)
**Timing Strategy:**
- **Single Profile Mode:** Uplinks every 5 minutes
- **Multi-Profile Auto-Rotation Mode:** Each profile sends every 5 minutes, staggered 1 minute apart

**How it works:**
1. Tracks last uplink time per profile (not global)
2. Checks if current profile is due (5 minutes since its last uplink)
3. If current profile not due, checks if next profile is ready
4. Ensures minimum 1-minute gap between any uplinks (staggering)
5. Rotates after sending, re-joins with new credentials

**Example Timeline (3 enabled profiles):**
```
Time 0:00 - Profile 0 sends, rotates to Profile 1, joins
Time 1:00 - Profile 1 sends, rotates to Profile 2, joins  
Time 2:00 - Profile 2 sends, rotates to Profile 0, joins
Time 5:00 - Profile 0 sends again (5min since 0:00)
Time 6:00 - Profile 1 sends again (5min since 1:00)
Time 7:00 - Profile 2 sends again (5min since 2:00)
```

**Code Flow:**
```cpp
// Track per-profile uplink times
if (current_profile due for uplink) {
    sendUplink();
    rotateToNextProfile();
    join();
} else if (next_profile is ready && 1min has passed) {
    rotateToNextProfile();
    join();
    sendUplink();
}
```

## Usage Guide

### Enable Auto-Rotation
1. Navigate to **LoRaWAN Profiles** page
2. Enable at least **2 profiles** (use Enable buttons)
3. Check the **"Enable Auto-Rotation"** checkbox
4. Device will now cycle through enabled profiles automatically

### Example Scenario
**Setup:**
- Profile 0: Enabled (Device A)
- Profile 1: Disabled
- Profile 2: Enabled (Device B)
- Profile 3: Enabled (Device C)
- Auto-Rotation: Enabled

**Staggered Timing (3 enabled profiles):**
```
Time 00:00 - Profile 0 sends (Device A) → Rotate to Profile 2 → Join
Time 01:00 - Profile 2 sends (Device B) → Rotate to Profile 3 → Join
Time 02:00 - Profile 3 sends (Device C) → Rotate to Profile 0 → Join
Time 05:00 - Profile 0 sends (Device A) - 5min since its last uplink
Time 06:00 - Profile 2 sends (Device B) - 5min since its last uplink
Time 07:00 - Profile 3 sends (Device C) - 5min since its last uplink
Time 10:00 - Profile 0 sends (Device A) - Pattern continues...
```

**Key Points:**
- Each device (profile) transmits every 5 minutes
- Devices are staggered 1 minute apart
- Profile 1 is skipped (disabled)
- Total cycle time depends on number of enabled profiles
- 3 profiles = 3 uplinks per 5-minute window (at 0min, 1min, 2min)

### Disable Auto-Rotation
1. Navigate to **LoRaWAN Profiles** page
2. Uncheck the **"Enable Auto-Rotation"** checkbox
3. Device will use only the currently active profile

## Technical Details

### NVS Storage
**Namespace:** `lorawan_prof`
**Key:** `auto_rotate`
**Type:** `uint8_t` (0 = disabled, 1 = enabled)

The setting persists across reboots and is loaded during initialization.

### Profile State Management
Each profile has:
- `enabled` flag (can be used for uplinks)
- Active status (currently in use)

Auto-rotation only considers profiles where `enabled = true`.

### Network Re-joining
When rotating profiles:
1. Device leaves current network session
2. Loads new profile credentials (DevEUI, JoinEUI, AppKey, NwkKey)
3. Performs OTAA join procedure
4. Establishes new session with network server
5. Sends uplink with new device identity

**Note:** Re-joining takes ~5-10 seconds. If join fails, the uplink is skipped and rotation continues on next attempt.

## API Endpoints

### GET `/lorawan/auto-rotate`
Toggle auto-rotation setting.

**Parameters:**
- `enabled`: `1` (enable) or `0` (disable)

**Response:**
```
200 OK: "Auto-rotation enabled" or "Auto-rotation disabled"
400 Bad Request: "Error: Missing enabled parameter"
```

**Example:**
```
GET /lorawan/auto-rotate?enabled=1
```

## Display Feedback
The e-ink display shows:
- "Profile rotated" - When switching profiles
- "Joined with new profile" - After successful join
- "Join failed after rotation" - If OTAA join fails

## Troubleshooting

### Auto-Rotation Not Working
**Symptom:** Checkbox disabled or profiles not rotating

**Solutions:**
1. **Check enabled profiles:** At least 2 profiles must be enabled
   - Go to Profiles page
   - Enable additional profiles using "Enable" buttons

2. **Verify join success:** Device must join successfully with each profile
   - Check Serial Monitor for join messages
   - Verify credentials are correct in each profile

3. **Check uplink interval:** Rotation occurs every 5 minutes
   - Wait for scheduled uplink time
   - Check serial output for "Auto-rotation: Switched to next profile"

### Profile Doesn't Switch
**Symptom:** Same profile used for multiple uplinks

**Possible Causes:**
1. Only one profile is enabled (others are disabled)
2. Auto-rotation checkbox is unchecked
3. Join failure with next profile (device stays on current)

**Debug Steps:**
```
1. Serial Monitor: Look for "Auto-rotation: Switched to next profile"
2. Check enabled count in UI (should be >= 2)
3. Verify auto-rotation status shows "ENABLED"
```

### Join Failures After Rotation
**Symptom:** "Join failed after rotation" on display

**Solutions:**
1. Verify all enabled profiles have correct credentials
2. Check that DevEUI, JoinEUI, AppKey, NwkKey are valid
3. Ensure network server has all devices registered
4. Check LoRaWAN gateway coverage

## Implementation Files
- `src/lorawan_handler.h` - Auto-rotation method declarations
- `src/lorawan_handler.cpp` - Auto-rotation implementation
- `src/main.cpp` - Web UI, API handler, main loop integration
- `src/config.h` - LoRaProfile struct definition

## Version History
- **v1.48** - Initial auto-rotation implementation
  - Backend rotation logic
  - Web UI toggle and status
  - Main loop integration
  - NVS persistence

## Future Enhancements
Potential improvements:
- [ ] Configurable rotation interval (per profile)
- [ ] Round-robin vs. weighted rotation strategies
- [ ] Rotation statistics (uplinks per profile)
- [ ] Conditional rotation (e.g., on ACK failure)
- [ ] Profile health monitoring with auto-skip
