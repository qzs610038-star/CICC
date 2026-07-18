#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "arm_controller.h"
#include "arm_positions.h"
#include "mycobot_protocol.h"
#include "mycobot_transport.h"
#include "mycobot_transaction.h"

typedef struct {
    int16_t target[ARM_CONTROLLER_JOINTS];
    uint16_t speed;
    uint16_t gripper;
    uint8_t send_count;
    uint8_t read_fail;
    uint8_t gripper_count;
    uint8_t reads_before_converge;
    uint8_t read_count;
    uint8_t read_fail_at;
    uint8_t read_fail_at_enable;
    int16_t read_offset[ARM_CONTROLLER_JOINTS];
    uint16_t last_send_speed;
    uint16_t retry_send_speed;
    uint8_t capture_retry_send_speed;
} mock_arm_t;

static int mock_send_angles(void *user,
                            const int16_t joint_deg_x10[ARM_CONTROLLER_JOINTS],
                            uint16_t speed)
{
    mock_arm_t *mock = (mock_arm_t *)user;
    memcpy(mock->target, joint_deg_x10, sizeof(mock->target));
    mock->speed = speed;
    mock->last_send_speed = speed;
    if (mock->capture_retry_send_speed != 0u) {
        mock->retry_send_speed = speed;
        mock->capture_retry_send_speed = 0u;
    }
    mock->send_count++;
    return 0;
}

static int mock_read_angles(void *user,
                            int16_t joint_deg_x10[ARM_CONTROLLER_JOINTS])
{
    mock_arm_t *mock = (mock_arm_t *)user;
    uint8_t i;
    if (mock->read_fail) {
        return -1;
    }
    if (mock->read_fail_at_enable && (mock->read_count == mock->read_fail_at)) {
        mock->read_count++;
        return -1;
    }
    memcpy(joint_deg_x10, mock->target, sizeof(mock->target));
    if (mock->read_count < mock->reads_before_converge) {
        for (i = 0; i < ARM_CONTROLLER_JOINTS; ++i) {
            joint_deg_x10[i] += mock->read_offset[i];
        }
    }
    mock->read_count++;
    return 0;
}

static int mock_set_gripper(void *user, uint16_t gripper, uint16_t speed)
{
    mock_arm_t *mock = (mock_arm_t *)user;
    (void)speed;
    mock->gripper = gripper;
    mock->gripper_count++;
    return 0;
}

static void fill_point(arm_controller_point_t *p,
                       int16_t j1, int16_t j2, int16_t j3,
                       int16_t j4, int16_t j5, int16_t j6,
                       uint16_t speed,
                       uint16_t radius)
{
    p->joint_deg_x10[0] = j1;
    p->joint_deg_x10[1] = j2;
    p->joint_deg_x10[2] = j3;
    p->joint_deg_x10[3] = j4;
    p->joint_deg_x10[4] = j5;
    p->joint_deg_x10[5] = j6;
    p->speed = speed;
    p->radius_mm_x10 = radius;
}

static arm_controller_plan_t make_valid_plan(void)
{
    arm_controller_plan_t p;
    memset(&p, 0, sizeof(p));

    fill_point(&p.points[ARM_POINT_HOME], 0, 0, 0, 0, 0, 0, 30, 0);
    fill_point(&p.points[ARM_POINT_HOME_READY], 0, -240, -80, 20, 0, 390, 20, 1900);
    fill_point(&p.points[ARM_POINT_PICK_HOVER], 400, -360, -690, 160, 10, 940, 16, 2420);
    fill_point(&p.points[ARM_POINT_PICK], 400, -590, -690, 360, -10, 930, 12, 2520);
    fill_point(&p.points[ARM_POINT_DROP_HOVER], 10, -400, -690, 210, -60, 540, 16, 2480);
    fill_point(&p.points[ARM_POINT_DROP], 10, -600, -690, 410, -40, 510, 12, 2550);

    p.gripper_open = 80;
    p.gripper_closed = 20;
    p.gripper_speed = 50;
    p.short_tol_deg_x10 = 30;
    p.home_tol_deg_x10 = 15;
    p.confirm_required = 2;
    p.poll_interval_ms = 50;
    p.move_timeout_ms = 4000;
    p.home_timeout_ms = 1500;
    p.r_max_mm_x10 = 2800;
    p.short_arm_joint_max_delta_x10 = 300;
    p.return_arm_joint_max_delta_x10 = 900;
    p.home_ready_arm_max_diff_x10 = 450;
    return p;
}

static void test_frame_build_and_parse(void)
{
    uint8_t payload[3] = {1u, 2u, 3u};
    uint8_t out[8] = {0};
    mycobot_frame_t frame;
    uint16_t consumed = 0;
    uint8_t n;

    n = mycobot_build_frame_ex(0x22u, payload, 3u, out, sizeof(out));
    assert(n == 8u);
    assert(out[0] == 0xFEu);
    assert(out[1] == 0xFEu);
    assert(out[2] == 5u);
    assert(out[3] == 0x22u);
    assert(out[4] == 1u && out[5] == 2u && out[6] == 3u);
    assert(out[7] == 0xFAu);

    assert(mycobot_parse_frame(out, n, &frame, &consumed) == MYCOBOT_PARSE_OK);
    assert(consumed == n);
    assert(frame.command == 0x22u);
    assert(frame.payload_len == 3u);
    assert(frame.payload[0] == 1u && frame.payload[2] == 3u);

    assert(mycobot_build_frame_ex(0x22u, payload, 3u, out, 6u) == 0u);
    assert(mycobot_parse_frame(out, 4u, &frame, &consumed) ==
           MYCOBOT_PARSE_INCOMPLETE);
    out[0] = 0xFEu;
    out[7] = 0x00u;
    assert(mycobot_parse_frame(out, n, &frame, &consumed) ==
           MYCOBOT_PARSE_BAD_FOOTER);
    out[7] = 0xFAu;
    out[0] = 0x00u;
    assert(mycobot_parse_frame(out, n, &frame, &consumed) ==
           MYCOBOT_PARSE_BAD_HEADER);
}

