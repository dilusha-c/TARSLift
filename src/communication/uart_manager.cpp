#include "communication/uart_manager.h"
#include "config.h"
#include "system/system_manager.h"
#include "errors/error_logger.h"
#include "demo/demo_manager.h"
#include "AGV_Commands.h"
#include "AGV_Protocol.h"
#include "AGV_Responses.h"

// Hardware Serial instance for STM32 UART link

static HardwareSerial stm32Serial(2);
static AGV_Communication agvComm;

static unsigned long lastHeartbeatTime = 0;
static unsigned long lastRxPacketTime = 0;
static bool watchdogTriggered = false;
static bool lastConnState = false;
static uint32_t totalBytesReceived = 0;
static uint32_t totalPacketsDecoded = 0;
static unsigned long lastDiagSummaryTime = 0;

static uint8_t rxBuffer[128];
static size_t rxIndex = 0;
static enum {
    WAIT_SOF1,
    WAIT_SOF2,
    WAIT_CMD,
    WAIT_SEQ,
    WAIT_LEN_L,
    WAIT_LEN_H,
    WAIT_PAYLOAD,
    WAIT_CRC_L,
    WAIT_CRC_H
} rxState = WAIT_SOF1;

static uint16_t expectedLen = 0;
static AGVPacket rxPacket;

static void sendPacketOverUart(const AGVPacket& pkt) {
    uint8_t txBuf[128];
    size_t encodedLen = AGV_Protocol::encode(pkt, txBuf, sizeof(txBuf));
    if (encodedLen > 0) {
        stm32Serial.write(txBuf, encodedLen);
        
        // Wait for the ESP32 to finish transmitting the physical bytes
        stm32Serial.flush();
        
        // Add a tiny 2ms delay to give the STM32 time to send its ACK
        // and re-enable its RX interrupt. This prevents Overrun Errors (ORE)
        // when the user aggressively spams commands on the Web UI!
        delay(2);
        
        // --- DEBUG: Print raw packet to Serial Monitor ---
        // if (pkt.cmd != AGVCommand::HEARTBEAT) { // Ignore heartbeat spam
        //     String hexStr = "ESP32 TX RAW [" + String(encodedLen) + " bytes]: ";
        //     for (size_t i = 0; i < encodedLen; i++) {
        //         if (txBuf[i] < 0x10) hexStr += "0";
        //         hexStr += String(txBuf[i], HEX) + " ";
        //     }
        //     hexStr.toUpperCase();
        //     webSerialPrintln(hexStr);
        // }
    }
}

void stm32UartInit() {
    webSerialPrintln("==================================================");
    webSerialPrintln("STM32 UART LINK INITIALIZATION:");
    webSerialPrintln("  Status: " + String(sysSettings.enable_stm32_uart ? "ENABLED" : "DISABLED"));
    webSerialPrintln("  Port: Hardware Serial2");
    webSerialPrintln("  ESP32 RX Pin: GPIO " + String(STM32_UART_RX_PIN) + " (Connect to STM32 TX / PA2)");
    webSerialPrintln("  ESP32 TX Pin: GPIO " + String(STM32_UART_TX_PIN) + " (Connect to STM32 RX / PA3)");
    webSerialPrintln("  Baud Rate: " + String(AGVConfig::UART_BAUD_RATE) + " 8N1");
    webSerialPrintln("==================================================");

    if (sysSettings.enable_stm32_uart) {
        stm32Serial.begin(AGVConfig::UART_BAUD_RATE, SERIAL_8N1, STM32_UART_RX_PIN, STM32_UART_TX_PIN);
    }
    rxState = WAIT_SOF1;
    rxIndex = 0;
    lastRxPacketTime = 0; // Wait for real incoming packets before marking connected
    watchdogTriggered = false;
    lastConnState = false;
    totalBytesReceived = 0;
    totalPacketsDecoded = 0;
}

bool isStm32Connected() {
    if (!sysSettings.enable_stm32_uart) return false;
    if (sysSettings.demo_mode) return true;
    return (lastRxPacketTime > 0) && (millis() - lastRxPacketTime <= 1500);
}


void checkUartWatchdog() {
    if (!sysSettings.enable_stm32_uart || sysSettings.demo_mode) return;

    unsigned long now = millis();
    if (lastRxPacketTime > 0 && (now - lastRxPacketTime > 1000)) {
        if (!watchdogTriggered) {
            watchdogTriggered = true;
            Serial.println("STM32 UART WATCHDOG: Link Timeout (>1000ms)! E-STOP Triggered!");
            logError("STM32", "E02", "UART Link Lost (>1000ms) - Emergency Stop Interlock Triggered");
            sendStm32Stop();
        }
    } else if (now - lastRxPacketTime <= 1000) {
        watchdogTriggered = false;
    }
}

