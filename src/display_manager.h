#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <heltec-eink-modules.h>
#include "config.h"

// ============================================================================
// DISPLAY MANAGER CLASS
// ============================================================================

// Forward declarations for parameter structures
struct HoldingRegisters;
struct InputRegisters;

class DisplayManager {
public:
    DisplayManager();

    // Initialization
    void begin(int rotation = 3);

    // Display updates
    void showStartupScreen();
    void update(
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
    );
    void showWiFiCredentials(const char* ssid, const char* password);

    // Font rendering (public for testing/custom displays)
    void drawChar(int x, int y, char c, int scale = 1);
    void drawText(int x, int y, const char* text, int scale = 1);
    void drawNumber(int x, int y, float value, int decimals, int scale = 1);

private:
    EInkDisplay_VisionMasterE290 display;
    int rotation;
    uint8_t update_count;  // Track updates for periodic full refresh

    // Bitmap font data (5x7 font)
    static const uint8_t font5x7[][5];
};

// Global instance
extern DisplayManager displayManager;

#endif // DISPLAY_MANAGER_H
