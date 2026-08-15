#pragma once

#include <stdint.h>
#include <stddef.h>

#include "AGV_Packet.h"

class AGV_Protocol
{
public:

    static size_t encode(
        const AGVPacket& packet,
        uint8_t* buffer,
        size_t bufferSize
    );

    static bool decode(
        const uint8_t* buffer,
        size_t bufferSize,
        AGVPacket& packet
    );

    static uint16_t calculateCRC(
        const AGVPacket& packet
    );
};