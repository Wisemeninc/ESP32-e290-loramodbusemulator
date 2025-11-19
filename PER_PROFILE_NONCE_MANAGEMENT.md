# Per-Profile Nonce Management

## Overview
The ESP32 LoRaWAN emulator now implements **per-profile nonce storage**, ensuring each device profile maintains its own DevNonce sequence independently. This is critical for proper LoRaWAN operation when using auto-rotation with multiple profiles.

## What is a DevNonce?

### Definition
**DevNonce (Device Nonce)** is a counter that increments with each OTAA join request:
- 16-bit counter (0-65535)
- Must be unique for each join attempt
- Used by network server to prevent replay attacks
- Must never repeat for the same device

### Importance
- Network servers **reject join requests with duplicate DevNonces**
- Essential for security (prevents replay attacks)
- Must persist across device reboots
- Each device identity needs its own DevNonce sequence

## Problem Without Per-Profile Nonces

### Before This Fix ❌
```
Boot → Profile 0 active → Join with DevNonce=1 → Save nonce
Rotate → Profile 2 active → Join with DevNonce=1 → Save nonce (overwrites Profile 0!)
Rotate → Profile 0 active → Join with DevNonce=1 → REJECTED! (duplicate nonce)
```

**Issues:**
- All profiles shared same nonce storage
- DevNonce reset to 1 each time profile switched
- Network server rejects joins with duplicate nonces
- Auto-rotation breaks after first cycle

## Solution: Per-Profile Nonce Storage ✅

### Implementation
Each profile now has dedicated NVS keys:
```cpp
// Storage keys per profile
"nonces_0"      → Profile 0 nonce buffer
"has_nonces_0"  → Profile 0 nonce flag
"nonces_1"      → Profile 1 nonce buffer
"has_nonces_1"  → Profile 1 nonce flag
"nonces_2"      → Profile 2 nonce buffer
"has_nonces_2"  → Profile 2 nonce flag
"nonces_3"      → Profile 3 nonce buffer
"has_nonces_3"  → Profile 3 nonce flag
```

### How It Works

#### 1. Save Nonces (After Join/Uplink)
```cpp
void saveSession() {
    // Get current nonce buffer from RadioLib
    uint8_t* noncesPtr = node->getBufferNonces();
    
    // Save to profile-specific key
    sprintf(noncesKey, "nonces_%d", active_profile_index);
    preferences.putBytes(noncesKey, noncesBuffer, noncesSize);
    
    // Mark this profile has saved nonces
    sprintf(hasNoncesKey, "has_nonces_%d", active_profile_index);
    preferences.putBool(hasNoncesKey, true);
}
```

#### 2. Restore Nonces (Before Join)
```cpp
bool restoreNonces() {
    // Check if this profile has saved nonces
    sprintf(hasNoncesKey, "has_nonces_%d", active_profile_index);
    bool hasNonces = preferences.getBool(hasNoncesKey, false);
    
    if (hasNonces) {
        // Load profile-specific nonces
        sprintf(noncesKey, "nonces_%d", active_profile_index);
        preferences.getBytes(noncesKey, noncesBuffer, noncesSize);
        
        // Restore to RadioLib
        node->setBufferNonces(noncesBuffer);
    }
}
```

#### 3. Profile Rotation Flow
```
Profile 0 Active:
  → Restore nonces_0 (DevNonce=5)
  → Join with DevNonce=6
  → Send uplink
  → Save nonces_0 (DevNonce=6)
  → Rotate to Profile 2

Profile 2 Active:
  → Restore nonces_2 (DevNonce=3)
  → Join with DevNonce=4
  → Send uplink
  → Save nonces_2 (DevNonce=4)
  → Rotate to Profile 3

Profile 3 Active:
  → Restore nonces_3 (DevNonce=7)
  → Join with DevNonce=8
  → Send uplink
  → Save nonces_3 (DevNonce=8)
  → Rotate to Profile 0

Profile 0 Active Again:
  → Restore nonces_0 (DevNonce=6) ✓
  → Join with DevNonce=7 ✓ (continues sequence!)
  → No duplicate nonce rejection!
```

