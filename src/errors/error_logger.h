#ifndef ERROR_LOGGER_H
#define ERROR_LOGGER_H

#include <Arduino.h>
#include <ArduinoJson.h>

struct SystemError {
    String timestamp;
    String source;
    String code;
    String description;
    bool active;
};

void errorLoggerInit();
void logError(const String &source, const String &code, const String &description);
void clearTodayLog();
String getTodayLogPath();
String readLogFile(const String &path);
JsonDocument getErrorStatistics();
SystemError getCurrentError();
void clearCurrentError();
void setCurrentError(const String &source, const String &code, const String &description);

// Web Serial Monitor functions
void webSerialPrint(const String &text);
void webSerialPrintln(const String &text);
JsonDocument getWebSerialLogs();
void clearWebSerialLogs();
void handleSerialCommand(const String &cmd);

#endif // ERROR_LOGGER_H
