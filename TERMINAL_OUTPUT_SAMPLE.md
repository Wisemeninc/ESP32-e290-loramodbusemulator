# Sample Terminal Output - LoRa Profile Configuration

## Startup Sequence

When the ESP32 boots, the terminal will now display a comprehensive summary of all LoRa profile configurations.

## Example Output

### Single Profile (Auto-Rotation Disabled)

```
========================================
Vision Master E290 - Modbus RTU/TCP
========================================
Framework: Arduino
Display: 2.9" E-Ink (296x128)
========================================

Modbus Slave ID: 1
Modbus TCP: Disabled

>>> Loading authentication credentials...
Authentication: Enabled

>>> Loading SF6 base values...

>>> Initializing LoRaWAN with multi-profile support...

========================================
Initializing LoRaWAN...
========================================
Initializing LoRa radio on SPI bus...
  LoRa pins: SCK=9, MISO=11, MOSI=10, NSS=8
  LoRa control: DIO1=14, RESET=12, BUSY=13
Initializing SPI bus... done
Initializing SX1262... success
Configuring oscillator (crystal mode)... success
Configuring RF switch (DIO2)... success
Configuring current limit... success

Loading LoRaWAN profiles from NVS...
Profiles loaded successfully
Active profile: 0

--- Profile 0 Info ---
Name: Device-0-A1B2C3
DevEUI: 0x0000000000A1B2C3
JoinEUI: 0x0000000000000000
Profile: ENABLED
========================================

========================================
LoRa Profile Configuration Summary
========================================
Auto-Rotation: DISABLED
Enabled Profiles: 1 of 4
Active Profile: 0

Profile 0: Device-0-A1B2C3
  Status: ENABLED
  DevEUI: 0x0000000000A1B2C3
  JoinEUI: 0x0000000000000000
  AppKey: 0x0123456789ABCDEF... (16 bytes)
  NwkKey: 0x0123456789ABCDEF... (16 bytes)
  >>> CURRENTLY ACTIVE <<<

Profile 1: Device-1-A1B2C3
  Status: DISABLED
  DevEUI: 0x0100000000A1B2C3
  JoinEUI: 0x0000000000000000
  AppKey: 0x0123456789ABCDEF... (16 bytes)
  NwkKey: 0x0123456789ABCDEF... (16 bytes)

Profile 2: Device-2-A1B2C3
  Status: DISABLED
  DevEUI: 0x0200000000A1B2C3
  JoinEUI: 0x0000000000000000
  AppKey: 0x0123456789ABCDEF... (16 bytes)
  NwkKey: 0x0123456789ABCDEF... (16 bytes)

Profile 3: Device-3-A1B2C3
  Status: DISABLED
  DevEUI: 0x0300000000A1B2C3
  JoinEUI: 0x0000000000000000
  AppKey: 0x0123456789ABCDEF... (16 bytes)
  NwkKey: 0x0123456789ABCDEF... (16 bytes)

========================================

Checking for saved nonces...
Performing OTAA join...
Join request sent
Waiting for join accept...
Join successful!
```

### Multiple Profiles with Auto-Rotation Enabled

