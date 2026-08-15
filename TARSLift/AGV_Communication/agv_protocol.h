#ifndef AGV_PROTOCOL_H
#define AGV_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#include "agv_packet.h"

typedef enum
{
    AGV_PROTOCOL_OK = 0,

    AGV_PROTOCOL_ERROR_NULL,
    AGV_PROTOCOL_ERROR_PAYLOAD_TOO_LARGE,
    AGV_PROTOCOL_ERROR_BUFFER_TOO_SMALL,
    AGV_PROTOCOL_ERROR_FRAME_TOO_SHORT,
    AGV_PROTOCOL_ERROR_SOF,
    AGV_PROTOCOL_ERROR_LENGTH,
    AGV_PROTOCOL_ERROR_CRC

} AGV_ProtocolResult_t;


/* Packet creation */

AGV_ProtocolResult_t AGV_Protocol_CreatePacket(
    AGV_Packet_t *packet,
    uint8_t cmd,
    uint8_t seq,
    const uint8_t *payload,
    uint16_t payloadLength
);


/* Encode packet into UART byte buffer */

AGV_ProtocolResult_t AGV_Protocol_Encode(
    const AGV_Packet_t *packet,
    uint8_t *buffer,
    uint16_t bufferSize,
    uint16_t *encodedLength
);


/* Decode a complete UART frame */

AGV_ProtocolResult_t AGV_Protocol_Decode(
    const uint8_t *buffer,
    uint16_t bufferLength,
    AGV_Packet_t *packet
);


/* Calculate packet CRC */

uint16_t AGV_Protocol_CalculateCRC(
    const AGV_Packet_t *packet
);


/* Check whether a command ID is known */

bool AGV_Protocol_IsKnownCommand(
    uint8_t cmd
);


/* Get expected payload length.
 *
 * Returns:
 *   >= 0 : fixed payload length
 *   -1   : variable / not fixed
 *   -2   : unknown command
 */
int16_t AGV_Protocol_GetExpectedLength(
    uint8_t cmd
);

#endif
