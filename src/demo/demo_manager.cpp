#include "demo/demo_manager.h"
#include "config.h"
#include "routes/route_manager.h"
#include "system/system_manager.h"

static TelemetryData tele;
static String activeDirection = "";
static int driveSpeedPercentage = 50;
static unsigned long lastUpdate = 0;

static float demoTargetL = 0.0f;
static float demoTargetR = 0.0f;
static float simActualL = 0.0f;
static float simActualR = 0.0f;

void setDemoTargetRpm(float targetL, float targetR) {
    demoTargetL = targetL;
    demoTargetR = targetR;
    tele.target_l_rpm = (int16_t)targetL;
    tele.target_r_rpm = (int16_t)targetR;
    tele.target_rpm = (int16_t)((targetL + targetR) / 2.0f);
}

// Demo coordinates for RFID checkpoints
struct DemoCheckpoint {
    float x;
    float y;
    String tag;
};

static DemoCheckpoint checkpoints[] = {
    {0.0f, 0.0f, "START"},
    {2.0f, 0.0f, "LAB"},
    {2.0f, 2.0f, "STORAGE"},
    {0.0f, 2.0f, "OFFICE"}
};
static const int numCheckpoints = 4;

void demoManagerInit() {
    tele.x = 0.0f;
    tele.y = 0.0f;
    tele.heading = 0.0f;
    tele.speed = 0.0f;
    tele.rfid = "NONE";
    tele.battery_pct = 78.0f;
    tele.battery_volt = 11.6f;
    tele.battery_curr = 0.45f;
    tele.tof_left = 1500;
    tele.tof_centre = 1800;
    tele.tof_right = 1400;
    tele.left_rpm = 0;
    tele.right_rpm = 0;
    tele.target_rpm = 0;
    tele.target_l_rpm = 0;
    tele.target_r_rpm = 0;
    demoTargetL = 0.0f;
    demoTargetR = 0.0f;
    simActualL = 0.0f;
    simActualR = 0.0f;
    
    lastUpdate = millis();
}

