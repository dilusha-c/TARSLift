#ifndef AGV_STM32_C_H
#define AGV_STM32_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
   AGV UART PROTOCOL SPECIFICATION - PURE C HEADER FOR STM32F103
   ============================================================================ */

#define AGV_SOF1                  0xAA
#define AGV_SOF2                  0x55
#define AGV_MAX_PAYLOAD_SIZE      64

/* Master Command IDs */
#define AGV_CMD_PING              0x01
#define AGV_CMD_GET_STATUS        0x02
#define AGV_CMD_GET_VERSION       0x03
#define AGV_CMD_HEARTBEAT         0x04

#define AGV_CMD_MOVE              0x10
#define AGV_CMD_TURN              0x11
#define AGV_CMD_STOP              0x12
#define AGV_CMD_SET_REPEAT_SPEED  0x13
#define AGV_CMD_SET_MOTOR_TRIM    0x14
#define AGV_CMD_SET_PID_TUNING    0x15
#define AGV_CMD_SET_ENCODER_CONFIG 0x16
#define AGV_CMD_SET_SPEEDS        0x19

#define AGV_CMD_TEACH_START       0x20
#define AGV_CMD_TEACH_STOP        0x21

#define AGV_CMD_ODOMETRY          0x30
#define AGV_CMD_IMU_DATA          0x31
#define AGV_CMD_ENCODER_DATA      0x32
#define AGV_CMD_TOF_DATA          0x33
#define AGV_CMD_MOTOR_STATUS      0x60

#define AGV_CMD_SEGMENT_START     0x50
#define AGV_CMD_STEP_MOVE         0x51
#define AGV_CMD_STEP_TURN         0x52
#define AGV_CMD_SEGMENT_COMPLETE  0x93

#define AGV_CMD_ACK               0x81
#define AGV_CMD_STATUS            0x82
#define AGV_CMD_VERSION           0x83
#define AGV_CMD_MOVE_DONE         0x91
#define AGV_CMD_TURN_DONE         0x92
#define AGV_CMD_ERROR             0xE0

/* Result Codes */
#define AGV_RESULT_OK             0x00
#define AGV_RESULT_BUSY           0x01
#define AGV_RESULT_INVALID_CMD    0x02
#define AGV_RESULT_INVALID_LEN    0x03
#define AGV_RESULT_CRC_ERROR      0x04
#define AGV_RESULT_ERROR          0x05

/* System States */
#define AGV_STATE_BOOT            0x00
#define AGV_STATE_READY           0x01
#define AGV_STATE_ERROR           0x02

/* Packet Structure */
typedef struct {
    uint8_t cmd;
    uint8_t seq;
    uint16_t length;
    uint8_t payload[AGV_MAX_PAYLOAD_SIZE];
    uint16_t crc;
} agv_packet_t;

/* Parser State Machine Structure */
typedef enum {
    AGV_PARSE_SOF1 = 0,
    AGV_PARSE_SOF2,
    AGV_PARSE_CMD,
    AGV_PARSE_SEQ,
    AGV_PARSE_LEN_L,
    AGV_PARSE_LEN_H,
    AGV_PARSE_PAYLOAD,
    AGV_PARSE_CRC_L,
    AGV_PARSE_CRC_H
} agv_parse_state_t;

typedef struct {
    agv_parse_state_t state;
    uint8_t rx_buf[8 + AGV_MAX_PAYLOAD_SIZE];
    uint16_t rx_idx;
    uint16_t expected_len;
    agv_packet_t rx_pkt;
} agv_stm32_parser_t;

/* CRC-16 Engine */
uint16_t agv_crc16_calc(const uint8_t *data, uint16_t len);

/* Packet Encoding & Decoding */
uint16_t agv_encode_packet(const agv_packet_t *pkt, uint8_t *out_buf, uint16_t max_len);
bool agv_decode_packet(const uint8_t *in_buf, uint16_t buf_len, agv_packet_t *pkt);

