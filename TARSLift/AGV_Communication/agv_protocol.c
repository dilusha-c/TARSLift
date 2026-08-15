#include "agv_protocol.h"

#include <string.h>

#include "agv_commands.h"
#include "agv_config.h"
#include "agv_crc16.h"


AGV_ProtocolResult_t AGV_Protocol_CreatePacket(
    AGV_Packet_t *packet,
    uint8_t cmd,
    uint8_t seq,
    const uint8_t *payload,
    uint16_t payloadLength
)
{
    if (packet == NULL)
    {
        return AGV_PROTOCOL_ERROR_NULL;
    }

    if (payloadLength > AGV_MAX_PAYLOAD)
    {
        return AGV_PROTOCOL_ERROR_PAYLOAD_TOO_LARGE;
    }

    packet->cmd = cmd;
    packet->seq = seq;
    packet->length = payloadLength;

    if ((payload != NULL) && (payloadLength > 0U))
    {
        memcpy(
            packet->payload,
            payload,
            payloadLength
        );
    }

    packet->crc =
        AGV_Protocol_CalculateCRC(packet);

    return AGV_PROTOCOL_OK;
}


uint16_t AGV_Protocol_CalculateCRC(
    const AGV_Packet_t *packet
)
{
    uint8_t data[4U + AGV_MAX_PAYLOAD];

    uint16_t index = 0U;

    data[index++] = packet->cmd;
    data[index++] = packet->seq;

    data[index++] =
        (uint8_t)(packet->length & 0xFFU);

    data[index++] =
        (uint8_t)((packet->length >> 8U) & 0xFFU);

    for (uint16_t i = 0U; i < packet->length; i++)
    {
        data[index++] = packet->payload[i];
    }

    return AGV_CRC16_Calculate(
        data,
        index
    );
}


AGV_ProtocolResult_t AGV_Protocol_Encode(
    const AGV_Packet_t *packet,
    uint8_t *buffer,
    uint16_t bufferSize,
    uint16_t *encodedLength
)
{
    if ((packet == NULL) ||
        (buffer == NULL) ||
        (encodedLength == NULL))
    {
        return AGV_PROTOCOL_ERROR_NULL;
    }

    if (packet->length > AGV_MAX_PAYLOAD)
    {
        return AGV_PROTOCOL_ERROR_PAYLOAD_TOO_LARGE;
    }

    uint16_t required =
        2U +
        1U +
        1U +
        2U +
        packet->length +
        2U;

    if (bufferSize < required)
    {
        return AGV_PROTOCOL_ERROR_BUFFER_TOO_SMALL;
    }

    uint16_t index = 0U;

    /* SOF */

    buffer[index++] = AGV_SOF1;
    buffer[index++] = AGV_SOF2;

    /* Header */

    buffer[index++] = packet->cmd;
    buffer[index++] = packet->seq;

    buffer[index++] =
        (uint8_t)(packet->length & 0xFFU);

    buffer[index++] =
        (uint8_t)((packet->length >> 8U) & 0xFFU);

    /* Payload */

    for (uint16_t i = 0U; i < packet->length; i++)
    {
        buffer[index++] = packet->payload[i];
    }

    /* CRC */

    uint16_t crc =
        AGV_Protocol_CalculateCRC(packet);

    buffer[index++] =
        (uint8_t)(crc & 0xFFU);

    buffer[index++] =
        (uint8_t)((crc >> 8U) & 0xFFU);

    *encodedLength = index;

    return AGV_PROTOCOL_OK;
}


