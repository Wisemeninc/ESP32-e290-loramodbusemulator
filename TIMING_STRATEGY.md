# LoRaWAN Uplink Timing Strategy

## Overview
The ESP32 LoRaWAN emulator uses intelligent timing logic to manage uplinks across multiple device profiles.

## Single Profile Mode
**When:** Auto-rotation is disabled OR only 1 profile is enabled

**Behavior:**
- Uplink transmitted every 5 minutes (300,000 ms)
- Simple timer: `if (now - last_lorawan_uplink >= 300000)`
- Uses currently active profile

**Example:**
```
Time 00:00 - Profile 0 sends
Time 05:00 - Profile 0 sends
Time 10:00 - Profile 0 sends
Time 15:00 - Profile 0 sends
```

## Multi-Profile Auto-Rotation Mode
**When:** Auto-rotation is enabled AND 2+ profiles are enabled

**Behavior:**
- Each profile sends every 5 minutes
- Profiles are staggered 1 minute apart
- Per-profile uplink tracking (not global)
- Minimum 1-minute gap between any uplinks

### Timing Algorithm

#### 1. Check Current Profile
```cpp
time_since_last = now - last_profile_uplinks[current_profile];
if (time_since_last >= 300000) {
    // Current profile is due (5 minutes elapsed)
    send_uplink();
    rotate_to_next_profile();
    join_network();
}
```

#### 2. Check Next Profile (Staggering)
```cpp
if (current_profile not due) {
    next_profile = get_next_enabled_profile();
    next_time_since_last = now - last_profile_uplinks[next_profile];
    
    if (next_time_since_last >= 300000 && 
        now - last_lorawan_uplink >= 60000) {
        // Next profile is due AND 1 minute has passed
        rotate_to_next_profile();
        join_network();
        send_uplink();
    }
}
```

### Data Structure
```cpp
static unsigned long last_profile_uplinks[MAX_LORA_PROFILES] = {0};
```
- Tracks last uplink timestamp for each profile (0-3)
- Independent timing per profile
- Survives across rotations

### Example Timeline: 3 Enabled Profiles

**Profiles:** 0, 2, 3 (Profile 1 disabled)

```
Time    Action                          Notes
-----   ------------------------------- --------------------------------
00:00   Profile 0 sends                 Initial uplink
00:00   → Rotate to Profile 2           After sending
00:00   → Join with Profile 2           Ready for next uplink

01:00   Profile 2 sends                 5min not elapsed, but 1min gap OK
01:00   → Rotate to Profile 3           After sending
01:00   → Join with Profile 3           Ready for next uplink

02:00   Profile 3 sends                 5min not elapsed, but 1min gap OK
02:00   → Rotate to Profile 0           After sending (wraps around)
02:00   → Join with Profile 0           Ready for next uplink

05:00   Profile 0 sends                 5min since 00:00 - Profile 0 due
05:00   → Rotate to Profile 2           After sending
05:00   → Join with Profile 2           Ready for next uplink

06:00   Profile 2 sends                 5min since 01:00 - Profile 2 due
06:00   → Rotate to Profile 3           After sending
06:00   → Join with Profile 3           Ready for next uplink

07:00   Profile 3 sends                 5min since 02:00 - Profile 3 due
07:00   → Rotate to Profile 0           After sending
07:00   → Join with Profile 0           Ready for next uplink

10:00   Profile 0 sends                 Pattern repeats every 5 minutes
...
```

### Key Features

#### Per-Profile Timing
Each profile maintains its own 5-minute interval:
- Profile 0: 00:00, 05:00, 10:00, 15:00...
- Profile 2: 01:00, 06:00, 11:00, 16:00...
- Profile 3: 02:00, 07:00, 12:00, 17:00...

#### Staggering Enforcement
Two conditions ensure 1-minute gaps:
1. Profile must be due (≥5 minutes since its last uplink)
2. Global timer (≥1 minute since ANY uplink)

This prevents:
- Multiple profiles sending simultaneously
- Less than 1-minute gaps between uplinks

#### Rotation Strategy
```
1. Send uplink from current profile
2. Update that profile's last uplink time
3. Rotate to next enabled profile
4. Re-join network with new credentials
5. Wait for that profile's timer
```

## Network Join Behavior

### After Rotation
- Device leaves current LoRaWAN session
- Loads new profile credentials (DevEUI, JoinEUI, AppKey, NwkKey)
- Performs OTAA join (~5-10 seconds)
- Establishes new session with network server

### Join Failure Handling
If join fails after rotation:
- Skip the uplink for that profile
- Device remains on failed profile
- Will retry join on next cycle
- Other profiles not affected

## Serial Monitor Output

### Successful Cycle
```
Profile 0 is due for uplink (5min elapsed)
Sending uplink from profile 0
Auto-rotation: Switching to next profile
Joined with profile 2

[Wait 1 minute]

Profile 2 is due for uplink (5min elapsed)
Sending uplink from profile 2
Auto-rotation: Switching to next profile
Joined with profile 3
```

### Staggering Check
```
Switching to profile 2 which is ready to send
Joined with profile 2
Sending uplink from profile 2
Auto-rotation: Switching to next profile
Joined with profile 3
```

## Benefits

### 1. Accurate Per-Device Timing
Each emulated device sends exactly every 5 minutes, mimicking real-world deployments.

### 2. Network-Friendly
Staggering prevents burst traffic and distributes load over time.

### 3. Scalable
Works with 2, 3, or 4 profiles without modification.

### 4. Robust
Independent tracking means one profile's failure doesn't affect others.

## Configuration

### Enable Auto-Rotation
1. Web UI: Navigate to `/lorawan/profiles`
2. Enable at least 2 profiles
3. Check "Enable Auto-Rotation" checkbox

### Adjust Timing
Modify constants in `main.cpp`:
```cpp
// Per-profile interval (currently 5 minutes)
if (time_since_last >= 300000) { ... }

// Stagger interval (currently 1 minute)
if (now - last_lorawan_uplink >= 60000) { ... }
```

## Troubleshooting

### Uplinks Not Staggered
**Symptom:** All profiles send at same time

**Cause:** Auto-rotation disabled or only 1 profile enabled

**Solution:** Enable auto-rotation and multiple profiles

### Profiles Sending Too Often
**Symptom:** Less than 5 minutes between same profile's uplinks

**Check:** 
- Verify per-profile tracking working
- Check Serial Monitor timestamps
- Ensure `last_profile_uplinks[]` array persisting

### Profiles Not Rotating
**Symptom:** Same profile used repeatedly

**Cause:** Rotation logic not executing

**Debug:**
1. Check `getEnabledProfileCount() > 1`
2. Verify `rotateToNextProfile()` returns true
3. Check join success after rotation

## Technical Implementation

### Files Modified
- `src/main.cpp` - Main loop timing logic

### Variables
```cpp
static unsigned long last_profile_uplinks[MAX_LORA_PROFILES] = {0};
static unsigned long last_lorawan_uplink = 0;
```

### Dependencies
- `lorawanHandler.getAutoRotation()`
- `lorawanHandler.getEnabledProfileCount()`
- `lorawanHandler.getActiveProfileIndex()`
- `lorawanHandler.getNextEnabledProfile()`
- `lorawanHandler.rotateToNextProfile()`
- `lorawanHandler.sendUplink()`

## Version History
- **v1.49** - Initial implementation of staggered timing
