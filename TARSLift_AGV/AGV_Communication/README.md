# STM32 C Communication Protocol Engine (`TARSLift_AGV/AGV_Communication/`)

This directory contains the pure ANSI C packet encoding, decoding, CRC calculation, and framing engine for the STM32 co-processor.

---

## 1. Files & Structure

- **`agv_stm32_c.h`**: Packet structures, opcode constants (`AGV_CMD_*`), ACK result codes, and parser state machine definitions.
- **`agv_stm32_c.c`**: State machine `agv_stm32_parser_feed()`, packet builders (`agv_build_odometry()`, `agv_build_tof_data()`, etc.), and CRC16-CCITT computation.

---

## 2. Zero-Allocation Streaming Parser

The parser uses a static buffer state machine that consumes incoming UART ISR bytes one-by-one without requiring dynamic heap memory allocations (`malloc`), ensuring deterministic execution in hard real-time interrupt contexts.
