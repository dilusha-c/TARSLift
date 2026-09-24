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
#include "mpu6050.h"
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

// ============================================================================
// CRASH FIX 1: DispatchCommand was called directly from the UART RxCplt ISR.
// Calling HAL_UART_Transmit() (blocking) from inside an ISR is UNDEFINED
// BEHAVIOR and will lock up the MCU when the transmit IRQ fires at higher
// priority. Fix: set a pending flag in the ISR; dequeue & dispatch in main loop.
// ============================================================================
static volatile bool cmd_pending = false;
static agv_packet_t pending_pkt;  // copy of last received packet

// Motor Trim Percentages (100 = 100%, 80 = 80%, etc)
static uint8_t trim_l_fwd = 100;
static uint8_t trim_r_fwd = 100;
static uint8_t trim_l_turn = 100;
static uint8_t trim_r_turn = 100;

// PID Variables
static float pid_kp_L = 1.0f, pid_ki_L = 0.0f, pid_kd_L = 0.0f;
static float pid_kp_R = 1.0f, pid_ki_R = 0.0f, pid_kd_R = 0.0f;
static float pid_integral_L = 0.0f, pid_integral_R = 0.0f;


static float target_rpm_L = 0.0f, target_rpm_R = 0.0f;
static bool pid_enabled = true;
static float enc_wheel_circ_mm = 138.2f;
static uint16_t enc_ppr_l = 287;
static uint16_t enc_ppr_r = 287;

// BUG FIX 3: repeat_speed_pct stores the speed percentage (0-100) sent by
// AGV_CMD_SET_REPEAT_SPEED. Previously this command was silently ignored.
static uint16_t repeat_speed_pct = 100; // default 100%

static uint32_t last_pid_time = 0;
static uint32_t last_tof_time = 0;  // Separate timer for ToF reads
static uint16_t cached_tof_l = 0, cached_tof_c = 0, cached_tof_r = 0;
static float tof_stop_distance_mm = 150.0f; // Configured stop distance (mm). 0 = disabled.
static bool obstacle_blocked = false;
static bool is_bench_speed_test = false; // Bypass obstacle interlock during PID tuning bench tests

static VL53L0X_Dev_t tof_left;
static VL53L0X_Dev_t tof_center;
static VL53L0X_Dev_t tof_right;
static MPU6050_t mpu_data;
static float current_yaw_float = 0.0f;
static float gyro_z_offset = 0.0f;
static bool calibrating_imu = false;
static uint16_t calib_samples = 0;
static float calib_sum = 0.0f;
static uint32_t i2c_error_count = 0;

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

// I2C runtime bus recovery: clocks SCL 9 times to free stuck slaves,
// then reinitializes the I2C peripheral.
void I2C_BusRecover(void) {
    // 1. Deinit the HAL I2C peripheral
    HAL_I2C_DeInit(&hi2c1);

    // 2. Bit-bang SCL (PB6) 9 times as GPIO open-drain
    GPIO_InitTypeDef recov = {0};
    recov.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    recov.Mode  = GPIO_MODE_OUTPUT_OD;
    recov.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &recov);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(1);
    for (int i = 0; i < 9; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); HAL_Delay(1);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   HAL_Delay(1);
    }
    // Generate STOP: SDA LOW -> HIGH while SCL HIGH
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET); HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);   HAL_Delay(1);
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);

    // 3. Reinit I2C peripheral
    MX_I2C1_Init();
}

