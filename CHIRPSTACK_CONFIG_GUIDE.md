# ChirpStack Configuration Guide

## Overview
This guide explains how to register your ESP32 LoRa profiles in ChirpStack using the credentials displayed at startup.

## Step 1: Get Device Credentials from Terminal

Connect to your ESP32 via Serial Monitor (115200 baud) and look for the profile configuration section:

```
========================================
LoRa Profile Configuration Summary
========================================
Auto-Rotation: ENABLED
Enabled Profiles: 3 of 4
Active Profile: 0

Profile 0: Warehouse-Sensor-01
  Status: ENABLED
  DevEUI: 0x0000000000A1B2C3
  JoinEUI: 0x70B3D57ED0000001
  AppKey: 0123456789ABCDEF0123456789ABCDEF
  NwkKey: 0123456789ABCDEF0123456789ABCDEF
  >>> CURRENTLY ACTIVE <<<
```

## Step 2: Register Application in ChirpStack

1. Log into ChirpStack web interface
2. Navigate to **Applications** → **Create Application**
3. Fill in application details:
   - **Name**: e.g., "ESP32 LoRa Emulator"
   - **Description**: "Multi-profile LoRaWAN device emulation"
4. Click **Submit**

## Step 3: Add Device to ChirpStack

For each **enabled** profile, create a device:

### 3.1 Create Device
1. Open your application
2. Click **Devices** → **Create**
3. Fill in device details:

**Device Information:**
- **Name**: Use profile name from terminal (e.g., "Warehouse-Sensor-01")
- **Description**: Optional
- **Device EUI**: Copy from terminal, **remove "0x" prefix**
  - Terminal shows: `0x0000000000A1B2C3`
  - Enter in ChirpStack: `0000000000A1B2C3`
- **Device Profile**: Select appropriate device profile (must support OTAA)

4. Click **Submit**

### 3.2 Configure Keys
After creating the device:

1. Click on the device name
2. Navigate to **Keys (OTAA)** tab
3. Fill in the keys:

**Application Key:**
- Copy **AppKey** from terminal (32 hex characters)
- Terminal shows: `0123456789ABCDEF0123456789ABCDEF`
- Paste directly into ChirpStack (no spaces, no "0x")

**Network Key (LoRaWAN 1.1+):**
- If your ChirpStack uses LoRaWAN 1.1 or later, also configure:
- Copy **NwkKey** from terminal
- Paste into Network Key field

4. Click **Submit**

## Step 4: Verify Join-EUI (Application EUI)

In ChirpStack:
1. Go to **Applications** → Your application
2. Click **Configuration**
3. Verify **Join-EUI** matches terminal output
   - Terminal shows: `0x70B3D57ED0000001`
   - ChirpStack should show: `70B3D57ED0000001`

If different, update device configuration to match.

## Step 5: Configure Multiple Profiles (Auto-Rotation)

If using auto-rotation with multiple profiles, repeat Step 3 for each **enabled** profile:

### Example: 3 Enabled Profiles

**Profile 0:**
- Name: Warehouse-Sensor-01
- DevEUI: `0000000000A1B2C3`
- AppKey: `0123456789ABCDEF0123456789ABCDEF`

**Profile 2:**
- Name: Factory-Gateway-A
- DevEUI: `0200000000A1B2C3`
- AppKey: `FEDCBA98765432100123456789ABCDEF`

**Profile 3:**
- Name: Office-Monitor-B
- DevEUI: `0300000000A1B2C3`
- AppKey: `ABCDEF01234567890123456789ABCDEF`

**Note:** Skip disabled profiles (e.g., Profile 1 if disabled).

## Step 6: Test Connection

1. Reboot ESP32 or wait for next join attempt
2. Watch Serial Monitor for:
   ```
   Checking for saved nonces...
   Performing OTAA join...
   Join request sent
   Waiting for join accept...
   Join successful!
   ```

3. Check ChirpStack **Device Data** tab for:
   - Join request received
   - Join accept sent
   - Device activation successful

## ChirpStack Configuration Tips

### Device Profile Settings
Create or use a device profile with:
- **Region**: EU868 (or your region)
- **MAC Version**: LoRaWAN 1.0.3 or 1.1.0
- **Regional Parameters Revision**: A or B
- **Supports OTAA**: ✓ Enabled
- **Uplink interval**: 5 minutes (300 seconds)

### Class Support
- **Device Class**: Class A (default)
- Not Class B or C (ESP32 uses Class A)

### Frame Counters
For development/testing:
- **Disable frame counter validation**: Optional
- Helps during repeated joins/resets
- **Enable for production**

## Auto-Rotation Considerations

