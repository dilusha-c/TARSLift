#include "routes/route_manager.h"
#include <LittleFS.h>
#include "config.h"
#include "errors/error_logger.h"
#include "rfid/rfid_manager.h"
#include "communication/uart_manager.h"
#include "demo/demo_manager.h"
#include "system/system_manager.h"

static AGVMode currentMode = MODE_IDLE;
static AGVState currentState = STATE_STOPPED;

// Active Train / Teach variables
static String startRfidUID = "";
static String lastDetectedCheckpointUID = "";
static unsigned long lastCheckpointLogTime = 0;

// Active Teach variables
static JsonDocument activeRouteDoc;
static String activeRouteId = "";
static unsigned long teachStartTime = 0;
static float routeDistance = 0.0f;
static float lastX = 0.0f, lastY = 0.0f;
static float lastLoggedX = 0.0f, lastLoggedY = 0.0f, lastLoggedHeading = 0.0f;
static unsigned long lastLoggedTime = 0;

// Active Repeat variables
static JsonDocument runningRouteDoc;
static int currentCheckpointIdx = 0;
static int totalCheckpoints = 0;
static String activeCheckpointName = "";
static unsigned long repeatStartTime = 0;
static float repeatProgress = 0.0f;

void routeManagerInit() {
    if (ENABLE_ROUTE_MANAGER) {
        if (!LittleFS.exists("/routes")) {
            LittleFS.mkdir("/routes");
        }
    }
}

void routeManagerUpdate() {
    if (ENABLE_ROUTE_MANAGER) {
        if (currentMode == MODE_TEACH && currentState == STATE_RECORDING) {
            // Check for intermediate RFID checkpoints while driving
            String liveTag = getCurrentActiveRFID();
            if (liveTag == "NONE" || liveTag.length() == 0) {
                if (millis() - getLastRFIDScanTime() < 1000) {
                    liveTag = getLastScannedRFID();
                }
            }

            if (liveTag != "NONE" && liveTag.length() > 0 && liveTag != lastDetectedCheckpointUID) {
                if (millis() - lastCheckpointLogTime > 2500) {
                    lastDetectedCheckpointUID = liveTag;
                    lastCheckpointLogTime = millis();

                    String cpName = getRFIDTagName(liveTag);
                    String cpLabel = (cpName.length() > 0) ? cpName : liveTag;

                    JsonArray cps = activeRouteDoc["checkpoints"].as<JsonArray>();
                    bool exists = false;
                    for (String cp : cps) {
                        if (cp == cpLabel || cp == liveTag) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        cps.add(cpLabel);
                        Serial.println("RouteManager: Train Mode - Intermediate Checkpoint logged: " + cpLabel);
                    }
                }
            }
        }
        else if (currentMode == MODE_REPEAT && currentState == STATE_RUNNING) {
            // Obstacle safety interlock: if obstacle is detected, pause route progression
            if (isObstacleDetected()) {
                static unsigned long lastObsWarn = 0;
                if (millis() - lastObsWarn > 2500) {
                    lastObsWarn = millis();
                    Serial.println("RouteManager: Obstacle detected in path! AGV paused. Waiting for clear path...");
                }
                sendStm32Stop();
                repeatStartTime += 20; // Maintain elapsed timer while halted (50Hz cycle)
                return;
            }

            // Handle Repeat Mode Simulation
            if (DEMO_MODE) {
                static unsigned long lastTick = 0;
                if (millis() - lastTick > 1000) {
                    lastTick = millis();
                    
                    // Fetch intermediate checkpoints
                    JsonArray cps = runningRouteDoc["checkpoints"].as<JsonArray>();
                    totalCheckpoints = cps.size();
                    
                    if (totalCheckpoints > 0) {
                        // Update index periodically to simulate progression
                        int elapsedSecs = (millis() - repeatStartTime) / 1000;
                        int step = elapsedSecs / 10; // 10 seconds per checkpoint segment
                        
                        if (step < totalCheckpoints) {
                            currentCheckpointIdx = step;
                            activeCheckpointName = cps[step].as<String>();
                            repeatProgress = ((float)step / (totalCheckpoints - 1)) * 100.0f;
                        } else {
                            // Reached end of the route
                            currentCheckpointIdx = totalCheckpoints - 1;
                            activeCheckpointName = cps[totalCheckpoints - 1].as<String>();
                            repeatProgress = 100.0f;
                            currentState = STATE_COMPLETE;
                            currentMode = MODE_IDLE;
                            Serial.println("RouteManager (Demo): Mission complete!");
                        }
                    }
                }
            } else {
                // Real repeat trajectory logic using encoders/MPU/RFID/UART would go here
                
                // MISSION FAILURE LOGIC: TARGET RFID NOT REACHED
                // Assuming we track distance since last checkpoint (distSinceLastCheck)
                // and the expected distance to the next checkpoint (expectedDist)
                /*
                float distSinceLastCheck = 0.0f; // Calculate from odometry
                float expectedDist = 0.0f; // Fetch from runningRouteDoc
                float thresholdMargin = 200.0f; // Allow 200mm overshoot
                
                if (distSinceLastCheck > (expectedDist + thresholdMargin)) {
                    Serial.println("RouteManager: Mission Failure! Target RFID not reached within expected distance.");
                    logError("RFID", "E14", "Mission Failed: Checkpoint Missed");
                    
                    // Stop motors immediately
                    // agv_send_manual_move("STOP", 0);
                    
                    currentState = STATE_ERROR;
                    currentMode = MODE_IDLE;
                }
                */
            }
        }
    }
}

