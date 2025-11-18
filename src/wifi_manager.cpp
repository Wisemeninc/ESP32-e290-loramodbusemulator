#include "wifi_manager.h"
#include "config.h"

// Global instance
WiFiManager wifiManager;

// ============================================================================
// CONSTRUCTOR
// ============================================================================

WiFiManager::WiFiManager() :
    ap_active(false),
    ap_start_time(0),
    client_connected(false) {

    memset(ap_password, 0, sizeof(ap_password));
    memset(client_ssid, 0, sizeof(client_ssid));
    memset(client_password, 0, sizeof(client_password));

    #if MODE_PRODUCTION
        strcpy(ap_password, "");  // Will be generated/loaded
    #else
        strcpy(ap_password, "modbus123");  // Development default
    #endif
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void WiFiManager::begin() {
    // Load client credentials if available
    loadClientCredentials();

    // Try client mode first
    if (strlen(client_ssid) > 0) {
        if (connectClient(client_ssid, client_password)) {
            Serial.println(">>> WiFi client connected - AP mode disabled");
            return;
        } else {
            Serial.println(">>> WiFi client connection failed - starting AP mode");
        }
    }

    // Fall back to AP mode
    startAP();
}

// ============================================================================
// AP MODE
// ============================================================================

bool WiFiManager::startAP() {
    Serial.println("Starting WiFi AP...");

    // Load or generate password based on mode
    #if MODE_PRODUCTION
        Serial.println(">>> Production mode: Loading/generating secure password");
        loadAPPassword();
    #else
        Serial.println(">>> Development mode: Using default password");
        strcpy(ap_password, "modbus123");
    #endif

    // Get MAC address and extract last 4 hex digits for unique SSID
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char mac_suffix[5];
    sprintf(mac_suffix, "%02X%02X", mac[4], mac[5]);

    // Create unique SSID: ESP32-Modbus-Config-XXXX
    ap_ssid = "ESP32-Modbus-Config-" + String(mac_suffix);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid.c_str(), ap_password, 1, 0, 4);

    ap_active = true;
    ap_start_time = millis();

    // Initialize mDNS
    startMDNS("stationsdata");

    printAPInfo();

    return true;
}

void WiFiManager::stopAP() {
    if (ap_active) {
        Serial.println("WiFi AP timeout - shutting down");
        WiFi.mode(WIFI_OFF);
        ap_active = false;
    }
}

bool WiFiManager::isAPActive() {
    return ap_active;
}

String WiFiManager::getAPSSID() {
    return ap_ssid;
}

String WiFiManager::getAPPassword() {
    return String(ap_password);
}

uint8_t WiFiManager::getAPClients() {
    return WiFi.softAPgetStationNum();
}

// ============================================================================
// CLIENT MODE
// ============================================================================

bool WiFiManager::connectClient(const char* ssid, const char* password) {
    if (strlen(ssid) == 0) {
        Serial.println("No WiFi client credentials configured");
        return false;
    }

    Serial.println("\n========================================");
    Serial.println("Attempting WiFi Client Connection");
    Serial.println("========================================");
    Serial.printf("SSID: %s\n", ssid);
    Serial.println("========================================\n");

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {  // 10 seconds timeout
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        client_connected = true;
        strncpy(client_ssid, ssid, sizeof(client_ssid) - 1);
        strncpy(client_password, password, sizeof(client_password) - 1);

        // Initialize mDNS
        startMDNS("stationsdata");

        printClientInfo();
        return true;
    } else {
        printConnectionFailure(WiFi.status());
        WiFi.mode(WIFI_OFF);
        return false;
    }
}

bool WiFiManager::isClientConnected() {
    // Update state based on actual WiFi status
    if (WiFi.status() != WL_CONNECTED) {
        client_connected = false;
    }
    return client_connected;
}

String WiFiManager::getClientSSID() {
    return WiFi.SSID();
}

IPAddress WiFiManager::getClientIP() {
    return WiFi.localIP();
}

