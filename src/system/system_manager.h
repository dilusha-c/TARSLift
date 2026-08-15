#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <Arduino.h>
#include "config.h"

struct SystemHealth {
    SubsystemState esp32;
    SubsystemState stm32;
    SubsystemState motor;
    SubsystemState encoder;
    SubsystemState mpu6050;
    SubsystemState rfid;
    SubsystemState tof;
    SubsystemState battery;
    SubsystemState uart;
};

void systemManagerInit();
void systemManagerUpdate();

uint32_t getSystemUptimeS();
size_t getFreeHeap();
size_t getMinFreeHeap();
size_t getMaxAllocHeap();
uint32_t getCpuFreqMHz();

void updatePerformanceMetrics(float loopMs, uint32_t loopHz, float cpuUsagePct);
float getAverageLoopMs();
uint32_t getLoopHz();
float getCpuUsagePct();
float getRamUsagePct();
float getEsp32TempC();

void getLittleFSInfo(size_t &totalBytes, size_t &usedBytes);
int8_t getWifiRSSI();
SystemHealth getSystemHealth();

#endif // SYSTEM_MANAGER_H
