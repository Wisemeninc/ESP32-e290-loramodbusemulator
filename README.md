# Vision Master E290 Modbus RTU Slave with HW-519

This project implements a full-featured Modbus RTU slave on the **Heltec Vision Master E290** (ESP32-S3R8 with E-Ink display and LoRa) using the HW-519 RS485 module. Perfect for testing SCADA systems, HMI applications, or learning Modbus protocol.

**Key Highlights:**
- 🔧 **12 Holding Registers** with ESP32 system metrics (CPU, memory, WiFi status)
- 🎯 **9 Input Registers** with realistic SF₆ gas sensor emulation
- 📊 **Dynamic sensor simulation** with graph-friendly realistic drift and noise
- 🌐 **WiFi configuration portal** with live register monitoring
- 📡 **HW-519 RS485 module** with automatic flow control (no RTS pin needed)
- 💾 **Persistent configuration** stored in NVS
- 📶 **LoRaWAN support** to transmit Modbus data wirelessly over long range
- 📺 **2.9" E-Ink display** shows real-time register data and LoRaWAN status

Case: https://www.printables.com/model/974647-vision-master-e290-v031-case-for-meshtastic/files
Board: https://www.amazon.de/-/en/ESP32-S3R8-Development-Compatible-Micpython-Meshtastic/dp/B0DRG5J5R5/ref=sr_1_1?crid=3U044HWSXMQ6A&dib=eyJ2IjoiMSJ9.YoHIFAF5SYU13wPMcI3z0o5FrWHRIAIURS_jXTnFTpBuP7UZve6GrgROBM5S9etb5bfGYUtu4rsEZLOdscTTNFs-IM4bNr3DjsYnKTtFmZWUq04eISMxsWONlUl04XZIYLjN-8t6Mk9vyMhSIrrhs19dqwEzApiuOnNlDGlJxTK2T0EpJVAQXFbpsOOBfXQj8S9x_1pfjDWp9Fd8jnI7yx7xQJ9tAMjJyrarr9p0xx4.v2UVvtgnnTRFwIt6zFcIBJht59kzyWvEq3QsAww-LrU&dib_tag=se&keywords=vision+master+e290&qid=1762809245&sprefix=vision+master+e290%2Caps%2C124&sr=8-1


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
- **HW-519 RS485 module** with automatic flow control
- **WiFi Access Point** for configuration (active for 20 minutes after boot)
  - SSID: `ESP32-Modbus-Config`
  - Password: `modbus123`
  - Configure Modbus slave ID via web interface (no restart required)
  - View real-time statistics (requests, uptime, etc.)
  - View and monitor all holding and input registers in real-time
  - Persistent configuration stored in NVS (Non-Volatile Storage)

### LoRaWAN Features
- **LoRaWAN OTAA** (Over-The-Air Activation) support
- **Periodic transmission** of Modbus register data
- **Configurable regions:** US915, EU868, AS923, AU915
- **Efficient payload encoding** (18 bytes for key registers)
- **SX1262 LoRa transceiver** integration framework
- **Downlink support** for remote configuration (framework)

