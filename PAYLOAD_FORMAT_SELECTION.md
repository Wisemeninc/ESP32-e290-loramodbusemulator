# LoRaWAN Payload Format Selection

## Overview

**Version:** v1.51  
**Feature:** Per-profile payload format selection

Each LoRaWAN profile can now use a different payload format, allowing more flexible device emulation scenarios where different virtual devices send different data structures.

## Available Payload Formats

### 1. Adeunis Modbus SF6 (Default)
**Size:** 10 bytes fixed  
**Use Case:** Original format for SF6 gas monitoring devices

**Structure:**
```
Byte 0-1:  Header (uplink counter) - Ignored by decoder
Byte 2-3:  SF6 Density (kg/m³ × 100, big-endian)
Byte 4-5:  SF6 Pressure @20°C (bar × 1000, big-endian)
Byte 6-7:  SF6 Temperature (Kelvin × 10, big-endian)
Byte 8-9:  SF6 Pressure Variance (bar × 1000, big-endian)
```

**Example Decoder (JavaScript):**
```javascript
function decodeUplink(input) {
  var bytes = input.bytes;
  
  // Skip header (bytes 0-1)
  
  var density = ((bytes[2] << 8) | bytes[3]) / 100.0;       // kg/m³
  var pressure = ((bytes[4] << 8) | bytes[5]) / 1000.0;     // bar
  var temp_k = ((bytes[6] << 8) | bytes[7]) / 10.0;         // Kelvin
  var temp_c = temp_k - 273.15;                              // °C
  var pressure_var = ((bytes[8] << 8) | bytes[9]) / 1000.0; // bar
  
  return {
    data: {
      sf6_density: density,
      sf6_pressure_20c: pressure,
      sf6_temperature: temp_c,
      sf6_pressure_variance: pressure_var
    }
  };
}
```

---

### 2. Cayenne LPP (Low Power Payload)
**Size:** Variable (12 bytes typical)  
**Use Case:** Standard IoT format compatible with many platforms

**Structure:**
```
Channel 1 (Temperature):
  Byte 0:    Channel = 1
  Byte 1:    Type = 0x67 (Temperature)
  Byte 2-3:  Value (0.1°C signed, big-endian)

Channel 2 (Pressure):
  Byte 4:    Channel = 2
  Byte 5:    Type = 0x02 (Analog Input)
  Byte 6-7:  Value (0.01 signed, big-endian)

Channel 3 (Density):
  Byte 8:    Channel = 3
  Byte 9:    Type = 0x02 (Analog Input)
  Byte 10-11: Value (0.01 signed, big-endian)
```

**Cayenne LPP Decoder:** Built into many LoRaWAN platforms (ChirpStack, TTN, etc.)

---

### 3. Raw Modbus Registers
**Size:** 10 bytes fixed  
**Use Case:** Direct register dump for debugging or custom applications

**Structure:**
```
Byte 0-1:   Uplink counter (uint16, big-endian)
Byte 2-3:   SF6 Density raw value (uint16, big-endian)
Byte 4-5:   SF6 Pressure @20°C raw value (uint16, big-endian)
Byte 6-7:   SF6 Temperature raw value (uint16, big-endian)
Byte 8-9:   SF6 Pressure Variance raw value (uint16, big-endian)
```

**Note:** Raw values must be decoded according to Modbus register specifications:
- Density: value / 100.0 → kg/m³
- Pressure: value / 1000.0 → bar
- Temperature: value / 10.0 → Kelvin (subtract 273.15 for °C)

---

### 4. Custom Format
**Size:** 13 bytes fixed  
**Use Case:** Example custom format using IEEE 754 floats

**Structure:**
```
Byte 0:    Format identifier (0xFF)
Byte 1-4:  Temperature (float, °C, little-endian)
Byte 5-8:  Pressure (float, bar, little-endian)
Byte 9-12: Density (float, kg/m³, little-endian)
```

