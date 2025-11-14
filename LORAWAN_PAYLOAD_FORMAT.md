# LoRaWAN Payload Format

## Overview

The firmware sends LoRaWAN uplinks using the **AdeunisModbusSf6** payload format, a simple wrapper around Modbus register data.

## Decoder Chain

The payload is decoded in two stages:

1. **AdeunisModbusSf6Decoder** - Validates length and strips header (first 2 bytes)
2. **TrafagH72517oModbusSf6Decoder** - Decodes the 4 Modbus registers into sensor values

## Payload Structure (10 bytes)

### Complete Frame

| Bytes | Field | Description |
|-------|-------|-------------|
| 0-1 | Header | Uplink counter (skipped by decoder, useful for tracking) |
| 2-3 | SF₆ Density | kg/m³ × 100 (big-endian uint16) |
| 4-5 | SF₆ Pressure @20°C | bar × 1000 (big-endian uint16) |
| 6-7 | SF₆ Temperature | Kelvin × 10 (big-endian uint16) |
| 8-9 | SF₆ Pressure Variance | bar × 1000 (big-endian uint16) |

### Decoder Behavior

**AdeunisModbusSf6Decoder:**
- Expects exactly 20 hex characters (10 bytes)
- Skips first 4 hex characters (bytes 0-1)
- Passes remaining 16 hex characters (bytes 2-9) to next decoder

**TrafagH72517oModbusSf6Decoder:**
- Receives 16 hex characters (8 bytes = 4 registers)
- Decodes each register with specific formula

### Register Encoding/Decoding

| Register | Bytes | Field | Modbus Storage | LoRa Encoding | Decoded Formula | Unit |
|----------|-------|-------|----------------|---------------|-----------------|------|
| 0 | 2-3 | Density | kg/m³ × 100 | kg/m³ × 100 | Value ÷ 100 | kg/m³ |
| 1 | 4-5 | Pressure @20°C | kPa × 10 | bar × 1000* | Value ÷ 1000 | bar |
| 2 | 6-7 | Temperature | K × 10 | K × 10 | (Value ÷ 10) - 273.15 | °C |
| 3 | 8-9 | Pressure Variance | kPa × 10 | bar × 1000* | Value ÷ 1000 | bar |

_*Note: kPa × 10 = bar × 1000 (since 1 bar = 100 kPa), so Modbus values are sent directly without conversion._

## Example Payload

```
Hex: 0001 09c4 157c 0b73 157c
     │    │    │    │    │
     │    │    │    │    └── Pressure Var: 0x157c = 5500 → 5.500 bar (550.0 kPa)
     │    │    │    └──────── Temperature: 0x0b73 = 2931 → 293.1 K → 19.95 °C
     │    │    └──────────── Pressure @20C: 0x157c = 5500 → 5.500 bar (550.0 kPa)
     │    └──────────────── Density: 0x09c4 = 2500 → 25.00 kg/m³
     └──────────────────── Header: uplink #1 (skipped by decoder)

Full hex string: 000109c4157c0b73157c
Total length: 20 characters (10 bytes)

Conversion verification:
- Modbus stores: 550 kPa × 10 = 5500
- Decoder expects: 5.5 bar × 1000 = 5500 ✓
- Decoded value: 5500 / 1000 = 5.5 bar = 550 kPa ✓
```

## LoRaWAN Port

All uplinks are sent on **port 1**.

## Decoder Validation

**AdeunisModbusSf6Decoder validates:**
- ✅ Exactly 20 hex characters (10 bytes)
- ✅ Valid hexadecimal string

**TrafagH72517oModbusSf6Decoder validates:**
- ✅ Exactly 16 hex characters (8 bytes)
- ✅ Valid hexadecimal string

## Serial Monitor Output

When an uplink is sent, the firmware prints:

```
========================================
Sending LoRaWAN uplink...
Payload (10 bytes): 000109c4157c0b73157c
Payload breakdown (AdeunisModbusSf6 format):
  Header (uplink #1) - bytes 0-1 skipped by decoder
  SF6 Density: 2500 (25.00 kg/m³)
  SF6 Pressure @20C: 5500 (5.500 bar = 550.0 kPa)
  SF6 Temperature: 2931 (20.0 °C)
  SF6 Pressure Var: 5500 (5.500 bar = 550.0 kPa)
Uplink successful!
RSSI: -45 dBm, SNR: 8.5 dB
========================================
```

## Testing

You can verify the payload encoding by:

1. **Check Serial Monitor** - View the hex payload before transmission
2. **Decode Manually** - Copy the hex string and test with the PHP decoders:
   ```php
   $payload = "000109c4157c0b73157c";
   $decoded = AdeunisModbusSf6Decoder::decode($payload, 1);
   // Expected output:
   // [
   //   'density' => 25.000,                      // 2500 ÷ 100
   //   'absolutePressureAt20Celsius' => 5.500,  // 5500 ÷ 1000
   //   'temperature' => 19.950,                  // (2931 ÷ 10) - 273.15
   //   'absolutePressure' => 5.500              // 5500 ÷ 1000
   // ]
   ```
3. **Verify Decoded Values** - Compare serial output with decoded sensor values

## Firmware Version

This payload format was implemented in **v1.28** (November 2025).

## Header Bytes Usage

The first 2 bytes are used for the uplink counter but **ignored by the decoder**. This is useful for:
- Tracking message sequence
- Detecting missed uplinks
- Debugging transmission issues

The decoder only processes bytes 2-9 (the actual Modbus register data).

## References

- Decoder: `AdeunisModbusSf6Decoder.php`
- Modbus Decoder: `TrafagH72517oModbusSf6Decoder.php`
- Firmware: `src/main.cpp` - `send_lorawan_uplink()`
