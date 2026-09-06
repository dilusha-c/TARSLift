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
#include "battery/battery_manager.h"
#include "communication/uart_manager.h"
#include "hardware/oled/oled_display.h"

// Telemetry Broadcast Interval (ms)
const unsigned long TELEMETRY_INTERVAL_MS = 200;
static unsigned long lastTelemetryBroadcast = 0;
static unsigned long lastWifiCheckTime = 0;

static TaskHandle_t webTaskHandle = NULL;
static TaskHandle_t realtimeTaskHandle = NULL;

static uint32_t loopCounter = 0;
static unsigned long lastLoopCalcTime = 0;
static unsigned long loopAccumMicros = 0;

static unsigned long lastSystemUpdate = 0;
static unsigned long lastUartUpdate = 0;
static unsigned long lastRfidUpdate = 0;
static unsigned long lastRouteUpdate = 0;
static unsigned long lastOledUpdate = 0;

// ============================================================================
// CORE 0 TASK: Wi-Fi Stack, Web Server, and WebSocket Telemetry Broadcaster
// ============================================================================
void webTask(void *pvParameters) {
    webSerialPrintln("FreeRTOS Core 0: Web & Network Task Started (Core 0)");
    for (;;) {
        unsigned long now = millis();
        if (now - lastTelemetryBroadcast >= TELEMETRY_INTERVAL_MS) {
            lastTelemetryBroadcast = now;
            broadcastTelemetry();
        }

        // Update OLED display every 500ms
        if (now - lastOledUpdate >= 500) {
            lastOledUpdate = now;
            TelemetryData tele = getTelemetry();
            String mode = (getAGVMode() == MODE_TEACH) ? "TEACH" : ((getAGVMode() == MODE_REPEAT) ? "REPEAT" : "IDLE");
            AGVState st = getAGVState();
            String stStr = "STOP";
            if (st == STATE_RUNNING) stStr = "RUN";
            else if (st == STATE_PAUSED) stStr = "PAUSE";
            else if (st == STATE_RECORDING) stStr = "REC";
            bool wifiSTA = (WiFi.status() == WL_CONNECTED);
            String wifiIP = wifiSTA ? WiFi.localIP().toString() : "";
            oledUpdateLive(tele, mode, stStr, isStm32Connected(), wifiSTA, wifiIP, getWifiRSSI());
        }

        // Background LAN Wi-Fi STA Reconnect Monitor (Every 10 seconds)
        if (now - lastWifiCheckTime >= 10000) {
            lastWifiCheckTime = now;
            if (strlen(sysSettings.wifi_ssid) > 0 && strcmp(sysSettings.wifi_ssid, "TARSLIFT_AGV") != 0 && WiFi.status() != WL_CONNECTED) {
                webSerialPrint("WiFi STA: Background Reconnecting to LAN '");
                webSerialPrint(sysSettings.wifi_ssid);
                webSerialPrintln("'...");
                WiFi.begin(sysSettings.wifi_ssid, sysSettings.wifi_password);
            }
        }


        // Cleanup dead/stale WebSocket clients
        cleanupWebSocketClients();

        vTaskDelay(pdMS_TO_TICKS(10)); // Yield to Wi-Fi stack
    }
}

// ============================================================================
// CORE 1 TASK: Real-Time Motion, STM32 UART Link, RFID & Watchdog Interlock
// ============================================================================
void realtimeTask(void *pvParameters) {
    webSerialPrintln("FreeRTOS Core 1: Real-Time Subsystems & Watchdog Task Started (Core 1)");
    for (;;) {
        unsigned long startMicros = micros();
        unsigned long now = millis();

        // 1. Service UART Bus Link (200 Hz)
        if (now - lastUartUpdate >= 5) {
            lastUartUpdate = now;
            stm32UartUpdate();
        }

        // 2. Service Trajectory & Route Manager (50 Hz)
        if (now - lastRouteUpdate >= 20) {
            lastRouteUpdate = now;
            routeManagerUpdate();
        }

        // 3. Service Hardware RFID Scanner (20 Hz)
        if (now - lastRfidUpdate >= 50) {
            lastRfidUpdate = now;
            rfidManagerUpdate();
        }

        // 4. Service System Health Manager (20 Hz)
        if (now - lastSystemUpdate >= 50) {
            lastSystemUpdate = now;
            systemManagerUpdate();
            batteryManagerUpdate();
        }

        if (DEMO_MODE) {
            demoManagerUpdate();
        }

        // 5. Hardware Fail-Safe Watchdog Interlock Check
        checkUartWatchdog();

        // Measure execution latency
        unsigned long workMicros = micros() - startMicros;
        loopAccumMicros += workMicros;
        loopCounter++;

        if (now - lastLoopCalcTime >= 1000) {
            float avgMs = (loopAccumMicros / 1000.0f) / (loopCounter > 0 ? loopCounter : 1);
            uint32_t hz = (loopCounter * 1000) / ((now - lastLoopCalcTime) > 0 ? (now - lastLoopCalcTime) : 1);
            float totalPeriodMicros = (now - lastLoopCalcTime) * 1000.0f;
            float cpuPct = (loopAccumMicros / (totalPeriodMicros > 0.0f ? totalPeriodMicros : 1.0f)) * 100.0f;
            if (cpuPct > 100.0f) cpuPct = 100.0f;
            if (cpuPct < 0.5f) cpuPct = 1.2f;

            updatePerformanceMetrics(avgMs, hz, cpuPct);
            
            loopAccumMicros = 0;
            loopCounter = 0;
            lastLoopCalcTime = now;
        }

        vTaskDelay(pdMS_TO_TICKS(2)); // Micro-yield to Core 1 idle task
    }
}

