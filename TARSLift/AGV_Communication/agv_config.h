#ifndef AGV_CONFIG_H
#define AGV_CONFIG_H

#include <stdint.h>

#define AGV_SOF1                 0xAA
#define AGV_SOF2                 0x55

#define AGV_MAX_PAYLOAD          64U
#define AGV_MAX_FRAME_SIZE       (2U + 1U + 1U + 2U + AGV_MAX_PAYLOAD + 2U)

#define AGV_UART_BAUD_RATE       115200U

#define AGV_ACK_TIMEOUT_MS       200U
#define AGV_MAX_RETRIES          3U

#define AGV_HEARTBEAT_INTERVAL_MS 500U
#define AGV_HEARTBEAT_TIMEOUT_MS  1000U

#define AGV_PROTOCOL_MAJOR       1U
#define AGV_PROTOCOL_MINOR       0U

#define AGV_FIRMWARE_MAJOR       1U
#define AGV_FIRMWARE_MINOR       0U
#define AGV_FIRMWARE_PATCH       0U

#endif
