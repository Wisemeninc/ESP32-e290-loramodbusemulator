# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP-IDF project for the **Heltec Vision Master E290** (ESP32-S3R8) that implements a Modbus RTU slave device with:
- RS485 communication via HW-519 module
- 2.9" E-Ink display (296×128 pixels)
- LoRaWAN transmission capability (SX1262)
- WiFi configuration portal
- SF₆ gas sensor emulation

The device emulates an industrial sensor that exposes system metrics and simulated SF₆ gas data over Modbus RTU, displays data on E-Ink, and can transmit over LoRaWAN.

## Build System

### Build Commands

```bash
# Build for Vision Master E290
platformio run -e heltec_vision_master_e290

# Upload firmware
platformio run -e heltec_vision_master_e290 --target upload

# Monitor serial output (115200 baud)
platformio run -e heltec_vision_master_e290 --target monitor

# Combined upload and monitor
platformio run -e heltec_vision_master_e290 --target upload --target monitor

# Clean build
platformio run -e heltec_vision_master_e290 -t clean
```

### Build System Notes
- Uses **PlatformIO** with ESP-IDF framework
- Source directory is `main/` (not `src/`)
- CMake component registration in `main/CMakeLists.txt`
- Platform-specific configs in `sdkconfig.defaults`
- Board-specific overrides in `sdkconfig.heltec_vision_master_e290`

## Hardware Architecture

### Pin Assignments

**Critical: Avoid GPIO conflicts when modifying code**

| Peripheral | GPIO Pins | Notes |
|------------|-----------|-------|
| E-Ink Display | GPIO 1-6 | MOSI, SCK, CS, DC, RST, BUSY |
| E-Ink Power | GPIO 18, 46 | **BOTH must be HIGH** for display to work |
| RS485 (HW-519) | GPIO 43 (TX), 44 (RX) | UART1, no RTS needed (auto flow control) |
| SX1262 LoRa | GPIO 8-14 | NSS, SCK, MOSI, MISO, RST, BUSY, DIO1 |

**Pin Conflict Notes:**
- Original code used GPIO 17/18 for RS485 but GPIO 18 conflicts with E-Ink power
- Current code uses GPIO 43/44 per platformio.ini and sdkconfig.defaults
- **Never use GPIO 1-6, 18, or 46 for anything other than E-Ink**
- There is inconsistency in main.c comments (still references old pins) - main.c should be updated to match actual configuration

### Hardware Modules

1. **HW-519 RS485 Module**
   - Requires 5V power (not 3.3V)
   - Automatic flow control (no RTS pin needed)
   - 120Ω termination resistor (jumper configurable)

2. **E-Ink Display (DEPG0290BNS800)**
   - 296×128 pixels, black and white
   - SPI interface with manual CS control
   - Requires both power pins HIGH (GPIO 18 and 46)
   - Buffer size: 4736 bytes (296×128/8)
   - SPI max transfer: Must be >= 4736 + 32 bytes
   - Update modes: Full refresh (clear) and partial refresh (fast, may ghost)

3. **SX1262 LoRa Module**
   - Framework code present but not fully implemented
   - Requires RadioLib or similar LoRaMAC stack to be functional

## Code Architecture

### Main Components

```
main/
├── main.c              - Entry point, Modbus slave, WiFi AP, web server
├── lorawan.c           - LoRaWAN framework (stub implementation)
├── lorawan.h           - LoRaWAN interface
├── lorawan_config.h    - LoRaWAN credentials and config
├── display.c           - E-Ink display driver and rendering
├── display.h           - Display interface
├── eink_driver.c       - Low-level E-Ink SPI communication
├── eink_driver.h       - E-Ink driver interface
└── eink_pin_test.h     - Pin testing utilities
```

### Key Data Structures

**Holding Registers (12 registers, address 0-11):**
```c
typedef struct {
    uint16_t sequential_counter;    // R/W, increments on access
    uint16_t random_number;         // Updates every 5s
    uint16_t uptime_seconds;        // Low 16-bit of uptime
    uint16_t free_heap_kb_low;      // Free heap KB (low word)
    uint16_t free_heap_kb_high;     // Free heap KB (high word)
    uint16_t min_free_heap_kb;      // Minimum free heap
    uint16_t cpu_freq_mhz;          // CPU frequency
    uint16_t task_count;            // FreeRTOS task count
    uint16_t temperature_x10;       // Chip temp × 10
    uint16_t cpu_cores;             // Number of cores
    uint16_t wifi_ap_enabled;       // WiFi AP status
    uint16_t wifi_clients;          // Connected clients
} holding_reg_params_t;
```

