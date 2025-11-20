#include "display_manager.h"
#include "modbus_handler.h"  // For register structures
#include "lorawan_handler.h"  // For LoRaWANHandler
#include <WiFi.h>

// Global instance
DisplayManager displayManager;

// ============================================================================
// BITMAP FONT DATA (5x7 font for letters and digits)
// ============================================================================

const uint8_t DisplayManager::font5x7[][5] = {
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

// ============================================================================
// CONSTRUCTOR
// ============================================================================

DisplayManager::DisplayManager() : rotation(3), update_count(0) {
    // Display object is initialized by heltec-eink-modules library
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void DisplayManager::begin(int rot) {
    rotation = rot;

    Serial.println("Initializing E-Ink display...");

    // The Heltec library handles VextOn() and pin setup automatically
    display.setRotation(rotation);
    
    // Enable partial refresh mode (fast mode) to reduce flicker
    display.fastmodeOn();

    showStartupScreen();

    Serial.println("E-Ink display initialized (partial refresh enabled)!");
    Serial.println("Startup screen displayed");
}

// ============================================================================
// DISPLAY UPDATES
// ============================================================================

void DisplayManager::showStartupScreen() {
    display.clear();

    // White background (normal, not inverted)
    display.fillRect(0, 0, 296, 128, WHITE);

    // Draw border (black)
    display.drawRect(0, 0, 296, 128, BLACK);

    // Title
    drawText(60, 10, "Stationsdata", 2);
    
    // Subtitle
    drawText(40, 40, "SF6 Modbus Gateway", 1);
    
    // Version
    char version_str[20];
    snprintf(version_str, sizeof(version_str), "Firmware v%d.%02d", FIRMWARE_VERSION / 100, FIRMWARE_VERSION % 100);
    drawText(70, 60, version_str, 1);
    
    // Status
    drawText(80, 85, "Initializing...", 1);
    
    // Bottom text
    drawText(50, 110, "LoRaWAN + Modbus RTU - TCP", 1);

    display.update();
}

void DisplayManager::update(
    const HoldingRegisters& holding,
    const InputRegisters& input,
    bool wifi_ap_active,
    bool wifi_client_connected,
    const char* ap_ssid,
    uint8_t modbus_slave_id,
    bool lorawan_joined,
    uint32_t lorawan_uplink_count,
    int16_t lorawan_last_rssi,
    float lorawan_last_snr,
    uint64_t devEUI
) {
    display.clear();

    // Fill background with white (normal display, not inverted)
    display.fillRect(0, 0, 296, 128, WHITE);

    // Draw border (black on white)
    display.drawRect(0, 0, 296, 128, BLACK);

    // WiFi status (top right corner) - scale 1
    drawText(130, 2, "W:", 1);
    if (wifi_client_connected) {
        drawText(142, 2, "STA", 1);
    } else if (holding.wifi_enabled) {
        drawNumber(142, 2, holding.wifi_clients, 0, 1);
    } else {
        drawText(142, 2, "OFF", 1);
    }

    // LoRaWAN status (top right corner) - scale 1
    drawText(175, 2, "L:", 1);
    if (lorawan_joined) {
        drawText(187, 2, "OK", 1);
        // Show RSSI and SNR from last uplink
        char lora_info[30];
        snprintf(lora_info, sizeof(lora_info), "R:%d S:%.1f", lorawan_last_rssi, lorawan_last_snr);
        drawText(210, 2, lora_info, 1);
    } else {
        drawText(187, 2, "NO", 1);
    }

    // Title area - scale 1 for compactness
    drawText(3, 2, "SF6 Monitor", 1);
    display.drawLine(0, 11, 295, 11, BLACK);

    // Convert temperature to Celsius
    float temp_celsius = input.sf6_temperature / 10.0 - 273.15;

    // Row 1: Density - scale 1
    drawText(3, 14, "Density:", 1);
    drawNumber(150, 14, input.sf6_density / 100.0, 2, 1);
    drawText(210, 14, "kg/m3", 1);

    // Row 2: Pressure @20C - scale 1
    drawText(3, 25, "Press@20C:", 1);
    drawNumber(150, 25, input.sf6_pressure_20c / 10.0, 1, 1);
    drawText(210, 25, "kPa", 1);

    // Row 3: Temperature - scale 1
    drawText(3, 36, "Temp:", 1);
    drawNumber(150, 36, temp_celsius, 1, 1);
    drawText(210, 36, "C", 1);

    // Row 4: Pressure Variance - scale 1
    drawText(3, 47, "Press Var:", 1);
    drawNumber(150, 47, input.sf6_pressure_var / 10.0, 1, 1);
    drawText(210, 47, "kPa", 1);

    // Row 5: Quartz Frequency - scale 1 (divided by 100)
    drawText(3, 58, "Quartz:", 1);
    drawNumber(150, 58, input.quartz_freq / 100.0, 2, 1);
    drawText(210, 58, "Hz", 1);

    // Divider line
    display.drawLine(0, 68, 295, 68, BLACK);

    // Row 6: Modbus info - scale 1
    drawText(3, 71, "Modbus ID:", 1);
    drawNumber(70, 71, modbus_slave_id, 0, 1);
    drawText(100, 71, "Req:", 1);
    drawNumber(130, 71, holding.sequential_counter % 10000, 0, 1);

    // Row 7: WiFi/Network - scale 1
    drawText(3, 82, "WiFi:", 1);
    if (wifi_client_connected) {
        // Client mode - show SSID and IP
        String ssid = WiFi.SSID();
        if (ssid.length() > 10) {
            ssid = ssid.substring(0, 10);
        }
        drawText(35, 82, ssid.c_str(), 1);
        drawText(110, 82, WiFi.localIP().toString().c_str(), 1);
    } else if (holding.wifi_enabled) {
        // AP mode - show full SSID
        if (ap_ssid && strlen(ap_ssid) > 0) {
            String ssid_str = String(ap_ssid);
            // Truncate if too long to fit display
            if (ssid_str.length() > 20) {
                ssid_str = ssid_str.substring(0, 20);
            }
            drawText(35, 82, ssid_str.c_str(), 1);
        } else {
            drawText(35, 82, "AP", 1);
        }
        // Show client count at the end
        drawText(180, 82, "(", 1);
        drawNumber(188, 82, holding.wifi_clients, 0, 1);
        drawText(196, 82, ")", 1);
    } else {
        drawText(35, 82, "OFF", 1);
    }

    // Row 8: LoRaWAN stats - scale 1
    drawText(3, 93, "LoRa:", 1);
    if (lorawan_joined) {
        drawText(35, 93, "JOINED", 1);
        drawText(80, 93, "TX:", 1);
        drawNumber(100, 93, lorawan_uplink_count, 0, 1);
    } else {
        drawText(35, 93, "NOT JOINED", 1);
    }

    // Display enabled DevEUIs (last 4 hex digits each)
    // Format: ..XXXX/..YYYY/..ZZZZ for multiple enabled profiles
    char eui_str[50];
    uint64_t enabled_euis[4];
    extern LoRaWANHandler lorawanHandler;
    int eui_count = lorawanHandler.getEnabledDevEUIs(enabled_euis, 4);
    
    if (eui_count > 0) {
        strcpy(eui_str, "..");
        for (int i = 0; i < eui_count; i++) {
            char suffix[8];
            snprintf(suffix, sizeof(suffix), "%04X", (uint16_t)(enabled_euis[i] & 0xFFFF));
            strcat(eui_str, suffix);
            if (i < eui_count - 1) {
                strcat(eui_str, "/..");
            }
        }
        drawText(150, 93, eui_str, 1);
    }

    // Row 9: System info - scale 1
    drawText(3, 104, "Uptime:", 1);
    drawNumber(45, 104, holding.uptime_seconds, 0, 1);
    drawText(85, 104, "s  Heap:", 1);
    drawNumber(130, 104, holding.free_heap_kb_low, 0, 1);
    drawText(160, 104, "KB", 1);

    // Row 10: CPU/Temp/Version - scale 1
    drawText(3, 115, "CPU:", 1);
    drawNumber(30, 115, holding.cpu_freq_mhz, 0, 1);
    drawText(60, 115, "MHz  Temp:", 1);
    drawNumber(120, 115, holding.temperature_x10 / 10.0, 1, 1);
    drawText(155, 115, "C  ", 1);

    // Version number at end of bottom line
    char version_str[10];
    snprintf(version_str, sizeof(version_str), "v%d.%02d", FIRMWARE_VERSION / 100, FIRMWARE_VERSION % 100);
    drawText(250, 115, version_str, 1);

    // Use partial refresh for most updates (no flicker)
    // Do full refresh every 10 updates to clear ghosting
    update_count++;
    if (update_count >= 10) {
        display.fastmodeOff();  // Full refresh
        display.update();
        display.fastmodeOn();   // Re-enable partial for next updates
        update_count = 0;
    } else {
        display.update();  // Partial refresh (fast, no flicker)
    }

    // Debug output
    Serial.println("Display updated - SF6 sensors with text labels");
    Serial.printf("  Density: %.2f kg/m3 (reg=%d)\n", input.sf6_density/100.0, input.sf6_density);
    Serial.printf("  Pressure: %.1f kPa (reg=%d)\n", input.sf6_pressure_20c/10.0, input.sf6_pressure_20c);
    Serial.printf("  Temperature: %.1fC (reg=%d)\n", temp_celsius, input.sf6_temperature);
    Serial.printf("  Pressure Var: %.1f kPa (reg=%d)\n", input.sf6_pressure_var/10.0, input.sf6_pressure_var);
    if (wifi_client_connected) {
        Serial.printf("  WiFi: Client Mode - SSID: %s, IP: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    } else {
        Serial.printf("  WiFi: %s (%d clients)\n", holding.wifi_enabled ? "AP Mode" : "OFF", holding.wifi_clients);
    }
    Serial.printf("  Slave ID: %d, Counter: %d, Uptime: %d\n", modbus_slave_id, holding.sequential_counter, holding.uptime_seconds);
}

void DisplayManager::showWiFiCredentials(const char* ssid, const char* password) {
    display.clear();
    display.fillRect(0, 0, 296, 128, WHITE);
    display.drawRect(0, 0, 296, 128, BLACK);

    // Title
    drawText(50, 5, "WiFi AP Credentials", 1);
    display.drawLine(0, 17, 295, 17, BLACK);

    // SSID
    drawText(5, 25, "SSID:", 1);
    drawText(5, 35, ssid, 1);

    // Password
    drawText(5, 55, "Password:", 2);
    drawText(5, 75, password, 2);

    // Instructions
    drawText(5, 100, "Connect using above", 1);
    drawText(5, 110, "Screen updates in 20s", 1);

    display.update();

    Serial.println("\n========================================");
    Serial.println("WiFi AP credentials displayed on screen");
    Serial.println("========================================\n");
}

// ============================================================================
// FONT RENDERING
// ============================================================================

void DisplayManager::drawChar(int x, int y, char c, int scale) {
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
                    display.fillRect(x + col * scale, y + row * scale, scale, scale, BLACK);
                }
            }
        }
    }
}

void DisplayManager::drawText(int x, int y, const char* text, int scale) {
    int cursor_x = x;
    for (int i = 0; text[i] != '\0'; i++) {
        drawChar(cursor_x, y, text[i], scale);
        cursor_x += (5 * scale) + scale;  // 5 pixels wide * scale + spacing
    }
}

void DisplayManager::drawNumber(int x, int y, float value, int decimals, int scale) {
    char buffer[12];
    if (decimals == 0) {
        snprintf(buffer, sizeof(buffer), "%.0f", value);
    } else if (decimals == 1) {
        snprintf(buffer, sizeof(buffer), "%.1f", value);
    } else {
        snprintf(buffer, sizeof(buffer), "%.2f", value);
    }
    drawText(x, y, buffer, scale);
}
