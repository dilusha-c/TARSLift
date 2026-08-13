# TARSLIFT AGV - ESP32-S3 Web Dashboard & Firmware

TARSLIFT AGV is a standalone high-level control system and dashboard web server hosted directly on an ESP32-S3 microcontroller. A web client (PC, phone, tablet) connects to the ESP32 via Wi-Fi and retrieves telemetry data via WebSocket or sends REST API requests.

The ESP32 communicates with a STM32F103C8T6 co-processor via UART to handle real-time motor PID loops, encoders, and sensor integrations.

---

## Workspace Structure

```text
TARSLIFT_AGV/
├── platformio.ini              # PlatformIO project configuration
├── README.md                   # Setup and system instructions
├── src/                        # ESP32-S3 Arduino C++ source code
│   ├── main.cpp                # Core boot setup and main loop
│   ├── config.h                # System Profiles, states, pin mapping
│   ├── system_manager.h/.cpp   # Memory, Uptime and Hardware diagnostics
│   ├── demo_manager.h/.cpp     # Mock telemetry simulator
│   ├── error_logger.h/.cpp     # Daily logs filesystem controller
│   ├── rfid_manager.h/.cpp     # Mappings RFID database
│   ├── route_manager.h/.cpp    # State-machine for Teach-and-Repeat
│   ├── uart_manager.h/.cpp     # STM32 UART (Intentionally empty stubs)
│   ├── web_server.h/.cpp       # Static server and REST API routers
│   └── websocket.h/.cpp        # Telemetry broadcaster
└── data/                       # LittleFS partition web files
    ├── index.html              # Dashboard interface (Dark Industrial)
    ├── style.css               # Styling definitions (pure vanilla CSS)
    ├── app.js                  # Telemetry parsing & driving controllers
    ├── rfid/
    │   └── tags.json           # Registered tags database
    ├── routes/
    │   └── route_001.json      # Sample saved route trajectory
    └── logs/
        └── 2026-08-13.log      # Example daily log file
```

---

## 1. System Testing Profiles (`src/config.h`)

Open `src/config.h` and change `#define TEST_PROFILE` to the desired setting:

* **Profile 1: `ROUTE_TEST` (Default)**
  * **Demo Mode**: ON
  * **Hardware Modules**: Disabled (ESP32-S3 and WebSocket Active)
  * **Purpose**: Test route creation, manual keyboard teleoperation driving, Repeat Mission player, and RFID manager registration entirely in simulation without physical sensors.
* **Profile 2: `MOTOR_TEST`**
  * **Demo Mode**: OFF
  * **Hardware Modules**: Motor & Encoders Enabled (via STM32 co-processor UART)
  * **Purpose**: Test real-time motor commands and encoder feedbacks.
* **Profile 3: `SENSOR_TEST`**
  * **Demo Mode**: OFF
  * **Hardware Modules**: MPU6050, RFID, ToF, and Battery Enabled.
* **Profile 4: `RFID_TEST`**
  * **Demo Mode**: OFF
  * **Hardware Modules**: RFID Scanner & RFID Manager Enabled.
* **Profile 5: `FULL_SYSTEM`**
  * **Demo Mode**: OFF
  * **Hardware Modules**: All enabled. Final AGV deployment.

---

## 2. Compilation and Uploading

### Compiling and Uploading Firmware
Open the workspace directory in PlatformIO (VS Code) or configure the Arduino IDE (selecting the "ESP32S3 Dev Module" board):
1. Compile the firmware.
2. Upload the compiled binary to the ESP32-S3 board.

### Uploading LittleFS Assets
The web assets located in the `data/` folder must be uploaded to the ESP32-S3 flash partition using the LittleFS filesystem:
1. Run the **Build Filesystem Image** command in PlatformIO.
2. Run the **Upload Filesystem Image** command to write the web dashboard files.

---

## 3. How to Connect and Open Dashboard

1. Once powered, the ESP32-S3 creates a Wi-Fi Access Point:
   * **SSID**: `TARSLIFT_AGV`
   * **Password**: `12345678`
2. Connect your PC, phone, or tablet to the `TARSLIFT_AGV` Wi-Fi network.
3. Open a browser and navigate to:
   ```text
   http://192.168.4.1
   ```
4. The dashboard will load directly from the ESP32-S3 and start displaying real-time telemetry updates.
