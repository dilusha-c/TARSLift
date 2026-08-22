# Hardware Abstraction Layer (`src/hardware/`)

This directory provides the high-level hardware interfaces and pin definitions for the ESP32-S3 microcontroller.

---

## 1. Pin Configuration Summary

```cpp
// UART link to STM32F103
#define STM32_UART_RX_PIN 18  // ESP32 RX2 (Connected to STM32 PA2 / TX2)
#define STM32_UART_TX_PIN 17  // ESP32 TX2 (Connected to STM32 PA3 / RX2)

// SPI Bus for RC522 RFID Reader
#define RFID_SCK_PIN      10
#define RFID_MISO_PIN     9
#define RFID_MOSI_PIN     12
#define RFID_CS_PIN       13
#define RFID_RST_PIN      11

// Battery Voltage Divider ADC
#define BATTERY_ADC_PIN   1

// I2C Bus for INA219 Power Monitor
#define I2C_SDA_PIN       4
#define I2C_SCL_PIN       5
```

---

## 2. Abstraction Strategy

By isolating pin mappings and bus initializations to `hardware_manager.h`, the upper navigation and web layers remain independent of physical board revisions or pin reassignments.
