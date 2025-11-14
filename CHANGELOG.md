# Changelog - Vision Master E290 SF6 Monitor

## Version 1.38 - SF6 Manual Control with NVS Persistence (2025-11-14)

### Added
- **SF6 Manual Control Panel** in /registers page
  - Text input fields for Density (0-60 kg/m³), Pressure (0-1100 kPa), Temperature (215-360 K)
  - "Update SF6 Values" button to save custom values
  - "Reset to Defaults" button to restore factory settings (25.0, 550.0, 293.0)
  - Client-side validation (HTML5 + JavaScript)
  - Server-side validation and range checking
- **NVS Persistence for SF6 Values**
  - `load_sf6_values()` - Loads saved values at startup
  - `save_sf6_values()` - Saves values to flash immediately
  - Values survive reboots, power cycles, and crashes
  - NVS namespace: "sf6" with keys: density, pressure, temperature, has_values
- **Two new HTTPS endpoints:**
  - `GET /sf6/update` - Updates SF6 base values with query parameters
  - `GET /sf6/reset` - Resets SF6 values to factory defaults
- **Enhanced logging** for SF6 operations with serial debug output

### Changed
- **SF6 emulation system** now uses persistent base values from NVS
- **Register page auto-refresh** changed from 2 to 5 seconds
- **HTTP response** changed from GIF/JSON to 204 No Content (more stable)
- **Critical sections** protect SF6 register updates from race conditions

### Fixed
- SF6 values now persist across reboots (previously volatile)
- Reset button now properly updates input registers immediately

### Known Issues
- **SSL library bug** in HTTPS_Server_Generic may cause occasional device reboots
  - Error: Core 1 panic (StoreProhibited) in SSL_write at ssl_lib.c:457
  - Crash occurs AFTER values are saved, so data is not lost
  - User warned via yellow banner in web interface
  - Workaround: Single-click submission (not continuous updates)

### Technical Details
- **Global variables:** `sf6_base_density`, `sf6_base_pressure`, `sf6_base_temperature`
- **Thread safety:** `portENTER_CRITICAL`/`portEXIT_CRITICAL` mutex protection
- **Update frequency:** Drift added every 3 seconds via `update_input_registers()`
- **Drift magnitude:** ±0.10 kg/m³, ±5.0 kPa, ±0.5 K per update
- **Valid ranges:** 0-60 kg/m³, 0-1100 kPa, 215-360 K (enforced by constrain)

### Version History (v1.34 - v1.38)
- **v1.34:** Initial text input implementation (replaced sliders)
- **v1.35:** Changed to 204 No Content response with warning message
- **v1.36:** Added NVS persistence functions
- **v1.37:** Added reset button for defaults
- **v1.38:** Added debug logging to SF6 reset handler

## Version 1.22 - Web Interface Authentication (2025-11-12)

### Added
- **HTTP Basic Authentication** for web interface security
- **Security settings page** (/security) for credential management
- **Configurable authentication** - enable/disable via web UI
- **Persistent credentials** stored in NVS (Non-Volatile Storage)
- **Debug logging** for authentication events with serial output
- **Security tab** in web interface navigation

### Changed
- **All web endpoints** now require authentication
- **Default credentials** set to username: `admin`, password: `admin`
- **Web interface** reorganized with Security tab added to all pages
- **Version format** updated to support minor versions up to .99

### Security
- ✅ **Fixed:** Unauthenticated web configuration access (HIGH severity)
- Users must authenticate before accessing any web pages or API endpoints
- Credentials can be changed via /security page
- Authentication can be disabled for trusted networks (not recommended)

### Technical Details
- **Authentication method:** HTTP Basic Auth (RFC 7617)
- **Credential storage:** NVS namespace "auth"
- **Function:** `check_authentication()` validates all requests
- **Username max length:** 32 characters
- **Password max length:** 32 characters