static void processIncomingPacket(const AGVPacket& pkt) {
    lastRxPacketTime = millis();
    switch (pkt.cmd) {
        case AGVCommand::ACK: {
            // ACK spam removed as per user request
            break;
        }

        case AGVCommand::STATUS: {
            uint8_t stateVal = pkt.payload[0];
            uint8_t errFlags = pkt.payload[1];
            webSerialPrintln("STM32 UART: Status update state=0x" + String(stateVal, HEX) + " errorFlags=0x" + String(errFlags, HEX));
            break;
        }

        case AGVCommand::VERSION: {
            if (pkt.length >= 5) {
                webSerialPrintln("STM32 UART: Version Proto " + String(pkt.payload[0]) + "." + String(pkt.payload[1]) + 
                                 " FW " + String(pkt.payload[2]) + "." + String(pkt.payload[3]) + "." + String(pkt.payload[4]));
            }
            break;
        }

        case AGVCommand::ERROR: {
            if (pkt.length >= 4) {
                uint16_t code = pkt.payload[0] | (pkt.payload[1] << 8);
                uint8_t severity = pkt.payload[2];
                uint8_t source = pkt.payload[3];
                logError("STM32", "0x" + String(code, HEX), "Hardware Severity " + String(severity) + " Source " + String(source));
                webSerialPrintln("STM32 UART ERROR: Code 0x" + String(code, HEX) + " Severity " + String(severity));
            }
            break;
        }

        case AGVCommand::ODOMETRY: {
            int32_t leftDistMm = 0, rightDistMm = 0;
            int16_t yawDegX10 = 0;
            if (AGV_Communication::parseOdometry(pkt, leftDistMm, rightDistMm, yawDegX10)) {
                float avgMeters = ((leftDistMm + rightDistMm) / 2.0f) / 1000.0f;
                float yawDeg = yawDegX10 / 10.0f;
                updateTelemetryOdometry(avgMeters, yawDeg);
            }
            break;
        }

        case AGVCommand::IMU_DATA: {
            int16_t ax, ay, az, gx, gy, gz, yawDegX10;
            if (AGV_Communication::parseIMUData(pkt, ax, ay, az, gx, gy, gz, yawDegX10)) {
                updateTelemetryHeading(yawDegX10 / 10.0f);
            }
            break;
        }

        case AGVCommand::TOF_DATA: {
            uint16_t leftMm, centerMm, rightMm;
            if (AGV_Communication::parseTofData(pkt, leftMm, centerMm, rightMm)) {
                updateTelemetryTof(leftMm, centerMm, rightMm);
                
                static unsigned long lastTofPrint = 0;
                unsigned long now = millis();
                if (now - lastTofPrint > 1000) {
                    webSerialPrintln("STM32 UART: TOF L=" + String(leftMm) + "mm C=" + String(centerMm) + "mm R=" + String(rightMm) + "mm");
                    lastTofPrint = now;
                }
            }
            break;
        }

        case AGVCommand::MOTOR_STATUS: {
            int16_t leftPwm, rightPwm;
            int8_t leftDir, rightDir;
            if (AGV_Communication::parseMotorStatus(pkt, leftPwm, rightPwm, leftDir, rightDir)) {
                // Previously this updated RPM with PWM values.
                // We'll leave it calling this but we really should separate PWM from RPM.
                // We'll let ENCODER_DATA overwrite it with real RPM.
            }
            break;
        }

        case AGVCommand::ENCODER_DATA: {
            int32_t leftPulses, rightPulses;
            if (AGV_Communication::parseEncoderData(pkt, leftPulses, rightPulses)) {
                static int32_t lastLeft = 0;
                static int32_t lastRight = 0;
                static unsigned long lastTime = 0;
                unsigned long now = millis();
                
                if (lastTime > 0 && now > lastTime) {
                    float dt = (now - lastTime) / 1000.0f;
                    
                    // The STM32 sends a 16-bit counter cast to int32_t. 
                    // To handle wraparound correctly, we calculate the delta as a signed 16-bit int
                    int16_t deltaLeft16 = (int16_t)(leftPulses - lastLeft);
                    int16_t deltaRight16 = (int16_t)(rightPulses - lastRight);
                    float dLeft = (float)deltaLeft16;
                    float dRight = (float)deltaRight16;
                    
                    // Maintain absolute count across wraparounds
                    static int32_t absLeft = 0;
                    static int32_t absRight = 0;
                    absLeft += deltaLeft16;
                    absRight += deltaRight16;
                    updateTelemetryRawEncoders(absLeft, absRight);
                    
                    float l_rpm = (dLeft / sysSettings.enc_ppr_l) / dt * 60.0f;
                    float r_rpm = (dRight / sysSettings.enc_ppr_r) / dt * 60.0f;
                    
                    float l_mms = (l_rpm / 60.0f) * sysSettings.wheel_circ_mm;
                    float r_mms = (r_rpm / 60.0f) * sysSettings.wheel_circ_mm;
                    
                    updateTelemetryMotors(l_rpm, r_rpm, l_mms, r_mms);
                }
                lastLeft = leftPulses;
                lastRight = rightPulses;
                lastTime = now;
            } else {
                webSerialPrintln("❌ [UART RX] Failed to parse ENCODER_DATA packet!");
            }
            break;
        }

        case AGVCommand::MOVE_DONE: {
            uint8_t origSeq = 0, res = 0;
            if (AGV_Communication::parseMoveDone(pkt, origSeq, res)) {
                // webSerialPrintln("STM32 UART: Move finished for seq=" + String(origSeq) + " result=" + String(res));
            }
            break;
        }

        case AGVCommand::TURN_DONE: {
            uint8_t origSeq = 0, res = 0;
            if (AGV_Communication::parseTurnDone(pkt, origSeq, res)) {
                // webSerialPrintln("STM32 UART: Turn finished for seq=" + String(origSeq) + " result=" + String(res));
            }
            break;
        }

        default:
            webSerialPrintln("STM32 UART: Received unknown CMD 0x" + String(pkt.cmd, HEX));
            break;
    }
}