JsonDocument listRoutes() {
    JsonDocument doc;
    JsonArray routes = doc.to<JsonArray>();

    if (ENABLE_ROUTE_MANAGER) {
        File dir = LittleFS.open("/routes");
        if (!dir || !dir.isDirectory()) {
            return doc;
        }

        File file = dir.openNextFile();
        while (file) {
            String filename = file.name();
            if (filename.endsWith(".json")) {
                JsonDocument fileDoc;
                DeserializationError error = deserializeJson(fileDoc, file);
                if (!error) {
                    JsonObject r = routes.add<JsonObject>();
                    r["id"] = fileDoc["id"];
                    r["name"] = fileDoc["name"];
                    r["start"] = fileDoc["start"];
                    r["destination"] = fileDoc["destination"];
                    r["distance"] = fileDoc["distance"];
                    r["duration"] = fileDoc["duration"];
                    r["created"] = fileDoc["created"];
                }
            }
            file = dir.openNextFile();
        }
    }
    return doc;
}

bool loadRoute(const String &routeId, JsonDocument &routeDoc) {
    if (ENABLE_ROUTE_MANAGER) {
        String path = "/routes/" + routeId + ".json";
        if (!LittleFS.exists(path)) {
            return false;
        }

        File file = LittleFS.open(path, FILE_READ);
        if (!file) {
            return false;
        }

        DeserializationError error = deserializeJson(routeDoc, file);
        file.close();

        if (error) {
            Serial.print("RouteManager: Route file corrupted: ");
            Serial.println(error.c_str());
            logError("SYSTEM", "E10", "Route File Corrupted: " + routeId);
            return false;
        }
        return true;
    } else {
        return false;
    }
}

bool saveRoute(const String &routeId, const JsonDocument &routeDoc) {
    if (ENABLE_ROUTE_MANAGER) {
        String path = "/routes/" + routeId + ".json";
        File file = LittleFS.open(path, FILE_WRITE);
        if (!file) {
            return false;
        }

        if (serializeJson(routeDoc, file) == 0) {
            file.close();
            return false;
        }

        file.close();
        return true;
    } else {
        return false;
    }
}

bool deleteRoute(const String &routeId) {
    if (ENABLE_ROUTE_MANAGER) {
        String path = "/routes/" + routeId + ".json";
        if (LittleFS.exists(path)) {
            return LittleFS.remove(path);
        }
    }
    return false;
}

