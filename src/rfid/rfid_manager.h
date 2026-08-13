#ifndef RFID_MANAGER_H
#define RFID_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>

struct RFIDTag {
    String uid;
    String name;
    String location;
};

void rfidManagerInit();
void rfidManagerUpdate();

// Tag management
bool loadRFIDTags();
bool saveRFIDTags();
JsonDocument getRFIDTagsJSON();
bool addOrUpdateRFIDTag(const String &uid, const String &name, const String &location);
bool deleteRFIDTag(const String &uid);

// Live scanning
void startRFIDScan();
bool isScanning();
bool getLatestScan(String &uid);
void clearLatestScan();

#endif // RFID_MANAGER_H