**Note:** The current LoRaWAN implementation provides a framework structure. For full functionality, integrate with:
- [Heltec LoRaWAN Library](https://github.com/HelTecAutomation/Heltec_ESP32) (Arduino-based)
- [RadioLib](https://github.com/jgromes/RadioLib) for ESP32
- Custom LoRaMAC implementation for ESP-IDF

### E-Ink Display Features
- **2.9" E-Paper display** (296×128 pixels, black and white)
- **Real-time monitoring** of Modbus registers sent over LoRaWAN
- **Automatic updates** every 30 seconds
- **Partial refresh** for fast updates, full refresh every 10 cycles
- **Low power** - E-Ink retains image without power
- **Display shows:**
  - LoRaWAN status (JOINED/JOINING/DISABLED)
  - LoRaWAN uplink counter
  - System metrics (uptime, temperature, RAM, CPU)
  - SF₆ sensor data (density, pressure, temperature)

**Note:** The current E-Ink display implementation provides a framework with ASCII preview via serial logs. For full graphics, integrate with:
- [GxEPD2](https://github.com/ZinggJM/GxEPD2) library (Arduino, can be ported)
- [Heltec E-Ink Library](https://github.com/todd-herbert/heltec-eink-modules)
- [U8g2](https://github.com/olikraus/u8g2) with E-Paper support
- Custom DEPG0290BNS800 driver

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

**Note:** These pins are pre-configured in `main/display.h`.

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

Currently, the display system outputs an **ASCII preview** to the serial console every 30 seconds:

```bash
# Monitor serial output to see display updates
platformio run -e heltec_vision_master_e290 --target monitor
```

Example output:
```
╔════════════════════════════════════╗
║  Vision Master E290 - Modbus RTU  ║
╠════════════════════════════════════╣
║ LoRaWAN: JOINED   TX:   42    ║
╠════════════════════════════════════╣
║ SYSTEM METRICS (Holding Regs)     ║
║  Uptime:    12345 sec              ║
║  Temp:       25.3°C                ║
║  Free RAM:    234 KB               ║
║  CPU Freq:    240 MHz              ║
║  WiFi:      ON  (2 clients)        ║
╠════════════════════════════════════╣
║ SF6 SENSOR (Input Registers)      ║
║  Density:   25.50 kg/m³            ║
║  Pressure:  550.0 kPa              ║
║  Temp:      293.0 K                ║
║  Press Var: 550.0 kPa              ║
╚════════════════════════════════════╝
```

### Enable/Disable Display

Edit `main/display.h` to control the display:

```c
#define EINK_ENABLED    true    // Set to false to disable
```

### Display Update Frequency

The display updates every **30 seconds** to conserve E-Ink refresh cycles. This can be adjusted in `main/main.c`:

```c
// In display_update_task()
vTaskDelay(pdMS_TO_TICKS(30000));  // Change 30000 to desired ms
```

**Important:** E-Ink displays have limited refresh cycles (~1 million for the entire display). Use:
- **Partial refresh** for frequent updates (faster, may cause ghosting)
- **Full refresh** periodically (slower, clearer image)

The code automatically does a full refresh every 10 updates.

## LoRaWAN Configuration

### Getting LoRaWAN Credentials

The Vision Master E290 has a built-in SX1262 LoRa transceiver that can be used for LoRaWAN communication.

1. **Register your device** with a LoRaWAN network provider:
   - [The Things Network (TTN)](https://www.thethingsnetwork.org/) - Free, community-run
   - [Chirpstack](https://www.chirpstack.io/) - Self-hosted option
   - Commercial providers (AWS IoT, Helium, etc.)

2. **Get your credentials:**
   - **DevEUI** (Device EUI) - 8 bytes, unique device identifier
   - **AppEUI** (Application EUI) - 8 bytes, identifies the application
   - **AppKey** (Application Key) - 16 bytes, secret key for OTAA join

### Configuring LoRaWAN

Edit `main/lorawan_config.h`:

```c
// Enable/Disable LoRaWAN
#define LORAWAN_ENABLED         true

// Set your region
#define LORAWAN_REGION          LORAWAN_REGION_US915  // or EU868, AS923, AU915

// Set your credentials (replace these!)
static const uint8_t LORAWAN_DEVEUI[8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Replace with your DevEUI
};

static const uint8_t LORAWAN_APPEUI[8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Replace with your AppEUI
};

static const uint8_t LORAWAN_APPKEY[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Replace with
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // your AppKey
};

// Transmission interval (milliseconds)
#define LORAWAN_TX_INTERVAL_MS  60000  // Send every 60 seconds

// Select which data to send
#define SEND_HOLDING_REGISTERS      true    // ESP32 system metrics
#define SEND_INPUT_REGISTERS        true    // SF₆ sensor data
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

**Note:** These pins are pre-configured in `lorawan_config.h`. Verify they match your board.

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

### Completing the LoRaWAN Implementation

The current code provides a **framework** for LoRaWAN. To make it fully functional:

#### Option 1: Use RadioLib (Recommended for ESP-IDF)

1. Add RadioLib to your project:
   ```bash
   cd components
   git clone https://github.com/jgromes/RadioLib.git
   ```

2. Modify `lorawan.c` to use RadioLib's SX1262 and LoRaWAN classes

3. Implement proper OTAA join and uplink/downlink handling

#### Option 2: Port Heltec's LoRaWAN Library

1. The Heltec library is Arduino-based but can be adapted for ESP-IDF
2. Reference: https://github.com/HelTecAutomation/Heltec_ESP32
3. Extract the LoRaWAN MAC layer and SX1262 driver

#### Option 3: Use LoRaMac-node

1. Integrate the official Semtech LoRaMac-node stack
2. Port the necessary files to ESP-IDF
3. Configure for SX1262 hardware

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

On boot, the device creates a WiFi Access Point for 2 minutes:

- **SSID:** `ESP32-Modbus-Config`
- **Password:** `modbus123`
- **Web Interface:** http://192.168.4.1

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
   - Auto-refreshes every 2 seconds
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

6. **Security Tab:**
   - Change web interface username and password
   - Enable/disable HTTP Basic Authentication
   - Credentials stored securely in NVS
   - Default credentials: username `admin`, password `admin`

**Note:** The web interface is protected by HTTP Basic Authentication. You'll be prompted for credentials when accessing any page. WiFi AP automatically turns off 20 minutes after boot (or immediately when connected as WiFi client) to save power.

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
- ESP-IDF framework (automatically installed by PlatformIO)
- Vision Master E290 connected via USB Type-C

### Build for Vision Master E290

The project is now pre-configured for the Vision Master E290:

```bash
platformio run -e heltec_vision_master_e290
```

### Upload to E290

```bash
platformio run -e heltec_vision_master_e290 --target upload
```

**Note:** You may need to hold the BOOT button (on the E290) while connecting USB or during upload to enter programming mode.

### Monitor

```bash
platformio run -e heltec_vision_master_e290 --target monitor
```

Or combine upload and monitor:

```bash
platformio run -e heltec_vision_master_e290 --target upload --target monitor
```

### Troubleshooting Build Issues

If you encounter build issues:

1. **Clean the build:** `platformio run -e heltec_vision_master_e290 -t clean`
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
I (XXX) MB_SLAVE: ========================================
I (XXX) MB_SLAVE: ESP32-S3 Modbus RTU Slave with HW-519
I (XXX) MB_SLAVE: ========================================
I (XXX) MB_SLAVE: Slave Address: 1
I (XXX) MB_SLAVE: Baudrate: 9600
I (XXX) MB_SLAVE: UART Port: 1
I (XXX) MB_SLAVE: TX Pin: GPIO18 (HW-519 TXD)
I (XXX) MB_SLAVE: RX Pin: GPIO16 (HW-519 RXD)
I (XXX) MB_SLAVE: ========================================
I (XXX) MB_SLAVE: Starting WiFi AP for configuration...
I (XXX) MB_SLAVE: WiFi AP started. SSID:ESP32-Modbus-Config Password:modbus123 Channel:1
I (XXX) MB_SLAVE: Connect to http://192.168.4.1 to configure
I (XXX) MB_SLAVE: AP will automatically turn off in 20 minutes
I (XXX) MB_SLAVE: Starting web server on port: 80
I (XXX) MB_SLAVE: Holding registers initialized:
I (XXX) MB_SLAVE:   Register 0 (Sequential Counter): 0
I (XXX) MB_SLAVE:   Register 1 (Random Number): 12345
I (XXX) MB_SLAVE:   ...
I (XXX) MB_SLAVE: Input registers initialized:
I (XXX) MB_SLAVE:   Register 1 (SF6 Density): 25.50 kg/m³
I (XXX) MB_SLAVE:   Register 2 (SF6 Pressure @20C): 550.0 kPa
I (XXX) MB_SLAVE:   Register 3 (SF6 Temperature): 293.0 K
I (XXX) MB_SLAVE:   ...
I (XXX) MB_SLAVE: Modbus slave stack initialized successfully
I (XXX) MB_SLAVE: SF6 sensor emulation task started - values will update every 3 seconds
I (XXX) MB_SLAVE: Modbus Holding Registers (0-11):
I (XXX) MB_SLAVE:   Address 0: Sequential Counter (Read/Write)
I (XXX) MB_SLAVE:   ...
I (XXX) MB_SLAVE: Modbus Input Registers (0-8):
I (XXX) MB_SLAVE:   Address 0: SF6 Gas Density (×0.01 kg/m³)
I (XXX) MB_SLAVE:   ...
I (XXX) MB_SLAVE: Waiting for Modbus master requests...
I (XXX) MB_SLAVE: Random number updated: 54321
I (XXX) MB_SLAVE: HOLDING READ: Addr=0, Size=2, Value[0]=0, Value[1]=54321
I (XXX) MB_SLAVE: INPUT READ: Addr=0, Size=3
I (XXX) MB_SLAVE:   SF6 Density: 25.32 kg/m³
I (XXX) MB_SLAVE: Sequential counter incremented to: 1
I (XXX) MB_SLAVE: AP timeout reached - shutting down WiFi AP
I (XXX) MB_SLAVE: WiFi AP stopped - device now running in Modbus-only mode
```

## Troubleshooting

### No Communication

1. **Check wiring:** 
   - TX (GPIO18) → HW-519 TXD
   - RX (GPIO16) → HW-519 RXD
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

Edit in `main/main.c`:
```c
#define MB_SLAVE_ADDR   (1)     // Change to your desired address (1-247)
```

### Change Baud Rate

Edit in `main/main.c`:
```c
#define MB_DEV_SPEED    (9600)  // Change to 115200, 19200, etc.
```

### Change Pins

Edit in `main/main.c`:
```c
#define MB_UART_TXD     (17)    // TX pin - HW-519 TXD (GPIO17 on E290)
#define MB_UART_RXD     (18)    // RX pin - HW-519 RXD (GPIO18 on E290)
// RTS not used with HW-519 (automatic flow control)
```

**Important:** When changing pins on the Vision Master E290, avoid these GPIO pins:
- GPIO1-6: Used by E-Ink display (MOSI, SCK, CS, DC, RST, BUSY)
- GPIO21: Custom button
- GPIO38-39: GPS interface (if using GPS module)
- Other LoRa module pins (consult E290 documentation)

### Add More Registers

Modify the `holding_reg_params_t` or `input_reg_params_t` structure:
```c
typedef struct {
    uint16_t sequential_counter;
    uint16_t random_number;
    uint16_t new_register1;      // Add new registers
    uint16_t new_register2;
    // ... etc
} holding_reg_params_t;
```

Don't forget to update:
```c
#define MB_REG_HOLDING_SIZE     (4)  // Update count
#define MB_REG_INPUT_SIZE       (9)  // Update if adding input registers
```

### Customize SF₆ Sensor Emulation

Modify the emulation ranges and behavior in `sf6_sensor_emulation_task()`:
```c
// Base values for simulation (mid-range)
float base_density = 25.0;        // Change starting point
float base_pressure = 550.0;
float base_temperature = 293.0;

// Adjust ranges
if (base_density < 15.0) base_density = 15.0;  // Min value
if (base_density > 35.0) base_density = 35.0;  // Max value

// Change update rate
vTaskDelay(pdMS_TO_TICKS(3000)); // Update every 3 seconds (adjust as needed)
```

To disable emulation and use static values, comment out the task creation:
```c
// xTaskCreate(sf6_sensor_emulation_task, "sf6_emulation", 4096, NULL, 5, NULL);
```

## Security Considerations

### Development vs. Production

This code is intended for **development and testing** purposes. For production deployments, consider the following security enhancements:

### WiFi Configuration Portal

⚠️ **Current Implementation:**
- Hard-coded WiFi AP credentials (`modbus123`)
- Plain HTTP (no TLS)
- WiFi password is logged to console

✅ **Security Features Implemented:**
- ✅ HTTP Basic Authentication for web interface (default: admin/admin)
- ✅ Credentials stored in NVS with ability to change via web UI
- ✅ Authentication can be enabled/disabled per deployment
- ✅ All endpoints protected with authentication checks
- ✅ Self-signed SSL certificate ready (10-year validity, mDNS support)

✅ **Additional Recommendations for Production:**
1. Generate unique WiFi AP passwords per device (store in NVS)
2. Change default web authentication credentials on first boot
3. Reduce AP timeout to minimum needed (or enable via physical button)
4. Remove password from logs (if present in console output)
5. Implement HTTPS (certificate ready - see `HTTPS_IMPLEMENTATION.md`)
6. Add CSRF protection for configuration changes
7. Enable mDNS for consistent access in STA mode (esp32-modbus.local)

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

For production firmware, enable these ESP-IDF security features:

```
CONFIG_SECURE_BOOT_V2_ENABLED=y          # Secure Boot V2
CONFIG_SECURE_FLASH_ENC_ENABLED=y        # Flash Encryption
CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=y      # Memory Protection
CONFIG_HEAP_POISONING_COMPREHENSIVE=y     # Heap Corruption Detection
CONFIG_FREERTOS_CHECK_STACKOVERFLOW=2     # Stack Overflow Detection
```

**Efuse Settings (one-way, test thoroughly first):**
- Disable JTAG debugging
- Disable USB Serial/JTAG (if not needed)
- Enable download mode protection

### Secure Coding Practices

✅ **Already Implemented:**
- Uses `snprintf` instead of `sprintf`
- Bounds checking on Modbus frame reception
- CRC validation on incoming frames
- Input validation on slave ID (1-247 range)

⚠️ **Additional Recommendations:**
1. Add rate limiting on HTTP endpoints
2. Validate all user inputs from web interface
3. Add Content Security Policy headers
4. Implement proper session management
5. Add mutex protection for shared register access

### Vulnerability Summary

| Risk Level | Issue | Status | Mitigation |
|------------|-------|--------|------------|
| HIGH | Hard-coded WiFi AP password | ⚠️ Open | Generate per-device passwords |
| MEDIUM | Default web credentials (admin/admin) | ⚠️ Partially Mitigated | Change on first boot, force password change |
| MEDIUM | No Modbus authentication | ⚠️ Open | Physical security + monitoring |
| MEDIUM | Password logged to console | ⚠️ Open | Remove or mask in logs |
| LOW | No HTTPS | ⚠️ Open | Add self-signed cert |
| LOW | No CSRF protection | ⚠️ Open | Add token validation |
| ~~HIGH~~ | ~~Unauthenticated web config~~ | ✅ **Fixed v1.22** | HTTP Basic Auth implemented |

For a detailed security review, see the analysis performed on this codebase.

## License

This project is based on the ESP-IDF Modbus example from Espressif Systems.

## References

- [ESP-IDF Modbus Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/modbus.html)
- [Espressif esp-modbus GitHub](https://github.com/espressif/esp-modbus)
- [Modbus Protocol Specification](https://modbus.org/specs.php)
- [HW-519 RS485 Module Information](https://www.google.com/search?q=HW-519+RS485+module)
