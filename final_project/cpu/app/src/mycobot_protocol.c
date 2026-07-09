#include "mycobot_protocol.h"

uint8_t mycobot_build_frame_ex(uint8_t command,
                               const uint8_t *payload,
                               uint8_t payload_len,
                               uint8_t *out,
                               uint8_t out_cap)
{
    uint8_t i;
    uint8_t total_len;

    if (!out) {
        return 0;
    }

    if (payload_len > MYCOBOT_MAX_PAYLOAD) {
        return 0;
    }

    if (payload_len && !payload) {
        return 0;
    }

    total_len = (uint8_t)(payload_len + MYCOBOT_FRAME_OVERHEAD);
    if (out_cap < total_len) {
        return 0;
    }

    out[0] = MYCOBOT_FRAME_HEADER;
    out[1] = MYCOBOT_FRAME_HEADER;
    out[2] = (uint8_t)(payload_len + 2u);
    out[3] = command;

    for (i = 0; i < payload_len; ++i) {
        out[4u + i] = payload[i];
    }
    out[4u + payload_len] = MYCOBOT_FRAME_FOOTER;

    return total_len;
}

uint8_t mycobot_build_frame(uint8_t command,
                            const uint8_t *payload,
                            uint8_t payload_len,
                            uint8_t *out)
{
    return mycobot_build_frame_ex(command, payload, payload_len, out, 0xFFu);
}

mycobot_parse_status_t mycobot_parse_frame(const uint8_t *buf,
                                           uint16_t buf_len,
                                           mycobot_frame_t *frame,
                                           uint16_t *consumed_len)
{
    uint8_t frame_len;
    uint8_t payload_len;
    uint16_t total_len;
    uint8_t i;

    if (!buf || !frame) {
        return MYCOBOT_PARSE_INVALID_ARG;
    }

    if (consumed_len) {
        *consumed_len = 0;
    }

    if (buf_len < 3u) {
        return MYCOBOT_PARSE_INCOMPLETE;
    }

    if (buf[0] != MYCOBOT_FRAME_HEADER || buf[1] != MYCOBOT_FRAME_HEADER) {
        return MYCOBOT_PARSE_BAD_HEADER;
    }

    frame_len = buf[2];
    if (frame_len < 2u) {
        return MYCOBOT_PARSE_BAD_LENGTH;
    }

    payload_len = (uint8_t)(frame_len - 2u);
    if (payload_len > MYCOBOT_MAX_PAYLOAD) {
        return MYCOBOT_PARSE_PAYLOAD_TOO_LONG;
    }

    total_len = (uint16_t)(payload_len + MYCOBOT_FRAME_OVERHEAD);
    if (buf_len < total_len) {
        return MYCOBOT_PARSE_INCOMPLETE;
    }

    if (buf[total_len - 1u] != MYCOBOT_FRAME_FOOTER) {
        return MYCOBOT_PARSE_BAD_FOOTER;
    }

    frame->command = buf[3];
    frame->payload_len = payload_len;
    for (i = 0; i < payload_len; ++i) {
        frame->payload[i] = buf[4u + i];
    }

    if (consumed_len) {
        *consumed_len = total_len;
    }

    return MYCOBOT_PARSE_OK;
}

uint8_t mycobot_encode_send_angles_payload(const int16_t joint_deg_x10[MYCOBOT_JOINT_COUNT],
                                           uint16_t speed,
                                           uint8_t *payload_out,
                                           uint8_t out_cap)
{
    uint8_t i;
    int32_t angle100;
    uint16_t u;

    if (!joint_deg_x10 || !payload_out) {
        return 0;
    }

    if (out_cap < 13u) {
        return 0;
    }

    if (speed < MYCOBOT_SPEED_MIN || speed > MYCOBOT_SPEED_MAX) {
        return 0;
    }

    for (i = 0; i < 6; ++i) {
        angle100 = (int32_t)joint_deg_x10[i] * 10;
        if (angle100 > 32767) {
            angle100 = 32767;
        }
        if (angle100 < -32768) {
            angle100 = -32768;
        }
        u = (uint16_t)(int16_t)angle100;
        payload_out[i * 2u] = (uint8_t)(u >> 8);
        payload_out[i * 2u + 1u] = (uint8_t)(u & 0xFFu);
    }

    payload_out[12] = (uint8_t)speed;
    return 13u;
}

mycobot_helper_status_t mycobot_decode_get_angles_response(const uint8_t *payload,
                                                            uint8_t payload_len,
                                                            int16_t joint_deg_x10[MYCOBOT_JOINT_COUNT])
{
    uint8_t i;
    int16_t angle100;

    if (!payload || !joint_deg_x10) {
        return MYCOBOT_HELPER_INVALID_ARG;
    }

    if (payload_len != 12u) {
        return MYCOBOT_HELPER_BAD_LENGTH;
    }

    for (i = 0; i < 6; ++i) {
        angle100 = (int16_t)(((uint16_t)payload[i * 2u] << 8) | payload[i * 2u + 1u]);
        joint_deg_x10[i] = angle100 / 10;
    }

    return MYCOBOT_HELPER_OK;
}

uint8_t mycobot_encode_gripper_state_payload(uint8_t state,
                                              uint8_t speed,
                                              uint8_t *payload_out,
                                              uint8_t out_cap)
{
    if (!payload_out) {
        return 0;
    }

    if (out_cap < 2u) {
        return 0;
    }

    if (state != MYCOBOT_GRIPPER_STATE_OPEN &&
        state != MYCOBOT_GRIPPER_STATE_CLOSE) {
        return 0;
    }

    if (speed < MYCOBOT_SPEED_MIN || speed > MYCOBOT_SPEED_MAX) {
        return 0;
    }

    payload_out[0] = state;
    payload_out[1] = speed;
    return 2u;
}

uint8_t mycobot_encode_gripper_value_payload(uint8_t value,
                                              uint8_t speed,
                                              uint8_t *payload_out,
                                              uint8_t out_cap)
{
    if (!payload_out) {
        return 0;
    }

    if (out_cap < 2u) {
        return 0;
    }

    if (value > MYCOBOT_GRIPPER_VALUE_MAX ||
        speed < MYCOBOT_SPEED_MIN ||
        speed > MYCOBOT_SPEED_MAX) {
        return 0;
    }

    payload_out[0] = value;
    payload_out[1] = speed;
    return 2u;
}
