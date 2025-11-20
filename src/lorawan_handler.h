#ifndef LORAWAN_HANDLER_H
#define LORAWAN_HANDLER_H

#include <Arduino.h>
#include <RadioLib.h>
#include <Preferences.h>
#include "config.h"

// ============================================================================
// LORAWAN HANDLER CLASS
// ============================================================================

// Forward declarations for parameter structures
struct InputRegisters;

class LoRaWANHandler {
public:
    LoRaWANHandler();

    // Initialization
    void begin(bool loadConfig = true);

    // OTAA Join
    bool join();
    bool isJoined() const;

    // Uplink/Downlink
    bool sendUplink(const InputRegisters& input);
    
    // Main processing loop (handles auto-rotation and periodic uplinks)
    void process(const InputRegisters& input);

    // Startup sequence (send initial uplink from all enabled profiles)
    void performStartupSequence(const InputRegisters& input);

    // Credentials management (legacy - for backward compatibility)
    void loadCredentials();
    void saveCredentials();
    void generateCredentials();
    void printCredentials();

    // Profile management (new multi-profile system)
    void loadProfiles();
    void saveProfiles();
    void initializeDefaultProfiles();
    void generateProfile(uint8_t index, const char* name);
    bool setActiveProfile(uint8_t index);
    uint8_t getActiveProfileIndex() const;
    LoRaProfile* getProfile(uint8_t index);
    bool updateProfile(uint8_t index, const LoRaProfile& profile);
    bool toggleProfileEnabled(uint8_t index);
    void printProfile(uint8_t index);
    
    // Auto-rotation (cycle through multiple profiles)
    void setAutoRotation(bool enabled);
    bool getAutoRotation() const;
    bool rotateToNextProfile();
    uint8_t getNextEnabledProfile() const;
    int getEnabledProfileCount() const;

    // Session persistence (nonces only - session restore not working in RadioLib 7.4.0)
    void saveSession();
    void loadSession();

    // Status getters
    uint32_t getUplinkCount() const;
    uint32_t getDownlinkCount() const;
    int16_t getLastRSSI() const;
    float getLastSNR() const;
    uint64_t getDevEUI() const;
    uint64_t getJoinEUI() const;
    void getAppKey(uint8_t* buffer) const;
    void getNwkKey(uint8_t* buffer) const;
    uint32_t getDevAddr() const;
    int getEnabledDevEUIs(uint64_t* euis, int max_count) const;

    // Payload builders
    size_t buildAdeunisModbusSF6Payload(uint8_t* payload, const InputRegisters& input);
    size_t buildCayenneLPPPayload(uint8_t* payload, const InputRegisters& input);
    size_t buildRawModbusPayload(uint8_t* payload, const InputRegisters& input);
    size_t buildCustomPayload(uint8_t* payload, const InputRegisters& input);
    size_t buildVistronLoraModConPayload(uint8_t* payload, const InputRegisters& input);

private:
    Preferences preferences;

    // Radio and node instances
    SX1262* radio;
    LoRaWANNode* node;

    // LoRaWAN credentials (OTAA) - legacy, kept for backward compatibility
    uint64_t joinEUI;  // AppEUI (MSB)
    uint64_t devEUI;   // DevEUI (MSB)
    uint8_t appKey[16]; // 128-bit AppKey (MSB)
    uint8_t nwkKey[16]; // 128-bit NwkKey (MSB)

    // Multi-profile system
    LoRaProfile profiles[MAX_LORA_PROFILES];
    uint8_t active_profile_index;
    bool auto_rotation_enabled;

    // Status
    bool joined;
    uint32_t uplink_count;
    uint32_t downlink_count;
    int16_t last_rssi;
    float last_snr;
    
    // Timing
    unsigned long last_uplink_time;
    unsigned long last_profile_uplinks[MAX_LORA_PROFILES];

    // Helper functions
    void initializeRadio();
    void configureRadio();
    bool restoreNonces();
};

// Global instance
extern LoRaWANHandler lorawanHandler;

#endif // LORAWAN_HANDLER_H
