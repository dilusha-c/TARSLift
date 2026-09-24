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

void rfidHardwareReset() {
    // Configure CS pin
    pinMode(RFID_SS_PIN, OUTPUT);
    digitalWrite(RFID_SS_PIN, HIGH);

    // Pulse hardware RST pin LOW to force cold restart of silicon & crystal oscillator
    pinMode(RFID_RST_PIN, OUTPUT);
    digitalWrite(RFID_RST_PIN, LOW);
    delay(10);
    digitalWrite(RFID_RST_PIN, HIGH);
    delay(25);

    // Initialize MFRC522 registers
    mfrc522.PCD_Init();
    delay(10);

    // Force maximum receiver sensitivity
    mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

    // Explicitly turn antenna drivers ON
    mfrc522.PCD_AntennaOn();

    byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
    byte tx = mfrc522.PCD_ReadRegister(mfrc522.TxControlReg);

    if (v == 0x00 || v == 0xFF) {
        mfrcInitialized = false;
        Serial.println("RFIDManager: Hardware reset FAILED. SPI unresponsive (Version: 0x00/0xFF).");
    } else {
        mfrcInitialized = true;
        Serial.printf("RFIDManager: Hardware reset SUCCESS (Version: 0x%02X, TxControl: 0x%02X).\n", v, tx);
    }
}

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

    rfidHardwareReset();

    if (mfrcInitialized) {
        byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
        Serial.printf("RFIDManager: Direct SPI MFRC522 active (Version: 0x%02X).\n", v);
    } else {
        Serial.println("RFIDManager: MFRC522 hardware NOT detected on SPI (GPIO 10,9,12,13,11).");
    }
}

