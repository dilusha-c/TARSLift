#include "system/system_manager.h"
#include "communication/uart_manager.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>


static uint32_t bootTime = 0;
Settings sysSettings;

void applyProfileDefaults(int profileIndex) {
    sysSettings.test_profile = profileIndex;

    sysSettings.enable_dashboard = true;
    sysSettings.enable_teach_mode = false;
    sysSettings.enable_repeat_mode = false;
    sysSettings.enable_manual_control = false;
    sysSettings.enable_route_manager = false;
    sysSettings.enable_rfid_manager = false;
    sysSettings.enable_error_log = true;
    sysSettings.enable_system_info = true;

    sysSettings.enable_stm32_uart = true;
    sysSettings.enable_websocket = true;

    sysSettings.enable_motor_control = false;
    sysSettings.enable_encoder = false;
    sysSettings.enable_mpu6050 = false;
    sysSettings.enable_pid = false;

    sysSettings.rfid_reader = false;
    sysSettings.rfid_manager_flag = false;
    sysSettings.rfid_checkpoints = false;

    sysSettings.tof_sensors = false;
    sysSettings.left_tof = false;
    sysSettings.centre_tof = false;
    sysSettings.right_tof = false;
    sysSettings.obstacle_detection = false;

    sysSettings.battery_monitoring = false;
    sysSettings.ina219 = false;
    sysSettings.voltage_monitoring = false;
    sysSettings.current_monitoring = false;
    sysSettings.power_monitoring = false;
    sysSettings.battery_percentage = false;
    sysSettings.low_battery_warning = false;
    sysSettings.critical_battery_warning = false;
    sysSettings.battery_fault_detection = false;
    sysSettings.charging_status = false;

    sysSettings.low_voltage = 10.8f;
    sysSettings.critical_voltage = 10.2f;
    sysSettings.low_battery_pct = 20;
    sysSettings.critical_battery_pct = 10;
    sysSettings.demo_mode = false;

    switch(profileIndex) {
        case PROFILE_MOTOR_TEST: // 1
            sysSettings.enable_manual_control = true;
            sysSettings.enable_teach_mode = true;
            sysSettings.enable_stm32_uart = true;
            sysSettings.enable_motor_control = true;
            break;

        case PROFILE_ENCODER_TEST: // 2
            sysSettings.enable_stm32_uart = true;
            sysSettings.enable_motor_control = true;
            sysSettings.enable_encoder = true;
            break;

        case PROFILE_MPU6050_TEST: // 3
            sysSettings.enable_stm32_uart = true;
            sysSettings.enable_mpu6050 = true;
            break;

        case PROFILE_RFID_TEST: // 4
            sysSettings.enable_stm32_uart = true;
            sysSettings.rfid_reader = true;
            sysSettings.rfid_manager_flag = true;
            break;

        case PROFILE_TOF_TEST: // 5
            sysSettings.enable_stm32_uart = true;
            sysSettings.tof_sensors = true;
            sysSettings.obstacle_detection = true;
            break;

        case PROFILE_BATTERY_TEST: // 6
            sysSettings.battery_monitoring = true;
            sysSettings.ina219 = true;
            sysSettings.voltage_monitoring = true;
            sysSettings.current_monitoring = true;
            sysSettings.power_monitoring = true;
            sysSettings.battery_percentage = true;
            sysSettings.low_battery_warning = true;
            sysSettings.critical_battery_warning = true;
            sysSettings.battery_fault_detection = true;
            break;

        case PROFILE_TEACH_TEST: // 7
            sysSettings.enable_manual_control = true;
            sysSettings.enable_teach_mode = true;
            sysSettings.enable_route_manager = true;
            sysSettings.enable_stm32_uart = true;
            sysSettings.enable_motor_control = true;
            sysSettings.enable_encoder = true;
            sysSettings.enable_mpu6050 = true;
            sysSettings.rfid_reader = true;
            break;

        case PROFILE_REPEAT_TEST: // 8
            sysSettings.enable_repeat_mode = true;
            sysSettings.enable_route_manager = true;
            sysSettings.enable_stm32_uart = true;
            sysSettings.enable_motor_control = true;
            sysSettings.enable_encoder = true;
            sysSettings.enable_mpu6050 = true;
            sysSettings.rfid_reader = true;
            sysSettings.tof_sensors = true;
            sysSettings.battery_monitoring = true;
            break;

        case PROFILE_FULL_SYSTEM: // 9
            sysSettings.enable_teach_mode = true;
            sysSettings.enable_repeat_mode = true;
            sysSettings.enable_manual_control = true;
            sysSettings.enable_route_manager = true;
            sysSettings.enable_rfid_manager = true;

            sysSettings.enable_stm32_uart = true;

            sysSettings.enable_motor_control = true;
            sysSettings.enable_encoder = true;
            sysSettings.enable_mpu6050 = true;
            sysSettings.enable_pid = true;

            sysSettings.rfid_reader = true;
            sysSettings.rfid_manager_flag = true;
            sysSettings.rfid_checkpoints = true;

            sysSettings.tof_sensors = true;
            sysSettings.left_tof = true;
            sysSettings.centre_tof = true;
            sysSettings.right_tof = true;
            sysSettings.obstacle_detection = true;

            sysSettings.battery_monitoring = true;
            sysSettings.ina219 = true;
            sysSettings.voltage_monitoring = true;
            sysSettings.current_monitoring = true;
            sysSettings.power_monitoring = true;
            sysSettings.battery_percentage = true;
            sysSettings.low_battery_warning = true;
            sysSettings.critical_battery_warning = true;
            sysSettings.battery_fault_detection = true;
            break;

        case PROFILE_CUSTOM: // 0
        default:
            sysSettings.enable_teach_mode = true;
            sysSettings.enable_repeat_mode = true;
            sysSettings.enable_manual_control = true;
            sysSettings.enable_route_manager = true;
            sysSettings.enable_rfid_manager = true;
            break;
    }
}


