#pragma once

#include <stdint.h>
#include <stddef.h>

#include "AGV_Commands.h"
#include "AGV_Responses.h"
#include "AGV_Config.h"
#include "AGV_Packet.h"
#include "AGV_Protocol.h"
#include "AGV_Communication.h"

/**
 * @brief STM32F103 Coprocessor UART Protocol Handler & Driver
 * 
 * Provides complete non-blocking byte stream decoding, command dispatch, 
 * automatic ACK response generation, and telemetry encoding for the STM32 side.
 */
class AGV_STM32_Driver {
public:
    AGV_STM32_Driver();

    void begin();

    /**
     * @brief Feed a single incoming raw byte from UART RX buffer into the decoder state machine.
     * @param byte Incoming byte
     * @return true if a complete valid frame was parsed and is ready for dispatch
     */
    bool feedByte(uint8_t byte);

    /**
     * @brief Get the last successfully decoded packet from ESP32-S3.
     */
    const AGVPacket& getLastPacket() const { return rxPacket; }

    /**
     * @brief Encode an ACK response packet for the specified command ID.
     */
    size_t encodeAck(uint8_t cmd, uint8_t result, uint8_t seq, uint8_t* outBuf, size_t maxLen);

    /**
     * @brief Encode a STATUS packet (0x82).
     */
    size_t encodeStatus(uint8_t state, uint8_t errorFlags, uint8_t seq, uint8_t* outBuf, size_t maxLen);

    /**
     * @brief Encode a VERSION packet (0x83).
     */
    size_t encodeVersion(uint8_t protoMaj, uint8_t protoMin, uint8_t fwMaj, uint8_t fwMin, uint8_t fwPatch, uint8_t seq, uint8_t* outBuf, size_t maxLen);

    /**
     * @brief Encode an ERROR report packet (0xE0).
     */
    size_t encodeError(uint16_t errorCode, uint8_t severity, uint8_t source, uint8_t seq, uint8_t* outBuf, size_t maxLen);

    /**
     * @brief Encode V2 ODOMETRY feedback packet (0x30).
     */
    size_t encodeOdometry(int32_t leftMm, int32_t rightMm, int16_t yawDegX10, uint8_t* outBuf, size_t maxLen);

    /**
     * @brief Encode V2 IMU_DATA feedback packet (0x31).
     */
    size_t encodeIMUData(int16_t ax, int16_t ay, int16_t az, int16_t gx, int16_t gy, int16_t gz, int16_t yawDegX10, uint8_t* outBuf, size_t maxLen);

    /**
     * @brief Encode V2 MOTOR_STATUS feedback packet (0x60).
     */
    size_t encodeMotorStatus(int16_t leftPwm, int16_t rightPwm, int8_t leftDir, int8_t rightDir, uint8_t* outBuf, size_t maxLen);

    /**
     * @brief Encode V3 MOVE_DONE completion event packet (0x91).
     */
    size_t encodeMoveDone(uint8_t origSeq, uint8_t result, uint8_t* outBuf, size_t maxLen);

    /**
     * @brief Encode V3 TURN_DONE completion event packet (0x92).
     */
    size_t encodeTurnDone(uint8_t origSeq, uint8_t result, uint8_t* outBuf, size_t maxLen);

private:
    enum DecoderState {
        WAIT_SOF1,
        WAIT_SOF2,
        WAIT_CMD,
        WAIT_SEQ,
        WAIT_LEN_L,
        WAIT_LEN_H,
        WAIT_PAYLOAD,
        WAIT_CRC_L,
        WAIT_CRC_H
    };

    DecoderState state;
    uint8_t rxBuffer[8 + AGVConfig::MAX_PAYLOAD_SIZE];
    size_t rxIndex;
    uint16_t expectedLen;
    AGVPacket rxPacket;
    AGV_Communication commHelper;
};