bool startTeaching(const String &name, const String &start, const String &destination, const String &description, String &outError) {
    if (!ENABLE_TEACH_MODE) {
        outError = "Teach/Train mode is disabled in system settings.";
        return false;
    }

    if (currentMode == MODE_TEACH && currentState == STATE_RECORDING) {
        outError = "Train mode is already in progress.";
        return false;
    }

    // 1. Detect Starting RFID Tag
    // Requirement: It must start with RFID.
    String detectedStartUid = getCurrentActiveRFID();
    if (detectedStartUid == "NONE" || detectedStartUid.length() == 0) {
        // Check if a tag was scanned within the last 5 seconds
        if (getLastScannedRFID() != "NONE" && getLastScannedRFID().length() > 0 && (millis() - getLastRFIDScanTime() < 5000)) {
            detectedStartUid = getLastScannedRFID();
        }
    }

    // Check if user explicitly provided a recognized tag UID or name
    if ((detectedStartUid == "NONE" || detectedStartUid.length() == 0) && start.length() > 0 && start != "START") {
        detectedStartUid = start;
    }

    if (detectedStartUid == "NONE" || detectedStartUid.length() == 0) {
        if (!DEMO_MODE) {
            outError = "Cannot start Train Mode: No starting RFID tag detected under AGV! Place AGV over an RFID tag to begin.";
            Serial.println("RouteManager: " + outError);
            return false;
        } else {
            detectedStartUid = "DEMO_START_TAG";
        }
    }

    startRfidUID = detectedStartUid;
    lastDetectedCheckpointUID = detectedStartUid;
    lastCheckpointLogTime = millis();

    String tagName = getRFIDTagName(detectedStartUid);
    String startLabel = (tagName.length() > 0) ? tagName : detectedStartUid;
    if (start.length() > 0 && start != "START") {
        startLabel = start;
    }

    currentMode = MODE_TEACH;
    currentState = STATE_RECORDING;
    teachStartTime = millis();
    routeDistance = 0.0f;
    lastX = 0.0f;
    lastY = 0.0f;
    lastLoggedX = 0.0f;
    lastLoggedY = 0.0f;
    lastLoggedHeading = getTelemetry().heading;
    lastLoggedTime = millis();
    resetTelemetryPosition();

    // Generate unique ID based on millisecond timestamp
    activeRouteId = "route_" + String(millis());

    activeRouteDoc.clear();
    activeRouteDoc["id"] = activeRouteId;
    activeRouteDoc["name"] = name;
    activeRouteDoc["start"] = startLabel;
    activeRouteDoc["start_rfid"] = detectedStartUid;
    activeRouteDoc["destination"] = (destination.length() > 0 && destination != "OFFICE") ? destination : "PENDING_RFID";
    activeRouteDoc["destination_rfid"] = "";
    activeRouteDoc["description"] = description;
    activeRouteDoc["distance"] = 0.0;
    activeRouteDoc["duration"] = 0;
    activeRouteDoc["created"] = "2026-08-13";
    activeRouteDoc.createNestedArray("checkpoints");
    activeRouteDoc.createNestedArray("trajectory");

    // Add start checkpoint
    activeRouteDoc["checkpoints"].add(startLabel);
    
    // Add start trajectory origin point
    addTrajectoryPoint(0.0f, 0.0f, lastLoggedHeading, detectedStartUid);

    sendStm32TeachStart();
    Serial.println("RouteManager: Started training route '" + name + "' at RFID: " + detectedStartUid + " (" + startLabel + ")");
    return true;
}

bool startTeaching(const String &name, const String &start, const String &destination, const String &description) {
    String dummy;
    return startTeaching(name, start, destination, description, dummy);
}

