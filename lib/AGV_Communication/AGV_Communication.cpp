#include "AGV_Communication.h"

#include <string.h>

#include "AGV_Commands.h"
#include "AGV_Protocol.h"


AGV_Communication::AGV_Communication()
{
    sequenceNumber = 0;
}


void AGV_Communication::begin()
{
    sequenceNumber = 0;
}


uint8_t AGV_Communication::nextSequence()
{
    return sequenceNumber++;
}


bool AGV_Communication::makeCommand(
    uint8_t command,
    const uint8_t* payload,
    uint16_t length,
    AGVPacket& packet
)
{
    if (length > 64)
        return false;

    packet.cmd = command;
    packet.seq = nextSequence();
    packet.length = length;

    if (payload != nullptr && length > 0)
    {
        memcpy(
            packet.payload,
            payload,
            length
        );
    }

    packet.crc =
        AGV_Protocol::calculateCRC(packet);

    return true;
}


bool AGV_Communication::makeEmptyCommand(
    uint8_t command,
    AGVPacket& packet
)
{
    return makeCommand(
        command,
        nullptr,
        0,
        packet
    );
}


bool AGV_Communication::createPacket(
    uint8_t command,
    const uint8_t* payload,
    uint16_t length,
    AGVPacket& packet
)
{
    return makeCommand(
        command,
        payload,
        length,
        packet
    );
}


size_t AGV_Communication::encodePacket(
    const AGVPacket& packet,
    uint8_t* buffer,
    size_t bufferSize
)
{
    return AGV_Protocol::encode(
        packet,
        buffer,
        bufferSize
    );
}


bool AGV_Communication::decodePacket(
    const uint8_t* buffer,
    size_t bufferSize,
    AGVPacket& packet
)
{
    return AGV_Protocol::decode(
        buffer,
        bufferSize,
        packet
    );
}


// =====================================================
// V1
// =====================================================

bool AGV_Communication::ping(
    AGVPacket& packet
)
{
    return makeEmptyCommand(
        AGVCommand::PING,
        packet
    );
}


bool AGV_Communication::getStatus(
    AGVPacket& packet
)
{
    return makeEmptyCommand(
        AGVCommand::GET_STATUS,
        packet
    );
}


bool AGV_Communication::getVersion(
    AGVPacket& packet
)
{
    return makeEmptyCommand(
        AGVCommand::GET_VERSION,
        packet
    );
}


bool AGV_Communication::heartbeat(
    AGVPacket& packet
)
{
    return makeEmptyCommand(
        AGVCommand::HEARTBEAT,
        packet
    );
}


// =====================================================
// V3
// =====================================================

bool AGV_Communication::move(
    int32_t distanceMm,
    uint16_t speedMmS,
    AGVPacket& packet
)
{
    uint8_t payload[6];

    payload[0] =
        static_cast<uint8_t>(distanceMm & 0xFF);

    payload[1] =
        static_cast<uint8_t>((distanceMm >> 8) & 0xFF);

    payload[2] =
        static_cast<uint8_t>((distanceMm >> 16) & 0xFF);

    payload[3] =
        static_cast<uint8_t>((distanceMm >> 24) & 0xFF);

    payload[4] =
        static_cast<uint8_t>(speedMmS & 0xFF);

    payload[5] =
        static_cast<uint8_t>((speedMmS >> 8) & 0xFF);

    return makeCommand(
        AGVCommand::MOVE,
        payload,
        6,
        packet
    );
}


bool AGV_Communication::turn(
    int16_t angleDegX10,
    uint16_t speedDegS,
    AGVPacket& packet
)
{
    uint8_t payload[4];

    payload[0] =
        static_cast<uint8_t>(angleDegX10 & 0xFF);

    payload[1] =
        static_cast<uint8_t>(
            (angleDegX10 >> 8) & 0xFF
        );

    payload[2] =
        static_cast<uint8_t>(speedDegS & 0xFF);

    payload[3] =
        static_cast<uint8_t>(
            (speedDegS >> 8) & 0xFF
        );

    return makeCommand(
        AGVCommand::TURN,
        payload,
        4,
        packet
    );
}


