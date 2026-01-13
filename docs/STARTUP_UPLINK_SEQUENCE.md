# Startup Uplink Sequence

## Overview
On device boot, the ESP32 automatically sends initial uplinks from **all enabled LoRa profiles** with a delay between each transmission. This ensures all emulated devices are immediately visible in the network server (e.g., ChirpStack) without waiting for the normal 5-minute rotation cycle.

## Behavior

### Startup Flow
```
1. Device boots
2. Load LoRa profiles from NVS
3. Display profile configuration summary
4. ┌─ Startup Uplink Sequence ─┐
   │                            │
   │ For each enabled profile:  │
   │   → Switch to profile      │
   │   → Join network (OTAA)    │
   │   → Send uplink            │
   │   → Wait 10 seconds        │
   │                            │
   └────────────────────────────┘
5. Return to initial profile
6. Continue with normal operation
```

### Example with 3 Enabled Profiles

**Profiles:**
- Profile 0: ENABLED (Warehouse-Sensor-01)
- Profile 1: DISABLED
- Profile 2: ENABLED (Factory-Gateway-A)
- Profile 3: ENABLED (Office-Monitor-B)

**Startup Sequence:**
```
Time 00:00 - Boot complete, load profiles
Time 00:05 - Profile 0: Join network
Time 00:10 - Profile 0: Send uplink (1/3)
Time 00:20 - Profile 2: Join network (skip Profile 1 - disabled)
Time 00:25 - Profile 2: Send uplink (2/3)
Time 00:35 - Profile 3: Join network
Time 00:40 - Profile 3: Send uplink (3/3)
Time 00:50 - Return to Profile 0 for normal operation
Time 00:55 - Normal auto-rotation begins
```

## Serial Monitor Output

### Startup Uplink Sequence
```
========================================
Startup Uplink Sequence
========================================
Sending initial uplinks from 3 enabled profile(s)...

>>> Switching to Profile 0: Warehouse-Sensor-01
>>> Joining network with Profile 0...
Checking for saved nonces (required for DevNonce tracking)...
>>> has_nonces flag for Profile 0: false
>>> No saved nonces found for Profile 0

Initializing LoRaWAN node...
Performing OTAA join...
Join request sent
Waiting for join accept...
Join successful!
>>> Join successful for Profile 0!
>>> Sending startup uplink from Profile 0: Warehouse-Sensor-01
>>> Uplink sent from Profile 0 (1/3)
>>> Waiting 10 seconds before next profile...

>>> Switching to Profile 2: Factory-Gateway-A
>>> Joining network with Profile 2...
Checking for saved nonces (required for DevNonce tracking)...
>>> has_nonces flag for Profile 2: false
>>> No saved nonces found for Profile 2

Initializing LoRaWAN node...
Performing OTAA join...
Join request sent
Waiting for join accept...
Join successful!
>>> Join successful for Profile 2!
>>> Sending startup uplink from Profile 2: Factory-Gateway-A
>>> Uplink sent from Profile 2 (2/3)
>>> Waiting 10 seconds before next profile...

>>> Switching to Profile 3: Office-Monitor-B
>>> Joining network with Profile 3...
Checking for saved nonces (required for DevNonce tracking)...
>>> has_nonces flag for Profile 3: false
>>> No saved nonces found for Profile 3

Initializing LoRaWAN node...
Performing OTAA join...
Join request sent
Waiting for join accept...
Join successful!
>>> Join successful for Profile 3!
>>> Sending startup uplink from Profile 3: Office-Monitor-B
>>> Uplink sent from Profile 3 (3/3)

========================================
Startup sequence complete: 3/3 uplinks sent
========================================

>>> Returning to initial Profile 0 for normal operation
```

### Join Failure Example
```
>>> Switching to Profile 2: Factory-Gateway-A
>>> Joining network with Profile 2...
Join failed!
>>> Join failed for Profile 2, skipping uplink

>>> Switching to Profile 3: Office-Monitor-B
>>> Joining network with Profile 3...
Join successful!
>>> Join successful for Profile 3!
>>> Sending startup uplink from Profile 3: Office-Monitor-B
>>> Uplink sent from Profile 3 (2/3)
```

## Timing Details

### Per-Profile Timing
- **Join time**: ~5 seconds (OTAA handshake + RX windows)
- **Uplink time**: ~2 seconds (TX + RX1 + RX2 windows)
- **Inter-profile delay**: 10 seconds
- **Total per profile**: ~17 seconds