/* Non-blocking UART Byte Stream Parser */
void agv_stm32_parser_init(agv_stm32_parser_t *parser);
bool agv_stm32_parser_feed(agv_stm32_parser_t *parser, uint8_t byte);

/* STM32 Response Packet Builders (HAL_UART_Transmit compatible) */
uint16_t agv_build_ack(uint8_t ack_cmd, uint8_t result, uint8_t seq, uint8_t *out_buf, uint16_t max_len);
uint16_t agv_build_status(uint8_t state_val, uint8_t error_flags, uint8_t seq, uint8_t *out_buf, uint16_t max_len);
uint16_t agv_build_version(uint8_t proto_maj, uint8_t proto_min, uint8_t fw_maj, uint8_t fw_min, uint8_t fw_patch, uint8_t seq, uint8_t *out_buf, uint16_t max_len);
uint16_t agv_build_error(uint16_t error_code, uint8_t severity, uint8_t source, uint8_t seq, uint8_t *out_buf, uint16_t max_len);

/* STM32 Telemetry & Event Builders */
uint16_t agv_build_odometry(int32_t left_mm, int32_t right_mm, int16_t yaw_deg_x10, uint8_t seq, uint8_t *out_buf, uint16_t max_len);
uint16_t agv_build_encoder_data(int32_t left_pulses, int32_t right_pulses, uint8_t seq, uint8_t *out_buf, uint16_t max_len);
uint16_t agv_build_imu_data(int16_t ax, int16_t ay, int16_t az, int16_t gx, int16_t gy, int16_t gz, int16_t yaw_deg_x10, uint8_t seq, uint8_t *out_buf, uint16_t max_len);
uint16_t agv_build_tof_data(uint16_t left_mm, uint16_t center_mm, uint16_t right_mm, uint8_t seq, uint8_t *out_buf, uint16_t max_len);
uint16_t agv_build_motor_status(int16_t left_pwm, int16_t right_pwm, int8_t left_dir, int8_t right_dir, uint8_t seq, uint8_t *out_buf, uint16_t max_len);
uint16_t agv_build_move_done(uint8_t orig_seq, uint8_t result, uint8_t seq, uint8_t *out_buf, uint16_t max_len);
uint16_t agv_build_turn_done(uint8_t orig_seq, uint8_t result, uint8_t seq, uint8_t *out_buf, uint16_t max_len);

/* Motion Command Payload Parsers (For STM32 to decode incoming ESP32 packets) */
bool agv_parse_move(const agv_packet_t *pkt, int32_t *distance_mm, uint16_t *speed_mm_s);
bool agv_parse_turn(const agv_packet_t *pkt, int16_t *angle_deg_x10, uint16_t *speed_deg_s);
bool agv_parse_set_repeat_speed(const agv_packet_t *pkt, uint16_t *speed_percent);
bool agv_parse_set_speeds(const agv_packet_t *pkt, int16_t *leftSpeedMmS, int16_t *rightSpeedMmS);
bool agv_parse_motor_trim(const agv_packet_t *pkt, uint8_t *lFwd, uint8_t *rFwd, uint8_t *lTurn, uint8_t *rTurn);
bool agv_parse_segment_start(const agv_packet_t *pkt, uint16_t *segment_id, uint16_t *src_rfid_id, uint16_t *dst_rfid_id);
bool agv_parse_segment_complete(const agv_packet_t *pkt, uint16_t *segment_id);
bool agv_parse_set_pid_tuning(const agv_packet_t *pkt, float *kpL, float *kiL, float *kdL, float *kpR, float *kiR, float *kdR);
bool agv_parse_set_encoder_config(const agv_packet_t *pkt, float *wheel_circ_mm, uint16_t *ppr_l, uint16_t *ppr_r);
bool agv_parse_tof_data(const agv_packet_t *pkt, uint16_t *left_mm, uint16_t *center_mm, uint16_t *right_mm);

#ifdef __cplusplus
}
#endif

#endif /* AGV_STM32_C_H */
