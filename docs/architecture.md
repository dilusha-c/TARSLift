# TARSLIFT AGV High-Level Architecture

The TARSLIFT AGV controller system runs a split-processor architecture:
1. **ESP32-S3**: Hosts the Wi-Fi AP, serves the dashboard assets, parses LittleFS routes, tracks profiles, and logs error reports.
2. **STM32F103**: Real-time motor speed PID control, wheel encoders, IMU, and ToF ranging loops.
