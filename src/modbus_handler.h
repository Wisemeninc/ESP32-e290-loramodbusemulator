#ifndef MODBUS_HANDLER_H
#define MODBUS_HANDLER_H

#include <Arduino.h>
#include <ModbusRTU.h>

// ============================================================================
// MODBUS DATA STRUCTURES
// ============================================================================

struct HoldingRegisters {
    uint16_t sequential_counter;
    uint16_t random_number;
    uint16_t uptime_seconds;
    uint16_t free_heap_kb_low;
    uint16_t free_heap_kb_high;
    uint16_t min_heap_kb;
    uint16_t cpu_freq_mhz;
    uint16_t task_count;
    uint16_t temperature_x10;
    uint16_t cpu_cores;
    uint16_t wifi_enabled;
    uint16_t wifi_clients;
};

struct InputRegisters {
    uint16_t sf6_density;           // kg/m3 x 100
    uint16_t sf6_pressure_20c;      // kPa x 10
    uint16_t sf6_temperature;       // K x 10
    uint16_t sf6_pressure_var;      // kPa x 10
    uint16_t slave_id;
    uint16_t serial_hi;
    uint16_t serial_lo;
    uint16_t sw_release;
    uint16_t quartz_freq;
};

// ============================================================================
// MODBUS STATISTICS
// ============================================================================

struct ModbusStats {
    uint32_t request_count;
    uint32_t read_count;
    uint32_t write_count;
    uint32_t error_count;
};

// ============================================================================
// MODBUS HANDLER CLASS
// ============================================================================

class ModbusHandler {
public:
    ModbusHandler();

    void begin(uint8_t slave_id);
    void task();  // Call from loop()

    // Register access
    HoldingRegisters& getHoldingRegisters();
    InputRegisters& getInputRegisters();
    ModbusStats& getStats();

    // Register updates
    void updateHoldingRegisters(bool wifi_enabled, uint8_t wifi_clients);
    void updateInputRegisters(float sf6_density, float sf6_pressure, float sf6_temperature);

    // Configuration
    void setSlaveId(uint8_t slave_id);
    uint8_t getSlaveId();

private:
    ModbusRTU mb;
    HoldingRegisters holding_regs;
    InputRegisters input_regs;
    ModbusStats stats;
    uint8_t slave_id;

    // Modbus callbacks
    static uint16_t cbRead(TRegister* reg, uint16_t val);
    static uint16_t cbWrite(TRegister* reg, uint16_t val);
};

// Global instance
extern ModbusHandler modbusHandler;

#endif // MODBUS_HANDLER_H
