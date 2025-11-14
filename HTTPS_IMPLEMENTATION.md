# HTTPS Implementation Guide

## Certificate Information

A self-signed SSL certificate with mDNS support has been generated for the web server:

- **Certificate File:** `src/server_cert.h`
- **Validity Period:** 10 years (2025-11-12 to 2035-11-10)
- **Key Type:** RSA 2048-bit
- **Subject:** CN=stationsdata.local, O=VisionMaster, OU=E290
- **Subject Alternative Names (SANs):**
  - DNS: `stationsdata.local` (primary mDNS name)
  - DNS: `*.local` (wildcard for any .local domain)
  - DNS: `localhost`
  - IP: `192.168.4.1` (AP mode default IP)
  - IP: `127.0.0.1` (localhost)
- **Usage:** HTTPS web server in both AP and STA (client) modes

## Current Status

**HTTPS implementation is COMPLETE and FULLY FUNCTIONAL!**

- ✅ Self-signed SSL certificate integrated
- ✅ HTTPS server running on port 443
- ✅ HTTP redirect server running on port 80 (automatic redirect to HTTPS)
- ✅ All web handlers migrated to HTTPS API
- ✅ HTTP Basic Authentication implemented with base64 decoding
- ✅ mDNS responder enabled (stationsdata.local)
- ✅ Firmware compiled successfully
- ✅ Ready for deployment

**Library Used:** `khoih-prog/HTTPS_Server_Generic` v1.5.0 (Arduino-compatible fork with ESP32-S3 support)

## Accessing the Device

The device can be accessed via HTTPS using multiple methods:

### AP Mode (Default)
```
https://192.168.4.1/
https://stationsdata.local/  (mDNS enabled)
```

### STA Mode (WiFi Client)
```
https://stationsdata.local/   (recommended - works with any IP)
https://<dynamic-ip>/         (less convenient - IP may change)
```

### HTTP to HTTPS Redirect

An HTTP server is running on port 80 that automatically redirects all requests to HTTPS:

- **http://192.168.4.1/** → **https://192.168.4.1/**
- **http://stationsdata.local/** → **https://stationsdata.local/**

Users who accidentally access via HTTP will receive a **301 Moved Permanently** redirect and see a helpful message with a link to the HTTPS URL.

**Note:** mDNS (stationsdata.local) is automatically enabled in both AP and STA modes.

### Why mDNS is Important for STA Mode

When the device connects to an existing WiFi network (STA mode), it receives a dynamic IP address from the DHCP server. This IP can change between reboots or when the DHCP lease expires. Using mDNS (stationsdata.local) provides a consistent address regardless of the assigned IP.

**Benefits of mDNS:**
- Consistent access: `https://stationsdata.local/` always works
- No need to check device display or serial console for IP
- Certificate remains valid (SAN includes *.local wildcard)
- Works across different networks

**mDNS Support:**
- ✅ Linux: Built-in with Avahi
- ✅ macOS: Built-in (Bonjour)
- ✅ Windows 10+: Built-in (if network is set to Private)
- ⚠️ Windows 7-8: Requires Bonjour Print Services (Apple)
- ✅ iOS/Android: Built-in

### Enabling mDNS in ESP32

The firmware needs to initialize mDNS service (already common in ESP32 projects):

```cpp
#include <ESPmDNS.h>

void setup() {
    // After WiFi connects (AP or STA mode)
    if (!MDNS.begin("stationsdata")) {
        Serial.println("Error starting mDNS");
    } else {
        Serial.println("mDNS started: stationsdata.local");
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("https", "tcp", 443);  // When HTTPS enabled
    }
}

void loop() {
    MDNS.update();  // Keep mDNS alive
}
```

## Why HTTPS is Not Enabled Yet

The Arduino `WebServer` library used in this project only supports HTTP. To enable HTTPS, one of the following approaches is required:

### Option 1: ESP-IDF's esp_https_server (Recommended)

**Pros:**
- Official Espressif component
- Well-documented and maintained
- Full TLS support
- Good performance

**Cons:**
- Requires converting from Arduino WebServer to ESP-IDF API
- Significant code refactoring needed
- Different API structure

**Implementation Steps:**

