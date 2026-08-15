#pragma once

#include <stdint.h>

#include "AGV_Config.h"

struct AGVPacket
{
    uint8_t cmd;
    uint8_t seq;

    uint16_t length;

    uint8_t payload[AGVConfig::MAX_PAYLOAD_SIZE];

    uint16_t crc;
};