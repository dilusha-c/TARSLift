# Error Logging & WebSerial Subsystem (`src/errors/`)

This directory implements the central diagnostic logging engine, persistent flash crash logging, and browser WebSerial terminal.

---

## 1. Features

- **Multi-Level Logging**: `INFO`, `WARN`, `ERROR`, `CRITICAL`.
- **In-Memory Ring Buffer**: Stores the most recent 50 console lines in RAM. When a new browser client connects via WebSocket, the entire buffer is dumped to the UI terminal.
- **Persistent Flash Logs**: Critical faults (E-Stop, Watchdog timeout, Low Battery) are appended to `/logs/errors_{date}.log` in LittleFS.
- **Interactive WebSerial Console**: Allows bidirectional interactive CLI commands from the Web Dashboard.

---

## 2. Supported WebSerial Commands

| Command | Action |
| :--- | :--- |
| `/help` | Lists all available console commands |
| `/scan` | Simulates an RFID tag scan for testing |
| `/clear` | Purges persistent error logs from LittleFS |
| `/stop` | Forces immediate mission abort / E-Stop |
| `/reboot` | Triggers a software reset on the ESP32-S3 |

---

## 3. Key Functions (`error_logger.h`)

- `void logError(ErrorLevel level, const String &msg)`: Logs timestamped event with severity tag.
- `void webSerialPrint(const String &text)`: Prints to hardware UART0 and streams to WebSocket clients.
- `void webSerialPrintln(const String &text)`: Prints line with newline.
- `void handleWebSerialCommand(const String &cmd)`: Dispatches interactive CLI commands.