void demoManagerUpdate() {
    if (DEMO_MODE) {
        unsigned long now = millis();
        float dt = (now - lastUpdate) / 1000.0f;
        lastUpdate = now;

        if (dt <= 0.0f || dt > 1.0f) return;

        // Simulate battery drainage/fluctuation
        tele.battery_pct -= 0.001f * dt;
        if (tele.battery_pct < 0.0f) tele.battery_pct = 100.0f;
        tele.battery_volt = 9.0f + (tele.battery_pct / 100.0f) * 3.6f;
        
        // Simulate ToF sensors fluctuating slightly
        tele.tof_left = constrain(tele.tof_left + random(-20, 21), 400, 2000);
        tele.tof_centre = constrain(tele.tof_centre + random(-25, 26), 400, 2000);
        tele.tof_right = constrain(tele.tof_right + random(-20, 21), 400, 2000);

        // Apply motion if a manual direction or repeat trajectory is active
        if (getAGVMode() == MODE_TEACH || getAGVMode() == MODE_IDLE) {
            // Target RPM to m/s
            float speedVal = (driveSpeedPercentage * sysSettings.wheel_circ_mm) / 60000.0f;

            if (activeDirection == "FORWARD") {
                if (isObstacleDetected()) {
                    tele.speed = 0.0f;
                    tele.left_rpm = 0;
                    tele.right_rpm = 0;
                    tele.target_rpm = 0;
                } else {
                    tele.speed = speedVal;
                    tele.left_rpm = driveSpeedPercentage;
                    tele.right_rpm = driveSpeedPercentage;
                    tele.target_rpm = driveSpeedPercentage;
                    
                    // Calculate components based on heading (degrees to radians)
                    float rad = tele.heading * DEG_TO_RAD;
                    tele.x += tele.speed * cos(rad) * dt;
                    tele.y += tele.speed * sin(rad) * dt;

                    // Report points during teach mode
                    if (getAGVMode() == MODE_TEACH) {
                        addTrajectoryPoint(tele.x, tele.y, tele.heading, tele.rfid);
                    }
                }
            } 
            else if (activeDirection == "REVERSE") {
                tele.speed = -speedVal;
                tele.left_rpm = -driveSpeedPercentage;
                tele.right_rpm = -driveSpeedPercentage;
                tele.target_rpm = -driveSpeedPercentage;
                
                float rad = tele.heading * DEG_TO_RAD;
                tele.x += tele.speed * cos(rad) * dt;
                tele.y += tele.speed * sin(rad) * dt;

                if (getAGVMode() == MODE_TEACH) {
                    addTrajectoryPoint(tele.x, tele.y, tele.heading, tele.rfid);
                }
            } 
            else if (activeDirection == "LEFT") {
                tele.speed = 0.0f;
                tele.left_rpm = -driveSpeedPercentage;
                tele.right_rpm = driveSpeedPercentage;
                tele.target_rpm = driveSpeedPercentage;
                tele.heading -= (driveSpeedPercentage / 1.5f) * dt; // Turn at speedDegS
            } 
            else if (activeDirection == "RIGHT") {
                tele.speed = 0.0f;
                tele.left_rpm = driveSpeedPercentage;
                tele.right_rpm = -driveSpeedPercentage;
                tele.target_rpm = driveSpeedPercentage;
                tele.heading += (driveSpeedPercentage / 1.5f) * dt;
            } 
            else {
                // Not moving via directional control - check if PID tuning targets are set
                if (demoTargetL != 0.0f || demoTargetR != 0.0f || fabs(simActualL) > 0.1f || fabs(simActualR) > 0.1f) {
                    float alpha = 0.20f;
                    simActualL += (demoTargetL - simActualL) * alpha;
                    simActualR += (demoTargetR - simActualR) * alpha;
                    
                    float jitterL = (fabs(simActualL) > 1.0f) ? (random(-15, 16) / 10.0f) : 0.0f;
                    float jitterR = (fabs(simActualR) > 1.0f) ? (random(-15, 16) / 10.0f) : 0.0f;

                    tele.speed = ((simActualL + simActualR) / 2.0f / 60.0f) * (sysSettings.wheel_circ_mm / 1000.0f);
                    tele.left_rpm = (int16_t)(simActualL + jitterL);
                    tele.right_rpm = (int16_t)(simActualR + jitterR);
                    tele.left_mms = (int16_t)((tele.left_rpm / 60.0f) * sysSettings.wheel_circ_mm);
                    tele.right_mms = (int16_t)((tele.right_rpm / 60.0f) * sysSettings.wheel_circ_mm);
                    tele.target_l_rpm = (int16_t)demoTargetL;
                    tele.target_r_rpm = (int16_t)demoTargetR;
                    tele.target_rpm = (int16_t)((demoTargetL + demoTargetR) / 2.0f);
                } else {
                    // STOP
                    tele.speed = 0.0f;
                    tele.left_rpm = 0;
                    tele.right_rpm = 0;
                    tele.target_rpm = 0;
                    tele.target_l_rpm = 0;
                    tele.target_r_rpm = 0;
                }
            }
        } 
        else if (getAGVMode() == MODE_REPEAT && getAGVState() == STATE_RUNNING) {
            if (isObstacleDetected()) {
                tele.speed = 0.0f;
                tele.left_rpm = 0;
                tele.right_rpm = 0;
                tele.target_rpm = 0;
            } else {
                // Telemetry follows Repeat Mission simulation in RouteManager
                tele.speed = 0.32f;
                tele.target_rpm = 60;
                tele.left_rpm = 60;
                tele.right_rpm = 60;
                
                // Simulating coordinate movement along a path loop
                static float repeatAngle = 0.0f;
                repeatAngle += 10.0f * dt;
                tele.heading = repeatAngle;
                
                float rad = tele.heading * DEG_TO_RAD;
                tele.x = 1.0f + cos(rad);
                tele.y = 1.0f + sin(rad);
                
                // Update rfid tag based on progress
                tele.rfid = getCurrentCheckpoint();
            }
        }

        // Keep heading normalized between 0-360 degrees
        if (tele.heading < 0.0f) tele.heading += 360.0f;
        if (tele.heading >= 360.0f) tele.heading -= 360.0f;

        // Checkpoint proximity matching
        if (activeDirection != "" || getAGVMode() == MODE_REPEAT) {
            String closestTag = "NONE";
            float minDistance = 0.3f; // Detect RFID tags within 30cm radius
            for (int i = 0; i < numCheckpoints; i++) {
                float dx = tele.x - checkpoints[i].x;
                float dy = tele.y - checkpoints[i].y;
                float dist = sqrt(dx * dx + dy * dy);
                if (dist < minDistance) {
                    closestTag = checkpoints[i].tag;
                    break;
                }
            }
            tele.rfid = closestTag;
        }
    }
}

