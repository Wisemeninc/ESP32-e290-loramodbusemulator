/**
 * LoRaWAN Payload Decoder for Vision Master E290 - SF₆ Monitor
 *
 * Compatible with:
 * - The Things Network (TTN) v3
 * - Chirpstack v4
 *
 * Payload Format (10 bytes):
 * - Bytes 0-1: SF₆ Density (uint16, big-endian) - Scale: ×0.01 kg/m³
 * - Bytes 2-3: SF₆ Pressure @20°C (uint16, big-endian) - Scale: ×0.1 kPa
 * - Bytes 4-5: SF₆ Temperature (uint16, big-endian) - Scale: ×0.1 K
 * - Bytes 6-7: SF₆ Pressure Variance (uint16, big-endian) - Scale: ×0.1 kPa
 * - Bytes 8-9: Modbus Request Counter (uint16, big-endian)
 */

// ============================================================================
// TTN v3 Decoder (Payload Formatters -> Uplink)
// ============================================================================

function decodeUplink(input) {
  var bytes = input.bytes;
  var port = input.fPort;

  // Only decode port 1
  if (port !== 1) {
    return {
      data: {},
      warnings: ["Unexpected port: " + port],
      errors: []
    };
  }

  // Check payload length
  if (bytes.length !== 10) {
    return {
      data: {},
      warnings: [],
      errors: ["Invalid payload length: expected 10 bytes, got " + bytes.length]
    };
  }

  // Decode payload
  var decoded = {
    sf6_density: ((bytes[0] << 8) | bytes[1]) / 100.0,           // kg/m³
    sf6_pressure_20c: ((bytes[2] << 8) | bytes[3]) / 10.0,       // kPa
    sf6_temperature_k: ((bytes[4] << 8) | bytes[5]) / 10.0,      // K
    sf6_temperature_c: (((bytes[4] << 8) | bytes[5]) / 10.0) - 273.15,  // °C
    sf6_pressure_var: ((bytes[6] << 8) | bytes[7]) / 10.0,       // kPa
    modbus_counter: (bytes[8] << 8) | bytes[9]                   // count
  };

  return {
    data: decoded,
    warnings: [],
    errors: []
  };
}

// ============================================================================
// Chirpstack v4 Decoder
// ============================================================================

function Decode(fPort, bytes) {
  // Only decode port 1
  if (fPort !== 1) {
    return {
      error: "Unexpected port: " + fPort
    };
  }

  // Check payload length
  if (bytes.length !== 10) {
    return {
      error: "Invalid payload length: expected 10 bytes, got " + bytes.length
    };
  }

  // Decode payload
  return {
    sf6_density: ((bytes[0] << 8) | bytes[1]) / 100.0,           // kg/m³
    sf6_pressure_20c: ((bytes[2] << 8) | bytes[3]) / 10.0,       // kPa
    sf6_temperature_k: ((bytes[4] << 8) | bytes[5]) / 10.0,      // K
    sf6_temperature_c: (((bytes[4] << 8) | bytes[5]) / 10.0) - 273.15,  // °C
    sf6_pressure_var: ((bytes[6] << 8) | bytes[7]) / 10.0,       // kPa
    modbus_counter: (bytes[8] << 8) | bytes[9]                   // count
  };
}

// ============================================================================
// Test Examples
// ============================================================================

// Example payload: [0x09, 0xFA, 0x15, 0x7C, 0x0B, 0x72, 0x15, 0x7C, 0x00, 0x2A]
// This represents:
// - Density: 0x09FA = 2554 → 25.54 kg/m³
// - Pressure @20C: 0x157C = 5500 → 550.0 kPa
// - Temperature: 0x0B72 = 2930 → 293.0 K (19.85°C)
// - Pressure Var: 0x157C = 5500 → 550.0 kPa
// - Modbus Counter: 0x002A = 42

// TTN v3 Test
console.log("TTN v3 Decoder Test:");
var ttn_result = decodeUplink({
  bytes: [0x09, 0xFA, 0x15, 0x7C, 0x0B, 0x72, 0x15, 0x7C, 0x00, 0x2A],
  fPort: 1
});
console.log(JSON.stringify(ttn_result, null, 2));

// Chirpstack Test
console.log("\nChirpstack Decoder Test:");
var chirpstack_result = Decode(1, [0x09, 0xFA, 0x15, 0x7C, 0x0B, 0x72, 0x15, 0x7C, 0x00, 0x2A]);
console.log(JSON.stringify(chirpstack_result, null, 2));

// ============================================================================
// Expected Output
// ============================================================================
/*
TTN v3 Decoder Test:
{
  "data": {
    "sf6_density": 25.54,
    "sf6_pressure_20c": 550,
    "sf6_temperature_k": 293,
    "sf6_temperature_c": 19.85,
    "sf6_pressure_var": 550,
    "modbus_counter": 42
  },
  "warnings": [],
  "errors": []
}

Chirpstack Decoder Test:
{
  "sf6_density": 25.54,
  "sf6_pressure_20c": 550,
  "sf6_temperature_k": 293,
  "sf6_temperature_c": 19.85,
  "sf6_pressure_var": 550,
  "modbus_counter": 42
}
*/