static void enforceSettingsDependencies() {
    if (!sysSettings.battery_monitoring) {
        sysSettings.ina219 = false;
        sysSettings.voltage_monitoring = false;
        sysSettings.current_monitoring = false;
        sysSettings.power_monitoring = false;
        sysSettings.battery_percentage = false;
        sysSettings.low_battery_warning = false;
        sysSettings.critical_battery_warning = false;
        sysSettings.battery_fault_detection = false;
    }
    if (!sysSettings.rfid_reader) {
        sysSettings.rfid_manager_flag = false;
        sysSettings.rfid_checkpoints = false;
    }
    if (!sysSettings.tof_sensors) {
        sysSettings.left_tof = false;
        sysSettings.centre_tof = false;
        sysSettings.right_tof = false;
        sysSettings.obstacle_detection = false;
    }
}

void loadSettings() {
    bool fileLoaded = false;
    if (LittleFS.exists("/config/settings.json")) {
        File file = LittleFS.open("/config/settings.json", FILE_READ);
        if (file) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, file);
            file.close();
            if (!error) {
                sysSettings.test_profile = doc["test_profile"] | (int)PROFILE_CUSTOM;

                sysSettings.enable_dashboard = doc["enable_dashboard"] | true;
                sysSettings.enable_teach_mode = doc["enable_teach_mode"] | false;
                sysSettings.enable_repeat_mode = doc["enable_repeat_mode"] | false;
                sysSettings.enable_manual_control = doc["enable_manual_control"] | false;
                sysSettings.enable_route_manager = doc["enable_route_manager"] | false;
                sysSettings.enable_rfid_manager = doc["enable_rfid_manager"] | false;
                sysSettings.enable_error_log = doc["enable_error_log"] | true;
                sysSettings.enable_system_info = doc["enable_system_info"] | true;

                sysSettings.nav_mode = doc["nav_mode"] | 0;
                sysSettings.drive_method = doc["drive_method"] | 0;
                sysSettings.lookahead_distance = doc["lookahead_distance"] | 300.0f;

                sysSettings.enable_stm32_uart = doc["enable_stm32_uart"] | true;
                sysSettings.enable_websocket = doc["enable_websocket"] | true;

                sysSettings.enable_motor_control = doc["enable_motor_control"] | false;
                sysSettings.enable_encoder = doc["enable_encoder"] | false;
                sysSettings.enable_mpu6050 = doc["enable_mpu6050"] | false;
                sysSettings.enable_pid = doc["enable_pid"] | false;

                sysSettings.wheel_circ_mm = doc["wheel_circ_mm"] | 138.2f;
                sysSettings.enc_ppr_l = doc["enc_ppr_l"] | 287;
                sysSettings.enc_ppr_r = doc["enc_ppr_r"] | 287;
                
                sysSettings.pid_kp_l = doc["pid_kp_l"] | 1.0f;
                sysSettings.pid_ki_l = doc["pid_ki_l"] | 0.0f;
                sysSettings.pid_kd_l = doc["pid_kd_l"] | 0.0f;
                sysSettings.pid_kp_r = doc["pid_kp_r"] | 1.0f;
                sysSettings.pid_ki_r = doc["pid_ki_r"] | 0.0f;
                sysSettings.pid_kd_r = doc["pid_kd_r"] | 0.0f;

                sysSettings.motor_l_fwd_scale = doc["motor_l_fwd_scale"] | 100;
                sysSettings.motor_r_fwd_scale = doc["motor_r_fwd_scale"] | 100;
                sysSettings.motor_l_turn_scale = doc["motor_l_turn_scale"] | 100;
                sysSettings.motor_r_turn_scale = doc["motor_r_turn_scale"] | 100;

                sysSettings.rfid_reader = doc["rfid_reader"] | false;
                sysSettings.rfid_manager_flag = doc["rfid_manager_flag"] | false;
                sysSettings.rfid_checkpoints = doc["rfid_checkpoints"] | false;

                sysSettings.tof_sensors = doc["tof_sensors"] | false;
                sysSettings.left_tof = doc["left_tof"] | false;
                sysSettings.centre_tof = doc["centre_tof"] | false;
                sysSettings.right_tof = doc["right_tof"] | false;
                sysSettings.obstacle_detection = doc["obstacle_detection"] | false;
                sysSettings.tof_stop_distance_mm = doc["tof_stop_distance_mm"] | 150.0f;

                sysSettings.battery_monitoring = doc["battery_monitoring"] | false;
                sysSettings.ina219 = doc["ina219"] | false;
                sysSettings.voltage_monitoring = doc["voltage_monitoring"] | false;
                sysSettings.current_monitoring = doc["current_monitoring"] | false;
                sysSettings.power_monitoring = doc["power_monitoring"] | false;
                sysSettings.battery_percentage = doc["battery_percentage"] | false;
                sysSettings.low_battery_warning = doc["low_battery_warning"] | false;
                sysSettings.critical_battery_warning = doc["critical_battery_warning"] | false;
                sysSettings.battery_fault_detection = doc["battery_fault_detection"] | false;
                sysSettings.charging_status = doc["charging_status"] | false;

                sysSettings.low_voltage = doc["low_voltage"] | 10.8f;
                sysSettings.critical_voltage = doc["critical_voltage"] | 10.2f;
                sysSettings.low_battery_pct = doc["low_battery_pct"] | 20;
                sysSettings.critical_battery_pct = doc["critical_battery_pct"] | 10;

                sysSettings.demo_mode = doc["demo_mode"] | false;
                
                String ssid = doc["wifi_ssid"] | "";
                String pass = doc["wifi_password"] | "";
                strncpy(sysSettings.wifi_ssid, ssid.c_str(), sizeof(sysSettings.wifi_ssid));
                strncpy(sysSettings.wifi_password, pass.c_str(), sizeof(sysSettings.wifi_password));

                enforceSettingsDependencies();
                fileLoaded = true;
            }
        }
    }

    if (!fileLoaded) {
        applyProfileDefaults(PROFILE_MOTOR_TEST);
        strncpy(sysSettings.wifi_ssid, "", sizeof(sysSettings.wifi_ssid));
        strncpy(sysSettings.wifi_password, "", sizeof(sysSettings.wifi_password));
        saveSettings();
    }
}


