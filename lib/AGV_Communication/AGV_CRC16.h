#pragma once

#include <stdint.h>

class AGV_CRC16
{
public:

    static uint16_t calculate(
        const uint8_t* data,
        uint16_t length
    );
};