int WiFiManager::getClientRSSI() {
    return WiFi.RSSI();
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void WiFiManager::loadClientCredentials() {
    if (!preferences.begin("wifi", true)) {  // Read-only
        Serial.println(">>> Failed to open wifi preferences");
        return;
    }

    bool has_client_config = preferences.getBool("has_client", false);

    if (has_client_config) {
        String ssid = preferences.getString("client_ssid", "");
        String password = preferences.getString("client_password", "");

        if (ssid.length() > 0 && ssid.length() <= 32) {
            strncpy(client_ssid, ssid.c_str(), sizeof(client_ssid) - 1);
            client_ssid[sizeof(client_ssid) - 1] = '\0';

            strncpy(client_password, password.c_str(), sizeof(client_password) - 1);
            client_password[sizeof(client_password) - 1] = '\0';

            Serial.println(">>> Loaded WiFi client credentials from NVS");
            Serial.printf("    SSID: %s\n", client_ssid);
        }
    } else {
        Serial.println(">>> No WiFi client credentials found");
    }

    preferences.end();
}

void WiFiManager::saveClientCredentials(const char* ssid, const char* password) {
    if (!preferences.begin("wifi", false)) {  // Read-write
        Serial.println(">>> Failed to open wifi preferences for writing");
        return;
    }

    preferences.putString("client_ssid", ssid);
    preferences.putString("client_password", password);
    preferences.putBool("has_client", true);
    preferences.end();

    Serial.println(">>> WiFi client credentials saved to NVS");
    Serial.printf("    SSID: %s\n", ssid);
}

void WiFiManager::clearClientCredentials() {
    if (!preferences.begin("wifi", false)) {
        Serial.println(">>> Failed to open wifi preferences for writing");
        return;
    }

    preferences.remove("client_ssid");
    preferences.remove("client_password");
    preferences.remove("has_client");
    preferences.end();

    memset(client_ssid, 0, sizeof(client_ssid));
    memset(client_password, 0, sizeof(client_password));

    Serial.println(">>> WiFi client credentials cleared");
}

// ============================================================================
// TIMEOUT MANAGEMENT
// ============================================================================

void WiFiManager::handleTimeout() {
    if (isTimeoutReached()) {
        stopAP();
    }
}

bool WiFiManager::isTimeoutReached() {
    return ap_active && !client_connected &&
           (millis() - ap_start_time >= WIFI_TIMEOUT_MS);
}

// ============================================================================
// mDNS
// ============================================================================

bool WiFiManager::startMDNS(const char* hostname) {
    if (MDNS.begin(hostname)) {
        Serial.printf("mDNS responder started: %s.local\n", hostname);
        MDNS.addService("https", "tcp", 443);
        MDNS.addService("http", "tcp", 80);
        return true;
    } else {
        Serial.println("Error starting mDNS");
        return false;
    }
}

// ============================================================================
// PASSWORD MANAGEMENT (PRODUCTION MODE)
// ============================================================================

void WiFiManager::loadAPPassword() {
    if (!preferences.begin("wifi", false)) {  // Read-write (creates if not exists)
        Serial.println(">>> Failed to open wifi preferences");
        generateAPPassword();
        return;
    }

    bool has_password = preferences.getBool("has_password", false);

    if (has_password) {
        String stored_password = preferences.getString("password", "");
        if (stored_password.length() == 16) {
            strcpy(ap_password, stored_password.c_str());
            Serial.println(">>> Loaded WiFi password from NVS");
            preferences.end();
            return;
        } else {
            Serial.println(">>> Invalid stored password, will generate new");
            has_password = false;
        }
    }

    preferences.end();

    // Generate new password if none exists
    if (!has_password) {
        generateAPPassword();
        saveAPPassword();
    }
}

void WiFiManager::saveAPPassword() {
    if (!preferences.begin("wifi", false)) {  // Read-write
        Serial.println(">>> Failed to open wifi preferences for writing");
        return;
    }

    preferences.putString("password", ap_password);
    preferences.putBool("has_password", true);
    preferences.end();

    Serial.println(">>> WiFi password saved to NVS");
}

void WiFiManager::generateAPPassword() {
    Serial.println(">>> Generating new WiFi password...");

    // Character set for password: uppercase and digits only
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const int charset_size = sizeof(charset) - 1;

    // Generate 16 character random password
    for (int i = 0; i < 16; i++) {
        ap_password[i] = charset[esp_random() % charset_size];
    }
    ap_password[16] = '\0';  // Null terminator

    Serial.printf(">>> Generated password: %s\n", ap_password);
}

// ============================================================================
// NETWORK SCANNING
// ============================================================================

int WiFiManager::scanNetworks() {
    Serial.println("Scanning for WiFi networks...");
    int n = WiFi.scanNetworks();
    Serial.printf("Found %d networks\n", n);
    return n;
}

String WiFiManager::getScannedSSID(int index) {
    return WiFi.SSID(index);
}

int WiFiManager::getScannedRSSI(int index) {
    return WiFi.RSSI(index);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void WiFiManager::printAPInfo() {
    Serial.println("\n========================================");
    Serial.println("WiFi AP Started");
    Serial.println("========================================");
    Serial.print("SSID:     ");
    Serial.println(ap_ssid);
    Serial.print("Password: ");
    Serial.println(ap_password);
    Serial.print("IP:       ");
    Serial.println(WiFi.softAPIP());
    Serial.print("mDNS:     ");
    Serial.println("stationsdata.local");
    Serial.printf("Timeout:  %d minutes\n", WIFI_TIMEOUT_MS / 60000);
    Serial.println("========================================\n");
}

void WiFiManager::printClientInfo() {
    Serial.println("\n========================================");
    Serial.println("WiFi Client Connected!");
    Serial.println("========================================");
    Serial.printf("SSID:       %s\n", WiFi.SSID().c_str());
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("mDNS:       stationsdata.local\n");
    Serial.printf("RSSI:       %d dBm\n", WiFi.RSSI());
    Serial.println("========================================\n");
}

void WiFiManager::printConnectionFailure(wl_status_t status) {
    Serial.println("\n========================================");
    Serial.println("WiFi Client Connection Failed");
    Serial.println("========================================");
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

    Serial.printf("SSID attempted: %s\n", client_ssid);
    Serial.printf("Password length: %d characters\n", strlen(client_password));
    Serial.println("========================================\n");
}
