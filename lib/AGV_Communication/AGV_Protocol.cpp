#include "AGV_Protocol.h"

#include "AGV_Config.h"
#include "AGV_CRC16.h"

size_t AGV_Protocol::encode(
    const AGVPacket& packet,
    uint8_t* buffer,
    size_t bufferSize
)
{
    if (packet.length > AGVConfig::MAX_PAYLOAD_SIZE)
        return 0;

    const size_t requiredSize =
        2 +       // SOF
        1 +       // CMD
        1 +       // SEQ
        2 +       // LEN
        packet.length +
        2;        // CRC

    if (bufferSize < requiredSize)
        return 0;

    size_t index = 0;

    buffer[index++] = AGVConfig::SOF1;
    buffer[index++] = AGVConfig::SOF2;

    buffer[index++] = packet.cmd;
    buffer[index++] = packet.seq;

    buffer[index++] =
        static_cast<uint8_t>(packet.length & 0xFF);

    buffer[index++] =
        static_cast<uint8_t>((packet.length >> 8) & 0xFF);

    for (uint16_t i = 0; i < packet.length; i++)
    {
        buffer[index++] = packet.payload[i];
    }

    uint16_t crc = calculateCRC(packet);

    buffer[index++] =
        static_cast<uint8_t>(crc & 0xFF);

    buffer[index++] =
        static_cast<uint8_t>((crc >> 8) & 0xFF);

    return index;
}


bool AGV_Protocol::decode(
    const uint8_t* buffer,
    size_t bufferSize,
    AGVPacket& packet
)
{
    if (bufferSize < 8)
        return false;

    if (buffer[0] != AGVConfig::SOF1 ||
        buffer[1] != AGVConfig::SOF2)
    {
        return false;
    }

    packet.cmd = buffer[2];
    packet.seq = buffer[3];

    packet.length =
        static_cast<uint16_t>(buffer[4]) |
        (static_cast<uint16_t>(buffer[5]) << 8);

    if (packet.length > AGVConfig::MAX_PAYLOAD_SIZE)
        return false;

    const size_t expectedSize =
        2 + 1 + 1 + 2 +
        packet.length +
        2;

    if (bufferSize < expectedSize)
        return false;

    for (uint16_t i = 0; i < packet.length; i++)
    {
        packet.payload[i] = buffer[6 + i];
    }

    const size_t crcIndex = 6 + packet.length;

    packet.crc =
        static_cast<uint16_t>(buffer[crcIndex]) |
        (static_cast<uint16_t>(buffer[crcIndex + 1]) << 8);

    uint16_t calculated =
        calculateCRC(packet);

    return calculated == packet.crc;
}


uint16_t AGV_Protocol::calculateCRC(
    const AGVPacket& packet
)
{
    uint8_t data[
        4 + AGVConfig::MAX_PAYLOAD_SIZE
    ];

    size_t index = 0;

    data[index++] = packet.cmd;
    data[index++] = packet.seq;

    data[index++] =
        static_cast<uint8_t>(packet.length & 0xFF);

    data[index++] =
        static_cast<uint8_t>((packet.length >> 8) & 0xFF);

    for (uint16_t i = 0; i < packet.length; i++)
    {
        data[index++] = packet.payload[i];
    }

    return AGV_CRC16::calculate(
        data,
        static_cast<uint16_t>(index)
    );
}