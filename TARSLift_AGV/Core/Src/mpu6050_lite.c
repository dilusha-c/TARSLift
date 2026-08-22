#include "mpu6050_lite.h"

uint8_t MPU6050_Init(I2C_HandleTypeDef *hi2c) {
    uint8_t check;
    uint8_t Data;

    // Check device ID
    HAL_I2C_Mem_Read(hi2c, MPU6050_ADDR, 0x75, 1, &check, 1, 1000);

    if (check == 0x68) {
        // Power management 1: wake up
        Data = 0x00;
        HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, 0x6B, 1, &Data, 1, 1000);

        // SMPLRT_DIV: Set sample rate to 1kHz
        Data = 0x07;
        HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, 0x19, 1, &Data, 1, 1000);

        // CONFIG: DLPF config
        Data = 0x00;
        HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, 0x1A, 1, &Data, 1, 1000);

        // GYRO_CONFIG: 500 deg/s
        Data = 0x08;
        HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, 0x1B, 1, &Data, 1, 1000);

        // ACCEL_CONFIG: +- 8g
        Data = 0x10;
        HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, 0x1C, 1, &Data, 1, 1000);

        return 0; // Success
    }
    return 1; // Error
}

void MPU6050_Read_All(I2C_HandleTypeDef *hi2c, MPU6050_Data_t *DataStruct) {
    uint8_t Rec_Data[14];
    HAL_I2C_Mem_Read(hi2c, MPU6050_ADDR, 0x3B, 1, Rec_Data, 14, 1000);

    DataStruct->accel_x = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->accel_y = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
    DataStruct->accel_z = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);
    DataStruct->temp    = (int16_t)(Rec_Data[6] << 8 | Rec_Data[7]);
    DataStruct->gyro_x  = (int16_t)(Rec_Data[8] << 8 | Rec_Data[9]);
    DataStruct->gyro_y  = (int16_t)(Rec_Data[10] << 8 | Rec_Data[11]);
    DataStruct->gyro_z  = (int16_t)(Rec_Data[12] << 8 | Rec_Data[13]);
}
