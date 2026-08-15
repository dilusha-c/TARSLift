#ifndef AGV_RESPONSES_H
#define AGV_RESPONSES_H

#include <stdint.h>

/* ACK result codes */

#define AGV_RESULT_OK               0x00
#define AGV_RESULT_BUSY             0x01
#define AGV_RESULT_INVALID_COMMAND  0x02
#define AGV_RESULT_INVALID_LENGTH   0x03
#define AGV_RESULT_CRC_ERROR        0x04
#define AGV_RESULT_ERROR            0x05


/* STM32 system states */

#define AGV_STATE_BOOT              0x00
#define AGV_STATE_READY             0x01
#define AGV_STATE_ERROR             0x02


/* Error severity */

#define AGV_SEVERITY_INFO           0x01
#define AGV_SEVERITY_WARNING        0x02
#define AGV_SEVERITY_ERROR          0x03
#define AGV_SEVERITY_CRITICAL       0x04


/* Error sources */

#define AGV_SOURCE_SYSTEM           0x01
#define AGV_SOURCE_UART             0x02
#define AGV_SOURCE_PROTOCOL         0x03


/* V1 error codes */

#define AGV_ERROR_UART_RX           0x0001
#define AGV_ERROR_UART_TX           0x0002
#define AGV_ERROR_UART_TIMEOUT      0x0003
#define AGV_ERROR_UART_CRC          0x0004
#define AGV_ERROR_INVALID_COMMAND   0x0005
#define AGV_ERROR_INVALID_LENGTH    0x0006
#define AGV_ERROR_INVALID_FRAME     0x0007
#define AGV_ERROR_SEQUENCE          0x0008

#endif
