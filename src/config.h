#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// FIRMWARE VERSION
// ============================================================================
// Format: MMmm where MM = major version, mm = minor version (2 digits)
// Examples: 111 = v1.11, 203 = v2.03, 1545 = v15.45
// Display format: v(FIRMWARE_VERSION/100).(FIRMWARE_VERSION%100)
#define FIRMWARE_VERSION 147  // v1.47 - Added factory reset feature in security tab

// ============================================================================
// DEPLOYMENT CONFIGURATION
// ============================================================================
// false = Development mode (default password: "modbus123")
// true  = Production mode (auto-generated strong password, stored in NVS)
#define MODE_PRODUCTION false

// ============================================================================
// DISPLAY CONFIGURATION
// ============================================================================
// Display rotation: 0=Portrait, 1=Landscape, 2=Portrait inverted, 3=Landscape inverted
#define DISPLAY_ROTATION 3

// ============================================================================
// MODBUS CONFIGURATION
// ============================================================================
#define MB_UART_NUM         1        // UART1
#define MB_UART_TX          43       // GPIO 43
#define MB_UART_RX          44       // GPIO 44
#define MB_UART_BAUD        9600
#define MB_SLAVE_ID_DEFAULT 1

// ============================================================================
// LORAWAN CONFIGURATION
// ============================================================================
// Vision Master E290 SX1262 LoRa Radio Pins
#define LORA_MOSI   10   // MOSI
#define LORA_MISO   11   // MISO
#define LORA_SCK    9    // SCK
#define LORA_NSS    8    // NSS/CS
#define LORA_DIO1   14   // DIO1
#define LORA_NRST   12   // RESET
#define LORA_BUSY   13   // BUSY

#define LORAWAN_ENABLED true

// ============================================================================
// WIFI CONFIGURATION
// ============================================================================
const unsigned long WIFI_TIMEOUT_MS = 20 * 60 * 1000;  // 20 minutes

#endif // CONFIG_H
