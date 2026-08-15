#if defined(STM32F1) || defined(STM32F103xB) || defined(STM32)

/**
 * ============================================================================
 * TARSLIFT AGV - STM32F103 CUBE HAL / KEIL MDK INTEGRATION EXAMPLE
 * File: stm32_main_example.c
 * ============================================================================
 * Complete ready-to-run C code for STM32F103 HAL UART communication test with ESP32-S3!
 */

#include "agv_stm32_c.h"

/* Global instances */
static agv_stm32_parser_t agv_parser;
static uint8_t rx_byte;
static uint8_t tx_buffer[128];

/* Global Motor / Sensor state */
static int32_t current_left_dist_mm = 0;
static int32_t current_right_dist_mm = 0;
static int16_t current_yaw_deg_x10 = 0;

/* External UART handle (Change huart2 to huart1 if using USART1) */
extern UART_HandleTypeDef huart2;

void AGV_STM32_Init(void) {
    agv_stm32_parser_init(&agv_parser);
    
    /* Start HAL UART Interrupt reception for single byte */
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}

/**
 * @brief STM32 Command Dispatcher: Processes valid frames from ESP32-S3
 */
void AGV_STM32_DispatchCommand(const agv_packet_t *pkt) {
    switch (pkt->cmd) {
        case AGV_CMD_PING: {
            /* Echo ACK OK back to ESP32 */
            uint16_t tx_len = agv_build_ack(AGV_CMD_PING, AGV_RESULT_OK, pkt->seq, tx_buffer, sizeof(tx_buffer));
            HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);
            break;
        }

        case AGV_CMD_HEARTBEAT: {
            /* Send Heartbeat ACK back to ESP32 */
            uint16_t tx_len = agv_build_ack(AGV_CMD_HEARTBEAT, AGV_RESULT_OK, pkt->seq, tx_buffer, sizeof(tx_buffer));
            HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);
            break;
        }

        case AGV_CMD_GET_VERSION: {
            /* Report STM32 Protocol V1.0, Firmware V1.2.0 */
            uint16_t tx_len = agv_build_version(1, 0, 1, 2, 0, pkt->seq, tx_buffer, sizeof(tx_buffer));
            HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);
            break;
        }

        case AGV_CMD_MOVE: {
            int32_t distance_mm = 0;
            uint16_t speed_mm_s = 0;
            if (agv_parse_move(pkt, &distance_mm, &speed_mm_s)) {
                current_left_dist_mm += distance_mm;
                current_right_dist_mm += distance_mm;

                /* Send immediate ACK */
                uint16_t tx_len = agv_build_ack(AGV_CMD_MOVE, AGV_RESULT_OK, pkt->seq, tx_buffer, sizeof(tx_buffer));
                HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);

                /* Respond with updated Odometry feedback */
                tx_len = agv_build_odometry(current_left_dist_mm, current_right_dist_mm, current_yaw_deg_x10, pkt->seq, tx_buffer, sizeof(tx_buffer));
                HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);

                /* Notify Move Completion event */
                tx_len = agv_build_move_done(pkt->seq, AGV_RESULT_OK, pkt->seq + 1, tx_buffer, sizeof(tx_buffer));
                HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);
            }
            break;
        }

        case AGV_CMD_TURN: {
            int16_t angle_deg_x10 = 0;
            uint16_t speed_deg_s = 0;
            if (agv_parse_turn(pkt, &angle_deg_x10, &speed_deg_s)) {
                current_yaw_deg_x10 += angle_deg_x10;

                uint16_t tx_len = agv_build_ack(AGV_CMD_TURN, AGV_RESULT_OK, pkt->seq, tx_buffer, sizeof(tx_buffer));
                HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);

                /* Notify Turn Completion event */
                tx_len = agv_build_turn_done(pkt->seq, AGV_RESULT_OK, pkt->seq + 1, tx_buffer, sizeof(tx_buffer));
                HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);
            }
            break;
        }

        case AGV_CMD_STOP: {
            /* Emergency brake motors */
            uint16_t tx_len = agv_build_ack(AGV_CMD_STOP, AGV_RESULT_OK, pkt->seq, tx_buffer, sizeof(tx_buffer));
            HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);
            break;
        }

        case AGV_CMD_SET_REPEAT_SPEED: {
            uint16_t speed_pct = 100;
            if (agv_parse_set_repeat_speed(pkt, &speed_pct)) {
                uint16_t tx_len = agv_build_ack(AGV_CMD_SET_REPEAT_SPEED, AGV_RESULT_OK, pkt->seq, tx_buffer, sizeof(tx_buffer));
                HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);
            }
            break;
        }

        case AGV_CMD_TEACH_START: {
            uint16_t tx_len = agv_build_ack(AGV_CMD_TEACH_START, AGV_RESULT_OK, pkt->seq, tx_buffer, sizeof(tx_buffer));
            HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);
            break;
        }

        case AGV_CMD_TEACH_STOP: {
            uint16_t tx_len = agv_build_ack(AGV_CMD_TEACH_STOP, AGV_RESULT_OK, pkt->seq, tx_buffer, sizeof(tx_buffer));
            HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);
            break;
        }

        case AGV_CMD_SEGMENT_START: {
            uint16_t seg_id = 0, src_rfid = 0, dst_rfid = 0;
            if (agv_parse_segment_start(pkt, &seg_id, &src_rfid, &dst_rfid)) {
                uint16_t tx_len = agv_build_ack(AGV_CMD_SEGMENT_START, AGV_RESULT_OK, pkt->seq, tx_buffer, sizeof(tx_buffer));
                HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);
            }
            break;
        }

        case AGV_CMD_STEP_MOVE: {
            int32_t distance_mm = 0;
            uint16_t speed_mm_s = 0;
            if (agv_parse_move(pkt, &distance_mm, &speed_mm_s)) {
                current_left_dist_mm += distance_mm;
                current_right_dist_mm += distance_mm;
                uint16_t tx_len = agv_build_ack(AGV_CMD_STEP_MOVE, AGV_RESULT_OK, pkt->seq, tx_buffer, sizeof(tx_buffer));
                HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);
            }
            break;
        }

        case AGV_CMD_STEP_TURN: {
            int16_t angle_deg_x10 = 0;
            uint16_t speed_deg_s = 0;
            if (agv_parse_turn(pkt, &angle_deg_x10, &speed_deg_s)) {
                current_yaw_deg_x10 += angle_deg_x10;
                uint16_t tx_len = agv_build_ack(AGV_CMD_STEP_TURN, AGV_RESULT_OK, pkt->seq, tx_buffer, sizeof(tx_buffer));
                HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);
            }
            break;
        }

        case AGV_CMD_SEGMENT_COMPLETE: {
            uint16_t seg_id = 0;
            if (agv_parse_segment_complete(pkt, &seg_id)) {
                uint16_t tx_len = agv_build_ack(AGV_CMD_SEGMENT_COMPLETE, AGV_RESULT_OK, pkt->seq, tx_buffer, sizeof(tx_buffer));
                HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);
            }
            break;
        }

        default:
            break;
    }
}

/**
 * @brief Call this from HAL_UART_RxCpltCallback on each received byte
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) { // Adjust USART instance if needed
        if (agv_stm32_parser_feed(&agv_parser, rx_byte)) {
            /* A full valid packet was parsed and CRC-verified */
            AGV_STM32_DispatchCommand(&agv_parser.rx_pkt);
        }
        /* Re-arm interrupt for next byte */
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    }
}

/**
 * @brief Periodically transmit V2 Odometry feedback to ESP32 (e.g., at 20Hz / 50ms)
 */
void AGV_STM32_SendOdometryTelemetry(void) {
    uint16_t tx_len = agv_build_odometry(current_left_dist_mm, current_right_dist_mm, current_yaw_deg_x10, 0, tx_buffer, sizeof(tx_buffer));
    HAL_UART_Transmit(&huart2, tx_buffer, tx_len, 100);
}

#endif // STM32