static void test_protocol_truth_table_and_joint_limits(void)
{
    int16_t angles[MYCOBOT_JOINT_COUNT] = {0, 0, 0, 0, 0, 0};
    uint8_t payload_len = 0u;
    uint8_t joint;
    const int16_t min_limits[MYCOBOT_JOINT_COUNT] = {
        -1680, -1350, -1500, -1450, -1650, -1800
    };
    const int16_t max_limits[MYCOBOT_JOINT_COUNT] = {
        1680, 1350, 1500, 1450, 1650, 1800
    };

    assert(mycobot_wire_len_is_valid(0x02u) == 1u);
    assert(mycobot_wire_len_is_valid(0x10u) == 1u);
    assert(mycobot_wire_len_is_valid(0x01u) == 0u);
    assert(mycobot_wire_len_is_valid(0x11u) == 0u);

    assert(mycobot_command_expected_response_payload_len(MYCOBOT_CMD_GET_ANGLES,
                                                          &payload_len) == 1u);
    assert(payload_len == 12u);
    assert(mycobot_command_expected_response_payload_len(MYCOBOT_CMD_GET_GRIPPER_VALUE,
                                                          &payload_len) == 1u);
    assert(payload_len == 1u);
    assert(mycobot_command_expected_response_payload_len(MYCOBOT_CMD_IS_MOVING,
                                                          &payload_len) == 1u);
    assert(payload_len == 1u);
    assert(mycobot_command_expected_response_payload_len(MYCOBOT_CMD_IS_GRIPPER_MOVING,
                                                          &payload_len) == 1u);
    assert(payload_len == 1u);
    assert(mycobot_command_has_reply(MYCOBOT_CMD_STOP) == 0u);
    assert(mycobot_command_has_reply(MYCOBOT_CMD_SEND_ANGLES) == 0u);
    assert(mycobot_command_has_reply(MYCOBOT_CMD_SET_GRIPPER_STATE) == 0u);
    assert(mycobot_command_has_reply(MYCOBOT_CMD_SET_GRIPPER_VALUE) == 0u);

    for (joint = 0u; joint < MYCOBOT_JOINT_COUNT; ++joint) {
        angles[joint] = min_limits[joint];
        assert(mycobot_joint_angles_within_limits(angles) == 1u);
        angles[joint] = max_limits[joint];
        assert(mycobot_joint_angles_within_limits(angles) == 1u);
        angles[joint] = (int16_t)(max_limits[joint] + 1);
        assert(mycobot_joint_angles_within_limits(angles) == 0u);
        angles[joint] = 0;
    }
}

