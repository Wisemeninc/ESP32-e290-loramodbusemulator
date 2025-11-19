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
#include <ModbusIP_ESP8266.h>
#include <RadioLib.h>
#include "mbedtls/base64.h"
#include "server_cert.h"

// Component headers (Gradual Integration - Phase 1)
#include "config.h"
#include "modbus_handler.h"
#include "wifi_manager.h"
#include "auth_manager.h"
#include "display_manager.h"
#include "lorawan_handler.h"

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
// FIRMWARE_VERSION now defined in config.h (Phase 1 integration)
// Examples: 111 = v1.11, 203 = v2.03, 1545 = v15.45
// Display format: v(FIRMWARE_VERSION/100).(FIRMWARE_VERSION%100)

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

// Modbus TCP instance
ModbusIP mbTCP;

// Statistics (combined for both RTU and TCP)
uint32_t modbus_request_count = 0;
uint32_t modbus_read_count = 0;
uint32_t modbus_write_count = 0;
uint32_t modbus_error_count = 0;
uint32_t modbus_tcp_request_count = 0;
uint32_t modbus_tcp_clients = 0;

// Modbus registers - using component definitions (Phase 2 integration)
// Structs now defined in modbus_handler.h
// Keep local variables for compatibility with existing code
HoldingRegisters holding_regs;
InputRegisters input_regs;

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
bool modbus_tcp_enabled = false;  // Modbus TCP enable flag (default: disabled)
bool wifi_ap_active = false;
bool wifi_client_connected = false;
unsigned long wifi_start_time = 0;
// WIFI_TIMEOUT_MS now defined in config.h (Phase 1 integration)
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

