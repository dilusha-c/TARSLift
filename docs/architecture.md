# TARSLIFT AGV - System Architecture & Engineering Design

This document details the dual-microcontroller architecture, hardware distribution, data communication pipeline, and navigation theories powering the TARSLIFT Autonomous Guided Vehicle.

---

## 1. High-Level Architecture Overview

TARSLIFT AGV uses a **master-slave asymmetric multiprocessing architecture**:
- **High-Level Master (ESP32-S3 Dual-Core Xtensa LX7 @ 240MHz)**:
  - Hosts the HTTP Web Server & WebSocket real-time telemetry broadcaster.
  - Manages persistent storage (LittleFS) for route graphs (`routes.json`), RFID tag databases (`tags.json`), and system settings (`settings.json`).
  - Runs the Navigation Engine: Dijkstra shortest-path graph search, waypoint trajectory replay, and mission state machine.
  - Interfaces directly via SPI with the MFRC522 RFID reader for ground node detection.
- **Low-Level Real-Time Co-Processor (STM32F103C8T6 ARM Cortex-M3 @ 72MHz)**:
  - Executes deterministic 50Hz motor speed PID control loops with anti-windup.
  - Interfaces with hardware quadrature optical/magnetic wheel encoders via Hardware Timer Encoder Mode (TIM2 & TIM3).
  - Generates 20kHz dual-channel PWM on TIM1 for H-Bridge motor drivers.
  - Drives 3x VL53L0X Time-of-Flight (ToF) laser ranging sensors across a single shared I2C1 bus with dynamic XSHUT address allocation (`0x54`, `0x56`, `0x58`).
  - Samples the MPU6050 6-DOF IMU (Accelerometer + Gyroscope) for orientation and turning angle tracking.

```
                  +-------------------------------------------------------+
                  |                    WEB BROWSER UI                     |
                  |  (Vanilla HTML5 / CSS Glassmorphism / WebSocket JSON) |
                  +-------------------------------------------------------+
                                              ^
                                              | Wi-Fi (AP + STA Mode)
                                              v
+-----------------------------------------------------------------------------------------+
| ESP32-S3 NAVIGATION MASTER (FreeRTOS Dual-Core)                                         |
|                                                                                         |
|  [Core 0: Network & Storage Task]              [Core 1: Real-Time Navigation Task]      |
|   - AsyncWebServer (:80)                        - Mission State Machine (Teach/Repeat)  |
|   - WebSocket Telemetry (20Hz)                  - Dijkstra Shortest-Path Graph Engine   |
|   - LittleFS JSON Persistence                   - RFID MFRC522 Scanner (SPI)            |
|   - Battery Monitor (INA219 / ADC)              - UART Watchdog & Link Supervisor       |
+-----------------------------------------------------------------------------------------+
                                              ^
                                              | Full-Duplex UART @ 115200 Baud (Binary Protocol)
                                              v
+-----------------------------------------------------------------------------------------+
| STM32F103C8T6 HARDWARE CO-PROCESSOR                                                     |
|                                                                                         |
|   - Dual Closed-Loop PID Motor Control (50Hz)   - TIM1 PWM Generation (20kHz)           |
|   - Hardware Quadrature Encoders (TIM2/TIM3)    - MPU6050 6-DOF IMU (I2C1 @ 0x68)       |
|   - 3x VL53L0X ToF Laser Sensors (I2C1: 0x54, 0x56, 0x58 with XSHUT control)            |
|   - 20Hz Odometry / ToF / IMU Telemetry Stream Generator                                |
+-----------------------------------------------------------------------------------------+
```

---

## 2. Hardware Pinout & Interfacing

### 2.1 ESP32-S3 Master Pinout
| Function | ESP32-S3 GPIO | Connected Component | Description |
| :--- | :--- | :--- | :--- |
| **UART RX2** | GPIO 18 | STM32 PA2 (TX2) | 115200 Baud packet input |
| **UART TX2** | GPIO 17 | STM32 PA3 (RX2) | 115200 Baud command output |
| **SPI SCK** | GPIO 10 | RC522 SCK | SPI Clock for RFID |
| **SPI MISO** | GPIO 9 | RC522 MISO | SPI Master In Slave Out |
| **SPI MOSI** | GPIO 12 | RC522 MOSI | SPI Master Out Slave In |
| **SPI SS/CS** | GPIO 13 | RC522 SDA/NSS | Chip Select |
| **RFID RST** | GPIO 11 | RC522 RST | Hardware Reset |
| **I2C SDA** | GPIO 4 | INA219 SDA | Optional Battery Monitor I2C |
| **I2C SCL** | GPIO 5 | INA219 SCL | Optional Battery Monitor I2C |
| **Battery ADC**| GPIO 1 | Voltage Divider | Direct analog voltage sensing |