### Total Startup Time
- **1 enabled profile**: ~7 seconds
- **2 enabled profiles**: ~24 seconds (7 + 10 + 7)
- **3 enabled profiles**: ~41 seconds (7 + 10 + 7 + 10 + 7)
- **4 enabled profiles**: ~58 seconds (7 + 10 + 7 + 10 + 7 + 10 + 7)

### Why 10 Second Delay?
- Allows LoRaWAN RX windows to complete (RX1 + RX2)
- Prevents SPI conflicts between radio and display
- Gives network server time to process join/uplink
- Ensures clean state before switching profiles

## Benefits

### 1. Immediate Device Visibility
All enabled devices appear in network server within ~1 minute:
- No need to wait 5 minutes for first rotation cycle
- All devices show "last seen" timestamp immediately
- Useful for deployment verification

### 2. Network Server Registration
Each device sends initial data:
- Confirms successful join
- Establishes device presence
- Initializes uplink counters
- Sets baseline for monitoring

### 3. Troubleshooting
Startup sequence helps identify issues:
- Join failures per profile (credentials, coverage)
- Network connectivity problems
- Gateway availability
- Profile configuration errors

### 4. Testing & Verification
Quick validation during development:
- Verify all profiles configured correctly
- Test multiple device registrations
- Confirm ChirpStack device list
- Check payload decoding

## ChirpStack Impact

### Device List After Boot
All enabled profiles appear immediately:
```
Devices:
├── Warehouse-Sensor-01  (Profile 0) - Last seen: 30s ago
├── Factory-Gateway-A    (Profile 2) - Last seen: 20s ago
└── Office-Monitor-B     (Profile 3) - Last seen: 10s ago
```

### Frame Counter Initialization
Each device starts with:
- Uplink counter: 1 (from startup uplink)
- Downlink counter: 0
- Join counter: 1

### Network Traffic
Startup generates:
- **Join requests**: 1 per enabled profile
- **Uplinks**: 1 per enabled profile
- **Total**: 2 messages per profile

Example: 3 enabled profiles = 6 messages in ~40 seconds

## Disabled Profiles

### Behavior
Disabled profiles are **automatically skipped**:
```
Profile 0: ENABLED  → Join + Uplink ✓
Profile 1: DISABLED → Skipped
Profile 2: ENABLED  → Join + Uplink ✓
Profile 3: DISABLED → Skipped
```

### Serial Output
```
Sending initial uplinks from 2 enabled profile(s)...

>>> Switching to Profile 0: ...
>>> Uplink sent from Profile 0 (1/2)

>>> Switching to Profile 2: ...  (Profile 1 skipped)
>>> Uplink sent from Profile 2 (2/2)

Startup sequence complete: 2/2 uplinks sent
```

## Configuration

### Enable/Disable Startup Uplinks
Currently **always enabled** for all enabled profiles.

To disable (requires code modification):
```cpp
// In setup() function, comment out startup sequence:
/*
for (int i = 0; i < MAX_LORA_PROFILES; i++) {
    // ... startup uplink code ...
}
*/
```

### Adjust Delay Between Profiles
Modify delay in `src/main.cpp`:
```cpp
// Current: 10 seconds
delay(10000);

// Change to 15 seconds (more conservative)
delay(15000);

// Change to 5 seconds (faster, but riskier)
delay(5000);
```

**Note**: Less than 10 seconds may cause SPI conflicts or incomplete RX windows.

## Normal Operation Resume

### After Startup Sequence
Device returns to initial profile and begins normal operation:
- Auto-rotation continues from Profile 0
- 5-minute uplink intervals per profile
- 1-minute stagger between profiles
- Nonces saved and restored per profile

### Example Timeline
```
Time 00:00 - Boot
Time 00:40 - Startup sequence complete (3 profiles)
Time 00:41 - Normal operation begins with Profile 0
Time 05:41 - Profile 0 scheduled uplink (5min since startup)
Time 06:41 - Profile 2 scheduled uplink
Time 07:41 - Profile 3 scheduled uplink
Time 10:41 - Profile 0 scheduled uplink (continues...)
```

**Note**: First rotation uplink is 5 minutes after **startup sequence completion**, not device boot.

## Troubleshooting

### All Joins Fail
**Symptom**: "Startup sequence complete: 0/3 uplinks sent"

