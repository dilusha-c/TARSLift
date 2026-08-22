# TARSLIFT AGV - Web Dashboard Manual & Operating Guide

The TARSLIFT Web Dashboard provides an industrial-grade interface rendered in pure HTML5, vanilla CSS (glassmorphism theme), and JavaScript. It communicates bi-directionally over low-latency WebSockets at 20Hz.

---

## 1. Top Navigation & Status Bar

- **System Mode Badge**: Displays current state (`IDLE`, `MANUAL`, `TEACH`, `REPEAT`, `ERROR`, `ESTOP`).
- **Safety Indicator**: Live watchdog status between ESP32 and STM32. Green indicates active packet stream; Red indicates link fault or E-Stop.
- **Battery Gauge**: Real-time voltage (V), current (mA), power (mW), and computed state-of-charge (SoC %).
- **Wi-Fi Indicator**: Signal RSSI and connected IP address (`192.168.4.1` on AP Hotspot, or LAN IP in STA mode).

---

## 2. Dashboard Sections & Tabs

### 2.1 📊 Dashboard (Home)
- **Real-Time Odometry**:
  - Distance Left / Right ($mm$).
  - Current Heading Yaw ($\theta^\circ$).
  - Estimated $(X, Y)$ coordinate plane plot.
- **Time-of-Flight Radar**:
  - 3-sector distance visualization for Left, Center, and Right laser sensors.
  - Proximity warning zones (< 200mm = Critical Stop, 200-500mm = Slowdown Warning).
- **IMU Orientation & G-Force**:
  - Pitch, Roll, and Yaw angles from MPU6050 sensor fusion.
  - Accelerometer force vector ($g$).
- **Active RFID Node**:
  - Displays last scanned RFID card UID, mapped Station Name, and timestamp.

### 2.2 🎮 Manual Drive Tab
- **Virtual Joystick / Directional D-Pad**:
  - Forward, Reverse, Left Pivot, Right Pivot, Smooth Turn Forward/Backward.
  - Speed Throttle Slider (0% to 100% PWM / mm/s target).
- **Emergency Stop (E-STOP)**:
  - High-priority button that immediately clamps PWM to 0, cancels any active mission, and holds motors in electronic brake mode.

### 2.3 🎓 Teach Mode Tab
1. Place the AGV over the initial RFID station tag.
2. Click **Start Recording Route**.
3. Drive the AGV manually to the next station(s).
4. The system logs encoder deltas, yaw increments, and RFID node checkpoints in real time.
5. Click **Save Route** and assign a human-readable identifier (e.g., `Station_A_to_B`).
6. Routes are serialized into JSON and stored in LittleFS at `/routes/{id}.json`.

### 2.4 🔁 Repeat / Autonomous Navigation Tab
- **Point-to-Point Waypoint Replay**: Select a recorded route from the dropdown and click **Start Mission**.
- **Topological Shortest-Path Dispatch**:
  - Select **Start Tag** and **Destination Tag**.
  - The ESP32 evaluates the graph using Dijkstra's algorithm.
  - Displays computed shortest path length and node sequence before autonomous dispatch.
- **Mission Execution HUD**: Live progress bar showing percentage of trajectory completed, segment countdown, and live obstacle pause status.

### 2.5 🏷️ RFID Tag Management Tab
- Scans new cards in real time.
- Allows assigning aliases (e.g., `Assembly Line 1`, `Charging Dock`, `Warehouse Bay 3`).
- Persists tag mappings in `/rfid/tags.json`.

### 2.6 ⚙️ Settings & Diagnostics Tab
- **Profile Selector**: Instantly toggle between 10 operating/testing profiles (`CUSTOM`, `MOTOR_TEST`, `ENCODER_TEST`, `FULL_SYSTEM`, etc.).
- **Motor Calibration**: Left/Right trim sliders, wheel circumference ($mm$), encoder PPR tuning.
- **PID Tuning**: Real-time gain adjustment for $K_p$, $K_i$, and $K_d$ sent directly over UART to the STM32 PID loop.
- **ToF Thresholds**: Adjustable obstacle stop distance (mm).
- **WebSerial Terminal**: Live 115200 Baud debug log feed with command inputs (`/help`, `/scan`, `/stop`, `/clear`, `/reboot`).

