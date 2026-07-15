#include "mycobot_protocol.h"

static const int16_t g_joint_min_deg_x10[MYCOBOT_JOINT_COUNT] = {
    -1680, -1350, -1500, -1450, -1650, -1800
};

static const int16_t g_joint_max_deg_x10[MYCOBOT_JOINT_COUNT] = {
    1680, 1350, 1500, 1450, 1650, 1800
};

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

uint8_t mycobot_wire_len_is_valid(uint8_t wire_len)
{
    return (uint8_t)(wire_len >= MYCOBOT_PROTOCOL_LEN_MIN &&
                     wire_len <= MYCOBOT_PROTOCOL_LEN_MAX);
}

uint8_t mycobot_command_expected_response_payload_len(uint8_t command,
                                                      uint8_t *payload_len_out)
{
    uint8_t payload_len;

    switch (command) {
    case MYCOBOT_CMD_GET_ANGLES:
        payload_len = 12u;
        break;
    case MYCOBOT_CMD_GET_GRIPPER_VALUE:
    case MYCOBOT_CMD_IS_MOVING:
    case MYCOBOT_CMD_IS_GRIPPER_MOVING:
        payload_len = 1u;
        break;
    default:
        return 0u;
    }

    if (payload_len_out) {
        *payload_len_out = payload_len;
    }
    return 1u;
}

uint8_t mycobot_command_has_reply(uint8_t command)
{
    return mycobot_command_expected_response_payload_len(command, 0);
}

uint8_t mycobot_joint_angles_within_limits(const int16_t joint_deg_x10[MYCOBOT_JOINT_COUNT])
{
    uint8_t i;

    if (!joint_deg_x10) {
        return 0u;
    }

    for (i = 0u; i < MYCOBOT_JOINT_COUNT; ++i) {
        if (joint_deg_x10[i] < g_joint_min_deg_x10[i] ||
            joint_deg_x10[i] > g_joint_max_deg_x10[i]) {
            return 0u;
        }
    }

    return 1u;
}

mycobot_helper_status_t mycobot_validate_response_payload(uint8_t command,
                                                           const uint8_t *payload,
                                                           uint8_t payload_len)
{
    int16_t angles[MYCOBOT_JOINT_COUNT];
    mycobot_helper_status_t decoded;
    uint8_t expected_len;

    if (!payload) {
        return MYCOBOT_HELPER_INVALID_ARG;
    }

    if (!mycobot_command_expected_response_payload_len(command, &expected_len) ||
        payload_len != expected_len) {
        return MYCOBOT_HELPER_BAD_LENGTH;
    }

    if (command == MYCOBOT_CMD_GET_ANGLES) {
        decoded = mycobot_decode_get_angles_response(payload, payload_len, angles);
        if (decoded != MYCOBOT_HELPER_OK) {
            return decoded;
        }
        return mycobot_joint_angles_within_limits(angles) ?
            MYCOBOT_HELPER_OK : MYCOBOT_HELPER_OUT_OF_RANGE;
    }

    if ((command == MYCOBOT_CMD_GET_GRIPPER_VALUE && payload[0] > MYCOBOT_GRIPPER_VALUE_MAX) ||
        ((command == MYCOBOT_CMD_IS_MOVING || command == MYCOBOT_CMD_IS_GRIPPER_MOVING) &&
         payload[0] > 1u)) {
        return MYCOBOT_HELPER_OUT_OF_RANGE;
    }

    return MYCOBOT_HELPER_OK;
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

    if (!mycobot_joint_angles_within_limits(joint_deg_x10)) {
        return 0;
    }

    for (i = 0; i < MYCOBOT_JOINT_COUNT; ++i) {
        angle100 = (int32_t)joint_deg_x10[i] * 10;
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