void rfidManagerUpdate() {
    static unsigned long lastCheckTime = 0;
    static unsigned long lastHealthCheck = 0;
    static unsigned long lastIdleRefresh = 0;
    static uint8_t consecutiveErrors = 0;
    unsigned long now = millis();
    
    // 1. Health Check Watchdog & Brownout Auto-Recovery (every 1000ms)
    if (now - lastHealthCheck >= 1000) {
        lastHealthCheck = now;
        
        byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
        if (v == 0x00 || v == 0xFF) {
            // Bus communication lost or chip frozen
            consecutiveErrors++;
            if (consecutiveErrors >= 2) {
                mfrcInitialized = false;
                Serial.printf("RFIDManager: Health check failed (Version=0x%02X). Triggering recovery...\n", v);
                rfidHardwareReset();
                consecutiveErrors = 0;
            }
        } else {
            mfrcInitialized = true;
            consecutiveErrors = 0;

            // Check if antenna driver turned OFF (brownout / internal POR detection)
            byte tx = mfrc522.PCD_ReadRegister(mfrc522.TxControlReg);
            if ((tx & 0x03) != 0x03) {
                Serial.println("RFIDManager: Brownout detected! Antenna was OFF. Re-enabling antenna & max gain...");
                webSerialPrintln("RFIDManager: Brownout detected! Antenna re-enabled.");
                mfrc522.PCD_AntennaOn();
                mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
            }

            // Ensure receiver gain remains at maximum
            byte gain = mfrc522.PCD_ReadRegister(mfrc522.RFCfgReg) & 0x70;
            if (gain != mfrc522.RxGain_max) {
                mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
            }
        }
    }

    // 2. Idle RF Maintenance (every 5000ms when no card is present)
    // Prevents register drift and clears lingering transceiver error flags
    if (mfrcInitialized && activeTagUID == "NONE" && (now - lastIdleRefresh >= 5000)) {
        lastIdleRefresh = now;
        mfrc522.PCD_StopCrypto1();
        mfrc522.PCD_WriteRegister(mfrc522.CommandReg, mfrc522.PCD_Idle);
        mfrc522.PCD_WriteRegister(mfrc522.FIFOLevelReg, 0x80); // Flush FIFO
        mfrc522.PCD_AntennaOn();
        mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
    }

    // 3. Smart Card Scanning (every 100ms = 10 Hz)
    // Avoids redundant double-polling to eliminate RF heating and blocking delays
    if (now - lastCheckTime >= 100) {
        lastCheckTime = now;
        
        if (mfrcInitialized) {
            byte bufferATQA[2];
            byte bufferSize = sizeof(bufferATQA);
            bool cardPresent = false;

            // Prepare clean registers for transceive
            mfrc522.PCD_WriteRegister(mfrc522.TxModeReg, 0x00);
            mfrc522.PCD_WriteRegister(mfrc522.RxModeReg, 0x00);
            mfrc522.PCD_WriteRegister(mfrc522.ModWidthReg, 0x26);

            if (activeTagUID == "NONE") {
                // When idle: ONLY send RequestA (catches newly arrived cards)
                MFRC522::StatusCode status = mfrc522.PICC_RequestA(bufferATQA, &bufferSize);
                if (status == MFRC522::STATUS_OK || status == MFRC522::STATUS_COLLISION) {
                    cardPresent = true;
                }
            } else {
                // When card already active: send WakeupA to track if it remains on reader
                MFRC522::StatusCode status = mfrc522.PICC_WakeupA(bufferATQA, &bufferSize);
                if (status == MFRC522::STATUS_OK || status == MFRC522::STATUS_COLLISION) {
                    cardPresent = true;
                } else {
                    // Check if a new card was placed instead
                    status = mfrc522.PICC_RequestA(bufferATQA, &bufferSize);
                    if (status == MFRC522::STATUS_OK || status == MFRC522::STATUS_COLLISION) {
                        cardPresent = true;
                    }
                }
            }
            
            if (cardPresent) {
                if (mfrc522.PICC_ReadCardSerial()) {
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
                        Serial.println(msg);
                        webSerialPrintln(msg);
                    }
                    
                    persistentLastUID = currentUID;
                    activeTagUID = currentUID;
                    lastTagScanTime = now;
                    scanActive = false;
                    updateTelemetryRFID(activeTagUID);
                    
                    // Halt card so WakeupA can continuously monitor it
                    mfrc522.PICC_HaltA();
                    mfrc522.PCD_StopCrypto1();
                } else {
                    // PICC was detected but serial read failed (card moved or collision)
                    // ALWAYS clean up state to prevent FIFO / Crypto deadlock!
                    mfrc522.PICC_HaltA();
                    mfrc522.PCD_StopCrypto1();
                    mfrc522.PCD_WriteRegister(mfrc522.CommandReg, mfrc522.PCD_Idle);
                    mfrc522.PCD_WriteRegister(mfrc522.FIFOLevelReg, 0x80);
                }
            } else {
                // Card not detected on this cycle - clear active state after 800ms debounce
                if (activeTagUID != "NONE" && (now - lastTagScanTime > 800)) {
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
    byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
    byte tx = mfrc522.PCD_ReadRegister(mfrc522.TxControlReg);
    byte gain = mfrc522.PCD_ReadRegister(mfrc522.RFCfgReg);
    byte cmd = mfrc522.PCD_ReadRegister(mfrc522.CommandReg);

    webSerialPrintln("Status: " + String(mfrcInitialized ? "[OK] Active & Initialized" : "[ERROR] Offline / Bus Error"));
    webSerialPrintln("VersionReg: 0x" + String(v, HEX) + ((v != 0x00 && v != 0xFF) ? " (Valid MFRC522)" : " (SPI ERROR / Unresponsive!)"));
    webSerialPrintln("TxControlReg: 0x" + String(tx, HEX) + (((tx & 0x03) == 0x03) ? " (Antenna ON)" : " (Antenna OFF - Brownout!)"));
    webSerialPrintln("Gain: 0x" + String(gain, HEX) + " | CommandReg: 0x" + String(cmd, HEX));

    if (activeTagUID != "" && activeTagUID != "NONE") {
        webSerialPrintln("Card Currently on Reader: " + activeTagUID + " (" + getRFIDTagName(activeTagUID) + ")");
    } else if (persistentLastUID != "" && persistentLastUID != "NONE") {
        webSerialPrintln("Last Scanned Card: " + persistentLastUID + " (" + getRFIDTagName(persistentLastUID) + ")");
    } else {
        webSerialPrintln("No tag currently detected. Please place a tag on the scanner.");
    }
    webSerialPrintln("------------------------");
}
