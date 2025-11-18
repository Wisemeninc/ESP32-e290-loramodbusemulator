/*
 * Vision Master E290 - Modbus RTU Slave with E-Ink Display
 * Arduino Framework Version
 *
 * This is a complete rewrite using Arduino framework to leverage
 * the Heltec E-Ink library directly.
 */

#include <Arduino.h>
#include <heltec-eink-modules.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPServer.hpp>
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <Preferences.h>
#include <ModbusRTU.h>
#include <RadioLib.h>
#include "mbedtls/base64.h"
#include "server_cert.h"

// ============================================================================
// DISPLAY SETUP
// ============================================================================

// Display rotation configuration
// 0 = Portrait (default)
// 1 = Landscape (90 degrees clockwise)
// 2 = Portrait inverted (180 degrees)
// 3 = Landscape inverted (270 degrees clockwise)
#define DISPLAY_ROTATION 3

// Deployment mode configuration
// false = Development mode (default password: "modbus123")
// true  = Production mode (auto-generated strong password, stored in NVS)
#define MODE_PRODUCTION false

// Firmware version configuration
// Format: MMmm where MM = major version, mm = minor version (2 digits)
// Examples: 111 = v1.11, 203 = v2.03, 1545 = v15.45
// Display format: v(FIRMWARE_VERSION/100).(FIRMWARE_VERSION%100)
#define FIRMWARE_VERSION 143  // v1.43 - Updated README: WiFi SSID dynamic with MAC, password modes clarified

// E-Ink display for Vision Master E290
EInkDisplay_VisionMasterE290 display;

// ============================================================================
// MODBUS CONFIGURATION
// ============================================================================

#define MB_UART_NUM         1        // UART1
#define MB_UART_TX          43       // GPIO 43
#define MB_UART_RX          44       // GPIO 44
#define MB_UART_BAUD        9600
#define MB_SLAVE_ID_DEFAULT 1

// Modbus RTU instance
ModbusRTU mb;

// Statistics
uint32_t modbus_request_count = 0;
uint32_t modbus_read_count = 0;
uint32_t modbus_write_count = 0;
uint32_t modbus_error_count = 0;

// Modbus registers
struct HoldingRegisters {
    uint16_t sequential_counter;
    uint16_t random_number;
    uint16_t uptime_seconds;
    uint16_t free_heap_kb_low;
    uint16_t free_heap_kb_high;
    uint16_t min_heap_kb;
    uint16_t cpu_freq_mhz;
    uint16_t task_count;
    uint16_t temperature_x10;
    uint16_t cpu_cores;
    uint16_t wifi_enabled;
    uint16_t wifi_clients;
} holding_regs;

struct InputRegisters {
    uint16_t sf6_density;           // kg/m3 x 100
    uint16_t sf6_pressure_20c;      // kPa x 10
    uint16_t sf6_temperature;       // K x 10
    uint16_t sf6_pressure_var;      // kPa x 10
    uint16_t slave_id;
    uint16_t serial_hi;
    uint16_t serial_lo;
    uint16_t sw_release;
    uint16_t quartz_freq;
} input_regs;

// SF6 emulation base values (global so sliders can update them)
float sf6_base_density = 25.0;       // kg/m3
float sf6_base_pressure = 550.0;     // kPa
float sf6_base_temperature = 293.0;  // K

// Mutex for protecting SF6 register updates
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// ============================================================================
// LORAWAN CONFIGURATION
// ============================================================================

// Vision Master E290 SX1262 LoRa Radio Pins (Heltec WiFi LoRa 32 V3)
#define LORA_MOSI   10   // MOSI
#define LORA_MISO   11   // MISO
#define LORA_SCK    9    // SCK
#define LORA_NSS    8    // NSS/CS
#define LORA_DIO1   14   // DIO1
#define LORA_NRST   12   // RESET
#define LORA_BUSY   13   // BUSY

// LoRaWAN Credentials (OTAA) - Loaded from NVS or generated on first boot
uint64_t joinEUI = 0x0000000000000000;   // AppEUI (MSB)
uint64_t devEUI  = 0x0000000000000000;   // DevEUI (MSB)
uint8_t appKey[16] = {0};                // 128-bit AppKey (MSB)
uint8_t nwkKey[16] = {0};                // 128-bit NwkKey (MSB)

// SX1262 radio instance
// Module(cs, irq, rst, gpio)
// RadioLib will handle SPI internally
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY);

// LoRaWAN node instance
LoRaWANNode node(&radio, &EU868);  // Change region as needed: US915, AU915, AS923, etc.

// LoRaWAN status
bool lorawan_joined = false;
uint32_t lorawan_uplink_count = 0;
uint32_t lorawan_downlink_count = 0;
int16_t lorawan_last_rssi = 0;
float lorawan_last_snr = 0;

// ============================================================================
// WIFI CONFIGURATION PORTAL
// ============================================================================

using namespace httpsserver;

// SSL Certificate
SSLCert serverCert = SSLCert(
    (unsigned char *)server_cert_pem,
    strlen(server_cert_pem) + 1,
    (unsigned char *)server_key_pem,
    strlen(server_key_pem) + 1
);

// HTTPS Server on port 443
HTTPSServer server = HTTPSServer(&serverCert);

// HTTP Server on port 80 (for redirects to HTTPS)
HTTPServer redirectServer = HTTPServer(80);

Preferences preferences;

uint8_t modbus_slave_id = MB_SLAVE_ID_DEFAULT;
bool wifi_ap_active = false;
bool wifi_client_connected = false;
unsigned long wifi_start_time = 0;
const unsigned long WIFI_TIMEOUT_MS = 20 * 60 * 1000;  // 20 minutes
char wifi_password[17] = "modbus123";  // WiFi AP password (16 chars + null terminator)

// WiFi Client Configuration
char wifi_client_ssid[33] = "";  // WiFi SSID (32 chars + null terminator)
char wifi_client_password[64] = "";  // WiFi password (63 chars + null terminator)

// Web Authentication Configuration
bool auth_enabled = true;  // Enable/disable authentication
char auth_username[33] = "admin";  // Default username
char auth_password[33] = "admin";  // Default password

// Debug Configuration
bool debug_https_enabled = false;  // Enable/disable HTTPS debug logging
bool debug_auth_enabled = false;   // Enable/disable authentication debug logging

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void setup_display();
void setup_wifi_ap();
void setup_web_server();
void setup_redirect_server();
void setup_modbus();
void setup_lorawan();
void update_holding_registers();
void update_input_registers();
void update_display();
void send_lorawan_uplink();
void handle_root(HTTPRequest * req, HTTPResponse * res);
void handle_stats(HTTPRequest * req, HTTPResponse * res);
void handle_registers(HTTPRequest * req, HTTPResponse * res);
void handle_sf6_update(HTTPRequest * req, HTTPResponse * res);
void handle_sf6_reset(HTTPRequest * req, HTTPResponse * res);
void handle_config(HTTPRequest * req, HTTPResponse * res);
void handle_lorawan(HTTPRequest * req, HTTPResponse * res);
void handle_lorawan_config(HTTPRequest * req, HTTPResponse * res);
void handle_wifi(HTTPRequest * req, HTTPResponse * res);
void handle_reboot(HTTPRequest * req, HTTPResponse * res);
void load_lorawan_credentials();
void save_lorawan_credentials();
void generate_lorawan_credentials();
void print_lorawan_credentials();
void save_lorawan_session();
void load_lorawan_session();
void generate_wifi_password();
void load_wifi_password();
void save_wifi_password();
void load_wifi_client_credentials();
void save_wifi_client_credentials();
void load_sf6_values();
void save_sf6_values();
bool setup_wifi_client();
void handle_wifi_scan(HTTPRequest * req, HTTPResponse * res);
void handle_wifi_connect(HTTPRequest * req, HTTPResponse * res);
void handle_wifi_status(HTTPRequest * req, HTTPResponse * res);
void load_auth_credentials();
void save_auth_credentials();
String readPostBody(HTTPRequest * req);
bool getPostParameterFromBody(const String& body, const char* name, String& value);
bool getPostParameter(HTTPRequest * req, const char* name, String& value);
bool check_authentication(HTTPRequest * req, HTTPResponse * res);
void handle_security(HTTPRequest * req, HTTPResponse * res);
void handle_security_update(HTTPRequest * req, HTTPResponse * res);
void handle_debug_update(HTTPRequest * req, HTTPResponse * res);
void handle_enable_auth(HTTPRequest * req, HTTPResponse * res);
void handle_http_redirect(HTTPRequest * req, HTTPResponse * res);
void display_password_on_screen(const char* ssid);
void draw_char(int x, int y, char c, int scale);
void draw_text(int x, int y, const char* text, int scale);
void draw_number(int x, int y, float value, int decimals, int scale);

// Modbus callbacks
uint16_t cb_read_holding_register(TRegister* reg, uint16_t val);
uint16_t cb_read_input_register(TRegister* reg, uint16_t val);
uint16_t cb_write_holding_register(TRegister* reg, uint16_t val);

// ============================================================================
// ARDUINO SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n========================================");
    Serial.println("Vision Master E290 - Modbus RTU Slave");
    Serial.println("========================================");
    Serial.println("Framework: Arduino");
    Serial.println("Display: 2.9\" E-Ink (296x128)");
    Serial.println("========================================\n");

    // Initialize preferences (NVS)
    preferences.begin("modbus", false);
    modbus_slave_id = preferences.getUChar("slave_id", MB_SLAVE_ID_DEFAULT);
    Serial.printf("Modbus Slave ID: %d\n", modbus_slave_id);
    preferences.end();  // Close modbus namespace

    // Load authentication credentials
    Serial.println("\n>>> Loading authentication credentials...");
    load_auth_credentials();
    Serial.printf("Authentication: %s\n", auth_enabled ? "Enabled" : "Disabled");

    // Load SF6 base values from NVS
    Serial.println("\n>>> Loading SF6 base values...");
    load_sf6_values();

    // Load or generate LoRaWAN credentials
    Serial.println("\n>>> Loading LoRaWAN credentials...");
    load_lorawan_credentials();

    // Print credentials to console for registration with network server
    Serial.println(">>> Printing LoRaWAN credentials...");
    print_lorawan_credentials();

    // Initialize LoRaWAN FIRST (before E-Ink display)
    // This ensures LoRa radio gets clean SPI access
    // E-Ink will be initialized AFTER join completes to avoid SPI conflicts during RX
    setup_lorawan();

    // Now safe to initialize E-Ink display after join attempt completes
    // The join process (TX + RX windows) is finished, SPI can be shared
    setup_display();

    // Initialize WiFi - try client mode first, fall back to AP if needed
    Serial.println("\n>>> Loading WiFi client credentials...");
    load_wifi_client_credentials();

    bool client_connected = false;
    if (strlen(wifi_client_ssid) > 0) {
        client_connected = setup_wifi_client();
    }

    // If client mode failed or not configured, start AP mode
    if (!client_connected) {
        setup_wifi_ap();
    } else {
        Serial.println(">>> WiFi client connected - AP mode disabled");
    }

    setup_web_server();
    setup_redirect_server();

    // Initialize Modbus
    setup_modbus();

    // Initialize registers with default values
    holding_regs.sequential_counter = 0;
    holding_regs.random_number = random(65535);
    holding_regs.cpu_cores = 2;

    // Get MAC address for serial number
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    input_regs.slave_id = modbus_slave_id;
    // Use last 4 bytes of MAC address as 32-bit serial number
    input_regs.serial_hi = (mac[2] << 8) | mac[3];
    input_regs.serial_lo = (mac[4] << 8) | mac[5];
    input_regs.sw_release = FIRMWARE_VERSION;
    input_regs.quartz_freq = 14750;  // 147.50 Hz

    Serial.printf("Device Serial Number: 0x%04X%04X (from MAC: %02X:%02X:%02X:%02X:%02X:%02X)\n",
                  input_regs.serial_hi, input_regs.serial_lo,
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    Serial.println("Initialization complete!\n");
}

// ============================================================================
// ARDUINO LOOP
// ============================================================================

void loop() {
    static unsigned long last_update = 0;
    static unsigned long last_display_update = 0;
    static unsigned long last_sf6_update = 0;
    static unsigned long last_lorawan_uplink = 0;

    unsigned long now = millis();

    // Update registers every 2 seconds
    if (now - last_update >= 2000) {
        last_update = now;
        update_holding_registers();
    }

    // Update SF6 sensor emulation every 3 seconds
    if (now - last_sf6_update >= 3000) {
        last_sf6_update = now;
        update_input_registers();
    }

    // Update display every 30 seconds
    if (now - last_display_update >= 30000) {
        last_display_update = now;
        update_display();
    }

    // Send LoRaWAN uplink every 5 minutes (if joined)
    if (lorawan_joined && (now - last_lorawan_uplink >= 300000)) {
        last_lorawan_uplink = now;
        send_lorawan_uplink();
    }

    // Handle WiFi AP timeout (only if not connected as client)
    // If connected to a WiFi network, keep WiFi active indefinitely
    if (!wifi_client_connected && wifi_ap_active && (now - wifi_start_time >= WIFI_TIMEOUT_MS)) {
        Serial.println("WiFi AP timeout - shutting down");
        WiFi.mode(WIFI_OFF);
        wifi_ap_active = false;
    }

    // Update WiFi client connection status
    if (wifi_client_connected) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi client disconnected - attempting reconnect...");
            wifi_client_connected = false;
            // Try to reconnect
            if (setup_wifi_client()) {
                Serial.println("WiFi client reconnected");
            } else {
                // Reconnection failed - start AP mode
                Serial.println("WiFi client reconnection failed - starting AP mode");
                setup_wifi_ap();
            }
        }
    }

    // Handle HTTPS server requests (both in AP mode and client mode)
    if (wifi_ap_active || wifi_client_connected) {
        server.loop();
        redirectServer.loop();
    }

    // Handle Modbus requests
    mb.task();
    yield();  // Allow other tasks to run

    delay(10);
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

void setup_display() {
    Serial.println("Initializing E-Ink display...");

    // The Heltec library handles VextOn() and pin setup automatically
    // Set display rotation using configured value
    display.setRotation(DISPLAY_ROTATION);

    // Show startup screen with simple graphics (inverted colors)
    display.clear();

    // Fill background black
    display.fillRect(0, 0, 296, 128, BLACK);

    // Draw border (white)
    display.drawRect(0, 0, 296, 128, WHITE);

    // Draw title box
    display.drawLine(0, 30, 295, 30, WHITE);

    // Draw some rectangles to show it's working (white on black)
    display.fillRect(10, 40, 50, 20, WHITE);  // Filled box
    display.drawRect(70, 40, 50, 20, WHITE);  // Outline box
    display.fillRect(130, 40, 50, 20, WHITE); // Filled box

    // Draw diagonal lines in bottom area
    display.drawLine(10, 70, 60, 110, WHITE);
    display.drawLine(70, 70, 120, 110, WHITE);
    display.drawLine(130, 70, 180, 110, WHITE);

    display.update();

    Serial.println("E-Ink display initialized!");
    Serial.println("Display shows: Border, boxes, and diagonal lines");
}

// Simple 5x7 font for letters and digits
const uint8_t font5x7[][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A (index 10)
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x00, 0x00, 0x00, 0x00}, // Space (index 36)
    {0x00, 0x00, 0x60, 0x60, 0x00}, // . (period)
    {0x00, 0x36, 0x36, 0x00, 0x00}, // : (colon)
    {0x08, 0x08, 0x08, 0x08, 0x08}, // - (minus/hyphen, index 39)
};

void draw_char(int x, int y, char c, int scale = 1) {
    int index = -1;
    if (c >= '0' && c <= '9') {
        index = c - '0';
    } else if (c >= 'A' && c <= 'Z') {
        index = c - 'A' + 10;
    } else if (c >= 'a' && c <= 'z') {
        index = c - 'a' + 10;  // Same as uppercase
    } else if (c == ' ') {
        index = 36;
    } else if (c == '.') {
        index = 37;
    } else if (c == ':') {
        index = 38;
    } else if (c == '-') {
        index = 39;
    }

    if (index >= 0 && index < 40) {
        for (int col = 0; col < 5; col++) {
            uint8_t line = font5x7[index][col];
            for (int row = 0; row < 7; row++) {
                if (line & (1 << row)) {
                    // Draw scaled pixel (scale x scale rectangle)
                    display.fillRect(x + col * scale, y + row * scale, scale, scale, WHITE);
                }
            }
        }
    }
}

void draw_text(int x, int y, const char* text, int scale = 1) {
    int cursor_x = x;
    for (int i = 0; text[i] != '\0'; i++) {
        draw_char(cursor_x, y, text[i], scale);
        cursor_x += (5 * scale) + scale;  // 5 pixels wide * scale + spacing
    }
}

void draw_number(int x, int y, float value, int decimals, int scale = 1) {
    char buffer[12];
    if (decimals == 0) {
        snprintf(buffer, sizeof(buffer), "%.0f", value);
    } else if (decimals == 1) {
        snprintf(buffer, sizeof(buffer), "%.1f", value);
    } else {
        snprintf(buffer, sizeof(buffer), "%.2f", value);
    }
    draw_text(x, y, buffer, scale);
}