bool AGV_Communication::stop(
    AGVPacket& packet
)
{
    return makeEmptyCommand(
        AGVCommand::STOP,
        packet
    );
}


bool AGV_Communication::setRepeatSpeed(
    uint16_t speedPercent,
    AGVPacket& packet
)
{
    uint8_t payload[2];

    payload[0] =
        static_cast<uint8_t>(speedPercent & 0xFF);

    payload[1] =
        static_cast<uint8_t>(
            (speedPercent >> 8) & 0xFF
        );

    return makeCommand(
        AGVCommand::SET_REPEAT_SPEED,
        payload,
        2,
        packet
    );
}


// =====================================================
// V4
// =====================================================

bool AGV_Communication::teachStart(
    AGVPacket& packet
)
{
    return makeEmptyCommand(
        AGVCommand::TEACH_START,
        packet
    );
}


bool AGV_Communication::teachStop(
    AGVPacket& packet
)
{
    return makeEmptyCommand(
        AGVCommand::TEACH_STOP,
        packet
    );
}


// =====================================================
// V5
// =====================================================

bool AGV_Communication::segmentStart(
    uint16_t segmentId,
    uint16_t srcRfidId,
    uint16_t dstRfidId,
    AGVPacket& packet
)
{
    uint8_t payload[6];

    payload[0] =
        static_cast<uint8_t>(segmentId & 0xFF);

    payload[1] =
        static_cast<uint8_t>((segmentId >> 8) & 0xFF);

    payload[2] =
        static_cast<uint8_t>(srcRfidId & 0xFF);

    payload[3] =
        static_cast<uint8_t>((srcRfidId >> 8) & 0xFF);

    payload[4] =
        static_cast<uint8_t>(dstRfidId & 0xFF);

    payload[5] =
        static_cast<uint8_t>((dstRfidId >> 8) & 0xFF);

    return makeCommand(
        AGVCommand::SEGMENT_START,
        payload,
        6,
        packet
    );
}


bool AGV_Communication::stepMove(
    int32_t distanceMm,
    uint16_t speedMmS,
    AGVPacket& packet
)
{
    uint8_t payload[6];

    payload[0] =
        static_cast<uint8_t>(distanceMm & 0xFF);

    payload[1] =
        static_cast<uint8_t>((distanceMm >> 8) & 0xFF);

    payload[2] =
        static_cast<uint8_t>((distanceMm >> 16) & 0xFF);

    payload[3] =
        static_cast<uint8_t>((distanceMm >> 24) & 0xFF);

    payload[4] =
        static_cast<uint8_t>(speedMmS & 0xFF);

    payload[5] =
        static_cast<uint8_t>((speedMmS >> 8) & 0xFF);

    return makeCommand(
        AGVCommand::STEP_MOVE,
        payload,
        6,
        packet
    );
}


bool AGV_Communication::stepTurn(
    int16_t angleDegX10,
    uint16_t speedDegS,
    AGVPacket& packet
)
{
    uint8_t payload[4];

    payload[0] =
        static_cast<uint8_t>(angleDegX10 & 0xFF);

    payload[1] =
        static_cast<uint8_t>(
            (angleDegX10 >> 8) & 0xFF
        );

    payload[2] =
        static_cast<uint8_t>(speedDegS & 0xFF);

    payload[3] =
        static_cast<uint8_t>(
            (speedDegS >> 8) & 0xFF
        );

    return makeCommand(
        AGVCommand::STEP_TURN,
        payload,
        4,
        packet
    );
}


bool AGV_Communication::segmentComplete(
    uint16_t segmentId,
    AGVPacket& packet
)
{
    uint8_t payload[2];

    payload[0] =
        static_cast<uint8_t>(segmentId & 0xFF);

    payload[1] =
        static_cast<uint8_t>(
            (segmentId >> 8) & 0xFF
        );

    return makeCommand(
        AGVCommand::SEGMENT_COMPLETE,
        payload,
        2,
        packet
    );
}

