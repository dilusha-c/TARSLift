# AGV ESP32-S3 ↔ STM32F103 UART Communication Protocol Specification

| Field | Value |
| :--- | :--- |
| **Document Title** | AGV ESP32-S3 ↔ STM32F103 UART Communication Protocol Specification |
| **Document Version** | 1.0 |
| **Protocol Family** | AGV UART Protocol |
| **Current Implementation** | V1.0 (Communication Test) |
| **Document Level** | High-Level Professional Engineering Specification |
| **Intended Audience** | Project supervisor, examiner, embedded engineers, AGV firmware team |
| **Physical Link** | UART, 115200-8N1, ESP32-S3 (host) ↔ STM32F103C8T6 (real-time controller) |

---

## 1. System Architecture

The AGV platform is built around two controllers with a strict separation of concerns:
- **ESP32-S3 (High-Level Controller)**: Owns route intelligence, RFID identification (RC522 reader directly attached), system-level monitoring, Web Dashboard server, and battery monitoring.
- **STM32F103C8T6 (Real-Time Controller)**: Owns closed-loop motor drive (PWM/PID), wheel encoder odometry, MPU6050 IMU acquisition, and physical motion execution.

> **Architecture Note**: The RC522 RFID reader is connected directly to the ESP32-S3 via SPI. The STM32 has no interface to the RC522 and never receives raw RFID UID data over UART.

### Controller Responsibility Matrix
| Function | ESP32-S3 | STM32F103 |
| :--- | :---: | :---: |
| RC522 RFID Reader | **YES** | NO |
| RFID UID Processing | **YES** | NO |
| Route Storage & Management | **YES** | NO |
| Teach / Repeat Orchestration | **YES** | NO |
| Shortest-Path Calculation | **YES** | NO |
| Battery Monitoring | **YES** | NO |
| Wheel Encoders | NO | **YES** |
| MPU6050 IMU | NO | **YES** |
| Motor Control / PID / PWM | NO | **YES** |
| Real-time Motion Execution | NO | **YES** |

---

## 2. Physical & UART Link Configuration

- **Baud Rate**: `115200`
- **Data Bits**: `8`
- **Parity**: `None`
- **Stop Bits**: `1`
- **Flow Control**: `None` (Format `8N1`)

```
ESP32-S3 TX  ------------------->  STM32 RX
ESP32-S3 RX  <-------------------  STM32 TX
ESP32-S3 GND ------------------->  STM32 GND
```

---

## 3. Binary Packet Format

```
+------+------+------+-----+---------+---------+-------+
| SOF1 | SOF2 | CMD  | SEQ |   LEN   | PAYLOAD | CRC16 |
| 0xAA | 0x55 | 1 B  | 1 B |   2 B   | 0-64 B  |  2 B  |
+------+------+------+-----+---------+---------+-------+
Wire Order: [AA][55][CMD][SEQ][LEN_L][LEN_H][PAYLOAD...][CRC_L][CRC_H]
```

- **SOF1 / SOF2**: `0xAA 0x55` (Start of Frame constants)
- **CMD**: 1 byte Command ID
- **SEQ**: 1 byte Rolling counter (`0x00`–`0xFF`), echoed back in response packets for request correlation
- **LEN**: 2 bytes Payload length in bytes (little-endian), max 64 bytes
- **PAYLOAD**: `0–64` bytes payload
- **CRC16**: 2 bytes Little-endian CRC-16-CCITT (`CCITT-FALSE`, Poly `0x1021`, Init `0xFFFF`) calculated over `CMD + SEQ + LEN + PAYLOAD`

---

## 4. Master Command Table

| Ver | CMD Hex | Direction | Command Name | Payload Size | Response / Event | Purpose |
| :--- | :--- | :--- | :--- | :---: | :--- | :--- |
| **V1** | `0x01` | ESP → STM | `PING` | 0 B | `ACK (0x81)` | Liveness test |
| **V1** | `0x02` | ESP → STM | `GET_STATUS` | 0 B | `STATUS (0x82)` | Request system status |
| **V1** | `0x03` | ESP → STM | `GET_VERSION` | 0 B | `VERSION (0x83)` | Request FW/Protocol version |
| **V1** | `0x04` | ESP → STM | `HEARTBEAT` | 0 B | `ACK (0x81)` | Periodic keep-alive (500 ms) |
| **V1** | `0x81` | STM → ESP | `ACK` | 2 B | — | Command Acknowledgment |
| **V1** | `0x82` | STM → ESP | `STATUS` | 2 B | — | System state & error flags |
| **V1** | `0x83` | STM → ESP | `VERSION` | 5 B | — | Version information |
| **V1** | `0xE0` | STM → ESP | `ERROR` | 4+ B | — | Asynchronous error notification |
| **V2\*** | `0x30` | STM → ESP | `ODOMETRY` | 10 B | — | Encoder distance & fused yaw |
| **V2\*** | `0x31` | STM → ESP | `IMU_DATA` | 14 B | — | Accelerometer, gyro, yaw |
| **V2\*** | `0x60` | STM → ESP | `MOTOR_STATUS` | 6 B | — | Motor PWM & direction feedback |
| **V3\*** | `0x10` | ESP → STM | `MOVE` | 6 B | `MOVE_DONE (0x91)` | Drive linear distance |
| **V3\*** | `0x11` | ESP → STM | `TURN` | 4 B | `TURN_DONE (0x92)` | Rotate angular angle |
| **V3\*** | `0x12` | ESP → STM | `STOP` | 0 B | `ACK (0x81)` | Emergency / motion stop |
| **V3\*** | `0x13` | ESP → STM | `SET_REPEAT_SPEED` | 2 B | `ACK (0x81)` | Scale playback speed % |
| **V3\*** | `0x91` | STM → ESP | `MOVE_DONE` | 2 B | — | Linear move completion event |
| **V3\*** | `0x92` | STM → ESP | `TURN_DONE` | 2 B | — | Angular turn completion event |
| **V4\*** | `0x20` | ESP → STM | `TEACH_START` | 0 B | `ACK (0x81)` | Start recording trajectory |
| **V4\*** | `0x21` | ESP → STM | `TEACH_STOP` | 0 B | `ACK (0x81)` | Stop recording trajectory |
| **V5\*** | `0x50` | ESP → STM | `SEGMENT_START` | 6 B | `ACK (0x81)` | Start RFID segment execution |
| **V5\*** | `0x51` | ESP → STM | `STEP_MOVE` | 6 B | `MOVE_DONE (0x91)` | Motion step within segment |
| **V5\*** | `0x52` | ESP → STM | `STEP_TURN` | 4 B | `TURN_DONE (0x92)` | Turn step within segment |
| **V5\*** | `0x93` | ESP → STM | `SEGMENT_COMPLETE` | 2 B | `ACK (0x81)` | ESP32 RFID segment confirm |
| **V6\*** | — | — | *Shortest Path* | — | — | ESP32 graph solver over V3/V5 |

*\* PLANNED roadmap protocol versions.*
