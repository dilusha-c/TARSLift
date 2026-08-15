#ifndef AGV_COMMANDS_H
#define AGV_COMMANDS_H

#include <stdint.h>

typedef enum
{
    /* ESP32 -> STM32 */

    AGV_CMD_PING              = 0x01,
    AGV_CMD_GET_STATUS        = 0x02,
    AGV_CMD_GET_VERSION       = 0x03,
    AGV_CMD_HEARTBEAT         = 0x04,

    AGV_CMD_MOVE              = 0x10,
    AGV_CMD_TURN              = 0x11,
    AGV_CMD_STOP              = 0x12,
    AGV_CMD_SET_REPEAT_SPEED  = 0x13,

    AGV_CMD_TEACH_START       = 0x20,
    AGV_CMD_TEACH_STOP        = 0x21,

    AGV_CMD_SEGMENT_START     = 0x50,
    AGV_CMD_STEP_MOVE         = 0x51,
    AGV_CMD_STEP_TURN         = 0x52,
    AGV_CMD_SEGMENT_COMPLETE  = 0x93,

    /* STM32 -> ESP32 */

    AGV_CMD_ODOMETRY          = 0x30,
    AGV_CMD_IMU_DATA          = 0x31,
    AGV_CMD_MOTOR_STATUS      = 0x60,

    AGV_CMD_ACK               = 0x81,
    AGV_CMD_STATUS            = 0x82,
    AGV_CMD_VERSION           = 0x83,

    AGV_CMD_MOVE_DONE         = 0x91,
    AGV_CMD_TURN_DONE         = 0x92,

    AGV_CMD_ERROR             = 0xE0

} AGV_Command_t;

#endif
