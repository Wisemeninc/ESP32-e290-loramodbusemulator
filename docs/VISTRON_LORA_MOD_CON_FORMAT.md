# Vistron Lora Mod Con Payload Format

## Overview

**Format Name:** Vistron Lora Mod Con  
**Payload Size:** 16 bytes (Frame Type 3 - Periodic Uplink)  
**Use Case:** Vistron LoRa Modbus Concentrator with Trafag H72517o SF6 sensor

This format wraps Modbus sensor data in a Vistron-specific frame structure, commonly used with LoRa Modbus concentrators that forward data from Modbus RTU devices to LoRaWAN networks.

---

## Payload Structure

### Frame Type 3: Periodic Modbus Data Uplink (16 bytes)

```
Byte 0:     Frame Type = 0x03 (Periodic Modbus data)
Byte 1:     Reserved/Status = 0x00
Byte 2:     Error Code = 0x00 (No errors)
Byte 3-4:   Uplink Counter (big-endian uint16)
Byte 5-6:   Reserved = 0x0000
Byte 7:     Modbus Data Length = 0x08 (8 bytes)
Byte 8-15:  Modbus Sensor Data (Trafag H72517o format)
```

### Modbus Sensor Data (Bytes 8-15)

Based on Trafag H72517o SF6 Monitoring System format:

```
Byte 8-9:   Density (kg/m³ × 100, big-endian)
Byte 10-11: Absolute Pressure at 20°C (bar × 1000, big-endian)
Byte 12-13: Temperature (Kelvin × 10, big-endian)
Byte 14-15: Absolute Pressure current (bar × 1000, big-endian)
```

---

## Example Payload

### Hex: `03 00 00 00 2A 00 00 08 17 D0 03 E9 0B CD 03 F5`

**Breakdown:**
```
Header:
  03        - Frame Type: Periodic Modbus uplink
  00        - Status: OK
  00        - Error Code: None
  00 2A     - Uplink Counter: 42
  00 00     - Reserved
  08        - Data Length: 8 bytes

Modbus Data (Trafag H72517o):
  17 D0     - Density: 6096 → 60.96 kg/m³
  03 E9     - Pressure @20°C: 1001 → 1.001 bar
  0B CD     - Temperature: 3021 → 302.1 K → 28.95 °C
  03 F5     - Absolute Pressure: 1013 → 1.013 bar
```

---

## ChirpStack Decoder (JavaScript)

### Complete Decoder

```javascript
function decodeUplink(input) {
  var bytes = input.bytes;
  var port = input.fPort;
  
  // Validate port
  if (port !== 1) {
    return {
      errors: ["Invalid port. Expected port 1, got: " + port]
    };
  }
  
  // Validate payload length
  if (bytes.length < 8) {
    return {
      errors: ["Payload too short: " + bytes.length + " bytes"]
    };
  }
  
  var frameType = bytes[0];
  
  // Check if frame contains Modbus data
  if (frameType !== 3 && frameType !== 5) {
    return {
      warnings: ["Frame type " + frameType + " does not contain Modbus data"]
    };
  }
  
  var modbusDataStart;
  var errorCode;
  
  if (frameType === 3) {
    // Periodic Modbus uplink
    errorCode = bytes[2];
    modbusDataStart = 8;
    
    if (errorCode !== 0) {
      return {
        errors: ["Modbus error code: " + errorCode]
      };
    }
  } else if (frameType === 5) {
    // On-demand Modbus request
    errorCode = bytes[1];
    modbusDataStart = 7;
    
    if (errorCode !== 0) {
      return {
        errors: ["Modbus error code: " + errorCode]
      };
    }
  }
  
  // Validate we have enough data for Modbus payload
  if (bytes.length < modbusDataStart + 8) {
    return {
      errors: ["Insufficient Modbus data. Expected 8 bytes at offset " + modbusDataStart]
    };
  }
  
  // Decode Trafag H72517o Modbus data
  var modbusData = bytes.slice(modbusDataStart, modbusDataStart + 8);
  
  // Density (kg/m³ × 100)
  var density_raw = (modbusData[0] << 8) | modbusData[1];
  var density = (density_raw / 100.0).toFixed(3);
  
  // Pressure at 20°C (bar × 1000)
  var pressure20_raw = (modbusData[2] << 8) | modbusData[3];
  var pressure20 = (pressure20_raw / 1000.0).toFixed(3);
  
  // Temperature (Kelvin × 10)
  var temp_k_raw = (modbusData[4] << 8) | modbusData[5];
  var temp_k = temp_k_raw / 10.0;
  var temp_c = (temp_k - 273.15).toFixed(3);
  
  // Absolute Pressure (bar × 1000)
  var pressure_raw = (modbusData[6] << 8) | modbusData[7];
  var pressure = (pressure_raw / 1000.0).toFixed(3);
  
  return {
    data: {
      frameType: frameType,
      density: parseFloat(density),
      absolutePressureAt20Celsius: parseFloat(pressure20),
      temperature: parseFloat(temp_c),
      absolutePressure: parseFloat(pressure)
    }
  };
}
```