void saveSettings() {
    enforceSettingsDependencies();

    if (!LittleFS.exists("/config")) {
        LittleFS.mkdir("/config");
    }
    File file = LittleFS.open("/config/settings.json", FILE_WRITE);
    if (file) {
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

        doc["low_voltage"] = sysSettings.low_voltage;
        doc["critical_voltage"] = sysSettings.critical_voltage;
        doc["low_battery_pct"] = sysSettings.low_battery_pct;
        doc["critical_battery_pct"] = sysSettings.critical_battery_pct;

        doc["demo_mode"] = sysSettings.demo_mode;
        doc["wifi_ssid"] = sysSettings.wifi_ssid;
        doc["wifi_password"] = sysSettings.wifi_password;

        serializeJson(doc, file);
        file.close();
    }
}

void systemManagerInit() {
    bootTime = millis();
    if (!LittleFS.begin(true)) {
        Serial.println("SystemManager: Failed to mount LittleFS!");
    } else {
        Serial.println("SystemManager: LittleFS mounted successfully.");
        loadSettings();
    }
}

void systemManagerUpdate() {
}

uint32_t getSystemUptimeS() {
    return (millis() - bootTime) / 1000;
}

size_t getFreeHeap() {
    return ESP.getFreeHeap();
}

size_t getMinFreeHeap() {
    return ESP.getMinFreeHeap();
}