void update_display() {
    display.clear();

    // Fill background with black for inverted display
    display.fillRect(0, 0, 296, 128, BLACK);

    // Draw border (white on black)
    display.drawRect(0, 0, 296, 128, WHITE);

    // WiFi status (top right corner) - scale 1
    draw_text(130, 2, "W:", 1);
    if (wifi_client_connected) {
        draw_text(142, 2, "STA", 1);
    } else if (holding_regs.wifi_enabled) {
        draw_number(142, 2, holding_regs.wifi_clients, 0, 1);
    } else {
        draw_text(142, 2, "OFF", 1);
    }

    // LoRaWAN status (top right corner) - scale 1
    draw_text(175, 2, "L:", 1);
    if (lorawan_joined) {
        draw_text(187, 2, "OK", 1);
        // Show RSSI and SNR from last uplink
        char lora_info[30];
        snprintf(lora_info, sizeof(lora_info), "R:%d S:%.1f", lorawan_last_rssi, lorawan_last_snr);
        draw_text(210, 2, lora_info, 1);
    } else {
        draw_text(187, 2, "NO", 1);
    }

    // Title area - scale 1 for compactness
    draw_text(3, 2, "SF6 Monitor", 1);
    display.drawLine(0, 11, 295, 11, WHITE);

    // Convert temperature to Celsius
    float temp_celsius = input_regs.sf6_temperature / 10.0 - 273.15;

    // Row 1: Density - scale 1
    draw_text(3, 14, "Density:", 1);
    draw_number(150, 14, input_regs.sf6_density / 100.0, 2, 1);
    draw_text(210, 14, "kg/m3", 1);

    // Row 2: Pressure @20C - scale 1
    draw_text(3, 25, "Press@20C:", 1);
    draw_number(150, 25, input_regs.sf6_pressure_20c / 10.0, 1, 1);
    draw_text(210, 25, "kPa", 1);

    // Row 3: Temperature - scale 1
    draw_text(3, 36, "Temp:", 1);
    draw_number(150, 36, temp_celsius, 1, 1);
    draw_text(210, 36, "C", 1);

    // Row 4: Pressure Variance - scale 1
    draw_text(3, 47, "Press Var:", 1);
    draw_number(150, 47, input_regs.sf6_pressure_var / 10.0, 1, 1);
    draw_text(210, 47, "kPa", 1);

    // Row 5: Quartz Frequency - scale 1 (divided by 100)
    draw_text(3, 58, "Quartz:", 1);
    draw_number(150, 58, input_regs.quartz_freq / 100.0, 2, 1);
    draw_text(210, 58, "Hz", 1);

    // Divider line
    display.drawLine(0, 68, 295, 68, WHITE);

    // Row 6: Modbus info - scale 1
    draw_text(3, 71, "Modbus ID:", 1);
    draw_number(70, 71, modbus_slave_id, 0, 1);
    draw_text(100, 71, "Req:", 1);
    draw_number(130, 71, holding_regs.sequential_counter % 10000, 0, 1);

    // Row 7: WiFi/Network - scale 1
    draw_text(3, 82, "WiFi:", 1);
    if (wifi_client_connected) {
        // Client mode - show SSID and IP
        String ssid = WiFi.SSID();
        if (ssid.length() > 10) {
            ssid = ssid.substring(0, 10);
        }
        draw_text(35, 82, ssid.c_str(), 1);
        draw_text(110, 82, WiFi.localIP().toString().c_str(), 1);
    } else if (holding_regs.wifi_enabled) {
        // AP mode - show client count
        draw_text(35, 82, "AP", 1);
        draw_number(55, 82, holding_regs.wifi_clients, 0, 1);
        draw_text(75, 82, "clients", 1);
    } else {
        draw_text(35, 82, "OFF", 1);
    }

    // Row 8: LoRaWAN stats - scale 1
    draw_text(3, 93, "LoRa:", 1);
    if (lorawan_joined) {
        draw_text(35, 93, "JOINED", 1);
        draw_text(80, 93, "TX:", 1);
        draw_number(100, 93, lorawan_uplink_count, 0, 1);
    } else {
        draw_text(35, 93, "NOT JOINED", 1);
    }

    // Display DevEUI (last 4 bytes as hex)
    char deveui_str[16];
    snprintf(deveui_str, sizeof(deveui_str), "EUI:%08X", (uint32_t)(devEUI & 0xFFFFFFFF));
    draw_text(150, 93, deveui_str, 1);

    // Row 9: System info - scale 1
    draw_text(3, 104, "Uptime:", 1);
    draw_number(45, 104, holding_regs.uptime_seconds, 0, 1);
    draw_text(85, 104, "s  Heap:", 1);
    draw_number(130, 104, holding_regs.free_heap_kb_low, 0, 1);
    draw_text(160, 104, "KB", 1);

    // Row 10: CPU/Temp/Version - scale 1
    draw_text(3, 115, "CPU:", 1);
    draw_number(30, 115, holding_regs.cpu_freq_mhz, 0, 1);
    draw_text(60, 115, "MHz  Temp:", 1);
    draw_number(120, 115, holding_regs.temperature_x10 / 10.0, 1, 1);
    draw_text(155, 115, "C  ", 1);

    // Version number at end of bottom line
    char version_str[10];
    snprintf(version_str, sizeof(version_str), "v%d.%02d", FIRMWARE_VERSION / 100, FIRMWARE_VERSION % 100);
    draw_text(250, 115, version_str, 1);

    display.update();

    Serial.println("Display updated - SF6 sensors with text labels");
    Serial.printf("  Density: %.2f kg/m3 (reg=%d)\n", input_regs.sf6_density/100.0, input_regs.sf6_density);
    Serial.printf("  Pressure: %.1f kPa (reg=%d)\n", input_regs.sf6_pressure_20c/10.0, input_regs.sf6_pressure_20c);
    Serial.printf("  Temperature: %.1fC (reg=%d)\n", temp_celsius, input_regs.sf6_temperature);
    Serial.printf("  Pressure Var: %.1f kPa (reg=%d)\n", input_regs.sf6_pressure_var/10.0, input_regs.sf6_pressure_var);
    if (wifi_client_connected) {
        Serial.printf("  WiFi: Client Mode - SSID: %s, IP: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    } else {
        Serial.printf("  WiFi: %s (%d clients)\n", holding_regs.wifi_enabled ? "AP Mode" : "OFF", holding_regs.wifi_clients);
    }
    Serial.printf("  Slave ID: %d, Counter: %d, Uptime: %d\n", modbus_slave_id, holding_regs.sequential_counter, holding_regs.uptime_seconds);
}

// ============================================================================
// REGISTER UPDATE FUNCTIONS
// ============================================================================

void update_holding_registers() {
    holding_regs.random_number = random(65535);
    holding_regs.uptime_seconds = millis() / 1000;

    uint32_t free_heap = ESP.getFreeHeap() / 1024;
    holding_regs.free_heap_kb_low = free_heap & 0xFFFF;
    holding_regs.free_heap_kb_high = (free_heap >> 16) & 0xFFFF;
    holding_regs.min_heap_kb = ESP.getMinFreeHeap() / 1024;

    holding_regs.cpu_freq_mhz = ESP.getCpuFreqMHz();
    holding_regs.task_count = uxTaskGetNumberOfTasks();
    holding_regs.temperature_x10 = (int16_t)(temperatureRead() * 10);
    holding_regs.wifi_enabled = (wifi_ap_active || wifi_client_connected) ? 1 : 0;
    holding_regs.wifi_clients = wifi_ap_active ? WiFi.softAPgetStationNum() : 0;
}

void update_input_registers() {
    // Emulate SF6 sensor with realistic drift using global base values

    portENTER_CRITICAL(&timerMux);

    // Add small random drift
    sf6_base_density += random(-10, 11) / 100.0;
    sf6_base_pressure += random(-50, 51) / 10.0;
    sf6_base_temperature += random(-5, 6) / 10.0;

    // Constrain to realistic ranges (0-60 kg/m3, 0-1100 kPa, 215-360 K)
    sf6_base_density = constrain(sf6_base_density, 0.0, 60.0);
    sf6_base_pressure = constrain(sf6_base_pressure, 0.0, 1100.0);
    sf6_base_temperature = constrain(sf6_base_temperature, 215.0, 360.0);

    input_regs.sf6_density = (uint16_t)(sf6_base_density * 100);
    input_regs.sf6_pressure_20c = (uint16_t)(sf6_base_pressure * 10);
    input_regs.sf6_temperature = (uint16_t)(sf6_base_temperature * 10);
    input_regs.sf6_pressure_var = input_regs.sf6_pressure_20c;

    portEXIT_CRITICAL(&timerMux);

    // Quartz frequency with minor variation
    input_regs.quartz_freq = 14750 + random(-50, 51);
}

// ============================================================================
// WIFI AND WEB SERVER FUNCTIONS
// ============================================================================

// Connect to WiFi client network
bool setup_wifi_client() {
    if (strlen(wifi_client_ssid) == 0) {
        Serial.println("No WiFi client credentials configured");
        return false;
    }

    Serial.println("\n========================================");
    Serial.println("Attempting WiFi Client Connection");
    Serial.println("========================================");
    Serial.printf("SSID: %s\n", wifi_client_ssid);
    Serial.println("========================================\n");

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_client_ssid, wifi_client_password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {  // 10 seconds timeout
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        wifi_client_connected = true;

        // Initialize mDNS
        if (MDNS.begin("stationsdata")) {
            Serial.println("mDNS responder started: stationsdata.local");
            MDNS.addService("https", "tcp", 443);
            MDNS.addService("http", "tcp", 80);
        } else {
            Serial.println("Error starting mDNS");
        }

        Serial.println("\n========================================");
        Serial.println("WiFi Client Connected!");
        Serial.println("========================================");
        Serial.printf("SSID:       %s\n", WiFi.SSID().c_str());
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("mDNS:       stationsdata.local\n");
        Serial.printf("RSSI:       %d dBm\n", WiFi.RSSI());
        Serial.println("========================================\n");
        return true;
    } else {
        Serial.println("\n========================================");
        Serial.println("WiFi Client Connection Failed");
        Serial.println("========================================");

        // Get detailed failure information
        wl_status_t status = WiFi.status();
        Serial.printf("WiFi Status Code: %d\n", status);

        switch(status) {
            case WL_NO_SSID_AVAIL:
                Serial.println("Reason: SSID not found");
                Serial.println("  - Check if SSID is correct");
                Serial.println("  - Ensure AP is in range");
                Serial.println("  - Check if AP is broadcasting SSID");
                break;
            case WL_CONNECT_FAILED:
                Serial.println("Reason: Connection failed");
                Serial.println("  - Wrong password");
                Serial.println("  - AP rejected connection");
                Serial.println("  - Authentication error");
                break;
            case WL_CONNECTION_LOST:
                Serial.println("Reason: Connection lost");
                break;
            case WL_DISCONNECTED:
                Serial.println("Reason: Disconnected");
                Serial.println("  - Timed out waiting for connection");
                Serial.println("  - Check password and SSID");
                break;
            case WL_IDLE_STATUS:
                Serial.println("Reason: Still in idle status");
                break;
            default:
                Serial.printf("Reason: Unknown (status %d)\n", status);
                break;
        }

        Serial.printf("SSID attempted: %s\n", wifi_client_ssid);
        Serial.printf("Password length: %d characters\n", strlen(wifi_client_password));
        Serial.println("========================================\n");

        WiFi.mode(WIFI_OFF);
        return false;
    }
}

void setup_wifi_ap() {
    Serial.println("Starting WiFi AP...");

    // Load or generate password based on mode
    #if MODE_PRODUCTION
        Serial.println(">>> Production mode: Loading/generating secure password");
        load_wifi_password();
    #else
        Serial.println(">>> Development mode: Using default password");
        strcpy(wifi_password, "modbus123");
    #endif

    // Get MAC address and extract last 4 hex digits for unique SSID
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char mac_suffix[5];
    sprintf(mac_suffix, "%02X%02X", mac[4], mac[5]);

    // Create unique SSID: ESP32-Modbus-Config-XXXX
    String ssid = "ESP32-Modbus-Config-" + String(mac_suffix);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid.c_str(), wifi_password, 1, 0, 4);

    wifi_ap_active = true;
    wifi_start_time = millis();

    // Initialize mDNS
    if (MDNS.begin("stationsdata")) {
        Serial.println("mDNS responder started: stationsdata.local");
        MDNS.addService("https", "tcp", 443);
        MDNS.addService("http", "tcp", 80);
    } else {
        Serial.println("Error starting mDNS");
    }

    Serial.println("\n========================================");
    Serial.println("WiFi AP Started");
    Serial.println("========================================");
    Serial.print("SSID:     ");
    Serial.println(ssid);
    Serial.print("Password: ");
    Serial.println(wifi_password);
    Serial.print("IP:       ");
    Serial.println(WiFi.softAPIP());
    Serial.print("mDNS:     ");
    Serial.println("stationsdata.local");
    Serial.printf("Timeout:  %d minutes\n", WIFI_TIMEOUT_MS / 60000);
    Serial.println("========================================\n");

    // Display SSID and password on screen in production mode
    #if MODE_PRODUCTION
        display_password_on_screen(ssid.c_str());
        delay(20000);  // Show credentials for 20 seconds
    #endif
}

void setup_web_server() {
    // Register GET handlers
    ResourceNode * nodeRoot = new ResourceNode("/", "GET", &handle_root);
    ResourceNode * nodeStats = new ResourceNode("/stats", "GET", &handle_stats);
    ResourceNode * nodeRegisters = new ResourceNode("/registers", "GET", &handle_registers);
    ResourceNode * nodeLoRaWAN = new ResourceNode("/lorawan", "GET", &handle_lorawan);
    ResourceNode * nodeWiFi = new ResourceNode("/wifi", "GET", &handle_wifi);
    ResourceNode * nodeWiFiScan = new ResourceNode("/wifi/scan", "GET", &handle_wifi_scan);
    ResourceNode * nodeWiFiStatus = new ResourceNode("/wifi/status", "GET", &handle_wifi_status);
    ResourceNode * nodeSecurity = new ResourceNode("/security", "GET", &handle_security);

    // Register POST handlers
    ResourceNode * nodeConfig = new ResourceNode("/config", "POST", &handle_config);
    ResourceNode * nodeLoRaWANConfig = new ResourceNode("/lorawan/config", "POST", &handle_lorawan_config);
    ResourceNode * nodeWiFiConnect = new ResourceNode("/wifi/connect", "POST", &handle_wifi_connect);
    ResourceNode * nodeSecurityUpdate = new ResourceNode("/security/update", "POST", &handle_security_update);
    ResourceNode * nodeDebugUpdate = new ResourceNode("/security/debug", "POST", &handle_debug_update);
    ResourceNode * nodeSF6Update = new ResourceNode("/sf6/update", "GET", &handle_sf6_update);
    ResourceNode * nodeSF6Reset = new ResourceNode("/sf6/reset", "GET", &handle_sf6_reset);
    ResourceNode * nodeEnableAuth = new ResourceNode("/security/enable", "GET", &handle_enable_auth);
    ResourceNode * nodeReboot = new ResourceNode("/reboot", "GET", &handle_reboot);

    server.registerNode(nodeRoot);
    server.registerNode(nodeStats);
    server.registerNode(nodeRegisters);
    server.registerNode(nodeConfig);
    server.registerNode(nodeLoRaWAN);
    server.registerNode(nodeLoRaWANConfig);
    server.registerNode(nodeWiFi);
    server.registerNode(nodeWiFiScan);
    server.registerNode(nodeWiFiConnect);
    server.registerNode(nodeWiFiStatus);
    server.registerNode(nodeSecurity);
    server.registerNode(nodeSecurityUpdate);
    server.registerNode(nodeDebugUpdate);
    server.registerNode(nodeSF6Update);
    server.registerNode(nodeSF6Reset);
    server.registerNode(nodeEnableAuth);
    server.registerNode(nodeReboot);

    server.start();
    Serial.println("HTTPS server started on port 443");
}

void setup_redirect_server() {
    // Register catch-all handler for HTTP to HTTPS redirect
    ResourceNode * nodeRedirect = new ResourceNode("/*", "GET", &handle_http_redirect);
    ResourceNode * nodeRedirectPost = new ResourceNode("/*", "POST", &handle_http_redirect);

    redirectServer.registerNode(nodeRedirect);
    redirectServer.registerNode(nodeRedirectPost);

    redirectServer.start();
    Serial.println("HTTP redirect server started on port 80");
}

void handle_http_redirect(HTTPRequest * req, HTTPResponse * res) {
    // Get the requested path
    std::string path = req->getRequestString();

    // Build HTTPS URL
    String httpsUrl = "https://";

    // Try to get Host header, otherwise use IP or mDNS name
    std::string host = req->getHeader("Host");
    if (!host.empty()) {
        // Remove port number if present
        String hostStr = String(host.c_str());
        int colonIdx = hostStr.indexOf(':');
        if (colonIdx > 0) {
            hostStr = hostStr.substring(0, colonIdx);
        }
        httpsUrl += hostStr;
    } else {
        // Fallback to mDNS name
        httpsUrl += "stationsdata.local";
    }

    // Add the original path
    httpsUrl += String(path.c_str());

    // Send 301 Moved Permanently redirect
    res->setStatusCode(301);
    res->setHeader("Location", httpsUrl.c_str());
    res->setHeader("Content-Type", "text/html");
    res->println("<!DOCTYPE html><html><body>");
    res->println("<h1>Redirecting to HTTPS...</h1>");
    res->print("<p>Please use: <a href=\"");
    res->print(httpsUrl.c_str());
    res->print("\">");
    res->print(httpsUrl.c_str());
    res->println("</a></p>");
    res->println("</body></html>");

    if (debug_https_enabled) {
        Serial.printf("HTTP->HTTPS redirect: %s\n", httpsUrl.c_str());
    }
}