TelemetryData getTelemetry() {
    return tele;
}

void handleDemoManualMove(const String &direction, int speedPct) {
    if (DEMO_MODE) {
        activeDirection = direction;
        driveSpeedPercentage = speedPct;
        Serial.println("DemoManager: Manual direction changed to: " + direction + " @ " + String(speedPct) + "%");
    }
}

void handleDemoManualStop() {
    if (DEMO_MODE) {
        activeDirection = "";
        demoTargetL = 0.0f;
        demoTargetR = 0.0f;
        simActualL = 0.0f;
        simActualR = 0.0f;
        tele.target_rpm = 0;
        tele.target_l_rpm = 0;
        tele.target_r_rpm = 0;
        Serial.println("DemoManager: Manual Stop received.");
    }
}

void updateTelemetryOdometry(float distanceMeters, float yawDeg) {
    // Keep heading updated, do not overwrite speed with distance!
    tele.heading = yawDeg;
    while (tele.heading < 0.0f) tele.heading += 360.0f;
    while (tele.heading >= 360.0f) tele.heading -= 360.0f;
}

void updateTelemetryHeading(float yawDeg) {
    tele.heading = yawDeg;
    while (tele.heading < 0.0f) tele.heading += 360.0f;
    while (tele.heading >= 360.0f) tele.heading -= 360.0f;
}

void updateTelemetryMotors(int16_t leftRpm, int16_t rightRpm, int16_t leftMms, int16_t rightMms) {
    tele.left_rpm = leftRpm;
    tele.right_rpm = rightRpm;
    tele.left_mms = leftMms;
    tele.right_mms = rightMms;
    // Calculate real linear speed in m/s
    tele.speed = ((leftMms + rightMms) / 2.0f) / 1000.0f;
}

void resetTelemetryPosition() {
    tele.x = 0.0f;
    tele.y = 0.0f;
}

void updateTelemetryPositionDelta(float deltaMeters, float yawDeg) {
    tele.heading = yawDeg;
    while (tele.heading < 0.0f) tele.heading += 360.0f;
    while (tele.heading >= 360.0f) tele.heading -= 360.0f;

    float rad = tele.heading * DEG_TO_RAD;
    tele.x += deltaMeters * cos(rad);
    tele.y += deltaMeters * sin(rad);
}

void updateTelemetryRawEncoders(int32_t left, int32_t right) {
    tele.raw_enc_l = left;
    tele.raw_enc_r = right;
}
void updateTelemetryTof(uint16_t leftMm, uint16_t centerMm, uint16_t rightMm) {
    tele.tof_left = leftMm;
    tele.tof_centre = centerMm;
    tele.tof_right = rightMm;
}

void updateTelemetryRFID(const String &uid) {
    tele.rfid = uid;
}

void updateTelemetryBattery(float volt, float curr, float pct) {
    tele.battery_volt = volt;
    tele.battery_curr = curr;
    tele.battery_pct = pct;
}
