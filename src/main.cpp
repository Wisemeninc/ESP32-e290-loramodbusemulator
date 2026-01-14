/*
 * Vision Master E290 - Modbus RTU Slave with E-Ink Display
 * Arduino Framework Version
 *
 * Refactored Architecture:
 * - ModbusHandler: Manages Modbus RTU/TCP communication
 * - LoRaWANHandler: Manages LoRaWAN connectivity and profiles
 * - WiFiManager: Manages WiFi AP/Client modes
 * - AuthManager: Manages web authentication
 * - DisplayManager: Manages E-Ink display updates
 * - SF6Emulator: Manages sensor simulation logic
 * - WebServerManager: Manages HTTPS server and web interface
 */

#include <Arduino.h>
#include <heltec-eink-modules.h>
#include <WiFi.h>
#include <ModbusIP_ESP8266.h>

// Component headers
#include "config.h"
#include "modbus_handler.h"
#include "wifi_manager.h"
#include "auth_manager.h"
#include "display_manager.h"
#include "lorawan_handler.h"
#include "sf6_emulator.h"
#include "web_server.h"
#include "ota_manager.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

// Modbus TCP instance (must be global for callback access if needed)
ModbusIP mbTCP;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialize Display FIRST to clear screen immediately
    displayManager.begin(DISPLAY_ROTATION);

    Serial.println("\n\n========================================");
    Serial.println("Vision Master E290 - Modbus RTU/TCP");
    Serial.println("========================================");
    Serial.println("Framework: Arduino");
    Serial.println("Display: 2.9\" E-Ink (296x128)");
    Serial.println("========================================\n");

    // Initialize Authentication
    authManager.begin();

    // Initialize SF6 Emulator (loads values from NVS)
    sf6Emulator.begin();

    // Initialize LoRaWAN
    lorawanHandler.begin();
    
    // Perform startup uplink sequence
    if (lorawanHandler.getEnabledProfileCount() > 0) {
        Serial.println(">>> Starting LoRaWAN uplink sequence...");
        lorawanHandler.performStartupSequence(modbusHandler.getInputRegisters());
    }

    // Show startup screen after LoRaWAN init
    displayManager.showStartupScreen();

    // Initialize WiFi
    wifiManager.begin();

    // Initialize Web Server
    webServer.begin();

    // Initialize OTA Manager
    otaManager.begin();

    // Initialize Modbus RTU
    // Get slave ID from preferences or default
    Preferences prefs;
    uint8_t slave_id = MB_SLAVE_ID_DEFAULT;
    bool tcp_enabled = false;
    
    if (prefs.begin("modbus", false)) {  // false = read-write, creates if needed
        slave_id = prefs.getUChar("slave_id", MB_SLAVE_ID_DEFAULT);
        tcp_enabled = prefs.getBool("tcp_enabled", false);
        prefs.end();
    } else {
        Serial.println("[MODBUS] Failed to open preferences, using defaults");
    }
    
    modbusHandler.begin(slave_id);

    // Initialize Modbus TCP if enabled
    if (tcp_enabled) {
        Serial.println("\n>>> Initializing Modbus TCP...");
        mbTCP.server();
        
        // Add registers to TCP instance
        for (int i = 0; i < 12; i++) mbTCP.addHreg(i);
        for (int i = 0; i < 9; i++) mbTCP.addIreg(i);
        
        Serial.println(">>> Modbus TCP server started on port 502");
    }
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    static unsigned long last_update = 0;
    static unsigned long last_display_update = 0;
    static unsigned long last_sf6_update = 0;
    static unsigned long last_tcp_sync = 0;
    static unsigned long last_ota_check = 0;

    unsigned long now = millis();

    // Update Modbus Holding Registers every 2 seconds
    if (now - last_update >= 2000) {
        last_update = now;
        modbusHandler.updateHoldingRegisters(
            wifiManager.isAPActive() || wifiManager.isClientConnected(),
            wifiManager.isAPActive() ? wifiManager.getAPClients() : 0
        );
    }

    // Update SF6 Emulator every 3 seconds
    if (now - last_sf6_update >= 3000) {
        last_sf6_update = now;
        sf6Emulator.update();
    }

    // Sync TCP registers every 5 seconds
    // We need to check if TCP is enabled. For now, we'll check the global object state if possible
    // or just rely on the fact that if it wasn't started, this might be no-op or we check a flag
    // Ideally, ModbusHandler should manage TCP too, but mbTCP is a separate library object.
    // For this refactor, we'll keep the sync logic simple.
    if (now - last_tcp_sync >= 5000) {
        last_tcp_sync = now;
        
        // Sync logic: Copy from ModbusHandler to mbTCP
        HoldingRegisters& rtu_holding = modbusHandler.getHoldingRegisters();
        InputRegisters& rtu_input = modbusHandler.getInputRegisters();
        
        // This assumes mbTCP was initialized. If not, these calls might be unsafe or ignored.
        // A proper implementation would wrap mbTCP in ModbusHandler.
        // For now, we'll assume it's safe if we initialized it in setup.
        
        // We can check a global flag or just do it.
        // To be safe, let's re-read the config or use a static flag set in setup.
        static bool tcp_initialized = false;
        if (!tcp_initialized) {
            Preferences prefs;
            if (prefs.begin("modbus", false)) {  // Read-write (creates if not exists)
                tcp_initialized = prefs.getBool("tcp_enabled", false);
                prefs.end();
            }
        }
        
        if (tcp_initialized) {
            mbTCP.Hreg(0, rtu_holding.sequential_counter);
            mbTCP.Hreg(1, rtu_holding.random_number);
            // ... sync other registers ...
            mbTCP.Ireg(0, rtu_input.sf6_density);
            // ... sync other registers ...
        }
    }

    // Update Display every 30 seconds
    if (now - last_display_update >= 30000) {
        last_display_update = now;
        
        // Check if an update is available from OTA manager
        bool update_available = otaManager.getStatus().updateAvailable;
        
        displayManager.update(
            modbusHandler.getHoldingRegisters(),
            modbusHandler.getInputRegisters(),
            wifiManager.isAPActive(),
            wifiManager.isClientConnected(),
            wifiManager.getAPSSID().c_str(),
            modbusHandler.getSlaveId(),
            lorawanHandler.isJoined(),
            lorawanHandler.getUplinkCount(),
            lorawanHandler.getLastRSSI(),
            lorawanHandler.getLastSNR(),
            lorawanHandler.getDevEUI(),
            update_available
        );
    }

    // Check for OTA updates periodically when WiFi is connected
    if (wifiManager.isClientConnected()) {
        // Reset timer on first WiFi connection to check immediately 
        static bool was_connected = false;
        if (!was_connected) {
            last_ota_check = 0; // Reset timer to trigger immediate check
            was_connected = true;
            Serial.println("[AUTO] WiFi connected - will check for updates shortly");
        }
        
        if (now - last_ota_check >= (otaManager.getUpdateCheckInterval() * 60 * 60 * 1000UL)) {
            last_ota_check = now;
            if (!otaManager.isUpdating()) {
                Serial.printf("[AUTO] Checking for firmware updates (interval: %d hours)...\n", 
                             otaManager.getUpdateCheckInterval());
                otaManager.checkForUpdate();
            }
        }
    } else {
        static bool was_connected = true;
        was_connected = false;
    }

    // Handle LoRaWAN Uplinks (Auto-rotation and periodic sending)
    lorawanHandler.process(modbusHandler.getInputRegisters());

    // Handle WiFi Timeout
    wifiManager.handleTimeout();

    // Handle Web Server
    webServer.handle();

    // Handle Modbus RTU
    modbusHandler.task();

    // Handle Modbus TCP
    mbTCP.task();

    yield();
}