void handle_root(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    String html = "<!DOCTYPE html><html><head><title>Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += ".container{max-width:900px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
    html += "h1{color:#2c3e50;margin-top:0;border-bottom:3px solid #3498db;padding-bottom:15px;}";
    html += "h2{color:#34495e;margin-top:30px;}";
    html += ".nav{background:#3498db;padding:15px;margin:-30px -30px 30px -30px;border-radius:10px 10px 0 0;display:flex;align-items:center;}";
    html += ".nav a{color:white;text-decoration:none;padding:10px 20px;margin:0 5px;background:#2980b9;border-radius:5px;display:inline-block;}";
    html += ".nav a:hover{background:#21618c;}";
    html += ".nav .reboot{margin-left:auto;background:#e74c3c;}";
    html += ".nav .reboot:hover{background:#c0392b;}";
    html += ".card{background:#ecf0f1;padding:20px;margin:15px 0;border-radius:8px;border-left:4px solid #3498db;}";
    html += ".info{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:15px;margin:20px 0;}";
    html += ".info-item{background:#fff;padding:15px;border-radius:5px;border:1px solid #ddd;}";
    html += ".info-label{font-size:12px;color:#7f8c8d;text-transform:uppercase;margin-bottom:5px;}";
    html += ".info-value{font-size:24px;font-weight:bold;color:#2c3e50;}";
    html += "form{background:#ecf0f1;padding:20px;border-radius:8px;}";
    html += "label{display:block;margin-bottom:8px;color:#2c3e50;font-weight:600;}";
    html += "input[type=number]{width:100%;padding:10px;border:2px solid #bdc3c7;border-radius:5px;font-size:16px;box-sizing:border-box;}";
    html += "input[type=number]:focus{border-color:#3498db;outline:none;}";
    html += "input[type=submit]{background:#27ae60;color:white;padding:12px 30px;border:none;border-radius:5px;font-size:16px;cursor:pointer;margin-top:10px;}";
    html += "input[type=submit]:hover{background:#229954;}";
    html += ".footer{text-align:center;margin-top:30px;color:#7f8c8d;font-size:14px;}";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<div class='nav'>";
    html += "<a href='/'>Home</a>";
    html += "<a href='/stats'>Statistics</a>";
    html += "<a href='/registers'>Registers</a>";
    html += "<a href='/lorawan'>LoRaWAN</a>";
    html += "<a href='/wifi'>WiFi</a>";
    html += "<a href='/security'>Security</a>";
    html += "<a href='/reboot' class='reboot' onclick='return confirm(\"Are you sure you want to reboot the device?\");'>Reboot</a>";
    html += "</div>";
    html += "<h1>Vision Master E290</h1>";
    html += "<div class='card'><strong>Status:</strong> System Running | <strong>Uptime:</strong> " + String(millis() / 1000) + " seconds</div>";

    html += "<div class='info'>";
    html += "<div class='info-item'><div class='info-label'>Modbus Slave ID</div><div class='info-value'>" + String(modbus_slave_id) + "</div></div>";
    html += "<div class='info-item'><div class='info-label'>Modbus Requests</div><div class='info-value'>" + String(modbus_request_count) + "</div></div>";

    // WiFi status - show different info based on mode
    if (wifi_client_connected) {
        html += "<div class='info-item'><div class='info-label'>WiFi Mode</div><div class='info-value' style='font-size:16px;'>Client<br><small style='font-size:12px;color:#7f8c8d;'>" + WiFi.SSID() + "<br>" + WiFi.localIP().toString() + "</small></div></div>";
    } else if (wifi_ap_active) {
        html += "<div class='info-item'><div class='info-label'>WiFi Mode</div><div class='info-value' style='font-size:16px;'>AP Mode<br><small style='font-size:12px;color:#7f8c8d;'>" + String(WiFi.softAPgetStationNum()) + " clients</small></div></div>";
    } else {
        html += "<div class='info-item'><div class='info-label'>WiFi Mode</div><div class='info-value'>OFF</div></div>";
    }

    html += "<div class='info-item'><div class='info-label'>LoRaWAN Status</div><div class='info-value'>" + String(lorawan_joined ? "JOINED" : "NOT JOINED") + "</div></div>";
    html += "<div class='info-item'><div class='info-label'>LoRa Uplinks</div><div class='info-value'>" + String(lorawan_uplink_count) + "</div></div>";
    html += "</div>";

    html += "<h2>Configuration</h2>";
    html += "<form action='/config' method='POST'>";
    html += "<label>Modbus Slave ID:</label>";
    html += "<input type='number' name='slave_id' min='1' max='247' value='" + String(modbus_slave_id) + "' required>";
    html += "<p style='font-size:12px;color:#7f8c8d;margin:5px 0 15px 0;'>Valid range: 1-247 (0=broadcast, 248-255=reserved)</p>";
    html += "<input type='submit' value='Save Configuration'>";
    html += "</form>";

    html += "<div class='footer'>ESP32-S3R8 | Modbus RTU + E-Ink Display | Arduino Framework</div>";
    html += "</div></body></html>";

    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());
}

void handle_stats(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    String html = "<!DOCTYPE html><html><head><title>Statistics - Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta http-equiv='refresh' content='5'>";  // Auto-refresh every 5 seconds
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += ".container{max-width:1100px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
    html += "h1{color:#2c3e50;margin-top:0;border-bottom:3px solid #3498db;padding-bottom:15px;}";
    html += "h2{color:#34495e;margin-top:30px;padding:10px;background:#ecf0f1;border-radius:5px;}";
    html += ".nav{background:#3498db;padding:15px;margin:-30px -30px 30px -30px;border-radius:10px 10px 0 0;display:flex;align-items:center;}";
    html += ".nav a{color:white;text-decoration:none;padding:10px 20px;margin:0 5px;background:#2980b9;border-radius:5px;display:inline-block;}";
    html += ".nav a:hover{background:#21618c;}";
    html += ".nav .reboot{margin-left:auto;background:#e74c3c;}";
    html += ".nav .reboot:hover{background:#c0392b;}";
    html += "table{border-collapse:collapse;width:100%;margin:15px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1);}";
    html += "th{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;padding:12px;text-align:left;font-weight:600;}";
    html += "td{border:1px solid #e0e0e0;padding:10px;background:white;}";
    html += "tr:nth-child(even) td{background:#f8f9fa;}";
    html += "tr:hover td{background:#e3f2fd;}";
    html += ".value{font-weight:bold;color:#2c3e50;}";
    html += ".refresh{text-align:center;color:#7f8c8d;font-size:12px;margin-top:20px;}";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<div class='nav'>";
    html += "<a href='/'>Home</a>";
    html += "<a href='/stats'>Statistics</a>";
    html += "<a href='/registers'>Registers</a>";
    html += "<a href='/lorawan'>LoRaWAN</a>";
    html += "<a href='/wifi'>WiFi</a>";
    html += "<a href='/security'>Security</a>";
    html += "<a href='/reboot' class='reboot' onclick='return confirm(\"Are you sure you want to reboot the device?\");'>Reboot</a>";
    html += "</div>";
    html += "<h1>System Statistics</h1>";

    // Modbus Communication
    html += "<h2>Modbus Communication</h2>";
    html += "<table><tr><th>Metric</th><th>Value</th><th>Description</th></tr>";
    html += "<tr><td>Total Requests</td><td class='value'>" + String(modbus_request_count) + "</td><td>Total Modbus requests received</td></tr>";
    html += "<tr><td>Read Operations</td><td class='value'>" + String(modbus_read_count) + "</td><td>Number of read operations</td></tr>";
    html += "<tr><td>Write Operations</td><td class='value'>" + String(modbus_write_count) + "</td><td>Number of write operations</td></tr>";
    html += "<tr><td>Error Count</td><td class='value'>" + String(modbus_error_count) + "</td><td>Communication errors</td></tr>";
    html += "</table>";

    // System Information
    html += "<h2>System Information</h2>";
    html += "<table><tr><th>Metric</th><th>Value</th><th>Description</th></tr>";
    html += "<tr><td>Uptime</td><td class='value'>" + String(millis() / 1000) + " seconds</td><td>System uptime since last boot</td></tr>";
    html += "<tr><td>Free Heap</td><td class='value'>" + String(ESP.getFreeHeap() / 1024) + " KB</td><td>Available RAM memory</td></tr>";
    html += "<tr><td>Min Free Heap</td><td class='value'>" + String(ESP.getMinFreeHeap() / 1024) + " KB</td><td>Minimum free heap since boot</td></tr>";
    html += "<tr><td>Temperature</td><td class='value'>" + String(temperatureRead(), 1) + " C</td><td>Internal CPU temperature</td></tr>";
    html += "<tr><td>WiFi Clients</td><td class='value'>" + String(WiFi.softAPgetStationNum()) + "</td><td>Connected WiFi clients</td></tr>";
    html += "</table>";

    // Device Information
    html += "<h2>Device Information</h2>";
    html += "<table><tr><th>Metric</th><th>Value</th><th>Description</th></tr>";
    html += "<tr><td>Modbus Slave ID</td><td class='value'>" + String(modbus_slave_id) + "</td><td>Current Modbus slave address</td></tr>";
    html += "<tr><td>CPU Frequency</td><td class='value'>" + String(ESP.getCpuFreqMHz()) + " MHz</td><td>Current CPU clock speed</td></tr>";
    html += "<tr><td>CPU Cores</td><td class='value'>2</td><td>Number of CPU cores</td></tr>";
    html += "<tr><td>FreeRTOS Tasks</td><td class='value'>" + String(uxTaskGetNumberOfTasks()) + "</td><td>Active FreeRTOS tasks</td></tr>";
    html += "<tr><td>Sequential Counter</td><td class='value'>" + String(holding_regs.sequential_counter) + "</td><td>Holding register 0 value</td></tr>";
    html += "</table>";

    // LoRaWAN Communication
    html += "<h2>LoRaWAN Communication</h2>";
    html += "<table><tr><th>Metric</th><th>Value</th><th>Description</th></tr>";
    html += "<tr><td>Network Status</td><td class='value'>" + String(lorawan_joined ? "JOINED" : "NOT JOINED") + "</td><td>LoRaWAN network connection status</td></tr>";
    html += "<tr><td>Total Uplinks</td><td class='value'>" + String(lorawan_uplink_count) + "</td><td>Number of uplinks sent</td></tr>";
    html += "<tr><td>Total Downlinks</td><td class='value'>" + String(lorawan_downlink_count) + "</td><td>Number of downlinks received</td></tr>";
    html += "<tr><td>Last RSSI</td><td class='value'>" + String(lorawan_last_rssi) + " dBm</td><td>Signal strength of last transmission</td></tr>";
    html += "<tr><td>Last SNR</td><td class='value'>" + String(lorawan_last_snr, 1) + " dB</td><td>Signal-to-noise ratio</td></tr>";
    html += "</table>";

    html += "<div class='refresh'>Auto-refreshing every 5 seconds</div>";
    html += "</div></body></html>";

    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());
}