// =====================================================
// V1 Response Builders
// =====================================================

bool AGV_Communication::makeAck(uint8_t ackCmd, uint8_t result, AGVPacket& packet) {
    uint8_t payload[2] = { ackCmd, result };
    return makeCommand(AGVCommand::ACK, payload, 2, packet);
}

bool AGV_Communication::makeStatus(uint8_t state, uint8_t errorFlags, AGVPacket& packet) {
    uint8_t payload[2] = { state, errorFlags };
    return makeCommand(AGVCommand::STATUS, payload, 2, packet);
}

bool AGV_Communication::makeVersion(uint8_t protoMaj, uint8_t protoMin, uint8_t fwMaj, uint8_t fwMin, uint8_t fwPatch, AGVPacket& packet) {
    uint8_t payload[5] = { protoMaj, protoMin, fwMaj, fwMin, fwPatch };
    return makeCommand(AGVCommand::VERSION, payload, 5, packet);
}

bool AGV_Communication::makeError(uint16_t errorCode, uint8_t severity, uint8_t source, const uint8_t* detail, uint8_t detailLen, AGVPacket& packet) {
    uint8_t payload[64];
    payload[0] = static_cast<uint8_t>(errorCode & 0xFF);
    payload[1] = static_cast<uint8_t>((errorCode >> 8) & 0xFF);
    payload[2] = severity;
    payload[3] = source;
    uint16_t len = 4;
    if (detail != nullptr && detailLen > 0) {
        if (detailLen > 60) detailLen = 60;
        memcpy(&payload[4], detail, detailLen);
        len += detailLen;
    }
    return makeCommand(AGVCommand::ERROR, payload, len, packet);
}

// =====================================================
// V2 Sensor / Motion Feedback Builders (STM32 -> ESP32)
// =====================================================

bool AGV_Communication::makeOdometry(int32_t leftDistMm, int32_t rightDistMm, int16_t yawDegX10, AGVPacket& packet) {
    uint8_t payload[10];
    payload[0] = static_cast<uint8_t>(leftDistMm & 0xFF);
    payload[1] = static_cast<uint8_t>((leftDistMm >> 8) & 0xFF);
    payload[2] = static_cast<uint8_t>((leftDistMm >> 16) & 0xFF);
    payload[3] = static_cast<uint8_t>((leftDistMm >> 24) & 0xFF);

    payload[4] = static_cast<uint8_t>(rightDistMm & 0xFF);
    payload[5] = static_cast<uint8_t>((rightDistMm >> 8) & 0xFF);
    payload[6] = static_cast<uint8_t>((rightDistMm >> 16) & 0xFF);
    payload[7] = static_cast<uint8_t>((rightDistMm >> 24) & 0xFF);

    payload[8] = static_cast<uint8_t>(yawDegX10 & 0xFF);
    payload[9] = static_cast<uint8_t>((yawDegX10 >> 8) & 0xFF);

    return makeCommand(AGVCommand::ODOMETRY, payload, 10, packet);
}

bool AGV_Communication::makeIMUData(int16_t ax, int16_t ay, int16_t az, int16_t gx, int16_t gy, int16_t gz, int16_t yawDegX10, AGVPacket& packet) {
    uint8_t payload[14];
    payload[0] = static_cast<uint8_t>(ax & 0xFF); payload[1] = static_cast<uint8_t>((ax >> 8) & 0xFF);
    payload[2] = static_cast<uint8_t>(ay & 0xFF); payload[3] = static_cast<uint8_t>((ay >> 8) & 0xFF);
    payload[4] = static_cast<uint8_t>(az & 0xFF); payload[5] = static_cast<uint8_t>((az >> 8) & 0xFF);
    payload[6] = static_cast<uint8_t>(gx & 0xFF); payload[7] = static_cast<uint8_t>((gx >> 8) & 0xFF);
    payload[8] = static_cast<uint8_t>(gy & 0xFF); payload[9] = static_cast<uint8_t>((gy >> 8) & 0xFF);
    payload[10] = static_cast<uint8_t>(gz & 0xFF); payload[11] = static_cast<uint8_t>((gz >> 8) & 0xFF);
    payload[12] = static_cast<uint8_t>(yawDegX10 & 0xFF); payload[13] = static_cast<uint8_t>((yawDegX10 >> 8) & 0xFF);

    return makeCommand(AGVCommand::IMU_DATA, payload, 14, packet);
}