static void test_get_angles_official_vectors_and_transaction(void)
{
    const uint8_t expected_request[] = {0xFEu, 0xFEu, 0x02u, 0x20u, 0xFAu};
    uint8_t request[MYCOBOT_FRAME_OVERHEAD];
    uint8_t response[17];
    uint8_t valid_payload[12] = {
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    uint8_t n;
    uint16_t consumed = 0u;
    mycobot_frame_t frame;
    mycobot_transaction_t transaction;
    const mycobot_transaction_counters_t *counters;

    n = mycobot_build_frame_ex(MYCOBOT_CMD_GET_ANGLES, 0, 0u,
                               request, sizeof(request));
    assert(n == sizeof(expected_request));
    assert(memcmp(request, expected_request, sizeof(expected_request)) == 0);

    n = mycobot_build_frame_ex(MYCOBOT_CMD_GET_ANGLES, valid_payload,
                               sizeof(valid_payload), response, sizeof(response));
    assert(n == sizeof(response));
    assert(response[2] == 0x0Eu);
    assert(mycobot_parse_frame(response, n, &frame, &consumed) == MYCOBOT_PARSE_OK);
    assert(consumed == n);
    assert(frame.command == MYCOBOT_CMD_GET_ANGLES);
    assert(frame.payload_len == 12u);
    assert(mycobot_validate_response_payload(frame.command, frame.payload,
                                             frame.payload_len) == MYCOBOT_HELPER_OK);

    mycobot_transaction_init(&transaction);
    assert(mycobot_transaction_begin(&transaction, MYCOBOT_CMD_GET_ANGLES, 100u) == 1u);
    assert(mycobot_transaction_begin(&transaction, MYCOBOT_CMD_GET_ANGLES, 101u) == 0u);

    frame.command = MYCOBOT_CMD_GET_GRIPPER_VALUE;
    frame.payload_len = 12u;
    memset(frame.payload, 0, 12u);
    assert(mycobot_transaction_accept_frame(&transaction, &frame, 102u) ==
           MYCOBOT_TRANSACTION_BAD_COMMAND);

    frame.command = MYCOBOT_CMD_GET_ANGLES;
    frame.payload_len = 11u;
    assert(mycobot_transaction_accept_frame(&transaction, &frame, 103u) ==
           MYCOBOT_TRANSACTION_BAD_LENGTH);

    frame.payload_len = 12u;
    memset(frame.payload, 0, 12u);
    frame.payload[0] = 0x41u;
    frame.payload[1] = 0xFAu; /* J1 = 1690 deg_x10, outside +1680. */
    assert(mycobot_transaction_accept_frame(&transaction, &frame, 104u) ==
           MYCOBOT_TRANSACTION_BAD_DOMAIN);

    memset(frame.payload, 0, 12u);
    assert(mycobot_transaction_accept_frame(&transaction, &frame, 105u) ==
           MYCOBOT_TRANSACTION_ACCEPTED);
    assert(mycobot_transaction_active(&transaction) == 0u);
    assert(mycobot_transaction_accept_frame(&transaction, &frame, 106u) ==
           MYCOBOT_TRANSACTION_IDLE);

    assert(mycobot_transaction_begin(&transaction, MYCOBOT_CMD_GET_ANGLES, 200u) == 1u);
    assert(mycobot_transaction_expire(&transaction, 950u) == 1u);
    assert(mycobot_transaction_active(&transaction) == 0u);

    counters = mycobot_transaction_get_counters(&transaction);
    assert(counters != 0);
    assert(counters->accepted == 1u);
    assert(counters->bad_command == 1u);
    assert(counters->bad_length == 1u);
    assert(counters->bad_domain == 1u);
    assert(counters->late_or_duplicate == 1u);
    assert(counters->timeout == 1u);
    assert(counters->single_flight_reject == 1u);
}

static void test_mycobot_transaction_timeout_boundary(void)
{
    mycobot_transaction_t transaction;
    const mycobot_transaction_counters_t *counters;

    mycobot_transaction_init(&transaction);
    assert(mycobot_transaction_begin(&transaction, MYCOBOT_CMD_GET_ANGLES, 100u) == 1u);
    assert(mycobot_transaction_expire(&transaction, 849u) == 0u); /* 749 ms */
    assert(mycobot_transaction_active(&transaction) == 1u);
    assert(mycobot_transaction_expire(&transaction, 850u) == 1u); /* 750 ms */
    assert(mycobot_transaction_active(&transaction) == 0u);

    counters = mycobot_transaction_get_counters(&transaction);
    assert(counters != 0);
    assert(counters->timeout == 1u);
}

static void test_mycobot_transaction_timeout_wraparound(void)
{
    mycobot_transaction_t transaction;
    const mycobot_transaction_counters_t *counters;
    const uint32_t start_ms = UINT32_MAX - 749u;

    mycobot_transaction_init(&transaction);
    assert(mycobot_transaction_begin(&transaction, MYCOBOT_CMD_GET_ANGLES, start_ms) == 1u);
    assert(transaction.deadline_ms == 0u);
    assert(mycobot_transaction_expire(&transaction, UINT32_MAX) == 0u);
    assert(mycobot_transaction_active(&transaction) == 1u);
    assert(mycobot_transaction_expire(&transaction, 0u) == 1u);
    assert(mycobot_transaction_active(&transaction) == 0u);

    counters = mycobot_transaction_get_counters(&transaction);
    assert(counters != 0);
    assert(counters->timeout == 1u);
}

static void test_mycobot_transaction_bad_frames_remain_in_flight(void)
{
    mycobot_frame_t frame;
    mycobot_transaction_t transaction;
    const mycobot_transaction_counters_t *counters;

    memset(&frame, 0, sizeof(frame));
    mycobot_transaction_init(&transaction);
    assert(mycobot_transaction_begin(&transaction, MYCOBOT_CMD_GET_ANGLES, 100u) == 1u);

    frame.command = MYCOBOT_CMD_GET_GRIPPER_VALUE;
    frame.payload_len = 12u;
    assert(mycobot_transaction_accept_frame(&transaction, &frame, 101u) ==
           MYCOBOT_TRANSACTION_BAD_COMMAND);
    assert(mycobot_transaction_active(&transaction) == 1u);

    frame.command = MYCOBOT_CMD_GET_ANGLES;
    frame.payload_len = 11u;
    assert(mycobot_transaction_accept_frame(&transaction, &frame, 102u) ==
           MYCOBOT_TRANSACTION_BAD_LENGTH);
    assert(mycobot_transaction_active(&transaction) == 1u);

    frame.payload_len = 12u;
    frame.payload[0] = 0x41u;
    frame.payload[1] = 0xFAu; /* J1 = 1690 deg_x10, outside +1680. */
    assert(mycobot_transaction_accept_frame(&transaction, &frame, 103u) ==
           MYCOBOT_TRANSACTION_BAD_DOMAIN);
    assert(mycobot_transaction_active(&transaction) == 1u);

    counters = mycobot_transaction_get_counters(&transaction);
    assert(counters != 0);
    assert(counters->bad_command == 1u);
    assert(counters->bad_length == 1u);
    assert(counters->bad_domain == 1u);
    assert(counters->accepted == 0u);
}

static void test_mycobot_transaction_first_valid_response_wins(void)
{
    mycobot_frame_t frame;
    mycobot_transaction_t transaction;
    const mycobot_transaction_counters_t *counters;

    memset(&frame, 0, sizeof(frame));
    frame.command = MYCOBOT_CMD_GET_ANGLES;
    frame.payload_len = 12u;

    mycobot_transaction_init(&transaction);
    assert(mycobot_transaction_begin(&transaction, MYCOBOT_CMD_GET_ANGLES, 100u) == 1u);
    assert(mycobot_transaction_accept_frame(&transaction, &frame, 101u) ==
           MYCOBOT_TRANSACTION_ACCEPTED);
    assert(mycobot_transaction_active(&transaction) == 0u);
    assert(mycobot_transaction_accept_frame(&transaction, &frame, 102u) ==
           MYCOBOT_TRANSACTION_IDLE);
    assert(mycobot_transaction_accept_frame(&transaction, &frame, 103u) ==
           MYCOBOT_TRANSACTION_IDLE);

    counters = mycobot_transaction_get_counters(&transaction);
    assert(counters != 0);
    assert(counters->accepted == 1u);
    assert(counters->late_or_duplicate == 2u);
}

static void test_mycobot_transaction_single_flight_reject(void)
{
    mycobot_transaction_t transaction;
    const mycobot_transaction_counters_t *counters;

    mycobot_transaction_init(&transaction);
    assert(mycobot_transaction_begin(&transaction, MYCOBOT_CMD_GET_ANGLES, 100u) == 1u);
    assert(mycobot_transaction_begin(&transaction, MYCOBOT_CMD_GET_GRIPPER_VALUE, 101u) == 0u);
    assert(mycobot_transaction_active(&transaction) == 1u);
    assert(transaction.expected_command == MYCOBOT_CMD_GET_ANGLES);

    counters = mycobot_transaction_get_counters(&transaction);
    assert(counters != 0);
    assert(counters->single_flight_reject == 1u);
}

static void test_plan_validation(void)
{
    arm_controller_plan_t p = make_valid_plan();
    assert(arm_controller_plan_validate(&p) == 0);
    p.points[ARM_POINT_PICK].radius_mm_x10 = 3000;
    assert(arm_controller_plan_validate(&p) != 0);

    p = make_valid_plan();
    p.points[ARM_POINT_PICK].speed = 101u;
    assert(arm_controller_plan_validate(&p) != 0);

    p = make_valid_plan();
    p.gripper_speed = 0u;
    assert(arm_controller_plan_validate(&p) != 0);

    p = make_valid_plan();
    p.gripper_open = 101u;
    assert(arm_controller_plan_validate(&p) != 0);
}

static void test_default_arm_positions_plan(void)
{
    arm_controller_plan_t p;

    assert(arm_controller_plan_validate(&g_arm_default_plan) == 0);
    assert(g_arm_safe_position.joint_deg_x10[0] == 0);
    assert(g_arm_safe_position.speed == 30u);
    assert(g_arm_safe_position.gripper == 80u);

    assert(g_arm_default_plan.gripper_open == 80u);
    assert(g_arm_default_plan.gripper_closed == 20u);
    assert(g_arm_default_plan.gripper_speed == 50u);
    assert(g_arm_default_plan.refine_speed == ARM_CONTROLLER_DEFAULT_REFINE_SPEED);

    assert(g_arm_default_plan.points[ARM_POINT_HOME].speed == 30u);
    assert(g_arm_default_plan.points[ARM_POINT_HOME_READY].has_coord == 1u);
    assert(g_arm_default_plan.points[ARM_POINT_HOME_READY].joint_deg_x10[1] == -244);
    assert(g_arm_default_plan.points[ARM_POINT_PICK_HOVER].joint_deg_x10[0] == 439);
    assert(g_arm_default_plan.points[ARM_POINT_PICK].radius_mm_x10 == 2521u);
    assert(g_arm_default_plan.points[ARM_POINT_DROP].coord_x10[2] == 878);

    p = g_arm_default_plan;
    p.r_max_mm_x10 = 2520u;
    assert(arm_controller_plan_validate(&p) != 0);

    p = g_arm_default_plan;
    p.short_arm_joint_max_delta_x10 = 200u;
    assert(arm_controller_plan_validate(&p) != 0);

    p = g_arm_default_plan;
    p.return_arm_joint_max_delta_x10 = 500u;
    assert(arm_controller_plan_validate(&p) != 0);

    p = g_arm_default_plan;
    p.home_ready_arm_max_diff_x10 = 200u;
    assert(arm_controller_plan_validate(&p) != 0);
}

static void test_controller_happy_path(void)
{
    arm_controller_plan_t plan = make_valid_plan();
    arm_controller_ops_t ops;
    arm_controller_t arm;
    mock_arm_t mock;
    uint32_t now;

    memset(&mock, 0, sizeof(mock));
    ops.send_angles = mock_send_angles;
    ops.read_angles = mock_read_angles;
    ops.set_gripper = mock_set_gripper;

    arm_controller_init(&arm, 1u);
    arm_controller_configure(&arm, &plan, &ops, &mock);
    assert(arm_controller_request_grab(&arm) == 0);

    for (now = 0; now < 5000u && arm_controller_get_state(&arm) != ARM_STATE_DONE; now += 50u) {
        arm_controller_tick(&arm, now);
    }

    assert(arm_controller_get_state(&arm) == ARM_STATE_DONE);
    assert(arm_controller_get_error(&arm) == ARM_ERR_NONE);
    assert(mock.send_count >= 8u);
    assert(mock.gripper_count == 2u);
}

static void test_controller_soft_timeout_post_readback_failure(void)
{
    arm_controller_plan_t plan = make_valid_plan();
    arm_controller_ops_t ops;
    arm_controller_t arm;
    mock_arm_t mock;
    uint32_t now;

    memset(&mock, 0, sizeof(mock));
    mock.read_fail = 1u;
    ops.send_angles = mock_send_angles;
    ops.read_angles = mock_read_angles;
    ops.set_gripper = mock_set_gripper;
    plan.move_timeout_ms = 150u;

    arm_controller_init(&arm, 1u);
    arm_controller_configure(&arm, &plan, &ops, &mock);
    assert(arm_controller_request_grab(&arm) == 0);

    for (now = 0; now <= 300u && arm_controller_get_state(&arm) != ARM_STATE_FAULT; now += 50u) {
        arm_controller_tick(&arm, now);
    }

    assert(arm_controller_get_state(&arm) == ARM_STATE_FAULT);
    assert(arm_controller_get_error(&arm) == ARM_ERR_POST_READ_FAILED);
    assert(arm_controller_get_none_count(&arm) > 0u);
}

static void test_controller_soft_timeout_soft_pass(void)
{
    arm_controller_plan_t plan = make_valid_plan();
    arm_controller_ops_t ops;
    arm_controller_t arm;
    mock_arm_t mock;
    uint32_t now;

    memset(&mock, 0, sizeof(mock));
    ops.send_angles = mock_send_angles;
    ops.read_angles = mock_read_angles;
    ops.set_gripper = mock_set_gripper;
    plan.move_timeout_ms = 120u;
    plan.poll_interval_ms = 50u;
    mock.read_fail = 1u;

    arm_controller_init(&arm, 1u);
    arm_controller_configure(&arm, &plan, &ops, &mock);
    assert(arm_controller_request_grab(&arm) == 0);

    for (now = 0; now <= 4000u && arm_controller_get_state(&arm) != ARM_STATE_DONE &&
         arm_controller_get_state(&arm) != ARM_STATE_FAULT; now += 50u) {
        if (now >= 120u) {
            mock.read_fail = 0u;
        }
        arm_controller_tick(&arm, now);
    }

    assert(arm_controller_get_state(&arm) == ARM_STATE_DONE);
    assert(arm_controller_get_error(&arm) == ARM_ERR_SOFT_PASS_WARNING);
    assert(mock.send_count > 0u);
}

static void test_controller_soft_timeout_retry_once(void)
{
    arm_controller_plan_t plan = make_valid_plan();
    arm_controller_ops_t ops;
    arm_controller_t arm;
    mock_arm_t mock;
    uint32_t now;
    arm_state_t prev_state = ARM_STATE_IDLE;

    memset(&mock, 0, sizeof(mock));
    ops.send_angles = mock_send_angles;
    ops.read_angles = mock_read_angles;
    ops.set_gripper = mock_set_gripper;
    memset(mock.read_offset, 0, sizeof(mock.read_offset));
    mock.read_offset[0] = 300;
    mock.read_offset[1] = 300;
    mock.read_offset[2] = 300;
    mock.read_offset[3] = 300;
    mock.read_offset[4] = 300;
    mock.read_offset[5] = 300;
    mock.reads_before_converge = 255u;
    plan.move_timeout_ms = 120u;
    plan.poll_interval_ms = 50u;
    plan.refine_speed = 8u;

    arm_controller_init(&arm, 1u);
    arm_controller_configure(&arm, &plan, &ops, &mock);
    assert(arm_controller_request_grab(&arm) == 0);

    for (now = 0; now <= 4000u && arm_controller_get_state(&arm) != ARM_STATE_DONE &&
         arm_controller_get_state(&arm) != ARM_STATE_FAULT; now += 50u) {
        arm_controller_tick(&arm, now);
        if (prev_state != ARM_STATE_RETRY_ONCE &&
            arm_controller_get_state(&arm) == ARM_STATE_RETRY_ONCE) {
            mock.capture_retry_send_speed = 1u;
            mock.reads_before_converge = mock.read_count;
        }
        prev_state = arm_controller_get_state(&arm);
    }

    assert(arm_controller_get_state(&arm) == ARM_STATE_DONE);
    assert(arm_controller_get_error(&arm) == ARM_ERR_NONE);
    assert(mock.retry_send_speed == plan.refine_speed);
}

static void test_controller_soft_timeout_retry_failed(void)
{
    arm_controller_plan_t plan = make_valid_plan();
    arm_controller_ops_t ops;
    arm_controller_t arm;
    mock_arm_t mock;
    uint32_t now;

    memset(&mock, 0, sizeof(mock));
    ops.send_angles = mock_send_angles;
    ops.read_angles = mock_read_angles;
    ops.set_gripper = mock_set_gripper;
    memset(mock.read_offset, 0, sizeof(mock.read_offset));
    mock.read_offset[0] = 300;
    mock.read_offset[1] = 300;
    mock.read_offset[2] = 300;
    mock.read_offset[3] = 300;
    mock.read_offset[4] = 300;
    mock.read_offset[5] = 300;
    mock.reads_before_converge = 255u;
    plan.move_timeout_ms = 120u;
    plan.poll_interval_ms = 50u;

    arm_controller_init(&arm, 1u);
    arm_controller_configure(&arm, &plan, &ops, &mock);
    assert(arm_controller_request_grab(&arm) == 0);

    for (now = 0; now <= 800u && arm_controller_get_state(&arm) != ARM_STATE_FAULT; now += 50u) {
        arm_controller_tick(&arm, now);
    }

    assert(arm_controller_get_state(&arm) == ARM_STATE_FAULT);
    assert(arm_controller_get_error(&arm) == ARM_ERR_RETRY_FAILED);
}

static void test_controller_no_consecutive_confirm_after_read_fail(void)
{
    arm_controller_plan_t plan = make_valid_plan();
    arm_controller_ops_t ops;
    arm_controller_t arm;
    mock_arm_t mock;

    memset(&mock, 0, sizeof(mock));
    ops.send_angles = mock_send_angles;
    ops.read_angles = mock_read_angles;
    ops.set_gripper = mock_set_gripper;
    memset(mock.read_offset, 0, sizeof(mock.read_offset));
    plan.confirm_required = 2u;
    plan.move_timeout_ms = 400u;
    plan.poll_interval_ms = 50u;
    mock.read_fail_at_enable = 1u;
    mock.read_fail_at = 1u;

    arm_controller_init(&arm, 1u);
    arm_controller_configure(&arm, &plan, &ops, &mock);
    assert(arm_controller_request_grab(&arm) == 0);

    arm_controller_tick(&arm, 0u);
    assert(arm_controller_get_state(&arm) == ARM_STATE_PICK_HOVER);

    arm_controller_tick(&arm, 50u);
    assert(arm_controller_get_state(&arm) == ARM_STATE_PICK_HOVER);

    arm_controller_tick(&arm, 100u);
    assert(arm_controller_get_state(&arm) == ARM_STATE_PICK_HOVER);

    arm_controller_tick(&arm, 150u);
    assert(arm_controller_get_state(&arm) == ARM_STATE_PICK_HOVER);

    arm_controller_tick(&arm, 200u);
    assert(arm_controller_get_state(&arm) == ARM_STATE_PICK_DOWN);
}

static void test_encode_send_angles_payload(void)
{
    uint8_t buf[16];
    uint8_t len;
    uint8_t i;
    int16_t zero[6];
    int16_t posneg[6];
    int16_t any[6];

    memset(zero, 0, sizeof(zero));
    memset(any, 0, sizeof(any));

    /* all zeros (HOME) */
    len = mycobot_encode_send_angles_payload(zero, 30, buf, sizeof(buf));
    assert(len == 13u);
    for (i = 0; i < 6; ++i) {
        assert(buf[i * 2u] == 0x00u);
        assert(buf[i * 2u + 1u] == 0x00u);
    }
    assert(buf[12] == 30u);

    /* positive and negative angles */
    posneg[0] = 123;   posneg[1] = 456;   posneg[2] = 789;
    posneg[3] = -123;  posneg[4] = -456;  posneg[5] = -789;

    len = mycobot_encode_send_angles_payload(posneg, 100, buf, sizeof(buf));
    assert(len == 13u);
    /* 123 *10 = 1230 = 0x04CE */
    assert(buf[0] == 0x04u && buf[1] == 0xCEu);
    /* 456 *10 = 4560 = 0x11D0 */
    assert(buf[2] == 0x11u && buf[3] == 0xD0u);
    /* 789 *10 = 7890 = 0x1ED2 */
    assert(buf[4] == 0x1Eu && buf[5] == 0xD2u);
    /* -123 *10 = -1230 = 0xFB32 */
    assert(buf[6] == 0xFBu && buf[7] == 0x32u);
    /* -456 *10 = -4560 = 0xEE30 */
    assert(buf[8] == 0xEEu && buf[9] == 0x30u);
    /* -789 *10 = -7890 = 0xE12E */
    assert(buf[10] == 0xE1u && buf[11] == 0x2Eu);
    assert(buf[12] == 100u);

    /* invalid speed rejected */
    len = mycobot_encode_send_angles_payload(any, 0, buf, sizeof(buf));
    assert(len == 0u);
    len = mycobot_encode_send_angles_payload(any, 101, buf, sizeof(buf));
    assert(len == 0u);

    /* buffer too small */
    len = mycobot_encode_send_angles_payload(any, 30, buf, 12u);
    assert(len == 0u);

    /* NULL pointer */
    len = mycobot_encode_send_angles_payload(NULL, 30, buf, sizeof(buf));
    assert(len == 0u);
}

static void test_decode_get_angles_response(void)
{
    uint8_t payload[12];
    int16_t angles[6];
    uint8_t i;

    /* all zeros */
    memset(payload, 0, sizeof(payload));
    assert(mycobot_decode_get_angles_response(payload, 12, angles) == MYCOBOT_HELPER_OK);
    for (i = 0; i < 6; ++i) {
        assert(angles[i] == 0);
    }

    /* known positive values */
    payload[0] = 0x04u; payload[1] = 0xCEu;
    payload[2] = 0x11u; payload[3] = 0xD0u;
    payload[4] = 0x1Eu; payload[5] = 0xD2u;
    payload[6] = 0x03u; payload[7] = 0xE8u;
    payload[8] = 0x07u; payload[9] = 0xD0u;
    payload[10] = 0x0Bu; payload[11] = 0xB8u;

    assert(mycobot_decode_get_angles_response(payload, 12, angles) == MYCOBOT_HELPER_OK);
    assert(angles[0] == 123);   /* 1230 / 10 */
    assert(angles[1] == 456);   /* 4560 / 10 */
    assert(angles[2] == 789);   /* 7890 / 10 */
    assert(angles[3] == 100);   /* 1000 / 10 */
    assert(angles[4] == 200);   /* 2000 / 10 */
    assert(angles[5] == 300);   /* 3000 / 10 */

    /* negative values: -1000 -> 0xFC18, -400 -> 0xFE70 */
    payload[0] = 0xFCu; payload[1] = 0x18u;
    payload[2] = 0xFEu; payload[3] = 0x70u;
    memset(&payload[4], 0, 8);

    assert(mycobot_decode_get_angles_response(payload, 12, angles) == MYCOBOT_HELPER_OK);
    assert(angles[0] == -100);  /* -1000 / 10 */
    assert(angles[1] == -40);   /* -400  / 10 */

    /* wrong length */
    assert(mycobot_decode_get_angles_response(payload, 6, angles) == MYCOBOT_HELPER_BAD_LENGTH);
    assert(mycobot_decode_get_angles_response(payload, 0, angles) == MYCOBOT_HELPER_BAD_LENGTH);

    /* NULL args */
    assert(mycobot_decode_get_angles_response(NULL, 12, angles) == MYCOBOT_HELPER_INVALID_ARG);
    assert(mycobot_decode_get_angles_response(payload, 12, NULL) == MYCOBOT_HELPER_INVALID_ARG);
}

static void test_angles_roundtrip(void)
{
    int16_t original[6];
    uint8_t buf[16];
    int16_t decoded[6];

    original[0] = 1230;  original[1] = -456;  original[2] = 789;
    original[3] = -100;  original[4] = 0;     original[5] = 1799;

    assert(mycobot_encode_send_angles_payload(original, 50, buf, sizeof(buf)) == 13u);
    assert(mycobot_decode_get_angles_response(buf, 12, decoded) == MYCOBOT_HELPER_OK);

    /* encode: deg_x10 * 10 -> int16; decode: / 10 -> deg_x10 */
    assert(decoded[0] == 1230);   /* 12300 / 10 */
    assert(decoded[1] == -456);   /* -4560 / 10 */
    assert(decoded[2] == 789);    /*  7890 / 10 */
    assert(decoded[3] == -100);   /* -1000 / 10 */
    assert(decoded[4] == 0);      /*     0 / 10 */
    assert(decoded[5] == 1799);
    original[5] = 1801;
    assert(mycobot_encode_send_angles_payload(original, 50, buf, sizeof(buf)) == 0u);
}

static void test_encode_gripper_payloads(void)
{
    uint8_t buf[8];
    uint8_t len;

    /* gripper state: open */
    len = mycobot_encode_gripper_state_payload(0, 50, buf, sizeof(buf));
    assert(len == 2u);
    assert(buf[0] == 0u);
    assert(buf[1] == 50u);

    /* gripper state: close */
    len = mycobot_encode_gripper_state_payload(1, 100, buf, sizeof(buf));
    assert(len == 2u);
    assert(buf[0] == 1u);
    assert(buf[1] == 100u);

    /* invalid state */
    len = mycobot_encode_gripper_state_payload(2, 50, buf, sizeof(buf));
    assert(len == 0u);
    len = mycobot_encode_gripper_state_payload(0, 0, buf, sizeof(buf));
    assert(len == 0u);
    len = mycobot_encode_gripper_state_payload(1, 101, buf, sizeof(buf));
    assert(len == 0u);

    /* gripper value */
    len = mycobot_encode_gripper_value_payload(80, 50, buf, sizeof(buf));
    assert(len == 2u);
    assert(buf[0] == 80u);
    assert(buf[1] == 50u);

    /* invalid gripper value or speed */
    len = mycobot_encode_gripper_value_payload(101, 50, buf, sizeof(buf));
    assert(len == 0u);
    len = mycobot_encode_gripper_value_payload(80, 0, buf, sizeof(buf));
    assert(len == 0u);
    len = mycobot_encode_gripper_value_payload(80, 101, buf, sizeof(buf));
    assert(len == 0u);

    /* buffer too small */
    len = mycobot_encode_gripper_state_payload(0, 50, buf, 1u);
    assert(len == 0u);
    len = mycobot_encode_gripper_value_payload(80, 50, buf, 1u);
    assert(len == 0u);

    /* NULL pointer */
    len = mycobot_encode_gripper_state_payload(0, 50, NULL, sizeof(buf));
    assert(len == 0u);
    len = mycobot_encode_gripper_value_payload(80, 50, NULL, sizeof(buf));
    assert(len == 0u);
}

static void test_transport_rx_partial_and_noise(void)
{
    mycobot_transport_t transport;
    mycobot_frame_t frame;
    const mycobot_transport_counters_t *counters;
    uint8_t payload[2] = {0x12u, 0x34u};
    uint8_t raw[8];
    uint8_t n;

    mycobot_transport_init(&transport);
    assert(mycobot_transport_rx_available(&transport) == 0u);
    assert(mycobot_transport_next_frame(&transport, &frame) == MYCOBOT_TRANSPORT_NO_FRAME);

    assert(mycobot_transport_rx_push_byte(&transport, 0x55u) == 1u);
    assert(mycobot_transport_next_frame(&transport, &frame) == MYCOBOT_TRANSPORT_NO_FRAME);
    counters = mycobot_transport_get_counters(&transport);
    assert(counters != 0);
    assert(counters->noise_bytes == 1u);

    n = mycobot_build_frame_ex(MYCOBOT_CMD_SET_GRIPPER_VALUE,
                               payload, 2u, raw, sizeof(raw));
    assert(n == 7u);
    assert(mycobot_transport_rx_push(&transport, raw, 3u) == 3u);
    assert(mycobot_transport_next_frame(&transport, &frame) == MYCOBOT_TRANSPORT_NO_FRAME);
    assert(mycobot_transport_rx_push(&transport, &raw[3], (uint16_t)(n - 3u)) ==
           (uint16_t)(n - 3u));

    assert(mycobot_transport_next_frame(&transport, &frame) == MYCOBOT_TRANSPORT_OK);
    assert(frame.command == MYCOBOT_CMD_SET_GRIPPER_VALUE);
    assert(frame.payload_len == 2u);
    assert(frame.payload[0] == 0x12u);
    assert(frame.payload[1] == 0x34u);
    assert(mycobot_transport_rx_available(&transport) == 0u);
    assert(counters->frames_ok == 1u);
}

static void test_transport_rx_bad_footer_resync(void)
{
    mycobot_transport_t transport;
    mycobot_frame_t frame;
    const mycobot_transport_counters_t *counters;
    uint8_t good[8];
    uint8_t bad[8];
    uint8_t n;

    mycobot_transport_init(&transport);
    n = mycobot_build_frame_ex(MYCOBOT_CMD_GET_ANGLES, 0, 0u, good, sizeof(good));
    assert(n == 5u);
    memcpy(bad, good, n);
    bad[n - 1u] = 0x00u;

    assert(mycobot_transport_rx_push(&transport, bad, n) == n);
    assert(mycobot_transport_rx_push(&transport, good, n) == n);
    assert(mycobot_transport_next_frame(&transport, &frame) == MYCOBOT_TRANSPORT_OK);
    assert(frame.command == MYCOBOT_CMD_GET_ANGLES);
    assert(frame.payload_len == 0u);

    counters = mycobot_transport_get_counters(&transport);
    assert(counters != 0);
    assert(counters->bad_footer == 1u);
    assert(counters->frames_ok == 1u);
}

static void test_transport_rx_wrap_and_overflow(void)
{
    mycobot_transport_t transport;
    mycobot_frame_t frame;
    const mycobot_transport_counters_t *counters;
    uint8_t payload[8] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u};
    uint8_t raw[16];
    uint16_t i;
    uint8_t n;

    mycobot_transport_init(&transport);
    for (i = 0u; i < 120u; ++i) {
        assert(mycobot_transport_rx_push_byte(&transport, 0x00u) == 1u);
    }
    assert(mycobot_transport_next_frame(&transport, &frame) == MYCOBOT_TRANSPORT_NO_FRAME);
    assert(mycobot_transport_rx_available(&transport) == 0u);

    n = mycobot_build_frame_ex(MYCOBOT_CMD_SEND_ANGLES,
                               payload, 8u, raw, sizeof(raw));
    assert(n == 13u);
    assert(mycobot_transport_rx_push(&transport, raw, n) == n);
    assert(mycobot_transport_next_frame(&transport, &frame) == MYCOBOT_TRANSPORT_OK);
    assert(frame.command == MYCOBOT_CMD_SEND_ANGLES);
    assert(frame.payload_len == 8u);
    assert(frame.payload[7] == 7u);

    for (i = 0u; i < MYCOBOT_TRANSPORT_RX_CAPACITY; ++i) {
        assert(mycobot_transport_rx_push_byte(&transport, 0x00u) == 1u);
    }
    assert(mycobot_transport_rx_push_byte(&transport, 0x00u) == 0u);
    counters = mycobot_transport_get_counters(&transport);
    assert(counters != 0);
    assert(counters->rx_overflow == 1u);
}

