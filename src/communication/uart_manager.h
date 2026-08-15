#ifndef STM32_UART_H
#define STM32_UART_H

#include <Arduino.h>
#include "AGV_Communication.h"

void stm32UartInit();
void stm32UartUpdate();
void checkUartWatchdog();
bool isStm32Connected();


// Motion Command Transmitters (ESP32 -> STM32)
bool sendStm32Move(int32_t distanceMm, uint16_t speedMmS);
bool sendStm32Turn(int16_t angleDegX10, uint16_t speedDegS);
bool sendStm32Stop();
bool sendStm32SetRepeatSpeed(uint16_t speedPercent);

// Teach Mode Transmitters (ESP32 -> STM32)
bool sendStm32TeachStart();
bool sendStm32TeachStop();

// Segment Trajectory Transmitters (V5 Protocol, ESP32 -> STM32)
bool sendStm32SegmentStart(uint16_t segmentId, uint16_t srcRfidId, uint16_t dstRfidId);
bool sendStm32StepMove(int32_t distanceMm, uint16_t speedMmS);
bool sendStm32StepTurn(int16_t angleDegX10, uint16_t speedDegS);
bool sendStm32SegmentComplete(uint16_t segmentId);

#endif // STM32_UART_H
