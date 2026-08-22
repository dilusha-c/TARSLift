#ifndef ROUTE_MANAGER_H
#define ROUTE_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>

struct RouteMetadata {
    String id;
    String name;
    String start;
    String destination;
    float distance;
    uint32_t duration;
    String created;
};

enum AGVState {
    STATE_STOPPED,
    STATE_RUNNING,
    STATE_PAUSED,
    STATE_RECORDING,
    STATE_COMPLETE,
    STATE_ERROR
};

enum AGVMode {
    MODE_IDLE,
    MODE_TEACH,
    MODE_REPEAT
};

void routeManagerInit();
void routeManagerUpdate();

// Route files CRUD
JsonDocument listRoutes();
bool loadRoute(const String &routeId, JsonDocument &routeDoc);
bool saveRoute(const String &routeId, const JsonDocument &routeDoc);
bool deleteRoute(const String &routeId);

// Teach Mode Control
bool startTeaching(const String &name, const String &start, const String &destination, const String &description);
bool stopRecordingAndSave();
void cancelRecording();
void addTrajectoryPoint(float x, float y, float heading, const String &rfidTag);

// Repeat Mode Control
bool startRepeating(const String &routeId);
bool startRepeatingShortestPath(const String &startRfid, const String &destRfid);
bool pauseRepeating();
bool resumeRepeating();
bool stopRepeating();

// Status Getters
AGVMode getAGVMode();
AGVState getAGVState();
String getActiveRouteName();
String getCurrentCheckpoint();
int getCheckpointIndex();
int getTotalCheckpoints();
float getMissionProgress();

#endif // ROUTE_MANAGER_H
