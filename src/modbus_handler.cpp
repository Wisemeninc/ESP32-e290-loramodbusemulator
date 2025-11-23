#include "modbus_handler.h"
#include "config.h"

// Global instance
ModbusHandler modbusHandler;

// Static instance pointer for callbacks
static ModbusHandler* instance = nullptr;

// ============================================================================
// CONSTRUCTOR
// ============================================================================

ModbusHandler::ModbusHandler() : slave_id(MB_SLAVE_ID_DEFAULT) {
    instance = this;
    memset(&holding_regs, 0, sizeof(holding_regs));
    memset(&input_regs, 0, sizeof(input_regs));
    memset(&stats, 0, sizeof(stats));
}

// ============================================================================
// PUBLIC METHODS
// ============================================================================

void ModbusHandler::begin(uint8_t slave_id) {
    this->slave_id = slave_id;

    Serial.println("\n========================================");
    Serial.println("Initializing Modbus RTU Slave...");
    Serial.println("========================================");

    // Initialize UART for RS485 (HW-519 module)
    Serial1.begin(MB_UART_BAUD, SERIAL_8N1, MB_UART_RX, MB_UART_TX);

    Serial.printf("UART1: TX=GPIO%d, RX=GPIO%d, Baud=%d\n",
                  MB_UART_TX, MB_UART_RX, MB_UART_BAUD);
    Serial.println("HW-519: Automatic flow control (no RTS needed)");

    // Configure Modbus RTU slave
    mb.begin(&Serial1);
    mb.slave(this->slave_id);

    // Add holding registers (0-11) - Read/Write
    for (uint16_t i = 0; i < 12; i++) {
        mb.addHreg(i, 0);
    }

    // Add input registers (0-8) - Read Only
    for (uint16_t i = 0; i < 9; i++) {
        mb.addIreg(i, 0);
    }

    // Initialize input registers 4-8 with device information
    input_regs.slave_id = slave_id;
    
    // Get ESP32 MAC address for serial number
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    input_regs.serial_hi = (mac[0] << 8) | mac[1];  // First 2 bytes
    input_regs.serial_lo = (mac[4] << 8) | mac[5];  // Last 2 bytes
    
    input_regs.sw_release = FIRMWARE_VERSION;  // From config.h
    input_regs.quartz_freq = 4000;  // 40.00 MHz (ESP32-S3 crystal frequency)

    // Set up callbacks
    mb.onGetHreg(0, cbRead, 13);   // Holding regs 0-12 (uptime now uses 2 regs)
    mb.onSetHreg(0, cbWrite, 13);  // Allow writes to holding regs
    mb.onGetIreg(0, cbRead, 9);    // Input regs 0-8

    Serial.printf("Modbus Slave ID: %d\n", this->slave_id);
    Serial.println("Holding Registers: 0-11 (Read/Write)");
    Serial.println("Input Registers: 0-8 (Read Only)");
    Serial.println("Modbus RTU Slave initialized!");
    Serial.println("========================================\n");
}

void ModbusHandler::task() {
    mb.task();
}

HoldingRegisters& ModbusHandler::getHoldingRegisters() {
    return holding_regs;
}

InputRegisters& ModbusHandler::getInputRegisters() {
    return input_regs;
}

ModbusStats& ModbusHandler::getStats() {
    return stats;
}

void ModbusHandler::updateHoldingRegisters(bool wifi_enabled, uint8_t wifi_clients) {
    // Update system metrics
    holding_regs.uptime_seconds = millis() / 1000;

    uint32_t free_heap_kb = ESP.getFreeHeap() / 1024;
    holding_regs.free_heap_kb_low = free_heap_kb & 0xFFFF;
    holding_regs.free_heap_kb_high = (free_heap_kb >> 16) & 0xFFFF;

    holding_regs.min_heap_kb = ESP.getMinFreeHeap() / 1024;
    holding_regs.cpu_freq_mhz = ESP.getCpuFreqMHz();
    holding_regs.task_count = uxTaskGetNumberOfTasks();

    // Temperature (ESP32-S3 doesn't have built-in temp sensor, use placeholder)
    holding_regs.temperature_x10 = 250;  // 25.0°C placeholder

    holding_regs.cpu_cores = 2;  // ESP32-S3 has 2 cores
    holding_regs.wifi_enabled = wifi_enabled ? 1 : 0;
    holding_regs.wifi_clients = wifi_clients;

    // Update random number every 5 seconds
    static unsigned long last_random_update = 0;
    if (millis() - last_random_update >= 5000) {
        holding_regs.random_number = random(0, 65536);
        last_random_update = millis();
    }
}