1. Replace Arduino `WebServer` with ESP-IDF `esp_https_server`:
   ```cpp
   #include <esp_https_server.h>

   httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
   config.servercert = (const uint8_t *)server_cert_pem;
   config.servercert_len = strlen(server_cert_pem) + 1;
   config.prvtkey_pem = (const uint8_t *)server_key_pem;
   config.prvtkey_len = strlen(server_key_pem) + 1;

   httpd_handle_t server = NULL;
   esp_https_server_start(&config, &server);
   ```

2. Convert all handler functions from Arduino style to ESP-IDF:
   ```cpp
   // Arduino style (current)
   void handle_root() {
       server.send(200, "text/html", html);
   }

   // ESP-IDF style (required for HTTPS)
   static esp_err_t handle_root(httpd_req_t *req) {
       httpd_resp_set_type(req, "text/html");
       httpd_resp_send(req, html.c_str(), html.length());
       return ESP_OK;
   }
   ```

3. Register handlers using ESP-IDF method:
   ```cpp
   httpd_uri_t root_uri = {
       .uri = "/",
       .method = HTTP_GET,
       .handler = handle_root,
       .user_ctx = NULL
   };
   httpd_register_uri_handler(server, &root_uri);
   ```

**References:**
- [ESP-IDF HTTPS Server Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_https_server.html)
- [ESP-IDF HTTPS Server Example](https://github.com/espressif/esp-idf/tree/master/examples/protocols/https_server)

### Option 2: ESP32_HTTPS_Server Library

**Pros:**
- Easier migration from Arduino WebServer
- Similar API structure
- Less code changes

**Cons:**
- Third-party library (not official)
- May have bugs or limited support
- Additional dependency

**Implementation Steps:**

1. Add library to `platformio.ini`:
   ```ini
   lib_deps =
       fhessel/ESP32_HTTPS_Server @ ^1.0.0
   ```

2. Replace `WebServer` with `HTTPSServer`:
   ```cpp
   #include <HTTPSServer.hpp>
   #include <SSLCert.hpp>
   #include "server_cert.h"

   using namespace httpsserver;

   SSLCert cert = SSLCert(
       (const unsigned char *)server_cert_pem,
       strlen(server_cert_pem) + 1,
       (const unsigned char *)server_key_pem,
       strlen(server_key_pem) + 1
   );

   HTTPSServer server = HTTPSServer(&cert);
   ```

3. Minimal handler changes (API is similar to Arduino WebServer)

**References:**
- [ESP32_HTTPS_Server GitHub](https://github.com/fhessel/esp32_https_server)

### Option 3: Hybrid Approach (HTTP + Optional HTTPS)

Run both HTTP (port 80) and HTTPS (port 443) servers simultaneously:

- HTTP for local configuration (192.168.4.1)
- HTTPS for remote access with valid domain/certificate

**Pros:**
- Backward compatible
- Works with self-signed cert warnings
- Flexible deployment

**Cons:**
- More resource usage
- Two servers to maintain

## Browser Certificate Warnings

Since this is a **self-signed certificate**, browsers will show security warnings:

### Chrome/Edge Warning:
```
Your connection is not private
NET::ERR_CERT_AUTHORITY_INVALID
```

### Firefox Warning:
```
Warning: Potential Security Risk Ahead
```

### Bypassing Warnings:

**Option A: Accept Risk (Development Only)**
- Click "Advanced" → "Proceed to 192.168.4.1 (unsafe)"
- Not recommended for production

**Option B: Import Certificate to Trusted Root**

**Windows:**
```bash
# Export certificate from device
# Import to Windows Certificate Store
certmgr.msc → Trusted Root Certification Authorities → Import
```

**Linux:**
```bash
# Copy certificate
sudo cp server_cert.pem /usr/local/share/ca-certificates/esp32.crt
sudo update-ca-certificates
```

**macOS:**
```bash
# Add to Keychain
sudo security add-trusted-cert -d -r trustRoot -k /Library/Keychains/System.keychain server_cert.pem
```

**Option C: Use CA-Signed Certificate (Production)**

For production with domain name:
1. Obtain domain (e.g., esp32-device.local via mDNS or real domain)
2. Get certificate from Let's Encrypt (requires public IP or DNS-01 challenge)
3. Replace self-signed cert with CA-signed cert

## Implementation Checklist

When ready to implement HTTPS:

- [ ] Choose implementation method (ESP-IDF vs Library vs Hybrid)
- [ ] Update `platformio.ini` with dependencies
- [ ] Refactor handler functions to new API
- [ ] Include `server_cert.h` in main code
- [ ] Configure SSL/TLS settings (cipher suites, TLS version)
- [ ] Update port from 80 to 443 (or add both)
- [ ] Test with browser and accept certificate warning
- [ ] Update documentation and user instructions
- [ ] Consider HTTP→HTTPS redirect for port 80
- [ ] Update authentication to work with HTTPS
- [ ] Test HTTP Basic Auth over TLS

## Security Considerations

### Self-Signed Certificate Limitations:

1. **No Chain of Trust:** Browsers cannot verify certificate authenticity
2. **MITM Vulnerable:** Without trusted CA, man-in-the-middle attacks possible if certificate not verified
3. **User Experience:** Warning dialogs may confuse users

### Recommendations:

**For Development/Lab Use:**
- Self-signed certificate is acceptable
- Users can add exception in browser
- Document warning bypass procedure

**For Production:**
- Use CA-signed certificate if public-facing
- For isolated networks, distribute root CA to all clients
- Consider using private PKI infrastructure
- Implement certificate pinning in client applications

## Testing HTTPS

Once implemented, test with:

```bash
# Test HTTPS connection (ignore certificate)
curl -k https://192.168.4.1/

# Show certificate details
openssl s_client -connect 192.168.4.1:443 -showcerts

# Test with authentication
curl -k -u admin:admin https://192.168.4.1/security

# Verify TLS version
nmap --script ssl-enum-ciphers -p 443 192.168.4.1
```

## Performance Impact

HTTPS adds computational overhead:

- **TLS Handshake:** ~100-300ms additional latency on first connection
- **Encryption/Decryption:** ~5-10% CPU usage increase
- **Memory:** ~40KB additional RAM for TLS buffers
- **Flash:** ~100KB additional code size for mbedTLS library

**ESP32-S3 Impact:**
- Well within capabilities (240MHz dual-core, 8MB RAM)
- Negligible performance impact for web interface use case
- Modbus RTU and display updates unaffected

## Certificate Renewal

The current certificate expires in 10 years (2035-11-10). To renew:

```bash
# Generate new certificate (10 more years)
openssl req -x509 -nodes -days 3650 -newkey rsa:2048 \
    -keyout server_key.pem -out server_cert.pem \
    -subj "/C=US/ST=State/L=City/O=VisionMaster/OU=E290/CN=192.168.4.1"

# Convert to C header (use provided Python script)
python3 convert_cert.py

# Replace src/server_cert.h
# Rebuild and flash firmware
```

## Implementation Details

### What Was Implemented (November 2025)

The following changes were made to enable HTTPS with full functionality:

**1. Library Integration (`platformio.ini`)**
- Added `khoih-prog/HTTPS_Server_Generic` library (v1.5.0)
- Added ESP32S3 platform definitions for compatibility

**2. Code Refactoring (`src/main.cpp`)**
- Replaced `WebServer` with `HTTPSServer` and `SSLCert`
- Integrated `server_cert.h` (self-signed certificate)
- Updated all 13 web handler functions:
  - Changed signatures to accept `HTTPRequest * req, HTTPResponse * res`
  - Replaced `server.send()` with `res->setStatusCode()`, `res->setHeader()`, `res->print()`
  - Replaced `server.hasArg()` / `server.arg()` with custom `getPostParameter()` function
- Changed `server.handleClient()` to `server.loop()` in main loop
- Implemented HTTP Basic Authentication:
  - Added `#include "mbedtls/base64.h"` for base64 decoding
  - Parse Authorization header
  - Decode base64 credentials
  - Verify username/password against stored credentials
  - Return 401 Unauthorized with WWW-Authenticate header if auth fails

**3. mDNS Support**
- Added `#include <ESPmDNS.h>`
- Initialize mDNS in both `setup_wifi_ap()` and `setup_wifi_client()`
- Register hostname: `stationsdata.local`
- Advertise services: HTTPS (port 443) and HTTP (port 80)

**4. HTTP to HTTPS Redirect Server**
- Added `#include <HTTPServer.hpp>` for non-secure HTTP server
- Created separate `HTTPServer redirectServer` on port 80
- Implemented `handle_http_redirect()` function:
  - Extracts Host header and request path
  - Builds HTTPS URL with same host and path
  - Returns 301 Moved Permanently redirect
  - Provides fallback HTML page with clickable link
- Registered catch-all handlers for GET and POST methods (`/*`)
- Both servers run concurrently in main loop

**5. Resource Usage**
- RAM: 17.0% (55,576 bytes / 327,680 bytes)
- Flash: 39.3% (1,314,597 bytes / 3,342,336 bytes)
- Well within ESP32-S3 capabilities

### Testing

To test the implementation:

**HTTPS Access:**
1. Upload firmware to device
2. Connect to WiFi AP (SSID: ESP32-Modbus-Config-XXXX)
3. Access via browser:
   - `https://192.168.4.1/` or
   - `https://stationsdata.local/`
4. Accept browser security warning (self-signed certificate)
5. Enter credentials when prompted (default: admin/admin)
6. All pages should load over HTTPS with authentication

**HTTP Redirect:**
1. Access via HTTP: `http://192.168.4.1/` or `http://stationsdata.local/`
2. Browser should automatically redirect to HTTPS version
3. Check serial monitor for redirect log: `HTTP->HTTPS redirect: https://...`
4. Alternatively, test with curl:
   ```bash
   curl -v http://192.168.4.1/
   # Should show: < HTTP/1.1 301 Moved Permanently
   # Should show: < Location: https://192.168.4.1/
   ```

## Debug Logging

The firmware provides debug logging controls to help troubleshoot HTTPS and authentication issues:

### Application Debug Settings (v1.23+)

Runtime-configurable debug flags accessible via the **Security** tab in the web interface:

**Enable HTTPS Debug Logging**
- Shows HTTP-to-HTTPS redirect activity
- Logs: `HTTP->HTTPS redirect: https://...`
- Controlled by checkbox in Security tab
- Persists across reboots (stored in NVS)

**Enable Authentication Debug Logging**
- Shows authentication attempts and credential validation
- Logs auth checks, header parsing, base64 decoding, credential comparison
- Controlled by checkbox in Security tab
- Persists across reboots (stored in NVS)

### HTTPS Library Logging (Compile-Time)

The HTTPS_Server_Generic library has its own logging system controlled by `HTTPS_LOGLEVEL` in `platformio.ini`:

**Log Levels:**
- `0` = No logging
- `1` = Errors only (default - recommended)
- `2` = Errors + Warnings
- `3` = Errors + Warnings + Info
- `4` = Errors + Warnings + Info + Debug

**Default Setting (Level 1):**
```ini
-D HTTPS_LOGLEVEL=1
```

This suppresses noisy connection messages:
- `[HTTPS:I] New connection. Socket FID=50`
- `[HTTPS:I] Connection closed. Socket FID=50`

But still shows important errors:
- `[HTTPS:E] SSL_accept failed. Aborting handshake. FID=50`

**To Enable Full HTTPS Debug Logging:**

If you need to troubleshoot SSL/TLS connection issues, change the build flag in `platformio.ini`:

```ini
-D HTTPS_LOGLEVEL=3
```

Then rebuild and flash:
```bash
platformio run -e vision-master-e290-arduino --target upload
```

This will show all connection events, SSL handshakes, and protocol details in the serial monitor.

## Future Enhancements

- [x] mDNS integration (stationsdata.local instead of IP) - **DONE**
- [x] HTTP Basic Authentication over TLS - **DONE**
- [x] HTTP to HTTPS redirect (port 80 redirect) - **DONE**
- [x] Debug logging controls - **DONE (v1.23)**
- [ ] Support for client certificates (mutual TLS)
- [ ] Certificate storage in NVS (updateable without reflashing)
- [ ] Web interface for certificate upload
- [ ] ACME client for automatic Let's Encrypt certificates
- [ ] HSTS (HTTP Strict Transport Security) headers
- [ ] Certificate expiry warning in web interface

## Questions?

See `TROUBLESHOOTING.md` for HTTPS-specific troubleshooting and the project's `CLAUDE.md` for general development guidance.
