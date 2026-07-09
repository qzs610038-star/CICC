#ifndef MYCOBOT_PROTOCOL_H
#define MYCOBOT_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MYCOBOT_FRAME_HEADER      0xFEu
#define MYCOBOT_FRAME_FOOTER      0xFAu
#define MYCOBOT_FRAME_OVERHEAD    5u
#define MYCOBOT_MAX_PAYLOAD       64u

#define MYCOBOT_CMD_GET_ANGLES         0x20u
#define MYCOBOT_CMD_SEND_ANGLES        0x22u
#define MYCOBOT_CMD_GET_COORDS         0x23u
#define MYCOBOT_CMD_GET_GRIPPER_VALUE  0x65u
#define MYCOBOT_CMD_SET_GRIPPER_STATE  0x66u
#define MYCOBOT_CMD_SET_GRIPPER_VALUE  0x67u

#define MYCOBOT_JOINT_COUNT            6u
#define MYCOBOT_SPEED_MIN              1u
#define MYCOBOT_SPEED_MAX              100u
#define MYCOBOT_GRIPPER_STATE_OPEN     0u
#define MYCOBOT_GRIPPER_STATE_CLOSE    1u
#define MYCOBOT_GRIPPER_VALUE_MIN      0u
#define MYCOBOT_GRIPPER_VALUE_MAX      100u

typedef enum {
    MYCOBOT_PARSE_OK = 0,
    MYCOBOT_PARSE_INCOMPLETE,
    MYCOBOT_PARSE_BAD_HEADER,
    MYCOBOT_PARSE_BAD_LENGTH,
    MYCOBOT_PARSE_BAD_FOOTER,
    MYCOBOT_PARSE_PAYLOAD_TOO_LONG,
    MYCOBOT_PARSE_INVALID_ARG
} mycobot_parse_status_t;

typedef struct {
    uint8_t command;
    uint8_t payload_len;
    uint8_t payload[MYCOBOT_MAX_PAYLOAD];
} mycobot_frame_t;

typedef enum {
    MYCOBOT_HELPER_OK = 0,
    MYCOBOT_HELPER_INVALID_ARG,
    MYCOBOT_HELPER_BAD_LENGTH
} mycobot_helper_status_t;

uint8_t mycobot_build_frame_ex(uint8_t command,
                               const uint8_t *payload,
                               uint8_t payload_len,
                               uint8_t *out,
                               uint8_t out_cap);

uint8_t mycobot_build_frame(uint8_t command,
                            const uint8_t *payload,
                            uint8_t payload_len,
                            uint8_t *out);

mycobot_parse_status_t mycobot_parse_frame(const uint8_t *buf,
                                           uint16_t buf_len,
                                           mycobot_frame_t *frame,
                                           uint16_t *consumed_len);

uint8_t mycobot_encode_send_angles_payload(const int16_t joint_deg_x10[MYCOBOT_JOINT_COUNT],
                                           uint16_t speed,
                                           uint8_t *payload_out,
                                           uint8_t out_cap);

mycobot_helper_status_t mycobot_decode_get_angles_response(const uint8_t *payload,
                                                            uint8_t payload_len,
                                                            int16_t joint_deg_x10[MYCOBOT_JOINT_COUNT]);

uint8_t mycobot_encode_gripper_state_payload(uint8_t state,
                                              uint8_t speed,
                                              uint8_t *payload_out,
                                              uint8_t out_cap);

uint8_t mycobot_encode_gripper_value_payload(uint8_t value,
                                              uint8_t speed,
                                              uint8_t *payload_out,
                                              uint8_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* MYCOBOT_PROTOCOL_H */