// Old component setup functions removed - now using component classes
void setup_web_server();
void setup_redirect_server();
void sync_tcp_registers();
void update_holding_registers();
void update_input_registers();
void handle_root(HTTPRequest * req, HTTPResponse * res);
void handle_stats(HTTPRequest * req, HTTPResponse * res);
void handle_registers(HTTPRequest * req, HTTPResponse * res);
void handle_sf6_update(HTTPRequest * req, HTTPResponse * res);
void handle_sf6_reset(HTTPRequest * req, HTTPResponse * res);
void handle_config(HTTPRequest * req, HTTPResponse * res);
void handle_lorawan(HTTPRequest * req, HTTPResponse * res);
void handle_lorawan_config(HTTPRequest * req, HTTPResponse * res);
void handle_lorawan_profiles(HTTPRequest * req, HTTPResponse * res);
void handle_lorawan_profile_update(HTTPRequest * req, HTTPResponse * res);
void handle_lorawan_profile_toggle(HTTPRequest * req, HTTPResponse * res);
void handle_lorawan_profile_activate(HTTPRequest * req, HTTPResponse * res);
void handle_lorawan_auto_rotate(HTTPRequest * req, HTTPResponse * res);
void handle_wifi(HTTPRequest * req, HTTPResponse * res);
void handle_reboot(HTTPRequest * req, HTTPResponse * res);
// Old credential/config functions removed - now in component classes
void load_wifi_client_credentials();  // Still used for web handler compatibility
void save_wifi_client_credentials();  // Still used for web handler compatibility
void load_sf6_values();  // SF6 specific - not in components yet
void save_sf6_values();  // SF6 specific - not in components yet
void handle_wifi_scan(HTTPRequest * req, HTTPResponse * res);
void handle_wifi_connect(HTTPRequest * req, HTTPResponse * res);
void handle_wifi_status(HTTPRequest * req, HTTPResponse * res);
void save_auth_credentials();  // Still used for web handler
String readPostBody(HTTPRequest * req);
bool getPostParameterFromBody(const String& body, const char* name, String& value);
bool getPostParameter(HTTPRequest * req, const char* name, String& value);
bool check_authentication(HTTPRequest * req, HTTPResponse * res);
void handle_security(HTTPRequest * req, HTTPResponse * res);
void handle_security_update(HTTPRequest * req, HTTPResponse * res);
void handle_debug_update(HTTPRequest * req, HTTPResponse * res);
void handle_enable_auth(HTTPRequest * req, HTTPResponse * res);
void handle_factory_reset(HTTPRequest * req, HTTPResponse * res);
void handle_http_redirect(HTTPRequest * req, HTTPResponse * res);

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
    Serial.println("Vision Master E290 - Modbus RTU/TCP");
    Serial.println("========================================");
    Serial.println("Framework: Arduino");
    Serial.println("Display: 2.9\" E-Ink (296x128)");
    Serial.println("========================================\n");

    // Initialize preferences (NVS)
    preferences.begin("modbus", false);
    modbus_slave_id = preferences.getUChar("slave_id", MB_SLAVE_ID_DEFAULT);
    modbus_tcp_enabled = preferences.getBool("tcp_enabled", false);  // Default: disabled
    Serial.printf("Modbus Slave ID: %d\n", modbus_slave_id);
    Serial.printf("Modbus TCP: %s\n", modbus_tcp_enabled ? "Enabled" : "Disabled");
    preferences.end();  // Close modbus namespace

    // Load authentication credentials - Phase 6 integration
    Serial.println("\n>>> Loading authentication credentials...");
    // Old: load_auth_credentials();
    authManager.begin();
    auth_enabled = authManager.isEnabled();  // Update global for web handler compatibility
    Serial.printf("Authentication: %s\n", auth_enabled ? "Enabled" : "Disabled");

    // Load SF6 base values from NVS
    Serial.println("\n>>> Loading SF6 base values...");
    load_sf6_values();

    // Initialize LoRaWAN FIRST (before E-Ink display) - Phase 4 integration
    // This ensures LoRa radio gets clean SPI access
    // E-Ink will be initialized AFTER join completes to avoid SPI conflicts during RX
    Serial.println("\n>>> Initializing LoRaWAN with multi-profile support...");
    // begin() now loads profiles, sets active profile, and prints info
    lorawanHandler.begin();
    
    // Print all profile configurations
    Serial.println("\n========================================");
    Serial.println("LoRa Profile Configuration Summary");
    Serial.println("========================================");
    int enabled_count = lorawanHandler.getEnabledProfileCount();
    Serial.printf("Auto-Rotation: %s\n", lorawanHandler.getAutoRotation() ? "ENABLED" : "DISABLED");
    Serial.printf("Enabled Profiles: %d of %d\n", enabled_count, MAX_LORA_PROFILES);
    Serial.printf("Active Profile: %d\n\n", lorawanHandler.getActiveProfileIndex());
    
    for (int i = 0; i < MAX_LORA_PROFILES; i++) {
        LoRaProfile* prof = lorawanHandler.getProfile(i);
        if (!prof) continue;
        
        Serial.printf("Profile %d: %s\n", i, prof->name);
        Serial.printf("  Status: %s\n", prof->enabled ? "ENABLED" : "DISABLED");
        
        char devEUIStr[17];
        sprintf(devEUIStr, "%016llX", prof->devEUI);
        Serial.printf("  DevEUI: 0x%s\n", devEUIStr);
        
        char joinEUIStr[17];
        sprintf(joinEUIStr, "%016llX", prof->joinEUI);
        Serial.printf("  JoinEUI: 0x%s\n", joinEUIStr);
        
        // Print full AppKey (16 bytes) for ChirpStack configuration
        Serial.print("  AppKey: ");
        for (int j = 0; j < 16; j++) {
            Serial.printf("%02X", prof->appKey[j]);
        }
        Serial.println();
        
        // Print full NwkKey (16 bytes) for ChirpStack configuration
        Serial.print("  NwkKey: ");
        for (int j = 0; j < 16; j++) {
            Serial.printf("%02X", prof->nwkKey[j]);
        }
        Serial.println();
        
        if (i == lorawanHandler.getActiveProfileIndex()) {
            Serial.println("  >>> CURRENTLY ACTIVE <<<");
        }
        Serial.println();
    }
    
    if (lorawanHandler.getAutoRotation() && enabled_count > 1) {
        Serial.println("Auto-Rotation Schedule (5 min per profile, 1 min stagger):");
        int profile_idx = 0;
        for (int i = 0; i < MAX_LORA_PROFILES; i++) {
            LoRaProfile* prof = lorawanHandler.getProfile(i);
            if (prof && prof->enabled) {
                Serial.printf("  Profile %d: T+%dmin, T+%dmin, T+%dmin...\n", 
                    i, profile_idx, profile_idx + 5, profile_idx + 10);
                profile_idx++;
            }
        }
    }
    
    Serial.println("========================================\n");
    
    // Send initial uplink from all enabled profiles (startup announcement)
    Serial.println("\n========================================");
    Serial.println("Startup Uplink Sequence");
    Serial.println("========================================");
    Serial.printf("Sending initial uplinks from %d enabled profile(s)...\n\n", enabled_count);
    
    // Track which profiles we've sent from
    bool sent_profiles[MAX_LORA_PROFILES] = {false};
    int uplinks_sent = 0;
    
    // Get initial profile index
    uint8_t initial_profile = lorawanHandler.getActiveProfileIndex();
    
    // Iterate through all profiles and send from enabled ones
    for (int i = 0; i < MAX_LORA_PROFILES; i++) {
        LoRaProfile* prof = lorawanHandler.getProfile(i);
        if (!prof || !prof->enabled) {
            continue; // Skip disabled profiles
        }
        
        // Switch to this profile if not already active
        if (lorawanHandler.getActiveProfileIndex() != i) {
            Serial.printf("\n>>> Switching to Profile %d: %s\n", i, prof->name);
            lorawanHandler.setActiveProfile(i);
            lorawanHandler.begin(); // Re-initialize with new profile credentials
        }
        
        // Join with this profile
        Serial.printf(">>> Joining network with Profile %d...\n", i);
        lorawan_joined = lorawanHandler.join();
        
        if (lorawan_joined) {
            Serial.printf(">>> Join successful for Profile %d!\n", i);
            delay(1000);  // Brief delay to ensure join is fully processed
            
            // Send uplink
            Serial.printf(">>> Sending startup uplink from Profile %d: %s\n", i, prof->name);
            update_input_registers();  // Update sensor data before uplink
            lorawanHandler.sendUplink(input_regs);
            sent_profiles[i] = true;
            uplinks_sent++;
            
            Serial.printf(">>> Uplink sent from Profile %d (%d/%d)\n", i, uplinks_sent, enabled_count);
            
            // Delay before next profile (10 seconds to allow RX windows to complete)
            if (uplinks_sent < enabled_count) {
                Serial.println(">>> Waiting 10 seconds before next profile...");
                delay(10000);
            }
        } else {
            Serial.printf(">>> Join failed for Profile %d, skipping uplink\n", i);
        }
    }
    
    Serial.println("\n========================================");
    Serial.printf("Startup sequence complete: %d/%d uplinks sent\n", uplinks_sent, enabled_count);
    Serial.println("========================================\n");
    
    // Return to initial profile for normal operation
    if (lorawanHandler.getActiveProfileIndex() != initial_profile) {
        Serial.printf("\n>>> Returning to initial Profile %d for normal operation\n", initial_profile);
        lorawanHandler.setActiveProfile(initial_profile);
        lorawanHandler.begin();
        lorawan_joined = lorawanHandler.join();
    } else {
        // Ensure lorawan_joined reflects the last join status
        lorawan_joined = lorawanHandler.join();
    }

    // Now safe to initialize E-Ink display after join attempt completes - Phase 5 integration
    // The join process (TX + RX windows) is finished, SPI can be shared
    // Old: setup_display();
    displayManager.begin(DISPLAY_ROTATION);
    displayManager.showStartupScreen();

    // Initialize WiFi (Phase 3 integration) - try client mode first, fall back to AP if needed
    Serial.println("\n>>> Loading WiFi client credentials...");
    // Keep old credential loading for web handler compatibility (Phase 8 will replace)
    load_wifi_client_credentials();

    // Initialize WiFi Manager
    wifiManager.begin();

    bool client_connected = false;
    if (strlen(wifi_client_ssid) > 0) {
        // Old: client_connected = setup_wifi_client();
        client_connected = wifiManager.connectClient(wifi_client_ssid, wifi_client_password);
        wifi_client_connected = client_connected;
    }

    // If client mode failed or not configured, start AP mode
    if (!client_connected) {
        // Old: setup_wifi_ap();
        wifiManager.startAP();
        wifi_ap_active = wifiManager.isAPActive();

        // Copy AP password to global variable for web handler compatibility
        String ap_pass_str = wifiManager.getAPPassword();
        strcpy(wifi_password, ap_pass_str.c_str());
    } else {
        Serial.println(">>> WiFi client connected - AP mode disabled");
    }

    setup_web_server();
    setup_redirect_server();

    // Initialize Modbus RTU (Phase 2 integration)
    // Old: setup_modbus();
    modbusHandler.begin(modbus_slave_id);

    // Initialize Modbus TCP (if enabled)
    if (modbus_tcp_enabled) {
        Serial.println("\n>>> Initializing Modbus TCP...");
        mbTCP.server();  // Start Modbus TCP server on port 502
        Serial.println(">>> Modbus TCP server started on port 502");
        Serial.printf(">>> Modbus TCP accessible at: ");
        if (wifi_client_connected) {
            Serial.println(WiFi.localIP());
        } else if (wifi_ap_active) {
            Serial.println(WiFi.softAPIP());
        } else {
            Serial.println("(WiFi not connected)");
        }
    } else {
        Serial.println("\n>>> Modbus TCP is disabled (enable in web configuration)");
    }

    // Initialize registers with default values
    holding_regs.sequential_counter = 0;
    holding_regs.random_number = random(65535);
    holding_regs.cpu_cores = 2;

    // Get MAC address for serial number
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // Set static input register values in the ModbusHandler
    InputRegisters& handler_input_regs = modbusHandler.getInputRegisters();
    handler_input_regs.slave_id = modbus_slave_id;
    // Use last 4 bytes of MAC address as 32-bit serial number
    handler_input_regs.serial_hi = (mac[2] << 8) | mac[3];
    handler_input_regs.serial_lo = (mac[4] << 8) | mac[5];
    handler_input_regs.sw_release = FIRMWARE_VERSION;
    handler_input_regs.quartz_freq = 14750;  // 147.50 Hz

    // Also update local copy for compatibility
    input_regs = handler_input_regs;

    Serial.printf("Device Serial Number: 0x%04X%04X (from MAC: %02X:%02X:%02X:%02X:%02X:%02X)\n",
                  handler_input_regs.serial_hi, handler_input_regs.serial_lo,
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Initialize Modbus TCP registers (shared with RTU) - only if TCP is enabled
    if (modbus_tcp_enabled) {
        // Add holding registers individually (0-11)
        Serial.println("\n>>> Adding Modbus TCP holding registers...");
        for (int i = 0; i < 12; i++) {
            mbTCP.addHreg(i);  // Add each holding register individually
        }

        // Add input registers individually (0-8)
        Serial.println(">>> Adding Modbus TCP input registers...");
        for (int i = 0; i < 9; i++) {
            mbTCP.addIreg(i);  // Add each input register individually
        }
        Serial.println(">>> Modbus TCP registers initialized");

        // Perform initial sync of register values
        Serial.println(">>> Performing initial TCP register sync...");
        sync_tcp_registers();

        // Verify input registers after sync
        Serial.println(">>> Verifying input register values:");
        InputRegisters& test_input = modbusHandler.getInputRegisters();
        Serial.printf("  [0] sf6_density: %u\n", test_input.sf6_density);
        Serial.printf("  [1] sf6_pressure_20c: %u\n", test_input.sf6_pressure_20c);
        Serial.printf("  [2] sf6_temperature: %u\n", test_input.sf6_temperature);
        Serial.printf("  [3] sf6_pressure_var: %u\n", test_input.sf6_pressure_var);
        Serial.printf("  [4] slave_id: %u\n", test_input.slave_id);
        Serial.printf("  [5] serial_hi: %u\n", test_input.serial_hi);
        Serial.printf("  [6] serial_lo: %u\n", test_input.serial_lo);
        Serial.printf("  [7] sw_release: %u\n", test_input.sw_release);
        Serial.printf("  [8] quartz_freq: %u\n", test_input.quartz_freq);
        Serial.println(">>> Initial sync complete");
    }

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
    static unsigned long last_tcp_sync = 0;
    static unsigned long last_profile_uplinks[MAX_LORA_PROFILES] = {0}; // Track last uplink time per profile

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

    // Sync TCP registers every 5 seconds (less frequent to reduce load)
    if (modbus_tcp_enabled && (now - last_tcp_sync >= 5000)) {
        last_tcp_sync = now;
        sync_tcp_registers();
    }

    // Update display every 30 seconds - Phase 5 integration
    if (now - last_display_update >= 30000) {
        last_display_update = now;
        // Old: update_display();
        displayManager.update(
            holding_regs,
            input_regs,
            wifi_ap_active,
            wifi_client_connected,
            modbus_slave_id,
            lorawan_joined,
            lorawanHandler.getUplinkCount(),
            lorawanHandler.getLastRSSI(),
            lorawanHandler.getLastSNR(),
            lorawanHandler.getDevEUI()
        );
    }

    // Send LoRaWAN uplink with staggered timing for multiple profiles
    // Single profile: Every 5 minutes
    // Multiple profiles with auto-rotation: Each profile every 5 minutes, staggered 1 minute apart
    if (lorawan_joined) {
        if (lorawanHandler.getAutoRotation() && lorawanHandler.getEnabledProfileCount() > 1) {
            // Multi-profile mode: Each profile sends every 5 minutes, staggered by 1 minute
            uint8_t current_profile = lorawanHandler.getActiveProfileIndex();
            unsigned long time_since_last = now - last_profile_uplinks[current_profile];
            
            // Check if current profile is due for transmission (5 minutes since its last uplink)
            if (time_since_last >= 300000) {
                Serial.print("Profile ");
                Serial.print(current_profile);
                Serial.println(" is due for uplink (5min elapsed)");
                
                // Send uplink from current profile
                last_profile_uplinks[current_profile] = now;
                last_lorawan_uplink = now;
                lorawanHandler.sendUplink(input_regs);
                
                // After sending, rotate to next enabled profile and join
                if (lorawanHandler.rotateToNextProfile()) {
                    Serial.println("Auto-rotation: Switching to next profile");
                    
                    // Re-join network with new profile credentials
                    lorawan_joined = false;
                    lorawanHandler.begin();
                    if (lorawanHandler.join()) {
                        lorawan_joined = true;
                        Serial.print("Joined with profile ");
                        Serial.println(lorawanHandler.getActiveProfileIndex());
                    } else {
                        Serial.println("Failed to join after profile rotation");
                    }
                }
            } else {
                // Check if we should switch to a different profile that's ready to send
                // This ensures 1-minute staggering between profiles
                uint8_t next_profile = lorawanHandler.getNextEnabledProfile();
                if (next_profile != current_profile) {
                    unsigned long next_time_since_last = now - last_profile_uplinks[next_profile];
                    
                    // If next profile is due AND at least 1 minute has passed since any uplink
                    if (next_time_since_last >= 300000 && (now - last_lorawan_uplink >= 60000)) {
                        Serial.print("Switching to profile ");
                        Serial.print(next_profile);
                        Serial.println(" which is ready to send");
                        
                        // Rotate to next profile
                        if (lorawanHandler.rotateToNextProfile()) {
                            // Re-join network
                            lorawan_joined = false;
                            lorawanHandler.begin();
                            if (lorawanHandler.join()) {
                                lorawan_joined = true;
                                Serial.print("Joined with profile ");
                                Serial.println(lorawanHandler.getActiveProfileIndex());
                                
                                // Send immediately after joining
                                last_profile_uplinks[next_profile] = now;
                                last_lorawan_uplink = now;
                                lorawanHandler.sendUplink(input_regs);
                            } else {
                                Serial.println("Failed to join with next profile");
                            }
                        }
                    }
                }
            }
        } else {
            // Single profile mode: Send every 5 minutes
            if (now - last_lorawan_uplink >= 300000) {
                last_lorawan_uplink = now;
                uint8_t current_profile = lorawanHandler.getActiveProfileIndex();
                last_profile_uplinks[current_profile] = now;
                
                Serial.print("Sending uplink from profile ");
                Serial.println(current_profile);
                lorawanHandler.sendUplink(input_regs);
            }
        }
    }

    // Handle WiFi AP timeout (only if not connected as client)
    // If connected to a WiFi network, keep WiFi active indefinitely
    if (!wifi_client_connected && wifi_ap_active && (now - wifi_start_time >= WIFI_TIMEOUT_MS)) {
        Serial.println("WiFi AP timeout - shutting down");
        WiFi.mode(WIFI_OFF);
        wifi_ap_active = false;
    }

    // Update WiFi client connection status (Phase 3 integration)
    if (wifi_client_connected) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi client disconnected - attempting reconnect...");
            wifi_client_connected = false;
            // Try to reconnect
            // Old: if (setup_wifi_client())
            if (wifiManager.connectClient(wifi_client_ssid, wifi_client_password)) {
                Serial.println("WiFi client reconnected");
                wifi_client_connected = true;
            } else {
                // Reconnection failed - start AP mode
                Serial.println("WiFi client reconnection failed - starting AP mode");
                // Old: setup_wifi_ap();
                wifiManager.startAP();
                wifi_ap_active = wifiManager.isAPActive();

                // Copy AP password to global variable
                String ap_pass_str = wifiManager.getAPPassword();
                strcpy(wifi_password, ap_pass_str.c_str());
            }
        }
    }

    // Handle HTTPS server requests (both in AP mode and client mode)
    if (wifi_ap_active || wifi_client_connected) {
        server.loop();
        redirectServer.loop();
    }

    // Handle Modbus RTU requests (Phase 2 integration)
    // Old: mb.task();
    modbusHandler.task();

    // Handle Modbus TCP requests (only if enabled)
    if (modbus_tcp_enabled && (wifi_ap_active || wifi_client_connected)) {
        mbTCP.task();
        yield();  // Allow other tasks after TCP processing
    }

    yield();  // Allow other tasks to run

    delay(10);
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================