void handle_registers(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    String html = "<!DOCTYPE html><html><head><title>Registers - Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta http-equiv='refresh' content='5'>";  // Auto-refresh every 5 seconds (paused during slider use)
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += ".container{max-width:1100px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
    html += "h1{color:#2c3e50;margin-top:0;border-bottom:3px solid #3498db;padding-bottom:15px;}";
    html += "h2{color:#34495e;margin-top:30px;padding:10px;background:#ecf0f1;border-radius:5px;}";
    html += ".nav{background:#3498db;padding:15px;margin:-30px -30px 30px -30px;border-radius:10px 10px 0 0;display:flex;align-items:center;}";
    html += ".nav a{color:white;text-decoration:none;padding:10px 20px;margin:0 5px;background:#2980b9;border-radius:5px;display:inline-block;}";
    html += ".nav a:hover{background:#21618c;}";
    html += ".nav .reboot{margin-left:auto;background:#e74c3c;}";
    html += ".nav .reboot:hover{background:#c0392b;}";
    html += "table{border-collapse:collapse;width:100%;margin:15px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1);}";
    html += "th{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;padding:12px;text-align:left;font-weight:600;}";
    html += "td{border:1px solid #e0e0e0;padding:10px;background:white;}";
    html += "tr:nth-child(even) td{background:#f8f9fa;}";
    html += "tr:hover td{background:#e3f2fd;}";
    html += ".value{font-weight:bold;color:#2c3e50;}";
    html += ".scaled{color:#27ae60;font-weight:600;}";
    html += ".refresh{text-align:center;color:#7f8c8d;font-size:12px;margin-top:20px;}";
    html += ".control-panel{background:#f8f9fa;padding:20px;border-radius:8px;margin:20px 0;border:2px solid #e0e0e0;}";
    html += ".control-panel h3{color:#2c3e50;margin-top:0;}";
    html += ".input-group{display:grid;grid-template-columns:150px 1fr 100px;gap:10px;align-items:center;margin:15px 0;}";
    html += ".input-group label{color:#555;font-weight:600;}";
    html += ".input-group input[type=number]{padding:8px;border:2px solid #ddd;border-radius:5px;font-size:14px;width:100%;}";
    html += ".input-group input[type=number]:focus{border-color:#3498db;outline:none;}";
    html += ".input-group .unit{color:#7f8c8d;font-size:13px;}";
    html += ".submit-btn{background:#3498db;color:white;padding:12px 30px;border:none;border-radius:5px;font-size:16px;font-weight:600;cursor:pointer;margin-top:15px;}";
    html += ".submit-btn:hover{background:#2980b9;}";
    html += ".submit-btn:active{background:#21618c;}";
    html += ".update-status{margin-top:10px;padding:10px;border-radius:5px;display:none;}";
    html += ".update-status.success{background:#d4edda;color:#155724;display:block;}";
    html += ".update-status.error{background:#f8d7da;color:#721c24;display:block;}";
    html += "</style>";
    html += "<script>";
    html += "function submitSF6Values() {";
    html += "  var density = parseFloat(document.getElementById('density-input').value);";
    html += "  var pressure = parseFloat(document.getElementById('pressure-input').value);";
    html += "  var temperature = parseFloat(document.getElementById('temperature-input').value);";
    html += "  var status = document.getElementById('update-status');";
    html += "  var errors = [];";
    html += "  if (isNaN(density) || density < 0 || density > 60) errors.push('Density must be 0-60 kg/m3');";
    html += "  if (isNaN(pressure) || pressure < 0 || pressure > 1100) errors.push('Pressure must be 0-1100 kPa');";
    html += "  if (isNaN(temperature) || temperature < 215 || temperature > 360) errors.push('Temperature must be 215-360 K');";
    html += "  if (errors.length > 0) {";
    html += "    status.className = 'update-status error';";
    html += "    status.textContent = errors.join('; ');";
    html += "    setTimeout(function() { status.style.display = 'none'; }, 4000);";
    html += "    return false;";
    html += "  }";
    html += "  var densityRaw = Math.round(density * 100);";
    html += "  var pressureRaw = Math.round(pressure * 10);";
    html += "  var temperatureRaw = Math.round(temperature * 10);";
    html += "  var img = new Image();";
    html += "  img.onload = function() {";
    html += "    status.className = 'update-status success';";
    html += "    status.textContent = 'Values updated successfully!';";
    html += "    setTimeout(function() { location.reload(); }, 2000);";
    html += "  };";
    html += "  img.onerror = function() {";
    html += "    status.className = 'update-status success';";
    html += "    status.textContent = 'Values updated successfully!';";
    html += "    setTimeout(function() { location.reload(); }, 2000);";
    html += "  };";
    html += "  img.src = '/sf6/update?density=' + densityRaw + '&pressure=' + pressureRaw + '&temperature=' + temperatureRaw + '&t=' + Date.now();";
    html += "  status.className = 'update-status success';";
    html += "  status.textContent = 'Updating values...';";
    html += "  return false;";
    html += "}";
    html += "function resetSF6Values() {";
    html += "  if (!confirm('Reset SF6 values to defaults?\\n\\nDensity: 25.0 kg/m³\\nPressure: 550.0 kPa\\nTemperature: 293.0 K')) return;";
    html += "  var img = new Image();";
    html += "  var status = document.getElementById('update-status');";
    html += "  img.onload = function() {";
    html += "    status.className = 'update-status success';";
    html += "    status.textContent = 'Values reset to defaults!';";
    html += "    setTimeout(function() { location.reload(); }, 2000);";
    html += "  };";
    html += "  img.onerror = function() {";
    html += "    status.className = 'update-status success';";
    html += "    status.textContent = 'Values reset to defaults!';";
    html += "    setTimeout(function() { location.reload(); }, 2000);";
    html += "  };";
    html += "  img.src = '/sf6/reset?t=' + Date.now();";
    html += "  status.className = 'update-status success';";
    html += "  status.textContent = 'Resetting to defaults...';";
    html += "}";
    html += "</script>";
    html += "</head><body>";
    html += "<div class='container'>";
    html += "<div class='nav'>";
    html += "<a href='/'>Home</a>";
    html += "<a href='/stats'>Statistics</a>";
    html += "<a href='/registers'>Registers</a>";
    html += "<a href='/lorawan'>LoRaWAN</a>";
    html += "<a href='/wifi'>WiFi</a>";
    html += "<a href='/security'>Security</a>";
    html += "<a href='/reboot' class='reboot' onclick='return confirm(\"Are you sure you want to reboot the device?\");'>Reboot</a>";
    html += "</div>";
    html += "<h1>Modbus Registers</h1>";

    html += "<div class='control-panel'>";
    html += "<h3>SF6 Manual Control</h3>";
    html += "<div style='background:#fff3cd;border:1px solid #ffc107;padding:12px;border-radius:5px;margin:10px 0;color:#856404;'>";
    html += "<strong>⚠ Warning:</strong> Using this feature may occasionally cause the device to reboot due to an SSL library bug. ";
    html += "Values are saved to flash and will persist across reboots.";
    html += "</div>";
    html += "<p style='color:#555;font-size:14px;margin:10px 0;'>Enter values and click Update to modify SF6 sensor emulation parameters.</p>";
    html += "<form onsubmit='return submitSF6Values();'>";
    html += "<div id='update-status' class='update-status'></div>";

    html += "<div class='input-group'>";
    html += "<label for='density-input'>Density:</label>";
    html += "<input type='number' id='density-input' step='0.01' min='0' max='60' value='" + String(input_regs.sf6_density / 100.0, 2) + "' required>";
    html += "<span class='unit'>kg/m³ (0-60)</span>";
    html += "</div>";

    html += "<div class='input-group'>";
    html += "<label for='pressure-input'>Pressure:</label>";
    html += "<input type='number' id='pressure-input' step='0.1' min='0' max='1100' value='" + String(input_regs.sf6_pressure_20c / 10.0, 1) + "' required>";
    html += "<span class='unit'>kPa (0-1100)</span>";
    html += "</div>";

    html += "<div class='input-group'>";
    html += "<label for='temperature-input'>Temperature:</label>";
    html += "<input type='number' id='temperature-input' step='0.1' min='215' max='360' value='" + String(input_regs.sf6_temperature / 10.0, 1) + "' required>";
    html += "<span class='unit'>K (215-360)</span>";
    html += "</div>";

    html += "<div style='display:flex;gap:10px;'>";
    html += "<button type='submit' class='submit-btn'>Update SF6 Values</button>";
    html += "<button type='button' class='submit-btn' style='background:#6c757d;' onclick='resetSF6Values()'>Reset to Defaults</button>";
    html += "</div>";
    html += "</form>";
    html += "</div>";

    // Holding Registers
    html += "<h2>Holding Registers (0-11) - Read/Write</h2>";
    html += "<table><tr><th>Address</th><th>Value</th><th>Hex</th><th>Description</th></tr>";
    html += "<tr><td>0</td><td class='value'>" + String(holding_regs.sequential_counter) + "</td><td>0x" + String(holding_regs.sequential_counter, HEX) + "</td><td>Sequential Counter</td></tr>";
    html += "<tr><td>1</td><td class='value'>" + String(holding_regs.random_number) + "</td><td>0x" + String(holding_regs.random_number, HEX) + "</td><td>Random Number</td></tr>";
    html += "<tr><td>2</td><td class='value'>" + String(holding_regs.uptime_seconds) + "</td><td>0x" + String(holding_regs.uptime_seconds, HEX) + "</td><td>Uptime (seconds)</td></tr>";

    uint32_t total_heap = ((uint32_t)holding_regs.free_heap_kb_high << 16) | holding_regs.free_heap_kb_low;
    html += "<tr><td>3</td><td class='value'>" + String(holding_regs.free_heap_kb_low) + "</td><td>0x" + String(holding_regs.free_heap_kb_low, HEX) + "</td><td>Free Heap (low word)</td></tr>";
    html += "<tr><td>4</td><td class='value'>" + String(holding_regs.free_heap_kb_high) + "</td><td>0x" + String(holding_regs.free_heap_kb_high, HEX) + "</td><td>Free Heap (high word) = <strong>" + String(total_heap) + " KB total</strong></td></tr>";
    html += "<tr><td>5</td><td class='value'>" + String(holding_regs.min_heap_kb) + "</td><td>0x" + String(holding_regs.min_heap_kb, HEX) + "</td><td>Min Free Heap (KB)</td></tr>";
    html += "<tr><td>6</td><td class='value'>" + String(holding_regs.cpu_freq_mhz) + "</td><td>0x" + String(holding_regs.cpu_freq_mhz, HEX) + "</td><td>CPU Frequency (MHz)</td></tr>";
    html += "<tr><td>7</td><td class='value'>" + String(holding_regs.task_count) + "</td><td>0x" + String(holding_regs.task_count, HEX) + "</td><td>FreeRTOS Tasks</td></tr>";
    html += "<tr><td>8</td><td class='value'>" + String(holding_regs.temperature_x10) + "</td><td>0x" + String(holding_regs.temperature_x10, HEX) + "</td><td>Temperature = <strong>" + String(holding_regs.temperature_x10 / 10.0, 1) + " C</strong></td></tr>";
    html += "<tr><td>9</td><td class='value'>" + String(holding_regs.cpu_cores) + "</td><td>0x" + String(holding_regs.cpu_cores, HEX) + "</td><td>CPU Cores</td></tr>";
    html += "<tr><td>10</td><td class='value'>" + String(holding_regs.wifi_enabled) + "</td><td>0x" + String(holding_regs.wifi_enabled, HEX) + "</td><td>WiFi AP Enabled</td></tr>";
    html += "<tr><td>11</td><td class='value'>" + String(holding_regs.wifi_clients) + "</td><td>0x" + String(holding_regs.wifi_clients, HEX) + "</td><td>WiFi Clients</td></tr>";
    html += "</table>";

    // Input Registers
    html += "<h2>Input Registers (0-8) - Read Only (SF6 Sensor)</h2>";
    html += "<table><tr><th>Address</th><th>Raw Value</th><th>Scaled Value</th><th>Description</th></tr>";
    html += "<tr><td>0</td><td class='value'>" + String(input_regs.sf6_density) + "</td><td class='scaled'>" + String(input_regs.sf6_density / 100.0, 2) + " kg/m3</td><td>SF6 Density</td></tr>";
    html += "<tr><td>1</td><td class='value'>" + String(input_regs.sf6_pressure_20c) + "</td><td class='scaled'>" + String(input_regs.sf6_pressure_20c / 10.0, 1) + " kPa</td><td>SF6 Pressure @20C</td></tr>";
    html += "<tr><td>2</td><td class='value'>" + String(input_regs.sf6_temperature) + "</td><td class='scaled'>" + String(input_regs.sf6_temperature / 10.0, 1) + " K (" + String(input_regs.sf6_temperature / 10.0 - 273.15, 1) + "C)</td><td>SF6 Temperature</td></tr>";
    html += "<tr><td>3</td><td class='value'>" + String(input_regs.sf6_pressure_var) + "</td><td class='scaled'>" + String(input_regs.sf6_pressure_var / 10.0, 1) + " kPa</td><td>SF6 Pressure Variance</td></tr>";
    html += "<tr><td>4</td><td class='value'>" + String(input_regs.slave_id) + "</td><td>-</td><td>Slave ID</td></tr>";

    uint32_t serial = ((uint32_t)input_regs.serial_hi << 16) | input_regs.serial_lo;
    html += "<tr><td>5</td><td class='value'>" + String(input_regs.serial_hi) + "</td><td class='scaled'>0x" + String(input_regs.serial_hi, HEX) + "</td><td>Serial Number (high)</td></tr>";
    html += "<tr><td>6</td><td class='value'>" + String(input_regs.serial_lo) + "</td><td class='scaled'>0x" + String(input_regs.serial_lo, HEX) + "</td><td>Serial Number (low) = <strong>0x" + String(serial, HEX) + "</strong></td></tr>";
    char version_buf[10];
    snprintf(version_buf, sizeof(version_buf), "v%d.%02d", input_regs.sw_release / 100, input_regs.sw_release % 100);
    html += "<tr><td>7</td><td class='value'>" + String(input_regs.sw_release) + "</td><td class='scaled'>" + String(version_buf) + "</td><td>Software Version</td></tr>";
    html += "<tr><td>8</td><td class='value'>" + String(input_regs.quartz_freq) + "</td><td class='scaled'>" + String(input_regs.quartz_freq / 100.0, 2) + " Hz</td><td>Quartz Frequency</td></tr>";
    html += "</table>";

    html += "<div class='refresh'>Auto-refreshing every 5 seconds</div>";
    html += "</div></body></html>";

    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());
}

void handle_sf6_update(HTTPRequest * req, HTTPResponse * res) {
    // WARNING: This endpoint has a known SSL bug that may cause device reboot
    // The crash occurs in ssl_lib.c:457 with null pointer dereference
    // User has been warned in the UI

    ResourceParameters * params = req->getParams();
    std::string value;

    portENTER_CRITICAL(&timerMux);

    if (params->getQueryParameter("density", value) && value.length() > 0) {
        uint16_t val = atoi(value.c_str());
        if (val <= 6000) {
            sf6_base_density = val / 100.0;
            input_regs.sf6_density = val;
        }
    }

    if (params->getQueryParameter("pressure", value) && value.length() > 0) {
        uint16_t val = atoi(value.c_str());
        if (val <= 11000) {
            sf6_base_pressure = val / 10.0;
            input_regs.sf6_pressure_20c = val;
            input_regs.sf6_pressure_var = val;
        }
    }

    if (params->getQueryParameter("temperature", value) && value.length() > 0) {
        uint16_t val = atoi(value.c_str());
        if (val >= 2150 && val <= 3600) {
            sf6_base_temperature = val / 10.0;
            input_regs.sf6_temperature = val;
        }
    }

    portEXIT_CRITICAL(&timerMux);

    // Save to NVS so values persist across reboots
    save_sf6_values();

    // Return 204 No Content (empty response, might be more stable)
    res->setStatusCode(204);
    res->setHeader("Cache-Control", "no-cache");
    // Don't call write() at all - 204 has no body
}

void handle_sf6_reset(HTTPRequest * req, HTTPResponse * res) {
    // Reset SF6 values to defaults
    // Same SSL bug warning applies as with handle_sf6_update

    Serial.println(">>> handle_sf6_reset called");

    portENTER_CRITICAL(&timerMux);

    // Reset to default values
    sf6_base_density = 25.0;       // kg/m3
    sf6_base_pressure = 550.0;     // kPa
    sf6_base_temperature = 293.0;  // K

    // Update registers immediately
    input_regs.sf6_density = (uint16_t)(sf6_base_density * 100);
    input_regs.sf6_pressure_20c = (uint16_t)(sf6_base_pressure * 10);
    input_regs.sf6_temperature = (uint16_t)(sf6_base_temperature * 10);
    input_regs.sf6_pressure_var = input_regs.sf6_pressure_20c;

    portEXIT_CRITICAL(&timerMux);

    Serial.printf(">>> Reset complete - Density: %u, Pressure: %u, Temp: %u\n",
                  input_regs.sf6_density, input_regs.sf6_pressure_20c, input_regs.sf6_temperature);

    // Save to NVS so defaults persist across reboots
    save_sf6_values();

    Serial.println(">>> SF6 values saved to NVS");

    // Return 204 No Content
    res->setStatusCode(204);
    res->setHeader("Cache-Control", "no-cache");
}

void handle_config(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    String html = "<!DOCTYPE html><html><head><title>Configuration - Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += ".container{max-width:600px;margin:50px auto;background:white;padding:40px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);text-align:center;}";
    html += "h1{color:#2c3e50;margin:10px 0;}";
    html += "p{color:#7f8c8d;font-size:18px;margin:20px 0;}";
    html += ".value{background:#ecf0f1;padding:15px;border-radius:5px;font-size:32px;font-weight:bold;color:#2c3e50;margin:20px 0;}";
    html += "a{display:inline-block;background:#3498db;color:white;padding:15px 30px;text-decoration:none;border-radius:5px;margin-top:20px;font-size:16px;}";
    html += "a:hover{background:#2980b9;}";
    html += "</style></head><body><div class='container'>";

    // Read POST body once
    String postBody = readPostBody(req);

    String slave_id_str;
    if (getPostParameterFromBody(postBody, "slave_id", slave_id_str)) {
        int new_id = slave_id_str.toInt();

        // Validate Modbus slave ID against reserved addresses
        String error_msg = "";
        if (new_id == 0) {
            error_msg = "Address 0 is reserved for broadcast messages.";
        } else if (new_id >= 248 && new_id <= 255) {
            error_msg = "Addresses 248-255 are reserved by Modbus specification.";
        } else if (new_id < 0 || new_id > 255) {
            error_msg = "Address must be in the range 0-255.";
        }

        if (error_msg == "" && new_id >= 1 && new_id <= 247) {
            // Valid slave ID
            modbus_slave_id = new_id;
            input_regs.slave_id = new_id;

            // Save to NVS
            preferences.begin("modbus", false);
            preferences.putUChar("slave_id", modbus_slave_id);
            preferences.end();

            mb.slave(modbus_slave_id);  // Update Modbus slave ID
            Serial.printf("Modbus Slave ID changed to: %d (saved to NVS)\n", modbus_slave_id);

            html += "<h1>Configuration Saved!</h1>";
            html += "<p>Modbus Slave ID updated to:</p>";
            html += "<div class='value'>" + String(modbus_slave_id) + "</div>";
            html += "<p>The new ID is active immediately and saved to NVS.</p>";
            html += "<a href='/'>Back to Home</a>";
        } else {
            // Invalid slave ID
            html += "<h1 style='color:#e74c3c;'>Invalid Slave ID</h1>";
            html += "<p style='color:#c0392b;font-weight:bold;'>" + error_msg + "</p>";
            html += "<div style='background:#fdeaea;padding:20px;border-radius:5px;margin:20px 0;border-left:4px solid #e74c3c;'>";
            html += "<p style='margin:5px 0;'><strong>You entered:</strong> " + String(new_id) + "</p>";
            html += "<p style='margin:5px 0;'><strong>Valid range:</strong> 1-247</p>";
            html += "<p style='margin:5px 0;'><strong>Reserved:</strong></p>";
            html += "<ul style='text-align:left;margin:10px 0;'>";
            html += "<li>0 = Broadcast address</li>";
            html += "<li>248-255 = Reserved addresses</li>";
            html += "</ul>";
            html += "</div>";
            html += "<a href='/'>Back to Home</a>";
        }
    } else {
        html += "<h1>Error</h1>";
        html += "<p>Missing slave_id parameter</p>";
        html += "<a href='/'>Back to Home</a>";
    }

    html += "</div></body></html>";
    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());
}

void handle_lorawan(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    String html = "<!DOCTYPE html><html><head><title>LoRaWAN Credentials - Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += ".container{max-width:800px;margin:20px auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
    html += "h1{color:#2c3e50;margin:10px 0;}";
    html += "h2{color:#34495e;margin-top:30px;border-bottom:2px solid #3498db;padding-bottom:10px;}";
    html += "table{width:100%;border-collapse:collapse;margin:20px 0;background:white;}";
    html += "th,td{padding:12px;text-align:left;border-bottom:1px solid #ddd;}";
    html += "th{background:#34495e;color:white;font-weight:600;}";
    html += "td.label{font-weight:600;width:25%;color:#34495e;}";
    html += "td.value{font-family:'Courier New',monospace;color:#2c3e50;word-break:break-all;background:#ecf0f1;padding:10px;}";
    html += "form{background:#f8f9fa;padding:20px;border-radius:5px;margin-top:20px;}";
    html += "label{display:block;margin:15px 0 5px;font-weight:600;color:#34495e;}";
    html += "input{width:100%;padding:10px;border:1px solid #ddd;border-radius:5px;font-family:'Courier New',monospace;box-sizing:border-box;}";
    html += "button{background:#3498db;color:white;padding:12px 30px;border:none;border-radius:5px;cursor:pointer;font-size:16px;margin-top:15px;}";
    html += "button:hover{background:#2980b9;}";
    html += ".warning{background:#fff3cd;border:1px solid #ffc107;padding:15px;border-radius:5px;margin:20px 0;color:#856404;}";
    html += ".nav{background:#3498db;padding:15px;margin:-30px -30px 30px -30px;border-radius:10px 10px 0 0;display:flex;align-items:center;}";
    html += ".nav a{color:white;text-decoration:none;padding:10px 20px;margin:0 5px;background:#2980b9;border-radius:5px;display:inline-block;}";
    html += ".nav a:hover{background:#21618c;}";
    html += ".nav .reboot{margin-left:auto;background:#e74c3c;}";
    html += ".nav .reboot:hover{background:#c0392b;}";
    html += "a{display:inline-block;background:#95a5a6;color:white;padding:10px 20px;text-decoration:none;border-radius:5px;margin-top:10px;}";
    html += "a:hover{background:#7f8c8d;}";
    html += "</style></head><body><div class='container'>";
    html += "<div class='nav'>";
    html += "<a href='/'>Home</a>";
    html += "<a href='/stats'>Statistics</a>";
    html += "<a href='/registers'>Registers</a>";
    html += "<a href='/lorawan'>LoRaWAN</a>";
    html += "<a href='/wifi'>WiFi</a>";
    html += "<a href='/security'>Security</a>";
    html += "<a href='/reboot' class='reboot' onclick='return confirm(\"Are you sure you want to reboot the device?\");'>Reboot</a>";
    html += "</div>";

    html += "<h1>LoRaWAN Credentials</h1>";
    html += "<p>These are your device's LoRaWAN OTAA credentials. Copy them to your network server (TTN, Chirpstack, etc.).</p>";

    // Display current credentials
    html += "<h2>Current Credentials</h2>";
    html += "<table>";
    html += "<tr><th>Parameter</th><th>Value</th></tr>";

    // JoinEUI
    char joinEUIStr[17];
    sprintf(joinEUIStr, "%016llX", joinEUI);
    html += "<tr><td class='label'>JoinEUI (AppEUI)</td><td class='value'>0x" + String(joinEUIStr) + "</td></tr>";

    // DevEUI
    char devEUIStr[17];
    sprintf(devEUIStr, "%016llX", devEUI);
    html += "<tr><td class='label'>DevEUI</td><td class='value'>0x" + String(devEUIStr) + "</td></tr>";

    // AppKey
    html += "<tr><td class='label'>AppKey</td><td class='value'>";
    for (int i = 0; i < 16; i++) {
        char buf[3];
        sprintf(buf, "%02X", appKey[i]);
        html += String(buf);
    }
    html += "</td></tr>";

    // NwkKey
    html += "<tr><td class='label'>NwkKey</td><td class='value'>";
    for (int i = 0; i < 16; i++) {
        char buf[3];
        sprintf(buf, "%02X", nwkKey[i]);
        html += String(buf);
    }
    html += "</td></tr>";
    html += "</table>";

    // Edit form
    html += "<h2>Update Credentials</h2>";
    html += "<div class='warning'>";
    html += "<strong>Warning:</strong> Changing credentials will require re-registering the device with your network server. ";
    html += "The device will lose its current session and must rejoin the network.";
    html += "</div>";

    html += "<form method='POST' action='/lorawan/config'>";
    html += "<label>JoinEUI (16 hex characters):</label>";
    html += "<input type='text' name='joinEUI' value='" + String(joinEUIStr) + "' pattern='[0-9A-Fa-f]{16}' required>";

    html += "<label>DevEUI (16 hex characters):</label>";
    html += "<input type='text' name='devEUI' value='" + String(devEUIStr) + "' pattern='[0-9A-Fa-f]{16}' required>";

    html += "<label>AppKey (32 hex characters):</label>";
    html += "<input type='text' name='appKey' value='";
    for (int i = 0; i < 16; i++) {
        char buf[3];
        sprintf(buf, "%02X", appKey[i]);
        html += String(buf);
    }
    html += "' pattern='[0-9A-Fa-f]{32}' required>";

    html += "<label>NwkKey (32 hex characters):</label>";
    html += "<input type='text' name='nwkKey' value='";
    for (int i = 0; i < 16; i++) {
        char buf[3];
        sprintf(buf, "%02X", nwkKey[i]);
        html += String(buf);
    }
    html += "' pattern='[0-9A-Fa-f]{32}' required>";

    html += "<button type='submit'>Save & Restart</button>";
    html += "</form>";

    html += "<a href='/'>Back to Home</a>";
    html += "</div></body></html>";
    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());
}

