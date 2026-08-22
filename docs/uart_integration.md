# STM32 - ESP32 Full-Duplex Binary UART Protocol Guide

This document specifies the binary communication interface connecting the ESP32-S3 (High-Level Master) and STM32F103 (Low-Level Co-Processor) over full-duplex UART at 115200 Baud (8N1).

---

## 1. Frame Structure

Every packet follows the strict binary framing format:

| Byte Index | Field Name | Type | Description |
| :--- | :--- | :--- | :--- |
| `0` | **SOF 1** | `uint8_t` | Start of Frame 1: `0xAA` |
| `1` | **SOF 2** | `uint8_t` | Start of Frame 2: `0x55` |
| `2` | **Payload Length ($L$)** | `uint8_t` | Length of payload in bytes ($0 \le L \le 64$) |
| `3` | **Sequence ($N$)** | `uint8_t` | Incremental sequence identifier ($0 \dots 255$) |
| `4` | **Command / Msg ID**| `uint8_t` | Opcode defined in `AGV_Protocol.h` |
| `5 \dots 4+L` | **Payload** | `uint8_t[L]` | Binary command parameters or telemetry fields |
| `5+L` | **CRC16 Low** | `uint8_t` | CRC-16-CCITT polynomial `0x1021` (Init: `0xFFFF`) |
| `6+L` | **CRC16 High** | `uint8_t` | CRC-16-CCITT MSB |

Total overhead per packet: **6 bytes** (2 SOF + 1 LEN + 1 SEQ + 2 CRC).

---

## 2. Command Set (ESP32 ➔ STM32)

| Opcode | Name | Payload Format | Description |
| :--- | :--- | :--- | :--- |
| `0x01` | `AGV_CMD_PING` | Empty ($L=0$) | Heartbeat link check |
| `0x02` | `AGV_CMD_GET_STATUS`| Empty ($L=0$) | Request status & error register |
| `0x03` | `AGV_CMD_GET_VERSION`| Empty ($L=0$) | Request firmware build numbers |
| `0x10` | `AGV_CMD_MOVE_DIST` | `int32_t dist_mm, int16_t speed_mm_s` | Drive distance with trajectory profile |
| `0x11` | `AGV_CMD_TURN_ANGLE`| `int16_t angle_x10, int16_t speed_deg_s`| Pivot turn using IMU Z feedback |
| `0x12` | `AGV_CMD_STOP` | Empty ($L=0$) | Immediate deceleration stop / brake |
| `0x13` | `AGV_CMD_EMERGENCY_STOP` | Empty ($L=0$) | Instantaneous PWM kill |
| `0x14` | `AGV_CMD_SET_SPEEDS`| `int16_t l_speed, int16_t r_speed` | Direct differential velocity drive (mm/s) |
| `0x20` | `AGV_CMD_SET_PID` | `float kp_l, ki_l, kd_l, kp_r, ki_r, kd_r`| Tune discrete 50Hz motor PID gains |
| `0x21` | `AGV_CMD_SET_ENCODER_CFG`| `float circ_mm, uint16_t ppr_l, ppr_r` | Wheel geometry & resolution |
| `0x22` | `AGV_CMD_CALIBRATE_IMU`| Empty ($L=0$) | Re-zero gyro drift offsets |
| `0x23` | `AGV_CMD_SET_TOF_CFG`| `uint16_t stop_dist_mm` | Obstacle threshold |

---

## 3. Telemetry Stream (STM32 ➔ ESP32 @ 20Hz)

| Opcode | Name | Payload Format | Description |
| :--- | :--- | :--- | :--- |
| `0x80` | `AGV_CMD_ACK` | `uint8_t orig_cmd, uint8_t result, uint8_t orig_seq` | Command receipt & execution status |
| `0x30` | `AGV_CMD_ODOMETRY` | `int32_t dist_l, int32_t dist_r, int16_t yaw_x10` | 20Hz Odometry position state |
| `0x31` | `AGV_CMD_ENCODER_DATA`| `int32_t pulses_l, int32_t pulses_r` | Raw timer encoder pulse counters |
| `0x32` | `AGV_CMD_IMU_DATA` | `int16_t ax, ay, az, gx, gy, gz` | 6-DOF raw accelerometer & gyro |
| `0x33` | `AGV_CMD_TOF_DATA` | `uint16_t tof_l, uint16_t tof_c, uint16_t tof_r` | Ranging distances in mm |

---

## 4. Hardware Watchdog & Error Recovery

1. **Link Timeout Detection**:
   The ESP32 tracks the timestamp of the last decoded valid packet. If `millis() - lastValidPacket > 1500ms`, the ESP32 enters `COMM_TIMEOUT` state, triggers visual dashboard alerts, and halts autonomous navigation.
2. **Noise Recovery**:
   If line noise induces Framing or Overrun errors on STM32 USART2, the `HAL_UART_ErrorCallback` automatically clears `__HAL_UART_CLEAR_OREFLAG` / `FEFLAG` and resumes interrupt reception without locking the processor.

