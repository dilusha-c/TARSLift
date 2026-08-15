#ifndef AGV_PACKET_H
#define AGV_PACKET_H

#include <stdint.h>

#include "agv_config.h"

typedef struct
{
    uint8_t  cmd;
    uint8_t  seq;

    uint16_t length;

    uint8_t  payload[AGV_MAX_PAYLOAD];

    uint16_t crc;

} AGV_Packet_t;

#endif