### 2.2 STM32F103C8T6 Co-Processor Pinout
| Function | STM32 Pin | Connected Component | Description |
| :--- | :--- | :--- | :--- |
| **UART TX2** | PA2 | ESP32 GPIO 18 (RX) | USART2 115200 8N1 |
| **UART RX2** | PA3 | ESP32 GPIO 17 (TX) | USART2 115200 8N1 |
| **PWM Left** | PA8 | Motor Driver PWM L | TIM1_CH1 (20kHz) |
| **PWM Right** | PA9 | Motor Driver PWM R | TIM1_CH2 (20kHz) |
| **DIR Left** | PB12 | Motor Driver DIR1 | Direction pin |
| **DIR Right**| PB13 | Motor Driver DIR2 | Direction pin |
| **ENC L (A/B)**| PA0, PA1 | Left Encoder | TIM2 Channel 1 & 2 Encoder Mode |
| **ENC R (A/B)**| PA6, PA7 | Right Encoder | TIM3 Channel 1 & 2 Encoder Mode |
| **I2C1 SCL** | PB6 | VL53L0X + MPU6050 | Shared I2C Bus (400kHz Fast Mode) |
| **I2C1 SDA** | PB7 | VL53L0X + MPU6050 | Shared I2C Bus (400kHz Fast Mode) |
| **TOF1 XSHUT**| PB0 | Left ToF XSHUT | Boot reset -> Readdress to `0x54` |
| **TOF2 XSHUT**| PB1 | Center ToF XSHUT | Boot reset -> Readdress to `0x56` |
| **TOF3 XSHUT**| PB10 | Right ToF XSHUT | Boot reset -> Readdress to `0x58` |
| **Status LED**| PC13 | Onboard LED | 1Hz Heartbeat Blink |

---

## 3. Core Navigation & Control Theories

### 3.1 Differential Drive Kinematics & Odometry
The AGV utilizes two independently driven wheels separated by track width $W$ (wheelbase) with radius $R$.
When encoders record pulses $\Delta N_L$ and $\Delta N_R$ over sampling period $\Delta t$:
$$\Delta d_L = \frac{\Delta N_L}{PPR} \cdot 2\pi R, \quad \Delta d_R = \frac{\Delta N_R}{PPR} \cdot 2\pi R$$
$$\Delta d = \frac{\Delta d_R + \Delta d_L}{2}, \quad \Delta \theta = \frac{\Delta d_R - \Delta d_L}{W}$$
Position is updated as:
$$x_{k+1} = x_k + \Delta d \cos\left(\theta_k + \frac{\Delta \theta}{2}\right)$$
$$y_{k+1} = y_k + \Delta d \sin\left(\theta_k + \frac{\Delta \theta}{2}\right)$$
$$\theta_{k+1} = \theta_k + \Delta \theta$$

### 3.2 Closed-Loop PID Speed Control
On the STM32, motor speed is regulated by a discrete-time PID algorithm executed at 50Hz:
$$e(t) = \text{RPM}_{\text{target}} - \text{RPM}_{\text{measured}}$$
$$u(t) = K_p e(t) + K_i \int_0^t e(\tau)d\tau + K_d \frac{de(t)}{dt}$$
Anti-windup clamping limits the integral accumulator:
$$u(t) = \text{clamp}(u(t), -1000, 1000)$$

### 3.3 Dijkstra Shortest Path Search on Topological RFID Graph
Routes are represented as a directed graph $G = (V, E)$, where:
- $V$ is the set of RFID tag locations (Nodes).
- $E$ is the set of recorded trajectories connecting tag pairs with weight $w(u,v) = \text{Distance}(u,v)$.
When the operator requests navigation from Start Tag $A$ to Destination Tag $B$, the ESP32 computes:
$$\text{dist}[v] = \min_{(u,v) \in E} (\text{dist}[u] + w(u,v))$$
The calculated shortest sequence of edges is then chained and dispatched sequentially to the motor execution engine.