void stm32UartUpdate() {
    if (!sysSettings.enable_stm32_uart) return;

    unsigned long now = millis();

    // Track connection state transitions for Web & Serial logging
    bool currConnected = isStm32Connected();
    if (currConnected != lastConnState) {
        lastConnState = currConnected;
        if (currConnected) {
            webSerialPrintln("✅ [UART LINK] STM32 connected successfully! (Packet stream active)");
        } else {
            webSerialPrintln("❌ [UART LINK] STM32 disconnected / lost! (>1500ms timeout)");
        }
    }

    // 1. Read incoming bytes from STM32 serial bus
    while (stm32Serial.available() > 0) {
        uint8_t b = stm32Serial.read();
        totalBytesReceived++;

        switch (rxState) {
            case WAIT_SOF1:
                if (b == AGVConfig::SOF1) {
                    rxIndex = 0;
                    rxBuffer[rxIndex++] = b;
                    rxState = WAIT_SOF2;
                } else {
                    static unsigned long lastSofWarn = 0;
                    if (now - lastSofWarn > 3000) {
                        lastSofWarn = now;
                        webSerialPrintln("⚠️ [UART RX DIAG] Received byte 0x" + String(b, HEX) + " instead of SOF1 (0xAA). Check TX/RX crossover or baud rate! (Total RX Bytes: " + String(totalBytesReceived) + ")");
                    }
                }
                break;

            case WAIT_SOF2:
                if (b == AGVConfig::SOF2) {
                    rxBuffer[rxIndex++] = b;
                    rxState = WAIT_CMD;
                } else {
                    rxState = WAIT_SOF1;
                }
                break;

            case WAIT_CMD:
                rxBuffer[rxIndex++] = b;
                rxPacket.cmd = b;
                rxState = WAIT_SEQ;
                break;

            case WAIT_SEQ:
                rxBuffer[rxIndex++] = b;
                rxPacket.seq = b;
                rxState = WAIT_LEN_L;
                break;

            case WAIT_LEN_L:
                rxBuffer[rxIndex++] = b;
                expectedLen = b;
                rxState = WAIT_LEN_H;
                break;

            case WAIT_LEN_H:
                rxBuffer[rxIndex++] = b;
                expectedLen |= (static_cast<uint16_t>(b) << 8);
                rxPacket.length = expectedLen;

                if (expectedLen > AGVConfig::MAX_PAYLOAD_SIZE) {
                    rxState = WAIT_SOF1;
                } else if (expectedLen == 0) {
                    rxState = WAIT_CRC_L;
                } else {
                    rxState = WAIT_PAYLOAD;
                }
                break;

            case WAIT_PAYLOAD:
                rxBuffer[rxIndex++] = b;
                rxPacket.payload[rxIndex - 6] = b;
                if ((rxIndex - 6) >= expectedLen) {
                    rxState = WAIT_CRC_L;
                }
                break;

            case WAIT_CRC_L:
                rxBuffer[rxIndex++] = b;
                rxState = WAIT_CRC_H;
                break;

            case WAIT_CRC_H:
                rxBuffer[rxIndex++] = b;
                rxPacket.crc = static_cast<uint16_t>(rxBuffer[rxIndex - 2]) | (static_cast<uint16_t>(b) << 8);
                rxState = WAIT_SOF1;

                if (AGV_Protocol::decode(rxBuffer, rxIndex, rxPacket)) {
                    totalPacketsDecoded++;
                    processIncomingPacket(rxPacket);
                } else {
                    webSerialPrintln("❌ [UART RX] CRC check failed on command 0x" + String(rxPacket.cmd, HEX));
                }
                break;
        }
    }

    // 2. Transmit V1 Heartbeat every 500 ms
    if (now - lastHeartbeatTime >= AGVConfig::HEARTBEAT_INTERVAL_MS) {
        lastHeartbeatTime = now;
        AGVPacket hbPkt;
        agvComm.heartbeat(hbPkt);
        sendPacketOverUart(hbPkt);
    }

    // 3. Periodic Diagnostic Troubleshooting Log (Every 10 seconds if disconnected)
    if (!currConnected && !sysSettings.demo_mode && (now - lastDiagSummaryTime >= 10000)) {
        lastDiagSummaryTime = now;
        webSerialPrintln("🔍 [UART DIAGNOSTIC SUMMARY]");
        webSerialPrintln("  ESP32 Pin Setup : RX = GPIO " + String(STM32_UART_RX_PIN) + ", TX = GPIO " + String(STM32_UART_TX_PIN) + " @ 115200 Baud");
        webSerialPrintln("  Total RX Bytes  : " + String(totalBytesReceived));
        webSerialPrintln("  Valid Packets   : " + String(totalPacketsDecoded));
        if (totalBytesReceived == 0) {
            webSerialPrintln("  --> TROUBLESHOOTING: 0 bytes received on RX Pin " + String(STM32_UART_RX_PIN) + ".");
            webSerialPrintln("      1. Connect ESP32 GPIO 16 (RX) -> STM32 PA2 (TX2)");
            webSerialPrintln("      2. Connect ESP32 GPIO 17 (TX) -> STM32 PA3 (RX2)");
            webSerialPrintln("      3. Ensure GND is connected between ESP32 and STM32!");
        } else if (totalPacketsDecoded == 0) {
            webSerialPrintln("  --> TROUBLESHOOTING: Bytes are arriving (" + String(totalBytesReceived) + " bytes), but no valid packets decoded.");
            webSerialPrintln("      Check for baud rate mismatch or incorrect packet format (SOF header 0xAA 0x55).");
        }
    }
}