void handle_lorawan_config(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    String html = "<!DOCTYPE html><html><head><title>Updating LoRaWAN - Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta http-equiv='refresh' content='10;url=/'>";
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += ".container{max-width:600px;margin:50px auto;background:white;padding:40px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);text-align:center;}";
    html += "h1{color:#27ae60;}";
    html += "p{color:#7f8c8d;font-size:16px;margin:20px 0;}";
    html += ".spinner{border:4px solid #f3f3f3;border-top:4px solid #3498db;border-radius:50%;width:50px;height:50px;animation:spin 1s linear infinite;margin:20px auto;}";
    html += "@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}";
    html += "</style></head><body><div class='container'>";

    bool success = false;

    // Read POST body once
    String postBody = readPostBody(req);

    Serial.println("\n>>> LoRaWAN Config Update:");
    Serial.printf("    POST body length: %d bytes\n", postBody.length());

    String joinEUIStr, devEUIStr, appKeyStr, nwkKeyStr;
    bool has_joinEUI = getPostParameterFromBody(postBody, "joinEUI", joinEUIStr);
    bool has_devEUI = getPostParameterFromBody(postBody, "devEUI", devEUIStr);
    bool has_appKey = getPostParameterFromBody(postBody, "appKey", appKeyStr);
    bool has_nwkKey = getPostParameterFromBody(postBody, "nwkKey", nwkKeyStr);

    Serial.printf("    JoinEUI: %s (length: %d)\n", has_joinEUI ? joinEUIStr.c_str() : "missing", joinEUIStr.length());
    Serial.printf("    DevEUI: %s (length: %d)\n", has_devEUI ? devEUIStr.c_str() : "missing", devEUIStr.length());
    Serial.printf("    AppKey: %s (length: %d)\n", has_appKey ? "[provided]" : "missing", appKeyStr.length());
    Serial.printf("    NwkKey: %s (length: %d)\n", has_nwkKey ? "[provided]" : "missing", nwkKeyStr.length());

    if (has_joinEUI && has_devEUI && has_appKey && has_nwkKey) {

        // Validate lengths
        if (joinEUIStr.length() == 16 && devEUIStr.length() == 16 &&
            appKeyStr.length() == 32 && nwkKeyStr.length() == 32) {

            // Parse JoinEUI
            joinEUI = strtoull(joinEUIStr.c_str(), NULL, 16);

            // Parse DevEUI
            devEUI = strtoull(devEUIStr.c_str(), NULL, 16);

            // Parse AppKey
            for (int i = 0; i < 16; i++) {
                char buf[3] = {appKeyStr[i*2], appKeyStr[i*2+1], 0};
                appKey[i] = strtol(buf, NULL, 16);
            }

            // Parse NwkKey
            for (int i = 0; i < 16; i++) {
                char buf[3] = {nwkKeyStr[i*2], nwkKeyStr[i*2+1], 0};
                nwkKey[i] = strtol(buf, NULL, 16);
            }

            // Save to NVS
            save_lorawan_credentials();

            html += "<h1>Credentials Updated!</h1>";
            html += "<div class='spinner'></div>";
            html += "<p>New credentials saved to non-volatile storage.</p>";
            html += "<p>Device is restarting to apply changes...</p>";
            html += "<p>You will be redirected to the home page in 10 seconds.</p>";
            success = true;
        } else {
            html += "<h1>Invalid Format</h1>";
            html += "<p>Credentials must be valid hexadecimal:</p>";
            html += "<ul style='text-align:left;'>";
            html += "<li>JoinEUI: 16 hex characters</li>";
            html += "<li>DevEUI: 16 hex characters</li>";
            html += "<li>AppKey: 32 hex characters</li>";
            html += "<li>NwkKey: 32 hex characters</li>";
            html += "</ul>";
            html += "<a href='/lorawan'>← Back</a>";
        }
    } else {
        html += "<h1>Error</h1>";
        html += "<p>Missing required parameters</p>";
        html += "<a href='/lorawan'>← Back</a>";
    }

    html += "</div></body></html>";
    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());

    // Restart device after successful update
    if (success) {
        delay(2000);
        Serial.println("Restarting device to apply new LoRaWAN credentials...");
        ESP.restart();
    }
}

// Handle WiFi configuration page
void handle_wifi(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    String html = "<!DOCTYPE html><html><head><title>WiFi Configuration - Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += ".container{max-width:900px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
    html += "h1{color:#2c3e50;margin-top:0;border-bottom:3px solid #3498db;padding-bottom:15px;}";
    html += "h2{color:#34495e;margin-top:30px;}";
    html += ".nav{background:#3498db;padding:15px;margin:-30px -30px 30px -30px;border-radius:10px 10px 0 0;display:flex;align-items:center;}";
    html += ".nav a{color:white;text-decoration:none;padding:10px 20px;margin:0 5px;background:#2980b9;border-radius:5px;display:inline-block;}";
    html += ".nav a:hover{background:#21618c;}";
    html += ".nav .reboot{margin-left:auto;background:#e74c3c;}";
    html += ".nav .reboot:hover{background:#c0392b;}";
    html += ".card{background:#ecf0f1;padding:20px;margin:15px 0;border-radius:8px;border-left:4px solid #3498db;}";
    html += ".status{padding:15px;margin:20px 0;border-radius:8px;background:#d4edda;border:1px solid #c3e6cb;color:#155724;}";
    html += ".status.disconnected{background:#f8d7da;border-color:#f5c6cb;color:#721c24;}";
    html += "form{background:#ecf0f1;padding:20px;border-radius:8px;margin:20px 0;}";
    html += "label{display:block;margin-bottom:8px;color:#2c3e50;font-weight:600;}";
    html += "input[type=text],input[type=password],select{width:100%;padding:10px;border:2px solid #bdc3c7;border-radius:5px;font-size:16px;box-sizing:border-box;margin-bottom:15px;}";
    html += "input[type=text]:focus,input[type=password]:focus,select:focus{border-color:#3498db;outline:none;}";
    html += "button,input[type=submit]{background:#27ae60;color:white;padding:12px 30px;border:none;border-radius:5px;font-size:16px;cursor:pointer;margin:5px 5px 5px 0;}";
    html += "button:hover,input[type=submit]:hover{background:#229954;}";
    html += "button.secondary{background:#3498db;}";
    html += "button.secondary:hover{background:#2980b9;}";
    html += "#scanResults{margin-top:15px;}";
    html += ".network{background:white;padding:12px;margin:8px 0;border-radius:5px;border:1px solid #ddd;cursor:pointer;}";
    html += ".network:hover{background:#e3f2fd;border-color:#3498db;}";
    html += ".network strong{color:#2c3e50;}";
    html += ".network .rssi{color:#7f8c8d;float:right;}";
    html += ".spinner{border:4px solid #f3f3f3;border-top:4px solid #3498db;border-radius:50%;width:40px;height:40px;animation:spin 1s linear infinite;display:inline-block;vertical-align:middle;}";
    html += "@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}";
    html += "</style>";
    html += "<script>";
    html += "function scanNetworks(){";
    html += "  document.getElementById('scanBtn').disabled=true;";
    html += "  document.getElementById('scanBtn').innerHTML='<span class=\"spinner\" style=\"width:20px;height:20px;border-width:3px;\"></span> Scanning...';";
    html += "  fetch('/wifi/scan').then(r=>r.json()).then(data=>{";
    html += "    let html='';";
    html += "    data.networks.forEach(n=>{";
    html += "      html+='<div class=\"network\" onclick=\"selectNetwork(\\\''+n.ssid+'\\\')\"><strong>'+n.ssid+'</strong><span class=\"rssi\">'+n.rssi+' dBm</span></div>';";
    html += "    });";
    html += "    document.getElementById('scanResults').innerHTML=html;";
    html += "    document.getElementById('scanBtn').disabled=false;";
    html += "    document.getElementById('scanBtn').innerHTML='Scan for Networks';";
    html += "  }).catch(e=>{";
    html += "    document.getElementById('scanResults').innerHTML='<p style=\"color:red;\">Scan failed</p>';";
    html += "    document.getElementById('scanBtn').disabled=false;";
    html += "    document.getElementById('scanBtn').innerHTML='Scan for Networks';";
    html += "  });";
    html += "}";
    html += "function selectNetwork(ssid){";
    html += "  document.getElementById('ssid').value=ssid;";
    html += "}";
    html += "function refreshStatus(){";
    html += "  fetch('/wifi/status').then(r=>r.json()).then(data=>{";
    html += "    let statusHtml='';";
    html += "    if(data.client_connected){";
    html += "      statusHtml='<div class=\"status\"><strong>Connected to WiFi</strong><br>SSID: '+data.client_ssid+'<br>IP Address: '+data.client_ip+'<br>Signal: '+data.client_rssi+' dBm</div>';";
    html += "    }else if(data.saved_ssid){";
    html += "      statusHtml='<div class=\"status disconnected\"><strong>Not Connected</strong><br>Saved network: '+data.saved_ssid+'<br>Will try to connect on next boot</div>';";
    html += "    }else{";
    html += "      statusHtml='<div class=\"status disconnected\"><strong>No WiFi Configured</strong><br>Use the form below to connect to a WiFi network</div>';";
    html += "    }";
    html += "    document.getElementById('statusArea').innerHTML=statusHtml;";
    html += "  });";
    html += "}";
    html += "window.onload=function(){refreshStatus();};";
    html += "</script>";
    html += "</head><body>";
    html += "<div class='container'>";
    html += "<div class='nav'>";
    html += "<a href='/'>Home</a>";
    html += "<a href='/stats'>Statistics</a>";
    html += "<a href='/registers'>Registers</a>";
    html += "<a href='/lorawan'>LoRaWAN</a>";
    html += "<a href='/wifi'>WiFi</a>";
    html += "<a href='/security'>Security</a>";
    html += "<a href='/reboot' class='reboot' onclick='return confirm(\"Are you sure you want to reboot the device?\");'>Reboot</a>";
    html += "</div>";
    html += "<h1>WiFi Configuration</h1>";

    html += "<div id='statusArea'>";
    // Status will be loaded via JavaScript
    html += "<div class='status'><div class='spinner'></div> Loading status...</div>";
    html += "</div>";

    html += "<h2>Connect to WiFi Network</h2>";
    html += "<div class='card'>";
    html += "<p>Connect this device to an existing WiFi network. When connected:</p>";
    html += "<ul style='margin:10px 0;padding-left:20px;'>";
    html += "<li>WiFi remains active indefinitely (no 20-minute timeout)</li>";
    html += "<li>Access point (AP) mode is disabled</li>";
    html += "<li>Web interface accessible via client IP address</li>";
    html += "<li>If network is lost, AP mode will restart automatically</li>";
    html += "</ul>";
    html += "<button id='scanBtn' class='secondary' onclick='scanNetworks()'>Scan for Networks</button>";
    html += "<div id='scanResults'></div>";
    html += "</div>";

    html += "<form action='/wifi/connect' method='POST'>";
    html += "<label>WiFi SSID:</label>";
    html += "<input type='text' id='ssid' name='ssid' placeholder='Enter network name' required>";
    html += "<label>WiFi Password:</label>";
    html += "<input type='password' name='password' placeholder='Enter password' autocomplete='off'>";
    html += "<p style='font-size:12px;color:#7f8c8d;margin:5px 0 15px 0;'>Leave password empty for open networks. Device will restart after saving.</p>";
    html += "<input type='submit' value='Connect to Network'>";
    html += "</form>";

    html += "</div></body></html>";
    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());
}

// Handle WiFi scan request
void handle_wifi_scan(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    Serial.println(">>> WiFi scan requested");

    // Start scan
    int n = WiFi.scanNetworks();

    // Build JSON response
    String json = "{\"networks\":[";
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"encryption\":" + String(WiFi.encryptionType(i));
        json += "}";
    }
    json += "]}";

    WiFi.scanDelete();
    res->setStatusCode(200);
    res->setHeader("Content-Type", "application/json");
    res->print(json.c_str());
    Serial.printf(">>> Found %d networks\n", n);
}

// Handle WiFi connect request
void handle_wifi_connect(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    String html = "<!DOCTYPE html><html><head><title>Connecting to WiFi - Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta http-equiv='refresh' content='15;url=/'>";
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += ".container{max-width:600px;margin:50px auto;background:white;padding:40px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);text-align:center;}";
    html += "h1{color:#27ae60;}";
    html += "p{color:#7f8c8d;font-size:16px;margin:20px 0;}";
    html += ".spinner{border:4px solid #f3f3f3;border-top:4px solid #3498db;border-radius:50%;width:50px;height:50px;animation:spin 1s linear infinite;margin:20px auto;}";
    html += "@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}";
    html += "</style></head><body><div class='container'>";

    bool success = false;

    // Read POST body once
    String postBody = readPostBody(req);

    Serial.println("\n>>> WiFi Connect Request:");
    Serial.printf("    POST body: %s\n", postBody.c_str());

    String ssid, password;
    bool has_ssid = getPostParameterFromBody(postBody, "ssid", ssid);
    bool has_password = getPostParameterFromBody(postBody, "password", password);

    Serial.printf("    SSID: %s (length: %d)\n", has_ssid ? ssid.c_str() : "missing", ssid.length());
    Serial.printf("    Password: %s (length: %d)\n", has_password ? "[provided]" : "missing", password.length());

    if (has_ssid && has_password) {

        if (ssid.length() > 0 && ssid.length() <= 32 && password.length() <= 63) {
            // Save credentials
            strncpy(wifi_client_ssid, ssid.c_str(), sizeof(wifi_client_ssid) - 1);
            wifi_client_ssid[sizeof(wifi_client_ssid) - 1] = '\0';

            strncpy(wifi_client_password, password.c_str(), sizeof(wifi_client_password) - 1);
            wifi_client_password[sizeof(wifi_client_password) - 1] = '\0';

            save_wifi_client_credentials();

            html += "<h1>WiFi Credentials Saved!</h1>";
            html += "<div class='spinner'></div>";
            html += "<p>Connecting to: <strong>" + ssid + "</strong></p>";
            html += "<p>Device is restarting to connect to the new network...</p>";
            html += "<p>You will be redirected in 15 seconds.</p>";
            html += "<p><small>If the connection fails, the device will start its own AP again.</small></p>";
            success = true;
        } else {
            html += "<h1>Invalid Input</h1>";
            html += "<p>SSID must be 1-32 characters, password up to 63 characters.</p>";
            html += "<a href='/'>← Back</a>";
        }
    } else {
        html += "<h1>Error</h1>";
        html += "<p>Missing SSID or password</p>";
        html += "<a href='/'>← Back</a>";
    }

    html += "</div></body></html>";
    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());

    // Restart device after successful save
    if (success) {
        delay(2000);
        Serial.println("Restarting device to connect to WiFi...");
        ESP.restart();
    }
}

// Handle WiFi status request
void handle_wifi_status(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    String json = "{";
    json += "\"client_connected\":" + String(wifi_client_connected ? "true" : "false") + ",";
    json += "\"ap_active\":" + String(wifi_ap_active ? "true" : "false") + ",";

    if (wifi_client_connected) {
        json += "\"client_ssid\":\"" + WiFi.SSID() + "\",";
        json += "\"client_ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"client_rssi\":" + String(WiFi.RSSI());
    } else {
        json += "\"client_ssid\":\"\",";
        json += "\"client_ip\":\"\",";
        json += "\"client_rssi\":0";
    }

    if (strlen(wifi_client_ssid) > 0) {
        json += ",\"saved_ssid\":\"" + String(wifi_client_ssid) + "\"";
    } else {
        json += ",\"saved_ssid\":\"\"";
    }

    json += "}";
    res->setStatusCode(200);
    res->setHeader("Content-Type", "application/json");
    res->print(json.c_str());
}

// ============================================================================
// AUTHENTICATION FUNCTIONS
// ============================================================================

