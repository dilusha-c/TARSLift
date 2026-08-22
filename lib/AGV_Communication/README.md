# AGV Communication Library (`lib/AGV_Communication/`)

This library provides the shared C++ protocol header and parser implementations used across the ESP32-S3 firmware.

---

## 1. Key Components

- **`AGV_Protocol.h`**: Defines the opcode enumerations, packet structures, and CRC16-CCITT routines.
- **`AGV_Responses.h`**: Result codes (`AGV_RESULT_OK`, `AGV_RESULT_ERROR`, `AGV_RESULT_CRC_FAIL`, etc.).
- **`AGV_STM32_Driver.h` / `AGV_STM32_Driver.cpp`**: Helper classes for building command frames and decoding telemetry streams.

---

## 2. Shared Protocol Compatibility

The constants, opcodes, and CRC polynomial defined here are binary-compatible with the C implementation running on the STM32 co-processor (`TARSLift_AGV/AGV_Communication/`).
