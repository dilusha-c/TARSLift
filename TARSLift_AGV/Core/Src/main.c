/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include "agv_stm32_c.h"
#include "motor.h"
#include "mpu6050_lite.h"
#include "vl53l0x.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static agv_stm32_parser_t agv_parser;
static uint8_t rx_byte;
static uint8_t tx_buffer[128];

static int32_t current_left_dist_mm = 0;
static int32_t current_right_dist_mm = 0;
static int16_t current_yaw_deg_x10 = 0;
static uint32_t last_telemetry_time = 0;

// Motor Trim Percentages (100 = 100%, 80 = 80%, etc)
static uint8_t trim_l_fwd = 100;
static uint8_t trim_r_fwd = 100;
static uint8_t trim_l_turn = 100;
static uint8_t trim_r_turn = 100;

// PID Variables
static float pid_kp_L = 1.0f, pid_ki_L = 0.0f, pid_kd_L = 0.0f;
static float pid_kp_R = 1.0f, pid_ki_R = 0.0f, pid_kd_R = 0.0f;
static float pid_integral_L = 0.0f, pid_integral_R = 0.0f;
static float pid_prev_err_L = 0.0f, pid_prev_err_R = 0.0f;

static float target_rpm_L = 0.0f, target_rpm_R = 0.0f;
static float enc_wheel_circ_mm = 138.2f;
static uint16_t enc_ppr_l = 287;
static uint16_t enc_ppr_r = 287;

static uint32_t last_pid_time = 0;

