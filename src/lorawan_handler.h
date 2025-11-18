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
    void begin();

    // OTAA Join
    bool join();
    bool isJoined() const;

    // Uplink/Downlink
    bool sendUplink(const InputRegisters& input);

    // Credentials management
    void loadCredentials();
    void saveCredentials();
    void generateCredentials();
    void printCredentials();

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

private:
    Preferences preferences;

    // Radio and node instances
    SX1262* radio;
    LoRaWANNode* node;

    // LoRaWAN credentials (OTAA)
    uint64_t joinEUI;  // AppEUI (MSB)
    uint64_t devEUI;   // DevEUI (MSB)
    uint8_t appKey[16]; // 128-bit AppKey (MSB)
    uint8_t nwkKey[16]; // 128-bit NwkKey (MSB)

    // Status
    bool joined;
    uint32_t uplink_count;
    uint32_t downlink_count;
    int16_t last_rssi;
    float last_snr;

    // Helper functions
    void initializeRadio();
    void configureRadio();
    bool restoreNonces();
};

// Global instance
extern LoRaWANHandler lorawanHandler;

#endif // LORAWAN_HANDLER_H
