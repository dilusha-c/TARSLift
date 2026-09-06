#include "rfid/rfid_manager.h"
#include <LittleFS.h>
#include <SPI.h>
#include <MFRC522.h>
#include "config.h"
#include "../errors/error_logger.h"
#include "demo/demo_manager.h"

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
static String activeTagUID = "NONE";
static String persistentLastUID = "NONE";
static unsigned long scanStartTime = 0;
static unsigned long lastTagScanTime = 0;

void rfidManagerInit() {
    if (!LittleFS.exists("/rfid")) {
        LittleFS.mkdir("/rfid");
    }
    loadRFIDTags();

    // Configure chip select pin manually
    pinMode(RFID_SS_PIN, OUTPUT);
    digitalWrite(RFID_SS_PIN, HIGH);

    // Initialize Direct SPI for MFRC522
    SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, -1);
    delay(10);
    mfrc522.PCD_Init();
    delay(10);
    
    // Increase antenna gain to MAX (crucial for clone chips and reliable reads)
    mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
    
    byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
    if (v == 0x00 || v == 0xFF) {
        Serial.println("RFIDManager: MFRC522 hardware NOT detected on SPI (GPIO 10,9,12,13,11).");
        mfrcInitialized = false;
    } else {
        Serial.printf("RFIDManager: Direct SPI MFRC522 detected (Version: 0x%02X).\n", v);
        mfrcInitialized = true;
    }
}

void rfidManagerUpdate() {
    static unsigned long lastCheckTime = 0;
    static unsigned long lastInitRetry = 0;
    unsigned long now = millis();
    
    // Auto-recovery if hardware wasn't ready at boot
    if (!mfrcInitialized && (now - lastInitRetry >= 3000)) {
        lastInitRetry = now;
        mfrc522.PCD_Init();
        mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
        byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
        if (v != 0x00 && v != 0xFF) {
            Serial.printf("RFIDManager: Direct SPI MFRC522 auto-detected (Version: 0x%02X).\n", v);
            webSerialPrintln("RFIDManager: Hardware auto-detected (Version: 0x" + String(v, HEX) + ")");
            mfrcInitialized = true;
        }
    }

    // Continuous card scanning every 80ms
    if (now - lastCheckTime >= 80) {
        lastCheckTime = now;
        
        if (mfrcInitialized) {
            byte bufferATQA[2];
            byte bufferSize = sizeof(bufferATQA);
            
            // Clear SPI registers for clean RF transmission
            mfrc522.PCD_WriteRegister(mfrc522.TxModeReg, 0x00);
            mfrc522.PCD_WriteRegister(mfrc522.RxModeReg, 0x00);
            mfrc522.PCD_WriteRegister(mfrc522.ModWidthReg, 0x26);

            bool cardPresent = false;
            
            // 1. Try RequestA (catches newly arrived cards)
            MFRC522::StatusCode status = mfrc522.PICC_RequestA(bufferATQA, &bufferSize);
            if (status == MFRC522::STATUS_OK || status == MFRC522::STATUS_COLLISION) {
                cardPresent = true;
            } else {
                // 2. Try WakeupA (catches cards that are still present and were halted)
                status = mfrc522.PICC_WakeupA(bufferATQA, &bufferSize);
                if (status == MFRC522::STATUS_OK || status == MFRC522::STATUS_COLLISION) {
                    cardPresent = true;
                }
            }
            
            if (cardPresent && mfrc522.PICC_ReadCardSerial()) {
                char uidBuf[16];
                snprintf(uidBuf, sizeof(uidBuf), "%02X%02X%02X%02X",
                         mfrc522.uid.uidByte[0], mfrc522.uid.uidByte[1],
                         mfrc522.uid.uidByte[2], mfrc522.uid.uidByte[3]);
                String currentUID = String(uidBuf);
                
                // Print to terminal on first detection or card change
                if (currentUID != persistentLastUID || activeTagUID == "NONE") {
                    String tagName = getRFIDTagName(currentUID);
                    String msg = "RFIDManager: Tag Detected: " + currentUID;
                    if (tagName.length() > 0) {
                        msg += " (" + tagName + ")";
                    }
                    webSerialPrintln(msg);
                }
                
                persistentLastUID = currentUID;
                activeTagUID = currentUID;
                lastTagScanTime = now;
                scanActive = false;
                updateTelemetryRFID(activeTagUID);
                
                // Halt card so WakeupA can continuously read it on subsequent cycles
                mfrc522.PICC_HaltA();
                mfrc522.PCD_StopCrypto1();
            } else {
                // Card not detected on this cycle
                if (activeTagUID != "NONE" && (now - lastTagScanTime > 1500)) {
                    activeTagUID = "NONE";
                    updateTelemetryRFID("NONE");
                }
            }
        }
    }

    if (scanActive) {
        // In demo profile, simulate detection after 2 seconds
        if (DEMO_MODE || RFID_STATE == STATE_DEMO) {
            if (now - scanStartTime > 2000) {
                String testUIDs[] = {"04A7329B6C", "04B2187F21", "04C59133A2", "04D8421190"};
                int index = random(0, 4);
                persistentLastUID = testUIDs[index];
                activeTagUID = persistentLastUID;
                scanActive = false;
                updateTelemetryRFID(activeTagUID);
                lastTagScanTime = now;
                Serial.print("RFIDManager (Demo): Tag scanned: ");
                Serial.println(activeTagUID);
                webSerialPrintln("RFIDManager (Demo): Tag scanned: " + activeTagUID + " (" + getRFIDTagName(activeTagUID) + ")");
            }
        }
    }
}