**Example Decoder (JavaScript):**
```javascript
function decodeUplink(input) {
  var bytes = input.bytes;
  
  if (bytes[0] !== 0xFF) {
    return { errors: ["Invalid custom format identifier"] };
  }
  
  // IEEE 754 float decoder (little-endian)
  function bytesToFloat(bytes, offset) {
    var bits = (bytes[offset]) | (bytes[offset+1] << 8) | 
               (bytes[offset+2] << 16) | (bytes[offset+3] << 24);
    var sign = (bits >>> 31) === 0 ? 1.0 : -1.0;
    var e = (bits >>> 23) & 0xff;
    var m = (e === 0) ? (bits & 0x7fffff) << 1 : (bits & 0x7fffff) | 0x800000;
    return sign * m * Math.pow(2, e - 150);
  }
  
  return {
    data: {
      temperature: bytesToFloat(bytes, 1),
      pressure: bytesToFloat(bytes, 5),
      density: bytesToFloat(bytes, 9)
    }
  };
}
```

---

## Configuration

### Code Structure

**Enum Definition (config.h):**
```cpp
enum PayloadType {
    PAYLOAD_ADEUNIS_MODBUS_SF6 = 0,
    PAYLOAD_CAYENNE_LPP = 1,
    PAYLOAD_RAW_MODBUS = 2,
    PAYLOAD_CUSTOM = 3
};
```

**Profile Structure (config.h):**
```cpp
struct LoRaProfile {
    char name[33];
    uint64_t devEUI;
    uint64_t joinEUI;
    uint8_t appKey[16];
    uint8_t nwkKey[16];
    bool enabled;
    PayloadType payload_type;  // New field in v1.51
};
```

### Setting Payload Type Per Profile

Currently, payload types are set during profile initialization:
- **Default:** All new profiles use `PAYLOAD_ADEUNIS_MODBUS_SF6`
- **Existing profiles:** Automatically migrated to default format if loaded from older firmware

### Future Web UI Enhancement (Planned)

```html
<form action="/api/lorawan/profile/0/payload" method="POST">
  <label for="payload_type">Payload Format:</label>
  <select name="payload_type" id="payload_type">
    <option value="0">Adeunis Modbus SF6</option>
    <option value="1">Cayenne LPP</option>
    <option value="2">Raw Modbus Registers</option>
    <option value="3">Custom Format</option>
  </select>
  <button type="submit">Update</button>
</form>
```

---

## Implementation Details

### Payload Builder Functions

Each format has a dedicated builder function in `lorawan_handler.cpp`:

```cpp
size_t buildAdeunisModbusSF6Payload(uint8_t* payload, const InputRegisters& input);
size_t buildCayenneLPPPayload(uint8_t* payload, const InputRegisters& input);
size_t buildRawModbusPayload(uint8_t* payload, const InputRegisters& input);
size_t buildCustomPayload(uint8_t* payload, const InputRegisters& input);
```

### Uplink Process

1. **Profile Selection:** Active profile determines payload type
2. **Format Detection:** `sendUplink()` reads `current_profile->payload_type`
3. **Builder Selection:** Switch statement calls appropriate builder
4. **Payload Construction:** Builder formats data according to specification
5. **Transmission:** RadioLib sends payload with returned size

### Serial Output Example

```
Using payload format: Cayenne LPP
Payload breakdown (Cayenne LPP):
  Ch1 Temperature: 25.3 °C
  Ch2 Pressure: 1.013 bar
  Ch3 Density: 6.12 kg/m³
Message sent successfully!
```

---

## Testing Scenarios

### Scenario 1: Multi-Format Fleet
- **Profile 0:** "SF6 Monitor Alpha" → Adeunis Modbus SF6
- **Profile 1:** "SF6 Monitor Beta" → Cayenne LPP
- **Profile 2:** "Raw Data Logger" → Raw Modbus Registers
- **Profile 3:** "Custom Sensor" → Custom Format

**Result:** Each profile sends data in its own format, simulating a heterogeneous fleet

