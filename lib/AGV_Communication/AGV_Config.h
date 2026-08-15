#pragma once

#include <stdint.h>

namespace AGVConfig
{
    constexpr uint8_t SOF1 = 0xAA;
    constexpr uint8_t SOF2 = 0x55;

    constexpr uint16_t MAX_PAYLOAD_SIZE = 64;

    constexpr uint32_t UART_BAUD_RATE = 115200;

    constexpr uint32_t ACK_TIMEOUT_MS = 200;
    constexpr uint8_t MAX_RETRIES = 3;

    constexpr uint32_t HEARTBEAT_INTERVAL_MS = 500;
    constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 1000;
}