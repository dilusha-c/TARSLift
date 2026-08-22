# Telemetry Simulator & Demo Mode (`src/demo/`)

This directory contains the `DemoManager` which generates synthetic telemetry when physical AGV hardware is not connected.

---

## 1. Purpose & Use Cases

- **Offline UI Development**: Allows testing of the Web Dashboard, WebSocket streaming, and UI layout on a PC/phone without needing physical STM32, motors, or sensors.
- **Exhibition / Showcase Mode**: Simulates realistic motion trajectories, battery discharge curves, and obstacle radar sweeps for demonstrations.

---

## 2. Simulation Logic

When `DEMO_MODE` is enabled in Settings:
- **Kinematic Simulation**: Computes $(X,Y)$ position and yaw rotation dynamically based on virtual joystick commands.
- **Simulated ToF Radar**: Sweeps distance values ($200\text{mm} \dots 1500\text{mm}$) with Gaussian noise.
- **Battery Discharge**: Simulates realistic battery drain based on virtual motor throttle.
- **Virtual RFID Scanner**: Can trigger card detections via the WebSerial `/scan` command.