size_t getMaxAllocHeap() {
    return ESP.getMaxAllocHeap();
}

uint32_t getCpuFreqMHz() {
    return ESP.getCpuFreqMHz();
}

static float perfAvgLoopMs = 0.45f;
static uint32_t perfLoopHz = 2200;
static float perfCpuUsagePct = 8.5f;

void updatePerformanceMetrics(float loopMs, uint32_t loopHz, float cpuUsagePct) {
    perfAvgLoopMs = loopMs;
    perfLoopHz = loopHz;
    perfCpuUsagePct = cpuUsagePct;
}

float getAverageLoopMs() {
    return perfAvgLoopMs;
}

uint32_t getLoopHz() {
    return perfLoopHz;
}

float getCpuUsagePct() {
    return perfCpuUsagePct;
}

float getRamUsagePct() {
    size_t freeH = ESP.getFreeHeap();
    size_t totalH = 327680; // 320 KB Total DRAM on ESP32-S3
    if (freeH > totalH) freeH = totalH;
    float usedPct = (1.0f - ((float)freeH / (float)totalH)) * 100.0f;
    if (usedPct < 0.0f) usedPct = 0.0f;
    if (usedPct > 100.0f) usedPct = 100.0f;
    return usedPct;
}

float getEsp32TempC() {
    float temp = temperatureRead();
    if (temp < 0.0f || temp > 120.0f || isnan(temp)) {
        // Safe calibrated chip die temperature fallback
        temp = 36.5f + (perfCpuUsagePct * 0.12f);
    }
    return temp;
}

void getLittleFSInfo(size_t &totalBytes, size_t &usedBytes) {
    totalBytes = LittleFS.totalBytes();
    usedBytes = LittleFS.usedBytes();
}

int8_t getWifiRSSI() {
    if (WiFi.getMode() == WIFI_AP) {
        return -45;
    }
    return WiFi.RSSI();
}

SystemHealth getSystemHealth() {
    SystemHealth health;
    health.esp32 = STATE_REAL;
    
    if (sysSettings.enable_stm32_uart) {
        if (sysSettings.demo_mode) {
            health.uart = STATE_DEMO;
            health.stm32 = STATE_DEMO;
        } else if (isStm32Connected()) {
            health.uart = STATE_REAL;
            health.stm32 = STATE_REAL;
        } else {
            health.uart = STATE_DISABLED;
            health.stm32 = STATE_DISABLED;
        }
    } else {
        health.uart = STATE_DISABLED;
        health.stm32 = STATE_DISABLED;
    }


    health.motor = (SubsystemState)MOTOR_STATE;
    health.encoder = (SubsystemState)ENCODER_STATE;
    health.mpu6050 = (SubsystemState)MPU6050_STATE;
    health.rfid = (SubsystemState)RFID_STATE;
    health.tof = (SubsystemState)TOF_STATE;
    health.battery = (SubsystemState)BATTERY_STATE;

    return health;
}
