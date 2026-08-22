# STM32 Core Application Layer (`TARSLift_AGV/Core/`)

This directory contains the main application logic, peripheral initializations, and sensor/actuator drivers for the STM32F103 co-processor.

---

## 1. Drivers & Key Modules

| Module | Files | Functionality |
| :--- | :--- | :--- |
| **Main Control & Telemetry** | `main.c`, `main.h` | Clock setup (72MHz), USART2 ISR, 50Hz PID, 20Hz telemetry |
| **Motor Driver** | `motor.c`, `motor.h` | Direction pin switching and TIM1 PWM duty cycle generation |
| **Multi-Sensor ToF Driver** | `vl53l0x.c`, `vl53l0x.h` | 3x VL53L0X multi-device instance driver with XSHUT readdressing |
| **MPU6050 6-DOF IMU** | `mpu6050_lite.c`, `mpu6050_lite.h` | Gyroscope and accelerometer sampling over I2C1 |

---

## 2. Multi-Sensor ToF Address Sequencing

Because all VL53L0X sensors boot with the default address `0x52`, the STM32 performs dynamic hardware readdressing at startup:
1. Drive all 3 XSHUT pins LOW (`PB0`, `PB1`, `PB10`) to put all sensors into hardware standby.
2. Release `TOF1_XSHUT` (`PB0`) HIGH. Call `initVL53L0X(&tof_left, 0, &hi2c1)` and `setAddress_VL53L0X(&tof_left, 0x54)`.
3. Release `TOF2_XSHUT` (`PB1`) HIGH. Call `initVL53L0X(&tof_center, 0, &hi2c1)` and `setAddress_VL53L0X(&tof_center, 0x56)`.
4. Release `TOF3_XSHUT` (`PB10`) HIGH. Call `initVL53L0X(&tof_right, 0, &hi2c1)` and `setAddress_VL53L0X(&tof_right, 0x58)`.
5. Start continuous back-to-back ranging on all three sensors: `startContinuous(&tof, 0)`.