// Load authentication credentials from NVS
void load_auth_credentials() {
    if (!preferences.begin("auth", true)) {  // Read-only
        Serial.println(">>> Failed to open auth preferences");
        return;
    }

    auth_enabled = preferences.getBool("enabled", true);
    debug_https_enabled = preferences.getBool("debug_https", false);
    debug_auth_enabled = preferences.getBool("debug_auth", false);

    String username = preferences.getString("username", "admin");
    String password = preferences.getString("password", "admin");

    if (username.length() > 0 && username.length() <= 32) {
        strncpy(auth_username, username.c_str(), sizeof(auth_username) - 1);
        auth_username[sizeof(auth_username) - 1] = '\0';
    }

    if (password.length() > 0 && password.length() <= 32) {
        strncpy(auth_password, password.c_str(), sizeof(auth_password) - 1);
        auth_password[sizeof(auth_password) - 1] = '\0';
    }

    preferences.end();

    Serial.println(">>> Authentication credentials loaded");
    Serial.printf("    Enabled: %s\n", auth_enabled ? "YES" : "NO");
    Serial.printf("    Username: %s\n", auth_username);
    Serial.printf("    HTTPS Debug: %s\n", debug_https_enabled ? "YES" : "NO");
    Serial.printf("    Auth Debug: %s\n", debug_auth_enabled ? "YES" : "NO");
}

// Save authentication credentials to NVS
void save_auth_credentials() {
    if (!preferences.begin("auth", false)) {  // Read-write
        Serial.println(">>> Failed to open auth preferences for writing");
        return;
    }

    preferences.putBool("enabled", auth_enabled);
    preferences.putString("username", auth_username);
    preferences.putString("password", auth_password);
    preferences.putBool("debug_https", debug_https_enabled);
    preferences.putBool("debug_auth", debug_auth_enabled);
    preferences.end();

    Serial.println(">>> Authentication credentials saved to NVS");
    Serial.printf("    HTTPS Debug: %s\n", debug_https_enabled ? "YES" : "NO");
    Serial.printf("    Auth Debug: %s\n", debug_auth_enabled ? "YES" : "NO");
}

// Helper function to parse URL-encoded POST data
// Read POST body once and return as String
String readPostBody(HTTPRequest * req) {
    byte buffer[512];
    size_t idx = 0;

    // Read POST body in chunks
    while (!req->requestComplete()) {
        size_t available = req->readBytes(buffer + idx, sizeof(buffer) - idx - 1);
        if (available == 0) break;
        idx += available;
        if (idx >= sizeof(buffer) - 1) break;
    }
    buffer[idx] = 0;

    return String((char*)buffer);
}

// Parse parameter from POST body string (body should be read once with readPostBody)
bool getPostParameterFromBody(const String& body, const char* name, String& value) {
    // Parse URL-encoded parameters
    int startIdx = body.indexOf(String(name) + "=");
    if (startIdx == -1) {
        return false;
    }

    startIdx += strlen(name) + 1;
    int endIdx = body.indexOf('&', startIdx);
    if (endIdx == -1) {
        endIdx = body.length();
    }

    value = body.substring(startIdx, endIdx);

    // URL decode the value
    value.replace("+", " ");
    value.replace("%20", " ");
    value.replace("%21", "!");
    value.replace("%22", "\"");
    value.replace("%23", "#");
    value.replace("%24", "$");
    value.replace("%25", "%");
    value.replace("%26", "&");
    value.replace("%27", "'");
    value.replace("%28", "(");
    value.replace("%29", ")");
    value.replace("%2A", "*");
    value.replace("%2B", "+");
    value.replace("%2C", ",");
    value.replace("%2D", "-");
    value.replace("%2E", ".");
    value.replace("%2F", "/");
    value.replace("%3A", ":");
    value.replace("%3B", ";");
    value.replace("%3C", "<");
    value.replace("%3D", "=");
    value.replace("%3E", ">");
    value.replace("%3F", "?");
    value.replace("%40", "@");

    return true;
}

// Legacy wrapper - reads body and parses single parameter (less efficient for multiple params)
bool getPostParameter(HTTPRequest * req, const char* name, String& value) {
    String body = readPostBody(req);
    return getPostParameterFromBody(body, name, value);
}

// Check HTTP Basic Authentication
bool check_authentication(HTTPRequest * req, HTTPResponse * res) {
    if (debug_auth_enabled) {
        Serial.printf(">>> Auth check: enabled=%d, user=%s\n", auth_enabled, auth_username);
    }

    if (!auth_enabled) {
        if (debug_auth_enabled) {
            Serial.println(">>> Auth disabled, allowing access");
        }
        return true;  // Auth disabled, allow access
    }

    // Check for Authorization header
    std::string authValueStd = req->getHeader("Authorization");
    if (authValueStd.empty()) {
        if (debug_auth_enabled) {
            Serial.println(">>> No Authorization header, requesting credentials");
        }
        res->setStatusCode(401);
        res->setHeader("WWW-Authenticate", "Basic realm=\"Vision Master E290\"");
        res->setHeader("Content-Type", "text/html");
        res->println("<!DOCTYPE html><html><body><h1>401 Unauthorized</h1><p>Authentication required</p></body></html>");
        return false;
    }

    // Parse Basic authentication header
    String authValue = String(authValueStd.c_str());
    if (!authValue.startsWith("Basic ")) {
        if (debug_auth_enabled) {
            Serial.println(">>> Invalid auth type (not Basic)");
        }
        res->setStatusCode(401);
        res->setHeader("WWW-Authenticate", "Basic realm=\"Vision Master E290\"");
        res->setHeader("Content-Type", "text/html");
        res->println("<!DOCTYPE html><html><body><h1>401 Unauthorized</h1></body></html>");
        return false;
    }

    // Extract base64 encoded credentials
    String base64Credentials = authValue.substring(6);
    base64Credentials.trim();

    // Decode base64 using mbedtls
    unsigned char decoded[128];
    size_t decoded_len = 0;
    int ret = mbedtls_base64_decode(decoded, sizeof(decoded), &decoded_len,
                                     (const unsigned char*)base64Credentials.c_str(),
                                     base64Credentials.length());

    if (ret != 0) {
        if (debug_auth_enabled) {
            Serial.printf(">>> Base64 decode failed: %d\n", ret);
        }
        res->setStatusCode(401);
        res->setHeader("WWW-Authenticate", "Basic realm=\"Vision Master E290\"");
        res->setHeader("Content-Type", "text/html");
        res->println("<!DOCTYPE html><html><body><h1>401 Unauthorized</h1></body></html>");
        return false;
    }

    String decodedCredentials = String((char*)decoded).substring(0, decoded_len);

    // Split credentials into username and password
    int colonIndex = decodedCredentials.indexOf(':');
    if (colonIndex == -1) {
        if (debug_auth_enabled) {
            Serial.println(">>> Invalid credentials format (no colon)");
        }
        res->setStatusCode(401);
        res->setHeader("WWW-Authenticate", "Basic realm=\"Vision Master E290\"");
        res->setHeader("Content-Type", "text/html");
        res->println("<!DOCTYPE html><html><body><h1>401 Unauthorized</h1></body></html>");
        return false;
    }

    String providedUsername = decodedCredentials.substring(0, colonIndex);
    String providedPassword = decodedCredentials.substring(colonIndex + 1);

    // Compare credentials
    if (providedUsername.equals(auth_username) && providedPassword.equals(auth_password)) {
        if (debug_auth_enabled) {
            Serial.println(">>> Auth successful");
        }
        return true;
    } else {
        if (debug_auth_enabled) {
            Serial.println(">>> Auth failed - incorrect credentials");
            Serial.printf(">>> Provided username: %s, expected: %s\n", providedUsername.c_str(), auth_username);
        }
        res->setStatusCode(401);
        res->setHeader("WWW-Authenticate", "Basic realm=\"Vision Master E290\"");
        res->setHeader("Content-Type", "text/html");
        res->println("<!DOCTYPE html><html><body><h1>401 Unauthorized</h1><p>Invalid credentials</p></body></html>");
        return false;
    }
}

// Handle security settings page
void handle_security(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    Serial.println("\n>>> Security Page Loaded:");
    Serial.printf("    auth_enabled: %s\n", auth_enabled ? "true (checkbox will be checked)" : "false (checkbox will be unchecked)");
    Serial.printf("    debug_https_enabled: %s\n", debug_https_enabled ? "true (checkbox will be checked)" : "false (checkbox will be unchecked)");
    Serial.printf("    debug_auth_enabled: %s\n", debug_auth_enabled ? "true (checkbox will be checked)" : "false (checkbox will be unchecked)");

    String html = "<!DOCTYPE html><html><head><title>Security Settings - Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += ".container{max-width:900px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
    html += "h1{color:#2c3e50;margin-top:0;border-bottom:3px solid #3498db;padding-bottom:15px;}";
    html += "h2{color:#34495e;margin-top:30px;}";
    html += ".nav{background:#3498db;padding:15px;margin:-30px -30px 30px -30px;border-radius:10px 10px 0 0;display:flex;align-items:center;}";
    html += ".nav a{color:white;text-decoration:none;padding:10px 20px;margin:0 5px;background:#2980b9;border-radius:5px;display:inline-block;}";
    html += ".nav a:hover{background:#21618c;}";
    html += ".nav .reboot{margin-left:auto;background:#e74c3c;}";
    html += ".nav .reboot:hover{background:#c0392b;}";
    html += ".card{background:#ecf0f1;padding:20px;margin:15px 0;border-radius:8px;border-left:4px solid #3498db;}";
    html += ".warning{background:#fff3cd;border-color:#ffc107;color:#856404;padding:15px;margin:15px 0;border-radius:8px;border-left:4px solid #ffc107;}";
    html += "form{background:#ecf0f1;padding:20px;border-radius:8px;margin:20px 0;}";
    html += "label{display:block;margin:15px 0 8px 0;color:#2c3e50;font-weight:600;}";
    html += "input[type=text],input[type=password]{width:100%;padding:10px;border:2px solid #bdc3c7;border-radius:5px;font-size:16px;box-sizing:border-box;}";
    html += "input[type=text]:focus,input[type=password]:focus{border-color:#3498db;outline:none;}";
    html += "input[type=checkbox]{width:20px;height:20px;margin-right:10px;vertical-align:middle;}";
    html += "input[type=submit]{background:#27ae60;color:white;padding:12px 30px;border:none;border-radius:5px;font-size:16px;cursor:pointer;margin-top:15px;}";
    html += "input[type=submit]:hover{background:#229954;}";
    html += ".checkbox-label{display:flex;align-items:center;margin:15px 0;}";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<div class='nav'>";
    html += "<a href='/'>Home</a>";
    html += "<a href='/stats'>Statistics</a>";
    html += "<a href='/registers'>Registers</a>";
    html += "<a href='/lorawan'>LoRaWAN</a>";
    html += "<a href='/wifi'>WiFi</a>";
    html += "<a href='/security'>Security</a>";
    html += "<a href='/reboot' class='reboot' onclick='return confirm(\"Are you sure you want to reboot the device?\");'>Reboot</a>";
    html += "</div>";
    html += "<h1>Security Settings</h1>";

    html += "<div class='warning'>";
    html += "<strong>Warning:</strong> Changing these settings will affect access to this web interface. ";
    html += "Make sure you remember your credentials. Default is username: <strong>admin</strong>, password: <strong>admin</strong>";
    html += "</div>";

    html += "<div class='card'>";
    html += "<p><strong>Current Status:</strong></p>";
    html += "<p>Authentication: <strong>" + String(auth_enabled ? "ENABLED" : "DISABLED") + "</strong></p>";
    html += "<p>Username: <strong>" + String(auth_username) + "</strong></p>";
    html += "</div>";

    html += "<h2>Update Authentication</h2>";
    html += "<form action='/security/update' method='POST'>";

    html += "<div class='checkbox-label'>";
    html += "<input type='checkbox' name='auth_enabled' value='1'" + String(auth_enabled ? " checked" : "") + ">";
    html += "<label style='margin:0;'>Enable Authentication (uncheck to disable login requirement)</label>";
    html += "</div>";

    html += "<label>Username:</label>";
    html += "<input type='text' name='username' value='" + String(auth_username) + "' maxlength='32' required>";
    html += "<p style='font-size:12px;color:#7f8c8d;margin:5px 0 0 0;'>Maximum 32 characters</p>";

    html += "<label>New Password:</label>";
    html += "<input type='password' name='password' placeholder='Leave empty to keep current password' maxlength='32'>";
    html += "<p style='font-size:12px;color:#7f8c8d;margin:5px 0 0 0;'>Maximum 32 characters. Leave empty to keep current password.</p>";

    html += "<input type='submit' value='Save Authentication Settings'>";
    html += "</form>";

    html += "<h2 style='margin-top:30px;'>Debug Settings</h2>";
    html += "<p style='color:#7f8c8d;'>Enable console logging for troubleshooting (check serial monitor at 115200 baud)</p>";

    html += "<form action='/security/debug' method='POST'>";

    html += "<div class='checkbox-label'>";
    html += "<input type='checkbox' name='debug_https' value='1'" + String(debug_https_enabled ? " checked" : "") + ">";
    html += "<label style='margin:0;'>Enable HTTPS Debug Logging</label>";
    html += "</div>";
    html += "<p style='font-size:12px;color:#7f8c8d;margin:5px 0 15px 30px;'>Shows HTTP requests, responses, redirects</p>";

    html += "<div class='checkbox-label'>";
    html += "<input type='checkbox' name='debug_auth' value='1'" + String(debug_auth_enabled ? " checked" : "") + ">";
    html += "<label style='margin:0;'>Enable Authentication Debug Logging</label>";
    html += "</div>";
    html += "<p style='font-size:12px;color:#7f8c8d;margin:5px 0 0 30px;'>Shows authentication attempts, credential validation</p>";

    html += "<input type='submit' value='Save Debug Settings'>";
    html += "</form>";

    html += "</div></body></html>";
    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());
}

// Handle security settings update
void handle_security_update(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    String html = "<!DOCTYPE html><html><head><title>Updating Security - Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta http-equiv='refresh' content='3;url=/security'>";
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += ".container{max-width:600px;margin:50px auto;background:white;padding:40px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);text-align:center;}";
    html += "h1{color:#27ae60;}";
    html += "p{color:#7f8c8d;font-size:16px;margin:20px 0;}";
    html += "</style></head><body><div class='container'>";

    bool success = false;

    // Read POST body once
    String postBody = readPostBody(req);

    Serial.println("\n>>> Security Update Request:");
    Serial.printf("    POST body: %s\n", postBody.c_str());

    // Parse all parameters from the body
    String new_username, new_password, auth_enabled_str;
    bool has_username = getPostParameterFromBody(postBody, "username", new_username);
    bool has_password = getPostParameterFromBody(postBody, "password", new_password);
    bool new_auth_enabled = getPostParameterFromBody(postBody, "auth_enabled", auth_enabled_str);

    Serial.printf("    Current auth_enabled: %s\n", auth_enabled ? "true" : "false");
    Serial.printf("    Form auth_enabled param: %s\n", new_auth_enabled ? "received (checkbox checked)" : "missing (checkbox unchecked)");
    Serial.printf("    Username from form: %s\n", new_username.c_str());

    if (has_username) {

        if (new_username.length() > 0 && new_username.length() <= 32) {
            // Update username
            strncpy(auth_username, new_username.c_str(), sizeof(auth_username) - 1);
            auth_username[sizeof(auth_username) - 1] = '\0';

            // Update password if provided
            if (new_password.length() > 0 && new_password.length() <= 32) {
                strncpy(auth_password, new_password.c_str(), sizeof(auth_password) - 1);
                auth_password[sizeof(auth_password) - 1] = '\0';
            }

            // Update enabled status
            auth_enabled = new_auth_enabled;

            Serial.printf("    After update - auth_enabled: %s\n", auth_enabled ? "true" : "false");

            // Save to NVS
            save_auth_credentials();

            html += "<h1>Security Settings Updated!</h1>";
            html += "<p>Authentication: <strong>" + String(auth_enabled ? "ENABLED" : "DISABLED") + "</strong></p>";
            html += "<p>Username: <strong>" + String(auth_username) + "</strong></p>";
            if (new_password.length() > 0) {
                html += "<p>Password has been changed.</p>";
            }
            html += "<p>You will be redirected to the security page in 3 seconds.</p>";
            html += "<p><small>You may need to log in again with the new credentials.</small></p>";
            success = true;
        } else {
            html += "<h1 style='color:#e74c3c;'>Error</h1>";
            html += "<p>Username must be 1-32 characters</p>";
            html += "<a href='/security'>← Back</a>";
        }
    } else {
        html += "<h1 style='color:#e74c3c;'>Error</h1>";
        html += "<p>Missing required parameters</p>";
        html += "<a href='/security'>← Back</a>";
    }

    html += "</div></body></html>";
    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());
}

