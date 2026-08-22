#ifndef MPU6050_LITE_H
#define MPU6050_LITE_H

#include "stm32f1xx_hal.h"

#define MPU6050_ADDR 0xD0 // 0x68 << 1

typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t temp;
} MPU6050_Data_t;

uint8_t MPU6050_Init(I2C_HandleTypeDef *hi2c);
void MPU6050_Read_All(I2C_HandleTypeDef *hi2c, MPU6050_Data_t *DataStruct);

#endif
