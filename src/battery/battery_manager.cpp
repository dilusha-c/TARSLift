#include "battery/battery_manager.h"
#include <Adafruit_INA219.h>
#include "demo/demo_manager.h"
#include "config.h"

Adafruit_INA219 ina219;
bool ina219_initialized = false;
unsigned long last_battery_read = 0;

void batteryManagerInit() {
    if (!sysSettings.battery_monitoring) return;
    
    // Wire.begin() is already called by oledInit() before this.
    // Initialize INA219 with default I2C address 0x40
    if (!ina219.begin()) {
        Serial.println("Failed to find INA219 chip");
        ina219_initialized = false;
    } else {
        Serial.println("INA219 Initialized on I2C.");
        // Use default 32V 2A calibration instead of 16V 400mA to avoid overflow returning 0 mA
        // ina219.setCalibration_16V_400mA(); 
        ina219_initialized = true;
    }
}

void batteryManagerUpdate() {
    if (!sysSettings.battery_monitoring || !ina219_initialized) return;

    unsigned long now = millis();
    if (now - last_battery_read >= 1000) { // Read every 1 second
        last_battery_read = now;
        
        float shuntvoltage = ina219.getShuntVoltage_mV();
        float busvoltage = ina219.getBusVoltage_V();
        float current_mA = ina219.getCurrent_mA();
        
        float loadvoltage = busvoltage + (shuntvoltage / 1000);
        
        // Very basic 3S LiPo percentage estimation (8.6V max, 6.0V min)
        float pct = ((loadvoltage - 6.0f) / (8.6f - 6.0f)) * 100.0f;
        if (pct > 100.0f) pct = 100.0f;
        if (pct < 0.0f) pct = 0.0f;
        
        // Update telemetry
        updateTelemetryBattery(loadvoltage, current_mA / 1000.0f, pct);
    }
}