// Handle debug settings update (separate from authentication settings)
void handle_debug_update(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    String html = "<!DOCTYPE html><html><head><title>Updating Debug Settings - Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta http-equiv='refresh' content='2;url=/security'>";
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += ".container{max-width:600px;margin:50px auto;background:white;padding:40px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);text-align:center;}";
    html += "h1{color:#27ae60;}";
    html += "p{color:#7f8c8d;font-size:16px;margin:20px 0;}";
    html += "</style></head><body><div class='container'>";

    // Read POST body once
    String postBody = readPostBody(req);

    Serial.println("\n>>> Debug Settings Update:");
    Serial.printf("    POST body: %s\n", postBody.c_str());

    // Update debug settings (checkboxes only send parameters when checked)
    String debug_https_str, debug_auth_str;
    debug_https_enabled = getPostParameterFromBody(postBody, "debug_https", debug_https_str);
    debug_auth_enabled = getPostParameterFromBody(postBody, "debug_auth", debug_auth_str);

    Serial.printf("    HTTPS Debug: %s\n", debug_https_enabled ? "ENABLED" : "DISABLED");
    Serial.printf("    Auth Debug: %s\n", debug_auth_enabled ? "ENABLED" : "DISABLED");

    // Save to NVS (saves debug settings along with auth credentials)
    save_auth_credentials();

    html += "<h1>Debug Settings Updated!</h1>";
    html += "<p>HTTPS Debug Logging: <strong>" + String(debug_https_enabled ? "ENABLED" : "DISABLED") + "</strong></p>";
    html += "<p>Auth Debug Logging: <strong>" + String(debug_auth_enabled ? "ENABLED" : "DISABLED") + "</strong></p>";
    html += "<p>You will be redirected to the security page in 2 seconds.</p>";
    html += "</div></body></html>";

    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());
}

// Emergency handler to force-enable authentication (no auth required to access this)
void handle_enable_auth(HTTPRequest * req, HTTPResponse * res) {
    Serial.println("\n>>> EMERGENCY: Force-enabling authentication");

    auth_enabled = true;
    save_auth_credentials();

    Serial.printf("    Authentication enabled: %s\n", auth_enabled ? "YES" : "NO");

    String html = "<!DOCTYPE html><html><head><title>Authentication Enabled - Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta http-equiv='refresh' content='3;url=/security'>";
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += ".container{max-width:600px;margin:50px auto;background:white;padding:40px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);text-align:center;}";
    html += "h1{color:#27ae60;}";
    html += "p{color:#7f8c8d;font-size:16px;margin:20px 0;}";
    html += "</style></head><body><div class='container'>";
    html += "<h1>Authentication Re-Enabled!</h1>";
    html += "<p>Authentication has been successfully enabled.</p>";
    html += "<p>Username: <strong>" + String(auth_username) + "</strong></p>";
    html += "<p>You will be redirected to the security page in 3 seconds.</p>";
    html += "<p><small>You will be prompted to log in.</small></p>";
    html += "</div></body></html>";

    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());
}

// Handle reboot request
void handle_reboot(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    String html = "<!DOCTYPE html><html><head><title>Rebooting - Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta http-equiv='refresh' content='10;url=/'>";
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += ".container{max-width:600px;margin:50px auto;background:white;padding:40px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);text-align:center;}";
    html += "h1{color:#e74c3c;}";
    html += "p{color:#7f8c8d;font-size:16px;margin:20px 0;}";
    html += ".spinner{border:4px solid #f3f3f3;border-top:4px solid #e74c3c;border-radius:50%;width:50px;height:50px;animation:spin 1s linear infinite;margin:20px auto;}";
    html += "@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}";
    html += "</style></head><body><div class='container'>";
    html += "<h1>Device Rebooting...</h1>";
    html += "<div class='spinner'></div>";
    html += "<p>The device is restarting now.</p>";
    html += "<p>You will be redirected to the home page in 10 seconds.</p>";
    html += "<p><small>If the page doesn't reload, please refresh manually.</small></p>";
    html += "</div></body></html>";

    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());

    delay(1000);
    Serial.println("Reboot requested via web interface - restarting device...");
    ESP.restart();
}

// ============================================================================
// MODBUS FUNCTIONS
// ============================================================================

void setup_modbus() {
    Serial.println("\n========================================");
    Serial.println("Initializing Modbus RTU Slave...");
    Serial.println("========================================");

    // Initialize UART for RS485 (HW-519 module)
    Serial1.begin(MB_UART_BAUD, SERIAL_8N1, MB_UART_RX, MB_UART_TX);

    Serial.printf("UART1: TX=GPIO%d, RX=GPIO%d, Baud=%d\n",
                  MB_UART_TX, MB_UART_RX, MB_UART_BAUD);
    Serial.println("HW-519: Automatic flow control (no RTS needed)");

    // Configure Modbus RTU slave
    mb.begin(&Serial1);
    mb.slave(modbus_slave_id);

    // Add holding registers (0-11) - Read/Write
    for (uint16_t i = 0; i < 12; i++) {
        mb.addHreg(i, 0);
    }

    // Add input registers (0-8) - Read Only
    for (uint16_t i = 0; i < 9; i++) {
        mb.addIreg(i, 0);
    }

    // Set up callbacks
    mb.onGetHreg(0, cb_read_holding_register, 12);   // Holding regs 0-11
    mb.onSetHreg(0, cb_write_holding_register, 12);  // Allow writes to holding regs
    mb.onGetIreg(0, cb_read_input_register, 9);      // Input regs 0-8

    Serial.printf("Modbus Slave ID: %d\n", modbus_slave_id);
    Serial.println("Holding Registers: 0-11 (Read/Write)");
    Serial.println("Input Registers: 0-8 (Read Only)");
    Serial.println("Modbus RTU Slave initialized!");
    Serial.println("========================================\n");
}

// Callback: Read Holding Register
uint16_t cb_read_holding_register(TRegister* reg, uint16_t val) {
    modbus_request_count++;
    modbus_read_count++;

    uint16_t addr = reg->address.address;

    // Increment sequential counter on every read of register 0
    if (addr == 0) {
        holding_regs.sequential_counter++;
    }

    // Return the appropriate register value
    switch (addr) {
        case 0: return holding_regs.sequential_counter;
        case 1: return holding_regs.random_number;
        case 2: return holding_regs.uptime_seconds;
        case 3: return holding_regs.free_heap_kb_low;
        case 4: return holding_regs.free_heap_kb_high;
        case 5: return holding_regs.min_heap_kb;
        case 6: return holding_regs.cpu_freq_mhz;
        case 7: return holding_regs.task_count;
        case 8: return holding_regs.temperature_x10;
        case 9: return holding_regs.cpu_cores;
        case 10: return holding_regs.wifi_enabled;
        case 11: return holding_regs.wifi_clients;
        default: return 0;
    }
}

// Callback: Read Input Register
uint16_t cb_read_input_register(TRegister* reg, uint16_t val) {
    modbus_request_count++;
    modbus_read_count++;

    uint16_t addr = reg->address.address;

    // Return the appropriate register value
    switch (addr) {
        case 0: return input_regs.sf6_density;
        case 1: return input_regs.sf6_pressure_20c;
        case 2: return input_regs.sf6_temperature;
        case 3: return input_regs.sf6_pressure_var;
        case 4: return input_regs.slave_id;
        case 5: return input_regs.serial_hi;
        case 6: return input_regs.serial_lo;
        case 7: return input_regs.sw_release;
        case 8: return input_regs.quartz_freq;
        default: return 0;
    }
}

// Callback: Write Holding Register
uint16_t cb_write_holding_register(TRegister* reg, uint16_t val) {
    modbus_request_count++;
    modbus_write_count++;

    uint16_t addr = reg->address.address;

    // Allow writing to register 0 (sequential counter)
    if (addr == 0) {
        holding_regs.sequential_counter = val;
        Serial.printf("Modbus Write: Register 0 = %d\n", val);
    }

    return val;
}

// ============================================================================
// LORAWAN FUNCTIONS
// ============================================================================

// Generate random LoRaWAN credentials on first boot
void generate_lorawan_credentials() {
    Serial.println(">>> Generating new LoRaWAN credentials...");

    // Seed random number generator with hardware
    randomSeed(esp_random());

    // Generate random JoinEUI (8 bytes)
    joinEUI = 0;
    for (int i = 0; i < 8; i++) {
        joinEUI = (joinEUI << 8) | (uint64_t)random(0, 256);
    }
    Serial.printf("    Generated JoinEUI: 0x%016llX\n", joinEUI);

    // Generate DevEUI from ESP32 MAC address + random bytes for uniqueness
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    devEUI = ((uint64_t)mac[0] << 56) | ((uint64_t)mac[1] << 48) |
             ((uint64_t)mac[2] << 40) | ((uint64_t)mac[3] << 32) |
             ((uint64_t)random(0, 256) << 24) | ((uint64_t)random(0, 256) << 16) |
             ((uint64_t)random(0, 256) << 8) | (uint64_t)random(0, 256);
    Serial.printf("    Generated DevEUI: 0x%016llX (MAC-based)\n", devEUI);

    // Generate random AppKey (16 bytes)
    for (int i = 0; i < 16; i++) {
        appKey[i] = random(0, 256);
    }
    Serial.print("    Generated AppKey: ");
    for (int i = 0; i < 16; i++) {
        Serial.printf("%02X", appKey[i]);
    }
    Serial.println();

    // Generate random NwkKey (16 bytes) - for LoRaWAN 1.0.x, copy AppKey
    memcpy(nwkKey, appKey, 16);

    Serial.println(">>> Generated unique credentials");
}

// Load LoRaWAN credentials from NVS
void load_lorawan_credentials() {
    Serial.println(">>> Opening LoRaWAN preferences namespace...");

    // Try to open in read-only mode first
    if (!preferences.begin("lorawan", true)) {
        // Namespace doesn't exist - this is first boot
        Serial.println(">>> Namespace doesn't exist (first boot)");
        Serial.println(">>> Generating new credentials...");

        // Generate and save new credentials
        generate_lorawan_credentials();
        save_lorawan_credentials();
        return;
    }

    Serial.println(">>> Preferences namespace opened");

    // Check if credentials exist
    bool hasCredentials = preferences.getBool("has_creds", false);
    Serial.printf(">>> Credentials exist flag: %s\n", hasCredentials ? "YES" : "NO");

    if (hasCredentials) {
        Serial.println(">>> Loading LoRaWAN credentials from NVS...");

        // Load JoinEUI and DevEUI
        joinEUI = preferences.getULong64("joinEUI", 0);
        devEUI = preferences.getULong64("devEUI", 0);

        // Load AppKey
        size_t appKeyLen = preferences.getBytes("appKey", appKey, 16);
        Serial.printf("    Loaded AppKey (%d bytes)\n", appKeyLen);

        // Load NwkKey
        size_t nwkKeyLen = preferences.getBytes("nwkKey", nwkKey, 16);
        Serial.printf("    Loaded NwkKey (%d bytes)\n", nwkKeyLen);

        Serial.println(">>> Loaded credentials from storage");
    } else {
        Serial.println(">>> No stored credentials found - will generate new ones");
        preferences.end();

        // Generate and save new credentials
        generate_lorawan_credentials();
        save_lorawan_credentials();
        return;
    }

    preferences.end();
    Serial.println(">>> Preferences closed");
}

// Save LoRaWAN credentials to NVS
void save_lorawan_credentials() {
    Serial.println(">>> Opening LoRaWAN preferences for writing...");
    if (!preferences.begin("lorawan", false)) {  // Read-write mode
        Serial.println(">>> Failed to open LoRaWAN preferences for writing");
        return;
    }
    Serial.println(">>> Preferences opened for writing");

    Serial.println(">>> Saving LoRaWAN credentials to NVS...");

    size_t written;

    written = preferences.putBool("has_creds", true);
    Serial.printf("    has_creds: %d bytes written\n", written);

    written = preferences.putULong64("joinEUI", joinEUI);
    Serial.printf("    joinEUI: %d bytes written (0x%016llX)\n", written, joinEUI);

    written = preferences.putULong64("devEUI", devEUI);
    Serial.printf("    devEUI: %d bytes written (0x%016llX)\n", written, devEUI);

    written = preferences.putBytes("appKey", appKey, 16);
    Serial.printf("    appKey: %d bytes written\n", written);

    written = preferences.putBytes("nwkKey", nwkKey, 16);
    Serial.printf("    nwkKey: %d bytes written\n", written);

    preferences.end();

    Serial.println(">>> Credentials saved to non-volatile storage");
}

// Save ONLY nonces to NVS (session persistence doesn't work with RadioLib 7.4.0)
void save_lorawan_session() {
    Serial.println(">>> saveLoRaWANNonces() called");

    // Get NONCES buffer from RadioLib (needed to track DevNonce)
    uint8_t* noncesPtr = node.getBufferNonces();
    if (noncesPtr == nullptr) {
        Serial.println(">>> ERROR: No nonces buffer available");
        return;
    }
    const size_t noncesSize = RADIOLIB_LORAWAN_NONCES_BUF_SIZE;
    uint8_t noncesBuffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
    memcpy(noncesBuffer, noncesPtr, noncesSize);

    if (!preferences.begin("lorawan", false)) {
        Serial.println(">>> Failed to open preferences");
        return;
    }

    // Save ONLY nonces (not session - session restore returns -1120)
    preferences.putBytes("nonces", noncesBuffer, noncesSize);
    preferences.putBool("has_nonces", true);
    preferences.end();

    Serial.printf(">>> Saved nonces (%d bytes) - DevNonce will persist across reboots\n", noncesSize);
}

// Load LoRaWAN session from NVS (to avoid rejoining)
void load_lorawan_session() {
    Serial.println(">>> load_lorawan_session() called");

    if (!preferences.begin("lorawan", true)) {
        Serial.println(">>> ERROR: Cannot open preferences to load session");
        return;
    }

    // Debug: Check all keys in namespace
    bool hasCreds = preferences.getBool("has_creds", false);
    bool hasSession = preferences.getBool("has_session", false);
    Serial.printf(">>> has_creds flag: %s\n", hasCreds ? "true" : "false");
    Serial.printf(">>> has_session flag: %s\n", hasSession ? "true" : "false");

    if (!hasSession) {
        Serial.println(">>> No saved session found in NVS");
        preferences.end();
        return;
    }

    // Allocate buffer for session
    const size_t sessionSize = RADIOLIB_LORAWAN_SESSION_BUF_SIZE;
    uint8_t sessionBuffer[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];

    Serial.printf(">>> Reading %d bytes from NVS...\n", sessionSize);
    size_t bytesRead = preferences.getBytes("session", sessionBuffer, sessionSize);
    preferences.end();

    Serial.printf(">>> Read %d bytes from NVS\n", bytesRead);

    if (bytesRead != sessionSize) {
        Serial.printf(">>> ERROR: Session read mismatch: expected %d, got %d bytes\n", sessionSize, bytesRead);
        return;
    }

    Serial.printf(">>> First 16 bytes of loaded session: ");
    for (int i = 0; i < 16; i++) {
        Serial.printf("%02X ", sessionBuffer[i]);
    }
    Serial.println();

    Serial.printf(">>> Calling setBufferSession()...\n");

    // Restore session to RadioLib (only needs buffer pointer)
    int16_t state = node.setBufferSession(sessionBuffer);
    Serial.printf(">>> setBufferSession() returned: %d\n", state);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(">>> Session restored successfully!");
        lorawan_joined = true;

        uint32_t devAddr = node.getDevAddr();
        Serial.printf(">>> Restored DevAddr: 0x%08X\n", devAddr);
    } else {
        Serial.printf(">>> ERROR: Session restore failed, code %d\n", state);
        // Clear invalid session
        preferences.begin("lorawan", false);
        preferences.putBool("has_session", false);
        preferences.end();
        Serial.println(">>> Cleared invalid session from NVS");
    }
}

// Print LoRaWAN credentials to serial console
void print_lorawan_credentials() {
    Serial.println("\n========================================");
    Serial.println("LoRaWAN Device Credentials");
    Serial.println("========================================");

    Serial.print("JoinEUI (AppEUI): 0x");
    Serial.println((unsigned long long)joinEUI, HEX);

    Serial.print("DevEUI:           0x");
    Serial.println((unsigned long long)devEUI, HEX);

    Serial.print("AppKey:           ");
    for (int i = 0; i < 16; i++) {
        if (appKey[i] < 0x10) Serial.print("0");
        Serial.print(appKey[i], HEX);
    }
    Serial.println();

    Serial.print("NwkKey:           ");
    for (int i = 0; i < 16; i++) {
        if (nwkKey[i] < 0x10) Serial.print("0");
        Serial.print(nwkKey[i], HEX);
    }
    Serial.println();

    Serial.println("========================================");
    Serial.println("Copy these credentials to your network server:");
    Serial.println("  - The Things Network (TTN)");
    Serial.println("  - Chirpstack");
    Serial.println("  - AWS IoT Core for LoRaWAN");
    Serial.println("========================================\n");
}

// ============================================================================
// WIFI PASSWORD MANAGEMENT
// ============================================================================

// Generate a strong random WiFi password
void generate_wifi_password() {
    Serial.println(">>> Generating new WiFi password...");

    // Character set for password: uppercase and digits only (display font supports these)
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const int charset_size = sizeof(charset) - 1;

    // Generate 16 character random password
    for (int i = 0; i < 16; i++) {
        wifi_password[i] = charset[esp_random() % charset_size];
    }
    wifi_password[16] = '\0';  // Null terminator

    Serial.printf(">>> Generated password: %s\n", wifi_password);
}

// Load WiFi password from NVS, or generate if not exists
void load_wifi_password() {
    if (!preferences.begin("wifi", false)) {  // Read-write (creates namespace if not exists)
        Serial.println(">>> Failed to open wifi preferences");
        // Fallback: generate password but can't save it
        generate_wifi_password();
        return;
    }

    bool has_password = preferences.getBool("has_password", false);

    if (has_password) {
        String stored_password = preferences.getString("password", "");
        if (stored_password.length() == 16) {
            strcpy(wifi_password, stored_password.c_str());
            Serial.println(">>> Loaded WiFi password from NVS");
            preferences.end();
            return;  // Password loaded successfully
        } else {
            Serial.println(">>> Invalid stored password, will generate new");
            has_password = false;
        }
    }

    preferences.end();

    // Generate new password if none exists
    if (!has_password) {
        generate_wifi_password();
        save_wifi_password();
    }
}

