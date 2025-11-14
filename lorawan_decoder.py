#!/usr/bin/env python3
"""
LoRaWAN Payload Decoder for Vision Master E290 - SF₆ Monitor

Python implementation for local testing and debugging.

Usage:
    python3 lorawan_decoder.py 09FA157C0B72157C002A
"""

import sys
import struct


def decode_payload(hex_string):
    """
    Decode LoRaWAN payload from hex string.

    Args:
        hex_string: Hex string of payload (e.g., "09FA157C0B72157C002A")

    Returns:
        Dictionary with decoded values
    """
    # Remove spaces and convert to bytes
    hex_string = hex_string.replace(" ", "").replace("0x", "")

    if len(hex_string) != 20:  # 10 bytes = 20 hex characters
        return {"error": f"Invalid payload length: expected 20 hex characters (10 bytes), got {len(hex_string)}"}

    try:
        payload = bytes.fromhex(hex_string)
    except ValueError as e:
        return {"error": f"Invalid hex string: {e}"}

    # Decode payload (big-endian uint16 values)
    sf6_density_raw = struct.unpack('>H', payload[0:2])[0]
    sf6_pressure_20c_raw = struct.unpack('>H', payload[2:4])[0]
    sf6_temperature_raw = struct.unpack('>H', payload[4:6])[0]
    sf6_pressure_var_raw = struct.unpack('>H', payload[6:8])[0]
    modbus_counter = struct.unpack('>H', payload[8:10])[0]

    # Apply scaling
    sf6_density = sf6_density_raw / 100.0           # kg/m³
    sf6_pressure_20c = sf6_pressure_20c_raw / 10.0  # kPa
    sf6_temperature_k = sf6_temperature_raw / 10.0  # K
    sf6_temperature_c = sf6_temperature_k - 273.15  # °C
    sf6_pressure_var = sf6_pressure_var_raw / 10.0  # kPa

    return {
        "sf6_density": round(sf6_density, 2),
        "sf6_density_unit": "kg/m³",
        "sf6_pressure_20c": round(sf6_pressure_20c, 1),
        "sf6_pressure_20c_unit": "kPa",
        "sf6_temperature_k": round(sf6_temperature_k, 1),
        "sf6_temperature_k_unit": "K",
        "sf6_temperature_c": round(sf6_temperature_c, 2),
        "sf6_temperature_c_unit": "°C",
        "sf6_pressure_var": round(sf6_pressure_var, 1),
        "sf6_pressure_var_unit": "kPa",
        "modbus_counter": modbus_counter,
        "raw_values": {
            "sf6_density_raw": sf6_density_raw,
            "sf6_pressure_20c_raw": sf6_pressure_20c_raw,
            "sf6_temperature_raw": sf6_temperature_raw,
            "sf6_pressure_var_raw": sf6_pressure_var_raw,
        }
    }


def print_decoded(decoded):
    """Pretty print decoded payload."""
    if "error" in decoded:
        print(f"❌ Error: {decoded['error']}")
        return

    print("\n" + "="*60)
    print("📡 LoRaWAN Payload Decoder - Vision Master E290")
    print("="*60)
    print(f"\n🌡️  SF₆ Sensor Data:")
    print(f"   Density:           {decoded['sf6_density']} {decoded['sf6_density_unit']}")
    print(f"   Pressure @20°C:    {decoded['sf6_pressure_20c']} {decoded['sf6_pressure_20c_unit']}")
    print(f"   Temperature:       {decoded['sf6_temperature_k']} {decoded['sf6_temperature_k_unit']} ({decoded['sf6_temperature_c']} {decoded['sf6_temperature_c_unit']})")
    print(f"   Pressure Variance: {decoded['sf6_pressure_var']} {decoded['sf6_pressure_var_unit']}")
    print(f"\n📊 Modbus Counter:    {decoded['modbus_counter']}")
    print(f"\n🔢 Raw Values:")
    print(f"   Density:           {decoded['raw_values']['sf6_density_raw']} (0x{decoded['raw_values']['sf6_density_raw']:04X})")
    print(f"   Pressure @20°C:    {decoded['raw_values']['sf6_pressure_20c_raw']} (0x{decoded['raw_values']['sf6_pressure_20c_raw']:04X})")
    print(f"   Temperature:       {decoded['raw_values']['sf6_temperature_raw']} (0x{decoded['raw_values']['sf6_temperature_raw']:04X})")
    print(f"   Pressure Variance: {decoded['raw_values']['sf6_pressure_var_raw']} (0x{decoded['raw_values']['sf6_pressure_var_raw']:04X})")
    print("="*60 + "\n")


if __name__ == "__main__":
    # Test with example payload if no arguments
    if len(sys.argv) < 2:
        print("Usage: python3 lorawan_decoder.py <hex_payload>")
        print("\nExample: python3 lorawan_decoder.py 09FA157C0B72157C002A")
        print("\nRunning with example payload...\n")
        hex_payload = "09FA157C0B72157C002A"
    else:
        hex_payload = sys.argv[1]

    decoded = decode_payload(hex_payload)
    print_decoded(decoded)

    # If successful, also print JSON format
    if "error" not in decoded:
        import json
        print("JSON Format (for APIs):")
        print(json.dumps(decoded, indent=2))
