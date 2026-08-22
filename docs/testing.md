# TARSLIFT AGV - Subsystem Testing & Verification Guide

This guide describes the step-by-step procedures for testing each subsystem using the 10 built-in testing profiles.

---

## 1. Testing Profiles Overview

In the Web Dashboard under **Settings** ➔ **Profiles & Flags**, select any of the following profiles:

| ID | Profile Name | Active Hardware / Subsystems | Primary Verification Goal |
| :--- | :--- | :--- | :--- |
| **0** | `CUSTOM` | User-defined checkboxes | Modular isolation testing |
| **1** | `MOTOR_TEST` | Motors + Manual Drive | Verify motor polarity & H-bridge switching |
| **2** | `ENCODER_TEST`| Motors + Encoders | Calibrate PPR & Wheel Circumference |
| **3** | `MPU6050_TEST`| MPU6050 IMU | Verify Gyro Z integration on 90°/180° turns |
| **4** | `RFID_TEST` | MFRC522 Scanner | Test SPI read range and tag registration |
| **5** | `TOF_TEST` | 3x VL53L0X Laser Sensors | Test obstacle stop triggers & distance accuracy |
| **6** | `BATTERY_TEST`| INA219 / ADC | Verify voltage divider scaling & low-power cutoffs |
| **7** | `TEACH_TEST` | Manual + Encoders + IMU + RFID | Validate route trajectory recording |
| **8** | `REPEAT_TEST`| Autonomous Engine + Safety Sensors | Validate point-to-point & shortest-path replay |
| **9** | `FULL_SYSTEM` | All Subsystems Active | Production deployment profile |

---

## 2. Step-by-Step Subsystem Validation

### 2.1 Motor Direction & Trim Testing (`Profile 1: MOTOR_TEST`)
1. Place AGV on a test stand (wheels elevated off ground).
2. Open Dashboard **Manual Drive** tab.
3. Tap **Forward (▲)**: Both wheels must rotate forward. If one spins backward, invert its motor wiring polarity or adjust direction in `motor.c`.
4. Check **Left Trim** and **Right Trim** sliders in Settings to ensure the AGV tracks straight over a 5-meter test run.

### 2.2 Encoder Calibration & PPR Measurement (`Profile 2: ENCODER_TEST`)
1. Place AGV on a measured 1000mm (1 meter) track.
2. Mark initial wheel position.
3. Drive forward manually until 1000mm is reached.
4. Calculate actual Pulses-Per-Revolution:
   $$\text{PPR}_{\text{actual}} = \frac{\text{Total Recorded Pulses}}{\text{Wheel Revolutions}}$$
5. Update `enc_ppr_l` and `enc_ppr_r` in the Settings tab.

### 2.3 Time-of-Flight Ranging & Multi-Sensor Verification (`Profile 5: TOF_TEST`)
1. Place an obstacle 300mm in front of the Left, Center, and Right sensors in turn.
2. Verify that `dash-tof-l`, `dash-tof-c`, and `dash-tof-r` reflect the physical distance within $\pm 10\text{mm}$.
3. Ensure no sensor returns `65535` (Not Ready) or byte-swapped values.

### 2.4 RFID Ground Station Node Testing (`Profile 4: RFID_TEST`)
1. Pass RFID card under the underside RC522 antenna (within 20-30mm clearance).
2. Confirm the card UID appears in the WebSerial terminal and the Dashboard Station card.
3. Open **RFID Tags** tab, assign a name (e.g. `Node_01`), and click **Save Tag**.