// Save WiFi password to NVS
void save_wifi_password() {
    if (!preferences.begin("wifi", false)) {  // Read-write
        Serial.println(">>> Failed to open wifi preferences for writing");
        return;
    }

    preferences.putString("password", wifi_password);
    preferences.putBool("has_password", true);
    preferences.end();

    Serial.println(">>> WiFi password saved to NVS");
}

// Load WiFi client credentials from NVS
void load_wifi_client_credentials() {
    if (!preferences.begin("wifi", true)) {  // Read-only
        Serial.println(">>> Failed to open wifi preferences");
        return;
    }

    bool has_client_config = preferences.getBool("has_client", false);

    if (has_client_config) {
        String ssid = preferences.getString("client_ssid", "");
        String password = preferences.getString("client_password", "");

        if (ssid.length() > 0 && ssid.length() <= 32) {
            strncpy(wifi_client_ssid, ssid.c_str(), sizeof(wifi_client_ssid) - 1);
            wifi_client_ssid[sizeof(wifi_client_ssid) - 1] = '\0';

            strncpy(wifi_client_password, password.c_str(), sizeof(wifi_client_password) - 1);
            wifi_client_password[sizeof(wifi_client_password) - 1] = '\0';

            Serial.println(">>> Loaded WiFi client credentials from NVS");
            Serial.printf("    SSID: %s\n", wifi_client_ssid);
        }
    } else {
        Serial.println(">>> No WiFi client credentials found");
    }

    preferences.end();
}

// Save WiFi client credentials to NVS
void save_wifi_client_credentials() {
    if (!preferences.begin("wifi", false)) {  // Read-write
        Serial.println(">>> Failed to open wifi preferences for writing");
        return;
    }

    preferences.putString("client_ssid", wifi_client_ssid);
    preferences.putString("client_password", wifi_client_password);
    preferences.putBool("has_client", strlen(wifi_client_ssid) > 0);
    preferences.end();

    Serial.println(">>> WiFi client credentials saved to NVS");
    Serial.printf("    SSID: %s\n", wifi_client_ssid);
}

// Load SF6 base values from NVS
void load_sf6_values() {
    if (!preferences.begin("sf6", true)) {  // Read-only
        Serial.println(">>> Failed to open sf6 preferences");
        return;
    }

    bool has_values = preferences.getBool("has_values", false);

    if (has_values) {
        sf6_base_density = preferences.getFloat("density", 25.0);
        sf6_base_pressure = preferences.getFloat("pressure", 550.0);
        sf6_base_temperature = preferences.getFloat("temperature", 293.0);

        Serial.println(">>> SF6 values loaded from NVS");
        Serial.printf("    Density: %.2f kg/m3\n", sf6_base_density);
        Serial.printf("    Pressure: %.1f kPa\n", sf6_base_pressure);
        Serial.printf("    Temperature: %.1f K\n", sf6_base_temperature);
    } else {
        Serial.println(">>> No stored SF6 values, using defaults");
    }

    preferences.end();
}

// Save SF6 base values to NVS
void save_sf6_values() {
    if (!preferences.begin("sf6", false)) {  // Read-write
        Serial.println(">>> Failed to open sf6 preferences for writing");
        return;
    }

    preferences.putFloat("density", sf6_base_density);
    preferences.putFloat("pressure", sf6_base_pressure);
    preferences.putFloat("temperature", sf6_base_temperature);
    preferences.putBool("has_values", true);
    preferences.end();

    Serial.println(">>> SF6 values saved to NVS");
    Serial.printf("    Density: %.2f kg/m3\n", sf6_base_density);
    Serial.printf("    Pressure: %.1f kPa\n", sf6_base_pressure);
    Serial.printf("    Temperature: %.1f K\n", sf6_base_temperature);
}

// Display WiFi SSID and password on E-Ink screen during startup
void display_password_on_screen(const char* ssid) {
    display.clear();
    display.fillRect(0, 0, 296, 128, BLACK);
    display.drawRect(0, 0, 296, 128, WHITE);

    // Title
    draw_text(50, 5, "WiFi AP Credentials", 1);
    display.drawLine(0, 17, 295, 17, WHITE);

    // SSID
    draw_text(5, 25, "SSID:", 1);
    draw_text(5, 35, ssid, 1);

    // Password
    draw_text(5, 55, "Password:", 2);
    draw_text(5, 75, wifi_password, 2);

    // Instructions
    draw_text(5, 100, "Connect using above", 1);
    draw_text(5, 110, "Screen updates in 20s", 1);

    display.update();

    Serial.println("\n========================================");
    Serial.println("WiFi AP credentials displayed on screen");
    Serial.println("========================================\n");
}

void setup_lorawan() {
    Serial.println("\n========================================");
    Serial.println("Initializing LoRaWAN...");
    Serial.println("========================================");

    // LoRa radio initializes SPI bus first
    // E-Ink display will share this SPI bus later
    Serial.println("Initializing LoRa radio on SPI bus...");
    Serial.printf("  LoRa pins: SCK=%d, MISO=%d, MOSI=%d, NSS=%d\n", LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
    Serial.printf("  LoRa control: DIO1=%d, RESET=%d, BUSY=%d\n", LORA_DIO1, LORA_NRST, LORA_BUSY);

    int state;  // Declare once for all operations

    // Initialize SPI bus explicitly for LoRa radio
    Serial.print("Initializing SPI bus... ");
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
    Serial.println("done");
    delay(100);

    // Initialize radio hardware minimally
    Serial.print("Initializing SX1262... ");
    state = radio.begin();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("success");
    } else {
        Serial.print("failed, code ");
        Serial.println(state);
    }

    // Configure TCXO - Vision Master E290 uses crystal oscillator (not TCXO)
    Serial.print("Configuring oscillator (crystal mode)... ");
    state = radio.setTCXO(0);  // 0 = disable TCXO, use crystal
    Serial.println(state == RADIOLIB_ERR_NONE ? "success" : "failed");

    Serial.print("Configuring RF switch (DIO2)... ");
    state = radio.setDio2AsRfSwitch(true);
    Serial.println(state == RADIOLIB_ERR_NONE ? "success" : "failed");

    // Configure current limit (helps with power stability during RX)
    Serial.print("Configuring current limit... ");
    state = radio.setCurrentLimit(140);  // 140 mA
    Serial.println(state == RADIOLIB_ERR_NONE ? "success" : "failed");

    // Credentials already printed by print_lorawan_credentials() in setup()

    // NOTE: Full session persistence disabled due to RadioLib 7.4.0 limitation
    // setBufferSession() returns -1120 (signature mismatch)
    // BUT we MUST persist nonces to avoid DevNonce replay errors!
    Serial.println("\nChecking for saved nonces (required for DevNonce tracking)...");

    bool sessionRestored = false;
    bool noncesRestored = false;

    // Try to restore ONLY nonces (not full session)
    if (preferences.begin("lorawan", true)) {
        bool hasNonces = preferences.getBool("has_nonces", false);
        Serial.printf(">>> has_nonces flag: %s\n", hasNonces ? "true" : "false");
        preferences.end();

        if (hasNonces) {
            Serial.println(">>> Found saved nonces - restoring...");

            // Initialize node first
            node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);

            // Load ONLY nonces from NVS
            if (preferences.begin("lorawan", true)) {
                const size_t noncesSize = RADIOLIB_LORAWAN_NONCES_BUF_SIZE;
                uint8_t noncesBuffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
                size_t noncesRead = preferences.getBytes("nonces", noncesBuffer, noncesSize);
                preferences.end();

                if (noncesRead == noncesSize) {
                    Serial.printf(">>> Loaded nonces (%d bytes)\n", noncesRead);
                    int16_t state = node.setBufferNonces(noncesBuffer);
                    Serial.printf(">>> setBufferNonces() returned: %d\n", state);

                    if (state == RADIOLIB_ERR_NONE) {
                        noncesRestored = true;
                        Serial.println(">>> Nonces restored - DevNonce will continue from last value");
                    } else {
                        Serial.printf(">>> Nonces restore failed: %d\n", state);
                    }
                }
            }
        }
    }

    // Initialize node if nonces weren't restored
    if (!noncesRestored) {
        Serial.println("\nInitializing LoRaWAN node...");
        Serial.println("Region: EU868");
        node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
    } else {
        Serial.println("\nNonces restored - proceeding with fresh join using incremented DevNonce...");
    }
    Serial.println("LoRaWAN credentials configured");

    // Attempt OTAA join
    Serial.println("\nAttempting OTAA join...");
    Serial.println("This may take 30-60 seconds...");
    Serial.println("Waiting for join accept from network server...");

    state = node.activateOTAA();

    // Check for successful join: RadioLib returns negative codes for LoRaWAN-specific states
    // -1118 = RADIOLIB_LORAWAN_NEW_SESSION (new join successful)
    // -1117 = RADIOLIB_LORAWAN_SESSION_RESTORED (restored from persistent storage)
    if (state == RADIOLIB_LORAWAN_NEW_SESSION || state == RADIOLIB_LORAWAN_SESSION_RESTORED) {
        Serial.println("\nJoin successful!");
        if (state == RADIOLIB_LORAWAN_NEW_SESSION) {
            Serial.println("Status: New LoRaWAN session established");
        } else {
            Serial.println("Status: Previous session restored");
        }
        Serial.print("DevAddr: 0x");
        Serial.println(node.getDevAddr(), HEX);
        lorawan_joined = true;

        // Send immediate uplink after join to initialize frame counters
        // This will also save the session with proper state
        Serial.println("\n>>> Sending immediate uplink after join...");
        delay(1000);  // Brief delay to ensure join is fully processed

        // Update sensor data before first uplink (otherwise payload is empty)
        update_input_registers();
        send_lorawan_uplink();
    } else {
        Serial.print("\nJoin failed, code ");
        Serial.println(state);

        // Print helpful error messages
        if (state == RADIOLIB_ERR_NO_JOIN_ACCEPT) {  // -1116
            Serial.println("Error: No Join-Accept received (RADIOLIB_ERR_NO_JOIN_ACCEPT)");
            Serial.println("  - Join request transmitted");
            Serial.println("  - No response received from network server");
            Serial.println("  - Check: Device registered in network server");
            Serial.println("  - Check: Gateway in range and online");
            Serial.println("  - Check: Credentials (joinEUI, devEUI, appKey)");
        } else if (state == RADIOLIB_ERR_CHIP_NOT_FOUND) {  // -1116 (same value, different context)
            Serial.println("Error: Radio communication lost (RADIOLIB_ERR_CHIP_NOT_FOUND)");
            Serial.println("  - SPI communication failure");
            Serial.println("  - Check: SPI bus conflicts");
            Serial.println("  - Check: Radio power and connections");
        } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {  // -101
            Serial.println("Error: Transmission timeout (RADIOLIB_ERR_TX_TIMEOUT)");
            Serial.println("  - Join request failed to transmit");
            Serial.println("  - Check: Radio configuration");
            Serial.println("  - Check: Antenna connection");
        } else {
            Serial.println("Error: Unknown error code");
            Serial.println("  - See RadioLib documentation for error code details");
            Serial.println("  - https://jgromes.github.io/RadioLib/group__status__codes.html");
        }

        Serial.println("\nWill retry in next cycle...");
        lorawan_joined = false;
    }

    Serial.println("========================================\n");
}

void send_lorawan_uplink() {
    if (!lorawan_joined) {
        Serial.println("LoRaWAN: Not joined, skipping uplink");
        return;
    }

    Serial.println("========================================");
    Serial.println("Sending LoRaWAN uplink...");

    // Set device status for LoRaWAN network server
    // Battery level: 0 = external power, 1-254 = battery level, 255 = unable to measure
    // Using 0 since this is a mains-powered device
    node.setDeviceStatus(0);

    // Prepare payload in AdeunisModbusSf6 format (10 bytes)
    // The decoder expects 20 hex characters (10 bytes):
    // - First 4 hex chars (2 bytes): Header - skipped by decoder
    // - Remaining 16 hex chars (8 bytes): Modbus data (4 registers × 2 bytes)
    //
    // Payload structure:
    // Bytes 0-1: Header (uplink counter for tracking, decoder ignores these)
    // Bytes 2-3: SF6 Density (kg/m³ × 100, big-endian)
    // Bytes 4-5: SF6 Pressure @20C (bar × 1000, big-endian)
    // Bytes 6-7: SF6 Temperature (Kelvin × 10, big-endian)
    // Bytes 8-9: SF6 Pressure Variance (bar × 1000, big-endian)
    uint8_t payload[10];

    // Header (bytes 0-1) - Uplink counter for tracking
    // Decoder skips these, but useful for debugging/tracking
    payload[0] = (lorawan_uplink_count >> 8) & 0xFF;
    payload[1] = lorawan_uplink_count & 0xFF;

    // Modbus register data (bytes 2-9) - TrafagH72517oModbusSf6 format
    // Register 0: SF6 Density (2 bytes, big-endian)
    // Modbus: kg/m³ × 100 → LoRa: kg/m³ × 100 (no conversion needed)
    payload[2] = (input_regs.sf6_density >> 8) & 0xFF;
    payload[3] = input_regs.sf6_density & 0xFF;

    // Register 1: SF6 Pressure @20C (2 bytes, big-endian)
    // Modbus: kPa × 10 → LoRa: bar × 1000
    // Convert: kPa × 10 = bar × 1000 (since 1 bar = 100 kPa)
    // (kPa × 10) = (bar × 100 × 10) = (bar × 1000) ✓
    // So: just send the register value as-is
    payload[4] = (input_regs.sf6_pressure_20c >> 8) & 0xFF;
    payload[5] = input_regs.sf6_pressure_20c & 0xFF;

    // Register 2: SF6 Temperature (2 bytes, big-endian)
    // Modbus: K × 10 → LoRa: K × 10 (no conversion needed)
    payload[6] = (input_regs.sf6_temperature >> 8) & 0xFF;
    payload[7] = input_regs.sf6_temperature & 0xFF;

    // Register 3: SF6 Pressure Variance (2 bytes, big-endian)
    // Modbus: kPa × 10 → LoRa: bar × 1000 (same conversion as pressure @20C)
    payload[8] = (input_regs.sf6_pressure_var >> 8) & 0xFF;
    payload[9] = input_regs.sf6_pressure_var & 0xFF;

    // Print payload in hex for debugging (AdeunisModbusSf6 format)
    Serial.print("Payload (");
    Serial.print(sizeof(payload));
    Serial.print(" bytes): ");
    for (size_t i = 0; i < sizeof(payload); i++) {
        if (payload[i] < 0x10) Serial.print("0");
        Serial.print(payload[i], HEX);
    }
    Serial.println();

    Serial.println("Payload breakdown (AdeunisModbusSf6 format):");
    Serial.printf("  Header (uplink #%u) - bytes 0-1 skipped by decoder\n",
        (payload[0] << 8) | payload[1]);
    Serial.printf("  SF6 Density: %u (%.2f kg/m³)\n",
        (payload[2] << 8) | payload[3],
        ((payload[2] << 8) | payload[3]) / 100.0);
    Serial.printf("  SF6 Pressure @20C: %u (%.3f bar)\n",
        (payload[4] << 8) | payload[5],
        ((payload[4] << 8) | payload[5]) / 1000.0);
    Serial.printf("  SF6 Temperature: %u (%.1f °C)\n",
        (payload[6] << 8) | payload[7],
        (((payload[6] << 8) | payload[7]) / 10.0) - 273.15);
    Serial.printf("  SF6 Pressure Var: %u (%.3f bar)\n",
        (payload[8] << 8) | payload[9],
        ((payload[8] << 8) | payload[9]) / 1000.0);

    // Send unconfirmed uplink on port 1 (RadioLib 7.x API)
    uint8_t downlinkPayload[256];
    size_t downlinkSize = 0;

    int state = node.sendReceive(payload, sizeof(payload), 1, downlinkPayload, &downlinkSize);

    // RadioLib sendReceive() return values:
    // < 0: Error occurred
    // 0: Success, no downlink
    // 1: Success, downlink in RX1 window
    // 2: Success, downlink in RX2 window
    if (state >= RADIOLIB_ERR_NONE) {
        Serial.println("Uplink successful!");
        lorawan_uplink_count++;

        // Get RSSI and SNR from last transmission
        lorawan_last_rssi = radio.getRSSI();
        lorawan_last_snr = radio.getSNR();

        Serial.print("RSSI: ");
        Serial.print(lorawan_last_rssi);
        Serial.print(" dBm, SNR: ");
        Serial.print(lorawan_last_snr);
        Serial.println(" dB");

        // Check if downlink was received
        if (state > 0) {
            Serial.print("Downlink received in RX");
            Serial.print(state);
            Serial.println(" window");
            lorawan_downlink_count++;

            // Check if downlink has data
            if (downlinkSize > 0) {
                Serial.print("Downlink payload (");
                Serial.print(downlinkSize);
                Serial.print(" bytes): ");
                for (size_t i = 0; i < downlinkSize; i++) {
                    if (downlinkPayload[i] < 0x10) Serial.print("0");
                    Serial.print(downlinkPayload[i], HEX);
                    Serial.print(" ");
                }
                Serial.println();
            } else {
                Serial.println("Downlink ACK received (no payload)");
            }
        }

        // Save nonces after uplink to persist DevNonce
        save_lorawan_session();
    } else {
        Serial.print("Uplink failed, code ");
        Serial.println(state);
    }

    Serial.print("Total uplinks: ");
    Serial.print(lorawan_uplink_count);
    Serial.print(", downlinks: ");
    Serial.println(lorawan_downlink_count);
    Serial.println("========================================\n");
}