bool stopRecordingAndSave(String &outError) {
    if (!ENABLE_TEACH_MODE) {
        outError = "Teach/Train mode is disabled in system settings.";
        return false;
    }

    if (currentMode != MODE_TEACH || currentState != STATE_RECORDING) {
        outError = "Train Mode is not currently active or recording.";
        return false;
    }

    // 2. Detect Destination RFID Tag
    // Requirement: It must end with RFID. If not, DO NOT allow to end and show message to user!
    String endTagUid = getCurrentActiveRFID();
    if (endTagUid == "NONE" || endTagUid.length() == 0) {
        // Check if a card was scanned recently at the stopping position (within 10 seconds)
        if (getLastScannedRFID() != "NONE" && getLastScannedRFID().length() > 0 && (millis() - getLastRFIDScanTime() < 10000)) {
            endTagUid = getLastScannedRFID();
        }
    }

    if (endTagUid == "NONE" || endTagUid.length() == 0) {
        if (!DEMO_MODE) {
            outError = "Cannot end Train Mode: Destination RFID tag not detected! Move the AGV over an RFID tag to finish.";
            Serial.println("RouteManager: " + outError);
            return false;
        } else {
            endTagUid = "DEMO_DEST_TAG";
        }
    }

    // Guard against immediate stop while still sitting on the starting tag without driving
    if (!DEMO_MODE && endTagUid == startRfidUID && (getLastRFIDScanTime() <= teachStartTime + 2000)) {
        outError = "Cannot end Train Mode: AGV is still at the starting tag! Drive to the destination RFID tag before ending.";
        Serial.println("RouteManager: " + outError);
        return false;
    }

    // Destination RFID tag is verified!
    String endTagName = getRFIDTagName(endTagUid);
    String destLabel = (endTagName.length() > 0) ? endTagName : endTagUid;

    // Log the final destination point into trajectory
    TelemetryData finalTele = getTelemetry();
    addTrajectoryPoint(finalTele.x, finalTele.y, finalTele.heading, endTagUid);

    activeRouteDoc["destination"] = destLabel;
    activeRouteDoc["destination_rfid"] = endTagUid;
    activeRouteDoc["distance"] = round(routeDistance * 100.0) / 100.0;
    activeRouteDoc["duration"] = (millis() - teachStartTime) / 1000;

    // Add destination to checkpoints if not already there
    JsonArray cps = activeRouteDoc["checkpoints"].as<JsonArray>();
    bool hasDest = false;
    for (String cp : cps) {
        if (cp == destLabel || cp == endTagUid) {
            hasDest = true;
            break;
        }
    }
    if (!hasDest) {
        cps.add(destLabel);
    }

    bool success = saveRoute(activeRouteId, activeRouteDoc);
    if (success) {
        currentState = STATE_STOPPED;
        currentMode = MODE_IDLE;
        sendStm32TeachStop();
        sendStm32Stop();
        outError = "";
        Serial.println("RouteManager: Train Mode completed! Saved route: " + activeRouteId + " (From " + activeRouteDoc["start"].as<String>() + " To " + destLabel + ")");
    } else {
        currentState = STATE_ERROR;
        outError = "Failed to write route file to filesystem.";
        logError("SYSTEM", "E11", "Failed to save route " + activeRouteId);
    }
    return success;
}

bool stopRecordingAndSave() {
    String dummy;
    return stopRecordingAndSave(dummy);
}

void cancelRecording() {
    currentMode = MODE_IDLE;
    currentState = STATE_STOPPED;
    activeRouteId = "";
    activeRouteDoc.clear();
    startRfidUID = "";
    lastDetectedCheckpointUID = "";
    sendStm32TeachStop();
    sendStm32Stop();
    Serial.println("RouteManager: Train Mode cancelled.");
}

void addTrajectoryPoint(float x, float y, float heading, const String &rfidTag) {
    if (ENABLE_TEACH_MODE) {
        if (currentMode != MODE_TEACH || currentState != STATE_RECORDING) {
            return;
        }

        JsonArray traj = activeRouteDoc["trajectory"].as<JsonArray>();
        JsonObject pt = traj.add<JsonObject>();
        pt["x"] = round(x * 100.0) / 100.0;
        pt["y"] = round(y * 100.0) / 100.0;
        pt["heading"] = round(heading * 10.0) / 10.0;
        
        if (rfidTag != "" && rfidTag != "NONE") {
            pt["rfid"] = rfidTag;
            
            // Add to checkpoints array if unique
            JsonArray cps = activeRouteDoc["checkpoints"].as<JsonArray>();
            bool exists = false;
            for (String cp : cps) {
                if (cp == rfidTag) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                cps.add(rfidTag);
            }
        }

        // Accumulate total distance
        if (traj.size() > 1) {
            float dx = x - lastX;
            float dy = y - lastY;
            routeDistance += sqrt(dx * dx + dy * dy);
        }

        lastX = x;
        lastY = y;
    }
}

bool startRepeating(const String &routeId) {
    if (ENABLE_REPEAT_MODE) {
        // Check if safety conditions met
        if (!DEMO_MODE) {
            // If not demo mode, we require actual motor, encoder, and mpu to function
            if (MOTOR_STATE == STATE_DISABLED || ENCODER_STATE == STATE_DISABLED || MPU6050_STATE == STATE_DISABLED) {
                Serial.println("RouteManager: Cannot start repeat: Required hardware disabled!");
                logError("SYSTEM", "E12", "Repeat hardware requirements not met");
                return false;
            }
        }

        runningRouteDoc.clear();
        if (!loadRoute(routeId, runningRouteDoc)) {
            currentState = STATE_ERROR;
            return false;
        }

        currentMode = MODE_REPEAT;
        currentState = STATE_RUNNING;
        currentCheckpointIdx = 0;
        repeatProgress = 0.0f;
        repeatStartTime = millis();
        
        JsonArray cps = runningRouteDoc["checkpoints"].as<JsonArray>();
        totalCheckpoints = cps.size();
        if (totalCheckpoints > 0) {
            activeCheckpointName = cps[0].as<String>();
        } else {
            activeCheckpointName = "START";
        }

        Serial.println("RouteManager: Started repeating route " + routeId);
        return true;
    } else {
        return false;
    }
}

