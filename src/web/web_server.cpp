#include "web/web_server.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "web/websocket.h"
#include "system/system_manager.h"
#include "routes/route_manager.h"
#include "rfid/rfid_manager.h"
#include "errors/error_logger.h"
#include "demo/demo_manager.h"
#include "communication/uart_manager.h"

static AsyncWebServer server(80);

// Helper to send CORS headers and JSON response
static void sendJsonResponse(AsyncWebServerRequest *request, const JsonDocument &doc, int code = 200) {
    String responseBody;
    serializeJson(doc, responseBody);
    AsyncWebServerResponse *response = request->beginResponse(code, "application/json", responseBody);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

static void sendSuccessResponse(AsyncWebServerRequest *request, const String &message = "Success") {
    JsonDocument doc;
    doc["status"] = "success";
    doc["message"] = message;
    sendJsonResponse(request, doc);
}

static void sendErrorResponse(AsyncWebServerRequest *request, const String &error, int code = 400) {
    JsonDocument doc;
    doc["status"] = "error";
    doc["message"] = error;
    sendJsonResponse(request, doc, code);
}

void webServerInit() {
    // ------------------------------------------------------------------------
    // OPTIONS Preflight Handler for CORS (Web clients running locally)
    // ------------------------------------------------------------------------
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    
    server.on("^.*$", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "");
        response->addHeader("Access-Control-Max-Age", "86400");
        request->send(response);
    });

    // ------------------------------------------------------------------------
    // REST API ENDPOINTS
    // ------------------------------------------------------------------------

    // GET /api/status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["mode"] = (getAGVMode() == MODE_TEACH) ? "TEACH" : ((getAGVMode() == MODE_REPEAT) ? "REPEAT" : "IDLE");
        AGVState state = getAGVState();
        String stateStr = "STOPPED";
        if (state == STATE_RUNNING) stateStr = "RUNNING";
        else if (state == STATE_PAUSED) stateStr = "PAUSED";
        else if (state == STATE_RECORDING) stateStr = "RECORDING";
        else if (state == STATE_COMPLETE) stateStr = "COMPLETE";
        else if (state == STATE_ERROR) stateStr = "ERROR";
        doc["state"] = stateStr;

        TelemetryData tele = getTelemetry();
        doc["x"] = tele.x;
        doc["y"] = tele.y;
        doc["heading"] = tele.heading;
        doc["speed"] = tele.speed;
        doc["rfid"] = tele.rfid;
        doc["rfid_name"] = getRFIDTagName(tele.rfid);
        doc["last_rfid"] = getLastScannedRFID();
        doc["last_rfid_name"] = getRFIDTagName(getLastScannedRFID());
        doc["rfid_hw_ok"] = isRFIDReaderHardwareReady();
        doc["enc_l_rpm"] = tele.left_rpm;
        doc["enc_r_rpm"] = tele.right_rpm;
        doc["enc_l_mms"] = tele.left_mms;
        doc["enc_r_mms"] = tele.right_mms;

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

        sendJsonResponse(request, doc);
    });

    // GET /api/routes
    server.on("/api/routes", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc = listRoutes();
        sendJsonResponse(request, doc);
    });

    // GET /api/route?id=<routeId>
    server.on("/api/route", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("id")) {
            sendErrorResponse(request, "Missing route ID");
            return;
        }
        String routeId = request->getParam("id")->value();
        String path = "/routes/" + routeId + ".json";
        if (!LittleFS.exists(path)) {
            sendErrorResponse(request, "Route not found");
            return;
        }
        request->send(LittleFS, path, "application/json");
    });

    // GET /api/rfid
    server.on("/api/rfid", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc = getRFIDTagsJSON();
        sendJsonResponse(request, doc);
    });

    // GET /api/errors
    server.on("/api/errors", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc = getErrorStatistics();
        String logContent = readLogFile(getTodayLogPath());
        doc["log"] = logContent;
        sendJsonResponse(request, doc);
    });

    // GET /api/system
    server.on("/api/system", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["uptime"] = getSystemUptimeS();
        doc["heap"] = getFreeHeap();
        doc["rssi"] = getWifiRSSI();

        size_t fsTotal = 0, fsUsed = 0;
        getLittleFSInfo(fsTotal, fsUsed);
        doc["fs_total"] = fsTotal;
        doc["fs_used"] = fsUsed;

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

        // Dedicated AP IP reporting (Router STA disabled)
        doc["ap_ip"] = WiFi.softAPIP().toString();
        doc["sta_ip"] = "Disabled";
        doc["sta_connected"] = false;
        doc["sta_ssid"] = "";

        sendJsonResponse(request, doc);
    });

    // GET /api/wifi/scan (Disabled in Dedicated AP-only High Power mode)
    server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc.to<JsonArray>();
        sendJsonResponse(request, doc);
    });

    // POST /api/wifi/connect (Disabled - Dedicated AP-only High Power mode)
    server.on("/api/wifi/connect", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        sendErrorResponse(request, "Router (STA) connection is disabled. Dedicated High-Power Hotspot mode is active.");
    });

    // POST /api/teach/start
    server.on("/api/teach/start", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err) {
            sendErrorResponse(request, "Invalid JSON payload");
            return;
        }

        String name = doc["name"] | "New Route";
        String start = doc["start"] | "";
        String destination = doc["destination"] | "";
        String description = doc["description"] | "";

        if (name.length() < 3) {
            sendErrorResponse(request, "Route name too short (min 3 chars)");
            return;
        }

        String errMsg = "";
        if (startTeaching(name, start, destination, description, errMsg)) {
            sendSuccessResponse(request, "Train mode started. Drive AGV to destination.");
        } else {
            sendErrorResponse(request, errMsg.length() > 0 ? errMsg.c_str() : "Cannot start Train mode: Place AGV on an RFID tag first.");
        }
    });

    // POST /api/teach/stop
    server.on("/api/teach/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
        String errMsg = "";
        if (stopRecordingAndSave(errMsg)) {
            sendSuccessResponse(request, "Train Mode complete! Route saved successfully.");
        } else {
            sendErrorResponse(request, errMsg.length() > 0 ? errMsg.c_str() : "Cannot end Train Mode: Destination RFID tag not detected!");
        }
    });

    // POST /api/teach/cancel
    server.on("/api/teach/cancel", HTTP_POST, [](AsyncWebServerRequest *request) {
        cancelRecording();
        sendSuccessResponse(request, "Train mode cancelled.");
    });

    // POST /api/route/delete
    server.on("/api/route/delete", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err) {
            sendErrorResponse(request, "Invalid JSON payload");
            return;
        }

        String routeId = doc["id"] | "";
        if (routeId == "") {
            sendErrorResponse(request, "Route ID required");
            return;
        }

        if (deleteRoute(routeId)) {
            sendSuccessResponse(request, "Route deleted");
        } else {
            sendErrorResponse(request, "Failed to delete route");
        }
    });

    // POST /api/repeat/start
    server.on("/api/repeat/start", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err) {
            sendErrorResponse(request, "Invalid JSON payload");
            return;
        }

        int mode = doc["mode"] | 0;
        bool success = false;

        if (mode == 0) {
            String routeId = doc["id"] | "";
            if (routeId == "") {
                sendErrorResponse(request, "Route ID required");
                return;
            }
            success = startRepeating(routeId);
        } else {
            String startRfid = doc["start_rfid"] | "";
            String destRfid = doc["dest_rfid"] | "";
            if (startRfid == "" || destRfid == "") {
                sendErrorResponse(request, "Start and Dest RFIDs required");
                return;
            }
            success = startRepeatingShortestPath(startRfid, destRfid);
        }

        if (success) {
            sendSuccessResponse(request, "Mission started");
        } else {
            sendErrorResponse(request, "Cannot start repeat: Required hardware disabled or route not found");
        }
    });

    // POST /api/repeat/pause
    server.on("/api/repeat/pause", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (pauseRepeating()) {
            sendSuccessResponse(request, "Mission paused");
        } else {
            sendErrorResponse(request, "Failed to pause mission");
        }
    });

    // POST /api/repeat/resume
    server.on("/api/repeat/resume", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (resumeRepeating()) {
            sendSuccessResponse(request, "Mission resumed");
        } else {
            sendErrorResponse(request, "Failed to resume mission");
        }
    });

    // POST /api/repeat/stop
    server.on("/api/repeat/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (stopRepeating()) {
            sendSuccessResponse(request, "Mission stopped");
        } else {
            sendErrorResponse(request, "Failed to stop mission");
        }
    });

    // POST /api/manual/move
    server.on("/api/manual/move", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err) {
            sendErrorResponse(request, "Invalid JSON payload");
            return;
        }

        String direction = doc["direction"] | "STOP";
        int speedRpm = doc["speed"] | 50; // Now interpreted as Target RPM

        // Allow 10 to 400 RPM
        if (speedRpm < 10 || speedRpm > 400) {
            sendErrorResponse(request, "Target RPM must be between 10 and 400");
            return;
        }

        if (direction != "FORWARD" && direction != "REVERSE" && direction != "LEFT" && direction != "RIGHT" && direction != "STOP") {
            sendErrorResponse(request, "Invalid movement direction");
            return;
        }

        if (direction == "STOP") {
            sendStm32Stop();
            handleDemoManualStop();
        } else {
            // Map Target RPM to mm/s for STM32 Move Command
            uint16_t speedMmS = (uint16_t)((speedRpm * sysSettings.wheel_circ_mm) / 60.0f);
            
            // Map Target RPM to deg/s for STM32 Turn Command (STM32 does turn_rpm = speedDegS * 1.5)
            uint16_t speedDegS = (uint16_t)(speedRpm / 1.5f);
            
            if (direction == "FORWARD") {
                sendStm32Move(10000, speedMmS); // Drive forward distance (10m)
            } else if (direction == "REVERSE") {
                sendStm32Move(-10000, speedMmS); // Drive reverse
            } else if (direction == "LEFT") {
                sendStm32Turn(-3600, speedDegS); // Turn left (negative yaw)
            } else if (direction == "RIGHT") {
                sendStm32Turn(3600, speedDegS); // Turn right (positive yaw)
            }
            
            handleDemoManualMove(direction, speedRpm);
        }
        sendSuccessResponse(request, "Movement command sent");
    });

    // POST /api/manual/stop
    server.on("/api/manual/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
        sendStm32Stop();
        handleDemoManualStop();
        sendSuccessResponse(request, "Stop command sent");
    });

    // POST /api/rfid/add
    server.on("/api/rfid/add", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err) {
            sendErrorResponse(request, "Invalid JSON payload");
            return;
        }

        String uid = doc["uid"] | "";
        String name = doc["name"] | "";
        String location = doc["location"] | "";

        if (uid == "" || name == "") {
            sendErrorResponse(request, "UID and Name are required");
            return;
        }

        if (addOrUpdateRFIDTag(uid, name, location)) {
            sendSuccessResponse(request, "RFID tag registered");
        } else {
            sendErrorResponse(request, "Failed to register RFID tag");
        }
    });

    // POST /api/rfid/scan
    server.on("/api/rfid/scan", HTTP_POST, [](AsyncWebServerRequest *request) {
        startRFIDScan();
        sendSuccessResponse(request, "RFID scan simulated");
    });

    // POST /api/rfid/delete
    server.on("/api/rfid/delete", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err) {
            sendErrorResponse(request, "Invalid JSON payload");
            return;
        }

        String uid = doc["uid"] | "";
        if (uid == "") {
            sendErrorResponse(request, "RFID card UID required");
            return;
        }

        if (deleteRFIDTag(uid)) {
            sendSuccessResponse(request, "RFID tag deleted");
        } else {
            sendErrorResponse(request, "Failed to delete RFID tag");
        }
    });

    // GET /api/settings
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["test_profile"] = sysSettings.test_profile;

        doc["enable_dashboard"] = sysSettings.enable_dashboard;
        doc["enable_teach_mode"] = sysSettings.enable_teach_mode;
        doc["enable_repeat_mode"] = sysSettings.enable_repeat_mode;
        doc["enable_manual_control"] = sysSettings.enable_manual_control;
        doc["enable_route_manager"] = sysSettings.enable_route_manager;
        doc["enable_rfid_manager"] = sysSettings.enable_rfid_manager;
        doc["enable_error_log"] = sysSettings.enable_error_log;
        doc["enable_system_info"] = sysSettings.enable_system_info;

        doc["nav_mode"] = sysSettings.nav_mode;
        doc["drive_method"] = sysSettings.drive_method;
        doc["lookahead_distance"] = sysSettings.lookahead_distance;

        doc["enable_stm32_uart"] = sysSettings.enable_stm32_uart;
        doc["enable_websocket"] = sysSettings.enable_websocket;

        doc["enable_motor_control"] = sysSettings.enable_motor_control;
        doc["enable_encoder"] = sysSettings.enable_encoder;
        doc["enable_mpu6050"] = sysSettings.enable_mpu6050;
        doc["enable_pid"] = sysSettings.enable_pid;

        doc["wheel_circ_mm"] = sysSettings.wheel_circ_mm;
        doc["enc_ppr_l"] = sysSettings.enc_ppr_l;
        doc["enc_ppr_r"] = sysSettings.enc_ppr_r;

        doc["pid_kp_l"] = sysSettings.pid_kp_l;
        doc["pid_ki_l"] = sysSettings.pid_ki_l;
        doc["pid_kd_l"] = sysSettings.pid_kd_l;
        doc["pid_kp_r"] = sysSettings.pid_kp_r;
        doc["pid_ki_r"] = sysSettings.pid_ki_r;
        doc["pid_kd_r"] = sysSettings.pid_kd_r;

        doc["motor_l_fwd_scale"] = sysSettings.motor_l_fwd_scale;
        doc["motor_r_fwd_scale"] = sysSettings.motor_r_fwd_scale;
        doc["motor_l_turn_scale"] = sysSettings.motor_l_turn_scale;
        doc["motor_r_turn_scale"] = sysSettings.motor_r_turn_scale;

        doc["rfid_reader"] = sysSettings.rfid_reader;
        doc["rfid_manager_flag"] = sysSettings.rfid_manager_flag;
        doc["rfid_checkpoints"] = sysSettings.rfid_checkpoints;

        doc["tof_sensors"] = sysSettings.tof_sensors;
        doc["left_tof"] = sysSettings.left_tof;
        doc["centre_tof"] = sysSettings.centre_tof;
        doc["right_tof"] = sysSettings.right_tof;
        doc["obstacle_detection"] = sysSettings.obstacle_detection;
        doc["tof_stop_distance_mm"] = sysSettings.tof_stop_distance_mm;

        doc["battery_monitoring"] = sysSettings.battery_monitoring;
        doc["ina219"] = sysSettings.ina219;
        doc["voltage_monitoring"] = sysSettings.voltage_monitoring;
        doc["current_monitoring"] = sysSettings.current_monitoring;
        doc["power_monitoring"] = sysSettings.power_monitoring;
        doc["battery_percentage"] = sysSettings.battery_percentage;
        doc["low_battery_warning"] = sysSettings.low_battery_warning;
        doc["critical_battery_warning"] = sysSettings.critical_battery_warning;
        doc["battery_fault_detection"] = sysSettings.battery_fault_detection;
        doc["charging_status"] = sysSettings.charging_status;

        doc["battery_max_voltage"] = sysSettings.battery_max_voltage;
        doc["battery_min_voltage"] = sysSettings.battery_min_voltage;
        doc["low_voltage"] = sysSettings.low_voltage;
        doc["critical_voltage"] = sysSettings.critical_voltage;
        doc["low_battery_pct"] = sysSettings.low_battery_pct;
        doc["critical_battery_pct"] = sysSettings.critical_battery_pct;

        doc["demo_mode"] = sysSettings.demo_mode;
        doc["wifi_sta_enabled"] = false;
        doc["wifi_ssid"] = sysSettings.wifi_ssid;
        doc["wifi_password"] = sysSettings.wifi_password;

        sendJsonResponse(request, doc);
    });

    // POST /api/settings
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err) {
            sendErrorResponse(request, "Invalid JSON payload");
            return;
        }

        if (doc.containsKey("test_profile")) {
            int prof = doc["test_profile"].as<int>();
            sysSettings.test_profile = prof;
            if (prof != PROFILE_CUSTOM) {
                applyProfileDefaults(prof);
            }
        }

        // Apply hardware trims regardless of profile
        if (doc.containsKey("motor_l_fwd_scale")) sysSettings.motor_l_fwd_scale = doc["motor_l_fwd_scale"].as<uint16_t>();
        if (doc.containsKey("motor_r_fwd_scale")) sysSettings.motor_r_fwd_scale = doc["motor_r_fwd_scale"].as<uint16_t>();
        if (doc.containsKey("motor_l_turn_scale")) sysSettings.motor_l_turn_scale = doc["motor_l_turn_scale"].as<uint16_t>();
        if (doc.containsKey("motor_r_turn_scale")) sysSettings.motor_r_turn_scale = doc["motor_r_turn_scale"].as<uint16_t>();

        if (doc.containsKey("wheel_circ_mm")) sysSettings.wheel_circ_mm = doc["wheel_circ_mm"].as<float>();
        if (doc.containsKey("enc_ppr_l")) sysSettings.enc_ppr_l = doc["enc_ppr_l"].as<uint16_t>();
        if (doc.containsKey("enc_ppr_r")) sysSettings.enc_ppr_r = doc["enc_ppr_r"].as<uint16_t>();
        
        if (doc.containsKey("pid_kp_l")) sysSettings.pid_kp_l = doc["pid_kp_l"].as<float>();
        if (doc.containsKey("pid_ki_l")) sysSettings.pid_ki_l = doc["pid_ki_l"].as<float>();
        if (doc.containsKey("pid_kd_l")) sysSettings.pid_kd_l = doc["pid_kd_l"].as<float>();
        if (doc.containsKey("pid_kp_r")) sysSettings.pid_kp_r = doc["pid_kp_r"].as<float>();
        if (doc.containsKey("pid_ki_r")) sysSettings.pid_ki_r = doc["pid_ki_r"].as<float>();
        if (doc.containsKey("pid_kd_r")) sysSettings.pid_kd_r = doc["pid_kd_r"].as<float>();

        // Apply changes individually if profile is CUSTOM or was overridden
        if (sysSettings.test_profile == PROFILE_CUSTOM) {
            if (doc.containsKey("enable_dashboard")) sysSettings.enable_dashboard = doc["enable_dashboard"];
            if (doc.containsKey("enable_teach_mode")) sysSettings.enable_teach_mode = doc["enable_teach_mode"];
            if (doc.containsKey("enable_repeat_mode")) sysSettings.enable_repeat_mode = doc["enable_repeat_mode"];
            if (doc.containsKey("enable_manual_control")) sysSettings.enable_manual_control = doc["enable_manual_control"];
            if (doc.containsKey("enable_route_manager")) sysSettings.enable_route_manager = doc["enable_route_manager"];
            if (doc.containsKey("enable_rfid_manager")) sysSettings.enable_rfid_manager = doc["enable_rfid_manager"];
            if (doc.containsKey("enable_error_log")) sysSettings.enable_error_log = doc["enable_error_log"];
            if (doc.containsKey("enable_system_info")) sysSettings.enable_system_info = doc["enable_system_info"];

            if (doc.containsKey("nav_mode")) sysSettings.nav_mode = doc["nav_mode"].as<int>();
            if (doc.containsKey("drive_method")) sysSettings.drive_method = doc["drive_method"].as<int>();
            if (doc.containsKey("lookahead_distance")) sysSettings.lookahead_distance = doc["lookahead_distance"].as<float>();

            if (doc.containsKey("enable_stm32_uart")) sysSettings.enable_stm32_uart = doc["enable_stm32_uart"];
            if (doc.containsKey("enable_websocket")) sysSettings.enable_websocket = doc["enable_websocket"];

            if (doc.containsKey("enable_motor_control")) sysSettings.enable_motor_control = doc["enable_motor_control"];
            if (doc.containsKey("enable_encoder")) sysSettings.enable_encoder = doc["enable_encoder"];
            if (doc.containsKey("enable_mpu6050")) sysSettings.enable_mpu6050 = doc["enable_mpu6050"];
            if (doc.containsKey("enable_pid")) sysSettings.enable_pid = doc["enable_pid"];

            if (doc.containsKey("rfid_reader")) sysSettings.rfid_reader = doc["rfid_reader"];
            if (doc.containsKey("rfid_manager_flag")) sysSettings.rfid_manager_flag = doc["rfid_manager_flag"];
            if (doc.containsKey("rfid_checkpoints")) sysSettings.rfid_checkpoints = doc["rfid_checkpoints"];

            if (doc.containsKey("tof_sensors")) sysSettings.tof_sensors = doc["tof_sensors"];
            if (doc.containsKey("left_tof")) sysSettings.left_tof = doc["left_tof"];
            if (doc.containsKey("centre_tof")) sysSettings.centre_tof = doc["centre_tof"];
            if (doc.containsKey("right_tof")) sysSettings.right_tof = doc["right_tof"];
            if (doc.containsKey("obstacle_detection")) sysSettings.obstacle_detection = doc["obstacle_detection"];
            if (doc.containsKey("tof_stop_distance_mm")) sysSettings.tof_stop_distance_mm = doc["tof_stop_distance_mm"].as<float>();

            if (doc.containsKey("battery_monitoring")) sysSettings.battery_monitoring = doc["battery_monitoring"];
            if (doc.containsKey("ina219")) sysSettings.ina219 = doc["ina219"];
            if (doc.containsKey("voltage_monitoring")) sysSettings.voltage_monitoring = doc["voltage_monitoring"];
            if (doc.containsKey("current_monitoring")) sysSettings.current_monitoring = doc["current_monitoring"];
            if (doc.containsKey("power_monitoring")) sysSettings.power_monitoring = doc["power_monitoring"];
            if (doc.containsKey("battery_percentage")) sysSettings.battery_percentage = doc["battery_percentage"];
            if (doc.containsKey("low_battery_warning")) sysSettings.low_battery_warning = doc["low_battery_warning"];
            if (doc.containsKey("critical_battery_warning")) sysSettings.critical_battery_warning = doc["critical_battery_warning"];
            if (doc.containsKey("battery_fault_detection")) sysSettings.battery_fault_detection = doc["battery_fault_detection"];
            if (doc.containsKey("charging_status")) sysSettings.charging_status = doc["charging_status"];
        }

        // Threshold values can always be set
        if (doc.containsKey("battery_max_voltage")) sysSettings.battery_max_voltage = doc["battery_max_voltage"].as<float>();
        if (doc.containsKey("battery_min_voltage")) sysSettings.battery_min_voltage = doc["battery_min_voltage"].as<float>();
        if (doc.containsKey("low_voltage")) sysSettings.low_voltage = doc["low_voltage"].as<float>();
        if (doc.containsKey("critical_voltage")) sysSettings.critical_voltage = doc["critical_voltage"].as<float>();
        if (doc.containsKey("low_battery_pct")) sysSettings.low_battery_pct = doc["low_battery_pct"].as<int>();
        if (doc.containsKey("critical_battery_pct")) sysSettings.critical_battery_pct = doc["critical_battery_pct"].as<int>();

        if (doc.containsKey("demo_mode")) sysSettings.demo_mode = doc["demo_mode"];

        sysSettings.wifi_sta_enabled = false;

        saveSettings();

        // Broadcast updated motor trims to the STM32 via UART
        if (sysSettings.enable_stm32_uart && !sysSettings.demo_mode) {
            sendStm32MotorTrim(
                sysSettings.motor_l_fwd_scale,
                sysSettings.motor_r_fwd_scale,
                sysSettings.motor_l_turn_scale,
                sysSettings.motor_r_turn_scale
            );
            sendStm32EncoderConfig(
                sysSettings.wheel_circ_mm,
                sysSettings.enc_ppr_l,
                sysSettings.enc_ppr_r
            );
            sendStm32PidTuning(
                sysSettings.pid_kp_l, sysSettings.pid_ki_l, sysSettings.pid_kd_l,
                sysSettings.pid_kp_r, sysSettings.pid_ki_r, sysSettings.pid_kd_r
            );
            sendStm32PidEnable(sysSettings.enable_pid);
            sendStm32TofConfig(sysSettings.tof_stop_distance_mm);
        }

        sendSuccessResponse(request, "Settings updated successfully");
    });

    // POST /api/errors/clear
    server.on("/api/errors/clear", HTTP_POST, [](AsyncWebServerRequest *request) {
        clearTodayLog();
        sendSuccessResponse(request, "Error logs cleared");
    });

    // POST /api/serial/command
    server.on("/api/serial/command", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err) {
            sendErrorResponse(request, "Invalid JSON payload");
            return;
        }

        String command = doc["command"] | "";
        if (command.length() == 0) {
            sendErrorResponse(request, "Command cannot be empty");
            return;
        }

        handleSerialCommand(command);
        sendSuccessResponse(request, "Command received");
    });

    // POST /api/system/wipe
    server.on("/api/system/wipe", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err) {
            sendErrorResponse(request, "Invalid JSON payload");
            return;
        }

        String password = doc["password"] | "";
        if (password != "1234") {
            sendErrorResponse(request, "Unauthorized: Invalid administrative password", 401);
            return;
        }

        // Wipe routes
        {
            File dir = LittleFS.open("/routes");
            if (dir && dir.isDirectory()) {
                File file = dir.openNextFile();
                while (file) {
                    String path = String("/routes/") + file.name();
                    file.close();
                    LittleFS.remove(path);
                    file = dir.openNextFile();
                }
            }
        }

        // Wipe logs
        {
            File dir = LittleFS.open("/logs");
            if (dir && dir.isDirectory()) {
                File file = dir.openNextFile();
                while (file) {
                    String path = String("/logs/") + file.name();
                    file.close();
                    LittleFS.remove(path);
                    file = dir.openNextFile();
                }
            }
        }

        // Wipe RFID database
        if (LittleFS.exists("/rfid/tags.json")) {
            LittleFS.remove("/rfid/tags.json");
        }

        // Wipe config settings
        if (LittleFS.exists("/config/settings.json")) {
            LittleFS.remove("/config/settings.json");
        }

        sendSuccessResponse(request, "System data wiped successfully. Rebooting...");

        // Restart ESP32 after delay
        request->onDisconnect([]() {
            delay(1000);
            ESP.restart();
        });
    });

    // ------------------------------------------------------------------------
    // WEB SERVER STATIC FILES FROM LittleFS
    // ------------------------------------------------------------------------
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("max-age=86400");

    // Init websockets
    webSocketInit(&server);

    // Launch server
    server.begin();
    Serial.println("Webserver: Started successfully on port 80");
}