static void test_transport_tx_queue_and_pop(void)
{
    mycobot_transport_t transport;
    const mycobot_transport_counters_t *counters;
    uint8_t payload[2] = {0x33u, 0x44u};
    uint8_t expected[8];
    uint8_t byte;
    uint8_t n;
    uint8_t i;

    mycobot_transport_init(&transport);
    n = mycobot_build_frame_ex(MYCOBOT_CMD_SET_GRIPPER_VALUE,
                               payload, 2u, expected, sizeof(expected));
    assert(n == 7u);

    assert(mycobot_transport_tx_pending(&transport) == 0u);
    assert(mycobot_transport_tx_busy(&transport) == 0u);
    assert(mycobot_transport_tx_queue_frame(&transport,
                                            MYCOBOT_CMD_SET_GRIPPER_VALUE,
                                            payload,
                                            2u) == MYCOBOT_TRANSPORT_OK);
    assert(mycobot_transport_tx_pending(&transport) == n);
    assert(mycobot_transport_tx_busy(&transport) == 1u);
    assert(mycobot_transport_tx_queue_frame(&transport,
                                            MYCOBOT_CMD_GET_ANGLES,
                                            0,
                                            0u) == MYCOBOT_TRANSPORT_BUSY);

    for (i = 0u; i < n; ++i) {
        assert(mycobot_transport_tx_pop_byte(&transport, &byte) == MYCOBOT_TRANSPORT_OK);
        assert(byte == expected[i]);
    }
    assert(mycobot_transport_tx_pop_byte(&transport, &byte) == MYCOBOT_TRANSPORT_NO_FRAME);
    assert(mycobot_transport_tx_busy(&transport) == 0u);
    assert(mycobot_transport_tx_pending(&transport) == 0u);

    counters = mycobot_transport_get_counters(&transport);
    assert(counters != 0);
    assert(counters->tx_frames_queued == 1u);
    assert(counters->tx_busy_reject == 1u);
    assert(counters->tx_bytes_popped == n);
}