### Simplified Decoder (No Error Checking)

```javascript
function decodeUplink(input) {
  var bytes = input.bytes;
  
  // Determine Modbus data start based on frame type
  var offset = (bytes[0] === 3) ? 8 : 7;
  
  return {
    data: {
      density: ((bytes[offset] << 8) | bytes[offset+1]) / 100.0,
      pressure20C: ((bytes[offset+2] << 8) | bytes[offset+3]) / 1000.0,
      temperature: (((bytes[offset+4] << 8) | bytes[offset+5]) / 10.0) - 273.15,
      pressure: ((bytes[offset+6] << 8) | bytes[offset+7]) / 1000.0
    }
  };
}
```

---

## PHP Decoder (Original Implementation)

The ESP32 implementation is based on these two PHP decoders:

### VistronLoraModConDecoder.php
- Extracts frame type and error status
- Determines Modbus data offset
- Forwards to Trafag decoder

### TrafagH72517oModbusSf6Decoder.php
- Decodes 8-byte Modbus payload
- Converts raw values to engineering units
- Returns sensor measurements

---

## Frame Types

### Frame Type 3: Periodic Modbus Data Uplink
- **Size:** 16 bytes
- **Trigger:** Scheduled periodic transmission
- **Modbus Data Offset:** Byte 8
- **Use Case:** Regular monitoring, automated reporting

### Frame Type 5: Receive Modbus Request Data
- **Size:** 15 bytes
- **Trigger:** On-demand query response
- **Modbus Data Offset:** Byte 7
- **Use Case:** Manual queries, debugging

**Note:** ESP32 implementation currently only generates Frame Type 3 (periodic).

---

## Error Codes

### Frame Type 3 (Byte 2: Error Code)
- `0x00` = No error (data valid)
- `0x01` = Modbus timeout
- `0x02` = Modbus CRC error
- `0x03` = Invalid Modbus response
- `0xFF` = General communication error

### Frame Type 5 (Byte 1: Error Code)
- `0x00` = No error (data valid)
- `0x01` = Request timeout
- `0x02` = Invalid response
- `0xFF` = General error

---

## Sensor Data Specifications

### Trafag H72517o SF6 Monitoring System

**Measured Parameters:**
1. **Gas Density** - Compensated SF6 density at 20°C
2. **Pressure at 20°C** - Pressure normalized to 20°C reference
3. **Temperature** - Ambient temperature inside gas compartment
4. **Absolute Pressure** - Current pressure (not temperature compensated)

**Measurement Ranges:**
- Density: 0 - 100 kg/m³ (resolution: 0.01 kg/m³)
- Pressure: 0 - 65.535 bar (resolution: 0.001 bar)
- Temperature: -40 to +85 °C (resolution: 0.1 °C)

**Accuracy:**
- Density: ±0.5 kg/m³
- Pressure: ±0.3% FS
- Temperature: ±0.5 °C

---

## Configuration in ChirpStack

### Step 1: Create Device Profile

1. Navigate to **Device Profiles** in ChirpStack
2. Click **Create**
3. Set:
   - Name: `Vistron LoRa Mod Con - Trafag SF6`
   - Region: `EU868` (or your region)
   - MAC Version: `1.0.3`
   - Regional Parameters Revision: `A`
   - Max EIRP: `16`

### Step 2: Configure Codec

1. In Device Profile, go to **Codec** tab
2. Select **Payload codec: JavaScript functions**
3. Paste the decoder function above
4. Click **Submit**

### Step 3: Test Decoder

Sample payloads for testing:

