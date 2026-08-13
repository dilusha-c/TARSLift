#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "system/system_manager.h"
#include "errors/error_logger.h"
#include "rfid/rfid_manager.h"
#include "routes/route_manager.h"
#include "demo/demo_manager.h"
#include "web/web_server.h"
#include "web/websocket.h"
#include "communication/stm32_uart.h"

// Telemetry Broadcast Interval (ms)
const unsigned long TELEMETRY_INTERVAL_MS = 200;
static unsigned long lastTelemetryBroadcast = 0;

void setup() {
    // Initialize serial console
    Serial.begin(115200);
    delay(1000);
    webSerialPrintln("\n==============================================");
    webSerialPrintln("TARSLIFT AGV - ESP32-S3 High-Level Controller");
    webSerialPrintln("==============================================");

    // Initialize System and mounting filesystem
    systemManagerInit();
    
    // Initialize Log structures
    errorLoggerInit();

    // Start WiFi in Access Point mode
    webSerialPrint("WiFi: Configuring Access Point '");
    webSerialPrint(WIFI_SSID);
    webSerialPrintln("'...");
    
    WiFi.mode(WIFI_AP);
    if (WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, 0, WIFI_MAX_CONN)) {
        IPAddress IP = WiFi.softAPIP();
        webSerialPrint("WiFi: AP started successfully. IP address: ");
        webSerialPrintln(IP.toString());
    } else {
        webSerialPrintln("WiFi: Failed to establish Access Point!");
        logError("SYSTEM", "E00", "Access Point Setup Failed");
    }

    // Initialize STM32 UART Link
    stm32UartInit();

    // Initialize Managers
    rfidManagerInit();
    routeManagerInit();

    if (DEMO_MODE) {
        webSerialPrintln("DemoManager: Initializing mock telemetry mode...");
        demoManagerInit();
    }

    // Initialize Web Server (hosts static LittleFS dashboard and REST API)
    webServerInit();

    webSerialPrintln("System: Initialization completed successfully.");
    logError("SYSTEM", "I00", "System Boot Successful"); // Log a info/startup tag
}

void loop() {
    // Periodically update core managers
    systemManagerUpdate();
    stm32UartUpdate();
    rfidManagerUpdate();
    routeManagerUpdate();

    if (DEMO_MODE) {
        demoManagerUpdate();
    }

    // WebSocket Telemetry Broadcaster
    unsigned long now = millis();
    if (now - lastTelemetryBroadcast >= TELEMETRY_INTERVAL_MS) {
        lastTelemetryBroadcast = now;
        broadcastTelemetry();
    }
}
