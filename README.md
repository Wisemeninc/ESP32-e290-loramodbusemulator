# Vision Master E290 Modbus RTU Slave with HW-519

This project implements a full-featured Modbus RTU slave on the **Heltec Vision Master E290** (ESP32-S3R8 with E-Ink display and LoRa) using the HW-519 RS485 module. Perfect for testing SCADA systems, HMI applications, or learning Modbus protocol.

**Framework:** Arduino (via PlatformIO)
**Platform:** Espressif32 (ESP32-S3)
**Current Version:** v1.42

**Key Highlights:**
- 🔧 **12 Holding Registers** with ESP32 system metrics (CPU, memory, WiFi status)
- 🎯 **9 Input Registers** with realistic SF₆ gas sensor emulation
- 📊 **Dynamic sensor simulation** with graph-friendly realistic drift and noise
- 🌐 **WiFi configuration portal** with live register monitoring
- 📡 **HW-519 RS485 module** with automatic flow control (no RTS pin needed)
- 💾 **Persistent configuration** stored in NVS
- 📶 **LoRaWAN support** to transmit Modbus data wirelessly over long range
- 📺 **2.9" E-Ink display** shows real-time register data and LoRaWAN status

Case: 
https://www.printables.com/model/974647-vision-master-e290-v031-case-for-meshtastic/files

Boards: 
https://www.tinytronics.nl/en/development-boards/microcontroller-boards/with-lora/heltec-vision-master-e290-esp32-s3-sx1262-lora-868mhz-with-2.9-inch-e-paper-display

https://www.amazon.de/-/en/ESP32-S3R8-Development-Compatible-Micpython-Meshtastic/dp/B0DRG5J5R5/ref=sr_1_1?crid=3U044HWSXMQ6A&dib=eyJ2IjoiMSJ9.YoHIFAF5SYU13wPMcI3z0o5FrWHRIAIURS_jXTnFTpBuP7UZve6GrgROBM5S9etb5bfGYUtu4rsEZLOdscTTNFs-IM4bNr3DjsYnKTtFmZWUq04eISMxsWONlUl04XZIYLjN-8t6Mk9vyMhSIrrhs19dqwEzApiuOnNlDGlJxTK2T0EpJVAQXFbpsOOBfXQj8S9x_1pfjDWp9Fd8jnI7yx7xQJ9tAMjJyrarr9p0xx4.v2UVvtgnnTRFwIt6zFcIBJht59kzyWvEq3QsAww-LrU&dib_tag=se&keywords=vision+master+e290&qid=1762809245&sprefix=vision+master+e290%2Caps%2C124&sr=8-1

## Features

### Modbus RTU Features
- **Modbus RTU Slave** with configurable address (1-247)
- **Twelve Holding Registers (0-11):** System status and metrics
  - Register 0: Sequential counter (increments on each access)
  - Register 1: Random number (updated every 5 seconds)
  - Register 2: System uptime in seconds (low 16-bit)
  - Register 3-4: Free heap memory in KB (32-bit value)
  - Register 5: Minimum free heap since boot (KB)
  - Register 6: CPU frequency (MHz)
  - Register 7: Number of active FreeRTOS tasks
  - Register 8: Chip temperature × 10 (e.g., 235 = 23.5°C)
  - Register 9: Number of CPU cores
  - Register 10: WiFi AP enabled (1=active, 0=disabled)
  - Register 11: Number of connected WiFi clients
- **Nine Input Registers (0-8):** SF₆ Gas Sensor Emulation
  - Register 0: SF₆ gas density (kg/m³)
  - Register 1: SF₆ gas pressure @20°C (kPa)
  - Register 2: SF₆ gas temperature (K)
  - Register 3: SF₆ gas pressure variance (kPa)
  - Register 4: Slave ID
  - Register 5-6: Serial number (32-bit)
  - Register 7: Software release version
  - Register 8: Quartz frequency (Hz)
  - **Dynamic emulation** with realistic sensor drift and noise
  - Values update every 3 seconds within realistic ranges
  - **Manual control** via web interface with persistent storage (NVS)
- **HW-519 RS485 module** with automatic flow control
- **WiFi Access Point** for configuration (active for 20 minutes after boot)
  - SSID: `ESP32-Modbus-Config`
  - Password: `modbus123`
  - Configure Modbus slave ID via web interface (no restart required)
  - View real-time statistics (requests, uptime, etc.)
  - View and monitor all holding and input registers in real-time
  - Persistent configuration stored in NVS (Non-Volatile Storage)

### LoRaWAN Features
- **LoRaWAN OTAA** (Over-The-Air Activation) - fully implemented
- **Periodic transmission** of Modbus register data
- **RadioLib integration** with SX1262 transceiver
- **Region support:** EU868 (configurable in src/main.cpp)
- **Efficient payload encoding** (18 bytes for key registers)
- **Session persistence** with NVS storage
- **Uplink and downlink** handling
- **Join status monitoring** on E-Ink display

