#include "rfid/rfid_manager.h"
#include <LittleFS.h>
#include "config.h"

static JsonDocument rfidDatabase;
static bool scanActive = false;
static String lastScannedUID = "";
static unsigned long scanStartTime = 0;

void rfidManagerInit() {
    #if ENABLE_RFID_MANAGER
    if (!LittleFS.exists("/rfid")) {
        LittleFS.mkdir("/rfid");
    }
    loadRFIDTags();
    #endif
}

void rfidManagerUpdate() {
    #if ENABLE_RFID_MANAGER
    if (scanActive) {
        // In demo or route-test profile, simulate detection after 2 seconds
        if (DEMO_MODE || RFID_STATE == STATE_DEMO) {
            if (millis() - scanStartTime > 2000) {
                // Return a random test tag UID
                String testUIDs[] = {"04A7329B6C", "04B2187F21", "04C59133A2", "04D8421190"};
                int index = random(0, 4);
                lastScannedUID = testUIDs[index];
                scanActive = false;
                Serial.print("RFIDManager (Demo): Tag scanned: ");
                Serial.println(lastScannedUID);
            }
        } else if (RFID_STATE == STATE_REAL) {
            // Real MFRC522 or RFID hardware check would go here
        }
    }
    #endif
}

bool loadRFIDTags() {
    #if ENABLE_RFID_MANAGER
    if (!LittleFS.exists("/rfid/tags.json")) {
        // Create an empty database
        rfidDatabase.clear();
        rfidDatabase["tags"] = JsonDocument().to<JsonArray>();
        saveRFIDTags();
        return true;
    }

    File file = LittleFS.open("/rfid/tags.json", FILE_READ);
    if (!file) {
        Serial.println("RFIDManager: Failed to open tags.json for reading!");
        return false;
    }

    DeserializationError error = deserializeJson(rfidDatabase, file);
    file.close();

    if (error) {
        Serial.print("RFIDManager: Deserialization failed: ");
        Serial.println(error.c_str());
        return false;
    }

    return true;
    #else
    return false;
    #endif
}

bool saveRFIDTags() {
    #if ENABLE_RFID_MANAGER
    File file = LittleFS.open("/rfid/tags.json", FILE_WRITE);
    if (!file) {
        Serial.println("RFIDManager: Failed to open tags.json for writing!");
        return false;
    }

    if (serializeJson(rfidDatabase, file) == 0) {
        Serial.println("RFIDManager: Failed to write JSON to file!");
        file.close();
        return false;
    }

    file.close();
    return true;
    #else
    return false;
    #endif
}

JsonDocument getRFIDTagsJSON() {
    return rfidDatabase;
}

bool addOrUpdateRFIDTag(const String &uid, const String &name, const String &location) {
    #if ENABLE_RFID_MANAGER
    JsonArray tags = rfidDatabase["tags"].as<JsonArray>();
    bool found = false;

    for (JsonObject tag : tags) {
        if (tag["uid"].as<String>() == uid) {
            tag["name"] = name;
            tag["location"] = location;
            found = true;
            break;
        }
    }

    if (!found) {
        JsonObject newTag = tags.add<JsonObject>();
        newTag["uid"] = uid;
        newTag["name"] = name;
        newTag["location"] = location;
    }

    return saveRFIDTags();
    #else
    return false;
    #endif
}

bool deleteRFIDTag(const String &uid) {
    #if ENABLE_RFID_MANAGER
    JsonArray tags = rfidDatabase["tags"].as<JsonArray>();
    int indexToRemove = -1;

    for (size_t i = 0; i < tags.size(); i++) {
        if (tags[i]["uid"].as<String>() == uid) {
            indexToRemove = i;
            break;
        }
    }

    if (indexToRemove != -1) {
        tags.remove(indexToRemove);
        return saveRFIDTags();
    }
    #endif
    return false;
}

void startRFIDScan() {
    #if ENABLE_RFID_MANAGER
    scanActive = true;
    lastScannedUID = "";
    scanStartTime = millis();
    Serial.println("RFIDManager: Scanning for tags...");
    #endif
}

bool isScanning() {
    return scanActive;
}

bool getLatestScan(String &uid) {
    if (lastScannedUID != "") {
        uid = lastScannedUID;
        return true;
    }
    return false;
}

void clearLatestScan() {
    lastScannedUID = "";
}