```
========================================
Vision Master E290 - Modbus RTU/TCP
========================================
Framework: Arduino
Display: 2.9" E-Ink (296x128)
========================================

Modbus Slave ID: 1
Modbus TCP: Disabled

>>> Loading authentication credentials...
Authentication: Enabled

>>> Loading SF6 base values...

>>> Initializing LoRaWAN with multi-profile support...

========================================
Initializing LoRaWAN...
========================================
Initializing LoRa radio on SPI bus...
  LoRa pins: SCK=9, MISO=11, MOSI=10, NSS=8
  LoRa control: DIO1=14, RESET=12, BUSY=13
Initializing SPI bus... done
Initializing SX1262... success
Configuring oscillator (crystal mode)... success
Configuring RF switch (DIO2)... success
Configuring current limit... success

Loading LoRaWAN profiles from NVS...
Profiles loaded successfully
Active profile: 0

--- Profile 0 Info ---
Name: Warehouse-Sensor-01
DevEUI: 0x0000000000A1B2C3
JoinEUI: 0x70B3D57ED0000001
Profile: ENABLED
========================================

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

Profile 1: Warehouse-Sensor-02
  Status: DISABLED
  DevEUI: 0x0100000000A1B2C3
  JoinEUI: 0x70B3D57ED0000001
  AppKey: 0123456789ABCDEF0123456789ABCDEF
  NwkKey: 0123456789ABCDEF0123456789ABCDEF

Profile 2: Factory-Gateway-A
  Status: ENABLED
  DevEUI: 0x0200000000A1B2C3
  JoinEUI: 0x70B3D57ED0000002
  AppKey: FEDCBA98765432100123456789ABCDEF
  NwkKey: FEDCBA98765432100123456789ABCDEF

Profile 3: Office-Monitor-B
  Status: ENABLED
  DevEUI: 0x0300000000A1B2C3
  JoinEUI: 0x70B3D57ED0000003
  AppKey: ABCDEF01234567890123456789ABCDEF
  NwkKey: ABCDEF01234567890123456789ABCDEF

Auto-Rotation Schedule (5 min per profile, 1 min stagger):
  Profile 0: T+0min, T+5min, T+10min...
  Profile 2: T+1min, T+6min, T+11min...
  Profile 3: T+2min, T+7min, T+12min...

========================================

Checking for saved nonces...
Performing OTAA join...
Join request sent
Waiting for join accept...
Join successful!
```

## Information Displayed

### Header Section
- Auto-Rotation status (ENABLED/DISABLED)
- Count of enabled profiles
- Currently active profile index

### Per-Profile Information
Each profile shows:
- **Profile Number** (0-3)
- **Name** - User-defined profile name
- **Status** - ENABLED or DISABLED
- **DevEUI** - Device EUI in hex format
- **JoinEUI** - Join EUI / AppEUI in hex format
- **AppKey** - First 8 bytes shown (16 bytes total)
- **NwkKey** - First 8 bytes shown (16 bytes total)
- **Active Indicator** - ">>> CURRENTLY ACTIVE <<<" for active profile

### Auto-Rotation Schedule (if enabled)
When auto-rotation is enabled with multiple profiles:
- Shows timing schedule for each enabled profile
- Indicates when each profile will send uplinks
- Format: T+0min, T+5min, T+10min (5-minute intervals)
- Shows 1-minute staggering between profiles

## Use Cases

### 1. Verification After Configuration
After configuring profiles via web UI, reboot and check terminal to verify:
- Correct DevEUI for each profile
- Proper enabled/disabled status
- Active profile selection
- Auto-rotation setting

### 2. Troubleshooting
When experiencing issues:
- Verify credentials match network server
- Check which profile is active
- Confirm auto-rotation state
- Identify disabled profiles

### 3. Documentation
Capture terminal output for:
- Device registration records
- Network configuration backup
- Support ticket information
- Deployment documentation

### 4. Multi-Device Testing
When emulating multiple devices:
- See all device identities at once
- Verify unique DevEUIs
- Check rotation schedule
- Confirm staggered timing

## Notes

- **Full Keys Displayed**: AppKey and NwkKey show all 32 hex characters (16 bytes) for ChirpStack configuration
- **ChirpStack Format**: Keys are shown without "0x" prefix - copy directly into ChirpStack
- **Timing**: Schedule assumes boot time as T+0
- **Disabled Profiles**: Still shown but marked as DISABLED
- **Active Indicator**: Only one profile can be active at a time
- **Auto-Rotation**: Only shows schedule if enabled AND 2+ profiles enabled

## Related Commands

To view this information after boot:
- Reboot device: Press reset button
- Monitor serial: 115200 baud
- Or navigate to web UI: `https://stationsdata.local/lorawan/profiles`

## Version
Added in firmware v1.49