**Input Registers (9 registers, address 0-8):**
```c
typedef struct {
    uint16_t sf6_density;           // kg/m³ × 100
    uint16_t sf6_pressure_20c;      // kPa × 10
    uint16_t sf6_temperature;       // K × 10
    uint16_t sf6_pressure_var;      // kPa × 10
    uint16_t slave_id;              // Device slave ID
    uint16_t serial_hi;             // Serial number high
    uint16_t serial_lo;             // Serial number low
    uint16_t sw_release;            // Version × 10
    uint16_t quartz_freq;           // Hz × 100
} input_reg_params_t;
```

### Task Architecture

The firmware runs multiple FreeRTOS tasks:

1. **Modbus Communication Task** - Handles RTU protocol on UART1
2. **SF₆ Sensor Emulation Task** - Updates input registers every 3s with realistic drift
3. **Random Number Update Task** - Updates holding register 1 every 5s
4. **Display Update Task** - Refreshes E-Ink screen every 30s
5. **LoRaWAN Task** - Transmits data every 60s (if enabled)
6. **WiFi AP Task** - Configuration portal (auto-disables after 20 minutes)

### Modbus Implementation

- Uses ESP-IDF `esp-modbus` component
- Function codes supported: 0x03 (read holding), 0x04 (read input), 0x06 (write single)
- Baud rate: 9600, 8-N-1
- Slave address: Configurable via WiFi portal (default 1, range 1-247)
- Register access handled via callbacks in main.c

### WiFi Configuration Portal

- AP active for 20 minutes after boot (or until WiFi client connects)
- SSID: `ESP32-Modbus-Config`, Password: `modbus123`
- Web UI at http://192.168.4.1
- **Authentication required:** Default credentials are admin/admin
- Six tabs: Statistics, Registers, Configuration, LoRaWAN, WiFi, Security
- Settings saved to NVS (non-volatile storage)
- Uses Arduino WebServer library
- WiFi client mode: Can connect to existing network, disables AP when connected
- Network scanning: Web interface can scan and connect to WiFi networks

### Display System

**E-Ink Update Flow:**
1. Power on both GPIO 18 and 46
2. Initialize SPI bus with large enough max_transfer_sz (>= 4768)
3. Reset display and wait for BUSY to go LOW
4. Render text to framebuffer (4736 bytes)
5. Send buffer in chunks (1024 bytes) with CS held LOW
6. Trigger refresh command
7. Wait for BUSY to indicate completion

**Known Issues:**
- If screen shows vertical/horizontal split, SPI buffer is too small or transfer is chunking incorrectly
- Both power pins (18 and 46) must be HIGH or display won't respond
- BUSY pin must transition properly or init failed

### LoRaWAN System

**Current State:** Framework/stub implementation
- Defines data structures and payload format
- Missing actual SX1262 driver integration
- Missing LoRaMAC stack

**To Complete:**
- Integrate RadioLib, Heltec's LoRaWAN library, or LoRaMac-node
- Implement OTAA join procedure
- Implement uplink/downlink handling
- Configure credentials in `lorawan_config.h`

**Payload Format (18 bytes):**
- Byte 0: Message type (0x03 = both register types)
- Bytes 1-10: Holding register subset (counter, uptime, heap, temp, WiFi)
- Bytes 11-18: Input register subset (SF₆ data)

## Development Workflow

### Testing Modbus Communication

```bash
# Using mbpoll (command line)
mbpoll -a 1 -0 -r 0 -c 12 -t 4 -b 9600 -P none -s 1 /dev/ttyUSB0

# Using Python pymodbus
from pymodbus.client.sync import ModbusSerialClient
client = ModbusSerialClient(method='rtu', port='/dev/ttyUSB0', baudrate=9600)
result = client.read_holding_registers(address=0, count=12, unit=1)
```

### Common Development Tasks

