# ESP32-S3 Master Firmware Subsystem Architecture (`src/`)

This directory contains the complete modular firmware running on the ESP32-S3 microcontroller using the Arduino ESP32 framework on FreeRTOS.

---

## 1. Dual-Core FreeRTOS Task Allocation

The firmware leverages both 240MHz Xtensa LX7 cores to eliminate latency jitter between web networking and safety-critical navigation:

### Core 0: Web, Networking & File I/O Task (`webNetworkTask`)
- Runs `AsyncWebServer` on Port 80.
- Serves static assets from the LittleFS partition (`index.html`, `style.css`, `app.js`).
- Pushes 20Hz JSON telemetry packets to connected clients over `/ws`.
- Handles REST API requests (`/api/routes`, `/api/tags`, `/api/settings`, `/api/control`).
- Performs non-blocking LittleFS JSON read/write operations.

### Core 1: Real-Time Subsystems & Watchdog Task (`realtimeTask`)
- Runs at a strict 50Hz (20ms tick) schedule.
- Pumps the UART packet parser receiving odometry and sensor data from the STM32 co-processor.
- Samples the MFRC522 RFID reader over SPI (GPIO 10, 9, 12, 13, 11).
- Evaluates ToF safety collision zones and enforces emergency stops.
- Drives the Mission Execution State Machine (Autonomous Replay & Teach recording).
- Supervises the 1500ms UART communication watchdog.

---

## 2. Directory Structure & Modular Breakdown

| Directory | Purpose | Key Files |
| :--- | :--- | :--- |
| **`battery/`** | Voltage/current monitoring, state-of-charge calculation, low-voltage alarms | `battery_manager.h`, `battery_manager.cpp` |
| **`communication/`**| STM32 UART interface, packet decoding, command transmission, watchdog | `uart_manager.h`, `uart_manager.cpp` |
| **`config/`** | 10 testing profiles, feature flag definitions, hardware config structs | `feature_flags.h`, `test_profiles.h` |
| **`demo/`** | Telemetry simulator / Mock mode for offline testing without hardware | `demo_manager.h`, `demo_manager.cpp` |
| **`errors/`** | Persistent circular error log buffer and WebSerial terminal system | `error_logger.h`, `error_logger.cpp` |
| **`hardware/`** | High-level hardware abstraction layer and pin mapping | `hardware_manager.h` |
| **`mission/`** | Mission state machine (Idle, Manual, Teach, Repeat, Error, E-Stop) | `mission_manager.h`, `mission_manager.cpp` |
| **`rfid/`** | RC522 SPI RFID driver, ground station detection, `/rfid/tags.json` manager | `rfid_manager.h`, `rfid_manager.cpp` |
| **`routes/`** | Dijkstra shortest-path topological graph solver and trajectory replay | `route_manager.h`, `route_manager.cpp` |
| **`system/`** | LittleFS storage manager, system wipe, reboot, and Wi-Fi Dual AP+STA | `system_manager.h`, `system_manager.cpp` |
| **`web/`** | HTTP REST endpoints, LittleFS static file server, and 20Hz WebSocket stream | `web_server.h`, `web_server.cpp`, `websocket.cpp` |
