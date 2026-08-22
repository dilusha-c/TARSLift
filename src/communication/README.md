# STM32 UART Communication & Watchdog Subsystem (`src/communication/`)

This directory contains the UART manager responsible for reliable, full-duplex binary packet exchange between the ESP32-S3 and STM32F103 co-processor.

---

## 1. Hardware Connections

| ESP32-S3 Pin | STM32 Pin | Signal | Baud Rate | Configuration |
| :--- | :--- | :--- | :--- | :--- |
| **GPIO 18** | **PA2 (TX2)** | ESP32 RX ➔ STM32 TX | 115200 | 8 Data Bits, No Parity, 1 Stop Bit (8N1) |
| **GPIO 17** | **PA3 (RX2)** | ESP32 TX ➔ STM32 RX | 115200 | 8 Data Bits, No Parity, 1 Stop Bit (8N1) |
| **GND** | **GND** | Common Ground | — | Mandatory common ground reference |

---

## 2. Communication Engine & State Machine

The parser processes incoming byte streams byte-by-byte using a deterministic state machine:

```
[WAIT_SOF1 (0xAA)] ➔ [WAIT_SOF2 (0x55)] ➔ [READ_LEN] ➔ [READ_SEQ] ➔ [READ_CMD] ➔ [READ_PAYLOAD] ➔ [VERIFY_CRC16]
        |                    |
        +-------(Invalid)----+ (Reset to WAIT_SOF1 on CRC or header mismatch)
```

### CRC-16-CCITT Verification
Every packet is validated using the standard CCITT polynomial ($0x1021$) with initial value $0xFFFF$:
$$\text{CRC}_{k+1} = (\text{CRC}_k \ll 8) \oplus \text{Table}\left[(\text{CRC}_k \gg 8) \oplus \text{Byte}\right]$$
Packets failing CRC verification are dropped immediately, and an error is logged to WebSerial.

---

## 3. Link Watchdog & Failsafe Supervisor

- **Heartbeat & Telemetry Expectation**: The STM32 sends telemetry (Odometry, Encoders, ToF, IMU) at 20Hz (every 50ms).
- **Watchdog Timeout ($1500\text{ms}$)**:
  - If no valid packet is received for $>1.5\text{s}$, the watchdog trips.
  - An emergency stop command is issued internally.
  - Active autonomous missions are aborted to prevent runaway conditions.
  - Status banner turns RED on the Web Dashboard.

---

## 4. Key Functions (`uart_manager.h`)

- `void initStm32Uart()`: Configures `HardwareSerial(2)` with GPIO 18/17 at 115200 Baud.
- `void stm32UartUpdate()`: Real-time pump called inside Core 1 `realtimeTask`. Decodes incoming frames and updates global telemetry caches.
- `bool sendMotorSpeeds(int16_t leftMmS, int16_t rightMmS)`: Dispatches differential speed commands.
- `bool sendPidTuning(...)`: Updates $K_p, K_i, K_d$ on the STM32.
- `bool sendEncoderConfig(...)`: Updates wheel diameter and encoder PPR on the STM32.
