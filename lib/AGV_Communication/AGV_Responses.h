#pragma once

#include <stdint.h>

namespace AGVResult
{
    constexpr uint8_t OK               = 0x00;
    constexpr uint8_t BUSY             = 0x01;
    constexpr uint8_t INVALID_COMMAND  = 0x02;
    constexpr uint8_t INVALID_LENGTH   = 0x03;
    constexpr uint8_t CRC_ERROR        = 0x04;
    constexpr uint8_t ERROR            = 0x05;
}


namespace AGVState
{
    constexpr uint8_t BOOT  = 0x00;
    constexpr uint8_t READY = 0x01;
    constexpr uint8_t ERROR = 0x02;
}


namespace AGVSeverity
{
    constexpr uint8_t INFO     = 0x01;
    constexpr uint8_t WARNING  = 0x02;
    constexpr uint8_t ERROR    = 0x03;
    constexpr uint8_t CRITICAL = 0x04;
}


namespace AGVSource
{
    constexpr uint8_t SYSTEM   = 0x01;
    constexpr uint8_t UART     = 0x02;
    constexpr uint8_t PROTOCOL = 0x03;
}


namespace AGVErrorCode
{
    // UART Protocol Errors
    constexpr uint16_t UART_RX_ERROR       = 0x0001;
    constexpr uint16_t UART_TX_ERROR       = 0x0002;
    constexpr uint16_t UART_TIMEOUT        = 0x0003;
    constexpr uint16_t UART_CRC_ERROR      = 0x0004;
    constexpr uint16_t INVALID_COMMAND     = 0x0005;
    constexpr uint16_t INVALID_LENGTH      = 0x0006;
    constexpr uint16_t INVALID_FRAME       = 0x0007;
    constexpr uint16_t SEQUENCE_ERROR      = 0x0008;

    // Encoder Errors
    constexpr uint16_t ENC1_NO_SIGNAL      = 0x0010;
    constexpr uint16_t ENC1_INVALID_READING= 0x0011;
    constexpr uint16_t ENC1_TIMEOUT        = 0x0012;
    constexpr uint16_t ENC1_OVERFLOW       = 0x0013;
    constexpr uint16_t ENC2_NO_SIGNAL      = 0x0020;
    constexpr uint16_t ENC2_INVALID_READING= 0x0021;
    constexpr uint16_t ENC2_TIMEOUT        = 0x0022;
    constexpr uint16_t ENC2_OVERFLOW       = 0x0023;

    // MPU6050 IMU Errors
    constexpr uint16_t MPU_NOT_FOUND       = 0x0030;
    constexpr uint16_t MPU_READ_ERROR      = 0x0031;
    constexpr uint16_t MPU_DATA_INVALID    = 0x0032;
    constexpr uint16_t MPU_CALIBRATION_ERR = 0x0033;

    // Motor & Motion Execution Errors
    constexpr uint16_t MOTOR_DRIVER_ERROR  = 0x0040;
    constexpr uint16_t MOTOR_LEFT_ERROR    = 0x0041;
    constexpr uint16_t MOTOR_RIGHT_ERROR   = 0x0042;
    constexpr uint16_t MOTOR_TIMEOUT       = 0x0043;
    constexpr uint16_t MOVE_TIMEOUT        = 0x0044;
    constexpr uint16_t TURN_TIMEOUT        = 0x0045;
    constexpr uint16_t TARGET_NOT_REACHED  = 0x0046;
    constexpr uint16_t POSITION_ERROR      = 0x0047;

    // ESP32 RC522 RFID Errors
    constexpr uint16_t RFID_NOT_FOUND      = 0x0050;
    constexpr uint16_t RFID_READ_ERROR     = 0x0051;
    constexpr uint16_t RFID_INVALID_TAG    = 0x0052;
    constexpr uint16_t RFID_WRONG_TAG      = 0x0053;
    constexpr uint16_t RFID_TIMEOUT        = 0x0054;
    constexpr uint16_t RFID_MODULE_ERROR   = 0x0055;
}