bool startRepeatingShortestPath(const String &startRfid, const String &destRfid) {
    if (!ENABLE_REPEAT_MODE) return false;

    // VERY BASIC SHORTEST PATH: For now, find an existing route that starts with startRfid and ends with destRfid
    // In the future, this should construct a topological graph and run Dijkstra
    
    JsonDocument list = listRoutes();
    JsonArray routes = list.as<JsonArray>();
    
    String foundRouteId = "";

    for (JsonObject r : routes) {
        String routeStart = r["start"].as<String>();
        String routeDest = r["destination"].as<String>();

        if (routeStart == startRfid && routeDest == destRfid) {
            foundRouteId = r["id"].as<String>();
            break;
        }
    }

    if (foundRouteId != "") {
        Serial.println("RouteManager: Found shortest path route: " + foundRouteId);
        return startRepeating(foundRouteId);
    } else {
        Serial.println("RouteManager: No path found between " + startRfid + " and " + destRfid);
        logError("SYSTEM", "E14", "No topological path found");
        return false;
    }
}

bool pauseRepeating() {
    if (currentMode == MODE_REPEAT && currentState == STATE_RUNNING) {
        currentState = STATE_PAUSED;
        return true;
    }
    return false;
}

bool resumeRepeating() {
    if (currentMode == MODE_REPEAT && currentState == STATE_PAUSED) {
        currentState = STATE_RUNNING;
        return true;
    }
    return false;
}

bool stopRepeating() {
    if (currentMode == MODE_REPEAT) {
        currentState = STATE_STOPPED;
        currentMode = MODE_IDLE;
        return true;
    }
    return false;
}

AGVMode getAGVMode() { return currentMode; }
AGVState getAGVState() { return currentState; }

String getActiveRouteName() {
    if (currentMode == MODE_TEACH) return activeRouteDoc["name"].as<String>();
    if (currentMode == MODE_REPEAT) return runningRouteDoc["name"].as<String>();
    return "";
}

String getCurrentCheckpoint() {
    return activeCheckpointName;
}

int getCheckpointIndex() {
    return currentCheckpointIdx;
}

int getTotalCheckpoints() {
    return totalCheckpoints;
}

float getMissionProgress() {
    if (currentMode == MODE_TEACH) return 0.0f;
    return repeatProgress;
}

float getTeachDistance() {
    if (currentMode == MODE_TEACH) return routeDistance;
    return 0.0f;
}

uint32_t getTeachDurationS() {
    if (currentMode == MODE_TEACH && currentState == STATE_RECORDING) {
        return (millis() - teachStartTime) / 1000;
    }
    return 0;
}

void processTeachOdometry(float deltaDist_m, float yawDeg) {
    if (currentMode != MODE_TEACH || currentState != STATE_RECORDING) return;

    TelemetryData tele = getTelemetry();
    unsigned long now = millis();

    float dHeading = fabs(tele.heading - lastLoggedHeading);
    if (dHeading > 180.0f) dHeading = 360.0f - dHeading;

    float dx = tele.x - lastLoggedX;
    float dy = tele.y - lastLoggedY;
    float distSinceLog = sqrt(dx * dx + dy * dy);

    String curRfid = getCurrentActiveRFID();
    if (curRfid.length() == 0 || curRfid == "NONE") {
        if (now - getLastRFIDScanTime() < 1000) {
            curRfid = getLastScannedRFID();
        }
    }

    bool rfidTrigger = (curRfid.length() > 0 && curRfid != "NONE" && curRfid != lastDetectedCheckpointUID);
    bool moveTrigger = (distSinceLog >= 0.05f); // Moved >= 5 cm
    bool turnTrigger = (dHeading >= 4.0f);      // Turned >= 4 degrees
    bool timeTrigger = (distSinceLog >= 0.01f && (now - lastLoggedTime >= 400)); // Slow drive

    if (moveTrigger || turnTrigger || timeTrigger || rfidTrigger) {
        addTrajectoryPoint(tele.x, tele.y, tele.heading, curRfid);
        lastLoggedX = tele.x;
        lastLoggedY = tele.y;
        lastLoggedHeading = tele.heading;
        lastLoggedTime = now;
    }
}

