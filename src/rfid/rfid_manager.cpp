#include "rfid/rfid_manager.h"
#include <LittleFS.h>
#include <SPI.h>
#include <MFRC522.h>
#include "config.h"

// SPI MFRC522 Hardware Pins for ESP32-S3
#define RFID_SS_PIN   10
#define RFID_RST_PIN  9
#define RFID_SCK_PIN  12
#define RFID_MISO_PIN 13
#define RFID_MOSI_PIN 11

static MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);
static bool mfrcInitialized = false;

static JsonDocument rfidDatabase;
static bool scanActive = false;
static String lastScannedUID = "";
static unsigned long scanStartTime = 0;

void rfidManagerInit() {
    if (ENABLE_RFID_MANAGER) {
        if (!LittleFS.exists("/rfid")) {
            LittleFS.mkdir("/rfid");
        }
        loadRFIDTags();

        // Initialize Direct SPI for MFRC522
        SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);
        mfrc522.PCD_Init();
        delay(10);
        byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
        if (v == 0x00 || v == 0xFF) {
            Serial.println("RFIDManager: MFRC522 hardware NOT detected on SPI (GPIO 10,9,12,13,11).");
            mfrcInitialized = false;
        } else {
            Serial.printf("RFIDManager: Direct SPI MFRC522 detected (Version: 0x%02X).\n", v);
            mfrcInitialized = true;
        }
    }
}

void rfidManagerUpdate() {
    if (ENABLE_RFID_MANAGER) {
        // Direct Hardware SPI Scan
        if (mfrcInitialized && mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
            char uidBuf[16];
            snprintf(uidBuf, sizeof(uidBuf), "%02X%02X%02X%02X",
                     mfrc522.uid.uidByte[0], mfrc522.uid.uidByte[1],
                     mfrc522.uid.uidByte[2], mfrc522.uid.uidByte[3]);

            lastScannedUID = String(uidBuf);
            scanActive = false;
            Serial.print("RFIDManager: Hardware SPI Card Scanned: ");
            Serial.println(lastScannedUID);

            mfrc522.PICC_HaltA();
            mfrc522.PCD_StopCrypto1();
            return;
        }

        if (scanActive) {
            // In demo or route-test profile, simulate detection after 2 seconds
            if (DEMO_MODE || RFID_STATE == STATE_DEMO) {
                if (millis() - scanStartTime > 2000) {
                    String testUIDs[] = {"04A7329B6C", "04B2187F21", "04C59133A2", "04D8421190"};
                    int index = random(0, 4);
                    lastScannedUID = testUIDs[index];
                    scanActive = false;
                    Serial.print("RFIDManager (Demo): Tag scanned: ");
                    Serial.println(lastScannedUID);
                }
            }
        }
    }
}

bool loadRFIDTags() {
    if (ENABLE_RFID_MANAGER) {
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
    } else {
        return false;
    }
}

bool saveRFIDTags() {
    if (ENABLE_RFID_MANAGER) {
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
    } else {
        return false;
    }
}

JsonDocument getRFIDTagsJSON() {
    return rfidDatabase;
}

bool addOrUpdateRFIDTag(const String &uid, const String &name, const String &location) {
    if (ENABLE_RFID_MANAGER) {
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
    } else {
        return false;
    }
}

bool deleteRFIDTag(const String &uid) {
    if (ENABLE_RFID_MANAGER) {
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
    }
    return false;
}

void startRFIDScan() {
    if (ENABLE_RFID_MANAGER) {
        scanActive = true;
        lastScannedUID = "";
        scanStartTime = millis();
        Serial.println("RFIDManager: Scanning for tags...");
    }
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
