#include "lorawan_handler.h"
#include "modbus_handler.h"  // For InputRegisters structure

// Global instance
LoRaWANHandler lorawanHandler;

// ============================================================================
// CONSTRUCTOR
// ============================================================================

LoRaWANHandler::LoRaWANHandler() :
    radio(nullptr),
    node(nullptr),
    joinEUI(0),
    devEUI(0),
    joined(false),
    uplink_count(0),
    downlink_count(0),
    last_rssi(0),
    last_snr(0.0) {

    memset(appKey, 0, sizeof(appKey));
    memset(nwkKey, 0, sizeof(nwkKey));
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void LoRaWANHandler::begin() {
    Serial.println("\n========================================");
    Serial.println("Initializing LoRaWAN...");
    Serial.println("========================================");

    // Initialize SPI bus for LoRa radio
    Serial.println("Initializing LoRa radio on SPI bus...");
    Serial.printf("  LoRa pins: SCK=%d, MISO=%d, MOSI=%d, NSS=%d\n", LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
    Serial.printf("  LoRa control: DIO1=%d, RESET=%d, BUSY=%d\n", LORA_DIO1, LORA_NRST, LORA_BUSY);

    Serial.print("Initializing SPI bus... ");
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
    Serial.println("done");
    delay(100);

    // Create radio instance
    radio = new SX1262(new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY));

    initializeRadio();
    configureRadio();

    // Load credentials (generates if not present)
    loadCredentials();
    printCredentials();

    // Create LoRaWAN node instance
    node = new LoRaWANNode(radio, &EU868);  // Change region as needed

    Serial.println("========================================\n");
}

// ============================================================================
// RADIO INITIALIZATION
// ============================================================================

void LoRaWANHandler::initializeRadio() {
    Serial.print("Initializing SX1262... ");
    int state = radio->begin();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("success");
    } else {
        Serial.print("failed, code ");
        Serial.println(state);
    }
}

void LoRaWANHandler::configureRadio() {
    int state;

    // Configure TCXO - Vision Master E290 uses crystal oscillator (not TCXO)
    Serial.print("Configuring oscillator (crystal mode)... ");
    state = radio->setTCXO(0);  // 0 = disable TCXO, use crystal
    Serial.println(state == RADIOLIB_ERR_NONE ? "success" : "failed");

    Serial.print("Configuring RF switch (DIO2)... ");
    state = radio->setDio2AsRfSwitch(true);
    Serial.println(state == RADIOLIB_ERR_NONE ? "success" : "failed");

    // Configure current limit (helps with power stability during RX)
    Serial.print("Configuring current limit... ");
    state = radio->setCurrentLimit(140);  // 140 mA
    Serial.println(state == RADIOLIB_ERR_NONE ? "success" : "failed");
}

// ============================================================================
// OTAA JOIN
// ============================================================================

bool LoRaWANHandler::join() {
    Serial.println("\nChecking for saved nonces (required for DevNonce tracking)...");

    bool noncesRestored = restoreNonces();

    // Initialize node if nonces weren't restored
    if (!noncesRestored) {
        Serial.println("\nInitializing LoRaWAN node...");
        Serial.println("Region: EU868");
        node->beginOTAA(joinEUI, devEUI, nwkKey, appKey);
    } else {
        Serial.println("\nNonces restored - proceeding with fresh join using incremented DevNonce...");
    }

    Serial.println("LoRaWAN credentials configured");

    // Attempt OTAA join
    Serial.println("\nAttempting OTAA join...");
    Serial.println("This may take 30-60 seconds...");
    Serial.println("Waiting for join accept from network server...");

    int state = node->activateOTAA();

    // Check for successful join
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
        Serial.println(node->getDevAddr(), HEX);
        joined = true;

        // Save nonces immediately after successful join to persist DevNonce
        Serial.println("\nSaving nonces to NVS for next boot...");
        saveSession();

        return true;
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
        } else if (state == RADIOLIB_ERR_CHIP_NOT_FOUND) {
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
        joined = false;
        return false;
    }
}

bool LoRaWANHandler::isJoined() const {
    return joined;
}

// ============================================================================
// UPLINK/DOWNLINK
// ============================================================================

