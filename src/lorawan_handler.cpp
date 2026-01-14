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
    active_profile_index(0),
    auto_rotation_enabled(false),
    joined(false),
    uplink_count(0),
    downlink_count(0),
    last_rssi(0),
    last_snr(0.0),
    last_uplink_time(0) {

    memset(last_profile_uplinks, 0, sizeof(last_profile_uplinks));
    memset(appKey, 0, sizeof(appKey));
    memset(nwkKey, 0, sizeof(nwkKey));
    
    // Initialize all profiles to empty
    for (int i = 0; i < MAX_LORA_PROFILES; i++) {
        memset(&profiles[i], 0, sizeof(LoRaProfile));
        profiles[i].enabled = false;
        snprintf(profiles[i].name, sizeof(profiles[i].name), "Profile %d", i);
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void LoRaWANHandler::begin(bool loadConfig) {
    Serial.println("\n========================================");
    Serial.println("Initializing LoRaWAN...");
    Serial.println("========================================");

    // Clean up existing instances if re-initializing
    if (node) {
        delete node;
        node = nullptr;
    }
    if (radio) {
        delete radio;
        radio = nullptr;
    }

    // Initialize SPI bus for LoRa radio (only once)
    static bool spi_initialized = false;
    if (!spi_initialized) {
        Serial.println("Initializing LoRa radio on SPI bus...");
        Serial.printf("  LoRa pins: SCK=%d, MISO=%d, MOSI=%d, NSS=%d\n", LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
        Serial.printf("  LoRa control: DIO1=%d, RESET=%d, BUSY=%d\n", LORA_DIO1, LORA_NRST, LORA_BUSY);

        Serial.print("Initializing SPI bus... ");
        SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
        Serial.println("done");
        delay(100);
        spi_initialized = true;
    }

    // Create radio instance
    radio = new SX1262(new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY));

    initializeRadio();
    configureRadio();

    if (loadConfig) {
        // Load profiles (generates if not present) - New multi-profile system
        loadProfiles();
        
        // Set active profile (loads credentials into legacy fields)
        setActiveProfile(active_profile_index);
    }
    
    // Print active profile info
    printProfile(active_profile_index);

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

    // Print diagnostic information before join attempt
    Serial.println("\n========================================");
    Serial.println("LoRaWAN Join Diagnostics");
    Serial.println("========================================");
    Serial.printf("Active Profile: %d (%s)\n", active_profile_index, profiles[active_profile_index].name);
    Serial.printf("DevEUI: 0x%016llX\n", devEUI);
    Serial.printf("JoinEUI: 0x%016llX\n", joinEUI);
    Serial.printf("Region: EU868\n");
    Serial.printf("TX Power: 14 dBm\n");
    Serial.printf("Data Rate: DR5 (SF7BW125)\n");
    Serial.println("========================================\n");

    // Attempt OTAA join
    Serial.println("Attempting OTAA join...");
    Serial.println("Transmitting join request...");
    unsigned long joinStart = millis();
    
    int state = node->activateOTAA();
    
    unsigned long joinDuration = millis() - joinStart;
    Serial.printf("Join attempt completed in %lu ms\n", joinDuration);

    // Save nonces after EVERY join attempt (successful or failed)
    // This keeps DevNonce synchronized even if device reboots after failed attempts
    // Critical: Network server may see failed join attempts and increment its expected DevNonce
    Serial.println("\nSaving DevNonce to NVS (keeps counter synchronized)...");
    saveSession();

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

        return true;
    } else {
        Serial.print("\nJoin failed, code ");
        Serial.println(state);

        // Print helpful error messages
        if (state == RADIOLIB_ERR_NO_JOIN_ACCEPT) {  // -1116
            Serial.println("Error: No Join-Accept received (RADIOLIB_ERR_NO_JOIN_ACCEPT)");
            Serial.println("  - Join request transmitted successfully");
            Serial.println("  - No response received from network server");
            Serial.println("\nPossible causes:");
            Serial.println("  1. Device not registered in network server (TTN/Chirpstack)");
            Serial.println("  2. Wrong credentials (DevEUI, JoinEUI, AppKey mismatch)");
            Serial.println("  3. No gateway in range or gateway offline");
            Serial.println("  4. Gateway not forwarding to correct network server");
            Serial.println("  5. Network server having issues");
            Serial.println("\nTroubleshooting:");
            Serial.println("  - Verify device is registered with EXACT credentials above");
            Serial.println("  - Check gateway coverage at your location");
            Serial.println("  - Verify gateway is connected to network server");
            Serial.println("  - Check network server console for join attempts");
            Serial.println("  - Try moving closer to a known gateway");
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

void LoRaWANHandler::performStartupSequence(const InputRegisters& input) {
    Serial.println("\n========================================");
    Serial.println("Startup Uplink Sequence");
    Serial.println("========================================");
    
    int enabled_count = getEnabledProfileCount();
    Serial.printf("Sending initial uplinks from %d enabled profile(s)...\n\n", enabled_count);
    
    // Track which profiles we've sent from
    int uplinks_sent = 0;
    
    // Get initial profile index to restore later
    uint8_t initial_profile = active_profile_index;
    
    // Iterate through all profiles and send from enabled ones
    for (int i = 0; i < MAX_LORA_PROFILES; i++) {
        if (!profiles[i].enabled) continue;
        
        // Switch to this profile
        Serial.printf("\n>>> Switching to Profile %d: %s\n", i, profiles[i].name);
        setActiveProfile(i);
        begin(false); // Re-initialize radio but DON'T reload profiles from NVS
        
        // Join with this profile
        Serial.printf(">>> Joining network with Profile %d...\n", i);
        if (join()) {
            Serial.printf(">>> Join successful for Profile %d!\n", i);
            delay(1000);
            
            // Send uplink
            Serial.printf(">>> Sending startup uplink from Profile %d\n", i);
            sendUplink(input);
            uplinks_sent++;
            
            // Mark this profile as having sent recently
            last_profile_uplinks[i] = millis();
            last_uplink_time = millis();
            
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
    
    // Return to initial profile (or keep last one if auto-rotation is on? usually restore initial)
    // If auto-rotation is on, we might want to start from the next one in sequence, but restoring initial is safer.
    if (active_profile_index != initial_profile) {
        Serial.printf("\n>>> Returning to initial Profile %d for normal operation\n", initial_profile);
        setActiveProfile(initial_profile);
        begin(false);
        join();
    }
}

void LoRaWANHandler::process(const InputRegisters& input) {
    // If not joined, try to join
    if (!joined) {
        static unsigned long last_join_attempt = 0;
        if (millis() - last_join_attempt > 30000) { // Retry every 30s
            last_join_attempt = millis();
            Serial.println("LoRaWAN not joined, attempting to join...");
            join();
        }
        return;
    }

    unsigned long now = millis();

    if (auto_rotation_enabled && getEnabledProfileCount() > 1) {
        // Multi-profile mode: Each profile sends every 5 minutes, staggered by 1 minute
        unsigned long time_since_last = now - last_profile_uplinks[active_profile_index];
        
        // Check if current profile is due for transmission (5 minutes since its last uplink)
        if (time_since_last >= 300000) {
            Serial.printf("Profile %d is due for uplink (5min elapsed)\n", active_profile_index);
            
            // Send uplink from current profile
            last_profile_uplinks[active_profile_index] = now;
            last_uplink_time = now;
            sendUplink(input);
            
            // After sending, rotate to next enabled profile and join
            if (rotateToNextProfile()) {
                Serial.println("Auto-rotation: Switching to next profile");
                
                // Re-join network with new profile credentials
                joined = false;
                begin(false); // Re-initialize radio
                if (join()) {
                    joined = true;
                    Serial.printf("Joined with profile %d\n", active_profile_index);
                } else {
                    Serial.println("Failed to join after profile rotation");
                }
            }
        } else {
            // Check if we should switch to a different profile that's ready to send
            // This ensures 1-minute staggering between profiles
            uint8_t next_profile = getNextEnabledProfile();
            if (next_profile != active_profile_index) {
                unsigned long next_time_since_last = now - last_profile_uplinks[next_profile];
                
                // If next profile is due AND at least 1 minute has passed since any uplink
                if (next_time_since_last >= 300000 && (now - last_uplink_time >= 60000)) {
                    Serial.printf("Switching to profile %d which is ready to send\n", next_profile);
                    
                    // Rotate to next profile
                    if (rotateToNextProfile()) {
                        // Re-join network
                        joined = false;
                        begin(false);
                        if (join()) {
                            joined = true;
                            Serial.printf("Joined with profile %d\n", active_profile_index);
                            
                            // Send immediately after joining
                            last_profile_uplinks[next_profile] = now;
                            last_uplink_time = now;
                            sendUplink(input);
                        } else {
                            Serial.println("Failed to join with next profile");
                        }
                    }
                }
            }
        }
    } else {
        // Single profile mode: Send every 5 minutes
        if (now - last_uplink_time >= 300000) {
            last_uplink_time = now;
            last_profile_uplinks[active_profile_index] = now;
            
            Serial.printf("Sending uplink from profile %d\n", active_profile_index);
            sendUplink(input);
        }
    }
}

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

    // Get current profile's payload type
    LoRaProfile* current_profile = getProfile(active_profile_index);
    PayloadType payload_type = current_profile ? current_profile->payload_type : PAYLOAD_ADEUNIS_MODBUS_SF6;
    
    Serial.printf("Using payload format: %s\n", PAYLOAD_TYPE_NAMES[payload_type]);

    // Prepare payload based on profile's payload type
    uint8_t payload[256];  // Max LoRaWAN payload size
    size_t payload_size = 0;
    
    switch (payload_type) {
        case PAYLOAD_ADEUNIS_MODBUS_SF6:
            payload_size = buildAdeunisModbusSF6Payload(payload, input);
            break;
        case PAYLOAD_VISTRON_LORA_MOD_CON:
            payload_size = buildVistronLoraModConPayload(payload, input);
            break;
        case PAYLOAD_CAYENNE_LPP:
            payload_size = buildCayenneLPPPayload(payload, input);
            break;
        case PAYLOAD_RAW_MODBUS:
            payload_size = buildRawModbusPayload(payload, input);
            break;
        case PAYLOAD_CUSTOM:
            payload_size = buildCustomPayload(payload, input);
            break;
        default:
            payload_size = buildAdeunisModbusSF6Payload(payload, input);
            break;
    }

    // Print payload in hex for debugging
    Serial.print("Payload (");
    Serial.print(payload_size);
    Serial.print(" bytes): ");
    for (size_t i = 0; i < payload_size; i++) {
        if (payload[i] < 0x10) Serial.print("0");
        Serial.print(payload[i], HEX);
    }
    Serial.println();

    // Send unconfirmed uplink on port 1
    uint8_t downlinkPayload[256];
    size_t downlinkSize = 0;

    int state = node->sendReceive(payload, payload_size, 1, downlinkPayload, &downlinkSize);

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
// PAYLOAD BUILDERS
// ============================================================================

size_t LoRaWANHandler::buildAdeunisModbusSF6Payload(uint8_t* payload, const InputRegisters& input) {
    // AdeunisModbusSf6 format (10 bytes)
    // Bytes 0-1: Header (uplink counter for tracking, decoder ignores these)
    // Bytes 2-3: SF6 Density (kg/m³ × 100, big-endian)
    // Bytes 4-5: SF6 Pressure @20C (bar × 1000, big-endian)
    // Bytes 6-7: SF6 Temperature (Kelvin × 10, big-endian)
    // Bytes 8-9: SF6 Pressure Variance (bar × 1000, big-endian)
    
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

    Serial.println("Payload breakdown (Adeunis Modbus SF6):");
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

    return 10;
}

size_t LoRaWANHandler::buildCayenneLPPPayload(uint8_t* payload, const InputRegisters& input) {
    // Cayenne LPP format (variable length)
    // Each data point: Channel (1) + Type (1) + Data (n bytes)
    size_t index = 0;

    // Channel 1: Temperature (Type 0x67, 2 bytes, 0.1°C signed)
    float temp_celsius = (input.sf6_temperature / 10.0) - 273.15;
    int16_t temp_encoded = (int16_t)(temp_celsius * 10);
    payload[index++] = 1;  // Channel
    payload[index++] = 0x67;  // Temperature type
    payload[index++] = (temp_encoded >> 8) & 0xFF;
    payload[index++] = temp_encoded & 0xFF;

    // Channel 2: Analog Input for Pressure (Type 0x02, 2 bytes, 0.01 signed)
    float pressure_bar = input.sf6_pressure_20c / 1000.0;
    int16_t pressure_encoded = (int16_t)(pressure_bar * 100);
    payload[index++] = 2;  // Channel
    payload[index++] = 0x02;  // Analog Input type
    payload[index++] = (pressure_encoded >> 8) & 0xFF;
    payload[index++] = pressure_encoded & 0xFF;

    // Channel 3: Analog Input for Density (Type 0x02, 2 bytes, 0.01 signed)
    float density_kgm3 = input.sf6_density / 100.0;
    int16_t density_encoded = (int16_t)(density_kgm3 * 100);
    payload[index++] = 3;  // Channel
    payload[index++] = 0x02;  // Analog Input type
    payload[index++] = (density_encoded >> 8) & 0xFF;
    payload[index++] = density_encoded & 0xFF;

    Serial.println("Payload breakdown (Cayenne LPP):");
    Serial.printf("  Ch1 Temperature: %.1f °C\n", temp_celsius);
    Serial.printf("  Ch2 Pressure: %.3f bar\n", pressure_bar);
    Serial.printf("  Ch3 Density: %.2f kg/m³\n", density_kgm3);

    return index;
}

size_t LoRaWANHandler::buildRawModbusPayload(uint8_t* payload, const InputRegisters& input) {
    // Raw Modbus registers format (10 bytes)
    // Send the raw uint16_t values as-is (big-endian)
    size_t index = 0;

    // Uplink counter (2 bytes)
    payload[index++] = (uplink_count >> 8) & 0xFF;
    payload[index++] = uplink_count & 0xFF;

    // SF6 Density (2 bytes)
    payload[index++] = (input.sf6_density >> 8) & 0xFF;
    payload[index++] = input.sf6_density & 0xFF;

    // SF6 Pressure @20C (2 bytes)
    payload[index++] = (input.sf6_pressure_20c >> 8) & 0xFF;
    payload[index++] = input.sf6_pressure_20c & 0xFF;

    // SF6 Temperature (2 bytes)
    payload[index++] = (input.sf6_temperature >> 8) & 0xFF;
    payload[index++] = input.sf6_temperature & 0xFF;

    // SF6 Pressure Variance (2 bytes)
    payload[index++] = (input.sf6_pressure_var >> 8) & 0xFF;
    payload[index++] = input.sf6_pressure_var & 0xFF;

    Serial.println("Payload breakdown (Raw Modbus Registers):");
    Serial.printf("  Uplink Count: %u\n", uplink_count);
    Serial.printf("  SF6 Density (raw): %u\n", input.sf6_density);
    Serial.printf("  SF6 Pressure @20C (raw): %u\n", input.sf6_pressure_20c);
    Serial.printf("  SF6 Temperature (raw): %u\n", input.sf6_temperature);
    Serial.printf("  SF6 Pressure Var (raw): %u\n", input.sf6_pressure_var);

    return index;
}

size_t LoRaWANHandler::buildCustomPayload(uint8_t* payload, const InputRegisters& input) {
    // Custom format - simple example (can be customized)
    // 1 byte: payload type identifier
    // 4 bytes: temperature (float, LSB)
    // 4 bytes: pressure (float, LSB)
    // 4 bytes: density (float, LSB)
    size_t index = 0;

    payload[index++] = 0xFF;  // Custom format identifier

    // Convert temperature to Celsius as float
    float temp_celsius = (input.sf6_temperature / 10.0) - 273.15;
    memcpy(&payload[index], &temp_celsius, 4);
    index += 4;

    // Convert pressure to bar as float
    float pressure_bar = input.sf6_pressure_20c / 1000.0;
    memcpy(&payload[index], &pressure_bar, 4);
    index += 4;

    // Convert density to kg/m³ as float
    float density_kgm3 = input.sf6_density / 100.0;
    memcpy(&payload[index], &density_kgm3, 4);
    index += 4;

    Serial.println("Payload breakdown (Custom Format):");
    Serial.printf("  Temperature: %.2f °C\n", temp_celsius);
    Serial.printf("  Pressure: %.3f bar\n", pressure_bar);
    Serial.printf("  Density: %.2f kg/m³\n", density_kgm3);

    return index;
}

size_t LoRaWANHandler::buildVistronLoraModConPayload(uint8_t* payload, const InputRegisters& input) {
    // Vistron LoRa Mod Con format (16 bytes total)
    // Frame Type 3: Periodic Modbus data uplink
    // Bytes 0-7: Vistron frame header
    // Bytes 8-15: Modbus sensor data (same as Trafag H72517o format)
    size_t index = 0;

    // Header for Frame Type 3 (Periodic Modbus uplink)
    payload[index++] = 0x03;  // Frame type: 3 = Periodic Modbus data
    payload[index++] = 0x00;  // Reserved/status byte
    payload[index++] = 0x00;  // Error code: 0 = No error
    
    // Additional header bytes (can be uplink counter, device status, etc.)
    payload[index++] = (uplink_count >> 8) & 0xFF;  // Uplink counter high byte
    payload[index++] = uplink_count & 0xFF;         // Uplink counter low byte
    payload[index++] = 0x00;  // Reserved
    payload[index++] = 0x00;  // Reserved
    payload[index++] = 0x08;  // Modbus data length (8 bytes)

    // Modbus sensor data (8 bytes) - Trafag H72517o SF6 format
    // Bytes 8-9: Density (kg/m³ × 100, big-endian)
    payload[index++] = (input.sf6_density >> 8) & 0xFF;
    payload[index++] = input.sf6_density & 0xFF;

    // Bytes 10-11: Absolute Pressure at 20°C (bar × 1000, big-endian)
    payload[index++] = (input.sf6_pressure_20c >> 8) & 0xFF;
    payload[index++] = input.sf6_pressure_20c & 0xFF;

    // Bytes 12-13: Temperature (Kelvin × 10, big-endian)
    payload[index++] = (input.sf6_temperature >> 8) & 0xFF;
    payload[index++] = input.sf6_temperature & 0xFF;

    // Bytes 14-15: Absolute Pressure current (bar × 1000, big-endian)
    // Using pressure_var as current pressure since we don't have a separate field
    payload[index++] = (input.sf6_pressure_var >> 8) & 0xFF;
    payload[index++] = input.sf6_pressure_var & 0xFF;

    Serial.println("Payload breakdown (Vistron Lora Mod Con):");
    Serial.printf("  Frame Type: 3 (Periodic Modbus uplink)\n");
    Serial.printf("  Error Code: 0 (No errors)\n");
    Serial.printf("  Uplink Count: %u\n", uplink_count);
    Serial.println("  Modbus Data (Trafag H72517o format):");
    Serial.printf("    Density: %u (%.2f kg/m³)\n",
        (payload[8] << 8) | payload[9],
        ((payload[8] << 8) | payload[9]) / 100.0);
    Serial.printf("    Pressure @20°C: %u (%.3f bar)\n",
        (payload[10] << 8) | payload[11],
        ((payload[10] << 8) | payload[11]) / 1000.0);
    Serial.printf("    Temperature: %u (%.1f °C)\n",
        (payload[12] << 8) | payload[13],
        (((payload[12] << 8) | payload[13]) / 10.0) - 273.15);
    Serial.printf("    Absolute Pressure: %u (%.3f bar)\n",
        (payload[14] << 8) | payload[15],
        ((payload[14] << 8) | payload[15]) / 1000.0);

    return index;
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

    // Open in read-write mode (creates if not exists)
    if (!preferences.begin("lorawan", false)) {
        Serial.println(">>> Failed to open lorawan preferences");
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
// PROFILE MANAGEMENT
// ============================================================================

void LoRaWANHandler::loadProfiles() {
    Serial.println(">>> Loading LoRaWAN profiles from NVS...");
    
    if (!preferences.begin("lorawan_prof", false)) {  // Read-write (creates if not exists)
        Serial.println(">>> Failed to open lorawan_prof namespace");
        initializeDefaultProfiles();
        return;
    }
    
    bool has_profiles = preferences.getBool("has_profiles", false);
    active_profile_index = preferences.getUChar("active_idx", 0);
    auto_rotation_enabled = preferences.getBool("auto_rotate", false);
    
    if (!has_profiles) {
        Serial.println(">>> No profiles found - initializing defaults");
        preferences.end();
        initializeDefaultProfiles();
        return;
    }
    
    // Load each profile
    for (int i = 0; i < MAX_LORA_PROFILES; i++) {
        char key[20];
        snprintf(key, sizeof(key), "prof%d", i);
        
        size_t len = preferences.getBytes(key, &profiles[i], sizeof(LoRaProfile));
        if (len == sizeof(LoRaProfile)) {
            // Validate payload_type (ensure it's within valid range)
            if (profiles[i].payload_type > PAYLOAD_VISTRON_LORA_MOD_CON) {
                Serial.printf("    Warning: Invalid payload_type for Profile %d, resetting to default\n", i);
                profiles[i].payload_type = PAYLOAD_ADEUNIS_MODBUS_SF6;
            }
            Serial.printf("    Loaded Profile %d: %s (%s, %s)\n", 
                i, profiles[i].name, 
                profiles[i].enabled ? "enabled" : "disabled",
                PAYLOAD_TYPE_NAMES[profiles[i].payload_type]);
        } else if (len > 0 && len < sizeof(LoRaProfile)) {
            // Partial load - likely from older firmware version without payload_type field
            Serial.printf("    Warning: Profile %d size mismatch (got %d bytes, expected %d) - setting default payload type\n", 
                i, len, sizeof(LoRaProfile));
            profiles[i].payload_type = PAYLOAD_ADEUNIS_MODBUS_SF6;
        } else {
            Serial.printf("    Warning: Profile %d load failed (got %d bytes)\n", i, len);
        }
    }
    
    preferences.end();
    Serial.printf(">>> Active profile index: %d\n", active_profile_index);
}

void LoRaWANHandler::saveProfiles() {
    Serial.println(">>> Saving LoRaWAN profiles to NVS...");
    
    if (!preferences.begin("lorawan_prof", false)) {
        Serial.println(">>> Failed to open lorawan_prof namespace for writing");
        return;
    }
    
    preferences.putBool("has_profiles", true);
    preferences.putUChar("active_idx", active_profile_index);
    preferences.putBool("auto_rotate", auto_rotation_enabled);
    
    // Save each profile
    for (int i = 0; i < MAX_LORA_PROFILES; i++) {
        char key[20];
        snprintf(key, sizeof(key), "prof%d", i);
        
        size_t written = preferences.putBytes(key, &profiles[i], sizeof(LoRaProfile));
        Serial.printf("    Saved Profile %d: %s (%d bytes)\n", 
            i, profiles[i].name, written);
    }
    
    preferences.end();
    Serial.println(">>> Profiles saved to NVS");
}

void LoRaWANHandler::initializeDefaultProfiles() {
    Serial.println(">>> Initializing default profiles...");
    
    // Generate first profile with unique credentials
    generateProfile(0, "Profile 0");
    profiles[0].enabled = true;  // First profile enabled by default
    
    // Initialize remaining profiles as disabled templates
    for (int i = 1; i < MAX_LORA_PROFILES; i++) {
        generateProfile(i, (String("Profile ") + String(i)).c_str());
        profiles[i].enabled = false;
    }
    
    active_profile_index = 0;
    
    // Save to NVS
    saveProfiles();
    
    Serial.println(">>> Default profiles initialized");
}

void LoRaWANHandler::generateProfile(uint8_t index, const char* name) {
    if (index >= MAX_LORA_PROFILES) {
        Serial.printf(">>> Error: Invalid profile index %d\n", index);
        return;
    }
    
    LoRaProfile* prof = &profiles[index];
    
    // Set name
    strncpy(prof->name, name, sizeof(prof->name) - 1);
    prof->name[sizeof(prof->name) - 1] = '\0';
    
    // Seed random number generator
    randomSeed(esp_random() + index);
    
    // Generate random JoinEUI (8 bytes)
    prof->joinEUI = 0;
    for (int i = 0; i < 8; i++) {
        prof->joinEUI = (prof->joinEUI << 8) | (uint64_t)random(0, 256);
    }
    
    // Generate DevEUI from ESP32 MAC address + random bytes for uniqueness
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    prof->devEUI = ((uint64_t)mac[0] << 56) | ((uint64_t)mac[1] << 48) |
                   ((uint64_t)mac[2] << 40) | ((uint64_t)mac[3] << 32) |
                   ((uint64_t)random(0, 256) << 24) | ((uint64_t)random(0, 256) << 16) |
                   ((uint64_t)(random(0, 256) + index) << 8) | (uint64_t)random(0, 256);
    
    // Generate random AppKey (16 bytes)
    for (int i = 0; i < 16; i++) {
        prof->appKey[i] = random(0, 256);
    }
    
    // Generate random NwkKey (16 bytes) - for LoRaWAN 1.0.x, copy AppKey
    memcpy(prof->nwkKey, prof->appKey, 16);
    
    // Set default payload type
    prof->payload_type = PAYLOAD_ADEUNIS_MODBUS_SF6;
    
    Serial.printf("    Generated Profile %d: %s\n", index, prof->name);
    Serial.printf("      DevEUI: 0x%016llX\n", prof->devEUI);
    Serial.printf("      JoinEUI: 0x%016llX\n", prof->joinEUI);
    Serial.printf("      Payload Type: %s\n", PAYLOAD_TYPE_NAMES[prof->payload_type]);
}

bool LoRaWANHandler::setActiveProfile(uint8_t index) {
    if (index >= MAX_LORA_PROFILES) {
        Serial.printf(">>> Error: Invalid profile index %d\n", index);
        return false;
    }
    
    if (!profiles[index].enabled) {
        Serial.printf(">>> Error: Profile %d is disabled\n", index);
        return false;
    }
    
    Serial.printf(">>> Setting active profile to %d: %s\n", index, profiles[index].name);
    
    active_profile_index = index;
    
    // Copy profile credentials to legacy fields (for backward compatibility)
    devEUI = profiles[index].devEUI;
    joinEUI = profiles[index].joinEUI;
    memcpy(appKey, profiles[index].appKey, 16);
    memcpy(nwkKey, profiles[index].nwkKey, 16);
    
    // Save active index to NVS
    if (preferences.begin("lorawan_prof", false)) {
        preferences.putUChar("active_idx", active_profile_index);
        preferences.end();
    }
    
    Serial.println(">>> Active profile updated");
    return true;
}

uint8_t LoRaWANHandler::getActiveProfileIndex() const {
    return active_profile_index;
}

LoRaProfile* LoRaWANHandler::getProfile(uint8_t index) {
    if (index >= MAX_LORA_PROFILES) {
        return nullptr;
    }
    return &profiles[index];
}

bool LoRaWANHandler::updateProfile(uint8_t index, const LoRaProfile& profile) {
    if (index >= MAX_LORA_PROFILES) {
        Serial.printf(">>> Error: Invalid profile index %d\n", index);
        return false;
    }
    
    Serial.printf(">>> Updating profile %d\n", index);
    
    // Copy profile data
    memcpy(&profiles[index], &profile, sizeof(LoRaProfile));
    
    // If this is the active profile, update legacy credentials
    if (index == active_profile_index) {
        devEUI = profile.devEUI;
        joinEUI = profile.joinEUI;
        memcpy(appKey, profile.appKey, 16);
        memcpy(nwkKey, profile.nwkKey, 16);
    }
    
    // Save to NVS
    saveProfiles();
    
    Serial.println(">>> Profile updated");
    return true;
}

bool LoRaWANHandler::toggleProfileEnabled(uint8_t index) {
    if (index >= MAX_LORA_PROFILES) {
        Serial.printf(">>> Error: Invalid profile index %d\n", index);
        return false;
    }
    
    // Don't allow disabling the active profile
    if (index == active_profile_index && profiles[index].enabled) {
        Serial.println(">>> Error: Cannot disable active profile");
        return false;
    }
    
    profiles[index].enabled = !profiles[index].enabled;
    Serial.printf(">>> Profile %d %s\n", index, 
        profiles[index].enabled ? "enabled" : "disabled");
    
    // Save to NVS
    saveProfiles();
    
    return true;
}

void LoRaWANHandler::printProfile(uint8_t index) {
    if (index >= MAX_LORA_PROFILES) {
        Serial.printf(">>> Error: Invalid profile index %d\n", index);
        return;
    }
    
    LoRaProfile* prof = &profiles[index];
    
    Serial.println("\n========================================");
    Serial.printf("LoRaWAN Profile %d: %s\n", index, prof->name);
    Serial.println("========================================");
    Serial.printf("Status:     %s\n", prof->enabled ? "ENABLED" : "DISABLED");
    Serial.printf("JoinEUI:    0x%016llX\n", prof->joinEUI);
    Serial.printf("DevEUI:     0x%016llX\n", prof->devEUI);
    Serial.print("AppKey:     ");
    for (int i = 0; i < 16; i++) {
        if (prof->appKey[i] < 0x10) Serial.print("0");
        Serial.print(prof->appKey[i], HEX);
    }
    Serial.println();
    Serial.print("NwkKey:     ");
    for (int i = 0; i < 16; i++) {
        if (prof->nwkKey[i] < 0x10) Serial.print("0");
        Serial.print(prof->nwkKey[i], HEX);
    }
    Serial.println();
    Serial.println("========================================\n");
}

// ============================================================================
// AUTO-ROTATION (MULTI-PROFILE CYCLING)
// ============================================================================

void LoRaWANHandler::setAutoRotation(bool enabled) {
    auto_rotation_enabled = enabled;
    Serial.printf(">>> Auto-rotation %s\n", enabled ? "enabled" : "disabled");
    
    // Save to NVS
    if (preferences.begin("lorawan_prof", false)) {
        preferences.putBool("auto_rotate", auto_rotation_enabled);
        preferences.end();
    }
}

bool LoRaWANHandler::getAutoRotation() const {
    return auto_rotation_enabled;
}

uint8_t LoRaWANHandler::getNextEnabledProfile() const {
    // Start searching from next profile after current
    uint8_t start_index = (active_profile_index + 1) % MAX_LORA_PROFILES;
    
    // Search for next enabled profile
    for (int i = 0; i < MAX_LORA_PROFILES; i++) {
        uint8_t check_index = (start_index + i) % MAX_LORA_PROFILES;
        if (profiles[check_index].enabled) {
            return check_index;
        }
    }
    
    // No other enabled profile found, return current
    return active_profile_index;
}

bool LoRaWANHandler::rotateToNextProfile() {
    if (!auto_rotation_enabled) {
        Serial.println(">>> Auto-rotation is disabled");
        return false;
    }
    
    uint8_t next_index = getNextEnabledProfile();
    
    if (next_index == active_profile_index) {
        Serial.println(">>> No other enabled profiles for rotation");
        return false;
    }
    
    Serial.printf(">>> Rotating from profile %d to profile %d\n", 
        active_profile_index, next_index);
    
    return setActiveProfile(next_index);
}

int LoRaWANHandler::getEnabledProfileCount() const {
    int count = 0;
    for (int i = 0; i < MAX_LORA_PROFILES; i++) {
        if (profiles[i].enabled) {
            count++;
        }
    }
    return count;
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

    // Save nonces per profile using profile-specific key with retry and verification
    char noncesKey[16];
    sprintf(noncesKey, "nonces_%d", active_profile_index);
    char hasNoncesKey[16];
    sprintf(hasNoncesKey, "has_nonces_%d", active_profile_index);
    
    const int MAX_RETRIES = 3;
    bool success = false;
    
    for (int attempt = 1; attempt <= MAX_RETRIES && !success; attempt++) {
        if (attempt > 1) {
            Serial.printf(">>> Retry attempt %d/%d...\n", attempt, MAX_RETRIES);
            delay(10); // Short delay before retry
        }
        
        // Write nonces to NVS
        size_t written = preferences.putBytes(noncesKey, noncesBuffer, noncesSize);
        
        // Verify write size
        if (written != noncesSize) {
            Serial.printf(">>> ERROR: Nonces write failed! Expected %d bytes, wrote %d bytes\n", noncesSize, written);
            continue; // Try again
        }
        
        // Read back and verify
        uint8_t verifyBuffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
        size_t readBack = preferences.getBytes(noncesKey, verifyBuffer, noncesSize);
        
        if (readBack != noncesSize) {
            Serial.printf(">>> ERROR: Nonces verification read failed! Expected %d bytes, read %d bytes\n", noncesSize, readBack);
            continue; // Try again
        }
        
        // Compare written and read data
        if (memcmp(noncesBuffer, verifyBuffer, noncesSize) == 0) {
            success = true;
            preferences.putBool(hasNoncesKey, true);
            Serial.printf(">>> ✓ Nonces verified: %d bytes saved correctly for Profile %d (attempt %d/%d)\n", 
                noncesSize, active_profile_index, attempt, MAX_RETRIES);
        } else {
            Serial.printf(">>> ERROR: Nonces verification failed! Data mismatch on attempt %d/%d\n", attempt, MAX_RETRIES);
        }
    }
    
    preferences.end();
    
    if (!success) {
        Serial.printf(">>> CRITICAL: Failed to save nonces after %d attempts! DevNonce may not persist!\n", MAX_RETRIES);
    }
}

void LoRaWANHandler::loadSession() {
    Serial.println(">>> load_lorawan_session() called");

    if (!preferences.begin("lorawan", false)) {  // Read-write (creates if not exists)
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

void LoRaWANHandler::resetNonces() {
    Serial.println("\n========================================");
    Serial.println("Resetting LoRaWAN Nonces");
    Serial.println("========================================");
    
    if (!preferences.begin("lorawan", false)) {
        Serial.println("Error: Failed to open NVS for nonce reset");
        return;
    }
    
    // Clear nonces for all profiles
    for (int i = 0; i < MAX_LORA_PROFILES; i++) {
        char hasNoncesKey[16];
        char noncesKey[16];
        sprintf(hasNoncesKey, "has_nonces_%d", i);
        sprintf(noncesKey, "nonces_%d", i);
        
        // Check if keys exist before trying to remove them to avoid NVS errors
        bool hasNoncesExists = preferences.isKey(hasNoncesKey);
        bool noncesExists = preferences.isKey(noncesKey);
        
        if (hasNoncesExists) {
            preferences.remove(hasNoncesKey);
        }
        if (noncesExists) {
            preferences.remove(noncesKey);
        }
        
        Serial.printf("✓ Cleared nonces for Profile %d\n", i);
    }
    
    preferences.end();
    
    Serial.println("✓ All nonces reset - DevNonce will start fresh on next join");
    Serial.println("✓ This resolves nonce misalignment with network server");
    Serial.println("========================================\n");
}

bool LoRaWANHandler::restoreNonces() {
    if (!preferences.begin("lorawan", false)) {  // Read-write (creates if not exists)
        return false;
    }

    // Check for profile-specific nonces
    char hasNoncesKey[16];
    sprintf(hasNoncesKey, "has_nonces_%d", active_profile_index);
    bool hasNonces = preferences.getBool(hasNoncesKey, false);
    Serial.printf(">>> has_nonces flag for Profile %d: %s\n", active_profile_index, hasNonces ? "true" : "false");
    preferences.end();

    if (!hasNonces) {
        Serial.printf(">>> No saved nonces found for Profile %d\n", active_profile_index);
        return false;
    }

    Serial.printf(">>> Found saved nonces for Profile %d - restoring...\n", active_profile_index);

    // Initialize node first
    node->beginOTAA(joinEUI, devEUI, nwkKey, appKey);

    // Load profile-specific nonces from NVS
    if (preferences.begin("lorawan", false)) {  // Read-write (creates if not exists)
        const size_t noncesSize = RADIOLIB_LORAWAN_NONCES_BUF_SIZE;
        uint8_t noncesBuffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
        
        char noncesKey[16];
        sprintf(noncesKey, "nonces_%d", active_profile_index);
        size_t noncesRead = preferences.getBytes(noncesKey, noncesBuffer, noncesSize);
        preferences.end();

        if (noncesRead == noncesSize) {
            Serial.printf(">>> Loaded nonces (%d bytes) for Profile %d\n", noncesRead, active_profile_index);
            int16_t state = node->setBufferNonces(noncesBuffer);
            Serial.printf(">>> setBufferNonces() returned: %d\n", state);

            if (state == RADIOLIB_ERR_NONE) {
                Serial.printf(">>> Nonces restored for Profile %d - DevNonce will continue from last value\n", active_profile_index);
                return true;
            } else {
                Serial.printf(">>> Nonces restore failed for Profile %d: %d\n", active_profile_index, state);
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

int LoRaWANHandler::getEnabledDevEUIs(uint64_t* euis, int max_count) const {
    int count = 0;
    for (int i = 0; i < MAX_LORA_PROFILES && count < max_count; i++) {
        if (profiles[i].enabled) {
            euis[count++] = profiles[i].devEUI;
        }
    }
    return count;
}
