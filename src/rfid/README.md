# RFID Station & Node Detection Subsystem (`src/rfid/`)

This directory manages the MFRC522 RFID reader over hardware SPI and maintains the station tag database in LittleFS.

---

## 1. Hardware Interface

The RC522 communicates directly with the ESP32-S3 over SPI:
- **SCK**: GPIO 10
- **MISO**: GPIO 9
- **MOSI**: GPIO 12
- **SS / SDA**: GPIO 13
- **RST**: GPIO 11

Operates at 13.56MHz using standard ISO/IEC 14443 Type A RFID cards/tags (Mifare Classic 1K / Ultralight).

---

## 2. Node Detection & Ground Station Mapping

RFID tags are installed along the AGV track (on the floor) to serve as ground-truth topological nodes:
1. When the AGV drives over a tag, the RC522 reads the Unique Identifier (UID) (e.g., `A3:B4:C5:D6`).
2. The `RFIDManager` looks up the UID in `/rfid/tags.json`.
3. If mapped, the station name (e.g., `Assembly_Station_2`) is broadcast to the Web Dashboard.
4. If in **Teach Mode**, the UID is embedded into the route file as a node checkpoint.
5. If in **Repeat Mode**, the UID triggers node arrival events, resetting dead-reckoning odometry drift.

---

## 3. Tag Database Format (`/rfid/tags.json`)

```json
[
  {
    "uid": "1342672322",
    "name": "Station A - Loading Dock",
    "x": 0.0,
    "y": 0.0
  },
  {
    "uid": "2451789012",
    "name": "Station B - Assembly Area",
    "x": 4500.0,
    "y": 1200.0
  }
]
```