bool AGV_Communication::makeMotorStatus(int16_t leftPwm, int16_t rightPwm, int8_t leftDir, int8_t rightDir, AGVPacket& packet) {
    uint8_t payload[6];
    payload[0] = static_cast<uint8_t>(leftPwm & 0xFF);
    payload[1] = static_cast<uint8_t>((leftPwm >> 8) & 0xFF);
    payload[2] = static_cast<uint8_t>(rightPwm & 0xFF);
    payload[3] = static_cast<uint8_t>((rightPwm >> 8) & 0xFF);
    payload[4] = static_cast<uint8_t>(leftDir);
    payload[5] = static_cast<uint8_t>(rightDir);

    return makeCommand(AGVCommand::MOTOR_STATUS, payload, 6, packet);
}

// =====================================================
// V3 Motion Done Event Builders
// =====================================================

bool AGV_Communication::makeMoveDone(uint8_t origSeq, uint8_t result, AGVPacket& packet) {
    uint8_t payload[2] = { origSeq, result };
    return makeCommand(AGVCommand::MOVE_DONE, payload, 2, packet);
}

bool AGV_Communication::makeTurnDone(uint8_t origSeq, uint8_t result, AGVPacket& packet) {
    uint8_t payload[2] = { origSeq, result };
    return makeCommand(AGVCommand::TURN_DONE, payload, 2, packet);
}

// =====================================================
// Payload Parsers (Static Helpers)
// =====================================================

bool AGV_Communication::parseOdometry(const AGVPacket& packet, int32_t& leftDistMm, int32_t& rightDistMm, int16_t& yawDegX10) {
    if (packet.cmd != AGVCommand::ODOMETRY || packet.length < 10) return false;
    leftDistMm = static_cast<int32_t>(packet.payload[0] | (packet.payload[1] << 8) | (packet.payload[2] << 16) | (packet.payload[3] << 24));
    rightDistMm = static_cast<int32_t>(packet.payload[4] | (packet.payload[5] << 8) | (packet.payload[6] << 16) | (packet.payload[7] << 24));
    yawDegX10 = static_cast<int16_t>(packet.payload[8] | (packet.payload[9] << 8));
    return true;
}

bool AGV_Communication::parseIMUData(const AGVPacket& packet, int16_t& ax, int16_t& ay, int16_t& az, int16_t& gx, int16_t& gy, int16_t& gz, int16_t& yawDegX10) {
    if (packet.cmd != AGVCommand::IMU_DATA || packet.length < 14) return false;
    ax = static_cast<int16_t>(packet.payload[0] | (packet.payload[1] << 8));
    ay = static_cast<int16_t>(packet.payload[2] | (packet.payload[3] << 8));
    az = static_cast<int16_t>(packet.payload[4] | (packet.payload[5] << 8));
    gx = static_cast<int16_t>(packet.payload[6] | (packet.payload[7] << 8));
    gy = static_cast<int16_t>(packet.payload[8] | (packet.payload[9] << 8));
    gz = static_cast<int16_t>(packet.payload[10] | (packet.payload[11] << 8));
    yawDegX10 = static_cast<int16_t>(packet.payload[12] | (packet.payload[13] << 8));
    return true;
}

