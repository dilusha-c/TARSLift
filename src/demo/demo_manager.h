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
    int16_t left_mms;
    int16_t right_mms;
    int16_t target_rpm;
    int32_t raw_enc_l;
    int32_t raw_enc_r;
};

void demoManagerInit();
void demoManagerUpdate();

// Telemetry output
TelemetryData getTelemetry();

// Real-time Telemetry updates from STM32 UART packets
void updateTelemetryOdometry(float distanceMeters, float yawDeg);
void updateTelemetryHeading(float yawDeg);
void updateTelemetryMotors(int16_t leftRpm, int16_t rightRpm, int16_t leftMms, int16_t rightMms);
void updateTelemetryRawEncoders(int32_t left, int32_t right);
void updateTelemetryTof(uint16_t leftMm, uint16_t centerMm, uint16_t rightMm);
void updateTelemetryRFID(const String &uid);
void updateTelemetryBattery(float volt, float curr, float pct);
void resetTelemetryPosition();
void updateTelemetryPositionDelta(float deltaMeters, float yawDeg);

// Input from client manual control
void handleDemoManualMove(const String &direction, int speedPct);
void handleDemoManualStop();

#endif // DEMO_MANAGER_H
