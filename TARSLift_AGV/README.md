# TARSLift_AGV - STM32F103 Real-Time Co-Processor Firmware

This folder contains the complete STM32CubeIDE project running on the **STM32F103C8T6 ARM Cortex-M3 (72MHz)** co-processor.

---

## 1. Directory Structure

```text
TARSLift_AGV/
├── AGV_Communication/        # Binary communication packet builder & parser (C)
│   ├── agv_stm32_c.h
│   └── agv_stm32_c.c
├── Core/
│   ├── Inc/                  # Header files (main.h, motor.h, vl53l0x.h, mpu6050_lite.h)
│   └── Src/                  # Source implementations (main.c, motor.c, vl53l0x.c, mpu6050_lite.c)
├── Drivers/                  # STM32 HAL and CMSIS peripheral drivers
└── TARSLift_AGV.ioc          # STM32CubeMX hardware pin & clock configuration file
```

---

## 2. Real-Time Hardware Control Loops

1. **50Hz Closed-Loop Motor PID Loop (20ms interval)**:
   - Samples hardware timer encoder counters (`TIM2` and `TIM3`).
   - Handles 16-bit integer timer counter overflow/underflow wrapping cleanly.
   - Computes Left and Right wheel rotational velocity ($RPM$).
   - Calculates error $e = \text{target\_rpm} - \text{measured\_rpm}$.
   - Evaluates proportional, integral, and derivative terms with anti-windup clamping.
   - Updates `TIM1` PWM output ($0 \dots 1000$ counts @ 20kHz).
2. **20Hz Telemetry Broadcast Loop (50ms interval)**:
   - Reads 3x VL53L0X Time-of-Flight sensors sequentially on I2C1.
   - Reads 6-axis accelerometer and gyroscope registers from MPU6050.
   - Packages and transmits `ODOMETRY`, `ENCODER_DATA`, `TOF_DATA`, and `IMU_DATA` frames to the ESP32 over `USART2`.
3. **1Hz Diagnostics Heartbeat**:
   - Toggles onboard status LED (`PC13`).

---

## 3. Peripheral Map & Pin Assignments

| Peripheral | Pins | Connected Device | Mode |
| :--- | :--- | :--- | :--- |
| **USART2** | `PA2 (TX)`, `PA3 (RX)` | ESP32-S3 (GPIO 18, 17) | 115200 8N1 Interrupt Driven |
| **TIM1** | `PA8 (CH1)`, `PA9 (CH2)` | Left / Right Motor PWM | 20kHz PWM Generation |
| **GPIO Out** | `PB12`, `PB13` | Left / Right Motor DIR | H-Bridge Direction |
| **TIM2** | `PA0 (CH1)`, `PA1 (CH2)` | Left Wheel Encoder | Hardware Encoder Mode (TI1 and TI2) |
| **TIM3** | `PA6 (CH1)`, `PA7 (CH2)` | Right Wheel Encoder | Hardware Encoder Mode (TI1 and TI2) |
| **I2C1** | `PB6 (SCL)`, `PB7 (SDA)` | VL53L0X Sensors + MPU6050 | 400kHz Fast-mode I2C |
| **GPIO Out** | `PB0`, `PB1`, `PB10` | ToF1, ToF2, ToF3 XSHUT | Dynamic I2C Address Sequencing |
| **GPIO Out** | `PC13` | Onboard LED | 1Hz Heartbeat Indicator |

---

## ⚡ Key Features

1. **System Clock**: **72 MHz** ($8\text{ MHz Crystal} \times 9\text{ PLL}$) with automatic **HSI (Internal RC Oscillator)** fallback.
2. **UART Serial Protocol**: Communicates with ESP32-S3 via **`USART2` @ 115200 Baud (8N1)** on **PA2 (TX)** and **PA3 (RX)**.
3. **Internal Pull-Up**: Enables `GPIO_PULLUP` on `PA3` (RX) to prevent floating pin electrical noise.
4. **Error Recovery**: Automatically resets HAL UART state (`huart2.ErrorCode`) and clears Overrun Errors (`ORE`) to maintain robust uninterrupted transmission.
5. **Visual Diagnostics**:
   - 🟢 **1 Hz Blink**: Normal operation & active 20 Hz odometry transmission.
   - 🔴 **10 Hz Rapid Flash**: Hardware peripheral error indicator.

---

## 🚀 How to Build & Flash

1. Open **STM32CubeIDE**, **Keil**, or **PlatformIO**.
2. Add `Core/Inc` and `AGV_Communication` to your include paths.
3. Add `Core/Src/main.c` and `AGV_Communication/agv_stm32_c.c` to your build sources.
4. Connect ST-Link programmer to STM32 (`SWCLK`, `SWDIO`, `GND`, `3.3V`).
5. Ensure **BOOT0 jumper = 0** and click **Build & Flash**!
