# LoRaWAN Setup Guide - Vision Master E290

This guide explains how to configure the Vision Master E290 SF₆ Monitor for LoRaWAN connectivity.

## Hardware

The Vision Master E290 includes an **SX1262 LoRa transceiver** with the following pin configuration:

| Pin | GPIO | Function |
|-----|------|----------|
| NSS | 8 | Chip Select |
| DIO1 | 14 | Digital I/O 1 |
| RESET | 12 | Reset |
| BUSY | 13 | Busy indicator |

## Frequency Plans

The firmware supports all major LoRaWAN regions. Edit `src/main.cpp` line 94 to select your region:

```cpp
LoRaWANNode node(&radio, &EU868);  // Change to your region
```

**Available regions:**
- `EU868` - Europe (863-870 MHz)
- `US915` - North America (902-928 MHz)
- `AU915` - Australia (915-928 MHz)
- `AS923` - Asia-Pacific (920-923 MHz)
- `IN865` - India (865-867 MHz)
- `KR920` - South Korea (920-923 MHz)
- `CN470` - China (470-510 MHz)
- `CN779` - Europe (779-787 MHz)

## Configuration Steps

### 1. Register Device on LoRaWAN Network Server

#### Option A: The Things Network (TTN)

1. Go to https://console.thethingsnetwork.org/
2. Register/login to your account
3. Create an application (if not already created)
4. Click **"+ Register end device"**
5. Select:
   - **Activation mode**: Over the air activation (OTAA)
   - **LoRaWAN version**: LoRaWAN Specification 1.0.x or 1.1
   - **Input method**: Enter end device specifics manually
6. Fill in device information:
   - **JoinEUI (AppEUI)**: Generate or enter custom
   - **DevEUI**: Generate or enter custom (must be unique)
   - **AppKey**: Generate automatically
   - **NwkKey**: Generate automatically (for LoRaWAN 1.1)
7. Click **Register end device**

#### Option B: Chirpstack

1. Open your Chirpstack web interface
2. Navigate to **Applications** → Select your application
3. Click **"+ Add device"**
4. Configure:
   - **Device name**: SF6-Monitor-E290
   - **Device EUI**: Enter or generate
   - **Device profile**: Select appropriate profile
5. Go to **Keys (OTAA)** tab
6. Enter or generate:
   - **Application key (AppKey)**
   - **Network key (NwkKey)** if using LoRaWAN 1.1

### 2. Update Firmware with Your Credentials

Edit `src/main.cpp` (lines 83-88) and replace with your credentials:

```cpp
// LoRaWAN Credentials (OTAA)
uint64_t joinEUI = 0x0000000000000000;   // Replace with your JoinEUI/AppEUI (MSB)
uint64_t devEUI  = 0x70B3D57ED0012345;   // Replace with your DevEUI (MSB)
uint8_t appKey[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,  // Replace with your AppKey
                    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
uint8_t nwkKey[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,  // Replace with your NwkKey
                    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
```

**Important Notes:**
- All values are in **MSB (Most Significant Byte first)** format
- JoinEUI and DevEUI are 64-bit values (8 bytes)
- AppKey and NwkKey are 128-bit values (16 bytes)
- For LoRaWAN 1.0.x, NwkKey = AppKey

### 3. Configure Payload Decoder

#### TTN v3 Decoder

1. In your TTN application, go to your device
2. Navigate to **Payload formatters** → **Uplink**
3. Select **Formatter type**: Custom Javascript formatter
4. Copy and paste the `decodeUplink()` function from `lorawan_decoder.js`
5. Click **Save changes**

#### Chirpstack Decoder

1. In Chirpstack, go to **Device profiles**
2. Select your device profile
3. Navigate to **Codec** tab
4. Select **Codec**: Custom JavaScript codec functions
5. Copy and paste the `Decode()` function from `lorawan_decoder.js`
6. Click **Submit**

### 4. Build and Upload Firmware

```bash
# Build firmware
platformio run -e vision-master-e290-arduino

# Upload to device
platformio run -e vision-master-e290-arduino --target upload

# Monitor serial output
platformio device monitor -p /dev/ttyACM0 -b 115200
```

### 5. Verify Connection

Monitor the serial output for join status:

```
========================================
Initializing LoRaWAN...
========================================
Initializing SX1262 radio... success!
Initializing LoRaWAN node... success!
Starting OTAA join...
Join successful!
========================================
```

Check your network server console for:
- Join request received
- Join accept sent
- First uplink received

## Payload Format

The device sends a **10-byte payload** on **port 1** every **5 minutes**:

| Bytes | Field | Format | Scale | Unit | Range |
|-------|-------|--------|-------|------|-------|
| 0-1 | SF₆ Density | uint16 BE | ×0.01 | kg/m³ | 15.00-35.00 |
| 2-3 | SF₆ Pressure @20°C | uint16 BE | ×0.1 | kPa | 300.0-800.0 |
| 4-5 | SF₆ Temperature | uint16 BE | ×0.1 | K | 288.0-303.0 |
| 6-7 | SF₆ Pressure Variance | uint16 BE | ×0.1 | kPa | 300.0-800.0 |
| 8-9 | Modbus Request Counter | uint16 BE | 1 | count | 0-65535 |

**BE** = Big Endian (Most Significant Byte first)

### Example Payload

**Raw bytes:** `09 FA 15 7C 0B 72 15 7C 00 2A`