**Note:** LoRaWAN is fully implemented using [RadioLib](https://github.com/jgromes/RadioLib). Credentials are auto-generated on first boot and can be viewed/modified via the web interface. Default region is EU868.

### E-Ink Display Features
- **2.9" E-Paper display** (296×128 pixels, black and white)
- **Fully functional graphics** using Heltec E-Ink library
- **Real-time monitoring** of Modbus registers and LoRaWAN status
- **Automatic updates** every 30 seconds
- **Configurable rotation** (portrait/landscape via DISPLAY_ROTATION define)
- **Low power** - E-Ink retains image without power
- **Display shows:**
  - LoRaWAN status (JOINED/JOINING/DISABLED)
  - LoRaWAN uplink counter
  - System metrics (uptime, temperature, RAM, CPU)
  - SF₆ sensor data (density, pressure, temperature)

**Note:** The display is fully implemented using the [Heltec E-Ink Library](https://github.com/todd-herbert/heltec-eink-modules) with custom bitmap font rendering. Display rotation can be configured in src/main.cpp (DISPLAY_ROTATION define).

## Hardware Requirements

- **Heltec Vision Master E290** (ESP32-S3R8 with 8MB Flash + 8MB PSRAM, E-Ink display, LoRa module)
- HW-519 RS485 module (automatic flow control)
- RS485 bus or USB-to-RS485 adapter for testing

### Vision Master E290 Features
- ESP32-S3R8 MCU with WiFi & Bluetooth
- 8MB Flash + 8MB OPI PSRAM
- 2.9" E-Ink display (296×128 pixels)
- SX1262 LoRa module
- Raspberry Pi compatible GPIO headers (2×20 pins)
- USB Type-C for programming and power

## Pin Configuration for Vision Master E290

The HW-519 module has **automatic flow control** and only requires TX and RX connections. The pins are configured to avoid conflicts with the E290's E-Ink display (GPIO1-6) and other peripherals:

| Function | GPIO Pin | HW-519 Pin | Notes                                    |
|----------|----------|------------|------------------------------------------|
| TX       | GPIO 43  | TXD        | UART1 transmit                           |
| RX       | GPIO 44  | RXD        | UART1 receive                            |
| GND      | GND      | GND        | Common ground                            |
| Power    | 5V       | VCC        | HW-519 needs 5V power                    |

**Note:**
- Unlike MAX485 modules, the HW-519 does **not** require an RTS pin for flow control. The module automatically switches between transmit and receive modes.
- GPIO43 and GPIO44 were chosen to avoid conflicts with E-Ink display power (GPIO18, GPIO46)
- These pins do not conflict with the E-Ink display (GPIO1-6), LoRa module, or display power pins

## Wiring Diagram

```
Vision Master E290    HW-519          RS485 Bus
------------------    ------          ---------
GPIO43 ------------> TXD
GPIO44 <------------ RXD
5V     ------------> VCC
GND    ------------- GND ----------- GND
                     A   ----------- A (Data+)
                     B   ----------- B (Data-)
```

**Accessing GPIO43 and GPIO44 on Vision Master E290:**
- GPIO43 and GPIO44 are available as U0TXD and U0RXD (USB serial pins)
- These pins are commonly used for UART communication on ESP32-S3
- They do not conflict with the E-Ink display power pins (GPIO18, GPIO46)

**HW-519 Features:**
- Automatic transmit/receive switching (no flow control pin needed)
- Built-in 120Ω termination resistor (check jumper setting)
- Protection circuits included
- 3.3V logic compatible with 5V power supply

**Vision Master E290 Notes:**
- The E290's 2.9" E-Ink display shows real-time Modbus register data
- The LoRa module transmits data over LoRaWAN (framework ready for full stack)
- The 8MB PSRAM provides plenty of memory for display buffers and future features
- WiFi AP configuration portal works as usual - connect to "ESP32-Modbus-Config"
- The board's USB Type-C port handles both programming and serial monitoring

## E-Ink Display Configuration

### Display Pin Mapping

The Vision Master E290 uses these GPIO pins for the 2.9" E-Ink display:

| Function | GPIO Pin | Description |
|----------|----------|-------------|
| MOSI     | GPIO 1   | SPI MOSI (data) |
| SCK      | GPIO 2   | SPI Clock |
| CS       | GPIO 3   | Chip Select |
| DC       | GPIO 4   | Data/Command |
| RST      | GPIO 5   | Reset |
| BUSY     | GPIO 6   | Busy indicator |

**Note:** These pins are pre-configured by the Heltec E-Ink library for Vision Master E290.

### Display Layout

The display shows Modbus register data in a structured layout:

```
┌─────────────────────────────────┐
│ Vision Master E290              │  Title
│ LoRaWAN: JOINED     TX: 123    │  Status line
├─────────────────────────────────┤
│ SYSTEM                          │  System section
│  Uptime: 12345s  Temp: 25.3°C  │    ESP32 metrics
│  RAM: 234KB  CPU: 240MHz       │    from holding regs
├─────────────────────────────────┤
│ SF6 SENSOR                      │  Sensor section
│  Density: 25.50 kg/m³           │    SF₆ data from
│  Pressure: 550.0 kPa            │    input registers
│  Temp: 293.0 K (19.9°C)         │
└─────────────────────────────────┘
```

### Viewing Display Output

The display system renders graphics directly to the E-Ink screen every 30 seconds. To monitor system output via serial console:

```bash
# Monitor serial output to see system status
platformio run -e vision-master-e290-arduino --target monitor
```

### Display Update Frequency

The display updates every **30 seconds** to conserve E-Ink refresh cycles. This can be adjusted in `src/main.cpp`:

```cpp
// In loop() function - search for "Display update (every 30 seconds)"
if (now - last_display_update >= 30000) {  // Change 30000 to desired ms
    last_display_update = now;
    update_display();
}
```

### Display Rotation

The display orientation can be configured in `src/main.cpp`:

```cpp
#define DISPLAY_ROTATION 3  // 0=Portrait, 1=Landscape, 2=Portrait inverted, 3=Landscape inverted
```

## LoRaWAN Configuration

### Getting LoRaWAN Credentials

The Vision Master E290 **automatically generates unique LoRaWAN credentials on first boot**:

- **DevEUI** - Generated from ESP32 MAC address (ensures uniqueness)
- **JoinEUI (AppEUI)** - Randomly generated 8 bytes
- **AppKey** - Randomly generated 16 bytes
- **NwkKey** - Copy of AppKey (for LoRaWAN 1.0.x compatibility)

**On first boot, the credentials are:**
1. Automatically generated using hardware RNG
2. Saved to NVS (Non-Volatile Storage)
3. Printed to serial console
4. Displayed in the web interface (LoRaWAN tab)

**To register your device:**
1. Boot the device and check serial console for credentials, or
2. Connect to web interface at https://stationsdata.local
3. Navigate to **LoRaWAN tab** to view credentials
4. Copy credentials to your LoRaWAN network server:
   - [The Things Network (TTN)](https://www.thethingsnetwork.org/) - Free, community-run
   - [Chirpstack](https://www.chirpstack.io/) - Self-hosted option
   - Commercial providers (AWS IoT, Helium, etc.)

### Configuring LoRaWAN

**Credentials are automatically generated on first boot** - no manual configuration needed!

**To view or change credentials:**
1. Access web interface at https://stationsdata.local
2. Navigate to **LoRaWAN tab**
3. View current credentials
4. (Optional) Update credentials if needed

**To change LoRaWAN region:**

Edit `src/main.cpp` (line ~3089) to change from EU868 to another region:

```cpp
// Current: EU868 (default)
LoRaWANNode node(&radio, &EU868);

// Change to US915:
LoRaWANNode node(&radio, &US915);

// Other options: AS923, AU915, IN865, etc.
```

**To change transmission interval:**

Edit `src/main.cpp` in the `loop()` function (line ~365):

```cpp
// Send LoRaWAN uplink every 5 minutes (if joined)
if (lorawan_joined && (now - last_lorawan_uplink >= 300000)) {  // 300000ms = 5 minutes
    last_lorawan_uplink = now;
    send_lorawan_uplink();
}

// Change to 1 minute (60000ms) or 10 minutes (600000ms) as needed
```

### SX1262 Pin Configuration

The Vision Master E290 uses these GPIO pins for the SX1262 LoRa module:

| Function | GPIO Pin | Description |
|----------|----------|-------------|
| NSS (CS) | GPIO 8   | SPI Chip Select |
| SCK      | GPIO 9   | SPI Clock |
| MOSI     | GPIO 10  | SPI MOSI |
| MISO     | GPIO 11  | SPI MISO |
| RST      | GPIO 12  | Reset |
| BUSY     | GPIO 13  | Busy indicator |
| DIO1     | GPIO 14  | IRQ interrupt |

**Note:** These pins are pre-configured in `src/main.cpp` for the Vision Master E290.

### LoRaWAN Payload Format

The device sends Modbus data in a compact binary format:

**Payload Format (18 bytes total):**
```
Byte  0:     Message Type (0x03 = Both registers)
Byte  1-2:   Sequential Counter (uint16_t)
Byte  3-4:   Uptime (seconds, uint16_t)
Byte  5-6:   Free Heap (KB, uint16_t)
Byte  7-8:   Temperature × 10 (uint16_t)
Byte  9:     WiFi Enabled (0/1)
Byte 10:     WiFi Clients (uint8_t)
Byte 11-12:  SF₆ Density (×100, uint16_t)
Byte 13-14:  SF₆ Pressure @20°C (×10, uint16_t)
Byte 15-16:  SF₆ Temperature (×10, uint16_t)
Byte 17-18:  SF₆ Pressure Variance (×10, uint16_t)
```

### LoRaWAN Implementation Details

The project uses **RadioLib** (v7.4.0+) for full LoRaWAN functionality:

- **OTAA Join:** Automatic Over-The-Air Activation with credential management
- **Session Persistence:** Join sessions saved to NVS, survives reboots
- **Uplink/Downlink:** Both supported with proper MAC command handling
- **Region Support:** Configured for EU868, can be changed to US915, AS923, AU915, etc.
- **Status Display:** Join status and uplink counter shown on E-Ink display

The LoRaWAN stack is fully operational - just configure your credentials in `src/main.cpp`.

### Decoding LoRaWAN Payloads

On your LoRaWAN server (TTN, Chirpstack), use this decoder:

```javascript
function decodeUplink(input) {
  var data = {};
  var bytes = input.bytes;

  if (bytes[0] === 0x03) { // Both registers
    data.counter = (bytes[1] << 8) | bytes[2];
    data.uptime_sec = (bytes[3] << 8) | bytes[4];
    data.free_heap_kb = (bytes[5] << 8) | bytes[6];
    data.temperature_c = ((bytes[7] << 8) | bytes[8]) / 10.0;
    data.wifi_enabled = bytes[9];
    data.wifi_clients = bytes[10];
    data.sf6_density_kgm3 = ((bytes[11] << 8) | bytes[12]) / 100.0;
    data.sf6_pressure_kpa = ((bytes[13] << 8) | bytes[14]) / 10.0;
    data.sf6_temperature_k = ((bytes[15] << 8) | bytes[16]) / 10.0;
    data.sf6_pressure_var_kpa = ((bytes[17] << 8) | bytes[18]) / 10.0;
  }

  return {
    data: data,
    warnings: [],
    errors: []
  };
}
```

## Modbus Configuration

- **Slave Address:** Configurable via web interface (default: 1)
- **Baud Rate:** 9600
- **Data Bits:** 8
- **Parity:** None
- **Stop Bits:** 1
- **Mode:** RTU (RS485)

## WiFi Configuration Portal

On boot, the device creates a WiFi Access Point for 20 minutes:

- **SSID:** `ESP32-Modbus-Config`
- **Password:** `modbus123`
- **Web Interface (AP Mode):** https://192.168.4.1
- **Web Interface (Client Mode):** https://stationsdata.local
- **Authentication:** Username: `admin`, Password: `admin` (change immediately!)

**Note:** Your browser will show a security warning due to the self-signed certificate. This is expected - click "Advanced" and proceed to accept the certificate.

### mDNS Support

The device supports **mDNS (Multicast DNS)** for easy access without needing to know the IP address:

- **Hostname:** `stationsdata.local`
- **Access in AP Mode:** Use https://192.168.4.1 or https://stationsdata.local
- **Access in Client Mode:** Use https://stationsdata.local (when connected to your WiFi network)

**Benefits:**
- No need to find the device's IP address after connecting to your network
- Consistent URL regardless of DHCP assignment
- Works on all major operating systems (Windows, macOS, Linux)

**Requirements:**
- Windows: Install [Bonjour Print Services](https://support.apple.com/kb/DL999) if mDNS doesn't work
- macOS/iOS: mDNS works natively (built-in)
- Linux: Install `avahi-daemon` package (usually pre-installed)
- Android: mDNS support varies by device/browser

### Web Interface Features

The web interface is organized into multiple tabs with HTTP Basic Authentication protection:

1. **Statistics Tab:**
   - Total Modbus requests
   - Read/Write request counts
   - Error count
   - System uptime
   - Current slave ID

2. **Registers Tab:**
   - Real-time view of all 12 holding registers and 9 input registers
   - Holding registers: values in decimal and hexadecimal
   - Input registers: raw values and scaled (engineering units)
   - **SF6 Manual Control Panel:**
     - Set custom SF₆ density (0-60 kg/m³)
     - Set custom SF₆ pressure (0-1100 kPa)
     - Set custom SF₆ temperature (215-360 K)
     - Reset to defaults button
     - Values persist to flash storage (NVS)
     - Client-side validation
   - Auto-refreshes every 5 seconds
   - Complete register descriptions

3. **Configuration Tab:**
   - Change Modbus slave ID (1-247) without device restart
   - Changes are saved to non-volatile storage
   - Changes take effect immediately

4. **LoRaWAN Tab:**
   - View and configure LoRaWAN credentials
   - DevEUI, AppEUI, and AppKey management
   - Join status and transmission statistics

5. **WiFi Tab:**
   - Connect to existing WiFi network (client mode)
   - Scan for available networks
   - View connection status and IP address
   - AP mode automatically disabled when connected as client
   - Access device via https://stationsdata.local after connecting

6. **Security Tab:**
   - Change web interface username and password
   - Enable/disable HTTP Basic Authentication
   - Credentials stored securely in NVS
   - Default credentials: username `admin`, password `admin`

**WiFi Modes:**

The device supports two WiFi modes:

1. **Access Point (AP) Mode:**
   - Creates its own WiFi network on boot
   - SSID: `ESP32-Modbus-Config`
   - Access via: https://192.168.4.1 or https://stationsdata.local
   - Automatically disables after 20 minutes or when connected as client

2. **Client (Station) Mode:**
   - Connects to your existing WiFi network
   - Configure via WiFi tab in web interface
   - Access via: https://stationsdata.local (or device IP address)
   - AP mode automatically disabled when connected

**Note:** The web interface is protected by HTTP Basic Authentication. You'll be prompted for credentials when accessing any page.

## Modbus Register Map

### Holding Registers (Function Code 0x03 / 0x06)

| Register Address | Type           | Description                                      | Access      | Update Rate |
|------------------|----------------|--------------------------------------------------|-------------|-------------|
| 0                | Holding (16bit)| Sequential counter (increments on each access)   | Read/Write  | On access   |
| 1                | Holding (16bit)| Random number (0-65535)                         | Read Only   | Every 5s    |
| 2                | Holding (16bit)| Uptime in seconds (low 16-bit, wraps at 65535)  | Read Only   | Every 2s    |
| 3                | Holding (16bit)| Free heap memory in KB (low word)               | Read Only   | Every 2s    |
| 4                | Holding (16bit)| Free heap memory in KB (high word)              | Read Only   | Every 2s    |
| 5                | Holding (16bit)| Minimum free heap since boot (KB)               | Read Only   | Every 2s    |
| 6                | Holding (16bit)| CPU frequency in MHz                            | Read Only   | Every 2s    |
| 7                | Holding (16bit)| Number of FreeRTOS tasks                        | Read Only   | Every 2s    |
| 8                | Holding (16bit)| Chip temperature × 10 (e.g., 235 = 23.5°C)     | Read Only   | Every 2s    |
| 9                | Holding (16bit)| Number of CPU cores                             | Read Only   | Static      |
| 10               | Holding (16bit)| WiFi AP enabled (1=active, 0=disabled)          | Read Only   | On change   |
| 11               | Holding (16bit)| Number of connected WiFi clients (0-4)          | Read Only   | On change   |

**Holding Register Notes:**
- Registers 3-4 form a 32-bit value for total free heap. To get the actual value in KB:
  ```
  free_heap_kb = (register_4 << 16) | register_3
  ```
- Register 10 shows WiFi AP status: 1 when active (first 20 minutes after boot), 0 after timeout
- Register 11 shows real-time count of connected WiFi clients (updates immediately on connect/disconnect)

### Input Registers (Function Code 0x04) - SF₆ Gas Sensor Emulation

| Register Address | Raw Value Range | Scaling Factor | Engineering Units | Description                           | Update Rate |
|------------------|-----------------|----------------|-------------------|---------------------------------------|-------------|
| 0                | 1500-3500       | ×0.01          | 15.00-35.00 kg/m³ | SF₆ gas density                       | Every 3s    |
| 1                | 3000-8000       | ×0.1           | 300.0-800.0 kPa   | SF₆ gas pressure @20°C                | Every 3s    |
| 2                | 2880-3030       | ×0.1           | 288.0-303.0 K     | SF₆ gas temperature (15-30°C)         | Every 3s    |
| 3                | 3000-8000       | ×0.1           | 300.0-800.0 kPa   | SF₆ gas pressure variance             | Every 3s    |
| 4                | 1-247           | 1              | 1-247             | Slave ID (matches configured address) | Static      |
| 5                | 0x0000-0xFFFF   | N/A            | Hex               | Serial number (high word)             | Static      |
| 6                | 0x0000-0xFFFF   | N/A            | Hex               | Serial number (low word)              | Static      |
| 7                | 10-50           | ×0.1           | 1.0-5.0           | Software release version              | Static      |
| 8                | 14500-15500     | ×0.01          | 145.00-155.00 Hz  | Quartz oscillator frequency           | Every 3s    |

**Input Register Notes:**
- These registers emulate a real SF₆ gas density sensor with realistic behavior
- Values drift slowly over time with small random noise
- Pressure correlates with density for physical realism
- Serial number spans registers 5-6: `serial = (register_5 << 16) | register_6`
- To convert raw values to engineering units: `actual_value = raw_value × scaling_factor`
- Example: Raw density value 2550 → 2550 × 0.01 = 25.50 kg/m³
- Emulation provides graph-friendly data for SCADA/HMI testing

## Building and Flashing

### Prerequisites

- [PlatformIO](https://platformio.org/) installed
- Arduino framework for ESP32 (automatically installed by PlatformIO)
- Vision Master E290 connected via USB Type-C

### Build for Vision Master E290

The project is pre-configured for the Vision Master E290:

```bash
platformio run -e vision-master-e290-arduino
```

### Upload to E290

```bash
platformio run -e vision-master-e290-arduino --target upload
```

**Note:** You may need to hold the BOOT button (on the E290) while connecting USB or during upload to enter programming mode.

### Monitor

```bash
platformio run -e vision-master-e290-arduino --target monitor
```

Or combine upload and monitor:

```bash
platformio run -e vision-master-e290-arduino --target upload --target monitor
```

### Troubleshooting Build Issues

If you encounter build issues:

1. **Clean the build:** `platformio run -e vision-master-e290-arduino -t clean`
2. **Update PlatformIO:** `platformio upgrade`
3. **Delete build cache:** Remove the `.pio` directory and rebuild

## Testing with Modbus Master

You can test this slave using various Modbus master tools:

### Using mbpoll (Command Line)

```bash
# Read all 12 holding registers starting at address 0
mbpoll -a 1 -0 -r 0 -c 12 -t 4 -b 9600 -P none -s 1 /dev/ttyUSB0

# Breakdown of flags:
# -a 1       : Slave address 1
# -0         : Use zero-based register addressing (PDU addressing)
# -r 0       : Start at register 0
# -c 12      : Read 12 registers
# -t 4       : Type 4 = holding registers (16-bit)
# -b 9600    : Baud rate
# -P none    : No parity
# -s 1       : 1 stop bit

# Read all 9 input registers (SF6 sensor data)
mbpoll -a 1 -0 -r 0 -c 9 -t 3 -b 9600 -P none -s 1 /dev/ttyUSB0
# Note: -t 3 = input registers (read-only)

# Read specific SF6 sensor values (density, pressure, temperature)
mbpoll -a 1 -0 -r 0 -c 3 -t 3 -b 9600 -P none -s 1 /dev/ttyUSB0

# Write a value to holding register 0 (sequential counter)
mbpoll -a 1 -0 -r 0 -t 4 -b 9600 -P none -s 1 /dev/ttyUSB0 500

# Read WiFi status registers (10 and 11)
mbpoll -a 1 -0 -r 10 -c 2 -t 4 -b 9600 -P none -s 1 /dev/ttyUSB0
```

### Using QModMaster (GUI)

1. Configure connection:
   - Mode: RTU
   - Port: Your serial port
   - Baud: 9600
   - Data bits: 8
   - Parity: None
   - Stop bits: 1

2. Read holding registers:
   - Function: Read Holding Registers (0x03)
   - Slave ID: 1
   - Start Address: 0
   - Number of registers: 12

3. Read input registers (SF₆ sensor):
   - Function: Read Input Registers (0x04)
   - Slave ID: 1
   - Start Address: 0
   - Number of registers: 9

### Using Python pymodbus

```python
from pymodbus.client.sync import ModbusSerialClient

client = ModbusSerialClient(
    method='rtu',
    port='/dev/ttyUSB0',
    baudrate=9600,
    timeout=1,
    parity='N',
    stopbits=1,
    bytesize=8
)

client.connect()

# Read all 12 holding registers
result = client.read_holding_registers(address=0, count=12, unit=1)
print("=== Holding Registers ===")
print(f"Sequential Counter: {result.registers[0]}")
print(f"Random Number: {result.registers[1]}")
print(f"Uptime (seconds): {result.registers[2]}")
print(f"Free Heap (KB): {(result.registers[4] << 16) | result.registers[3]}")
print(f"CPU Frequency (MHz): {result.registers[6]}")
print(f"Temperature (°C): {result.registers[8] / 10.0}")
print(f"WiFi Enabled: {result.registers[10]}")
print(f"WiFi Clients: {result.registers[11]}")

# Read all 9 input registers (SF6 sensor data)
result = client.read_input_registers(address=0, count=9, unit=1)
print("\n=== Input Registers (SF₆ Sensor) ===")
print(f"SF₆ Density: {result.registers[0] * 0.01:.2f} kg/m³")
print(f"SF₆ Pressure @20°C: {result.registers[1] * 0.1:.1f} kPa")
print(f"SF₆ Temperature: {result.registers[2] * 0.1:.1f} K ({result.registers[2] * 0.1 - 273.15:.1f}°C)")
print(f"SF₆ Pressure Var: {result.registers[3] * 0.1:.1f} kPa")
print(f"Slave ID: {result.registers[4]}")
serial_number = (result.registers[5] << 16) | result.registers[6]
print(f"Serial Number: 0x{serial_number:08X}")
print(f"SW Release: {result.registers[7] * 0.1:.1f}")
print(f"Quartz Freq: {result.registers[8] * 0.01:.2f} Hz")

# Write to sequential counter
client.write_register(address=0, value=500, unit=1)

client.close()
```

## Expected Output

```
========================================
Vision Master E290 - Modbus RTU Slave
Firmware: v1.42
========================================

Modbus RTU Configuration:
  Slave ID: 1
  UART1: TX=GPIO43, RX=GPIO44, Baud=9600

Display initialized (rotation: 3)
Display updated

Modbus RTU slave started on UART1
Slave ID: 1

WiFi AP Configuration:
  SSID: ESP32-Modbus-Config
  Password: modbus123
  IP: 192.168.4.1
  mDNS: stationsdata.local
  Connect to: https://192.168.4.1 or https://stationsdata.local
  AP timeout: 20 minutes

Web server started on port 443 (HTTPS)
mDNS responder started: stationsdata.local

LoRaWAN Device Credentials
========================================
JoinEUI (AppEUI): 0x1234567890ABCDEF
DevEUI:           0xAABBCCDDEEFF0011
AppKey:           0x00112233445566778899AABBCCDDEEFF
NwkKey:           0x00112233445566778899AABBCCDDEEFF
========================================
Copy these credentials to your network server:
  - The Things Network (TTN)
  - Chirpstack
  - AWS IoT Core for LoRaWAN
========================================

LoRaWAN: Attempting OTAA join...
[LoRaWAN status updates will appear here]

Modbus and web interface ready
Waiting for connections...
```

## Troubleshooting

### No Communication

1. **Check wiring:**
   - TX (GPIO43) → HW-519 TXD
   - RX (GPIO44) → HW-519 RXD
   - 5V → HW-519 VCC (not 3.3V!)
   - GND → HW-519 GND
   - A/B to RS485 bus (try swapping if needed)
2. **Check termination:** HW-519 has built-in 120Ω termination - check jumper position
3. **Check baud rate:** Master and slave must use the same baud rate (9600)
4. **Check slave address:** Make sure the master is addressing slave ID 1
5. **Power:** HW-519 needs 5V, not 3.3V

### Garbled Data

1. **Check baud rate match:** Both devices must use 9600 baud
2. **Check ground connection:** Ensure common ground between devices
3. **Check HW-519 power:** Ensure HW-519 has proper 5V supply
4. **Check A/B wiring:** A to A, B to B (if reversed, swap them)

### ESP32-S3 Not Responding

1. **Check pins:** Verify GPIO pins match the configuration
2. **Check UART port:** UART1 is used (not UART0 which is typically used for console)
3. **Flash the firmware:** Run `pio run --target upload`

## Customization

### Change Slave Address

The slave address can be changed via the web interface (Configuration tab) or by editing `src/main.cpp`:
```cpp
#define MB_SLAVE_ID_DEFAULT 1   // Change to your desired address (1-247)
```

Changes via web interface take effect immediately and are saved to NVS.

### Change Baud Rate

Edit in `src/main.cpp`:
```cpp
#define MB_UART_BAUD    9600    // Change to 115200, 19200, etc.
```

### Change Pins

Edit in `src/main.cpp` (lines 53-55):
```cpp
#define MB_UART_NUM         1        // UART1
#define MB_UART_TX          43       // GPIO 43 - TX pin to HW-519 TXD
#define MB_UART_RX          44       // GPIO 44 - RX pin to HW-519 RXD
// RTS not used with HW-519 (automatic flow control)
```

**Important:** When changing pins on the Vision Master E290, avoid these GPIO pins:
- GPIO1-6: Used by E-Ink display (MOSI, SCK, CS, DC, RST, BUSY)
- GPIO8-14: Used by SX1262 LoRa module (NSS, SCK, MOSI, MISO, RST, BUSY, DIO1)
- GPIO18, 46: E-Ink power control
- Other peripherals as documented in the pinout

### Add More Registers

Modify the register structures in `src/main.cpp` (lines 69-94):
```cpp
struct HoldingRegisters {
    uint16_t sequential_counter;
    uint16_t random_number;
    uint16_t new_register1;      // Add new registers
    uint16_t new_register2;
    // ... etc
} holding_regs;

struct InputRegisters {
    uint16_t sf6_density;
    uint16_t sf6_pressure_20c;
    uint16_t new_register1;      // Add new registers
    // ... etc
} input_regs;
```

Update the Modbus register handler callbacks and web interface HTML accordingly.

### Customize SF₆ Sensor Emulation

#### Method 1: Web Interface (Recommended)

Use the web interface at https://192.168.4.1/registers:
1. Navigate to the "Registers" tab
2. Use the **SF6 Manual Control** panel
3. Enter desired values:
   - Density: 0-60 kg/m³
   - Pressure: 0-1100 kPa
   - Temperature: 215-360 K
4. Click "Update SF6 Values" to apply
5. Or click "Reset to Defaults" to restore factory defaults

**Note:** Values are automatically saved to flash (NVS) and persist across reboots. The emulation task adds small random drift around your set values for realistic behavior.

#### Method 2: Modify Default Values in Code

Edit `src/main.cpp` to change the default starting values:
```c
// SF6 emulation base values (global so sliders can update them)
float sf6_base_density = 25.0;       // kg/m3 - change default here
float sf6_base_pressure = 550.0;     // kPa
float sf6_base_temperature = 293.0;  // K
```

To adjust drift behavior, modify `update_input_registers()`:
```c
// Adjust drift magnitude
sf6_base_density += random(-10, 11) / 100.0;     // ±0.10 kg/m³ per update
sf6_base_pressure += random(-50, 51) / 10.0;     // ±5.0 kPa per update
sf6_base_temperature += random(-5, 6) / 10.0;    // ±0.5 K per update

// Adjust valid ranges
sf6_base_density = constrain(sf6_base_density, 0.0, 60.0);
sf6_base_pressure = constrain(sf6_base_pressure, 0.0, 1100.0);
sf6_base_temperature = constrain(sf6_base_temperature, 215.0, 360.0);
```

To disable drift entirely (static values), comment out the drift lines:
```c
// sf6_base_density += random(-10, 11) / 100.0;     // Disabled
// sf6_base_pressure += random(-50, 51) / 10.0;     // Disabled
// sf6_base_temperature += random(-5, 6) / 10.0;    // Disabled
```

## Security Considerations

### Development vs. Production

This code is intended for **development and testing** purposes. For production deployments, consider the following security enhancements:

### WiFi Configuration Portal

⚠️ **Current Implementation:**
- Hard-coded WiFi AP credentials (`modbus123`)
- HTTPS enabled with self-signed certificate (10-year validity)
- WiFi password is logged to console
- Known SSL library bug causing intermittent crashes

✅ **Security Features Implemented:**
- ✅ HTTP Basic Authentication for web interface (default: admin/admin)
- ✅ Credentials stored in NVS with ability to change via web UI
- ✅ Authentication can be enabled/disabled per deployment
- ✅ All endpoints protected with authentication checks (including SF6 control)
- ✅ Input validation on SF6 values (client-side HTML5 + server-side range checks)
- ✅ Thread-safe register updates with critical section protection
- ✅ NVS persistence with proper error handling
- ✅ Self-signed SSL certificate ready (10-year validity, mDNS support)

✅ **Additional Recommendations for Production:**
1. Generate unique WiFi AP passwords per device (store in NVS)
2. Change default web authentication credentials on first boot
3. Reduce AP timeout to minimum needed (or enable via physical button)
4. Remove password from logs (if present in console output)
5. Replace HTTPS_Server_Generic library to fix SSL crash bug
6. Add CSRF protection for configuration changes
7. Enable mDNS for consistent access in STA mode (esp32-modbus.local)
8. Add rate limiting on SF6 control endpoints (/sf6/update, /sf6/reset)
9. Implement request throttling to prevent flash wear from excessive NVS writes
10. Add audit logging for all SF6 value changes with timestamps
11. Use production certificate signed by trusted CA (not self-signed)

### Known SSL Library Vulnerability

⚠️ **HTTPS_Server_Generic Library Bug (HIGH):**
- **Issue:** Null pointer dereference in `SSL_write()` at ssl_lib.c:457
- **Impact:** Device may reboot when responding to SF6 control requests
- **Trigger:** Occurs during HTTPS response transmission (not during data processing)
- **Data Safety:** ✅ SF6 values are saved to NVS BEFORE response is sent - no data loss
- **Frequency:** Intermittent - does not happen on every request
- **Mitigation:**
  - User warned via yellow banner in web interface
  - Critical data saved before potential crash point
  - Consider replacing HTTPS_Server_Generic with esp_https_server
  - Reduce request frequency to minimize crash probability

**Error Details:**
```
Core 1 panic'ed (StoreProhibited). Exception was unhandled.
PC: 0x420e2d59 in SSL_write at ssl_lib.c:457
EXCVADDR: 0x00000038 (null pointer dereference)
```

**Why Values Are Safe:**
```cpp
// 1. Update values in critical section
portENTER_CRITICAL(&timerMux);
sf6_base_density = new_value;
input_regs.sf6_density = raw_value;
portEXIT_CRITICAL(&timerMux);

// 2. Save to NVS immediately
save_sf6_values();  // ✅ Completes successfully

// 3. Send HTTP response (crash may occur here)
res->setStatusCode(204);  // ⚠️ Crash point
```

### Modbus Protocol

⚠️ **Inherent Limitations:**
- Modbus RTU has no built-in authentication or encryption
- Any device on the RS485 bus can send/receive frames
- Register values are transmitted in plain text

✅ **Mitigation Strategies:**
1. **Physical Security:** Protect RS485 bus wiring
2. **Network Segmentation:** Isolate Modbus network from untrusted networks
3. **Application-Layer Security:** Consider adding custom authentication in unused registers
4. **Rate Limiting:** Monitor for abnormal request patterns
5. **Logging & Monitoring:** Track all configuration changes

### ESP32-S3 Hardware Security Features

For production firmware, consider enabling ESP32-S3 security features via PlatformIO build flags in `platformio.ini`:

```ini
build_flags =
    -D CONFIG_SECURE_BOOT_V2_ENABLED=1
    -D CONFIG_SECURE_FLASH_ENC_ENABLED=1
    -D CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=1
```

**Note:** Secure boot and flash encryption are permanent one-way operations. Test thoroughly in development before enabling in production.

**Additional Security:**
- Enable Arduino OTA updates with authentication
- Use ESP32 efuse settings to disable JTAG
- Implement firmware signing for updates

### Secure Coding Practices

✅ **Already Implemented:**
- Uses `snprintf` instead of `sprintf`
- Bounds checking on Modbus frame reception
- CRC validation on incoming frames
- Input validation on slave ID (1-247 range)
- Input validation on SF6 control values (range checks)
- Mutex protection for SF6 register updates (critical sections)
- Client-side and server-side validation for web inputs
- NVS storage with proper error handling

⚠️ **Additional Recommendations:**
1. Add rate limiting on HTTP endpoints (especially SF6 control endpoints)
2. Add Content Security Policy headers
3. Implement proper session management with tokens
4. Add CSRF protection for state-changing operations
5. Implement request throttling to prevent abuse

### Vulnerability Summary

| Risk Level | Issue | Status | Mitigation |
|------------|-------|--------|------------|
| HIGH | Hard-coded WiFi AP password | ⚠️ Open | Generate per-device passwords |
| HIGH | SSL library crash vulnerability | ⚠️ Known Issue | HTTPS_Server_Generic bug (ssl_lib.c:457) - Data saved before crash |
| MEDIUM | Default web credentials (admin/admin) | ⚠️ Partially Mitigated | Change on first boot, force password change |
| MEDIUM | Self-signed certificate (MITM risk) | ⚠️ Accepted | Use CA-signed cert for production |
| MEDIUM | No Modbus authentication | ⚠️ By Design | Physical security + monitoring |
| MEDIUM | Password logged to console | ⚠️ Open | Remove or mask in logs |
| MEDIUM | No rate limiting on SF6 endpoints | ⚠️ Open | Add request throttling |
| MEDIUM | Flash wear from excessive NVS writes | ⚠️ Open | Implement write throttling |
| LOW | No CSRF protection | ⚠️ Open | Add token validation |
| ~~HIGH~~ | ~~Unauthenticated web config~~ | ✅ **Fixed v1.22** | HTTP Basic Auth implemented |
| ~~MEDIUM~~ | ~~No HTTPS~~ | ✅ **Fixed v1.22** | Self-signed certificate deployed |
| ~~MEDIUM~~ | ~~No input validation on web forms~~ | ✅ **Fixed v1.38** | Client + server-side validation |
| ~~MEDIUM~~ | ~~Race conditions in register updates~~ | ✅ **Fixed v1.38** | Critical section protection added |

For a detailed security review, see the analysis performed on this codebase.

## Project History

This project was originally developed using ESP-IDF framework and has been converted to Arduino framework for better library compatibility and easier development. The core Modbus functionality is inspired by Espressif's Modbus examples.

## Library Dependencies

The project uses the following key libraries (automatically installed by PlatformIO):

- **heltec-eink-modules** - E-Ink display driver for Vision Master E290
- **modbus-esp8266** - Modbus RTU implementation for Arduino
- **RadioLib** - LoRaWAN stack with SX1262 support
- **HTTPS_Server_Generic** - HTTPS web server with SSL/TLS

## References

- [Modbus Protocol Specification](https://modbus.org/specs.php)
- [RadioLib Documentation](https://github.com/jgromes/RadioLib)
- [Heltec E-Ink Library](https://github.com/todd-herbert/heltec-eink-modules)
- [Vision Master E290 Hardware](https://heltec.org/project/vision-master-e290/)
- [HW-519 RS485 Module Information](https://www.google.com/search?q=HW-519+RS485+module)
