#ifndef DEMO_MANAGER_H
#define DEMO_MANAGER_H

#include <Arduino.h>

struct TelemetryData {
    float x;
    float y;
    float heading;
    float speed;
    String rfid;
    float battery_pct;
    float battery_volt;
    float battery_curr;
    uint16_t tof_left;
    uint16_t tof_centre;
    uint16_t tof_right;
    int16_t left_rpm;
    int16_t right_rpm;
    int16_t target_rpm;
};

void demoManagerInit();
void demoManagerUpdate();

// Telemetry output
TelemetryData getTelemetry();

// Input from client manual control
void handleDemoManualMove(const String &direction, int speedPct);
void handleDemoManualStop();

#endif // DEMO_MANAGER_H
