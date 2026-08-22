#include "agv_stm32_c.h"
#include <string.h>

/* ============================================================================
   AGV UART PROTOCOL SPECIFICATION - PURE C IMPLEMENTATION FOR STM32F103
   ============================================================================ */

uint16_t agv_crc16_calc(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

uint16_t agv_encode_packet(const agv_packet_t *pkt, uint8_t *out_buf, uint16_t max_len) {
    if (pkt == NULL || out_buf == NULL || pkt->length > AGV_MAX_PAYLOAD_SIZE) {
        return 0;
    }

    uint16_t required_len = 2 + 1 + 1 + 2 + pkt->length + 2;
    if (max_len < required_len) {
        return 0;
    }

    uint16_t idx = 0;
    out_buf[idx++] = AGV_SOF1;
    out_buf[idx++] = AGV_SOF2;
    out_buf[idx++] = pkt->cmd;
    out_buf[idx++] = pkt->seq;
    out_buf[idx++] = (uint8_t)(pkt->length & 0xFF);
    out_buf[idx++] = (uint8_t)((pkt->length >> 8) & 0xFF);

    for (uint16_t i = 0; i < pkt->length; i++) {
        out_buf[idx++] = pkt->payload[i];
    }

    uint16_t crc_calc_len = 4 + pkt->length;
    uint16_t crc = agv_crc16_calc(&out_buf[2], crc_calc_len);

    out_buf[idx++] = (uint8_t)(crc & 0xFF);
    out_buf[idx++] = (uint8_t)((crc >> 8) & 0xFF);

    return idx;
}

bool agv_decode_packet(const uint8_t *in_buf, uint16_t buf_len, agv_packet_t *pkt) {
    if (in_buf == NULL || pkt == NULL || buf_len < 8) {
        return false;
    }

    if (in_buf[0] != AGV_SOF1 || in_buf[1] != AGV_SOF2) {
        return false;
    }

    pkt->cmd = in_buf[2];
    pkt->seq = in_buf[3];
    pkt->length = (uint16_t)in_buf[4] | ((uint16_t)in_buf[5] << 8);

    if (pkt->length > AGV_MAX_PAYLOAD_SIZE) {
        return false;
    }

    uint16_t expected_total = 2 + 1 + 1 + 2 + pkt->length + 2;
    if (buf_len < expected_total) {
        return false;
    }

    for (uint16_t i = 0; i < pkt->length; i++) {
        pkt->payload[i] = in_buf[6 + i];
    }

    uint16_t crc_offset = 6 + pkt->length;
    pkt->crc = (uint16_t)in_buf[crc_offset] | ((uint16_t)in_buf[crc_offset + 1] << 8);

    uint16_t calculated = agv_crc16_calc(&in_buf[2], 4 + pkt->length);
    return (calculated == pkt->crc);
}

void agv_stm32_parser_init(agv_stm32_parser_t *parser) {
    if (parser == NULL) return;
    parser->state = AGV_PARSE_SOF1;
    parser->rx_idx = 0;
    parser->expected_len = 0;
    memset(&parser->rx_pkt, 0, sizeof(agv_packet_t));
}

bool agv_stm32_parser_feed(agv_stm32_parser_t *parser, uint8_t b) {
    if (parser == NULL) return false;

    switch (parser->state) {
        case AGV_PARSE_SOF1:
            if (b == AGV_SOF1) {
                parser->rx_idx = 0;
                parser->rx_buf[parser->rx_idx++] = b;
                parser->state = AGV_PARSE_SOF2;
            }
            break;

        case AGV_PARSE_SOF2:
            if (b == AGV_SOF2) {
                parser->rx_buf[parser->rx_idx++] = b;
                parser->state = AGV_PARSE_CMD;
            } else {
                parser->state = AGV_PARSE_SOF1;
            }
            break;

        case AGV_PARSE_CMD:
            parser->rx_buf[parser->rx_idx++] = b;
            parser->rx_pkt.cmd = b;
            parser->state = AGV_PARSE_SEQ;
            break;

        case AGV_PARSE_SEQ:
            parser->rx_buf[parser->rx_idx++] = b;
            parser->rx_pkt.seq = b;
            parser->state = AGV_PARSE_LEN_L;
            break;

        case AGV_PARSE_LEN_L:
            parser->rx_buf[parser->rx_idx++] = b;
            parser->expected_len = b;
            parser->state = AGV_PARSE_LEN_H;
            break;

        case AGV_PARSE_LEN_H:
            parser->rx_buf[parser->rx_idx++] = b;
            parser->expected_len |= ((uint16_t)b << 8);
            parser->rx_pkt.length = parser->expected_len;

            if (parser->expected_len > AGV_MAX_PAYLOAD_SIZE) {
                parser->state = AGV_PARSE_SOF1;
                return false;
            }

            if (parser->expected_len == 0) {
                parser->state = AGV_PARSE_CRC_L;
            } else {
                parser->state = AGV_PARSE_PAYLOAD;
            }
            break;

        case AGV_PARSE_PAYLOAD:
            parser->rx_buf[parser->rx_idx++] = b;
            parser->rx_pkt.payload[parser->rx_idx - 6] = b;
            if ((parser->rx_idx - 6) >= parser->expected_len) {
                parser->state = AGV_PARSE_CRC_L;
            }
            break;

        case AGV_PARSE_CRC_L:
            parser->rx_buf[parser->rx_idx++] = b;
            parser->state = AGV_PARSE_CRC_H;
            break;

        case AGV_PARSE_CRC_H:
            parser->rx_buf[parser->rx_idx++] = b;
            parser->rx_pkt.crc = (uint16_t)parser->rx_buf[parser->rx_idx - 2] | ((uint16_t)b << 8);
            parser->state = AGV_PARSE_SOF1;

            return agv_decode_packet(parser->rx_buf, parser->rx_idx, &parser->rx_pkt);
    }

    return false;
}

uint16_t agv_build_ack(uint8_t ack_cmd, uint8_t result, uint8_t seq, uint8_t *out_buf, uint16_t max_len) {
    agv_packet_t pkt;
    pkt.cmd = AGV_CMD_ACK;
    pkt.seq = seq;
    pkt.length = 2;
    pkt.payload[0] = ack_cmd;
    pkt.payload[1] = result;
    return agv_encode_packet(&pkt, out_buf, max_len);
}

uint16_t agv_build_status(uint8_t state_val, uint8_t error_flags, uint8_t seq, uint8_t *out_buf, uint16_t max_len) {
    agv_packet_t pkt;
    pkt.cmd = AGV_CMD_STATUS;
    pkt.seq = seq;
    pkt.length = 2;
    pkt.payload[0] = state_val;
    pkt.payload[1] = error_flags;
    return agv_encode_packet(&pkt, out_buf, max_len);
}

uint16_t agv_build_version(uint8_t proto_maj, uint8_t proto_min, uint8_t fw_maj, uint8_t fw_min, uint8_t fw_patch, uint8_t seq, uint8_t *out_buf, uint16_t max_len) {
    agv_packet_t pkt;
    pkt.cmd = AGV_CMD_VERSION;
    pkt.seq = seq;
    pkt.length = 5;
    pkt.payload[0] = proto_maj;
    pkt.payload[1] = proto_min;
    pkt.payload[2] = fw_maj;
    pkt.payload[3] = fw_min;
    pkt.payload[4] = fw_patch;
    return agv_encode_packet(&pkt, out_buf, max_len);
}

uint16_t agv_build_error(uint16_t error_code, uint8_t severity, uint8_t source, uint8_t seq, uint8_t *out_buf, uint16_t max_len) {
    agv_packet_t pkt;
    pkt.cmd = AGV_CMD_ERROR;
    pkt.seq = seq;
    pkt.length = 4;
    pkt.payload[0] = (uint8_t)(error_code & 0xFF);
    pkt.payload[1] = (uint8_t)((error_code >> 8) & 0xFF);
    pkt.payload[2] = severity;
    pkt.payload[3] = source;
    return agv_encode_packet(&pkt, out_buf, max_len);
}

uint16_t agv_build_odometry(int32_t left_mm, int32_t right_mm, int16_t yaw_deg_x10, uint8_t seq, uint8_t *out_buf, uint16_t max_len) {
    agv_packet_t pkt;
    pkt.cmd = AGV_CMD_ODOMETRY;
    pkt.seq = seq;
    pkt.length = 10;
    pkt.payload[0] = (uint8_t)(left_mm & 0xFF);
    pkt.payload[1] = (uint8_t)((left_mm >> 8) & 0xFF);
    pkt.payload[2] = (uint8_t)((left_mm >> 16) & 0xFF);
    pkt.payload[3] = (uint8_t)((left_mm >> 24) & 0xFF);

    pkt.payload[4] = (uint8_t)(right_mm & 0xFF);
    pkt.payload[5] = (uint8_t)((right_mm >> 8) & 0xFF);
    pkt.payload[6] = (uint8_t)((right_mm >> 16) & 0xFF);
    pkt.payload[7] = (uint8_t)((right_mm >> 24) & 0xFF);

    pkt.payload[8] = (uint8_t)(yaw_deg_x10 & 0xFF);
    pkt.payload[9] = (uint8_t)((yaw_deg_x10 >> 8) & 0xFF);
    return agv_encode_packet(&pkt, out_buf, max_len);
}

uint16_t agv_build_encoder_data(int32_t left_pulses, int32_t right_pulses, uint8_t seq, uint8_t *out_buf, uint16_t max_len) {
    agv_packet_t pkt;
    pkt.cmd = AGV_CMD_ENCODER_DATA;
    pkt.seq = seq;
    pkt.length = 8;
    pkt.payload[0] = (uint8_t)(left_pulses & 0xFF);
    pkt.payload[1] = (uint8_t)((left_pulses >> 8) & 0xFF);
    pkt.payload[2] = (uint8_t)((left_pulses >> 16) & 0xFF);
    pkt.payload[3] = (uint8_t)((left_pulses >> 24) & 0xFF);

    pkt.payload[4] = (uint8_t)(right_pulses & 0xFF);
    pkt.payload[5] = (uint8_t)((right_pulses >> 8) & 0xFF);
    pkt.payload[6] = (uint8_t)((right_pulses >> 16) & 0xFF);
    pkt.payload[7] = (uint8_t)((right_pulses >> 24) & 0xFF);

    return agv_encode_packet(&pkt, out_buf, max_len);
}

uint16_t agv_build_imu_data(int16_t ax, int16_t ay, int16_t az, int16_t gx, int16_t gy, int16_t gz, int16_t yaw_deg_x10, uint8_t seq, uint8_t *out_buf, uint16_t max_len) {
    agv_packet_t pkt;
    pkt.cmd = AGV_CMD_IMU_DATA;
    pkt.seq = seq;
    pkt.length = 14;
    pkt.payload[0] = (uint8_t)(ax & 0xFF); pkt.payload[1] = (uint8_t)((ax >> 8) & 0xFF);
    pkt.payload[2] = (uint8_t)(ay & 0xFF); pkt.payload[3] = (uint8_t)((ay >> 8) & 0xFF);
    pkt.payload[4] = (uint8_t)(az & 0xFF); pkt.payload[5] = (uint8_t)((az >> 8) & 0xFF);
    pkt.payload[6] = (uint8_t)(gx & 0xFF); pkt.payload[7] = (uint8_t)((gx >> 8) & 0xFF);
    pkt.payload[8] = (uint8_t)(gy & 0xFF); pkt.payload[9] = (uint8_t)((gy >> 8) & 0xFF);
    pkt.payload[10] = (uint8_t)(gz & 0xFF); pkt.payload[11] = (uint8_t)((gz >> 8) & 0xFF);
    pkt.payload[12] = (uint8_t)(yaw_deg_x10 & 0xFF); pkt.payload[13] = (uint8_t)((yaw_deg_x10 >> 8) & 0xFF);
    return agv_encode_packet(&pkt, out_buf, max_len);
}

uint16_t agv_build_tof_data(uint16_t left_mm, uint16_t center_mm, uint16_t right_mm, uint8_t seq, uint8_t *out_buf, uint16_t max_len) {
    agv_packet_t pkt;
    pkt.cmd = AGV_CMD_TOF_DATA;
    pkt.seq = seq;
    pkt.length = 6;
    pkt.payload[0] = (uint8_t)(left_mm & 0xFF); pkt.payload[1] = (uint8_t)((left_mm >> 8) & 0xFF);
    pkt.payload[2] = (uint8_t)(center_mm & 0xFF); pkt.payload[3] = (uint8_t)((center_mm >> 8) & 0xFF);
    pkt.payload[4] = (uint8_t)(right_mm & 0xFF); pkt.payload[5] = (uint8_t)((right_mm >> 8) & 0xFF);
    return agv_encode_packet(&pkt, out_buf, max_len);
}

uint16_t agv_build_motor_status(int16_t left_pwm, int16_t right_pwm, int8_t left_dir, int8_t right_dir, uint8_t seq, uint8_t *out_buf, uint16_t max_len) {
    agv_packet_t pkt;
    pkt.cmd = AGV_CMD_MOTOR_STATUS;
    pkt.seq = seq;
    pkt.length = 6;
    pkt.payload[0] = (uint8_t)(left_pwm & 0xFF);
    pkt.payload[1] = (uint8_t)((left_pwm >> 8) & 0xFF);
    pkt.payload[2] = (uint8_t)(right_pwm & 0xFF);
    pkt.payload[3] = (uint8_t)((right_pwm >> 8) & 0xFF);
    pkt.payload[4] = (uint8_t)left_dir;
    pkt.payload[5] = (uint8_t)right_dir;
    return agv_encode_packet(&pkt, out_buf, max_len);
}

uint16_t agv_build_move_done(uint8_t orig_seq, uint8_t result, uint8_t seq, uint8_t *out_buf, uint16_t max_len) {
    agv_packet_t pkt;
    pkt.cmd = AGV_CMD_MOVE_DONE;
    pkt.seq = seq;
    pkt.length = 2;
    pkt.payload[0] = orig_seq;
    pkt.payload[1] = result;
    return agv_encode_packet(&pkt, out_buf, max_len);
}

uint16_t agv_build_turn_done(uint8_t orig_seq, uint8_t result, uint8_t seq, uint8_t *out_buf, uint16_t max_len) {
    agv_packet_t pkt;
    pkt.cmd = AGV_CMD_TURN_DONE;
    pkt.seq = seq;
    pkt.length = 2;
    pkt.payload[0] = orig_seq;
    pkt.payload[1] = result;
    return agv_encode_packet(&pkt, out_buf, max_len);
}

bool agv_parse_move(const agv_packet_t *pkt, int32_t *distance_mm, uint16_t *speed_mm_s) {
    if (pkt == NULL || (pkt->cmd != AGV_CMD_MOVE && pkt->cmd != AGV_CMD_STEP_MOVE) || pkt->length < 6) return false;
    if (distance_mm) *distance_mm = (int32_t)(pkt->payload[0] | (pkt->payload[1] << 8) | (pkt->payload[2] << 16) | (pkt->payload[3] << 24));
    if (speed_mm_s) *speed_mm_s = (uint16_t)(pkt->payload[4] | (pkt->payload[5] << 8));
    return true;
}

bool agv_parse_turn(const agv_packet_t *pkt, int16_t *angle_deg_x10, uint16_t *speed_deg_s) {
    if (pkt == NULL || (pkt->cmd != AGV_CMD_TURN && pkt->cmd != AGV_CMD_STEP_TURN) || pkt->length < 4) return false;
    if (angle_deg_x10) *angle_deg_x10 = (int16_t)(pkt->payload[0] | (pkt->payload[1] << 8));
    if (speed_deg_s) *speed_deg_s = (uint16_t)(pkt->payload[2] | (pkt->payload[3] << 8));
    return true;
}

bool agv_parse_set_repeat_speed(const agv_packet_t *pkt, uint16_t *speed_percent) {
    if (pkt == NULL || pkt->cmd != AGV_CMD_SET_REPEAT_SPEED || pkt->length < 2) return false;
    if (speed_percent) *speed_percent = (uint16_t)(pkt->payload[0] | (pkt->payload[1] << 8));
    return true;
}

bool agv_parse_set_speeds(const agv_packet_t *pkt, int16_t *leftSpeedMmS, int16_t *rightSpeedMmS) {
    if (pkt == NULL || pkt->cmd != AGV_CMD_SET_SPEEDS || pkt->length < 4) return false;
    if (leftSpeedMmS) *leftSpeedMmS = (int16_t)(pkt->payload[0] | (pkt->payload[1] << 8));
    if (rightSpeedMmS) *rightSpeedMmS = (int16_t)(pkt->payload[2] | (pkt->payload[3] << 8));
    return true;
}

bool agv_parse_motor_trim(const agv_packet_t *pkt, uint8_t *lFwd, uint8_t *rFwd, uint8_t *lTurn, uint8_t *rTurn) {
    if (pkt == NULL || pkt->cmd != AGV_CMD_SET_MOTOR_TRIM || pkt->length < 4) return false;
    if (lFwd) *lFwd = pkt->payload[0];
    if (rFwd) *rFwd = pkt->payload[1];
    if (lTurn) *lTurn = pkt->payload[2];
    if (rTurn) *rTurn = pkt->payload[3];
    return true;
}

bool agv_parse_segment_start(const agv_packet_t *pkt, uint16_t *segment_id, uint16_t *src_rfid_id, uint16_t *dst_rfid_id) {
    if (pkt == NULL || pkt->cmd != AGV_CMD_SEGMENT_START || pkt->length < 6) return false;
    if (segment_id) *segment_id = (uint16_t)(pkt->payload[0] | (pkt->payload[1] << 8));
    if (src_rfid_id) *src_rfid_id = (uint16_t)(pkt->payload[2] | (pkt->payload[3] << 8));
    if (dst_rfid_id) *dst_rfid_id = (uint16_t)(pkt->payload[4] | (pkt->payload[5] << 8));
    return true;
}

bool agv_parse_segment_complete(const agv_packet_t *pkt, uint16_t *segment_id) {
    if (pkt == NULL || pkt->cmd != AGV_CMD_SEGMENT_COMPLETE || pkt->length < 2) return false;
    if (segment_id) *segment_id = (uint16_t)(pkt->payload[0] | (pkt->payload[1] << 8));
    return true;
}

bool agv_parse_set_pid_tuning(const agv_packet_t *pkt, float *kpL, float *kiL, float *kdL, float *kpR, float *kiR, float *kdR) {
    if (pkt == NULL || pkt->cmd != AGV_CMD_SET_PID_TUNING || pkt->length < 24) return false;
    if (kpL) memcpy(kpL, &pkt->payload[0], 4);
    if (kiL) memcpy(kiL, &pkt->payload[4], 4);
    if (kdL) memcpy(kdL, &pkt->payload[8], 4);
    if (kpR) memcpy(kpR, &pkt->payload[12], 4);
    if (kiR) memcpy(kiR, &pkt->payload[16], 4);
    if (kdR) memcpy(kdR, &pkt->payload[20], 4);
    return true;
}

bool agv_parse_set_encoder_config(const agv_packet_t *pkt, float *wheel_circ_mm, uint16_t *ppr_l, uint16_t *ppr_r) {
    if (pkt == NULL || pkt->cmd != AGV_CMD_SET_ENCODER_CONFIG || pkt->length < 8) return false;
    if (wheel_circ_mm) memcpy(wheel_circ_mm, &pkt->payload[0], 4);
    if (ppr_l) *ppr_l = (uint16_t)(pkt->payload[4] | (pkt->payload[5] << 8));
    if (ppr_r) *ppr_r = (uint16_t)(pkt->payload[6] | (pkt->payload[7] << 8));
    return true;
}

bool agv_parse_tof_data(const agv_packet_t *pkt, uint16_t *left_mm, uint16_t *center_mm, uint16_t *right_mm) {
    if (pkt == NULL || pkt->cmd != AGV_CMD_TOF_DATA || pkt->length < 6) return false;
    if (left_mm) *left_mm = (uint16_t)(pkt->payload[0] | (pkt->payload[1] << 8));
    if (center_mm) *center_mm = (uint16_t)(pkt->payload[2] | (pkt->payload[3] << 8));
    if (right_mm) *right_mm = (uint16_t)(pkt->payload[4] | (pkt->payload[5] << 8));
    return true;
}