#include "rfid/rfid_manager.h"

void setup() {
    // Initialize serial console
    // Initialize serial console
    Serial.begin(115200);
    // For native USB, wait until the host opens the port
    unsigned long waitStart = millis();
    while (!Serial && (millis() - waitStart) < 3000) { delay(10); }
    // After 3 s we continue even if the host hasn't opened the port
    Serial.println("System ready - type /scan in Serial Monitor");
    // Heartbeat ticker (optional)
    static unsigned long lastHb = 0;
    if (millis() - lastHb >= 5000) {
        Serial.println("[HB] ESP32 alive");
        lastHb = millis();
    }

    // Initialize OLED display and play boot sequence
    oledInit();
    oledBootSequence();

    webSerialPrintln("\n==============================================");
    webSerialPrintln("TARSLIFT AGV - ESP32-S3 High-Level Controller");
    webSerialPrintln("==============================================");

    // Initialize System and mounting filesystem
    systemManagerInit();
    
    // Initialize Log structures
    errorLoggerInit();

    // Start WiFi in AP + STA Dual Mode
    webSerialPrintln("WiFi: Initializing Dual AP + STA Mode...");
    WiFi.mode(WIFI_AP_STA);
    
    // 1. Configure AP Hotspot for ultra-low latency direct phone control (192.168.4.1)
    if (WiFi.softAP("TARSLIFT_AGV", "12345678", 1, 0, 4)) {
        IPAddress apIP = WiFi.softAPIP();
        webSerialPrint("WiFi AP Hotspot: Active at ");
        webSerialPrintln(apIP.toString());
    } else {
        webSerialPrintln("WiFi AP: Failed to start Access Point!");
        logError("SYSTEM", "E00", "Access Point Setup Failed");
    }

    // 2. Connect to LAN Router simultaneously if SSID is configured
    if (strlen(sysSettings.wifi_ssid) > 0 && strcmp(sysSettings.wifi_ssid, "TARSLIFT_AGV") != 0) {
        webSerialPrint("WiFi STA: Connecting to LAN Router '");
        webSerialPrint(sysSettings.wifi_ssid);
        webSerialPrintln("'...");
        WiFi.begin(sysSettings.wifi_ssid, sysSettings.wifi_password);
    }


    // Initialize STM32 UART Link
    stm32UartInit();

    // Wait for STM32 to boot and establish comms
    delay(1000);

    // Send initial configuration to STM32
    sendStm32EncoderConfig(sysSettings.wheel_circ_mm, sysSettings.enc_ppr_l, sysSettings.enc_ppr_r);
    sendStm32TofConfig(sysSettings.tof_stop_distance_mm);
    sendStm32PidTuning(sysSettings.pid_kp_l, sysSettings.pid_ki_l, sysSettings.pid_kd_l,
                       sysSettings.pid_kp_r, sysSettings.pid_ki_r, sysSettings.pid_kd_r);
    sendStm32PidEnable(sysSettings.enable_pid);

    // Push initial motor trim settings to STM32
    if (sysSettings.enable_stm32_uart && !sysSettings.demo_mode) {
        sendStm32MotorTrim(
            sysSettings.motor_l_fwd_scale,
            sysSettings.motor_r_fwd_scale,
            sysSettings.motor_l_turn_scale,
            sysSettings.motor_r_turn_scale
        );
    }

    // Initialize Managers
    batteryManagerInit();
    rfidManagerInit();
    routeManagerInit();

    if (DEMO_MODE) {
        webSerialPrintln("DemoManager: Initializing mock telemetry mode...");
        demoManagerInit();
    }

    // Initialize Web Server (hosts static LittleFS dashboard and REST API)
    webServerInit();

    // Spawn Core 0 Task: Wi-Fi & Web Broadcasting
    xTaskCreatePinnedToCore(
        webTask,
        "webTask",
        4096,
        NULL,
        1,
        &webTaskHandle,
        0 // Pin to Core 0
    );

    // Spawn Core 1 Task: Real-Time Motion, UART & Watchdog Safety
    xTaskCreatePinnedToCore(
        realtimeTask,
        "realtimeTask",
        8192,
        NULL,
        2,
        &realtimeTaskHandle,
        1 // Pin to Core 1
    );

    webSerialPrintln("System: Dual-Core FreeRTOS & Wi-Fi Dual-Mode Active.");
    logError("SYSTEM", "I00", "System Boot Successful - AP+STA Active");
}

void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "/scan") {
            extern void testRFIDScan();
            testRFIDScan();
        }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}
