#include "motor.h"

extern TIM_HandleTypeDef htim1;

void Motor_Init(void) {
    // Enable STBY to turn on the motor driver
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN, GPIO_PIN_SET);

    // Initial state: Stopped
    Motor_SetSpeed(0, 0);
}

// Accepts left_speed and right_speed from -1000 (full reverse) to 1000 (full forward)
void Motor_SetSpeed(int16_t left_speed, int16_t right_speed) {
    
    // Determine Direction for Left Motor (A)
    if (left_speed > 0) {
        HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_RESET);
    } else if (left_speed < 0) {
        HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_SET);
        left_speed = -left_speed; // make positive for PWM
    } else {
        // Coast (both LOW). Can also be Short Brake (both HIGH) depending on preference.
        HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_RESET);
    }

    // Determine Direction for Right Motor (B)
    if (right_speed > 0) {
        HAL_GPIO_WritePin(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_BIN2_PORT, MOTOR_BIN2_PIN, GPIO_PIN_SET);
    } else if (right_speed < 0) {
        HAL_GPIO_WritePin(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(MOTOR_BIN2_PORT, MOTOR_BIN2_PIN, GPIO_PIN_RESET);
        right_speed = -right_speed;
    } else {
        HAL_GPIO_WritePin(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_BIN2_PORT, MOTOR_BIN2_PIN, GPIO_PIN_RESET);
    }

    // Cap the speeds to max 1000
    if (left_speed > 1000) left_speed = 1000;
    if (right_speed > 1000) right_speed = 1000;

    // Get max PWM value from timer autoreload register
    uint32_t max_pwm = __HAL_TIM_GET_AUTORELOAD(&htim1);

    // Map the input speed (0 to 1000) to the PWM range (0 to max_pwm)
    uint32_t left_pwm = (uint32_t)left_speed * max_pwm / 1000;
    uint32_t right_pwm = (uint32_t)right_speed * max_pwm / 1000;

    // Set PWM Compare values
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, left_pwm);   // PWMA
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, right_pwm);  // PWMB
}