### Upgrade Notes
- First access will prompt for default credentials: admin/admin
- **Important:** Change default credentials immediately after first login
- Clear browser cache/use incognito mode to test authentication
- Credentials persist across firmware updates (stored in NVS)

### Added (Post-Release)
- **SSL Certificate Generated** for future HTTPS implementation
  - 10-year self-signed certificate with mDNS support
  - Subject: CN=esp32-modbus.local
  - SANs: esp32-modbus.local, *.local, localhost, 192.168.4.1
  - Wildcard support for STA mode with dynamic IPs
  - Certificate ready in `src/server_cert.h`
  - See `HTTPS_IMPLEMENTATION.md` for integration guide

### Known Issues
- HTTP (not HTTPS) - credentials transmitted in Base64 encoding
  - HTTPS certificate generated but not yet integrated
  - Implementation requires WebServer → esp_https_server migration
- No CSRF protection yet
- No rate limiting on authentication attempts

---

## Version 1.1.0 - LoRaWAN Integration (2025-11-10)

### Added
- **LoRaWAN connectivity** using RadioLib 7.4.0 with SX1262 transceiver
- **OTAA join support** for EU868 (configurable for other regions)
- **10-byte payload** with SF₆ sensor data (density, pressure, temperature, variance, counter)
- **Payload decoders** for TTN v3 and Chirpstack v4 (JavaScript)
- **Python decoder** for local testing and validation
- **LoRaWAN status** display on E-Ink (L: OK / L: NO)
- **Web interface** LoRaWAN statistics (uplinks, downlinks, RSSI, SNR)
- **Device status response** to network server MAC commands
- **Comprehensive documentation** (LORAWAN_SETUP.md)

### Changed
- **Display layout** changed to 1x text size for maximum data visibility
- **Display now shows:**
  - SF₆ sensor data with units
  - Modbus ID and request counter
  - WiFi status and clients
  - LoRaWAN join status and TX count
  - System uptime, heap, CPU frequency, temperature
- **Uplink interval:** 5 minutes (300 seconds)
- **Initialization order:** LoRa initialized before E-Ink to avoid SPI conflicts

### Fixed
- **Join detection:** Correctly recognize RadioLib return codes (-1118 = success, not error)
- **Uplink success detection:** Handle positive return codes (0/1/2 = success with/without downlink)
- **TCXO configuration:** Disabled for Vision Master E290 (uses crystal oscillator)
- **RX window handling:** Properly detect downlinks in RX1/RX2 windows
- **Counter tracking:** LoRaWAN uplink/downlink counters now increment correctly

### Technical Details
- **Radio:** SX1262 on shared SPI bus with E-Ink display
- **Pins:** NSS=8, DIO1=14, RESET=12, BUSY=13
- **Region:** EU868 (868 MHz)
- **Activation:** OTAA with JoinEUI, DevEUI, AppKey, NwkKey
- **Class:** A (default)
- **ADR:** Enabled by RadioLib
- **Confirmed uplinks:** No (unconfirmed)
- **Power:** External (battery status = 0)

### Known Issues
None - system fully operational.

### Files Added
- `lorawan_decoder.js` - TTN/Chirpstack payload decoder
- `lorawan_decoder.py` - Python testing decoder
- `LORAWAN_SETUP.md` - Complete setup guide
- `CHANGELOG.md` - This file

### Dependencies
- RadioLib 7.4.0 (LoRaWAN stack)
- heltec-eink-modules (E-Ink display)
- modbus-esp8266 4.1.0 (Modbus RTU slave)

---

## Version 1.0.0 - Initial Release

### Features
- ESP32-S3 Arduino framework implementation
- E-Ink display (296×128 pixels) with 30-second refresh
- Modbus RTU Slave on UART1 (9600 baud)
- WiFi configuration portal (20-minute timeout)
- Web interface for statistics and registers
- SF₆ sensor emulation (density, pressure, temperature)
- Holding registers (12 registers)
- Input registers (9 registers)
- Configurable slave ID via web interface
- System statistics (heap, CPU, temperature, uptime)