**Decoded:**
```json
{
  "sf6_density": 25.54,          // 0x09FA = 2554 → 25.54 kg/m³
  "sf6_pressure_20c": 550.0,     // 0x157C = 5500 → 550.0 kPa
  "sf6_temperature_k": 293.0,    // 0x0B72 = 2930 → 293.0 K
  "sf6_temperature_c": 19.85,    // 293.0 - 273.15 = 19.85 °C
  "sf6_pressure_var": 550.0,     // 0x157C = 5500 → 550.0 kPa
  "modbus_counter": 42           // 0x002A = 42
}
```

## Uplink Schedule

- **Frequency**: Every 5 minutes (300 seconds)
- **Port**: 1 (FPort 1)
- **Confirmed**: No (unconfirmed uplinks)
- **Adaptive Data Rate (ADR)**: Enabled by default in RadioLib

## Monitoring

### Web Interface

Access the web interface at http://192.168.4.1 when connected to WiFi AP:

**Home Page:**
- LoRaWAN Status: JOINED / NOT JOINED
- LoRa Uplinks: Total count

**Statistics Page:**
- Network Status
- Total Uplinks
- Total Downlinks
- Last RSSI (signal strength in dBm)
- Last SNR (signal-to-noise ratio in dB)

### E-Ink Display

Top right corner shows:
- **L: OK** - LoRaWAN joined and active
- **L: NO** - LoRaWAN not joined

### Serial Monitor

```
========================================
Sending LoRaWAN uplink...
Uplink successful!
RSSI: -45 dBm, SNR: 9.5 dB
Total uplinks: 12, downlinks: 0
========================================
```

## Troubleshooting

### Join Fails

**Symptoms:**
```
Join failed, code -2
Will retry in next cycle...
```

**Possible causes:**
1. **Wrong credentials** - Verify JoinEUI, DevEUI, AppKey, NwkKey
2. **Wrong frequency plan** - Check region setting matches your gateway
3. **Out of range** - Device too far from gateway
4. **Gateway offline** - Verify gateway is connected to network server
5. **Byte order** - Ensure keys are in MSB format

### No Uplinks Received

1. **Check join status** - Device must join before sending uplinks
2. **Check serial monitor** - Look for "Uplink successful!" messages
3. **Check network server logs** - Verify packets are received
4. **Check duty cycle** - EU868 has duty cycle restrictions
5. **Check spreading factor** - High SF may cause timeouts

### Poor Signal Quality

**Low RSSI (< -120 dBm) or Low SNR (< 0 dB):**
1. Move device closer to gateway
2. Check antenna connection
3. Use external antenna if available
4. Check for interference

## Advanced Configuration

### Change Uplink Interval

Edit `src/main.cpp` line 216:

```cpp
// Send LoRaWAN uplink every 5 minutes (if joined)
if (lorawan_joined && (now - last_lorawan_uplink >= 300000)) {  // Change 300000 to desired interval in ms
```

**Examples:**
- 1 minute: `60000`
- 10 minutes: `600000`
- 30 minutes: `1800000`
- 1 hour: `3600000`

**Note:** Respect local regulations and network fair use policies!

### Confirmed Uplinks

To enable confirmed uplinks (with acknowledgment), modify `sendLoRaWANUplink()`:

```cpp
// Change from:
int state = node.sendReceive(payload, sizeof(payload), 1);

// To:
int state = node.sendReceive(payload, sizeof(payload), 1, true);  // Last parameter = confirmed
```

### Add Downlink Handling

The device already receives downlinks. To process downlink data, add code after line 947 in `src/main.cpp`:

```cpp
node.getDownlink(downlink, downlinkSize);
lorawan_downlink_count++;

// Add your downlink processing here
// Example: Control relay based on downlink byte
if (downlinkSize > 0) {
    if (downlink[0] == 0x01) {
        // Turn something on
    } else if (downlink[0] == 0x00) {
        // Turn something off
    }
}
```

## Specifications

- **Radio**: SX1262 LoRa transceiver
- **Frequency**: Region-dependent (EU868, US915, etc.)
- **LoRaWAN Version**: 1.0.x / 1.1 compatible
- **Activation**: OTAA (Over-The-Air Activation)
- **Classes**: Class A (default)
- **Maximum Payload**: 51 bytes (EU868, SF12) to 242 bytes (US915, SF7)
- **Current Payload**: 10 bytes
- **Power Output**: +22 dBm max (configurable)
- **Sensitivity**: -148 dBm @ SF12 BW125

## Important Notes for RadioLib 7.x

### Success Return Codes
RadioLib 7.x uses special return codes for LoRaWAN operations:

**Join (activateOTAA):**
- `-1118` (`RADIOLIB_LORAWAN_NEW_SESSION`) = ✅ **Success** - New join
- `-1117` (`RADIOLIB_LORAWAN_SESSION_RESTORED`) = ✅ **Success** - Session restored
- Negative < -1118 = ❌ Error

**Uplink (sendReceive):**
- `0` = ✅ Success, no downlink
- `1` = ✅ Success, downlink in RX1 window
- `2` = ✅ Success, downlink in RX2 window
- Negative = ❌ Error

### Hardware Configuration
- **TCXO:** Disabled (Vision Master E290 uses crystal oscillator)
- **RF Switch:** DIO2 as RF switch enabled
- **Current Limit:** 140 mA

## Support

For issues or questions:
- RadioLib documentation: https://github.com/jgromes/RadioLib
- RadioLib status codes: https://jgromes.github.io/RadioLib/group__status__codes.html
- TTN Community Forum: https://www.thethingsnetwork.org/forum/
- Chirpstack documentation: https://www.chirpstack.io/docs/