```javascript
// Good data (Frame Type 3)
0300002A000008 17D003E90BCD03F5
// Expected: density=60.96, pressure20C=1.001, temp=28.95, pressure=1.013

// Good data (Frame Type 5)
0500 00000008 0BB803E10BA203E8
// Expected: density=30.00, pressure20C=0.993, temp=23.05, pressure=1.000

// Error case (Frame Type 3 with error)
03000102000008 0000000000000000
// Expected: Error message
```

### Step 4: Add Device

1. Create new device with Vistron profile
2. Enter DevEUI, AppKey, NwkKey from ESP32
3. Assign to **Vistron LoRa Mod Con** profile
4. Verify uplinks decode correctly

---

## Integration with ESP32 Multi-Profile System

### Selecting Vistron Format

**Via Web UI:**
1. Go to `https://stationsdata.local/lorawan/profiles`
2. Click **Edit** on desired profile
3. Select **"Vistron Lora Mod Con"** from dropdown
4. Click **Save Profile**

**Example Multi-Format Fleet:**
```
Profile 0: "SF6 Main"       → Adeunis Modbus SF6
Profile 1: "SF6 Vistron"    → Vistron Lora Mod Con
Profile 2: "Debug Raw"      → Raw Modbus Registers
Profile 3: "IoT Standard"   → Cayenne LPP
```

### Serial Output Example

```
>>> Sending LoRaWAN uplink for Profile 1: SF6 Vistron
Using payload format: Vistron Lora Mod Con
Payload breakdown (Vistron Lora Mod Con):
  Frame Type: 3 (Periodic Modbus uplink)
  Error Code: 0 (No errors)
  Uplink Count: 42
  Modbus Data (Trafag H72517o format):
    Density: 6096 (60.96 kg/m³)
    Pressure @20°C: 1001 (1.001 bar)
    Temperature: 3021 (28.9 °C)
    Absolute Pressure: 1013 (1.013 bar)
Payload (16 bytes): 03 00 00 00 2A 00 00 08 17 D0 03 E9 0B CD 03 F5
Message sent successfully!
```

---

## Comparison with Other Formats

| Aspect | Vistron Lora Mod Con | Adeunis Modbus SF6 | Cayenne LPP |
|--------|---------------------|-------------------|-------------|
| **Size** | 16 bytes | 10 bytes | 12 bytes |
| **Header** | 8 bytes (detailed) | 2 bytes (minimal) | None |
| **Error Reporting** | Built-in | None | None |
| **Standard** | Vistron proprietary | Custom | IoT standard |
| **Frame Types** | Multiple (3, 5) | Single | Single |
| **Modbus Compatible** | Yes (wrapped) | Yes (direct) | No |
| **ChirpStack Support** | Custom decoder | Custom decoder | Built-in |
| **Use Case** | Modbus concentrators | Direct devices | Generic IoT |

---

## Advantages

✅ **Error Reporting** - Frame includes error codes  
✅ **Frame Type Identification** - Supports multiple message types  
✅ **Uplink Counter** - Built into frame header  
✅ **Modbus Native** - Designed for Modbus RTU forwarding  
✅ **Industrial Standard** - Common in SCADA/industrial IoT  
✅ **Diagnostic Info** - Reserved bytes for future expansion  

---

## Limitations

⚠️ **Larger Payload** - 16 bytes vs 10 bytes (Adeunis)  
⚠️ **Proprietary Format** - Vistron-specific, not widely supported  
⚠️ **Frame Type 5 Not Implemented** - ESP32 only sends Frame Type 3  
⚠️ **Current Pressure Mapping** - Uses `pressure_var` field (no dedicated field)  

---

## Future Enhancements

### Planned Features
- [ ] Frame Type 5 support (on-demand requests)
- [ ] Configurable error codes
- [ ] Additional sensor data in reserved bytes
- [ ] Support for multiple Modbus slaves
- [ ] Variable-length Modbus data

### Extensibility
To add Frame Type 5:
1. Modify header structure (remove 1 byte)
2. Adjust Modbus data offset to byte 7
3. Update decoder to handle both frame types

---

## References

- **Vistron LoRa Mod Con:** Modbus-to-LoRaWAN concentrator
- **Trafag H72517o:** SF6 Gas Density Monitoring System
- **Modbus RTU Protocol:** IEC 61158 / RS-485
- **LoRaWAN:** LoRa Alliance specification v1.0.3

---

**Last Updated:** 2025-11-19  
**Firmware Version:** v1.51+  
**Format ID:** 4 (PAYLOAD_VISTRON_LORA_MOD_CON)