static VL53L0X_Dev_t tof_left;
static VL53L0X_Dev_t tof_center;
static VL53L0X_Dev_t tof_right;
static MPU6050_Data_t mpu_data;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
void AGV_STM32_DispatchCommand(const agv_packet_t *pkt);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void AGV_STM32_DispatchCommand(const agv_packet_t *pkt) {
    uint8_t tx_buf[128];
    switch (pkt->cmd) {
        case AGV_CMD_PING: {
            uint16_t tx_len = agv_build_ack(AGV_CMD_PING, AGV_RESULT_OK, pkt->seq, tx_buf, sizeof(tx_buf));
            HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            break;
        }

        case AGV_CMD_SET_REPEAT_SPEED: {
            uint16_t tx_len = agv_build_ack(AGV_CMD_SET_REPEAT_SPEED, AGV_RESULT_OK, pkt->seq, tx_buf, sizeof(tx_buf));
            HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            break;
        }

        case AGV_CMD_SET_MOTOR_TRIM: {
            uint8_t lFwd = 100, rFwd = 100, lTurn = 100, rTurn = 100;
            if (agv_parse_motor_trim(pkt, &lFwd, &rFwd, &lTurn, &rTurn)) {
                trim_l_fwd = lFwd;
                trim_r_fwd = rFwd;
                trim_l_turn = lTurn;
                trim_r_turn = rTurn;
                uint16_t tx_len = agv_build_ack(AGV_CMD_SET_MOTOR_TRIM, AGV_RESULT_OK, pkt->seq, tx_buf, sizeof(tx_buf));
                HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            }
            break;
        }

        case AGV_CMD_SET_PID_TUNING: {
            float kpL, kiL, kdL, kpR, kiR, kdR;
            if (agv_parse_set_pid_tuning(pkt, &kpL, &kiL, &kdL, &kpR, &kiR, &kdR)) {
                pid_kp_L = kpL; pid_ki_L = kiL; pid_kd_L = kdL;
                pid_kp_R = kpR; pid_ki_R = kiR; pid_kd_R = kdR;
                uint16_t tx_len = agv_build_ack(AGV_CMD_SET_PID_TUNING, AGV_RESULT_OK, pkt->seq, tx_buf, sizeof(tx_buf));
                HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            }
            break;
        }

        case AGV_CMD_SET_ENCODER_CONFIG: {
            float wheel_circ; uint16_t ppr_l, ppr_r;
            if (agv_parse_set_encoder_config(pkt, &wheel_circ, &ppr_l, &ppr_r)) {
                enc_wheel_circ_mm = wheel_circ;
                enc_ppr_l = ppr_l;
                enc_ppr_r = ppr_r;
                uint16_t tx_len = agv_build_ack(AGV_CMD_SET_ENCODER_CONFIG, AGV_RESULT_OK, pkt->seq, tx_buf, sizeof(tx_buf));
                HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            }
            break;
        }

        case AGV_CMD_TEACH_START: {
            uint16_t tx_len = agv_build_status(AGV_STATE_READY, 0x00, pkt->seq, tx_buf, sizeof(tx_buf));
            HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            break;
        }

        case AGV_CMD_HEARTBEAT: {
            uint16_t tx_len = agv_build_ack(AGV_CMD_HEARTBEAT, AGV_RESULT_OK, pkt->seq, tx_buf, sizeof(tx_buf));
            HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            break;
        }

        case AGV_CMD_GET_STATUS: {
            uint16_t tx_len = agv_build_status(AGV_STATE_READY, 0x00, pkt->seq, tx_buf, sizeof(tx_buf));
            HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            break;
        }

        case AGV_CMD_GET_VERSION: {
            uint16_t tx_len = agv_build_version(1, 0, 1, 2, 0, pkt->seq, tx_buf, sizeof(tx_buf));
            HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            break;
        }

        case AGV_CMD_MOVE: {
            int32_t distance_mm = 0;
            uint16_t speed_mm_s = 0;
            if (agv_parse_move(pkt, &distance_mm, &speed_mm_s)) {
                current_left_dist_mm += distance_mm;
                current_right_dist_mm += distance_mm;

                // Closed-Loop Drive Mapping
                float target_rpm = (speed_mm_s * 60.0f) / enc_wheel_circ_mm;
                
                if (distance_mm < 0) { // Reverse
                    target_rpm = -target_rpm;
                }
                
                target_rpm_L = target_rpm;
                target_rpm_R = target_rpm;

                uint16_t tx_len = agv_build_ack(AGV_CMD_MOVE, AGV_RESULT_OK, pkt->seq, tx_buf, sizeof(tx_buf));
                HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);

                tx_len = agv_build_odometry(current_left_dist_mm, current_right_dist_mm, current_yaw_deg_x10, pkt->seq, tx_buf, sizeof(tx_buf));
                HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);

                tx_len = agv_build_move_done(pkt->seq, AGV_RESULT_OK, pkt->seq + 1, tx_buf, sizeof(tx_buf));
                HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            }
            break;
        }

        case AGV_CMD_TURN: {
            int16_t angle_deg_x10 = 0;
            uint16_t speed_deg_s = 0;
            if (agv_parse_turn(pkt, &angle_deg_x10, &speed_deg_s)) {
                current_yaw_deg_x10 += angle_deg_x10;

                // Closed-Loop Turn Mapping (skid steer)
                // Roughly map degrees/s to RPM (can be tuned later)
                float turn_rpm = speed_deg_s * 1.5f; 
                
                if (angle_deg_x10 > 0) { // Turn Right
                    target_rpm_L = turn_rpm;
                    target_rpm_R = -turn_rpm;
                } else { // Turn Left
                    target_rpm_L = -turn_rpm;
                    target_rpm_R = turn_rpm;
                }

                uint16_t tx_len = agv_build_ack(AGV_CMD_TURN, AGV_RESULT_OK, pkt->seq, tx_buf, sizeof(tx_buf));
                HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);

                tx_len = agv_build_turn_done(pkt->seq, AGV_RESULT_OK, pkt->seq + 1, tx_buf, sizeof(tx_buf));
                HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            }
            break;
        }

        case AGV_CMD_STOP: {
            target_rpm_L = 0;
            target_rpm_R = 0;
            pid_integral_L = 0;
            pid_integral_R = 0;
            Motor_SetSpeed(0, 0); // Stop both motors
            
            uint16_t tx_len = agv_build_ack(AGV_CMD_STOP, AGV_RESULT_OK, pkt->seq, tx_buf, sizeof(tx_buf));
            HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            break;
        }

        case AGV_CMD_SET_SPEEDS: {
            int16_t left_speed_mm_s = 0;
            int16_t right_speed_mm_s = 0;
            if (agv_parse_set_speeds(pkt, &left_speed_mm_s, &right_speed_mm_s)) {
                // Convert mm/s to RPM
                target_rpm_L = (left_speed_mm_s * 60.0f) / enc_wheel_circ_mm;
                target_rpm_R = (right_speed_mm_s * 60.0f) / enc_wheel_circ_mm;

                uint16_t tx_len = agv_build_ack(AGV_CMD_SET_SPEEDS, AGV_RESULT_OK, pkt->seq, tx_buf, sizeof(tx_buf));
                HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            }
            break;
        }

        default: {
            uint16_t tx_len = agv_build_ack(pkt->cmd, AGV_RESULT_OK, pkt->seq, tx_buf, sizeof(tx_buf));
            HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            break;
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        if (agv_stm32_parser_feed(&agv_parser, rx_byte)) {
            AGV_STM32_DispatchCommand(&agv_parser.rx_pkt);
        }
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        __HAL_UART_CLEAR_PEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_OREFLAG(huart);
        huart->ErrorCode = HAL_UART_ERROR_NONE;
        huart->gState = HAL_UART_STATE_READY;
        huart->RxState = HAL_UART_STATE_READY;
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  agv_stm32_parser_init(&agv_parser);
  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
  
  // Start Motor Timers and Init Driver
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  
  // Start Encoder Timers
  HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
  
  Motor_Init();

  // Reset all ToF sensors
  HAL_GPIO_WritePin(GPIOB, TOF1_XSHUT_Pin | TOF2_XSHUT_Pin | TOF3_XSHUT_Pin, GPIO_PIN_RESET);
  HAL_Delay(10);

  // Init TOF1 (Left)
  HAL_GPIO_WritePin(GPIOB, TOF1_XSHUT_Pin, GPIO_PIN_SET);
  HAL_Delay(10);
  tof_left.I2C_Handler = &hi2c1;
  tof_left.i2cAddr = ADDRESS_DEFAULT;
  initVL53L0X(&tof_left, 0, &hi2c1);
  setAddress_VL53L0X(&tof_left, 0x54);

  // Init TOF2 (Center)
  HAL_GPIO_WritePin(GPIOB, TOF2_XSHUT_Pin, GPIO_PIN_SET);
  HAL_Delay(10);
  tof_center.I2C_Handler = &hi2c1;
  tof_center.i2cAddr = ADDRESS_DEFAULT;
  initVL53L0X(&tof_center, 0, &hi2c1);
  setAddress_VL53L0X(&tof_center, 0x56);

  // Init TOF3 (Right)
  HAL_GPIO_WritePin(GPIOB, TOF3_XSHUT_Pin, GPIO_PIN_SET);
  HAL_Delay(10);
  tof_right.I2C_Handler = &hi2c1;
  tof_right.i2cAddr = ADDRESS_DEFAULT;
  initVL53L0X(&tof_right, 0, &hi2c1);
  setAddress_VL53L0X(&tof_right, 0x58);

  // Start continuous mode
  startContinuous(&tof_left, 0); // 0 = Back-to-back mode
  startContinuous(&tof_center, 0);
  startContinuous(&tof_right, 0);

  // Init MPU6050
  MPU6050_Init(&hi2c1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();
    static uint32_t last_led_time = 0;
    if (now - last_led_time >= 500) { 
        last_led_time = now;
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }

    if (now - last_pid_time >= 20) { // 50Hz PID loop
        float dt = (now - last_pid_time) / 1000.0f;
        last_pid_time = now;

        // Read real hardware encoders (16-bit cast to handle underflow/overflow correctly for deltas)
        static int32_t last_enc_l = 0, last_enc_r = 0;
        int32_t current_enc_l = -(int16_t)__HAL_TIM_GET_COUNTER(&htim2);
        int32_t current_enc_r = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);
        
        int16_t delta_l = (int16_t)(current_enc_l - last_enc_l);
        int16_t delta_r = (int16_t)(current_enc_r - last_enc_r);
        last_enc_l = current_enc_l;
        last_enc_r = current_enc_r;

        float actual_rpm_L = ((float)delta_l / enc_ppr_l) / dt * 60.0f;
        float actual_rpm_R = ((float)delta_r / enc_ppr_r) / dt * 60.0f;

        // PID Left
        float err_L = target_rpm_L - actual_rpm_L;
        pid_integral_L += err_L * dt;
        float deriv_L = (err_L - pid_prev_err_L) / dt;
        pid_prev_err_L = err_L;
        float out_L = (pid_kp_L * err_L) + (pid_ki_L * pid_integral_L) + (pid_kd_L * deriv_L);

        // PID Right
        float err_R = target_rpm_R - actual_rpm_R;
        pid_integral_R += err_R * dt;
        float deriv_R = (err_R - pid_prev_err_R) / dt;
        pid_prev_err_R = err_R;
        float out_R = (pid_kp_R * err_R) + (pid_ki_R * pid_integral_R) + (pid_kd_R * deriv_R);

        // Apply output
        if (target_rpm_L == 0.0f && target_rpm_R == 0.0f) {
            Motor_SetSpeed(0, 0);
            pid_integral_L = 0;
            pid_integral_R = 0;
        } else {
            int16_t pwm_L = (int16_t)out_L;
            if (pwm_L > 1000) pwm_L = 1000;
            if (pwm_L < -1000) pwm_L = -1000;

            int16_t pwm_R = (int16_t)out_R;
            if (pwm_R > 1000) pwm_R = 1000;
            if (pwm_R < -1000) pwm_R = -1000;

            Motor_SetSpeed(pwm_L, pwm_R);
        }
    }

    if (now - last_telemetry_time >= 50) { // 20Hz Odometry Telemetry to ESP32
        last_telemetry_time = now;
        uint16_t tx_len = agv_build_odometry(current_left_dist_mm, current_right_dist_mm, current_yaw_deg_x10, 0, tx_buffer, sizeof(tx_buffer));
        
        // Auto-recover from HAL UART error state if noise or overrun occurred
        if (huart2.ErrorCode != HAL_UART_ERROR_NONE || huart2.gState == HAL_UART_STATE_ERROR) {
            __HAL_UART_CLEAR_OREFLAG(&huart2);
            __HAL_UART_CLEAR_NEFLAG(&huart2);
            __HAL_UART_CLEAR_FEFLAG(&huart2);
            huart2.ErrorCode = HAL_UART_ERROR_NONE;
            huart2.gState = HAL_UART_STATE_READY;
        }
        
        HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 50);

        // We already read the hardware encoders in the PID loop, but for telemetry we can just fetch the counters directly
        int32_t left_pulses = -(int16_t)__HAL_TIM_GET_COUNTER(&htim2);
        int32_t right_pulses = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);

        uint16_t enc_tx_len = agv_build_encoder_data(left_pulses, right_pulses, 0, tx_buffer, sizeof(tx_buffer));
        HAL_UART_Transmit(&huart2, tx_buffer, enc_tx_len, 50);

        // Read and send ToF data
        uint16_t dist_l = readRangeContinuousMillimeters(&tof_left, NULL);
        uint16_t dist_c = readRangeContinuousMillimeters(&tof_center, NULL);
        uint16_t dist_r = readRangeContinuousMillimeters(&tof_right, NULL);
        uint16_t tof_tx_len = agv_build_tof_data(dist_l, dist_c, dist_r, 0, tx_buffer, sizeof(tx_buffer));
        HAL_UART_Transmit(&huart2, tx_buffer, tof_tx_len, 50);

        // Read and send MPU6050 data
        MPU6050_Read_All(&hi2c1, &mpu_data);
        uint16_t imu_tx_len = agv_build_imu_data(mpu_data.accel_x, mpu_data.accel_y, mpu_data.accel_z, mpu_data.gyro_x, mpu_data.gyro_y, mpu_data.gyro_z, 0, 0, tx_buffer, sizeof(tx_buffer));
        HAL_UART_Transmit(&huart2, tx_buffer, imu_tx_len, 50);
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, STBY_Pin|BIN1_Pin|BIN2_Pin|TOF1_XSHUT_Pin
                          |TOF2_XSHUT_Pin|TOF3_XSHUT_Pin|AIN1_Pin|AIN2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : STBY_Pin BIN1_Pin BIN2_Pin TOF1_XSHUT_Pin
                           TOF2_XSHUT_Pin TOF3_XSHUT_Pin AIN1_Pin AIN2_Pin */
  GPIO_InitStruct.Pin = STBY_Pin|BIN1_Pin|BIN2_Pin|TOF1_XSHUT_Pin
                          |TOF2_XSHUT_Pin|TOF3_XSHUT_Pin|AIN1_Pin|AIN2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    for (volatile uint32_t i = 0; i < 500000; i++) {} // fast blink delay
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