bool LoRaWANHandler::sendUplink(const InputRegisters& input) {
    if (!joined) {
        Serial.println("LoRaWAN: Not joined, skipping uplink");
        return false;
    }

    Serial.println("========================================");
    Serial.println("Sending LoRaWAN uplink...");

    // Set device status for LoRaWAN network server
    // Battery level: 0 = external power, 1-254 = battery level, 255 = unable to measure
    node->setDeviceStatus(0);

    // Prepare payload in AdeunisModbusSf6 format (10 bytes)
    // Bytes 0-1: Header (uplink counter for tracking, decoder ignores these)
    // Bytes 2-3: SF6 Density (kg/m³ × 100, big-endian)
    // Bytes 4-5: SF6 Pressure @20C (bar × 1000, big-endian)
    // Bytes 6-7: SF6 Temperature (Kelvin × 10, big-endian)
    // Bytes 8-9: SF6 Pressure Variance (bar × 1000, big-endian)
    uint8_t payload[10];

    // Header (bytes 0-1) - Uplink counter for tracking
    payload[0] = (uplink_count >> 8) & 0xFF;
    payload[1] = uplink_count & 0xFF;

    // SF6 Density (2 bytes, big-endian)
    payload[2] = (input.sf6_density >> 8) & 0xFF;
    payload[3] = input.sf6_density & 0xFF;

    // SF6 Pressure @20C (2 bytes, big-endian)
    payload[4] = (input.sf6_pressure_20c >> 8) & 0xFF;
    payload[5] = input.sf6_pressure_20c & 0xFF;

    // SF6 Temperature (2 bytes, big-endian)
    payload[6] = (input.sf6_temperature >> 8) & 0xFF;
    payload[7] = input.sf6_temperature & 0xFF;

    // SF6 Pressure Variance (2 bytes, big-endian)
    payload[8] = (input.sf6_pressure_var >> 8) & 0xFF;
    payload[9] = input.sf6_pressure_var & 0xFF;

    // Print payload in hex for debugging
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

    // Send unconfirmed uplink on port 1
    uint8_t downlinkPayload[256];
    size_t downlinkSize = 0;

    int state = node->sendReceive(payload, sizeof(payload), 1, downlinkPayload, &downlinkSize);

    // RadioLib sendReceive() return values:
    // < 0: Error occurred
    // 0: Success, no downlink
    // 1: Success, downlink in RX1 window
    // 2: Success, downlink in RX2 window
    if (state >= RADIOLIB_ERR_NONE) {
        Serial.println("Uplink successful!");
        uplink_count++;

        // Get RSSI and SNR from last transmission
        last_rssi = radio->getRSSI();
        last_snr = radio->getSNR();

        Serial.print("RSSI: ");
        Serial.print(last_rssi);
        Serial.print(" dBm, SNR: ");
        Serial.print(last_snr);
        Serial.println(" dB");

        // Check if downlink was received
        if (state > 0) {
            Serial.print("Downlink received in RX");
            Serial.print(state);
            Serial.println(" window");
            downlink_count++;

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
        saveSession();

        Serial.println("========================================");
        return true;
    } else {
        Serial.print("Uplink failed, code ");
        Serial.println(state);
        Serial.println("========================================");
        return false;
    }
}

// ============================================================================
// CREDENTIALS MANAGEMENT
// ============================================================================

void LoRaWANHandler::generateCredentials() {
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

void LoRaWANHandler::loadCredentials() {
    Serial.println(">>> Opening LoRaWAN preferences namespace...");

    // Try to open in read-only mode first
    if (!preferences.begin("lorawan", true)) {
        // Namespace doesn't exist - this is first boot
        Serial.println(">>> Namespace doesn't exist (first boot)");
        Serial.println(">>> Generating new credentials...");

        // Generate and save new credentials
        generateCredentials();
        saveCredentials();
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
        generateCredentials();
        saveCredentials();
        return;
    }

    preferences.end();
    Serial.println(">>> Preferences closed");
}

void LoRaWANHandler::saveCredentials() {
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

void LoRaWANHandler::printCredentials() {
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
// SESSION PERSISTENCE
// ============================================================================

void LoRaWANHandler::saveSession() {
    Serial.println(">>> saveLoRaWANNonces() called");

    // Get NONCES buffer from RadioLib (needed to track DevNonce)
    uint8_t* noncesPtr = node->getBufferNonces();
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

void LoRaWANHandler::loadSession() {
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

    Serial.printf(">>> Calling setBufferSession()...\n");

    // Restore session to RadioLib (only needs buffer pointer)
    int16_t state = node->setBufferSession(sessionBuffer);
    Serial.printf(">>> setBufferSession() returned: %d\n", state);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(">>> Session restored successfully!");
        joined = true;

        uint32_t devAddr = node->getDevAddr();
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

bool LoRaWANHandler::restoreNonces() {
    if (!preferences.begin("lorawan", true)) {
        return false;
    }

    bool hasNonces = preferences.getBool("has_nonces", false);
    Serial.printf(">>> has_nonces flag: %s\n", hasNonces ? "true" : "false");
    preferences.end();

    if (!hasNonces) {
        return false;
    }

    Serial.println(">>> Found saved nonces - restoring...");

    // Initialize node first
    node->beginOTAA(joinEUI, devEUI, nwkKey, appKey);

    // Load ONLY nonces from NVS
    if (preferences.begin("lorawan", true)) {
        const size_t noncesSize = RADIOLIB_LORAWAN_NONCES_BUF_SIZE;
        uint8_t noncesBuffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
        size_t noncesRead = preferences.getBytes("nonces", noncesBuffer, noncesSize);
        preferences.end();

        if (noncesRead == noncesSize) {
            Serial.printf(">>> Loaded nonces (%d bytes)\n", noncesRead);
            int16_t state = node->setBufferNonces(noncesBuffer);
            Serial.printf(">>> setBufferNonces() returned: %d\n", state);

            if (state == RADIOLIB_ERR_NONE) {
                Serial.println(">>> Nonces restored - DevNonce will continue from last value");
                return true;
            } else {
                Serial.printf(">>> Nonces restore failed: %d\n", state);
            }
        }
    }

    return false;
}

// ============================================================================
// STATUS GETTERS
// ============================================================================

uint32_t LoRaWANHandler::getUplinkCount() const {
    return uplink_count;
}

uint32_t LoRaWANHandler::getDownlinkCount() const {
    return downlink_count;
}

int16_t LoRaWANHandler::getLastRSSI() const {
    return last_rssi;
}

float LoRaWANHandler::getLastSNR() const {
    return last_snr;
}

uint64_t LoRaWANHandler::getDevEUI() const {
    return devEUI;
}

uint64_t LoRaWANHandler::getJoinEUI() const {
    return joinEUI;
}

void LoRaWANHandler::getAppKey(uint8_t* buffer) const {
    memcpy(buffer, appKey, 16);
}

void LoRaWANHandler::getNwkKey(uint8_t* buffer) const {
    memcpy(buffer, nwkKey, 16);
}

uint32_t LoRaWANHandler::getDevAddr() const {
    if (node && joined) {
        return node->getDevAddr();
    }
    return 0;
}
