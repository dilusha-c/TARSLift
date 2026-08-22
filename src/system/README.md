# System Management, LittleFS & Wi-Fi Subsystem (`src/system/`)

This directory contains system utilities, non-volatile storage routines, Wi-Fi networking, and administrative commands.

---

## 1. Wi-Fi Dual AP + STA Operating Mode

The ESP32-S3 boots into concurrent Access Point (AP) and Station (STA) mode:
- **SoftAP Hotspot**:
  - SSID: `TARSLIFT_AGV_AP` (or user-defined in settings)
  - Default IP: `192.168.4.1`
  - Allows direct peer-to-peer tablet/phone control in factory settings without existing Wi-Fi routers.
- **Station (STA) Client Mode**:
  - Connects in the background to facility Wi-Fi.
  - Automatically reconnects if router signal is lost.

---

## 2. LittleFS Flash File System Management

All configuration, logs, and route data are stored in the internal SPI Flash partition formatted with LittleFS:
- `/index.html`, `/style.css`, `/app.js`: Web assets.
- `/config/settings.json`: System profiles & calibration parameters.
- `/rfid/tags.json`: RFID tag alias database.
- `/routes/*.json`: Saved trajectory routes.
- `/logs/*.log`: Persistent error logs.

---

## 3. Administrative System Wipe (Password: `1234`)

Executing `/api/system/wipe` with password `1234`:
1. Iterates recursively through `/routes/`, `/logs/`, `/rfid/`, and `/config/`.
2. Unlinks all JSON and log files.
3. Formats the LittleFS partition.
4. Restores default factory profile settings.
5. Reboots the ESP32-S3 after a 1.0-second delay.
