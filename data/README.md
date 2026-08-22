# Web Application & LittleFS Assets (`data/`)

This directory contains the client-side single-page application (SPA) served directly from the ESP32's LittleFS flash partition.

---

## 1. Asset Structure

```text
data/
├── index.html          # Main HTML5 Single Page Application
├── style.css           # Glassmorphism dark industrial design theme
├── app.js              # Real-time WebSocket dispatcher & state manager
├── config/
│   └── settings.json   # Serialized system settings
├── rfid/
│   └── tags.json       # Registered station tags
└── routes/
    └── *.json          # Saved trajectory routes
```

---

## 2. Flashing Web Assets to ESP32 LittleFS

When updating `index.html`, `style.css`, or `app.js`, the LittleFS partition image must be built and uploaded to flash memory:

### Using PlatformIO (VS Code)
1. Open PlatformIO sidebar.
2. Under **Project Tasks** ➔ **esp32-s3-devkitc-1** ➔ **Platform**:
3. Click **Build Filesystem Image**.
4. Click **Upload Filesystem Image**.
*(Or run in terminal: `pio run --target uploadfs`)*