bool AGV_Communication::parseMotorStatus(const AGVPacket& packet, int16_t& leftPwm, int16_t& rightPwm, int8_t& leftDir, int8_t& rightDir) {
    if (packet.cmd != AGVCommand::MOTOR_STATUS || packet.length < 6) return false;
    leftPwm = static_cast<int16_t>(packet.payload[0] | (packet.payload[1] << 8));
    rightPwm = static_cast<int16_t>(packet.payload[2] | (packet.payload[3] << 8));
    leftDir = static_cast<int8_t>(packet.payload[4]);
    rightDir = static_cast<int8_t>(packet.payload[5]);
    return true;
}

bool AGV_Communication::parseMove(const AGVPacket& packet, int32_t& distanceMm, uint16_t& speedMmS) {
    if (packet.cmd != AGVCommand::MOVE && packet.cmd != AGVCommand::STEP_MOVE) return false;
    if (packet.length < 6) return false;
    distanceMm = static_cast<int32_t>(packet.payload[0] | (packet.payload[1] << 8) | (packet.payload[2] << 16) | (packet.payload[3] << 24));
    speedMmS = static_cast<uint16_t>(packet.payload[4] | (packet.payload[5] << 8));
    return true;
}

bool AGV_Communication::parseTurn(const AGVPacket& packet, int16_t& angleDegX10, uint16_t& speedDegS) {
    if (packet.cmd != AGVCommand::TURN && packet.cmd != AGVCommand::STEP_TURN) return false;
    if (packet.length < 4) return false;
    angleDegX10 = static_cast<int16_t>(packet.payload[0] | (packet.payload[1] << 8));
    speedDegS = static_cast<uint16_t>(packet.payload[2] | (packet.payload[3] << 8));
    return true;
}

bool AGV_Communication::parseSetRepeatSpeed(const AGVPacket& packet, uint16_t& speedPercent) {
    if (packet.cmd != AGVCommand::SET_REPEAT_SPEED || packet.length < 2) return false;
    speedPercent = static_cast<uint16_t>(packet.payload[0] | (packet.payload[1] << 8));
    return true;
}

bool AGV_Communication::parseMoveDone(const AGVPacket& packet, uint8_t& origSeq, uint8_t& result) {
    if (packet.cmd != AGVCommand::MOVE_DONE || packet.length < 2) return false;
    origSeq = packet.payload[0];
    result = packet.payload[1];
    return true;
}

bool AGV_Communication::parseTurnDone(const AGVPacket& packet, uint8_t& origSeq, uint8_t& result) {
    if (packet.cmd != AGVCommand::TURN_DONE || packet.length < 2) return false;
    origSeq = packet.payload[0];
    result = packet.payload[1];
    return true;
}

bool AGV_Communication::parseSegmentStart(const AGVPacket& packet, uint16_t& segmentId, uint16_t& srcRfidId, uint16_t& dstRfidId) {
    if (packet.cmd != AGVCommand::SEGMENT_START || packet.length < 6) return false;
    segmentId = static_cast<uint16_t>(packet.payload[0] | (packet.payload[1] << 8));
    srcRfidId = static_cast<uint16_t>(packet.payload[2] | (packet.payload[3] << 8));
    dstRfidId = static_cast<uint16_t>(packet.payload[4] | (packet.payload[5] << 8));
    return true;
}

bool AGV_Communication::parseStepMove(const AGVPacket& packet, int32_t& distanceMm, uint16_t& speedMmS) {
    return parseMove(packet, distanceMm, speedMmS);
}

bool AGV_Communication::parseStepTurn(const AGVPacket& packet, int16_t& angleDegX10, uint16_t& speedDegS) {
    return parseTurn(packet, angleDegX10, speedDegS);
}

bool AGV_Communication::parseSegmentComplete(const AGVPacket& packet, uint16_t& segmentId) {
    if (packet.cmd != AGVCommand::SEGMENT_COMPLETE || packet.length < 2) return false;
    segmentId = static_cast<uint16_t>(packet.payload[0] | (packet.payload[1] << 8));
    return true;
}