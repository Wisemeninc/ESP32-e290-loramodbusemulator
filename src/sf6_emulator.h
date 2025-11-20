#ifndef SF6_EMULATOR_H
#define SF6_EMULATOR_H

#include <Arduino.h>
#include <Preferences.h>
#include "modbus_handler.h"

class SF6Emulator {
public:
    SF6Emulator();
    
    // Initialization
    void begin();
    
    // Update simulation
    void update();
    
    // Getters
    float getDensity() const { return base_density; }
    float getPressure() const { return base_pressure; }
    float getTemperature() const { return base_temperature; }
    
    // Setters (manual control)
    void setValues(float density, float pressure, float temperature);
    void resetToDefaults();
    
    // NVS Storage
    void load();
    void save();

private:
    Preferences preferences;
    
    // Base values
    float base_density;       // kg/m3
    float base_pressure;      // kPa
    float base_temperature;   // K
    
    // Mutex for thread safety
    portMUX_TYPE timerMux;
};

// Global instance
extern SF6Emulator sf6Emulator;

#endif // SF6_EMULATOR_H