# LoRaWAN Dynamic Credentials

## Overview

The Vision Master E290 now automatically generates unique LoRaWAN credentials on first boot and stores them persistently in Non-Volatile Storage (NVS). These credentials can be viewed and edited through both the serial console and web interface.

## Features

### Automatic Credential Generation
- **First Boot:** Device generates unique credentials automatically
- **DevEUI:** Based on ESP32 MAC address + random bytes for guaranteed uniqueness
- **JoinEUI:** Randomly generated 64-bit identifier
- **AppKey/NwkKey:** Randomly generated 128-bit keys

### Persistent Storage
- Credentials saved to ESP32 NVS (flash memory)
- Survives reboots and power cycles
- Separate namespace from Modbus configuration

### Serial Console Output
On every boot, credentials are displayed in the serial console:

```
========================================
LoRaWAN Device Credentials
========================================
JoinEUI (AppEUI): 0x1A2B3C4D5E6F7890
DevEUI:           0xABCDEF0123456789
AppKey:           0102030405060708090A0B0C0D0E0F10
NwkKey:           0102030405060708090A0B0C0D0E0F10
========================================
Copy these credentials to your network server:
  - The Things Network (TTN)
  - Chirpstack
  - AWS IoT Core for LoRaWAN
========================================
```

### Web Interface
Access credentials at: **http://192.168.4.1/lorawan**

**Features:**
- View current credentials in formatted table
- Copy-paste friendly hexadecimal format
- Edit all four credential fields
- Input validation (correct hex length)
- Warning before changes
- Automatic device restart after update

## Usage

### First Time Setup

1. **Boot the device** - Credentials are automatically generated
2. **Connect to WiFi AP:**
   - SSID: `ESP32-Modbus-Config`
   - Password: `modbus123`
3. **Open web browser** to http://192.168.4.1/lorawan
4. **Copy credentials** to your network server (TTN, Chirpstack, etc.)
5. **Register device** in network server with copied credentials
6. **Device automatically joins** on next LoRaWAN cycle

### Viewing Credentials

**Serial Console:**
- Credentials printed on every boot
- Displayed before LoRaWAN initialization

**Web Interface:**
- Navigate to http://192.168.4.1/lorawan
- Credentials shown in formatted table
- Includes JoinEUI, DevEUI, AppKey, NwkKey

### Changing Credentials

**Via Web Interface:**
1. Navigate to http://192.168.4.1/lorawan
2. Scroll to "Update Credentials" section
3. Edit any of the four fields:
   - JoinEUI: 16 hexadecimal characters (8 bytes)
   - DevEUI: 16 hexadecimal characters (8 bytes)
   - AppKey: 32 hexadecimal characters (16 bytes)
   - NwkKey: 32 hexadecimal characters (16 bytes)
4. Click "Save & Restart"
5. Device saves to NVS and restarts automatically
6. Update credentials in your network server
7. Device will rejoin with new credentials

**Important:** Changing credentials requires re-registering the device in your network server. The device will lose its current session.

## Technical Details

### Storage Location
- **NVS Namespace:** `lorawan`
- **Keys:**
  - `has_creds` (bool) - Flag indicating credentials exist
  - `joinEUI` (uint64) - 64-bit JoinEUI
  - `devEUI` (uint64) - 64-bit DevEUI
  - `appKey` (bytes[16]) - 128-bit AppKey
  - `nwkKey` (bytes[16]) - 128-bit NwkKey

### DevEUI Generation Algorithm
```cpp
DevEUI = MAC[0-3] (4 bytes) + Random (4 bytes)
```
This ensures:
- Each device has a unique DevEUI
- DevEUI is traceable to hardware MAC address
- Additional randomness prevents collisions

### JoinEUI Generation
- Fully random 64-bit value
- Can be customized per deployment
- Suitable for private LoRaWAN networks

### Key Generation
- Uses ESP32 hardware RNG
- 128-bit security for both AppKey and NwkKey
- LoRaWAN 1.0.x compatible (NwkKey = AppKey)

### API Functions

#### `generateLoRaWANCredentials()`
Generates new random credentials using ESP32 MAC and RNG.

#### `loadLoRaWANCredentials()`
Loads credentials from NVS, or generates new ones if none exist.

#### `saveLoRaWANCredentials()`
Saves current credentials to NVS flash storage.

#### `printLoRaWANCredentials()`
Prints credentials to serial console in formatted output.

## Network Server Configuration

### The Things Network (TTN)

1. Create application (if not exists)
2. Click "Register end device"
3. Select:
   - **Activation mode:** OTAA
   - **LoRaWAN version:** MAC V1.0.3
   - **Input method:** Enter end device specifics manually
