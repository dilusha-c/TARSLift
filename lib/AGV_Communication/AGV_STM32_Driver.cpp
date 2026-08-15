#include "AGV_STM32_Driver.h"
#include "AGV_Protocol.h"
#include <string.h>

AGV_STM32_Driver::AGV_STM32_Driver() {
    state = WAIT_SOF1;
    rxIndex = 0;
    expectedLen = 0;
}

void AGV_STM32_Driver::begin() {
    state = WAIT_SOF1;
    rxIndex = 0;
    expectedLen = 0;
}

bool AGV_STM32_Driver::feedByte(uint8_t b) {
    switch (state) {
        case WAIT_SOF1:
            if (b == AGVConfig::SOF1) {
                rxIndex = 0;
                rxBuffer[rxIndex++] = b;
                state = WAIT_SOF2;
            }
            break;

        case WAIT_SOF2:
            if (b == AGVConfig::SOF2) {
                rxBuffer[rxIndex++] = b;
                state = WAIT_CMD;
            } else {
                state = WAIT_SOF1;
            }
            break;

        case WAIT_CMD:
            rxBuffer[rxIndex++] = b;
            rxPacket.cmd = b;
            state = WAIT_SEQ;
            break;

        case WAIT_SEQ:
            rxBuffer[rxIndex++] = b;
            rxPacket.seq = b;
            state = WAIT_LEN_L;
            break;

        case WAIT_LEN_L:
            rxBuffer[rxIndex++] = b;
            expectedLen = b;
            state = WAIT_LEN_H;
            break;

        case WAIT_LEN_H:
            rxBuffer[rxIndex++] = b;
            expectedLen |= (static_cast<uint16_t>(b) << 8);
            rxPacket.length = expectedLen;

            if (expectedLen > AGVConfig::MAX_PAYLOAD_SIZE) {
                state = WAIT_SOF1;
                return false;
            }

            if (expectedLen == 0) {
                state = WAIT_CRC_L;
            } else {
                state = WAIT_PAYLOAD;
            }
            break;

        case WAIT_PAYLOAD:
            rxBuffer[rxIndex++] = b;
            rxPacket.payload[rxIndex - 6] = b;
            if ((rxIndex - 6) >= expectedLen) {
                state = WAIT_CRC_L;
            }
            break;

        case WAIT_CRC_L:
            rxBuffer[rxIndex++] = b;
            state = WAIT_CRC_H;
            break;

        case WAIT_CRC_H:
            rxBuffer[rxIndex++] = b;
            rxPacket.crc = static_cast<uint16_t>(rxBuffer[rxIndex - 2]) | (static_cast<uint16_t>(b) << 8);
            state = WAIT_SOF1;

            // Validate packet framing & CRC
            return AGV_Protocol::decode(rxBuffer, rxIndex, rxPacket);
    }

    return false;
}

size_t AGV_STM32_Driver::encodeAck(uint8_t cmd, uint8_t result, uint8_t seq, uint8_t* outBuf, size_t maxLen) {
    AGVPacket pkt;
    commHelper.makeAck(cmd, result, pkt);
    pkt.seq = seq;
    pkt.crc = AGV_Protocol::calculateCRC(pkt);
    return AGV_Protocol::encode(pkt, outBuf, maxLen);
}

size_t AGV_STM32_Driver::encodeStatus(uint8_t stateVal, uint8_t errorFlags, uint8_t seq, uint8_t* outBuf, size_t maxLen) {
    AGVPacket pkt;
    commHelper.makeStatus(stateVal, errorFlags, pkt);
    pkt.seq = seq;
    pkt.crc = AGV_Protocol::calculateCRC(pkt);
    return AGV_Protocol::encode(pkt, outBuf, maxLen);
}

size_t AGV_STM32_Driver::encodeVersion(uint8_t protoMaj, uint8_t protoMin, uint8_t fwMaj, uint8_t fwMin, uint8_t fwPatch, uint8_t seq, uint8_t* outBuf, size_t maxLen) {
    AGVPacket pkt;
    commHelper.makeVersion(protoMaj, protoMin, fwMaj, fwMin, fwPatch, pkt);
    pkt.seq = seq;
    pkt.crc = AGV_Protocol::calculateCRC(pkt);
    return AGV_Protocol::encode(pkt, outBuf, maxLen);
}

size_t AGV_STM32_Driver::encodeError(uint16_t errorCode, uint8_t severity, uint8_t source, uint8_t seq, uint8_t* outBuf, size_t maxLen) {
    AGVPacket pkt;
    commHelper.makeError(errorCode, severity, source, nullptr, 0, pkt);
    pkt.seq = seq;
    pkt.crc = AGV_Protocol::calculateCRC(pkt);
    return AGV_Protocol::encode(pkt, outBuf, maxLen);
}

size_t AGV_STM32_Driver::encodeOdometry(int32_t leftMm, int32_t rightMm, int16_t yawDegX10, uint8_t* outBuf, size_t maxLen) {
    AGVPacket pkt;
    commHelper.makeOdometry(leftMm, rightMm, yawDegX10, pkt);
    return AGV_Protocol::encode(pkt, outBuf, maxLen);
}

size_t AGV_STM32_Driver::encodeIMUData(int16_t ax, int16_t ay, int16_t az, int16_t gx, int16_t gy, int16_t gz, int16_t yawDegX10, uint8_t* outBuf, size_t maxLen) {
    AGVPacket pkt;
    commHelper.makeIMUData(ax, ay, az, gx, gy, gz, yawDegX10, pkt);
    return AGV_Protocol::encode(pkt, outBuf, maxLen);
}

size_t AGV_STM32_Driver::encodeMotorStatus(int16_t leftPwm, int16_t rightPwm, int8_t leftDir, int8_t rightDir, uint8_t* outBuf, size_t maxLen) {
    AGVPacket pkt;
    commHelper.makeMotorStatus(leftPwm, rightPwm, leftDir, rightDir, pkt);
    return AGV_Protocol::encode(pkt, outBuf, maxLen);
}

size_t AGV_STM32_Driver::encodeMoveDone(uint8_t origSeq, uint8_t result, uint8_t* outBuf, size_t maxLen) {
    AGVPacket pkt;
    commHelper.makeMoveDone(origSeq, result, pkt);
    return AGV_Protocol::encode(pkt, outBuf, maxLen);
}

size_t AGV_STM32_Driver::encodeTurnDone(uint8_t origSeq, uint8_t result, uint8_t* outBuf, size_t maxLen) {
    AGVPacket pkt;
    commHelper.makeTurnDone(origSeq, result, pkt);
    return AGV_Protocol::encode(pkt, outBuf, maxLen);
}
