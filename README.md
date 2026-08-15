# TARSLIFT AGV - ESP32-S3 Web Dashboard & Firmware

TARSLIFT AGV is a standalone high-level control system and dashboard web server hosted directly on an ESP32-S3 microcontroller. A web client (PC, phone, tablet) connects to the ESP32 via Wi-Fi and retrieves telemetry data via WebSocket or sends REST API requests.

The ESP32 communicates with a STM32F103C8T6 co-processor via UART to handle real-time motor PID loops, encoders, and sensor integrations.

---

## Workspace Structure

The project has been refactored into the following clean layout:
```text
TARSLIFT_AGV/
├── platformio.ini              # PlatformIO project configuration
├── README.md                   # Setup and system instructions
├── src/                        # ESP32-S3 Arduino C++ source code
│   ├── main.cpp                # Core boot setup and main loop
│   ├── config.h                # System Profiles, states, pin mapping
│   ├── config/                 # Header configs (test_profiles.h, feature_flags.h)
│   ├── web/                    # HTTP & WebSockets (web_server, websocket)
│   ├── mission/                # Mission state managers
│   ├── routes/                 # Trajectory Route Manager
│   ├── rfid/                   # RFID readers
│   ├── battery/                # Battery monitor
│   ├── errors/                 # Error Logger
│   ├── system/                 # Diagnostics and system info managers
│   ├── demo/                   # Telemetry simulator
│   ├── communication/          # UART bus link (stm32_uart.h, stm32_uart.cpp)
│   └── hardware/               # Peripheral drivers (motor, encoder, mpu6050, tof)
└── data/                       # LittleFS partition web files
    ├── index.html              # Dashboard interface (Dark Industrial)
    ├── style.css               # Styling definitions (pure vanilla CSS)
    ├── app.js                  # Telemetry parsing & driving controllers
    ├── pages/                  # Static panel section stubs
    ├── js/                     # Component script stubs
    ├── css/                    # Component stylesheet stubs
    ├── rfid/
    │   └── tags.json           # Registered tags database
    ├── routes/
    │   └── route_001.json      # Sample saved route trajectory
    └── config/
        └── settings.json       # Serialized system configuration file
```

---

## 1. System Testing Profiles (`src/config.h`)

Open `src/config.h` or navigate to the Settings page. There are 10 predefined configurations (0-9):

* **Profile 0: `CUSTOM`**
  * **Description**: Allows you to check/uncheck individual subsystems, features, and logs directly in the Settings.
* **Profile 1: `MOTOR_TEST` (Default)**
  * **Description**: Enables DC Motors and Manual driving controls.
* **Profile 2: `ENCODER_TEST`**
  * **Description**: Encoders and Motors enabled. Used for telemetry calibration.
* **Profile 3: `MPU6050_TEST`**
  * **Description**: MPU6050 enabled for IMU drift calibration.
* **Profile 4: `RFID_TEST`**
  * **Description**: RFID reader and Tag database manager active.
* **Profile 5: `TOF_TEST`**
  * **Description**: Time-of-Flight sensors and Obstacle avoidance active.
* **Profile 6: `BATTERY_TEST`**
  * **Description**: INA219 current/voltage diagnostics active.
* **Profile 7: `TEACH_TEST`**
  * **Description**: Manual controls, encoders, MPU, and RFID enabled to teach routes.
* **Profile 8: `REPEAT_TEST`**
  * **Description**: Enables repeat mission playback, sensors, motors, and batteries.
* **Profile 9: `FULL_SYSTEM`**
  * **Description**: All hardware drivers, communication ports, and safety logs enabled. Final deploy profile.

---

## 2. Settings Tabbed Panel Navigation

The Settings page is organized into three distinct sub-tabs:
1. **Profiles & Flags**: Select testing profiles, or check/uncheck modular features and motion/sensor options under CUSTOM profile.
2. **Battery Settings**: Configure INA219 logging switches and input warning thresholds (Low Voltage, Critical Voltage, Battery %).
3. **System & Access Point**: Modify WiFi SSID/Password, toggle simulation mode (`DEMO_MODE`), reset to profile defaults, or perform a system wipe.

---

## 3. Administrative System Wipe (Password: `1234`)

To wipe all data from the AGV flash memory and restart the system:
1. Go to the **Settings** (⚙) page ➔ **System & Access Point** tab.
2. Click the **🔴 WIPE ALL SYSTEM DATA** button.
3. When prompted, type the administrative password:
   ```text
   1234
   ```
4. Clicking OK erases all saved routes (`/routes/`), logs (`/logs/`), RFID tag databases (`/rfid/tags.json`), and custom system configurations, then programmatically reboots the ESP32-S3.

---

## 4. Compilation and Uploading

### Compiling and Uploading Firmware
Open the workspace directory in PlatformIO (VS Code):
1. Compile the firmware.
2. Upload the compiled binary to the ESP32-S3 board.

### Uploading LittleFS Assets
The web assets located in the `data/` folder must be uploaded to the ESP32-S3 flash partition using the LittleFS filesystem:
1. Run the **Build Filesystem Image** command in PlatformIO.
2. Run the **Upload Filesystem Image** command to write the web dashboard files.