### Scenario 2: Format Migration Testing
- Start with Adeunis format on all profiles
- Change Profile 1 to Cayenne LPP
- Verify both decoders work correctly in ChirpStack
- Compare data accuracy between formats

### Scenario 3: Bandwidth Optimization
- Use Cayenne LPP (12 bytes) for most devices
- Use Custom float format (13 bytes) for high-precision needs
- Use Raw Modbus (12 bytes) for debugging
- Compare network airtime consumption

---

## ChirpStack Configuration

### Device Profile Settings

1. **Create Separate Device Profiles** for each payload type:
   - "Adeunis SF6 Monitors" → Adeunis decoder
   - "Cayenne LPP Devices" → Cayenne LPP codec (built-in)
   - "Raw Modbus Devices" → Raw Modbus decoder
   - "Custom Format Devices" → Custom decoder

2. **Assign Devices** to appropriate profile based on their payload_type

3. **Configure Decoders** in Device Profile:
   - Codec: "JavaScript functions"
   - Paste appropriate decoder function
   - Test with sample payloads

---

## Migration Notes

### From v1.50 to v1.51

**NVS Compatibility:**
- Existing profiles automatically load with `PAYLOAD_ADEUNIS_MODBUS_SF6`
- No manual migration required
- Old firmware behavior preserved

**Code Changes:**
- `LoRaProfile` struct grew by 1 byte (PayloadType enum)
- `sendUplink()` refactored to support multiple formats
- Four new payload builder functions added

---

## Performance Impact

### Memory Usage
- **ROM:** +2.5 KB (payload builder functions)
- **RAM:** +1 byte per profile (payload_type field)
- **Stack:** +256 bytes (temporary payload buffer)

### Airtime Comparison (SF7, EU868)
| Format | Size | Time-on-Air | Daily Quota (1% duty cycle) |
|--------|------|-------------|------------------------------|
| Adeunis SF6 | 10 bytes | ~41 ms | 2,100 messages |
| Cayenne LPP | 12 bytes | ~46 ms | 1,870 messages |
| Raw Modbus | 10 bytes | ~41 ms | 2,100 messages |
| Custom Float | 13 bytes | ~46 ms | 1,870 messages |

---

## Future Enhancements

### Planned Features
- [ ] Web UI dropdown for payload type selection per profile
- [ ] JSON API endpoint: `POST /api/lorawan/profile/{id}/payload`
- [ ] Live payload preview in web UI
- [ ] Decoder test tool with sample data
- [ ] Custom format editor for user-defined payloads
- [ ] Payload compression options (e.g., variable-length encoding)

### Extensibility
To add a new payload format:

1. Add enum value to `PayloadType` in `config.h`
2. Add name to `PAYLOAD_TYPE_NAMES` array
3. Implement `buildYourFormatPayload()` in `lorawan_handler.cpp`
4. Add declaration to `lorawan_handler.h`
5. Add case to switch statement in `sendUplink()`
6. Document format in this file
7. Provide decoder example

---

## Troubleshooting

### Issue: Profile shows wrong payload format after upgrade
**Solution:** Profiles load with default format. Check serial output during boot.

### Issue: ChirpStack shows decode errors
**Solution:** Verify device profile uses correct decoder for payload_type.

### Issue: Data values seem incorrect
**Solution:** Check payload breakdown in serial output. Verify decoder matches format.

### Issue: Compile error after adding new format
**Solution:** Ensure enum value, name array, and switch case all match.

---

## References

- [Cayenne LPP Specification](https://developers.mydevices.com/cayenne/docs/lora/)
- [RadioLib LoRaWAN Documentation](https://jgromes.github.io/RadioLib/)
- [ChirpStack Codec API](https://www.chirpstack.io/docs/chirpstack/use/device-profiles.html)
- Original implementation: v1.48 (2025-01-15)
- Payload format feature: v1.51 (2025-01-XX)

---

**Last Updated:** 2025-01-XX  
**Firmware Version:** 1.51  
**Author:** ESP32 LoRaWAN Multi-Profile Emulator Project
