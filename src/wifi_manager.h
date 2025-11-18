#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// ============================================================================
// WIFI MANAGER CLASS
// ============================================================================

class WiFiManager {
public:
    WiFiManager();

    // Initialization
    void begin();

    // WiFi AP Mode
    bool startAP();
    void stopAP();
    bool isAPActive();
    String getAPSSID();
    String getAPPassword();
    uint8_t getAPClients();

    // WiFi Client Mode
    bool connectClient(const char* ssid, const char* password);
    bool isClientConnected();
    String getClientSSID();
    IPAddress getClientIP();
    int getClientRSSI();

    // Configuration
    void loadClientCredentials();
    void saveClientCredentials(const char* ssid, const char* password);
    void clearClientCredentials();

    // Timeout management
    void handleTimeout();
    bool isTimeoutReached();

    // mDNS
    bool startMDNS(const char* hostname);

    // Password management (production mode)
    void loadAPPassword();
    void saveAPPassword();
    void generateAPPassword();

    // Network scanning
    int scanNetworks();
    String getScannedSSID(int index);
    int getScannedRSSI(int index);

private:
    Preferences preferences;

    // AP Mode state
    bool ap_active;
    unsigned long ap_start_time;
    char ap_password[17];  // 16 chars + null
    String ap_ssid;

    // Client Mode state
    bool client_connected;
    char client_ssid[33];      // 32 chars + null
    char client_password[64];  // 63 chars + null

    // Helper functions
    void printAPInfo();
    void printClientInfo();
    void printConnectionFailure(wl_status_t status);
};

// Global instance
extern WiFiManager wifiManager;

#endif // WIFI_MANAGER_H
