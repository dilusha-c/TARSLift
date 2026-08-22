#include "errors/error_logger.h"
#include <LittleFS.h>
#include "config.h"
#include <vector>
#include "rfid/rfid_manager.h"
#include "routes/route_manager.h"

static SystemError currentActiveError = {"", "", "", "", false};
static String todayDate = "2026-08-13"; // Default date, can be updated via NTP or manually

void errorLoggerInit() {
    if (ENABLE_ERROR_LOG) {
        if (!LittleFS.exists("/logs")) {
            LittleFS.mkdir("/logs");
        }
    }
}

static unsigned long lastLogTimestamp = 0;
static String lastLogCode = "";

void logError(const String &source, const String &code, const String &description) {
    if (ENABLE_ERROR_LOG) {
        // Prevent duplicate SPI flash write wear if same error repeats continuously
        unsigned long now = millis();
        if (code == lastLogCode && (now - lastLogTimestamp < 2000)) {
            return;
        }
        lastLogCode = code;
        lastLogTimestamp = now;

        String logPath = getTodayLogPath();
        File logFile = LittleFS.open(logPath, FILE_APPEND);
        if (!logFile) {
            Serial.println("ErrorLogger: Failed to open log file for appending!");
            return;
        }

        // Format: HH:MM:SS\tSOURCE\tCODE\tDESCRIPTION
        // We simulate a simple time index if NTP is not set
        unsigned long secs = millis() / 1000;
        char timeStr[9];
        snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu:%02lu", (secs / 3600) % 24, (secs / 60) % 60, secs % 60);

        logFile.printf("%s\t%s\t%s\t%s\n", timeStr, source.c_str(), code.c_str(), description.c_str());
        logFile.close();

        // Update active error
        setCurrentError(source, code, description);
    }
}

void clearTodayLog() {
    if (ENABLE_ERROR_LOG) {
        String logPath = getTodayLogPath();
        if (LittleFS.exists(logPath)) {
            LittleFS.remove(logPath);
        }
        clearCurrentError();
    }
}

String getTodayLogPath() {
    return "/logs/" + todayDate + ".log";
}

String readLogFile(const String &path) {
    if (ENABLE_ERROR_LOG) {
        if (!LittleFS.exists(path)) {
            return "";
        }
        File logFile = LittleFS.open(path, FILE_READ);
        if (!logFile) {
            return "";
        }
        String content = logFile.readString();
        logFile.close();
        return content;
    } else {
        return "";
    }
}

JsonDocument getErrorStatistics() {
    JsonDocument doc;
    JsonObject stats = doc.to<JsonObject>();
    stats["total"] = 0;
    
    JsonObject categories = stats["categories"].to<JsonObject>();
    categories["UART"] = 0;
    categories["RFID"] = 0;
    categories["MPU6050"] = 0;
    categories["ToF"] = 0;
    categories["Encoder"] = 0;
    categories["Motor"] = 0;
    categories["Battery"] = 0;
    categories["System"] = 0;

    if (ENABLE_ERROR_LOG) {
        String logPath = getTodayLogPath();
        if (LittleFS.exists(logPath)) {
            File logFile = LittleFS.open(logPath, FILE_READ);
            if (logFile) {
                int count = 0;
                while (logFile.available()) {
                    String line = logFile.readStringUntil('\n');
                    if (line.length() == 0) continue;
                    count++;
                    
                    // Classify category by matching strings in line
                    if (line.indexOf("UART") >= 0) categories["UART"] = categories["UART"].as<int>() + 1;
                    else if (line.indexOf("RFID") >= 0) categories["RFID"] = categories["RFID"].as<int>() + 1;
                    else if (line.indexOf("MPU6050") >= 0) categories["MPU6050"] = categories["MPU6050"].as<int>() + 1;
                    else if (line.indexOf("ToF") >= 0) categories["ToF"] = categories["ToF"].as<int>() + 1;
                    else if (line.indexOf("Encoder") >= 0) categories["Encoder"] = categories["Encoder"].as<int>() + 1;
                    else if (line.indexOf("Motor") >= 0) categories["Motor"] = categories["Motor"].as<int>() + 1;
                    else if (line.indexOf("Battery") >= 0) categories["Battery"] = categories["Battery"].as<int>() + 1;
                    else categories["System"] = categories["System"].as<int>() + 1;
                }
                stats["total"] = count;
                logFile.close();
            }
        }
    }

    return doc;
}

SystemError getCurrentError() {
    return currentActiveError;
}

