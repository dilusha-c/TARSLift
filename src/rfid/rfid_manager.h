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
String getRFIDTagName(const String &uid);

// Live scanning
void startRFIDScan();
void simulateRFIDScan();
void testRFIDScan();
bool isScanning();
bool getLatestScan(String &uid);
void clearLatestScan();
String getLastScannedRFID();
String getCurrentActiveRFID();
unsigned long getLastRFIDScanTime();
bool isRFIDReaderHardwareReady();

#endif // RFID_MANAGER_H

