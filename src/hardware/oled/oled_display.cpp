#include "hardware/oled/oled_display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

static Adafruit_SSD1306 disp(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool oledReady = false;
static uint16_t animFrame = 0;  // Global animation frame counter

// ═══════════════════════════════════════════════════════════════════════════
//  TINY BITMAPS
// ═══════════════════════════════════════════════════════════════════════════

// 16x16 AGV logo icon (top-down vehicle silhouette)
static const uint8_t agvLogo[] PROGMEM = {
    0x03, 0xC0, 0x04, 0x20, 0x1F, 0xF8, 0x20, 0x04,
    0x4F, 0xF2, 0x48, 0x12, 0x48, 0x12, 0x4F, 0xF2,
    0x48, 0x12, 0x48, 0x12, 0x4F, 0xF2, 0x20, 0x04,
    0x1F, 0xF8, 0x04, 0x20, 0x03, 0xC0, 0x00, 0x00,
};

// ═══════════════════════════════════════════════════════════════════════════
//  DRAWING HELPERS
// ═══════════════════════════════════════════════════════════════════════════

static void drawDoubleFrame() {
    disp.drawRoundRect(0, 0, 128, 64, 4, SSD1306_WHITE);
    disp.drawRoundRect(2, 2, 124, 60, 3, SSD1306_WHITE);
}

static void drawCentered(const char *text, int16_t y, uint8_t sz = 1) {
    disp.setTextSize(sz);
    int16_t x1, y1;
    uint16_t w, h;
    disp.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    disp.setCursor((128 - w) / 2, y);
    disp.print(text);
}

// Small ✓ checkmark drawn with lines at position (x, y)
static void drawCheck(int16_t x, int16_t y) {
    disp.drawLine(x, y + 4, x + 3, y + 7, SSD1306_WHITE);
    disp.drawLine(x + 3, y + 7, x + 8, y + 1, SSD1306_WHITE);
    disp.drawLine(x, y + 5, x + 3, y + 8, SSD1306_WHITE);  // thicken
    disp.drawLine(x + 3, y + 8, x + 8, y + 2, SSD1306_WHITE);
}

// Battery icon with fill level at (x, y)
static void drawBatteryIcon(int16_t x, int16_t y, float pct, uint16_t color = SSD1306_BLACK) {
    disp.drawRect(x, y + 1, 11, 7, color);
    disp.fillRect(x + 11, y + 3, 2, 3, color); // nub
    int fill = (int)(pct / 100.0f * 7.0f);
    if (fill < 0) fill = 0;
    if (fill > 7) fill = 7;
    if (fill > 0) disp.fillRect(x + 2, y + 3, fill, 3, color);
}

// ═══════════════════════════════════════════════════════════════════════════
//  AGV VEHICLE ANIMATION (top-down, drawn with primitives)
// ═══════════════════════════════════════════════════════════════════════════

enum MoveDir { MOVE_IDLE = 0, MOVE_FWD, MOVE_REV, MOVE_LEFT, MOVE_RIGHT };

static MoveDir getMoveDir(const TelemetryData &t) {
    bool lPos = t.left_rpm > 8, lNeg = t.left_rpm < -8;
    bool rPos = t.right_rpm > 8, rNeg = t.right_rpm < -8;
    if (lPos && rPos)  return MOVE_FWD;
    if (lNeg && rNeg)  return MOVE_REV;
    if (lNeg && rPos)  return MOVE_LEFT;
    if (lPos && rNeg)  return MOVE_RIGHT;
    return MOVE_IDLE;
}

static void drawAGVVehicle(int16_t cx, int16_t cy, MoveDir dir, uint16_t frame) {
    // ── Body ──
    disp.drawRoundRect(cx - 7, cy - 11, 14, 22, 2, SSD1306_WHITE);

    // ── Wheels (4 small filled rects) ──
    disp.fillRect(cx - 10, cy - 8, 3, 6, SSD1306_WHITE);  // L-front
    disp.fillRect(cx - 10, cy + 3, 3, 6, SSD1306_WHITE);  // L-rear
    disp.fillRect(cx + 7,  cy - 8, 3, 6, SSD1306_WHITE);  // R-front
    disp.fillRect(cx + 7,  cy + 3, 3, 6, SSD1306_WHITE);  // R-rear

    // ── Front direction triangle ──
    disp.fillTriangle(cx - 3, cy - 9, cx + 3, cy - 9, cx, cy - 14, SSD1306_WHITE);

    // ── Headlights ──
    disp.drawPixel(cx - 4, cy - 12, SSD1306_WHITE);
    disp.drawPixel(cx + 4, cy - 12, SSD1306_WHITE);

    // ── Motion indicators ──
    int phase = frame % 4;

    if (dir == MOVE_FWD) {
        // Speed lines behind (below) the vehicle, cycling downward
        for (int i = 0; i < 3; i++) {
            int ly = cy + 13 + i * 3 + phase;
            int w = 8 - i * 2;
            if (ly < cy + 26) {
                disp.drawFastHLine(cx - w / 2, ly, w, SSD1306_WHITE);
            }
        }
    } else if (dir == MOVE_REV) {
        // Lines in front (above), cycling upward
        for (int i = 0; i < 3; i++) {
            int ly = cy - 16 - i * 3 - phase;
            int w = 8 - i * 2;
            if (ly > cy - 28) {
                disp.drawFastHLine(cx - w / 2, ly, w, SSD1306_WHITE);
            }
        }
    } else if (dir == MOVE_LEFT) {
        // Arrow on left side
        int ax = cx - 14 - phase;
        disp.drawLine(ax + 4, cy - 3, ax, cy, SSD1306_WHITE);
        disp.drawLine(ax, cy, ax + 4, cy + 3, SSD1306_WHITE);
        disp.drawFastHLine(ax, cy, 6, SSD1306_WHITE);
    } else if (dir == MOVE_RIGHT) {
        // Arrow on right side
        int ax = cx + 12 + phase;
        disp.drawLine(ax, cy - 3, ax + 4, cy, SSD1306_WHITE);
        disp.drawLine(ax + 4, cy, ax, cy + 3, SSD1306_WHITE);
        disp.drawFastHLine(ax - 2, cy, 6, SSD1306_WHITE);
    } else {
        // Idle: small pulsing dots near wheels
        if (frame % 2 == 0) {
            disp.drawPixel(cx - 12, cy - 5, SSD1306_WHITE);
            disp.drawPixel(cx + 12, cy + 5, SSD1306_WHITE);
        } else {
            disp.drawPixel(cx + 12, cy - 5, SSD1306_WHITE);
            disp.drawPixel(cx - 12, cy + 5, SSD1306_WHITE);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  COMPASS (small circular compass with rotating needle)
// ═══════════════════════════════════════════════════════════════════════════

static void drawCompass(int16_t cx, int16_t cy, int16_t r, float heading) {
    // Outer ring
    disp.drawCircle(cx, cy, r, SSD1306_WHITE);
    disp.drawCircle(cx, cy, r - 1, SSD1306_WHITE);

    // Cardinal tick marks (small lines at N/E/S/W)
    disp.drawFastVLine(cx, cy - r + 1, 3, SSD1306_WHITE);     // N
    disp.drawFastVLine(cx, cy + r - 3, 3, SSD1306_WHITE);     // S
    disp.drawFastHLine(cx + r - 3, cy, 3, SSD1306_WHITE);     // E
    disp.drawFastHLine(cx - r + 1, cy, 3, SSD1306_WHITE);     // W

    // "N" label above compass
    disp.setTextSize(1);
    disp.setCursor(cx - 2, cy - r - 9);
    disp.print("N");

    // Needle: filled triangle pointing in heading direction
    float rad = heading * PI / 180.0f;
    float sinH = sin(rad), cosH = cos(rad);

    // Tip of needle (pointing toward heading)
    int16_t tipX = cx + (int16_t)(sinH * (r - 3));
    int16_t tipY = cy - (int16_t)(cosH * (r - 3));

    // Two base points (perpendicular to heading, at center)
    float perpSin = sin(rad + PI / 2.0f);
    float perpCos = cos(rad + PI / 2.0f);
    int16_t b1x = cx + (int16_t)(perpSin * 2);
    int16_t b1y = cy - (int16_t)(perpCos * 2);
    int16_t b2x = cx - (int16_t)(perpSin * 2);
    int16_t b2y = cy + (int16_t)(perpCos * 2);

    disp.fillTriangle(tipX, tipY, b1x, b1y, b2x, b2y, SSD1306_WHITE);

    // Tail (opposite direction, thinner line)
    int16_t tailX = cx - (int16_t)(sinH * (r - 4) * 0.5f);
    int16_t tailY = cy + (int16_t)(cosH * (r - 4) * 0.5f);
    disp.drawLine(cx, cy, tailX, tailY, SSD1306_WHITE);

    // Center dot
    disp.fillCircle(cx, cy, 1, SSD1306_WHITE);
}

// ═══════════════════════════════════════════════════════════════════════════
//  INITIALISE
// ═══════════════════════════════════════════════════════════════════════════

void oledInit() {
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

    if (disp.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        oledReady = true;
        disp.clearDisplay();
        disp.setTextColor(SSD1306_WHITE);
        disp.display();
        Serial.println("OLED: SSD1306 128x64 initialized on I2C (GPIO 4/5).");
    } else {
        oledReady = false;
        Serial.println("OLED: SSD1306 NOT detected on I2C 0x3C!");
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  BOOT ANIMATION SEQUENCE  (~7 seconds)
// ═══════════════════════════════════════════════════════════════════════════

void oledBootSequence() {
    if (!oledReady) return;

    // ── Phase 1: Futuristic scan-line power-on effect (1.2s) ──
    for (int pass = 0; pass < 3; pass++) {
        disp.clearDisplay();
        for (int y = 0; y < 64; y += 2) {
            disp.drawFastHLine(0, y, 128, SSD1306_WHITE);
            if (y % 6 == 0) {
                disp.display();
                delay(8);
            }
        }
        delay(40);
        // Quick inverted flash
        disp.invertDisplay(true);
        delay(30);
        disp.invertDisplay(false);
        disp.clearDisplay();
        disp.display();
        delay(60);
    }

    // ── Phase 2: Logo reveal with typewriter effect (1.8s) ──
    disp.clearDisplay();
    drawDoubleFrame();
    disp.display();
    delay(150);

    // AGV icon drops in from top
    for (int y = -16; y <= 6; y += 2) {
        disp.clearDisplay();
        drawDoubleFrame();
        disp.drawBitmap(56, y, agvLogo, 16, 16, SSD1306_WHITE);
        disp.display();
        delay(15);
    }
    delay(100);

    // "TARSLIFT" typed out letter by letter (size 2 = 12px wide each)
    const char* title = "TARSLIFT";
    disp.setTextSize(2);
    disp.setTextColor(SSD1306_WHITE);
    int16_t x1, y1;
    uint16_t tw, th;
    disp.getTextBounds(title, 0, 0, &x1, &y1, &tw, &th);
    int startX = (128 - tw) / 2;
    for (int i = 0; i < 8; i++) {
        disp.setCursor(startX + i * 12, 24);
        disp.print(title[i]);
        disp.display();
        delay(60);
    }
    delay(100);

    // Subtitle slides in
    disp.setTextSize(1);
    drawCentered("A G V   S Y S T E M", 44);
    disp.display();
    delay(400);

    // ── Phase 3: Animated progress bar with percentage (1.5s) ──
    disp.drawRoundRect(14, 53, 100, 8, 3, SSD1306_WHITE);
    disp.display();
    delay(80);

    for (int w = 0; w <= 96; w += 3) {
        disp.fillRoundRect(16, 55, w, 4, 2, SSD1306_WHITE);

        // Percentage text
        disp.fillRect(90, 44, 35, 8, SSD1306_BLACK); // clear old pct
        disp.setCursor(106, 44);
        int pct = (w * 100) / 96;
        disp.print(pct);
        disp.print("%");
        disp.display();
        delay(14);
    }
    delay(400);

    // ── Phase 4: System Check screen (2.5s) ──
    disp.clearDisplay();
    drawDoubleFrame();

    // Inverted title bar
    disp.fillRoundRect(16, 4, 96, 13, 3, SSD1306_WHITE);
    disp.setTextColor(SSD1306_BLACK);
    disp.setTextSize(1);
    drawCentered("SYSTEM CHECK", 7);
    disp.setTextColor(SSD1306_WHITE);
    disp.display();
    delay(300);

    // Check items appear one by one
    const char* items[] = {"STM32", "RFID", "UART", "BATTERY"};
    int itemY[] = {21, 30, 39, 48};

    for (int i = 0; i < 4; i++) {
        // Item name with dot leader
        disp.setCursor(14, itemY[i]);
        disp.print(items[i]);

        // Animated dots
        for (int d = 0; d < 3; d++) {
            disp.setCursor(70 + d * 6, itemY[i]);
            disp.print(".");
            disp.display();
            delay(60);
        }

        // Clear dots, show checkmark + OK
        disp.fillRect(70, itemY[i], 40, 8, SSD1306_BLACK);
        drawCheck(90, itemY[i] - 1);
        disp.display();
        delay(120);
    }

    delay(800);

    // ── Phase 5: "READY" screen with version (hold 1.5s) ──
    disp.clearDisplay();
    drawDoubleFrame();

    // Decorative corner accents
    disp.drawPixel(5, 5, SSD1306_WHITE);
    disp.drawPixel(6, 5, SSD1306_WHITE);
    disp.drawPixel(5, 6, SSD1306_WHITE);
    disp.drawPixel(122, 5, SSD1306_WHITE);
    disp.drawPixel(121, 5, SSD1306_WHITE);
    disp.drawPixel(122, 6, SSD1306_WHITE);
    disp.drawPixel(5, 57, SSD1306_WHITE);
    disp.drawPixel(6, 57, SSD1306_WHITE);
    disp.drawPixel(5, 58, SSD1306_WHITE);
    disp.drawPixel(122, 57, SSD1306_WHITE);
    disp.drawPixel(121, 57, SSD1306_WHITE);
    disp.drawPixel(122, 58, SSD1306_WHITE);

    disp.drawBitmap(56, 8, agvLogo, 16, 16, SSD1306_WHITE);

    disp.setTextSize(1);
    drawCentered("TARSLIFT AGV", 28);

    // Decorative separator
    disp.drawFastHLine(24, 38, 80, SSD1306_WHITE);
    disp.drawPixel(22, 38, SSD1306_WHITE);
    disp.drawPixel(104, 38, SSD1306_WHITE);

    drawCentered("SYSTEM READY", 43);

    disp.setCursor(44, 54);
    disp.print("v1.0.1");

    disp.display();
    delay(2000);
}

// ═══════════════════════════════════════════════════════════════════════════
//  LIVE STATUS SCREEN  (called every 500ms)
// ═══════════════════════════════════════════════════════════════════════════

void oledUpdateLive(const TelemetryData &tele, const String &mode, const String &state,
                    bool stm32Connected, bool wifiSTA, const String &wifiIP, int8_t rssi) {
    if (!oledReady) return;
    animFrame++;

    disp.clearDisplay();

    // ── Top bar (inverted, 11px tall) ──
    disp.fillRect(0, 0, 128, 11, SSD1306_WHITE);
    disp.setTextColor(SSD1306_BLACK);
    disp.setTextSize(1);

    // Voltage (e.g. "12.4V")
    char voltBuf[8];
    snprintf(voltBuf, sizeof(voltBuf), "%.1fV", tele.battery_volt);
    disp.setCursor(1, 2);
    disp.print(voltBuf);

    // Power in Watts (e.g. "5.2W")
    float power = fabs(tele.battery_volt * tele.battery_curr);
    char wattBuf[8];
    if (power >= 100.0f) {
        snprintf(wattBuf, sizeof(wattBuf), "%.0fW", power);
    } else {
        snprintf(wattBuf, sizeof(wattBuf), "%.1fW", power);
    }
    disp.setCursor(34, 2);
    disp.print(wattBuf);

    // Battery icon + percentage
    drawBatteryIcon(67, 1, tele.battery_pct, SSD1306_BLACK);
    disp.setCursor(82, 2);
    char batBuf[6];
    snprintf(batBuf, sizeof(batBuf), "%d%%", (int)tele.battery_pct);
    disp.print(batBuf);

    // STM32 indicator (rightmost)
    disp.setCursor(109, 2);
    disp.print(stm32Connected ? "STM" : "---");

    disp.setTextColor(SSD1306_WHITE);

    // ── Separator ──
    disp.drawFastHLine(0, 12, 128, SSD1306_WHITE);

    // ── Main area: AGV (left) | Compass (right) ──
    // Shrunk to fit y=13..43 (30px)
    disp.drawFastVLine(63, 13, 30, SSD1306_WHITE);

    // AGV Vehicle (left half, centered at 31, 27) — compact
    MoveDir dir = getMoveDir(tele);
    drawAGVVehicle(31, 27, dir, animFrame);

    // Compass (right half, centered at 93, 25, radius 10) — compact
    drawCompass(93, 25, 10, tele.heading);

    // Heading value below compass
    disp.setTextSize(1);
    char hdgBuf[8];
    snprintf(hdgBuf, sizeof(hdgBuf), "%.0f", tele.heading);
    int16_t x1, y1;
    uint16_t tw, th;
    disp.getTextBounds(hdgBuf, 0, 0, &x1, &y1, &tw, &th);
    disp.setCursor(93 - tw / 2, 37);
    disp.print(hdgBuf);
    disp.print((char)247); // degree symbol

    // ── Bottom separator ──
    disp.drawFastHLine(0, 44, 128, SSD1306_WHITE);

    // ── Row 1 (y=46): Mode | State | Speed ──
    disp.setCursor(1, 46);
    disp.print(mode);
    disp.setCursor(36, 46);
    disp.print("|");
    disp.setCursor(42, 46);
    disp.print(state);

    // Speed (average of left+right RPM → simple speed value)
    float speed = (fabs(tele.left_rpm) + fabs(tele.right_rpm)) / 2.0f;
    char spdBuf[12];
    snprintf(spdBuf, sizeof(spdBuf), "%.0fRPM", speed);
    disp.setCursor(80, 46);
    disp.print(spdBuf);

    // ── Row 2 (y=56): IP address | Uptime ──
    disp.setCursor(1, 56);
    if (wifiSTA && wifiIP.length() > 0 && wifiIP != "0.0.0.0") {
        // Connected to STA router — show STA IP
        disp.print(wifiIP);
    } else {
        // Not connected to STA — show AP IP
        disp.print("192.168.4.1");
    }

    // Uptime (right side)
    unsigned long upSec = millis() / 1000;
    char upBuf[10];
    snprintf(upBuf, sizeof(upBuf), "%02lu:%02lu", (upSec % 3600) / 60, upSec % 60);
    disp.setCursor(96, 56);
    disp.print(upBuf);

    disp.display();
}