void clearCurrentError() {
    currentActiveError.active = false;
    currentActiveError.source = "";
    currentActiveError.code = "";
    currentActiveError.description = "";
}

void setCurrentError(const String &source, const String &code, const String &description) {
    unsigned long secs = millis() / 1000;
    char timeStr[9];
    snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu:%02lu", (secs / 3600) % 24, (secs / 60) % 60, secs % 60);

    currentActiveError.timestamp = String(timeStr);
    currentActiveError.source = source;
    currentActiveError.code = code;
    currentActiveError.description = description;
    currentActiveError.active = true;
}

// Web Serial Monitor implementations (Thread-Safe across Core 0 & Core 1)
static std::vector<String> webSerialLogs;
static const size_t MAX_LOG_LINES = 50;
static String currentLinePending = "";
static SemaphoreHandle_t webSerialMutex = NULL;

static void initWebSerialMutex() {
    if (webSerialMutex == NULL) {
        webSerialMutex = xSemaphoreCreateMutex();
    }
}

void webSerialPrint(const String &text) {
    Serial.print(text);
    initWebSerialMutex();

    if (webSerialMutex != NULL) {
        xSemaphoreTake(webSerialMutex, portMAX_DELAY);
    }

    currentLinePending += text;
    
    int idx;
    while ((idx = currentLinePending.indexOf('\n')) >= 0) {
        String line = currentLinePending.substring(0, idx);
        if (line.endsWith("\r")) {
            line = line.substring(0, line.length() - 1);
        }
        
        unsigned long secs = millis() / 1000;
        char timeStr[15];
        snprintf(timeStr, sizeof(timeStr), "[%02lu:%02lu:%02lu] ", (secs / 3600) % 24, (secs / 60) % 60, secs % 60);
        String formattedLine = String(timeStr) + line;

        webSerialLogs.push_back(formattedLine);
        if (webSerialLogs.size() > MAX_LOG_LINES) {
            webSerialLogs.erase(webSerialLogs.begin());
        }
        currentLinePending = currentLinePending.substring(idx + 1);
    }

    if (webSerialMutex != NULL) {
        xSemaphoreGive(webSerialMutex);
    }
}

void webSerialPrintln(const String &text) {
    webSerialPrint(text + "\n");
}

JsonDocument getWebSerialLogs() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    initWebSerialMutex();

    if (webSerialMutex != NULL) {
        xSemaphoreTake(webSerialMutex, portMAX_DELAY);
    }

    for (const auto &log : webSerialLogs) {
        arr.add(log);
    }

    if (webSerialMutex != NULL) {
        xSemaphoreGive(webSerialMutex);
    }
    return doc;
}

void clearWebSerialLogs() {
    initWebSerialMutex();
    if (webSerialMutex != NULL) {
        xSemaphoreTake(webSerialMutex, portMAX_DELAY);
    }

    webSerialLogs.clear();
    currentLinePending = "";

    if (webSerialMutex != NULL) {
        xSemaphoreGive(webSerialMutex);
    }
}

void handleSerialCommand(const String &cmd) {
    String trimmed = cmd;
    trimmed.trim();
    if (trimmed.length() == 0) return;

    webSerialPrintln("> " + trimmed);

    if (trimmed.equalsIgnoreCase("/help")) {
        webSerialPrintln("Available Commands:");
        webSerialPrintln("  /help   - Show this help message");
        webSerialPrintln("  /scan   - Simulate an RFID card scan");
        webSerialPrintln("  /clear  - Clear today's persistent error logs");
        webSerialPrintln("  /stop   - Terminate current repeat mission");
        webSerialPrintln("  /reboot - Restart the ESP32-S3 microcontroller");
    } else if (trimmed.equalsIgnoreCase("/reboot")) {
        webSerialPrintln("Rebooting system in 1 second...");
        delay(1000);
        ESP.restart();
    } else if (trimmed.equalsIgnoreCase("/scan")) {
        webSerialPrintln("Triggering RFID card scanner simulation...");
        startRFIDScan();
    } else if (trimmed.equalsIgnoreCase("/clear")) {
        clearTodayLog();
        webSerialPrintln("Persistent error logs cleared successfully.");
    } else if (trimmed.equalsIgnoreCase("/stop")) {
        stopRepeating();
        webSerialPrintln("Mission stop command broadcasted.");
    } else {
        webSerialPrintln("Error: Unknown command '" + trimmed + "'. Type /help for assistance.");
    }
}
