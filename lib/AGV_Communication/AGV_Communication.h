#pragma once

#include <stdint.h>
#include <stddef.h>

#include "AGV_Packet.h"

class AGV_Communication
{
public:

    AGV_Communication();

    void begin();

    uint8_t nextSequence();

    // Generic packet

    bool createPacket(
        uint8_t command,
        const uint8_t* payload,
        uint16_t length,
        AGVPacket& packet
    );

    size_t encodePacket(
        const AGVPacket& packet,
        uint8_t* buffer,
        size_t bufferSize
    );

    bool decodePacket(
        const uint8_t* buffer,
        size_t bufferSize,
        AGVPacket& packet
    );

    // V1 Commands & Responses
    bool ping(AGVPacket& packet);
    bool getStatus(AGVPacket& packet);
    bool getVersion(AGVPacket& packet);
    bool heartbeat(AGVPacket& packet);

    bool makeAck(uint8_t ackCmd, uint8_t result, AGVPacket& packet);
    bool makeStatus(uint8_t state, uint8_t errorFlags, AGVPacket& packet);
    bool makeVersion(uint8_t protoMaj, uint8_t protoMin, uint8_t fwMaj, uint8_t fwMin, uint8_t fwPatch, AGVPacket& packet);
    bool makeError(uint16_t errorCode, uint8_t severity, uint8_t source, const uint8_t* detail, uint8_t detailLen, AGVPacket& packet);

    // V2 Sensor / Motion Feedback (STM32 -> ESP32)
    bool makeOdometry(int32_t leftDistMm, int32_t rightDistMm, int16_t yawDegX10, AGVPacket& packet);
    bool makeIMUData(int16_t ax, int16_t ay, int16_t az, int16_t gx, int16_t gy, int16_t gz, int16_t yawDegX10, AGVPacket& packet);
    bool makeMotorStatus(int16_t leftPwm, int16_t rightPwm, int8_t leftDir, int8_t rightDir, AGVPacket& packet);

    // V2 Telemetry Payload Parsers
    static bool parseOdometry(const AGVPacket& packet, int32_t& leftDistMm, int32_t& rightDistMm, int16_t& yawDegX10);
    static bool parseIMUData(const AGVPacket& packet, int16_t& ax, int16_t& ay, int16_t& az, int16_t& gx, int16_t& gy, int16_t& gz, int16_t& yawDegX10);
    static bool parseMotorStatus(const AGVPacket& packet, int16_t& leftPwm, int16_t& rightPwm, int8_t& leftDir, int8_t& rightDir);

    // V3 Motion Control & Completion (ESP32 <-> STM32)
    bool move(int32_t distanceMm, uint16_t speedMmS, AGVPacket& packet);
    bool turn(int16_t angleDegX10, uint16_t speedDegS, AGVPacket& packet);
    bool stop(AGVPacket& packet);
    bool setRepeatSpeed(uint16_t speedPercent, AGVPacket& packet);

    bool makeMoveDone(uint8_t origSeq, uint8_t result, AGVPacket& packet);
    bool makeTurnDone(uint8_t origSeq, uint8_t result, AGVPacket& packet);

    // V3 Payload Parsers
    static bool parseMove(const AGVPacket& packet, int32_t& distanceMm, uint16_t& speedMmS);
    static bool parseTurn(const AGVPacket& packet, int16_t& angleDegX10, uint16_t& speedDegS);
    static bool parseSetRepeatSpeed(const AGVPacket& packet, uint16_t& speedPercent);
    static bool parseMoveDone(const AGVPacket& packet, uint8_t& origSeq, uint8_t& result);
    static bool parseTurnDone(const AGVPacket& packet, uint8_t& origSeq, uint8_t& result);

    // V4 Teach & Repeat (ESP32 -> STM32)
    bool teachStart(AGVPacket& packet);
    bool teachStop(AGVPacket& packet);

    // V5 RFID-to-RFID Segment Protocol (ESP32 -> STM32)
    bool segmentStart(uint16_t segmentId, uint16_t srcRfidId, uint16_t dstRfidId, AGVPacket& packet);
    bool stepMove(int32_t distanceMm, uint16_t speedMmS, AGVPacket& packet);
    bool stepTurn(int16_t angleDegX10, uint16_t speedDegS, AGVPacket& packet);
    bool segmentComplete(uint16_t segmentId, AGVPacket& packet);

    // V5 Payload Parsers
    static bool parseSegmentStart(const AGVPacket& packet, uint16_t& segmentId, uint16_t& srcRfidId, uint16_t& dstRfidId);
    static bool parseStepMove(const AGVPacket& packet, int32_t& distanceMm, uint16_t& speedMmS);
    static bool parseStepTurn(const AGVPacket& packet, int16_t& angleDegX10, uint16_t& speedDegS);
    static bool parseSegmentComplete(const AGVPacket& packet, uint16_t& segmentId);

private:

    uint8_t sequenceNumber;

    bool makeCommand(
        uint8_t command,
        const uint8_t* payload,
        uint16_t length,
        AGVPacket& packet
    );

    bool makeEmptyCommand(
        uint8_t command,
        AGVPacket& packet
    );
};