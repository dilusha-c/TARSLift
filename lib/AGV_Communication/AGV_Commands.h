#pragma once

#include <stdint.h>

namespace AGVCommand
{
    // =====================================================
    // ESP32 -> STM32
    // =====================================================

    // Basic communication
    constexpr uint8_t PING             = 0x01;
    constexpr uint8_t GET_STATUS       = 0x02;
    constexpr uint8_t GET_VERSION      = 0x03;
    constexpr uint8_t HEARTBEAT        = 0x04;

    // Motion control
    constexpr uint8_t MOVE             = 0x10;
    constexpr uint8_t TURN             = 0x11;
    constexpr uint8_t STOP             = 0x12;
    constexpr uint8_t SET_REPEAT_SPEED = 0x13;

    // Teach / Repeat
    constexpr uint8_t TEACH_START      = 0x20;
    constexpr uint8_t TEACH_STOP       = 0x21;

    // RFID route segments
    constexpr uint8_t SEGMENT_START    = 0x50;
    constexpr uint8_t STEP_MOVE        = 0x51;
    constexpr uint8_t STEP_TURN        = 0x52;
    constexpr uint8_t SEGMENT_COMPLETE = 0x93;


    // =====================================================
    // STM32 -> ESP32
    // =====================================================

    // Sensor / motion feedback
    constexpr uint8_t ODOMETRY         = 0x30;
    constexpr uint8_t IMU_DATA         = 0x31;
    constexpr uint8_t MOTOR_STATUS     = 0x60;

    // Command completion
    constexpr uint8_t MOVE_DONE        = 0x91;
    constexpr uint8_t TURN_DONE        = 0x92;

    // Common responses
    constexpr uint8_t ACK              = 0x81;
    constexpr uint8_t STATUS           = 0x82;
    constexpr uint8_t VERSION          = 0x83;
    constexpr uint8_t ERROR            = 0xE0;
}