// Simple 5x7 font for letters and digits

// ============================================================================
// REGISTER UPDATE FUNCTIONS
// ============================================================================

// Synchronize RTU registers to TCP (only if TCP is enabled)
void sync_tcp_registers() {
    if (!modbus_tcp_enabled) return;

    // Get RTU registers
    HoldingRegisters& rtu_holding = modbusHandler.getHoldingRegisters();
    InputRegisters& rtu_input = modbusHandler.getInputRegisters();

    // Update TCP holding registers from RTU - using direct struct access
    mbTCP.Hreg(0, rtu_holding.sequential_counter);
    mbTCP.Hreg(1, rtu_holding.random_number);
    mbTCP.Hreg(2, rtu_holding.uptime_seconds);
    mbTCP.Hreg(3, rtu_holding.free_heap_kb_low);
    mbTCP.Hreg(4, rtu_holding.free_heap_kb_high);
    mbTCP.Hreg(5, rtu_holding.min_heap_kb);
    mbTCP.Hreg(6, rtu_holding.cpu_freq_mhz);
    mbTCP.Hreg(7, rtu_holding.task_count);
    mbTCP.Hreg(8, rtu_holding.temperature_x10);
    mbTCP.Hreg(9, rtu_holding.cpu_cores);
    mbTCP.Hreg(10, rtu_holding.wifi_enabled);
    mbTCP.Hreg(11, rtu_holding.wifi_clients);

    // Update TCP input registers from RTU - using direct struct access
    mbTCP.Ireg(0, rtu_input.sf6_density);
    mbTCP.Ireg(1, rtu_input.sf6_pressure_20c);
    mbTCP.Ireg(2, rtu_input.sf6_temperature);
    mbTCP.Ireg(3, rtu_input.sf6_pressure_var);
    mbTCP.Ireg(4, rtu_input.slave_id);
    mbTCP.Ireg(5, rtu_input.serial_hi);
    mbTCP.Ireg(6, rtu_input.serial_lo);
    mbTCP.Ireg(7, rtu_input.sw_release);
    mbTCP.Ireg(8, rtu_input.quartz_freq);
}