## DevNonce Sequence Example

### First Boot (Fresh Device)
```
Profile 0: DevNonce=1 → Join → Save nonces_0
Profile 1: DevNonce=1 → Join → Save nonces_1
Profile 2: DevNonce=1 → Join → Save nonces_2
Profile 3: DevNonce=1 → Join → Save nonces_3
```
✓ Each profile starts its own sequence

### After 5 Minutes (First Rotation Cycle)
```
Profile 0: Load nonces_0 → DevNonce=2 → Join → Save
Profile 2: Load nonces_2 → DevNonce=2 → Join → Save
Profile 3: Load nonces_3 → DevNonce=2 → Join → Save
```
✓ Each profile increments independently

### After Reboot
```
Profile 0: Load nonces_0 → DevNonce=3 → Continue from last
Profile 2: Load nonces_2 → DevNonce=3 → Continue from last
Profile 3: Load nonces_3 → DevNonce=3 → Continue from last
```
✓ Sequences persist across power cycles

## NVS Storage Details

### Namespace
All nonce data stored in `"lorawan"` namespace (separate from `"lorawan_prof"` used for profiles)

### Key Format
- **Nonces Key**: `nonces_X` where X = profile index (0-3)
- **Flag Key**: `has_nonces_X` where X = profile index (0-3)

### Data Size
- **RADIOLIB_LORAWAN_NONCES_BUF_SIZE**: Typically 56 bytes
- Contains DevNonce, JoinNonce, and other replay protection data

### Example NVS Layout
```
Namespace: "lorawan"
├── nonces_0: [56 bytes] → Profile 0 nonce buffer
├── has_nonces_0: true
├── nonces_1: [56 bytes] → Profile 1 nonce buffer
├── has_nonces_1: true
├── nonces_2: [56 bytes] → Profile 2 nonce buffer
├── has_nonces_2: true
├── nonces_3: [56 bytes] → Profile 3 nonce buffer
└── has_nonces_3: true

Namespace: "lorawan_prof"
├── prof0: [Profile 0 credentials]
├── prof1: [Profile 1 credentials]
├── prof2: [Profile 2 credentials]
├── prof3: [Profile 3 credentials]
├── active_idx: 0
└── auto_rotate: true
```

## Serial Monitor Output

### Save Nonces
```
>>> saveLoRaWANNonces() called
>>> Saved nonces (56 bytes) for Profile 0 - DevNonce will persist across reboots
```

### Restore Nonces
```
Checking for saved nonces (required for DevNonce tracking)...
>>> has_nonces flag for Profile 0: true
>>> Found saved nonces for Profile 0 - restoring...
>>> Loaded nonces (56 bytes) for Profile 0
>>> setBufferNonces() returned: 0
>>> Nonces restored for Profile 0 - DevNonce will continue from last value
```

### Profile Rotation
```
Auto-rotation: Switching to next profile
Joined with profile 2
Checking for saved nonces (required for DevNonce tracking)...
>>> has_nonces flag for Profile 2: true
>>> Found saved nonces for Profile 2 - restoring...
>>> Loaded nonces (56 bytes) for Profile 2
>>> setBufferNonces() returned: 0
>>> Nonces restored for Profile 2 - DevNonce will continue from last value
```

## Benefits

### 1. Proper Multi-Device Emulation
Each profile behaves as completely independent device:
- Unique DevEUI ✓
- Unique AppKey ✓
- Unique DevNonce sequence ✓

### 2. No Join Rejections
Network server accepts all join requests:
- No duplicate DevNonce errors
- Proper replay attack protection maintained
- Each device identity tracked separately

### 3. Persistent State
DevNonce survives:
- Device reboots
- Profile switches
- Power cycles
- Firmware updates (if NVS preserved)

### 4. Network Server Compliance
Meets LoRaWAN specification requirements:
- DevNonce must never repeat (per device)
- Proper OTAA security
- Replay attack prevention