bool loadRFIDTags() {
    if (!LittleFS.exists("/rfid/tags.json")) {
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
}

bool saveRFIDTags() {
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
}

JsonDocument getRFIDTagsJSON() {
    return rfidDatabase;
}

bool addOrUpdateRFIDTag(const String &uid, const String &name, const String &location) {
    JsonArray tags = rfidDatabase["tags"].as<JsonArray>();
    bool found = false;

    for (JsonObject tag : tags) {
        if (tag["uid"].as<String>().equalsIgnoreCase(uid)) {
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
}

bool deleteRFIDTag(const String &uid) {
    JsonArray tags = rfidDatabase["tags"].as<JsonArray>();
    int indexToRemove = -1;

    for (size_t i = 0; i < tags.size(); i++) {
        if (tags[i]["uid"].as<String>().equalsIgnoreCase(uid)) {
            indexToRemove = i;
            break;
        }
    }

    if (indexToRemove != -1) {
        tags.remove(indexToRemove);
        return saveRFIDTags();
    }
    return false;
}

String getRFIDTagName(const String &uid) {
    if (uid == "" || uid == "NONE") return "";
    JsonArray tags = rfidDatabase["tags"].as<JsonArray>();
    for (JsonObject tag : tags) {
        if (tag["uid"].as<String>().equalsIgnoreCase(uid)) {
            return tag["name"].as<String>();
        }
    }
    return "";
}

String getLastScannedRFID() {
    return persistentLastUID;
}

String getCurrentActiveRFID() {
    return activeTagUID;
}

bool isRFIDReaderHardwareReady() {
    return mfrcInitialized;
}

unsigned long getLastRFIDScanTime() {
    return lastTagScanTime;
}

void startRFIDScan() {
    scanActive = true;
    scanStartTime = millis();
    Serial.println("RFIDManager: Scanning for tags...");
}

void simulateRFIDScan() {
    String testUIDs[] = {"04A7329B6C", "04B2187F21", "04C59133A2", "04D8421190"};
    int index = random(0, 4);
    persistentLastUID = testUIDs[index];
    activeTagUID = persistentLastUID;
    scanActive = false;
    updateTelemetryRFID(activeTagUID);
    lastTagScanTime = millis();
    String tagName = getRFIDTagName(activeTagUID);
    String msg = "RFIDManager (Simulated): Tag scanned: " + activeTagUID;
    if (tagName.length() > 0) msg += " (" + tagName + ")";
    webSerialPrintln(msg);
}

bool isScanning() {
    return scanActive;
}

bool getLatestScan(String &uid) {
    if (persistentLastUID != "" && persistentLastUID != "NONE") {
        uid = persistentLastUID;
        return true;
    }
    return false;
}

void clearLatestScan() {
    persistentLastUID = "NONE";
    activeTagUID = "NONE";
    updateTelemetryRFID("NONE");
}

void testRFIDScan() {
    webSerialPrintln("--- RFID Diagnostics ---");
    if (!mfrcInitialized) {
        webSerialPrintln("STATUS: [ERROR] Hardware NOT initialized.");
        webSerialPrintln("ACTION: Check 3.3V power and SPI wiring (SCK:12, MOSI:11, MISO:13, RST:9, SS:10).");
    } else {
        webSerialPrintln("STATUS: [OK] Hardware Initialized & Communicating on SPI.");
        byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
        webSerialPrintln("VersionReg: 0x" + String(v, HEX));
        if (activeTagUID != "" && activeTagUID != "NONE") {
            webSerialPrintln("Card Currently on Reader: " + activeTagUID + " (" + getRFIDTagName(activeTagUID) + ")");
        } else if (persistentLastUID != "" && persistentLastUID != "NONE") {
            webSerialPrintln("Last Scanned Card: " + persistentLastUID + " (" + getRFIDTagName(persistentLastUID) + ")");
        } else {
            webSerialPrintln("No tag currently detected. Please place a tag on the scanner.");
        }
    }
    webSerialPrintln("------------------------");
}