AGV_ProtocolResult_t AGV_Protocol_Decode(
    const uint8_t *buffer,
    uint16_t bufferLength,
    AGV_Packet_t *packet
)
{
    if ((buffer == NULL) ||
        (packet == NULL))
    {
        return AGV_PROTOCOL_ERROR_NULL;
    }

    /* Minimum frame:
     *
     * AA 55 CMD SEQ LEN_L LEN_H CRC_L CRC_H
     */

    if (bufferLength < 8U)
    {
        return AGV_PROTOCOL_ERROR_FRAME_TOO_SHORT;
    }

    /* SOF */

    if ((buffer[0] != AGV_SOF1) ||
        (buffer[1] != AGV_SOF2))
    {
        return AGV_PROTOCOL_ERROR_SOF;
    }

    packet->cmd = buffer[2];
    packet->seq = buffer[3];

    packet->length =
        (uint16_t)buffer[4] |
        ((uint16_t)buffer[5] << 8U);

    if (packet->length > AGV_MAX_PAYLOAD)
    {
        return AGV_PROTOCOL_ERROR_LENGTH;
    }

    uint16_t expectedLength =
        2U +
        1U +
        1U +
        2U +
        packet->length +
        2U;

    if (bufferLength < expectedLength)
    {
        return AGV_PROTOCOL_ERROR_FRAME_TOO_SHORT;
    }

    /* Payload */

    for (uint16_t i = 0U; i < packet->length; i++)
    {
        packet->payload[i] =
            buffer[6U + i];
    }

    /* Received CRC */

    uint16_t crcIndex =
        (uint16_t)(6U + packet->length);

    packet->crc =
        (uint16_t)buffer[crcIndex] |
        ((uint16_t)buffer[crcIndex + 1U] << 8U);

    /* Calculate CRC */

    uint16_t calculated =
        AGV_Protocol_CalculateCRC(packet);

    if (calculated != packet->crc)
    {
        return AGV_PROTOCOL_ERROR_CRC;
    }

    return AGV_PROTOCOL_OK;
}


bool AGV_Protocol_IsKnownCommand(
    uint8_t cmd
)
{
    switch (cmd)
    {
        case AGV_CMD_PING:
        case AGV_CMD_GET_STATUS:
        case AGV_CMD_GET_VERSION:
        case AGV_CMD_HEARTBEAT:

        case AGV_CMD_MOVE:
        case AGV_CMD_TURN:
        case AGV_CMD_STOP:
        case AGV_CMD_SET_REPEAT_SPEED:

        case AGV_CMD_TEACH_START:
        case AGV_CMD_TEACH_STOP:

        case AGV_CMD_SEGMENT_START:
        case AGV_CMD_STEP_MOVE:
        case AGV_CMD_STEP_TURN:
        case AGV_CMD_SEGMENT_COMPLETE:

        case AGV_CMD_ODOMETRY:
        case AGV_CMD_IMU_DATA:
        case AGV_CMD_MOTOR_STATUS:

        case AGV_CMD_ACK:
        case AGV_CMD_STATUS:
        case AGV_CMD_VERSION:

        case AGV_CMD_MOVE_DONE:
        case AGV_CMD_TURN_DONE:

        case AGV_CMD_ERROR:
            return true;

        default:
            return false;
    }
}


int16_t AGV_Protocol_GetExpectedLength(
    uint8_t cmd
)
{
    switch (cmd)
    {
        /* No payload */

        case AGV_CMD_PING:
        case AGV_CMD_GET_STATUS:
        case AGV_CMD_GET_VERSION:
        case AGV_CMD_HEARTBEAT:
        case AGV_CMD_STOP:
        case AGV_CMD_TEACH_START:
        case AGV_CMD_TEACH_STOP:
            return 0;


        /* ACK */

        case AGV_CMD_ACK:
            return 2;


        /* STATUS */

        case AGV_CMD_STATUS:
            return 2;


        /* VERSION */

        case AGV_CMD_VERSION:
            return 5;


        /* ERROR */

        case AGV_CMD_ERROR:
            return -1;


        /* V2 */

        case AGV_CMD_ODOMETRY:
            return 10;

        case AGV_CMD_IMU_DATA:
            return 14;

        case AGV_CMD_MOTOR_STATUS:
            return 6;


        /* V3 */

        case AGV_CMD_MOVE:
            return 6;

        case AGV_CMD_TURN:
            return 4;

        case AGV_CMD_SET_REPEAT_SPEED:
            return 2;

        case AGV_CMD_MOVE_DONE:
            return 2;

        case AGV_CMD_TURN_DONE:
            return 2;


        /* V5 */

        case AGV_CMD_SEGMENT_START:
            return 6;

        case AGV_CMD_STEP_MOVE:
            return 6;

        case AGV_CMD_STEP_TURN:
            return 4;

        case AGV_CMD_SEGMENT_COMPLETE:
            return 2;


        default:
            return -2;
    }
}
