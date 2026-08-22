#ifndef MOTOR_H
#define MOTOR_H

#include "main.h"

// TB6612FNG Control Pins
#define MOTOR_STBY_PORT STBY_GPIO_Port
#define MOTOR_STBY_PIN  STBY_Pin

#define MOTOR_AIN1_PORT AIN1_GPIO_Port
#define MOTOR_AIN1_PIN  AIN1_Pin
#define MOTOR_AIN2_PORT AIN2_GPIO_Port
#define MOTOR_AIN2_PIN  AIN2_Pin

#define MOTOR_BIN1_PORT BIN1_GPIO_Port
#define MOTOR_BIN1_PIN  BIN1_Pin
#define MOTOR_BIN2_PORT BIN2_GPIO_Port
#define MOTOR_BIN2_PIN  BIN2_Pin

void Motor_Init(void);
void Motor_SetSpeed(int16_t left_speed, int16_t right_speed); // Accepts -1000 to 1000

#endif // MOTOR_H
