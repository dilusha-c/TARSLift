#include "web/websocket.h"
#include <ArduinoJson.h>
#include "config.h"
#include "demo/demo_manager.h"
#include "system/system_manager.h"
#include "routes/route_manager.h"
#include "errors/error_logger.h"
#include "communication/uart_manager.h"
#include "rfid/rfid_manager.h"

static AsyncWebSocket ws("/ws");

static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket: Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        // Send initial state
        broadcastTelemetry();
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WebSocket: Client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0;
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, (char*)data);
            if (!err) {
                if (doc["type"] == "update_pid") {
                    JsonObject d = doc["data"];
                    float kpL = d["kpL"] | 0.0f;
                    float kiL = d["kiL"] | 0.0f;
                    float kdL = d["kdL"] | 0.0f;
                    float kpR = d["kpR"] | 0.0f;
                    float kiR = d["kiR"] | 0.0f;
                    float kdR = d["kdR"] | 0.0f;
                    sendStm32PidTuning(kpL, kiL, kdL, kpR, kiR, kdR);
                    Serial.println("WebSocket: Sent new PID tuning to STM32");
                }
                else if (doc["type"] == "update_encoder_config") {
                    JsonObject d = doc["data"];
                    float wheelCircMm = d["wheel_circ_mm"] | 138.2f;
                    uint16_t pprL = d["ppr_l"] | 287;
                    uint16_t pprR = d["ppr_r"] | 287;
                    sendStm32EncoderConfig(wheelCircMm, pprL, pprR);
                    Serial.println("WebSocket: Sent new encoder config to STM32");
                }
                else if (doc["type"] == "calibrate_imu") {
                    sendStm32CalibrateImu();
                    Serial.println("WebSocket: Sent IMU calibrate command to STM32");
                }
                else if (doc["type"] == "update_tof_config") {
                    JsonObject d = doc["data"];
                    float stopDistanceMm = d["stop_distance_mm"] | 150.0f;
                    sysSettings.tof_stop_distance_mm = stopDistanceMm;
                    sysSettings.obstacle_detection = (stopDistanceMm > 0);
                    sendStm32TofConfig(stopDistanceMm);
                    Serial.printf("WebSocket: Sent ToF stop distance %.1f mm to STM32\n", stopDistanceMm);
                }
                else if (doc["type"] == "update_target_rpm") {
                    JsonObject d = doc["data"];
                    float targetRpmL = 0.0f;
                    float targetRpmR = 0.0f;
                    if (d["rpmL"].is<float>() || d["rpmR"].is<float>()) {
                        targetRpmL = d["rpmL"] | 0.0f;
                        targetRpmR = d["rpmR"] | 0.0f;
                    } else {
                        float targetRpm = d["rpm"] | 0.0f;
                        targetRpmL = targetRpm;
                        targetRpmR = targetRpm;
                    }
                    
                    // Convert RPM to mm/s
                    float circ = sysSettings.wheel_circ_mm > 0 ? sysSettings.wheel_circ_mm : 138.2f;
                    int16_t speedMmsL = (int16_t)((targetRpmL * circ) / 60.0f);
                    int16_t speedMmsR = (int16_t)((targetRpmR * circ) / 60.0f);
                    
                    sendStm32SetSpeeds(speedMmsL, speedMmsR);
                    setDemoTargetRpm(targetRpmL, targetRpmR);
                    Serial.printf("WebSocket: Sent target RPM L:%.1f, R:%.1f (mm/s: %d, %d) to STM32\n", targetRpmL, targetRpmR, speedMmsL, speedMmsR);
                }
            }
        }
    }
}

void webSocketInit(AsyncWebServer *server) {
    if (ENABLE_WEBSOCKET) {
        ws.onEvent(onWsEvent);
        server->addHandler(&ws);
        Serial.println("WebSocket: Registered /ws handler");
    }
}