void ModbusHandler::updateInputRegisters(float sf6_density, float sf6_pressure, float sf6_temperature) {
    // Update SF6 sensor values
    input_regs.sf6_density = (uint16_t)(sf6_density * 100.0);
    input_regs.sf6_pressure_20c = (uint16_t)(sf6_pressure * 10.0);
    input_regs.sf6_temperature = (uint16_t)(sf6_temperature * 10.0);
    input_regs.sf6_pressure_var = input_regs.sf6_pressure_20c;  // Same as pressure for now
}

void ModbusHandler::setSlaveId(uint8_t slave_id) {
    this->slave_id = slave_id;
    input_regs.slave_id = slave_id;  // Update input register
    mb.slave(slave_id);
    Serial.printf("Modbus Slave ID changed to: %d\n", slave_id);
}

uint8_t ModbusHandler::getSlaveId() {
    return slave_id;
}

// ============================================================================
// MODBUS CALLBACKS
// ============================================================================

uint16_t ModbusHandler::cbRead(TRegister* reg, uint16_t val) {
    if (!instance) return 0;

    instance->stats.request_count++;
    instance->stats.read_count++;

    uint16_t addr = reg->address.address;

    // Determine if holding or input register based on register type
    // For holding registers
    if (reg->address.type == TAddress::HREG) {
        // Increment sequential counter on every read of register 0
        if (addr == 0) {
            instance->holding_regs.sequential_counter++;
        }

        switch (addr) {
            case 0: return instance->holding_regs.sequential_counter;
            case 1: return instance->holding_regs.random_number;
            case 2: return (uint16_t)(instance->holding_regs.uptime_seconds & 0xFFFF);  // Low word
            case 3: return (uint16_t)(instance->holding_regs.uptime_seconds >> 16);     // High word
            case 4: return instance->holding_regs.free_heap_kb_low;
            case 5: return instance->holding_regs.free_heap_kb_high;
            case 6: return instance->holding_regs.min_heap_kb;
            case 7: return instance->holding_regs.cpu_freq_mhz;
            case 8: return instance->holding_regs.task_count;
            case 9: return instance->holding_regs.temperature_x10;
            case 10: return instance->holding_regs.cpu_cores;
            case 11: return instance->holding_regs.wifi_enabled;
            case 12: return instance->holding_regs.wifi_clients;
            default: return 0;
        }
    }
    // For input registers
    else if (reg->address.type == TAddress::IREG) {
        switch (addr) {
            case 0: return instance->input_regs.sf6_density;
            case 1: return instance->input_regs.sf6_pressure_20c;
            case 2: return instance->input_regs.sf6_temperature;
            case 3: return instance->input_regs.sf6_pressure_var;
            case 4: return instance->input_regs.slave_id;
            case 5: return instance->input_regs.serial_hi;
            case 6: return instance->input_regs.serial_lo;
            case 7: return instance->input_regs.sw_release;
            case 8: return instance->input_regs.quartz_freq;
            default: return 0;
        }
    }

    return 0;
}

uint16_t ModbusHandler::cbWrite(TRegister* reg, uint16_t val) {
    if (!instance) return val;

    instance->stats.request_count++;
    instance->stats.write_count++;

    uint16_t addr = reg->address.address;

    // Allow writing to register 0 (sequential counter)
    if (addr == 0) {
        instance->holding_regs.sequential_counter = val;
        Serial.printf("Modbus Write: Register 0 = %d\n", val);
    }

    return val;
}