## Troubleshooting

### Join Rejected: "Invalid DevNonce"
**Symptom:** Network server rejects join with "duplicate nonce" or "invalid nonce"

**Possible Causes:**
1. Network server not recognizing per-device nonce sequences
2. NVS corrupted or cleared
3. DevNonce rolled over (>65535 joins)

**Solutions:**
1. Reset device frame counters in network server
2. Erase NVS: `pio run -t erase`
3. Re-register devices in network server

### Nonces Not Persisting
**Symptom:** DevNonce resets to 1 after reboot

**Check:**
1. Serial Monitor shows: `>>> Saved nonces (56 bytes) for Profile X`
2. NVS namespace "lorawan" exists
3. Keys `has_nonces_X` set to true

**Debug:**
```cpp
// Check NVS contents
preferences.begin("lorawan", true);
bool has0 = preferences.getBool("has_nonces_0", false);
bool has1 = preferences.getBool("has_nonces_1", false);
Serial.printf("Profile 0 nonces: %s\n", has0 ? "saved" : "missing");
Serial.printf("Profile 1 nonces: %s\n", has1 ? "saved" : "missing");
preferences.end();
```

### Profile Shares Nonces
**Symptom:** Different profiles have same DevNonce

**Cause:** Old firmware without per-profile nonce support

**Solution:**
1. Update to v1.49+
2. Erase NVS to clear old global nonces
3. Reboot and let each profile establish its sequence

## Migration from Old Version

### Before v1.49 (Global Nonces)
```
NVS Layout:
├── nonces: [56 bytes] ← All profiles shared this
└── has_nonces: true
```

### After v1.49 (Per-Profile Nonces)
```
NVS Layout:
├── nonces_0: [56 bytes] ← Profile 0 only
├── has_nonces_0: true
├── nonces_1: [56 bytes] ← Profile 1 only
├── has_nonces_1: true
└── ...
```

### Migration Steps
1. Update firmware to v1.49+
2. Old `nonces` key is ignored (not deleted)
3. Each profile starts fresh DevNonce sequence on first join
4. Network server may show "new device" joins initially
5. After first cycle, all profiles have saved nonces

## Technical Details

### RadioLib Integration
Uses RadioLib's nonce buffer management:
```cpp
// Get buffer from RadioLib
uint8_t* noncesPtr = node->getBufferNonces();

// Save to NVS
memcpy(noncesBuffer, noncesPtr, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
preferences.putBytes(noncesKey, noncesBuffer, noncesSize);

// Restore from NVS
preferences.getBytes(noncesKey, noncesBuffer, noncesSize);
node->setBufferNonces(noncesBuffer);
```

### Buffer Contents
Nonce buffer includes:
- **DevNonce** (16-bit)
- **JoinNonce** (24-bit)
- **Network ID**
- **Other replay protection data**

### Save Triggers
Nonces saved after:
1. Successful OTAA join
2. Each uplink transmission
3. Ensures latest DevNonce always persisted

### Restore Timing
Nonces restored:
1. Before OTAA join attempt
2. During profile activation
3. After profile rotation

## Code References

### Implementation Files
- `src/lorawan_handler.cpp` - saveSession(), restoreNonces()
- `src/lorawan_handler.h` - Method declarations

### Key Functions
```cpp
void LoRaWANHandler::saveSession()      // Save current profile's nonces
bool LoRaWANHandler::restoreNonces()    // Restore current profile's nonces
bool LoRaWANHandler::join()             // Calls restoreNonces() before join
void LoRaWANHandler::sendUplink()       // Calls saveSession() after uplink
```

## Version History
- **v1.49** - Implemented per-profile nonce storage
- **<v1.49** - Single global nonce storage (all profiles shared)

## Related Documentation
- `AUTO_ROTATION_FEATURE.md` - Multi-profile rotation system
- `TIMING_STRATEGY.md` - Staggered uplink timing
- `CHIRPSTACK_CONFIG_GUIDE.md` - Network server setup
