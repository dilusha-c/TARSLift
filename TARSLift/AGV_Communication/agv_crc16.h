#ifndef AGV_CRC16_H
#define AGV_CRC16_H

#include <stdint.h>

uint16_t AGV_CRC16_Calculate(
    const uint8_t *data,
    uint16_t length
);

#endif
