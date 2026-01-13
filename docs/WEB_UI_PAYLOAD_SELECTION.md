# Web UI Payload Format Selection - User Guide

## Overview

**Version:** v1.51  
**Feature:** Web-based payload format selection per profile

Each LoRaWAN profile can now have its payload format configured directly from the web interface without needing to recompile or manually edit code.

---

## Accessing the Configuration

### Step 1: Navigate to Profiles Page

1. Open your web browser
2. Go to `https://stationsdata.local` (or your device IP)
3. Log in with your credentials
4. Click **"Profiles"** in the navigation menu

### Step 2: View Profile Overview

The profile overview table now includes a **"Payload Format"** column showing the current format for each profile:

| Profile | Name | DevEUI | **Payload Format** | Status | Actions |
|---------|------|--------|-------------------|--------|---------|
| 0 🟢 | Profile 0 | 0x70B3D5F9D0010203 | **Adeunis Modbus SF6** | ENABLED | Edit |
| 1 | SF6 Monitor Beta | 0x70B3D5F9D0010204 | **Cayenne LPP** | ENABLED | Enable / Edit |
| 2 | Raw Logger | 0x70B3D5F9D0010205 | **Raw Modbus Registers** | DISABLED | Enable / Edit |
| 3 | Custom Device | 0x70B3D5F9D0010206 | **Custom** | DISABLED | Enable / Edit |

---

## Changing Payload Format

### Step 1: Edit a Profile

1. Click the **"Edit"** button for the profile you want to modify
2. Scroll down to the profile detail section

### Step 2: Select Payload Format

In the profile edit form, you'll find a dropdown menu labeled **"Payload Format:"**

```
Profile Name: [Profile 0____________]

Payload Format: [Adeunis Modbus SF6 ▼]
                 ├─ Adeunis Modbus SF6
                 ├─ Cayenne LPP
                 ├─ Raw Modbus Registers
                 └─ Custom

💡 Different payload formats encode sensor data differently.
   Change this to match your decoder configuration.

JoinEUI (AppEUI): [0123456789ABCDEF]
DevEUI:           [70B3D5F9D0010203]
...
```

### Step 3: Choose Format

Select one of the four available formats:

1. **Adeunis Modbus SF6** (Default)
   - 10 bytes fixed
   - Original format for SF6 monitoring
   - Best for: Production SF6 monitoring applications

2. **Cayenne LPP**
   - 12 bytes typical
   - Standard IoT format
   - Best for: Multi-platform compatibility (ChirpStack, TTN, etc.)

3. **Raw Modbus Registers**
   - 10 bytes fixed
   - Direct register values
   - Best for: Debugging, custom applications

4. **Custom**
   - 13 bytes fixed
   - IEEE 754 float format
   - Best for: High-precision requirements, custom integrations

### Step 4: Save Changes

1. Click **"Save Profile X"** button at the bottom of the form
2. Wait for confirmation message: "Profile Updated!"
3. You'll be redirected back to the profiles page after 3 seconds

---

## Using Multiple Formats Simultaneously

### Scenario: Multi-Format Fleet Simulation

You can configure each profile with a different payload format to simulate a heterogeneous device fleet:

**Example Configuration:**

```
Profile 0: "SF6 Main Monitor"    → Adeunis Modbus SF6
Profile 1: "SF6 Backup Monitor"  → Cayenne LPP
Profile 2: "Debug Logger"        → Raw Modbus Registers
Profile 3: "Custom Integration"  → Custom Format
```

**With Auto-Rotation Enabled:**
- Each uplink cycles through enabled profiles
- Each transmission uses its profile's payload format
- ChirpStack receives different formats from the same device
- Perfect for testing multiple decoders

---

## ChirpStack Configuration

### Important: Match Decoders to Formats

Each payload format requires its own decoder in ChirpStack:

#### Method 1: Separate Device Profiles

1. Create 4 device profiles in ChirpStack:
   - "Adeunis SF6 Devices"
   - "Cayenne LPP Devices"
   - "Raw Modbus Devices"
   - "Custom Format Devices"

2. Configure appropriate decoder for each:
   - Adeunis: Use JavaScript decoder from `PAYLOAD_FORMAT_SELECTION.md`
   - Cayenne LPP: Built-in codec (select "Cayenne LPP" in dropdown)
   - Raw Modbus: Custom decoder for raw register values
   - Custom: IEEE 754 float decoder

3. When adding devices to ChirpStack:
   - Assign each DevEUI to the device profile matching its payload format
   - Example: Profile 0 (Adeunis) → "Adeunis SF6 Devices" profile

#### Method 2: Single Device with Format Detection

Create a smart decoder that auto-detects format based on payload size:

```javascript
function decodeUplink(input) {
  var bytes = input.bytes;
  
  // Detect format by payload size
  if (bytes.length === 10 && bytes[0] !== 0xFF) {
    // Adeunis Modbus SF6 or Raw Modbus (need additional logic)
    return decodeAdeunisModbusSF6(bytes);
  } else if (bytes.length === 12) {
    // Cayenne LPP (check channel markers)
    if (bytes[0] === 1 && bytes[1] === 0x67) {
      return decodeCayenneLPP(bytes);
    }
  } else if (bytes.length === 13 && bytes[0] === 0xFF) {
    // Custom format
    return decodeCustom(bytes);
  }
  
  return { errors: ["Unknown format"] };
}
```

