#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// PROFILE DEFINITIONS
// ============================================================================
#define PROFILE_CUSTOM          0
#define PROFILE_MOTOR_TEST      1
#define PROFILE_ENCODER_TEST    2
#define PROFILE_MPU6050_TEST    3
#define PROFILE_RFID_TEST       4
#define PROFILE_TOF_TEST        5
#define PROFILE_BATTERY_TEST    6
#define PROFILE_TEACH_TEST      7
#define PROFILE_REPEAT_TEST     8
#define PROFILE_FULL_SYSTEM     9

// Default compile-time baseline profile
#define TEST_PROFILE PROFILE_FULL_SYSTEM // We mapped this, but we'll load default from file or default to Profile 6 (Battery Test) or 1 (Motor Test)

enum SubsystemState {
    STATE_DISABLED = 0,
    STATE_DEMO     = 1,
    STATE_REAL     = 2
};

// ============================================================================
// SYSTEM RUNTIME SETTINGS STRUCT
// ============================================================================
struct Settings {
    int test_profile;

    // 1. Dashboard features
    bool enable_dashboard;
    bool enable_teach_mode;
    bool enable_repeat_mode;
    bool enable_manual_control;
    bool enable_route_manager;
    bool enable_rfid_manager;
    bool enable_error_log;
    bool enable_system_info;

    // 2. Communication
    bool enable_stm32_uart;
    bool enable_websocket;

    // 3. Motion
    bool enable_motor_control;
    bool enable_encoder;
    bool enable_mpu6050;
    bool enable_pid;

    // 4. RFID
    bool rfid_reader;
    bool rfid_manager_flag;
    bool rfid_checkpoints;

    // 5. Obstacle
    bool tof_sensors;
    bool left_tof;
    bool centre_tof;
    bool right_tof;
    bool obstacle_detection;

    // 6. Battery
    bool battery_monitoring;
    bool ina219;
    bool voltage_monitoring;
    bool current_monitoring;
    bool power_monitoring;
    bool battery_percentage;
    bool low_battery_warning;
    bool critical_battery_warning;
    bool battery_fault_detection;
    bool charging_status;

    // 7. Battery Thresholds
    float low_voltage;
    float critical_voltage;
    int low_battery_pct;
    int critical_battery_pct;

    // Other configurations
    bool demo_mode;
    char wifi_ssid[32];
    char wifi_password[64];
};

extern Settings sysSettings;

void loadSettings();
void saveSettings();
void applyProfileDefaults(int profileIndex);

// ============================================================================
// RUNTIME MACRO WRAPPERS
// ============================================================================
#define ENABLE_TEACH_MODE       (sysSettings.enable_teach_mode)
#define ENABLE_REPEAT_MODE      (sysSettings.enable_repeat_mode)
#define ENABLE_ROUTE_MANAGER    (sysSettings.enable_route_manager)
#define ENABLE_RFID_MANAGER     (sysSettings.enable_rfid_manager)
#define ENABLE_ERROR_LOG        (sysSettings.enable_error_log)
#define ENABLE_SYSTEM_INFO      (sysSettings.enable_system_info)

// Check state based on toggles
#define MOTOR_STATE             (sysSettings.enable_motor_control ? (sysSettings.demo_mode ? STATE_DEMO : STATE_REAL) : STATE_DISABLED)
#define ENCODER_STATE           (sysSettings.enable_encoder ? (sysSettings.demo_mode ? STATE_DEMO : STATE_REAL) : STATE_DISABLED)
#define MPU6050_STATE           (sysSettings.enable_mpu6050 ? (sysSettings.demo_mode ? STATE_DEMO : STATE_REAL) : STATE_DISABLED)
#define RFID_STATE              (sysSettings.rfid_reader ? (sysSettings.demo_mode ? STATE_DEMO : STATE_REAL) : STATE_DISABLED)
#define TOF_STATE               (sysSettings.tof_sensors ? (sysSettings.demo_mode ? STATE_DEMO : STATE_REAL) : STATE_DISABLED)
#define BATTERY_STATE           (sysSettings.battery_monitoring ? (sysSettings.demo_mode ? STATE_DEMO : STATE_REAL) : STATE_DISABLED)

#define ENABLE_STM32_UART       (sysSettings.enable_stm32_uart)
#define ENABLE_WEBSOCKET        (sysSettings.enable_websocket)
#define DEMO_MODE               (sysSettings.demo_mode)

#define WIFI_SSID               (sysSettings.wifi_ssid)
#define WIFI_PASSWORD           (sysSettings.wifi_password)
#define WIFI_CHANNEL            1
#define WIFI_MAX_CONN           4

// UART
#define STM32_UART_RX_PIN   16
#define STM32_UART_TX_PIN   17
#define STM32_UART_BAUD     115200


#define LED_PIN             2

#endif // CONFIG_H
