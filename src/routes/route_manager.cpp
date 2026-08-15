#include "routes/route_manager.h"
#include <LittleFS.h>
#include "config.h"
#include "errors/error_logger.h"

static AGVMode currentMode = MODE_IDLE;
static AGVState currentState = STATE_STOPPED;

// Active Teach variables
static JsonDocument activeRouteDoc;
static String activeRouteId = "";
static unsigned long teachStartTime = 0;
static float routeDistance = 0.0f;
static float lastX = 0.0f, lastY = 0.0f;

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
        if (currentMode == MODE_REPEAT && currentState == STATE_RUNNING) {
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

bool startTeaching(const String &name, const String &start, const String &destination, const String &description) {
    if (ENABLE_TEACH_MODE) {
        currentMode = MODE_TEACH;
        currentState = STATE_RECORDING;
        teachStartTime = millis();
        routeDistance = 0.0f;
        lastX = 0.0f;
        lastY = 0.0f;

        // Generate unique ID based on millisecond timestamp
        activeRouteId = "route_" + String(millis());

        activeRouteDoc.clear();
        activeRouteDoc["id"] = activeRouteId;
        activeRouteDoc["name"] = name;
        activeRouteDoc["start"] = start;
        activeRouteDoc["destination"] = destination;
        activeRouteDoc["description"] = description;
        activeRouteDoc["distance"] = 0.0;
        activeRouteDoc["duration"] = 0;
        activeRouteDoc["created"] = "2026-08-13";
        activeRouteDoc.createNestedArray("checkpoints");
        activeRouteDoc.createNestedArray("trajectory");

        // Add start checkpoint
        activeRouteDoc["checkpoints"].add(start);
        
        Serial.println("RouteManager: Started teaching route " + name);
        return true;
    } else {
        return false;
    }
}

bool stopRecordingAndSave() {
    if (ENABLE_TEACH_MODE) {
        if (currentMode != MODE_TEACH || currentState != STATE_RECORDING) {
            return false;
        }

        activeRouteDoc["distance"] = round(routeDistance * 100.0) / 100.0;
        activeRouteDoc["duration"] = (millis() - teachStartTime) / 1000;
        
        // Add destination to checkpoints if not already there
        JsonArray cps = activeRouteDoc["checkpoints"].as<JsonArray>();
        String dest = activeRouteDoc["destination"].as<String>();
        bool hasDest = false;
        for (String cp : cps) {
            if (cp == dest) {
                hasDest = true;
                break;
            }
        }
        if (!hasDest) {
            cps.add(dest);
        }

        bool success = saveRoute(activeRouteId, activeRouteDoc);
        if (success) {
            currentState = STATE_STOPPED;
            currentMode = MODE_IDLE;
            Serial.println("RouteManager: Saved taught route: " + activeRouteId);
        } else {
            currentState = STATE_ERROR;
            logError("SYSTEM", "E11", "Failed to save route " + activeRouteId);
        }
        return success;
    } else {
        return false;
    }
}

void cancelRecording() {
    currentMode = MODE_IDLE;
    currentState = STATE_STOPPED;
    activeRouteId = "";
    activeRouteDoc.clear();
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