**Possible Causes:**
1. Gateway offline or out of range
2. Incorrect credentials in all profiles
3. Network server down
4. Wrong frequency plan

**Debug Steps:**
1. Check gateway status in ChirpStack
2. Verify gateway EUI matches your gateway
3. Test single device join manually
4. Check Serial Monitor for join error codes

### Some Profiles Fail
**Symptom**: "Startup sequence complete: 2/3 uplinks sent"

**Possible Causes:**
1. Incorrect credentials for failed profile
2. DevEUI not registered in network server
3. AppKey mismatch for that device
4. Profile-specific issue

**Debug Steps:**
1. Check Serial Monitor for which profile failed
2. Verify credentials for failed profile in ChirpStack
3. Re-enter AppKey from terminal output
4. Check network server logs for that DevEUI

### Startup Takes Too Long
**Symptom**: Device takes >60 seconds to boot

**Expected for 4 profiles** (~58 seconds normal)

**To speed up:**
1. Disable unused profiles (only enable needed ones)
2. Reduce inter-profile delay (risky, may cause issues)
3. Remove startup sequence (code modification)

### Device Reboots During Startup
**Symptom**: Watchdog reset or crash during sequence

**Possible Causes:**
1. Memory allocation issues
2. SPI conflicts
3. Stack overflow

**Solutions:**
1. Check available heap in Serial Monitor
2. Ensure delays between profiles
3. Update to latest firmware

## Implementation Details

### Code Location
`src/main.cpp` - `setup()` function

### Key Variables
```cpp
bool sent_profiles[MAX_LORA_PROFILES] = {false};  // Track which profiles sent
int uplinks_sent = 0;                              // Count successful uplinks
uint8_t initial_profile = lorawanHandler.getActiveProfileIndex();
```

### Profile Iteration
```cpp
for (int i = 0; i < MAX_LORA_PROFILES; i++) {
    LoRaProfile* prof = lorawanHandler.getProfile(i);
    if (!prof || !prof->enabled) {
        continue; // Skip disabled
    }
    
    // Switch, join, send, delay
}
```

### State Restoration
After sequence completes, device returns to initial profile:
```cpp
if (lorawanHandler.getActiveProfileIndex() != initial_profile) {
    lorawanHandler.setActiveProfile(initial_profile);
    lorawanHandler.begin();
    lorawan_joined = lorawanHandler.join();
}
```

## Network Server View

### ChirpStack Activity Log
After boot, you'll see:
```
Device Activity:
├── Warehouse-Sensor-01
│   └── 12:00:10 - Join request
│   └── 12:00:15 - Uplink (FPort 1)
├── Factory-Gateway-A
│   └── 12:00:25 - Join request
│   └── 12:00:30 - Uplink (FPort 1)
└── Office-Monitor-B
    └── 12:00:40 - Join request
    └── 12:00:45 - Uplink (FPort 1)
```

### Payload Content
Startup uplinks contain current sensor data:
- SF6 Density (float, 4 bytes)
- Temperature (float, 4 bytes)
- Pressure (float, 4 bytes)

Same format as normal uplinks.

## Best Practices

### 1. Register Devices First
Before booting ESP32:
- Add all enabled profiles to ChirpStack
- Configure DevEUI, AppKey for each
- Set up device profiles

### 2. Monitor Startup
Watch Serial Monitor during first boot:
- Verify all profiles join successfully
- Check for error messages
- Confirm uplink counts

### 3. Enable Only Needed Profiles
Don't enable all 4 profiles if you only need 2:
- Reduces startup time
- Less network traffic
- Simpler management

### 4. Check Network Server
After startup, verify in ChirpStack:
- All devices show "last seen" timestamp
- Uplink counters = 1 for each
- Device status = Active

## Related Documentation
- `AUTO_ROTATION_FEATURE.md` - Multi-profile rotation
- `PER_PROFILE_NONCE_MANAGEMENT.md` - DevNonce handling
- `TIMING_STRATEGY.md` - Normal operation timing
- `CHIRPSTACK_CONFIG_GUIDE.md` - Network server setup

## Version History
- **v1.49** - Added startup uplink sequence for all enabled profiles
- **<v1.49** - Single profile startup uplink only

## Future Enhancements
Potential improvements:
- [ ] Configurable startup delay via web UI
- [ ] Option to disable startup sequence
- [ ] Retry failed profiles once
- [ ] Parallel joins (risky, but faster)
- [ ] Progress indicator on e-ink display