static void test_transport_tx_abort_and_invalid(void)
{
    mycobot_transport_t transport;
    const mycobot_transport_counters_t *counters;
    uint8_t payload[1] = {0x99u};
    uint8_t too_long[MYCOBOT_MAX_PAYLOAD + 1u];
    uint8_t byte;

    mycobot_transport_init(&transport);
    assert(mycobot_transport_tx_queue_frame(&transport,
                                            MYCOBOT_CMD_GET_ANGLES,
                                            payload,
                                            1u) == MYCOBOT_TRANSPORT_OK);
    assert(mycobot_transport_tx_busy(&transport) == 1u);
    mycobot_transport_tx_abort(&transport);
    assert(mycobot_transport_tx_busy(&transport) == 0u);
    assert(mycobot_transport_tx_pop_byte(&transport, &byte) == MYCOBOT_TRANSPORT_NO_FRAME);

    assert(mycobot_transport_tx_queue_frame(&transport,
                                            MYCOBOT_CMD_GET_ANGLES,
                                            0,
                                            1u) == MYCOBOT_TRANSPORT_INVALID_ARG);
    assert(mycobot_transport_tx_queue_frame(0,
                                            MYCOBOT_CMD_GET_ANGLES,
                                            0,
                                            0u) == MYCOBOT_TRANSPORT_INVALID_ARG);

    memset(too_long, 0, sizeof(too_long));
    assert(mycobot_transport_tx_queue_frame(&transport,
                                            MYCOBOT_CMD_GET_ANGLES,
                                            too_long,
                                            (uint8_t)sizeof(too_long)) ==
           MYCOBOT_TRANSPORT_BUFFER_TOO_SMALL);

    counters = mycobot_transport_get_counters(&transport);
    assert(counters != 0);
    assert(counters->tx_build_failed == 1u);
}

int main(void)
{
    test_frame_build_and_parse();
    test_protocol_truth_table_and_joint_limits();
    test_get_angles_official_vectors_and_transaction();
    test_mycobot_transaction_timeout_boundary();
    test_mycobot_transaction_timeout_wraparound();
    test_mycobot_transaction_bad_frames_remain_in_flight();
    test_mycobot_transaction_first_valid_response_wins();
    test_mycobot_transaction_single_flight_reject();
    test_plan_validation();
    test_default_arm_positions_plan();
    test_controller_happy_path();
    test_controller_soft_timeout_post_readback_failure();
    test_controller_soft_timeout_soft_pass();
    test_controller_soft_timeout_retry_once();
    test_controller_soft_timeout_retry_failed();
    test_controller_no_consecutive_confirm_after_read_fail();
    test_encode_send_angles_payload();
    test_decode_get_angles_response();
    test_angles_roundtrip();
    test_encode_gripper_payloads();
    test_transport_rx_partial_and_noise();
    test_transport_rx_bad_footer_resync();
    test_transport_rx_wrap_and_overflow();
    test_transport_tx_queue_and_pop();
    test_transport_tx_abort_and_invalid();
    return 0;
}