4. Enter credentials from device:
   - **JoinEUI (AppEUI):** Copy from device
   - **DevEUI:** Copy from device
   - **AppKey:** Copy from device
5. Click "Register end device"
6. Device will join automatically

### Chirpstack

1. Navigate to Applications → Your Application
2. Click "Add device"
3. Fill in:
   - **Device name:** SF6-Monitor-E290
   - **Device EUI:** Copy from device
   - **Device profile:** Select appropriate profile
4. Go to "Keys (OTAA)" tab
5. Enter:
   - **Application key:** Copy AppKey from device
   - **Network key:** Copy NwkKey from device (for LoRaWAN 1.1)
6. Save
7. Device will join automatically

### AWS IoT Core for LoRaWAN

1. Navigate to IoT Core → LoRaWAN → Devices
2. Click "Add wireless device"
3. Select "OTAA v1.0.x"
4. Enter:
   - **DevEUI:** Copy from device
   - **AppKey:** Copy from device
5. Create destination for device
6. Device will join automatically

## Security Considerations

### Credential Strength
- **Random generation:** Uses ESP32 hardware RNG
- **Key length:** 128-bit AppKey/NwkKey meets LoRaWAN security requirements
- **DevEUI uniqueness:** MAC-based ensures no collisions

### Storage Security
- **NVS encryption:** Enable ESP32 NVS encryption for production
- **Flash encryption:** Enable ESP32 flash encryption for maximum security
- **Access control:** Credentials only accessible via physical WiFi AP connection

### Best Practices
1. **Enable flash encryption** in production builds
2. **Disable WiFi AP** after initial configuration if not needed
3. **Use LoRaWAN 1.1** with separate NwkKey for enhanced security
4. **Rotate credentials** periodically for high-security deployments
5. **Log credential access** for audit trails

## Troubleshooting

### Credentials Not Generating
**Symptoms:** Device starts with all zeros (0x0000...)

**Solutions:**
1. Check NVS partition is not corrupted
2. Erase NVS: `esptool.py erase_flash`
3. Reflash firmware
4. Device will generate new credentials on next boot

### Cannot Save Credentials via Web
**Symptoms:** "Failed to open LoRaWAN preferences" error

**Solutions:**
1. Check NVS partition has space
2. Verify preferences namespace not corrupted
3. Try erasing and reflashing
4. Check serial console for detailed errors

### Invalid Hex Format Error
**Symptoms:** Web interface rejects credential update

**Causes:**
- Wrong length (JoinEUI/DevEUI must be 16 hex chars, keys must be 32)
- Non-hexadecimal characters (0-9, A-F only)
- Missing leading zeros

**Solution:** Ensure format matches:
- JoinEUI: `0123456789ABCDEF` (16 chars)
- DevEUI: `FEDCBA9876543210` (16 chars)
- AppKey: `0123456789ABCDEF0123456789ABCDEF` (32 chars)
- NwkKey: `0123456789ABCDEF0123456789ABCDEF` (32 chars)

### Device Won't Join After Changing Credentials
**Causes:**
1. Credentials not updated in network server
2. Device address already registered with old credentials
3. Network server nonce mismatch

**Solutions:**
1. Update credentials in network server
2. Clear device nonces in Chirpstack/TTN
3. Wait for device to retry join (happens every 5 minutes)
4. Check serial console for join error codes

## Example Workflow

### New Device Deployment

1. **Power on device** (first boot)
   ```
   ✓ Generated unique credentials
   ✓ Credentials saved to non-volatile storage
   ```

2. **Connect to WiFi AP**
   - SSID: ESP32-Modbus-Config
   - Password: modbus123

3. **Access web interface**
   - http://192.168.4.1/lorawan

4. **Copy credentials**
   ```
   JoinEUI: A1B2C3D4E5F6G7H8
   DevEUI:  1122334455667788
   AppKey:  0102030405060708090A0B0C0D0E0F10
   NwkKey:  0102030405060708090A0B0C0D0E0F10
   ```

5. **Register in Chirpstack**
   - Create device with DevEUI
   - Set AppKey and NwkKey

6. **Wait for join**
   - Device attempts join every 5 minutes
   - Check serial console: "✓ Join successful!"
   - Check web interface: LoRaWAN Status = JOINED

7. **Verify uplinks**
   - First uplink 5 minutes after join
   - Check Chirpstack for received data
   - Verify payload decodes correctly

## References

- [LoRaWAN Specification](https://lora-alliance.org/resource_hub/lorawan-specification-v1-0-3/)
- [ESP32 NVS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
- [The Things Network OTAA Guide](https://www.thethingsnetwork.org/docs/devices/registration/)
- [Chirpstack Device Management](https://www.chirpstack.io/docs/chirpstack/use/devices.html)
