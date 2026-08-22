# Web Server & WebSocket Streaming (`src/web/`)

This directory implements the embedded HTTP server and high-frequency real-time WebSocket communication layer using `ESPAsyncWebServer`.

---

## 1. WebSocket Protocol & Telemetry Pipeline (`/ws`)

The ESP32 broadcasts a 20Hz JSON telemetry packet to all connected web clients:

```json
{
  "type": "telemetry",
  "state": "IDLE",
  "battery": { "v": 12.4, "pct": 92, "i": 420, "p": 5208 },
  "odom": { "l": 1240, "r": 1242, "yaw": 0.0, "x": 1241.0, "y": 0.0 },
  "tof": { "l": 450, "c": 820, "r": 480 },
  "imu": { "pitch": 0.2, "roll": -0.1, "yaw": 0.0, "gx": 0, "gy": 0, "gz": 0 },
  "rfid": { "uid": "1342672322", "name": "Station A" },
  "safety": { "estop": false, "comm_ok": true, "obstacle": false }
}
```

---

## 2. HTTP REST API Endpoints (`web_server.cpp`)

| Endpoint | Method | Payload / Parameters | Description |
| :--- | :--- | :--- | :--- |
| `/api/status` | `GET` | None | Returns current system state & hardware status |
| `/api/control/drive` | `POST` | `{"dir":"FWD","speed":50}` | Dispatches manual driving command |
| `/api/control/estop` | `POST` | None | Triggers emergency stop |
| `/api/routes` | `GET` | None | Lists all saved route files in LittleFS |
| `/api/routes/save` | `POST` | Route JSON | Saves recorded trajectory to LittleFS |
| `/api/routes/delete`| `POST` | `{"id":"route_001"}` | Deletes route from LittleFS |
| `/api/routes/start` | `POST` | `{"routeId":"route_001"}` | Starts repeat trajectory playback |
| `/api/routes/shortest_path`| `POST`| `{"start":"A","dest":"B"}`| Computes & starts shortest path mission |
| `/api/tags` | `GET`/`POST` | Tag JSON | Gets or registers RFID tags |
| `/api/settings` | `GET`/`POST` | Settings JSON | Reads or modifies system parameters |
| `/api/system/wipe`| `POST` | `{"password":"1234"}` | Formats flash and restores defaults |
| `/api/system/reboot`| `POST` | None | Reboots the ESP32-S3 |