// Command Transmission Helpers
bool sendStm32Move(int32_t distanceMm, uint16_t speedMmS) {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.move(distanceMm, speedMmS, pkt)) {
        // webSerialPrintln("ESP32 -> STM32: MOTOR DRIVE (MOVE) dist=" + String(distanceMm) + "mm, speed=" + String(speedMmS) + "mm/s");
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32Turn(int16_t angleDegX10, uint16_t speedDegS) {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.turn(angleDegX10, speedDegS, pkt)) {
        // webSerialPrintln("ESP32 -> STM32: MOTOR DRIVE (TURN) angle=" + String(angleDegX10/10.0f) + "deg, speed=" + String(speedDegS) + "deg/s");
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32Stop() {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.stop(pkt)) {
        // webSerialPrintln("ESP32 -> STM32: MOTOR DRIVE (STOP)");
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32SetRepeatSpeed(uint16_t speedPercent) {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.setRepeatSpeed(speedPercent, pkt)) {
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32MotorTrim(uint8_t lFwd, uint8_t rFwd, uint8_t lTurn, uint8_t rTurn) {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.setMotorTrim(lFwd, rFwd, lTurn, rTurn, pkt)) {
        // webSerialPrintln("ESP32 -> STM32: SET MOTOR TRIM (LF:" + String(lFwd) + "% RF:" + String(rFwd) + "% LT:" + String(lTurn) + "% RT:" + String(rTurn) + "%)");
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32PidTuning(float kpL, float kiL, float kdL, float kpR, float kiR, float kdR) {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.setPidTuning(kpL, kiL, kdL, kpR, kiR, kdR, pkt)) {
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32EncoderConfig(float wheelCircMm, uint16_t pprL, uint16_t pprR) {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.setEncoderConfig(wheelCircMm, pprL, pprR, pkt)) {
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32CalibrateImu() {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.calibrateImu(pkt)) {
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32TofConfig(float stopDistanceMm) {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.setTofConfig(stopDistanceMm, pkt)) {
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32TeachStart() {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.teachStart(pkt)) {
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32TeachStop() {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.teachStop(pkt)) {
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32SegmentStart(uint16_t segmentId, uint16_t srcRfidId, uint16_t dstRfidId) {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.segmentStart(segmentId, srcRfidId, dstRfidId, pkt)) {
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32StepMove(int32_t distanceMm, uint16_t speedMmS) {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.stepMove(distanceMm, speedMmS, pkt)) {
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32StepTurn(int16_t angleDegX10, uint16_t speedDegS) {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.stepTurn(angleDegX10, speedDegS, pkt)) {
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32SegmentComplete(uint16_t segmentId) {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.segmentComplete(segmentId, pkt)) {
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}
