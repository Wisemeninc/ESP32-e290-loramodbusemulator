#include "sf6_emulator.h"

// Global instance
SF6Emulator sf6Emulator;

SF6Emulator::SF6Emulator() :
    base_density(25.0),
    base_pressure(550.0),
    base_temperature(293.0),
    timerMux(portMUX_INITIALIZER_UNLOCKED) {
}

void SF6Emulator::begin() {
    load();
    
    // Initial update to set registers
    modbusHandler.updateInputRegisters(base_density, base_pressure, base_temperature);
}

void SF6Emulator::update() {
    portENTER_CRITICAL(&timerMux);

    // Add small random drift
    base_density += random(-10, 11) / 100.0;
    base_pressure += random(-50, 51) / 10.0;
    base_temperature += random(-5, 6) / 10.0;

    // Constrain to realistic ranges (0-60 kg/m3, 0-1100 kPa, 215-360 K)
    base_density = constrain(base_density, 0.0, 60.0);
    base_pressure = constrain(base_pressure, 0.0, 1100.0);
    base_temperature = constrain(base_temperature, 215.0, 360.0);

    portEXIT_CRITICAL(&timerMux);

    // Update the modbus handler's internal registers
    modbusHandler.updateInputRegisters(base_density, base_pressure, base_temperature);
}

void SF6Emulator::setValues(float density, float pressure, float temperature) {
    portENTER_CRITICAL(&timerMux);
    
    if (density >= 0 && density <= 60.0) base_density = density;
    if (pressure >= 0 && pressure <= 1100.0) base_pressure = pressure;
    if (temperature >= 215.0 && temperature <= 360.0) base_temperature = temperature;
    
    portEXIT_CRITICAL(&timerMux);
    
    // Update registers immediately
    modbusHandler.updateInputRegisters(base_density, base_pressure, base_temperature);
    
    // Save to NVS
    save();
}

void SF6Emulator::resetToDefaults() {
    portENTER_CRITICAL(&timerMux);
    
    base_density = 25.0;       // kg/m3
    base_pressure = 550.0;     // kPa
    base_temperature = 293.0;  // K
    
    portEXIT_CRITICAL(&timerMux);
    
    // Update registers immediately
    modbusHandler.updateInputRegisters(base_density, base_pressure, base_temperature);
    
    // Save to NVS
    save();
}

void SF6Emulator::load() {
    if (!preferences.begin("sf6", false)) {  // Read-write (creates if not exists)
        Serial.println(">>> Failed to open sf6 preferences");
        return;
    }

    bool has_values = preferences.getBool("has_values", false);

    if (has_values) {
        base_density = preferences.getFloat("density", 25.0);
        base_pressure = preferences.getFloat("pressure", 550.0);
        base_temperature = preferences.getFloat("temperature", 293.0);

        Serial.println(">>> SF6 values loaded from NVS");
        Serial.printf("    Density: %.2f kg/m3\n", base_density);
        Serial.printf("    Pressure: %.1f kPa\n", base_pressure);
        Serial.printf("    Temperature: %.1f K\n", base_temperature);
    } else {
        Serial.println(">>> No stored SF6 values, using defaults");
    }

    preferences.end();
}

void SF6Emulator::save() {
    if (!preferences.begin("sf6", false)) {  // Read-write
        Serial.println(">>> Failed to open sf6 preferences for writing");
        return;
    }

    preferences.putFloat("density", base_density);
    preferences.putFloat("pressure", base_pressure);
    preferences.putFloat("temperature", base_temperature);
    preferences.putBool("has_values", true);
    preferences.end();

    Serial.println(">>> SF6 values saved to NVS");
}