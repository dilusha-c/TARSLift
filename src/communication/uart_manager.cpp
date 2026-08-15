#include "communication/uart_manager.h"
#include "config.h"
#include "system/system_manager.h"
#include "errors/error_logger.h"
#include "demo/demo_manager.h"
#include "AGV_Commands.h"
#include "AGV_Protocol.h"
#include "AGV_Responses.h"

// Hardware Serial instance for STM32 UART link (UART2 on pins 16/17)
#define STM32_UART_RX_PIN 16
#define STM32_UART_TX_PIN 17

static HardwareSerial stm32Serial(2);
static AGV_Communication agvComm;

static unsigned long lastHeartbeatTime = 0;
static unsigned long lastRxPacketTime = 0;
static bool watchdogTriggered = false;

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
    }
}

void stm32UartInit() {
    if (sysSettings.enable_stm32_uart) {
        stm32Serial.begin(AGVConfig::UART_BAUD_RATE, SERIAL_8N1, STM32_UART_RX_PIN, STM32_UART_TX_PIN);
        webSerialPrintln("STM32 UART: Initialized on Serial2 (115200 8N1)");
    } else {
        webSerialPrintln("STM32 UART: Disabled in configuration settings.");
    }
    rxState = WAIT_SOF1;
    rxIndex = 0;
    lastRxPacketTime = 0; // Wait for real incoming packets before marking connected
    watchdogTriggered = false;
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
            uint8_t ackCmd = pkt.payload[0];
            uint8_t result = pkt.payload[1];
            webSerialPrintln("STM32 UART: Received ACK for command 0x" + String(ackCmd, HEX) + " result=" + String(result));
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

        case AGVCommand::MOTOR_STATUS: {
            int16_t leftPwm, rightPwm;
            int8_t leftDir, rightDir;
            if (AGV_Communication::parseMotorStatus(pkt, leftPwm, rightPwm, leftDir, rightDir)) {
                updateTelemetryMotors(leftPwm, rightPwm);
            }
            break;
        }

        case AGVCommand::MOVE_DONE: {
            uint8_t origSeq = 0, res = 0;
            if (AGV_Communication::parseMoveDone(pkt, origSeq, res)) {
                webSerialPrintln("STM32 UART: Move finished for seq=" + String(origSeq) + " result=" + String(res));
            }
            break;
        }

        case AGVCommand::TURN_DONE: {
            uint8_t origSeq = 0, res = 0;
            if (AGV_Communication::parseTurnDone(pkt, origSeq, res)) {
                webSerialPrintln("STM32 UART: Turn finished for seq=" + String(origSeq) + " result=" + String(res));
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

    // 1. Read incoming bytes from STM32 serial bus
    while (stm32Serial.available() > 0) {
        uint8_t b = stm32Serial.read();

        switch (rxState) {
            case WAIT_SOF1:
                if (b == AGVConfig::SOF1) {
                    rxIndex = 0;
                    rxBuffer[rxIndex++] = b;
                    rxState = WAIT_SOF2;
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
                    processIncomingPacket(rxPacket);
                } else {
                    webSerialPrintln("STM32 UART: CRC check failed on command 0x" + String(rxPacket.cmd, HEX));
                }
                break;
        }
    }

    // 2. Transmit V1 Heartbeat every 500 ms
    unsigned long now = millis();
    if (now - lastHeartbeatTime >= AGVConfig::HEARTBEAT_INTERVAL_MS) {
        lastHeartbeatTime = now;
        AGVPacket hbPkt;
        agvComm.heartbeat(hbPkt);
        sendPacketOverUart(hbPkt);
    }
}

// Command Transmission Helpers
bool sendStm32Move(int32_t distanceMm, uint16_t speedMmS) {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.move(distanceMm, speedMmS, pkt)) {
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32Turn(int16_t angleDegX10, uint16_t speedDegS) {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.turn(angleDegX10, speedDegS, pkt)) {
        sendPacketOverUart(pkt);
        return true;
    }
    return false;
}

bool sendStm32Stop() {
    if (!sysSettings.enable_stm32_uart) return false;
    AGVPacket pkt;
    if (agvComm.stop(pkt)) {
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