**Adding New Modbus Registers:**
1. Update struct in main.c (holding_reg_params_t or input_reg_params_t)
2. Update MB_REG_HOLDING_SIZE or MB_REG_INPUT_SIZE
3. Update descriptor array (holding_reg_params or input_reg_params)
4. Update web UI HTML in main.c (if register should be visible)
5. Update display.c if register should appear on E-Ink

**Changing UART Pins:**
1. Update platformio.ini build_flags (CONFIG_MB_UART_TXD/RXD)
2. Update sdkconfig.defaults (CONFIG_MB_UART_TXD/RXD)
3. Verify no conflicts with E-Ink (GPIO 1-6, 18, 46) or LoRa (GPIO 8-14)

**Modifying Display Layout:**
1. Edit `display_update_registers()` in display.c
2. Update `render_text_display()` for ASCII preview
3. Test with full refresh first, then partial refresh

### Troubleshooting Workflow

**No Modbus Communication:**
1. Check monitor output for "Modbus slave alive" messages
2. Verify wiring (especially 5V to HW-519, not 3.3V)
3. Check UART pins match sdkconfig.defaults (GPIO 43/44)
4. Try swapping RS485 A/B wires
5. Use mbpoll with explicit parameters including `-s 1` (stop bits)

**E-Ink Display Not Working:**
1. Verify both power pins HIGH (GPIO 18 and 46)
2. Check BUSY pin transitions in logs
3. Verify SPI max_transfer_sz >= 4768 in display.c
4. Check for SPI errors in logs
5. Reduce SPI clock from 6 MHz to 4 MHz if signal integrity issues

**Build Failures:**
1. Clean build: `platformio run -e heltec_vision_master_e290 -t clean`
2. Delete `.pio/` directory
3. Check ESP-IDF version compatibility

## Configuration Files

- `platformio.ini` - Build environment, pins, board settings
- `sdkconfig.defaults` - ESP-IDF Kconfig defaults (Modbus, UART, PSRAM)
- `sdkconfig.heltec_vision_master_e290` - Board-specific overrides (if present)
- `main/lorawan_config.h` - LoRaWAN credentials and region
- `main/display.h` - Display enable/disable, pin definitions

## Security Considerations

**Current Implementation (v1.22):**
- ✅ HTTP Basic Authentication for web interface (default: admin/admin)
- ✅ Configurable credentials stored in NVS
- ⚠️ Hard-coded WiFi AP credentials (modbus123)
- ⚠️ No HTTPS (credentials transmitted in Base64)
- ⚠️ Modbus RTU has no built-in security

**Security Features Implemented:**
- Web interface requires username/password authentication
- All endpoints protected by `check_authentication()` function
- Credentials changeable via /security page
- Authentication can be disabled for trusted networks
- Credentials persist across reboots (NVS storage)

**For Production:**
- Enable ESP32-S3 secure boot and flash encryption
- Generate unique per-device WiFi passwords
- Change default web credentials (admin/admin) immediately
- Add HTTPS with self-signed certificate
- Use physical security for RS485 bus
- Add rate limiting on authentication attempts
- Implement CSRF protection

## Important Notes

1. **Pin Inconsistency:** main.c comments reference GPIO 17/18 for RS485 but actual config uses GPIO 43/44. Always trust platformio.ini and sdkconfig.defaults.

2. **E-Ink Power Critical:** Both GPIO 18 and 46 must be set HIGH before initializing display. Missing GPIO 46 is a common failure mode.

3. **SPI Buffer Size:** E-Ink requires full framebuffer (4736 bytes) to be transferred. Set SPI max_transfer_sz appropriately.

4. **HW-519 Auto Flow Control:** No RTS pin needed. If adding RTS pin to config, it will break communication.

5. **LoRaWAN Incomplete:** Framework exists but requires integration with actual LoRaMAC stack to function.

6. **SF₆ Emulation:** Input registers update every 3s with realistic drift. Disable by commenting out task creation if static values needed.

7. **WiFi AP Timeout:** Portal automatically disables after 20 minutes to save power. Restart device to re-enable.

8. **Register Endianness:** 32-bit values (heap, serial number) split across two 16-bit registers. High word in higher address.
- Dont use emojis in the code