// Check if the I2C bus is in an error state and recover if needed.
// Returns true if a recovery was performed (caller should skip sensor reads).
bool I2C_CheckAndRecover(void) {
    if (hi2c1.ErrorCode != HAL_I2C_ERROR_NONE ||
        hi2c1.State == HAL_I2C_STATE_ERROR ||
        hi2c1.State == HAL_I2C_STATE_BUSY ||
        hi2c1.State == HAL_I2C_STATE_BUSY_TX ||
        hi2c1.State == HAL_I2C_STATE_BUSY_RX) {
        i2c_error_count++;
        I2C_BusRecover();
        return true;
    }
    return false;
}
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

        case AGV_CMD_CALIBRATE_IMU: {
            calibrating_imu = true;
            calib_samples = 0;
            calib_sum = 0.0f;
            current_yaw_float = 0.0f;
            current_yaw_deg_x10 = 0;
            uint16_t tx_len = agv_build_ack(AGV_CMD_CALIBRATE_IMU, AGV_RESULT_OK, pkt->seq, tx_buf, sizeof(tx_buf));
            HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            break;
        }

        case AGV_CMD_SET_REPEAT_SPEED: {
            // BUG FIX 3: Actually parse and store the speed percentage.
            // agv_parse_set_repeat_speed() returns speed as a 0-100 percent value.
            uint16_t spd_pct = 100;
            if (agv_parse_set_repeat_speed(pkt, &spd_pct)) {
                if (spd_pct > 100) spd_pct = 100;
                repeat_speed_pct = spd_pct;
            }
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

        case AGV_CMD_SET_PID_ENABLE: {
            bool enabled;
            if (agv_parse_set_pid_enable(pkt, &enabled)) {
                pid_enabled = enabled;
                uint16_t tx_len = agv_build_ack(AGV_CMD_SET_PID_ENABLE, AGV_RESULT_OK, pkt->seq, tx_buf, sizeof(tx_buf));
                HAL_UART_Transmit(&huart2, tx_buf, tx_len, 100);
            }
            break;
        }

        case AGV_CMD_SET_TOF_CONFIG: {
            float stop_dist = 0.0f;
            if (agv_parse_set_tof_config(pkt, &stop_dist)) {
                tof_stop_distance_mm = stop_dist;
                uint16_t tx_len = agv_build_ack(AGV_CMD_SET_TOF_CONFIG, AGV_RESULT_OK, pkt->seq, tx_buf, sizeof(tx_buf));
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
            is_bench_speed_test = false;
            int32_t distance_mm = 0;
            uint16_t speed_mm_s = 0;
            if (agv_parse_move(pkt, &distance_mm, &speed_mm_s)) {
                current_left_dist_mm += distance_mm;
                current_right_dist_mm += distance_mm;

                // Closed-Loop Drive Mapping: apply repeat_speed_pct scale
                float target_rpm = (speed_mm_s * 60.0f) / enc_wheel_circ_mm;
                target_rpm = target_rpm * repeat_speed_pct / 100.0f;

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
            is_bench_speed_test = false;
            int16_t angle_deg_x10 = 0;
            uint16_t speed_deg_s = 0;
            if (agv_parse_turn(pkt, &angle_deg_x10, &speed_deg_s)) {
                current_yaw_deg_x10 += angle_deg_x10;

                // Closed-Loop Turn Mapping: convert turning speed to differential wheel RPM
                float track_width_mm = 160.0f;
                float turn_speed_rad_s = (speed_deg_s * 3.14159f) / 180.0f;
                float wheel_speed_mm_s = turn_speed_rad_s * (track_width_mm / 2.0f);
                float turn_rpm = (wheel_speed_mm_s * 60.0f) / enc_wheel_circ_mm;
                turn_rpm = turn_rpm * repeat_speed_pct / 100.0f;
                
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
            is_bench_speed_test = false;
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
                is_bench_speed_test = (target_rpm_L != 0.0f || target_rpm_R != 0.0f);

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
        // CRASH FIX 1: Do NOT call DispatchCommand() here.
        // HAL_UART_Transmit() is a blocking call that waits on the TX-complete flag.
        // Calling it from inside the UART ISR deadlocks when the TXE interrupt
        // fires (same or lower priority) — the ISR never returns -> HardFault.
        // Instead: parse the byte into the packet buffer, then set a flag for
        // the main loop to pick up and execute the dispatch safely.
        if (agv_stm32_parser_feed(&agv_parser, rx_byte)) {
            if (!cmd_pending) {  // don't overwrite an unhandled packet
                pending_pkt = agv_parser.rx_pkt;
                cmd_pending = true;
            }
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

  // CRASH FIX 3: VL53L0X ioTimeout is 0 by default, meaning checkTimeoutExpired()
  // ALWAYS returns false -> while loops in readRangeContinuousMillimeters() spin
  // forever if a sensor fails or I2C hangs. Set a 50ms timeout so a dead sensor
  // returns 65535 instead of locking the MCU.
  setTimeout(&tof_left, 50);
  setTimeout(&tof_center, 50);
  setTimeout(&tof_right, 50);

  // Init MPU6050
  MPU6050_Init(&hi2c1);
  Load_IMU_Offset();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();

    // CRASH FIX 1: Process any command queued by the UART ISR safely from the
    // main loop context, where HAL_UART_Transmit() blocking calls are safe.
    if (cmd_pending) {
        // Take a local copy of the packet BEFORE clearing the flag.
        // This prevents a race where the ISR fires after clear but before
        // we finish reading the struct.
        agv_packet_t local_pkt = pending_pkt;
        cmd_pending = false;  // clear flag so ISR can queue next packet
        AGV_STM32_DispatchCommand(&local_pkt);
    }

    static uint32_t last_led_time = 0;
    if (now - last_led_time >= 500) {
        last_led_time = now;
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }

    if (now - last_pid_time >= 20) { // 50Hz PID loop
        float dt = (now - last_pid_time) / 1000.0f;

        // BUG FIX 5: Guard against dt == 0 (same millisecond tick)
        if (dt < 0.001f) {
            goto pid_skip;
        }

        // I2C HEALTH CHECK: If bus is stuck, recover before reading sensors.
        if (I2C_CheckAndRecover()) {
            // Bus was in error — skip this PID cycle, sensors need reinit time
            goto pid_skip;
        }

        // We successfully passed all checks, now we can commit the new timestamp
        last_pid_time = now;

        // MPU6050 Read and Integrate
        MPU6050_Read_All(&hi2c1, &mpu_data);
        
        if (calibrating_imu) {
            calib_sum += mpu_data.Gz;
            calib_samples++;
            if (calib_samples >= 100) { // 2 seconds at 50Hz
                gyro_z_offset = calib_sum / 100.0f;
                calibrating_imu = false;
                Save_IMU_Offset();
            }
        } else {
            // Integrate Gyro Z (Gz is in degrees/second)
            // Subtract offset to prevent drift
            float raw_gz = mpu_data.Gz - gyro_z_offset;
            
            // Add a deadband to ignore tiny sensor noise when perfectly still
            if (raw_gz > -0.3f && raw_gz < 0.3f) {
                raw_gz = 0.0f;
            }
            
            current_yaw_float += raw_gz * dt;
            
            // Wrap around -180 to 180
            if (current_yaw_float > 180.0f) current_yaw_float -= 360.0f;
            if (current_yaw_float < -180.0f) current_yaw_float += 360.0f;
            
            current_yaw_deg_x10 = (int16_t)(current_yaw_float * 10.0f);
        }

        // Read real hardware encoders (16-bit cast to handle underflow/overflow correctly for deltas)
        static int32_t last_enc_l = 0, last_enc_r = 0;
        int32_t current_enc_l = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);
        int32_t current_enc_r = (int16_t)__HAL_TIM_GET_COUNTER(&htim2);
        
        int16_t delta_l = (int16_t)(current_enc_l - last_enc_l);
        int16_t delta_r = (int16_t)(current_enc_r - last_enc_r);
        last_enc_l = current_enc_l;
        last_enc_r = current_enc_r;

        float actual_rpm_L = ((float)delta_l / enc_ppr_l) / dt * 60.0f;
        float actual_rpm_R = ((float)delta_r / enc_ppr_r) / dt * 60.0f;

        // Static variables for Derivative on Measurement
        static float prev_actual_rpm_L = 0.0f;
        static float prev_actual_rpm_R = 0.0f;

        // ----- IMU HEADING ASSIST -----
        // Uses the MPU6050 yaw to fix heading drift when going straight ("no change")
        static float target_heading = 0.0f;
        static bool was_turning = true;
        
        float current_target_L = target_rpm_L;
        float current_target_R = target_rpm_R;
        
        if (target_rpm_L != target_rpm_R || target_rpm_L == 0.0f) {
            was_turning = true; // Not moving straight, or stopped
        } else {
            if (was_turning) {
                target_heading = current_yaw_float; // Lock in the heading
                was_turning = false;
            }
            
            // Calculate how far we drifted from our locked heading
            float heading_error = current_yaw_float - target_heading;
            
            // Wrap around -180 to 180
            if (heading_error > 180.0f) heading_error -= 360.0f;
            if (heading_error < -180.0f) heading_error += 360.0f;
            
            // Proportional correction: adjust target RPM based on drift
            // (e.g. 1.0 RPM per degree of error)
            float assist_kp_rpm = 2.0f; 
            float correction_rpm = heading_error * assist_kp_rpm;
            
            // Limit the maximum correction so we don't accidentally ask a motor 
            // to do 300 RPM just to fix a sharp drift
            if (correction_rpm > 30.0f) correction_rpm = 30.0f;
            if (correction_rpm < -30.0f) correction_rpm = -30.0f;
            
            // Apply correction
            current_target_L += correction_rpm;
            current_target_R -= correction_rpm;
            
            // Absolute safety clamp for target RPMs to prevent surging out of control
            if (current_target_L > 200.0f) current_target_L = 200.0f;
            if (current_target_L < -200.0f) current_target_L = -200.0f;
            if (current_target_R > 200.0f) current_target_R = 200.0f;
            if (current_target_R < -200.0f) current_target_R = -200.0f;
        }
        // ------------------------------

        // ----- PID Left -----
        float err_L = current_target_L - actual_rpm_L;
        
        // Anti-Windup for Integral (clamp to max PWM contribution)
        float max_i_L = (pid_ki_L > 0.0001f) ? (1000.0f / pid_ki_L) : 0.0f;
        pid_integral_L += err_L * dt;
        if (max_i_L > 0.0f) {
            if (pid_integral_L > max_i_L) pid_integral_L = max_i_L;
            if (pid_integral_L < -max_i_L) pid_integral_L = -max_i_L;
        } else {
            pid_integral_L = 0.0f;
        }

        // Derivative on Measurement (prevents derivative kick on target changes)
        float deriv_L = -(actual_rpm_L - prev_actual_rpm_L) / dt;
        prev_actual_rpm_L = actual_rpm_L;

        // Feed-Forward (open loop base speed estimation)
        float ff_L = current_target_L * (1000.0f / 330.0f);
        
        float out_L = ff_L + (pid_kp_L * err_L) + (pid_ki_L * pid_integral_L) + (pid_kd_L * deriv_L);


        // ----- PID Right -----
        float err_R = current_target_R - actual_rpm_R;
        
        // Anti-Windup for Integral
        float max_i_R = (pid_ki_R > 0.0001f) ? (1000.0f / pid_ki_R) : 0.0f;
        pid_integral_R += err_R * dt;
        if (max_i_R > 0.0f) {
            if (pid_integral_R > max_i_R) pid_integral_R = max_i_R;
            if (pid_integral_R < -max_i_R) pid_integral_R = -max_i_R;
        } else {
            pid_integral_R = 0.0f;
        }

        // Derivative on Measurement
        float deriv_R = -(actual_rpm_R - prev_actual_rpm_R) / dt;
        prev_actual_rpm_R = actual_rpm_R;

        // Feed-Forward (open loop base speed estimation)
        float ff_R = current_target_R * (1000.0f / 330.0f);

        float out_R = ff_R + (pid_kp_R * err_R) + (pid_ki_R * pid_integral_R) + (pid_kd_R * deriv_R);

        // Obstacle Safety Check (Front ToF Sensors)
        bool is_obstacle = false;
        if (tof_stop_distance_mm > 0.0f) {
            uint16_t thresh = (uint16_t)tof_stop_distance_mm;
            if ((cached_tof_l > 20 && cached_tof_l <= thresh) ||
                (cached_tof_c > 20 && cached_tof_c <= thresh) ||
                (cached_tof_r > 20 && cached_tof_r <= thresh)) {
                is_obstacle = true;
            }
        }
        obstacle_blocked = is_obstacle;
        float net_forward = (target_rpm_L + target_rpm_R) / 2.0f;

        // Apply output
        if (target_rpm_L == 0.0f && target_rpm_R == 0.0f) {
            Motor_SetSpeed(0, 0);
            pid_integral_L = 0;
            pid_integral_R = 0;
        } else if (obstacle_blocked && net_forward > 0.0f && !is_bench_speed_test) {
            // Forward motion blocked by obstacle during autonomous drive! Stop motors and clear integrals.
            // When obstacle clears (obstacle_blocked becomes false), motion automatically resumes!
            Motor_SetSpeed(0, 0);
            pid_integral_L = 0;
            pid_integral_R = 0;
        } else if (!pid_enabled) {
            // Open-loop control (assume 330 RPM = max PWM 1000)
            int16_t pwm_L = (int16_t)(current_target_L * (1000.0f / 330.0f));
            int16_t pwm_R = (int16_t)(current_target_R * (1000.0f / 330.0f));

            if (pwm_L > 1000) pwm_L = 1000;
            if (pwm_L < -1000) pwm_L = -1000;
            if (pwm_R > 1000) pwm_R = 1000;
            if (pwm_R < -1000) pwm_R = -1000;
            // BUG FIX 4: Apply motor trim factors to open-loop output
            pwm_L = (int16_t)((pwm_L >= 0) ? (pwm_L * trim_l_fwd / 100) : (pwm_L * trim_l_turn / 100));
            pwm_R = (int16_t)((pwm_R >= 0) ? (pwm_R * trim_r_fwd / 100) : (pwm_R * trim_r_turn / 100));
            Motor_SetSpeed(pwm_L, pwm_R);
        } else {
            int16_t pwm_L = (int16_t)out_L;
            if (pwm_L > 1000) pwm_L = 1000;
            if (pwm_L < -1000) pwm_L = -1000;

            int16_t pwm_R = (int16_t)out_R;
            if (pwm_R > 1000) pwm_R = 1000;
            if (pwm_R < -1000) pwm_R = -1000;

            // BUG FIX 4: Apply motor trim factors to closed-loop (PID) output
            pwm_L = (int16_t)((pwm_L >= 0) ? (pwm_L * trim_l_fwd / 100) : (pwm_L * trim_l_turn / 100));
            pwm_R = (int16_t)((pwm_R >= 0) ? (pwm_R * trim_r_fwd / 100) : (pwm_R * trim_r_turn / 100));

            Motor_SetSpeed(pwm_L, pwm_R);
        }
    }
    pid_skip:;

    // ================================================================
    // ToF sensors: Read at 5Hz (every 200ms) instead of 20Hz.
    // Each VL53L0X read can take up to 50ms on timeout, so reading
    // all 3 at 20Hz (50ms cycle) can starve the MPU6050 PID loop.
    // ================================================================
    if (now - last_tof_time >= 100) { // 10Hz ToF updates
        last_tof_time = now;
        // Non-blocking architecture: Only read if data is ready.
        // If a sensor fails or hangs, it will be skipped instantly!
        if (!I2C_CheckAndRecover()) {
            if (isDataReady(&tof_left)) cached_tof_l = readRangeContinuousMillimeters(&tof_left, NULL);
            if (isDataReady(&tof_center)) cached_tof_c = readRangeContinuousMillimeters(&tof_center, NULL);
            if (isDataReady(&tof_right)) cached_tof_r = readRangeContinuousMillimeters(&tof_right, NULL);
        }
    }

    if (now - last_telemetry_time >= 50) { // 20Hz Unified Telemetry to ESP32
        last_telemetry_time = now;
        
        // Auto-recover from HAL UART error state if noise or overrun occurred
        if (huart2.ErrorCode != HAL_UART_ERROR_NONE || huart2.gState == HAL_UART_STATE_ERROR) {
            __HAL_UART_CLEAR_OREFLAG(&huart2);
            __HAL_UART_CLEAR_NEFLAG(&huart2);
            __HAL_UART_CLEAR_FEFLAG(&huart2);
            huart2.ErrorCode = HAL_UART_ERROR_NONE;
            huart2.gState = HAL_UART_STATE_READY;
        }

        int32_t left_pulses = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);
        int32_t right_pulses = (int16_t)__HAL_TIM_GET_COUNTER(&htim2);
        
        // Build unified telemetry packet
        uint16_t tx_len = agv_build_telemetry_sync(
            current_left_dist_mm, current_right_dist_mm, // Odometry
            left_pulses, right_pulses,                   // Encoders
            cached_tof_l, cached_tof_c, cached_tof_r,    // ToF
            mpu_data.Accel_X_RAW, mpu_data.Accel_Y_RAW, mpu_data.Accel_Z_RAW, // IMU Accel
            mpu_data.Gyro_X_RAW, mpu_data.Gyro_Y_RAW, mpu_data.Gyro_Z_RAW,    // IMU Gyro
            current_yaw_deg_x10,                         // IMU Yaw
            0, tx_buffer, sizeof(tx_buffer)
        );
        
        HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 50);
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
