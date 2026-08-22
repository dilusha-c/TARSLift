# System Configuration & Testing Profiles (`src/config/`)

This directory houses the profile definitions, feature flags, and hardware constant mappings for the TARSLIFT AGV.

---

## 1. Modular Testing Profiles Architecture

The system implements 10 pre-configured operating modes:

1. **`CUSTOM (0)`**: Dynamic user control via dashboard checkboxes.
2. **`MOTOR_TEST (1)`**: DC motors enabled for direction and H-Bridge verification.
3. **`ENCODER_TEST (2)`**: Encoders + Motors enabled for Pulses-Per-Revolution (PPR) calibration.
4. **`MPU6050_TEST (3)`**: IMU enabled for drift and angular velocity verification.
5. **`RFID_TEST (4)`**: RC522 SPI scanner active for tag detection.
6. **`TOF_TEST (5)`**: 3x VL53L0X laser distance sensors active for collision avoidance testing.
7. **`BATTERY_TEST (6)`**: INA219 / ADC voltage monitor active for power curve profiling.
8. **`TEACH_TEST (7)`**: Manual driving + Encoders + IMU + RFID for route mapping.
9. **`REPEAT_TEST (8)`**: Autonomous path replay engine with obstacle safety.
10. **`FULL_SYSTEM (9)`**: Production release profile with all subsystems active.

---

## 2. Feature Flags Bitmask (`feature_flags.h`)

Subsystems are toggled dynamically at runtime without recompiling firmware:
- `FLAG_MOTORS_ENABLED`
- `FLAG_ENCODERS_ENABLED`
- `FLAG_MPU6050_ENABLED`
- `FLAG_TOF_ENABLED`
- `FLAG_RFID_ENABLED`
- `FLAG_BATTERY_ENABLED`
- `FLAG_DEMO_SIMULATION`

---

## 3. Persistent JSON Storage (`/config/settings.json`)

All configuration parameters are serialized using `ArduinoJson` (v7) and saved to LittleFS. On boot, the system mounts LittleFS, reads `settings.json`, and initializes the hardware with the saved calibration values.