void broadcastTelemetry() {
    if (!ENABLE_WEBSOCKET) return;
    if (ws.count() == 0) return; // No clients connected
    if (!ws.availableForWriteAll()) return; // Skip frame if client write queue is full to prevent overflow log

    JsonDocument doc;
    
    // Mode & Mission States
    doc["mode"] = (getAGVMode() == MODE_TEACH) ? "TEACH" : ((getAGVMode() == MODE_REPEAT) ? "REPEAT" : "IDLE");
    
    AGVState state = getAGVState();
    String stateStr = "STOPPED";
    if (state == STATE_RUNNING) stateStr = "RUNNING";
    else if (state == STATE_PAUSED) stateStr = "PAUSED";
    else if (state == STATE_RECORDING) stateStr = "RECORDING";
    else if (state == STATE_COMPLETE) stateStr = "COMPLETE";
    else if (state == STATE_ERROR) stateStr = "ERROR";
    doc["state"] = stateStr;

    // Telemetry Sensor Fields
    TelemetryData tele = getTelemetry();
    doc["x"] = round(tele.x * 100.0) / 100.0;
    doc["y"] = round(tele.y * 100.0) / 100.0;
    doc["heading"] = round(tele.heading * 10.0) / 10.0;
    doc["speed"] = round(tele.speed * 100.0) / 100.0;
    doc["teach_distance"] = round(getTeachDistance() * 100.0) / 100.0;
    doc["teach_duration"] = getTeachDurationS();
    doc["rfid"] = tele.rfid;
    doc["rfid_name"] = getRFIDTagName(tele.rfid);
    doc["last_rfid"] = getLastScannedRFID();
    doc["last_rfid_name"] = getRFIDTagName(getLastScannedRFID());
    doc["rfid_hw_ok"] = isRFIDReaderHardwareReady();

    // Battery fields matching settings configuration
    if (sysSettings.battery_monitoring) {
        doc["battery_enabled"] = true;
        doc["voltage"] = sysSettings.voltage_monitoring ? String(tele.battery_volt, 1) + " V" : "N/A";
        doc["current"] = sysSettings.current_monitoring ? String(tele.battery_curr, 2) + " A" : "N/A";
        
        if (sysSettings.voltage_monitoring && sysSettings.current_monitoring && sysSettings.power_monitoring) {
            float powerVal = tele.battery_volt * tele.battery_curr;
            doc["power"] = String(powerVal, 1) + " W";
        } else {
            doc["power"] = "N/A";
        }

        doc["battery"] = sysSettings.battery_percentage ? String((int)tele.battery_pct) + "%" : "N/A";

        // Status resolution
        String batStatus = "NORMAL";
        if (sysSettings.battery_fault_detection && (tele.battery_volt < 5.0f || tele.battery_volt > 15.0f)) {
            batStatus = "FAULT";
        } else if (sysSettings.critical_battery_warning && (tele.battery_volt <= sysSettings.critical_voltage || tele.battery_pct <= sysSettings.critical_battery_pct)) {
            batStatus = "CRITICAL";
        } else if (sysSettings.low_battery_warning && (tele.battery_volt <= sysSettings.low_voltage || tele.battery_pct <= sysSettings.low_battery_pct)) {
            batStatus = "LOW";
        }
        doc["battery_status"] = batStatus;
    } else {
        doc["battery_enabled"] = false;
        doc["voltage"] = "N/A";
        doc["current"] = "N/A";
        doc["power"] = "N/A";
        doc["battery"] = "N/A";
        doc["battery_status"] = "DISABLED";
    }

    // ToF Sensors & Obstacle Detection
    JsonObject tof = doc["tof"].to<JsonObject>();
    tof["left"] = tele.tof_left;
    tof["centre"] = tele.tof_centre;
    tof["right"] = tele.tof_right;
    doc["obstacle_detected"] = isObstacleDetected();
    doc["obstacle_stop_dist"] = sysSettings.tof_stop_distance_mm;

    // Motor Speeds
    JsonObject motor = doc["motor"].to<JsonObject>();
    motor["left_rpm"] = tele.left_rpm;
    motor["right_rpm"] = tele.right_rpm;
    motor["target_rpm"] = tele.target_rpm;
    motor["target_l_rpm"] = tele.target_l_rpm;
    motor["target_r_rpm"] = tele.target_r_rpm;
    doc["enc_l_rpm"] = tele.left_rpm;
    doc["enc_r_rpm"] = tele.right_rpm;
    doc["target_l_rpm"] = tele.target_l_rpm;
    doc["target_r_rpm"] = tele.target_r_rpm;
    doc["enc_l_mms"] = tele.left_mms;
    doc["enc_r_mms"] = tele.right_mms;
    doc["raw_enc_l"] = tele.raw_enc_l;
    doc["raw_enc_r"] = tele.raw_enc_r;

    // System Health & Diagnostics
    SystemHealth health = getSystemHealth();
    JsonObject healthObj = doc["health"].to<JsonObject>();
    healthObj["esp32"] = (int)health.esp32;
    healthObj["stm32"] = (int)health.stm32;
    healthObj["motor"] = (int)health.motor;
    healthObj["encoder"] = (int)health.encoder;
    healthObj["mpu6050"] = (int)health.mpu6050;
    healthObj["rfid"] = (int)health.rfid;
    healthObj["tof"] = (int)health.tof;
    healthObj["battery"] = (int)health.battery;
    healthObj["uart"] = (int)health.uart;

    // Uptime and Heap
    doc["uptime"] = getSystemUptimeS();
    doc["heap"] = getFreeHeap();
    doc["rssi"] = getWifiRSSI();

    // Dedicated High-Power AP IP Reporting
    doc["ap_ip"] = WiFi.softAPIP().toString();
    doc["sta_ip"] = "Disabled";
    doc["sta_connected"] = false;
    doc["sta_ssid"] = "";

    size_t fsTotal = 0, fsUsed = 0;
    getLittleFSInfo(fsTotal, fsUsed);
    doc["fs_total"] = fsTotal;
    doc["fs_used"] = fsUsed;

    // ESP32-S3 Hardware Performance Profiler Metrics
    doc["cpu_mhz"] = getCpuFreqMHz();
    doc["min_heap"] = getMinFreeHeap();
    doc["max_alloc"] = getMaxAllocHeap();
    doc["loop_ms"] = getAverageLoopMs();
    doc["loop_hz"] = getLoopHz();
    doc["cpu_usage"] = getCpuUsagePct();
    doc["ram_usage"] = getRamUsagePct();
    doc["esp_temp"] = getEsp32TempC();

    // Active Route Information
    doc["active_route"] = getActiveRouteName();
    doc["checkpoint_idx"] = getCheckpointIndex();
    doc["checkpoint_total"] = getTotalCheckpoints();
    doc["mission_progress"] = round(getMissionProgress());

    // Current Active Error
    SystemError err = getCurrentError();
    JsonObject errObj = doc["active_error"].to<JsonObject>();
    errObj["active"] = err.active;
    if (err.active) {
        errObj["timestamp"] = err.timestamp;
        errObj["source"] = err.source;
        errObj["code"] = err.code;
        errObj["description"] = err.description;
    }

    // Live Web Serial Console log buffer (limit to latest 12 entries in telemetry stream to save bandwidth)
    JsonDocument logsDoc = getWebSerialLogs();
    JsonArray logsArr = logsDoc.as<JsonArray>();
    JsonArray targetArr = doc["serial_logs"].to<JsonArray>();
    size_t totalLogs = logsArr.size();
    size_t startIdx = (totalLogs > 12) ? (totalLogs - 12) : 0;
    for (size_t i = startIdx; i < totalLogs; i++) {
        targetArr.add(logsArr[i]);
    }

    // Serialize and broadcast
    String buffer;
    buffer.reserve(1024);
    serializeJson(doc, buffer);
    ws.textAll(buffer);
}

bool isWebSocketClientConnected() {
    if (ENABLE_WEBSOCKET) {
        return ws.count() > 0;
    }
    return false;
}

void cleanupWebSocketClients() {
    if (ENABLE_WEBSOCKET) {
        ws.cleanupClients();
    }
}