When auto-rotation is enabled:

1. **Multiple Devices**: Each profile appears as separate device in ChirpStack
2. **Staggered Uplinks**: Devices send 1 minute apart
3. **Individual Tracking**: Each device has own frame counters and session
4. **Network Load**: Total uplinks = (number of enabled profiles) × 12 per hour

### Example Schedule (3 profiles)
```
T+0min: Profile 0 joins and sends → ChirpStack sees "Warehouse-Sensor-01"
T+1min: Profile 2 joins and sends → ChirpStack sees "Factory-Gateway-A"
T+2min: Profile 3 joins and sends → ChirpStack sees "Office-Monitor-B"
T+5min: Profile 0 sends again
T+6min: Profile 2 sends again
T+7min: Profile 3 sends again
```

## Troubleshooting

### Join Failed
**Symptom:** "Join failed" in Serial Monitor

**Check:**
1. **DevEUI matches** in ChirpStack and ESP32
2. **AppKey is correct** (32 hex chars, no spaces)
3. **Join-EUI matches** application configuration
4. **Gateway is online** and receiving packets
5. **Frequency plan** matches (EU868, US915, etc.)

### Wrong AppKey
**Symptom:** Join request received but no join accept

**Solution:**
1. Copy AppKey from terminal startup output
2. Verify no extra characters or spaces
3. Update in ChirpStack Keys (OTAA) tab
4. Reboot ESP32 to try again

### Multiple Join Attempts
**Symptom:** Device joins multiple times with same DevEUI

**Cause:** Auto-rotation disabled but device restarting

**Solution:**
- Check if intended behavior
- If single profile mode, join once and send periodically
- If multi-profile, verify auto-rotation enabled

### Frame Counter Issues
**Symptom:** "Invalid frame counter" in ChirpStack

**Solution:**
1. Reset frame counter in ChirpStack (Device → Edit)
2. Or disable frame counter validation (testing only)
3. Reboot ESP32 for fresh session

## Payload Format

### Uplink Payload Structure
The ESP32 sends SF6 sensor data (from Modbus registers):

**Byte Layout:**
- Bytes 0-3: SF6 Density (float, LSB first)
- Bytes 4-7: Temperature (float, LSB first)
- Bytes 8-11: Pressure (float, LSB first)

### Decoder for ChirpStack

Add this JavaScript decoder in ChirpStack Application:

```javascript
function Decode(fPort, bytes) {
    if (bytes.length !== 12) {
        return {error: "Invalid payload length"};
    }
    
    // Helper to decode float (LSB)
    function bytesToFloat(bytes, offset) {
        var bits = (bytes[offset+3] << 24) | (bytes[offset+2] << 16) | 
                   (bytes[offset+1] << 8) | bytes[offset];
        var sign = (bits >>> 31 === 0) ? 1.0 : -1.0;
        var e = (bits >>> 23) & 0xff;
        var m = (e === 0) ? (bits & 0x7fffff) << 1 : (bits & 0x7fffff) | 0x800000;
        return sign * m * Math.pow(2, e - 150);
    }
    
    return {
        sf6_density: bytesToFloat(bytes, 0),
        temperature: bytesToFloat(bytes, 4),
        pressure: bytesToFloat(bytes, 8)
    };
}
```

## Quick Reference Card

### Copy These Values to ChirpStack

**From Terminal Output:**
```
DevEUI: 0x0000000000A1B2C3  → ChirpStack: 0000000000A1B2C3 (remove 0x)
JoinEUI: 0x70B3D57ED0000001 → ChirpStack: 70B3D57ED0000001 (remove 0x)
AppKey: 0123...CDEF         → ChirpStack: 0123...CDEF (copy exact 32 chars)
NwkKey: 0123...CDEF         → ChirpStack: 0123...CDEF (copy exact 32 chars)
```

**Formatting Rules:**
- Remove "0x" prefix from DevEUI and JoinEUI
- Copy AppKey/NwkKey exactly (32 hex characters)
- No spaces, no dashes, no colons
- Case insensitive (but keep uppercase for consistency)

## Additional Resources

- **ChirpStack Docs**: https://www.chirpstack.io/docs/
- **LoRaWAN Spec**: https://lora-alliance.org/resource_hub/lorawan-specification-v1-0-3/
- **ESP32 Repo**: Check README.md for project details

## Support

If you encounter issues:
1. Check Serial Monitor output for error messages
2. Verify ChirpStack gateway is receiving packets
3. Compare terminal credentials with ChirpStack configuration
4. Check LoRaWAN frequency plan matches gateway
5. Review ChirpStack application and device logs

## Version
Updated for firmware v1.49 with full key display
