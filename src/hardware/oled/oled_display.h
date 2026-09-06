#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Arduino.h>
#include "demo/demo_manager.h"

// OLED I2C Pins
#define OLED_SDA_PIN  4
#define OLED_SCL_PIN  5
#define OLED_ADDR     0x3C
#define OLED_WIDTH    128
#define OLED_HEIGHT   64

// Initialize the OLED display hardware
void oledInit();

// Full boot animation sequence (~7 seconds, blocking, call once in setup)
void oledBootSequence();

// Live status screen (call every 500ms from a task)
void oledUpdateLive(const TelemetryData &tele, const String &mode, const String &state,
                    bool stm32Connected, bool wifiSTA, const String &wifiIP, int8_t rssi);

#endif // OLED_DISPLAY_H