---

## Verification

### Check Serial Output

After saving a profile configuration, monitor the serial console during the next uplink:

```
>>> Sending LoRaWAN uplink for Profile 1: SF6 Monitor Beta
Using payload format: Cayenne LPP
Payload breakdown (Cayenne LPP):
  Ch1 Temperature: 25.3 °C
  Ch2 Pressure: 1.013 bar
  Ch3 Density: 6.12 kg/m³
Message sent successfully!
Uplink count: 42
```

### Check Web Interface

1. Go back to `/lorawan/profiles`
2. Verify the **"Payload Format"** column shows the updated format
3. If auto-rotation is enabled, watch it cycle through formats

---

## Common Workflows

### Workflow 1: Testing New Decoder

**Goal:** Test a new Cayenne LPP decoder before production

1. Navigate to Profiles page
2. Edit Profile 1 (a test profile)
3. Change payload format to **"Cayenne LPP"**
4. Save profile
5. Manually activate Profile 1 (device will restart)
6. Watch ChirpStack for decoded data
7. Verify values match expected sensor readings

### Workflow 2: Migrating Format

**Goal:** Migrate from Adeunis to Cayenne LPP across all devices

1. Test new format on one profile first (Workflow 1)
2. Once verified, update remaining profiles:
   - Profile 0: Keep Adeunis (for comparison)
   - Profile 1-3: Change to Cayenne LPP
3. Enable auto-rotation
4. Monitor that both decoders work correctly
5. Eventually disable Profile 0 when migration complete

### Workflow 3: Debug Mode

**Goal:** Troubleshoot decoding issues

1. Create/edit a debug profile (e.g., Profile 2)
2. Change payload format to **"Raw Modbus Registers"**
3. Save and activate this profile
4. In ChirpStack, examine raw hex payload
5. Manually decode values to identify issue
6. Switch back to original format once resolved

---

## Payload Format Reference

### Quick Comparison

| Format | Size | Encoding | Use Case | ChirpStack Decoder |
|--------|------|----------|----------|-------------------|
| **Adeunis Modbus SF6** | 10 bytes | Big-endian integers | Production SF6 monitoring | Custom JavaScript |
| **Cayenne LPP** | 12 bytes | LPP standard | Multi-platform IoT | Built-in "Cayenne LPP" |
| **Raw Modbus** | 10 bytes | Big-endian integers | Debugging, raw data | Custom JavaScript |
| **Custom** | 13 bytes | IEEE 754 floats | High precision, custom | Custom JavaScript |

### Decoder Examples

See `PAYLOAD_FORMAT_SELECTION.md` for complete decoder implementations for all formats.

---

## Troubleshooting

### Issue: Dropdown not showing in form
**Solution:** Clear browser cache and reload page. Ensure firmware v1.51+

### Issue: Changes not saved
**Solution:** Check serial console for errors. Verify all required fields filled.

### Issue: Wrong data after format change
**Solution:** Verify ChirpStack decoder matches new payload format. Check serial output for correct format name.

### Issue: Format resets to Adeunis
**Solution:** This is expected for newly created profiles. Edit and save to change format.

### Issue: Can't see payload format in table
**Solution:** Widen browser window or scroll horizontally. Table is responsive.

---

## Advanced: API Usage

### Update Payload Type via HTTP POST

```bash
curl -X POST https://stationsdata.local/lorawan/profile/update \
  -u "admin:password" \
  -d "index=1" \
  -d "name=Test+Profile" \
  -d "payload_type=1" \
  -d "joinEUI=0123456789ABCDEF" \
  -d "devEUI=70B3D5F9D0010203" \
  -d "appKey=0123456789ABCDEF0123456789ABCDEF" \
  -d "nwkKey=0123456789ABCDEF0123456789ABCDEF"
```

**Payload Type Values:**
- `0` = Adeunis Modbus SF6
- `1` = Cayenne LPP
- `2` = Raw Modbus Registers
- `3` = Custom

---

## Benefits

✅ **No Code Changes Required** - Change formats without recompiling  
✅ **Per-Profile Configuration** - Each device can use different format  
✅ **Visual Feedback** - See current format in profile table  
✅ **Auto-Rotation Compatible** - Cycle through multiple formats  
✅ **Persistent Storage** - Format saved in NVS, survives reboot  
✅ **ChirpStack Ready** - Match decoders to payload formats  

---

## Related Documentation

- `PAYLOAD_FORMAT_SELECTION.md` - Technical details and decoder examples
- `AUTO_ROTATION_FEATURE.md` - Using multiple profiles together
- `LORA_PROFILES_IMPLEMENTATION.md` - Profile system overview
- `CHIRPSTACK_CONFIG_GUIDE.md` - Network server configuration

---

**Last Updated:** 2025-11-19  
**Firmware Version:** v1.51  
**Feature:** Web UI Payload Format Selection