void update_holding_registers() {
    // Update the modbus handler's internal registers
    modbusHandler.updateHoldingRegisters(
        (wifi_ap_active || wifi_client_connected),
        wifi_ap_active ? WiFi.softAPgetStationNum() : 0
    );

    // Also update local copy for compatibility
    HoldingRegisters& rtu_holding = modbusHandler.getHoldingRegisters();
    holding_regs = rtu_holding;
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

    portEXIT_CRITICAL(&timerMux);

    // Update the modbus handler's internal registers
    modbusHandler.updateInputRegisters(sf6_base_density, sf6_base_pressure, sf6_base_temperature);

    // Also update local copy for compatibility
    InputRegisters& rtu_input = modbusHandler.getInputRegisters();
    input_regs = rtu_input;
}

// ============================================================================
// WIFI AND WEB SERVER FUNCTIONS
// ============================================================================

// Connect to WiFi client network

void setup_web_server() {
    // Register GET handlers
    ResourceNode * nodeRoot = new ResourceNode("/", "GET", &handle_root);
    ResourceNode * nodeStats = new ResourceNode("/stats", "GET", &handle_stats);
    ResourceNode * nodeRegisters = new ResourceNode("/registers", "GET", &handle_registers);
    ResourceNode * nodeLoRaWAN = new ResourceNode("/lorawan", "GET", &handle_lorawan);
    ResourceNode * nodeLoRaWANProfiles = new ResourceNode("/lorawan/profiles", "GET", &handle_lorawan_profiles);
    ResourceNode * nodeWiFi = new ResourceNode("/wifi", "GET", &handle_wifi);
    ResourceNode * nodeWiFiScan = new ResourceNode("/wifi/scan", "GET", &handle_wifi_scan);
    ResourceNode * nodeWiFiStatus = new ResourceNode("/wifi/status", "GET", &handle_wifi_status);
    ResourceNode * nodeSecurity = new ResourceNode("/security", "GET", &handle_security);

    // Register POST handlers
    ResourceNode * nodeConfig = new ResourceNode("/config", "POST", &handle_config);
    ResourceNode * nodeLoRaWANConfig = new ResourceNode("/lorawan/config", "POST", &handle_lorawan_config);
    ResourceNode * nodeLoRaWANProfileUpdate = new ResourceNode("/lorawan/profile/update", "POST", &handle_lorawan_profile_update);
    ResourceNode * nodeLoRaWANProfileToggle = new ResourceNode("/lorawan/profile/toggle", "GET", &handle_lorawan_profile_toggle);
    ResourceNode * nodeLoRaWANProfileActivate = new ResourceNode("/lorawan/profile/activate", "GET", &handle_lorawan_profile_activate);
    ResourceNode * nodeLoRaWANAutoRotate = new ResourceNode("/lorawan/auto-rotate", "GET", &handle_lorawan_auto_rotate);
    ResourceNode * nodeWiFiConnect = new ResourceNode("/wifi/connect", "POST", &handle_wifi_connect);
    ResourceNode * nodeSecurityUpdate = new ResourceNode("/security/update", "POST", &handle_security_update);
    ResourceNode * nodeDebugUpdate = new ResourceNode("/security/debug", "POST", &handle_debug_update);
    ResourceNode * nodeSF6Update = new ResourceNode("/sf6/update", "GET", &handle_sf6_update);
    ResourceNode * nodeSF6Reset = new ResourceNode("/sf6/reset", "GET", &handle_sf6_reset);
    ResourceNode * nodeEnableAuth = new ResourceNode("/security/enable", "GET", &handle_enable_auth);
    ResourceNode * nodeFactoryReset = new ResourceNode("/factory-reset", "GET", &handle_factory_reset);
    ResourceNode * nodeReboot = new ResourceNode("/reboot", "GET", &handle_reboot);

    server.registerNode(nodeRoot);
    server.registerNode(nodeStats);
    server.registerNode(nodeRegisters);
    server.registerNode(nodeConfig);
    server.registerNode(nodeLoRaWAN);
    server.registerNode(nodeLoRaWANConfig);
    server.registerNode(nodeLoRaWANProfiles);
    server.registerNode(nodeLoRaWANProfileUpdate);
    server.registerNode(nodeLoRaWANProfileToggle);
    server.registerNode(nodeLoRaWANProfileActivate);
    server.registerNode(nodeLoRaWANAutoRotate);
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
    server.registerNode(nodeFactoryReset);
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
    html += "<a href='/lorawan/profiles'>Profiles</a>";
    html += "<a href='/wifi'>WiFi</a>";
    html += "<a href='/security'>Security</a>";
    html += "<a href='/reboot' class='reboot' onclick='return confirm(\"Are you sure you want to reboot the device?\");'>Reboot</a>";
    html += "</div>";
    html += "<h1>Vision Master E290</h1>";
    html += "<div class='card'><strong>Status:</strong> System Running | <strong>Uptime:</strong> " + String(millis() / 1000) + " seconds</div>";

    html += "<div class='info'>";
    html += "<div class='info-item'><div class='info-label'>Modbus Slave ID</div><div class='info-value'>" + String(modbus_slave_id) + "</div></div>";
    ModbusStats& modbus_stats = modbusHandler.getStats();
    html += "<div class='info-item'><div class='info-label'>Modbus RTU Requests</div><div class='info-value'>" + String(modbus_stats.request_count) + "</div></div>";

    // Add Modbus TCP status
    String tcp_status = "Disabled";
    String tcp_color = "#95a5a6";  // Gray for disabled
    if (modbus_tcp_enabled) {
        if (wifi_client_connected) {
            tcp_status = WiFi.localIP().toString() + ":502";
            tcp_color = "#27ae60";  // Green for enabled
        } else if (wifi_ap_active) {
            tcp_status = WiFi.softAPIP().toString() + ":502";
            tcp_color = "#27ae60";  // Green for enabled
        } else {
            tcp_status = "Enabled (WiFi Off)";
            tcp_color = "#e67e22";  // Orange for waiting
        }
    }
    html += "<div class='info-item'><div class='info-label'>Modbus TCP</div><div class='info-value' style='font-size:14px;color:" + tcp_color + ";'>" + tcp_status + "</div></div>";

    // WiFi status - show different info based on mode
    if (wifi_client_connected) {
        html += "<div class='info-item'><div class='info-label'>WiFi Mode</div><div class='info-value' style='font-size:16px;'>Client<br><small style='font-size:12px;color:#7f8c8d;'>" + WiFi.SSID() + "<br>" + WiFi.localIP().toString() + "</small></div></div>";
    } else if (wifi_ap_active) {
        html += "<div class='info-item'><div class='info-label'>WiFi Mode</div><div class='info-value' style='font-size:16px;'>AP Mode<br><small style='font-size:12px;color:#7f8c8d;'>" + String(WiFi.softAPgetStationNum()) + " clients</small></div></div>";
    } else {
        html += "<div class='info-item'><div class='info-label'>WiFi Mode</div><div class='info-value'>OFF</div></div>";
    }

    html += "<div class='info-item'><div class='info-label'>LoRaWAN Status</div><div class='info-value'>" + String(lorawan_joined ? "JOINED" : "NOT JOINED") + "</div></div>";
    html += "<div class='info-item'><div class='info-label'>LoRa Uplinks</div><div class='info-value'>" + String(lorawanHandler.getUplinkCount()) + "</div></div>";
    html += "</div>";

    html += "<h2>Configuration</h2>";
    html += "<form action='/config' method='POST'>";
    html += "<label>Modbus Slave ID:</label>";
    html += "<input type='number' name='slave_id' min='1' max='247' value='" + String(modbus_slave_id) + "' required>";
    html += "<p style='font-size:12px;color:#7f8c8d;margin:5px 0 15px 0;'>Valid range: 1-247 (0=broadcast, 248-255=reserved)</p>";

    html += "<div style='text-align:left;margin:20px 0;'>";
    html += "<label style='display:flex;align-items:center;cursor:pointer;'>";
    html += "<input type='checkbox' name='tcp_enabled' value='1' " + String(modbus_tcp_enabled ? "checked" : "") + " style='width:20px;height:20px;margin-right:10px;'>";
    html += "<span>Enable Modbus TCP (port 502)</span>";
    html += "</label>";
    html += "<p style='font-size:12px;color:#7f8c8d;margin:5px 0 0 30px;'>Requires device restart to take effect</p>";
    html += "</div>";

    html += "<input type='submit' value='Save Configuration'>";
    html += "</form>";

    html += "<div class='footer'>ESP32-S3R8 | Modbus RTU/TCP + E-Ink Display | Arduino Framework</div>";
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
    html += "<a href='/lorawan/profiles'>Profiles</a>";
    html += "<a href='/wifi'>WiFi</a>";
    html += "<a href='/security'>Security</a>";
    html += "<a href='/reboot' class='reboot' onclick='return confirm(\"Are you sure you want to reboot the device?\");'>Reboot</a>";
    html += "</div>";
    html += "<h1>System Statistics</h1>";

    // Modbus Communication - get stats from ModbusHandler
    ModbusStats& stats = modbusHandler.getStats();
    html += "<h2>Modbus Communication</h2>";
    html += "<table><tr><th>Metric</th><th>Value</th><th>Description</th></tr>";
    html += "<tr><td>Total Requests</td><td class='value'>" + String(stats.request_count) + "</td><td>Total Modbus RTU requests received</td></tr>";
    html += "<tr><td>Read Operations</td><td class='value'>" + String(stats.read_count) + "</td><td>Number of read operations</td></tr>";
    html += "<tr><td>Write Operations</td><td class='value'>" + String(stats.write_count) + "</td><td>Number of write operations</td></tr>";
    html += "<tr><td>Error Count</td><td class='value'>" + String(stats.error_count) + "</td><td>Communication errors</td></tr>";
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
    html += "<a href='/lorawan/profiles'>Profiles</a>";
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
    String tcp_enabled_str;

    if (getPostParameterFromBody(postBody, "slave_id", slave_id_str)) {
        int new_id = slave_id_str.toInt();

        // Check if TCP enable checkbox was present
        bool tcp_enable_checked = getPostParameterFromBody(postBody, "tcp_enabled", tcp_enabled_str);

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

            // Update TCP enabled flag
            bool old_tcp_enabled = modbus_tcp_enabled;
            modbus_tcp_enabled = tcp_enable_checked;

            // Save to NVS
            preferences.begin("modbus", false);
            preferences.putUChar("slave_id", modbus_slave_id);
            preferences.putBool("tcp_enabled", modbus_tcp_enabled);
            preferences.end();

            mb.slave(modbus_slave_id);  // Update Modbus slave ID
            Serial.printf("Modbus Slave ID changed to: %d (saved to NVS)\n", modbus_slave_id);
            Serial.printf("Modbus TCP: %s (saved to NVS)\n", modbus_tcp_enabled ? "Enabled" : "Disabled");

            html += "<h1>Configuration Saved!</h1>";
            html += "<p>Modbus Slave ID updated to:</p>";
            html += "<div class='value'>" + String(modbus_slave_id) + "</div>";
            html += "<p>Modbus TCP: <strong>" + String(modbus_tcp_enabled ? "Enabled" : "Disabled") + "</strong></p>";

            // Show restart message if TCP state changed
            if (old_tcp_enabled != modbus_tcp_enabled) {
                html += "<p style='color:#e67e22;font-weight:bold;'>⚠ Please restart the device for TCP changes to take effect.</p>";
            } else {
                html += "<p>The new slave ID is active immediately.</p>";
            }

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
    html += "<a href='/lorawan/profiles'>Profiles</a>";
    html += "<a href='/wifi'>WiFi</a>";
    html += "<a href='/security'>Security</a>";
    html += "<a href='/reboot' class='reboot' onclick='return confirm(\"Are you sure you want to reboot the device?\");'>Reboot</a>";
    html += "</div>";

    html += "<h1>LoRaWAN Configuration</h1>";
    html += "<p>Device credentials and network status for LoRaWAN OTAA. Copy credentials to your network server (TTN, ChirpStack, AWS IoT Core, etc.).</p>";

    // Display network status
    html += "<h2>Network Status</h2>";
    html += "<table>";
    html += "<tr><th>Parameter</th><th>Value</th></tr>";
    html += "<tr><td class='label'>Connection Status</td><td class='value' style='background:" + String(lorawan_joined ? "#d4edda" : "#f8d7da") + ";color:" + String(lorawan_joined ? "#155724" : "#721c24") + ";font-weight:bold;'>" + String(lorawan_joined ? "JOINED" : "NOT JOINED") + "</td></tr>";

    if (lorawan_joined) {
        html += "<tr><td class='label'>DevAddr</td><td class='value'>0x" + String(lorawanHandler.getDevAddr(), HEX) + "</td></tr>";
    }

    html += "<tr><td class='label'>Total Uplinks</td><td class='value'>" + String(lorawanHandler.getUplinkCount()) + "</td></tr>";
    html += "<tr><td class='label'>Total Downlinks</td><td class='value'>" + String(lorawanHandler.getDownlinkCount()) + "</td></tr>";
    html += "<tr><td class='label'>Last RSSI</td><td class='value'>" + String(lorawanHandler.getLastRSSI()) + " dBm</td></tr>";
    html += "<tr><td class='label'>Last SNR</td><td class='value'>" + String(lorawanHandler.getLastSNR(), 1) + " dB</td></tr>";
    html += "<tr><td class='label'>Region</td><td class='value'>EU868</td></tr>";
    html += "</table>";

    // Display active profile info
    uint8_t active_idx = lorawanHandler.getActiveProfileIndex();
    LoRaProfile* active_prof = lorawanHandler.getProfile(active_idx);
    if (active_prof) {
        html += "<h2>Active Profile</h2>";
        html += "<table>";
        html += "<tr><th>Parameter</th><th>Value</th></tr>";
        html += "<tr><td class='label'>Profile</td><td class='value'>" + String(active_idx) + " - " + String(active_prof->name) + "</td></tr>";
        html += "<tr><td class='label'>Status</td><td class='value' style='background:#d4edda;color:#155724;font-weight:bold;'>ACTIVE</td></tr>";
        html += "</table>";
        html += "<p><a href='/lorawan/profiles' style='background:#3498db;'>Manage All Profiles →</a></p>";
    }

    // Display current credentials
    html += "<h2>Current Credentials (Active Profile)</h2>";
    html += "<table>";
    html += "<tr><th>Parameter</th><th>Value</th></tr>";

    // Get credentials from component
    uint64_t currentJoinEUI = lorawanHandler.getJoinEUI();
    uint64_t currentDevEUI = lorawanHandler.getDevEUI();
    uint8_t currentAppKey[16];
    uint8_t currentNwkKey[16];
    lorawanHandler.getAppKey(currentAppKey);
    lorawanHandler.getNwkKey(currentNwkKey);

    // JoinEUI
    char joinEUIStr[17];
    sprintf(joinEUIStr, "%016llX", currentJoinEUI);
    html += "<tr><td class='label'>JoinEUI (AppEUI)</td><td class='value'>0x" + String(joinEUIStr) + "</td></tr>";

    // DevEUI
    char devEUIStr[17];
    sprintf(devEUIStr, "%016llX", currentDevEUI);
    html += "<tr><td class='label'>DevEUI</td><td class='value'>0x" + String(devEUIStr) + "</td></tr>";

    // AppKey
    html += "<tr><td class='label'>AppKey</td><td class='value'>";
    for (int i = 0; i < 16; i++) {
        char buf[3];
        sprintf(buf, "%02X", currentAppKey[i]);
        html += String(buf);
    }
    html += "</td></tr>";

    // NwkKey
    html += "<tr><td class='label'>NwkKey</td><td class='value'>";
    for (int i = 0; i < 16; i++) {
        char buf[3];
        sprintf(buf, "%02X", currentNwkKey[i]);
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

            // Save to NVS using component
            lorawanHandler.saveCredentials();

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

// Handle LoRaWAN Profiles page
void handle_lorawan_profiles(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    String html = "<!DOCTYPE html><html><head><title>LoRaWAN Profiles - Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += ".container{max-width:1100px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
    html += "h1{color:#2c3e50;margin-top:0;border-bottom:3px solid #3498db;padding-bottom:15px;}";
    html += "h2{color:#34495e;margin-top:30px;}";
    html += ".nav{background:#3498db;padding:15px;margin:-30px -30px 30px -30px;border-radius:10px 10px 0 0;display:flex;align-items:center;}";
    html += ".nav a{color:white;text-decoration:none;padding:10px 20px;margin:0 5px;background:#2980b9;border-radius:5px;display:inline-block;}";
    html += ".nav a:hover{background:#21618c;}";
    html += ".nav .reboot{margin-left:auto;background:#e74c3c;}";
    html += ".nav .reboot:hover{background:#c0392b;}";
    html += "table{border-collapse:collapse;width:100%;margin:15px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1);}";
    html += "th{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;padding:12px;text-align:left;font-weight:600;}";
    html += "td{border:1px solid #e0e0e0;padding:10px;background:white;}";
    html += "tr:nth-child(even) td{background:#f8f9fa;}";
    html += ".active-badge{background:#27ae60;color:white;padding:3px 8px;border-radius:3px;font-size:11px;font-weight:bold;}";
    html += ".enabled{color:#27ae60;font-weight:bold;}";
    html += ".disabled{color:#95a5a6;}";
    html += ".btn{display:inline-block;padding:6px 12px;border-radius:4px;text-decoration:none;margin:2px;font-size:13px;cursor:pointer;border:none;}";
    html += ".btn-primary{background:#3498db;color:white;}";
    html += ".btn-primary:hover{background:#2980b9;}";
    html += ".btn-success{background:#27ae60;color:white;}";
    html += ".btn-success:hover{background:#229954;}";
    html += ".btn-warning{background:#f39c12;color:white;}";
    html += ".btn-warning:hover{background:#e67e22;}";
    html += ".btn-danger{background:#e74c3c;color:white;}";
    html += ".btn-danger:hover{background:#c0392b;}";
    html += ".profile-detail{background:#ecf0f1;padding:20px;border-radius:8px;margin:15px 0;}";
    html += "form{margin:20px 0;}";
    html += "label{display:block;margin:10px 0 5px;font-weight:600;}";
    html += "input[type=text]{width:100%;padding:8px;border:1px solid #ddd;border-radius:4px;font-family:monospace;box-sizing:border-box;}";
    html += ".warning{background:#fff3cd;border:1px solid #ffc107;padding:12px;border-radius:5px;margin:10px 0;color:#856404;}";
    html += "</style>";
    html += "<script>";
    html += "function toggleProfile(idx){";
    html += "  fetch('/lorawan/profile/toggle?index='+idx).then(r=>r.text()).then(()=>location.reload());";
    html += "}";
    html += "function activateProfile(idx){";
    html += "  if(confirm('Switch to this profile? Device will restart to join with new credentials.')){";
    html += "    fetch('/lorawan/profile/activate?index='+idx).then(()=>{";
    html += "      alert('Profile activated! Device will restart...');";
    html += "      setTimeout(()=>location.href='/',10000);";
    html += "    });";
    html += "  }";
    html += "}";
    html += "function toggleAutoRotate(enabled){";
    html += "  fetch('/lorawan/auto-rotate?enabled='+(enabled?'1':'0')).then(r=>r.text()).then(()=>location.reload());";
    html += "}";
    html += "</script>";
    html += "</head><body>";
    html += "<div class='container'>";
    html += "<div class='nav'>";
    html += "<a href='/'>Home</a>";
    html += "<a href='/stats'>Statistics</a>";
    html += "<a href='/registers'>Registers</a>";
    html += "<a href='/lorawan'>LoRaWAN</a>";
    html += "<a href='/lorawan/profiles'>Profiles</a>";
    html += "<a href='/lorawan/profiles'>Profiles</a>";
    html += "<a href='/wifi'>WiFi</a>";
    html += "<a href='/security'>Security</a>";
    html += "<a href='/reboot' class='reboot' onclick='return confirm(\"Reboot device?\");'>Reboot</a>";
    html += "</div>";
    html += "<h1>LoRaWAN Device Profiles</h1>";
    html += "<p>Manage up to 4 LoRaWAN device profiles. Each profile can emulate a different device with unique credentials.</p>";
    
    html += "<div class='warning'>";
    html += "<strong>Note:</strong> Only enabled profiles can be activated. Switching profiles requires a device restart.";
    html += "</div>";

    // Auto-rotation controls
    bool auto_rotate = lorawanHandler.getAutoRotation();
    int enabled_count = lorawanHandler.getEnabledProfileCount();
    
    html += "<div class='profile-detail' style='background:#d5f4e6;border:2px solid #27ae60;'>";
    html += "<h2>🔄 Auto-Rotation</h2>";
    html += "<p>Automatically cycle through enabled profiles during uplink transmissions. Each transmission will use the next enabled profile.</p>";
    html += "<p><strong>Status:</strong> <span style='color:" + String(auto_rotate ? "#27ae60" : "#95a5a6") + ";font-weight:bold;'>";
    html += auto_rotate ? "ENABLED" : "DISABLED";
    html += "</span> | <strong>Enabled Profiles:</strong> " + String(enabled_count) + "</p>";
    
    if (enabled_count < 2) {
        html += "<p style='color:#e67e22;font-weight:bold;'>⚠️ Enable at least 2 profiles to use auto-rotation.</p>";
    }
    
    html += "<label style='display:inline-block;margin:10px 0;'>";
    html += "<input type='checkbox' id='autoRotateCheckbox' ";
    if (auto_rotate) html += "checked ";
    if (enabled_count < 2) html += "disabled ";
    html += "onchange='toggleAutoRotate(this.checked)'> ";
    html += "Enable Auto-Rotation";
    html += "</label>";
    html += "</div>";

    // Display all profiles
    html += "<h2>Profile Overview</h2>";
    html += "<table>";
    html += "<tr><th>Profile</th><th>Name</th><th>DevEUI</th><th>Status</th><th>Actions</th></tr>";
    
    uint8_t active_idx = lorawanHandler.getActiveProfileIndex();
    for (int i = 0; i < MAX_LORA_PROFILES; i++) {
        LoRaProfile* prof = lorawanHandler.getProfile(i);
        if (!prof) continue;
        
        html += "<tr>";
        html += "<td><strong>" + String(i) + "</strong>";
        if (i == active_idx) {
            html += " <span class='active-badge'>ACTIVE</span>";
        }
        html += "</td>";
        html += "<td>" + String(prof->name) + "</td>";
        
        char devEUIStr[17];
        sprintf(devEUIStr, "%016llX", prof->devEUI);
        html += "<td style='font-family:monospace;font-size:12px;'>0x" + String(devEUIStr) + "</td>";
        
        html += "<td><span class='" + String(prof->enabled ? "enabled" : "disabled") + "'>";
        html += prof->enabled ? "ENABLED" : "DISABLED";
        html += "</span></td>";
        
        html += "<td>";
        // Toggle enable/disable button
        if (i != active_idx || !prof->enabled) {
            html += "<button class='btn " + String(prof->enabled ? "btn-warning" : "btn-success") + "' onclick='toggleProfile(" + String(i) + ")'>";
            html += prof->enabled ? "Disable" : "Enable";
            html += "</button> ";
        }
        // Activate button (only for enabled, non-active profiles)
        if (prof->enabled && i != active_idx) {
            html += "<button class='btn btn-primary' onclick='activateProfile(" + String(i) + ")'>Activate</button> ";
        }
        html += "<a href='#profile" + String(i) + "' class='btn btn-primary'>Edit</a>";
        html += "</td>";
        html += "</tr>";
    }
    html += "</table>";

    // Detail forms for each profile
    for (int i = 0; i < MAX_LORA_PROFILES; i++) {
        LoRaProfile* prof = lorawanHandler.getProfile(i);
        if (!prof) continue;
        
        html += "<div id='profile" + String(i) + "' class='profile-detail'>";
        html += "<h2>Profile " + String(i) + ": " + String(prof->name) + "</h2>";
        
        if (i == active_idx) {
            html += "<p style='color:#27ae60;font-weight:bold;'>⚫ This is the currently active profile</p>";
        }
        
        html += "<form method='POST' action='/lorawan/profile/update'>";
        html += "<input type='hidden' name='index' value='" + String(i) + "'>";
        
        html += "<label>Profile Name:</label>";
        html += "<input type='text' name='name' value='" + String(prof->name) + "' maxlength='32' required>";
        
        html += "<label>JoinEUI (AppEUI) - 16 hex characters:</label>";
        char joinEUIStr[17];
        sprintf(joinEUIStr, "%016llX", prof->joinEUI);
        html += "<input type='text' name='joinEUI' value='" + String(joinEUIStr) + "' pattern='[0-9A-Fa-f]{16}' required>";
        
        html += "<label>DevEUI - 16 hex characters:</label>";
        char devEUIStr[17];
        sprintf(devEUIStr, "%016llX", prof->devEUI);
        html += "<input type='text' name='devEUI' value='" + String(devEUIStr) + "' pattern='[0-9A-Fa-f]{16}' required>";
        
        html += "<label>AppKey - 32 hex characters:</label>";
        html += "<input type='text' name='appKey' value='";
        for (int j = 0; j < 16; j++) {
            char buf[3];
            sprintf(buf, "%02X", prof->appKey[j]);
            html += String(buf);
        }
        html += "' pattern='[0-9A-Fa-f]{32}' required>";
        
        html += "<label>NwkKey - 32 hex characters:</label>";
        html += "<input type='text' name='nwkKey' value='";
        for (int j = 0; j < 16; j++) {
            char buf[3];
            sprintf(buf, "%02X", prof->nwkKey[j]);
            html += String(buf);
        }
        html += "' pattern='[0-9A-Fa-f]{32}' required>";
        
        html += "<button type='submit' class='btn btn-success' style='margin-top:15px;'>Save Profile " + String(i) + "</button>";
        html += "</form>";
        html += "</div>";
    }
    
    html += "</div></body></html>";
    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());
}

// Handle profile update
void handle_lorawan_profile_update(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    String html = "<!DOCTYPE html><html><head><title>Updating Profile - Vision Master E290</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta http-equiv='refresh' content='3;url=/lorawan/profiles'>";
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += ".container{max-width:600px;margin:50px auto;background:white;padding:40px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);text-align:center;}";
    html += "h1{color:#27ae60;}";
    html += "p{color:#7f8c8d;font-size:16px;margin:20px 0;}";
    html += "</style></head><body><div class='container'>";

    String postBody = readPostBody(req);
    String indexStr, name, joinEUIStr, devEUIStr, appKeyStr, nwkKeyStr;
    
    if (getPostParameterFromBody(postBody, "index", indexStr) &&
        getPostParameterFromBody(postBody, "name", name) &&
        getPostParameterFromBody(postBody, "joinEUI", joinEUIStr) &&
        getPostParameterFromBody(postBody, "devEUI", devEUIStr) &&
        getPostParameterFromBody(postBody, "appKey", appKeyStr) &&
        getPostParameterFromBody(postBody, "nwkKey", nwkKeyStr)) {
        
        int index = indexStr.toInt();
        
        if (index >= 0 && index < MAX_LORA_PROFILES &&
            joinEUIStr.length() == 16 && devEUIStr.length() == 16 &&
            appKeyStr.length() == 32 && nwkKeyStr.length() == 32) {
            
            LoRaProfile profile;
            strncpy(profile.name, name.c_str(), sizeof(profile.name) - 1);
            profile.name[sizeof(profile.name) - 1] = '\0';
            
            profile.joinEUI = strtoull(joinEUIStr.c_str(), NULL, 16);
            profile.devEUI = strtoull(devEUIStr.c_str(), NULL, 16);
            
            for (int i = 0; i < 16; i++) {
                char buf[3] = {appKeyStr[i*2], appKeyStr[i*2+1], 0};
                profile.appKey[i] = strtol(buf, NULL, 16);
                char buf2[3] = {nwkKeyStr[i*2], nwkKeyStr[i*2+1], 0};
                profile.nwkKey[i] = strtol(buf2, NULL, 16);
            }
            
            // Keep existing enabled status
            LoRaProfile* existing = lorawanHandler.getProfile(index);
            if (existing) {
                profile.enabled = existing->enabled;
            }
            
            lorawanHandler.updateProfile(index, profile);
            
            html += "<h1>Profile Updated!</h1>";
            html += "<p>Profile " + String(index) + " has been saved.</p>";
            html += "<p>Redirecting to profiles page...</p>";
        } else {
            html += "<h1>Invalid Input</h1>";
            html += "<p>Please check your input values.</p>";
        }
    } else {
        html += "<h1>Error</h1>";
        html += "<p>Missing required parameters.</p>";
    }
    
    html += "</div></body></html>";
    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());
}

// Handle profile toggle (enable/disable)
void handle_lorawan_profile_toggle(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    ResourceParameters * params = req->getParams();
    std::string indexValue;
    
    if (params->getQueryParameter("index", indexValue)) {
        int index = atoi(indexValue.c_str());
        lorawanHandler.toggleProfileEnabled(index);
    }
    
    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/plain");
    res->print("OK");
}

// Handle profile activation (switch active profile)
void handle_lorawan_profile_activate(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    ResourceParameters * params = req->getParams();
    std::string indexValue;
    
    if (params->getQueryParameter("index", indexValue)) {
        int index = atoi(indexValue.c_str());
        
        if (lorawanHandler.setActiveProfile(index)) {
            // Success - device will restart
            res->setStatusCode(200);
            res->setHeader("Content-Type", "text/plain");
            res->print("Profile activated - restarting...");
            
            delay(1000);
            ESP.restart();
            return;
        }
    }
    
    res->setStatusCode(400);
    res->setHeader("Content-Type", "text/plain");
    res->print("Error: Cannot activate profile");
}

// Handle auto-rotation toggle
void handle_lorawan_auto_rotate(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    ResourceParameters * params = req->getParams();
    std::string enabledValue;
    
    if (params->getQueryParameter("enabled", enabledValue)) {
        bool enable = (enabledValue == "1");
        lorawanHandler.setAutoRotation(enable);
        
        res->setStatusCode(200);
        res->setHeader("Content-Type", "text/plain");
        res->print(enable ? "Auto-rotation enabled" : "Auto-rotation disabled");
        return;
    }
    
    res->setStatusCode(400);
    res->setHeader("Content-Type", "text/plain");
    res->print("Error: Missing enabled parameter");
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
    html += "<a href='/lorawan/profiles'>Profiles</a>";
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
    html += "<a href='/lorawan/profiles'>Profiles</a>";
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

    html += "<h2 style='margin-top:30px;color:#e74c3c;'>Factory Reset</h2>";
    html += "<div class='warning' style='background:#ffe5e5;border-color:#e74c3c;color:#721c24;'>";
    html += "<strong>Danger Zone:</strong> This will erase ALL stored settings and restore factory defaults.";
    html += "</div>";
    html += "<div class='card'>";
    html += "<p><strong>This will reset:</strong></p>";
    html += "<ul style='text-align:left;margin:10px 0;padding-left:20px;'>";
    html += "<li>Modbus Slave ID (back to 1)</li>";
    html += "<li>Modbus TCP enable setting (disabled)</li>";
    html += "<li>Authentication credentials (back to admin/admin)</li>";
    html += "<li>WiFi client credentials (will be cleared)</li>";
    html += "<li>SF6 manual control values (back to defaults)</li>";
    html += "<li>LoRaWAN session and DevNonce (will rejoin on next boot)</li>";
    html += "</ul>";
    html += "<p style='color:#e74c3c;'><strong>Device will automatically reboot after reset.</strong></p>";
    html += "<button onclick='if(confirm(\"Are you sure you want to erase ALL settings and restore factory defaults? This cannot be undone!\")) window.location.href=\"/factory-reset\"' ";
    html += "style='background:#e74c3c;color:white;padding:12px 30px;border:none;border-radius:5px;font-size:16px;cursor:pointer;margin-top:10px;'>";
    html += "Factory Reset (Erase All Settings)</button>";
    html += "</div>";

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

// Handle factory reset
void handle_factory_reset(HTTPRequest * req, HTTPResponse * res) {
    if (!check_authentication(req, res)) return;

    String html = "<!DOCTYPE html><html><head><title>Factory Reset - Vision Master E290</title>";
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
    html += "<h1>Factory Reset in Progress...</h1>";
    html += "<div class='spinner'></div>";
    html += "<p>All settings are being erased and defaults restored.</p>";
    html += "<p>The device will reboot automatically.</p>";
    html += "<p><small>Default credentials after reboot: admin/admin</small></p>";
    html += "</div></body></html>";

    res->setStatusCode(200);
    res->setHeader("Content-Type", "text/html");
    res->print(html.c_str());

    Serial.println("\n========================================");
    Serial.println("FACTORY RESET REQUESTED");
    Serial.println("========================================");

    delay(500);

    // Clear all NVS namespaces
    Serial.println("Clearing Modbus settings...");
    preferences.begin("modbus", false);
    preferences.clear();
    preferences.end();

    Serial.println("Clearing Authentication settings...");
    preferences.begin("auth", false);
    preferences.clear();
    preferences.end();

    Serial.println("Clearing WiFi settings...");
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();

    Serial.println("Clearing SF6 settings...");
    preferences.begin("sf6", false);
    preferences.clear();
    preferences.end();

    Serial.println("Clearing LoRaWAN settings...");
    preferences.begin("lorawan", false);
    preferences.clear();
    preferences.end();

    Serial.println("\nFactory reset complete!");
    Serial.println("All NVS settings have been erased.");
    Serial.println("Device will restart with factory defaults.");
    Serial.println("========================================\n");

    delay(1000);
    ESP.restart();
}

// ============================================================================
// MODBUS FUNCTIONS
// ============================================================================


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
        wifiManager.saveAPPassword();
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


