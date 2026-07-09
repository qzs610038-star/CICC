#include "arm_controller.h"

static uint16_t cfg_u16(uint16_t value, uint16_t fallback)
{
    return value ? value : fallback;
}

static uint32_t cfg_u32(uint32_t value, uint32_t fallback)
{
    return value ? value : fallback;
}

static uint16_t abs_delta_x10(int16_t a, int16_t b)
{
    int32_t d = (int32_t)a - (int32_t)b;
    if (d < 0) {
        d = -d;
    }
    if (d > 65535) {
        return 65535u;
    }
    return (uint16_t)d;
}

static int time_reached(uint32_t now_ms, uint32_t target_ms)
{
    return (int32_t)(now_ms - target_ms) >= 0;
}

static uint16_t max_arm_delta_from_home(const arm_controller_point_t *point)
{
    uint8_t i;
    uint16_t max_delta = 0;

    for (i = 0; i < 5u; ++i) {
        uint16_t d = abs_delta_x10(point->joint_deg_x10[i], 0);
        if (d > max_delta) {
            max_delta = d;
        }
    }

    return max_delta;
}

static uint16_t max_pair_delta(const arm_controller_point_t *a,
                               const arm_controller_point_t *b,
                               uint8_t first_joint,
                               uint8_t joint_count)
{
    uint8_t i;
    uint16_t max_delta = 0;

    for (i = 0; i < joint_count; ++i) {
        uint8_t idx = (uint8_t)(first_joint + i);
        uint16_t d = abs_delta_x10(a->joint_deg_x10[idx], b->joint_deg_x10[idx]);
        if (d > max_delta) {
            max_delta = d;
        }
    }

    return max_delta;
}

static int validate_point(const arm_controller_plan_t *plan, arm_point_id_t point_id)
{
    const arm_controller_point_t *point = &plan->points[point_id];
    uint16_t r_max = cfg_u16(plan->r_max_mm_x10,
                             ARM_CONTROLLER_DEFAULT_R_MAX_X10);

    if (point->speed == 0u || point->speed > ARM_CONTROLLER_SPEED_MAX) {
        return -1;
    }

    if (point->radius_mm_x10 && point->radius_mm_x10 > r_max) {
        return -1;
    }

    return 0;
}

static void set_fault(arm_controller_t *arm, arm_error_t error)
{
    if (!arm) {
        return;
    }
    arm->state = ARM_STATE_FAULT;
    arm->error = error;
}

static void start_move(arm_controller_t *arm,
                       arm_state_t state,
                       arm_point_id_t point,
                       uint16_t tol_deg_x10,
                       uint32_t now_ms,
                       uint32_t timeout_ms)
{
    arm->state = state;
    arm->active_point = point;
    arm->active_tol_deg_x10 = tol_deg_x10;
    arm->deadline_ms = now_ms + timeout_ms;
    arm->next_poll_ms = now_ms;
    arm->move_sent = 0u;
    arm->confirm_count = 0u;
    arm->none_count = 0u;
    arm->retry_count = 0u;
    arm->move_state_snapshot = (uint8_t)state;
}

static uint16_t required_confirm(const arm_controller_t *arm)
{
    return cfg_u16(arm->plan->confirm_required,
                   ARM_CONTROLLER_DEFAULT_CONFIRM);
}

static uint16_t poll_interval(const arm_controller_t *arm)
{
    return cfg_u16(arm->plan->poll_interval_ms,
                   ARM_CONTROLLER_DEFAULT_POLL_MS);
}

static uint32_t move_timeout(const arm_controller_t *arm)
{
    return cfg_u32(arm->plan->move_timeout_ms,
                   ARM_CONTROLLER_DEFAULT_MOVE_TIMEOUT_MS);
}

static uint32_t home_timeout(const arm_controller_t *arm)
{
    return cfg_u32(arm->plan->home_timeout_ms,
                   ARM_CONTROLLER_DEFAULT_HOME_TIMEOUT_MS);
}

static uint16_t short_tol(const arm_controller_t *arm)
{
    return cfg_u16(arm->plan->short_tol_deg_x10,
                   ARM_CONTROLLER_DEFAULT_SHORT_TOL_X10);
}

static uint16_t home_tol(const arm_controller_t *arm)
{
    return cfg_u16(arm->plan->home_tol_deg_x10,
                   ARM_CONTROLLER_DEFAULT_HOME_TOL_X10);
}

static uint16_t refine_speed(const arm_controller_t *arm)
{
    return cfg_u16(arm->plan->refine_speed,
                   ARM_CONTROLLER_DEFAULT_REFINE_SPEED);
}

static uint16_t max_angle_error(const int16_t actual[ARM_CONTROLLER_JOINTS],
                                const int16_t target[ARM_CONTROLLER_JOINTS])
{
    uint8_t i;
    uint16_t max_delta = 0;

    for (i = 0; i < ARM_CONTROLLER_JOINTS; ++i) {
        uint16_t d = abs_delta_x10(actual[i], target[i]);
        if (d > max_delta) {
            max_delta = d;
        }
    }

    return max_delta;
}

static void advance_from_move(arm_controller_t *arm,
                               arm_state_t move_state,
                               uint32_t now_ms)
{
    switch (move_state) {
    case ARM_STATE_PICK_HOVER:
        start_move(arm, ARM_STATE_PICK_DOWN, ARM_POINT_PICK,
                   short_tol(arm), now_ms, move_timeout(arm));
        break;
    case ARM_STATE_PICK_DOWN:
        arm->state = ARM_STATE_GRIP_CLOSE;
        break;
    case ARM_STATE_PICK_LIFT:
        start_move(arm, ARM_STATE_DROP_HOVER, ARM_POINT_DROP_HOVER,
                   short_tol(arm), now_ms, move_timeout(arm));
        break;
    case ARM_STATE_DROP_HOVER:
        start_move(arm, ARM_STATE_DROP_DOWN, ARM_POINT_DROP,
                   short_tol(arm), now_ms, move_timeout(arm));
        break;
    case ARM_STATE_DROP_DOWN:
        arm->state = ARM_STATE_GRIP_OPEN;
        break;
    case ARM_STATE_DROP_LIFT:
        start_move(arm, ARM_STATE_RETURN_HOME_READY, ARM_POINT_HOME_READY,
                   short_tol(arm), now_ms, move_timeout(arm));
        break;
    case ARM_STATE_RETURN_HOME_READY:
        start_move(arm, ARM_STATE_RETURN_HOME, ARM_POINT_HOME,
                   home_tol(arm), now_ms, home_timeout(arm));
        break;
    case ARM_STATE_RETURN_HOME:
        arm->state = ARM_STATE_DONE;
        break;
    default:
        set_fault(arm, ARM_ERR_TARGET_INVALID);
        break;
    }
}

static int is_move_state(arm_state_t state)
{
    return state == ARM_STATE_PICK_HOVER ||
           state == ARM_STATE_PICK_DOWN ||
           state == ARM_STATE_PICK_LIFT ||
           state == ARM_STATE_DROP_HOVER ||
           state == ARM_STATE_DROP_DOWN ||
           state == ARM_STATE_DROP_LIFT ||
           state == ARM_STATE_RETURN_HOME_READY ||
           state == ARM_STATE_RETURN_HOME ||
           state == ARM_STATE_RETRY_ONCE;
}

static void handle_move_state(arm_controller_t *arm, uint32_t now_ms)
{
    int16_t actual[ARM_CONTROLLER_JOINTS];
    const arm_controller_point_t *target;
    uint16_t err;

    target = &arm->plan->points[arm->active_point];

    if (!arm->ops || !arm->ops->send_angles || !arm->ops->read_angles) {
        set_fault(arm, ARM_ERR_NO_TRANSPORT);
        return;
    }

    if (!arm->move_sent) {
        uint16_t speed = (arm->state == ARM_STATE_RETRY_ONCE) ?
                         refine_speed(arm) : target->speed;
        if (arm->ops->send_angles(arm->user, target->joint_deg_x10,
                                  speed) != 0) {
            set_fault(arm, ARM_ERR_PROTOCOL_TIMEOUT);
            return;
        }
        arm->move_sent = 1u;
    }

    if (!time_reached(now_ms, arm->next_poll_ms)) {
        return;
    }

    arm->next_poll_ms = now_ms + poll_interval(arm);

    if (arm->ops->read_angles(arm->user, actual) != 0) {
        arm->none_count++;
        arm->confirm_count = 0u;
    } else {
        err = max_angle_error(actual, target->joint_deg_x10);
        if (err <= arm->active_tol_deg_x10) {
            if (arm->confirm_count < 255u) {
                arm->confirm_count++;
            }
        } else {
            arm->confirm_count = 0u;
        }

        if (arm->confirm_count >= required_confirm(arm)) {
            if (arm->state == ARM_STATE_RETRY_ONCE) {
                arm->error = ARM_ERR_NONE;
            }
            advance_from_move(arm, (arm_state_t)arm->move_state_snapshot, now_ms);
            return;
        }
    }

    if (time_reached(now_ms, arm->deadline_ms)) {
        if (arm->state == ARM_STATE_RETRY_ONCE) {
            set_fault(arm, ARM_ERR_RETRY_FAILED);
        } else {
            arm->error = ARM_ERR_SOFT_TIMEOUT;
            arm->move_state_snapshot = (uint8_t)arm->state;
            arm->state = ARM_STATE_POST_READBACK;
            arm->next_poll_ms = now_ms;
        }
    }
}

static void handle_post_readback(arm_controller_t *arm, uint32_t now_ms)
{
    int16_t actual[ARM_CONTROLLER_JOINTS];
    const arm_controller_point_t *target;
    uint16_t err;

    if (!arm->ops || !arm->ops->read_angles) {
        set_fault(arm, ARM_ERR_POST_READ_FAILED);
        return;
    }

    if (arm->ops->read_angles(arm->user, actual) != 0) {
        set_fault(arm, ARM_ERR_POST_READ_FAILED);
        return;
    }

    target = &arm->plan->points[arm->active_point];
    err = max_angle_error(actual, target->joint_deg_x10);

    if (err <= arm->active_tol_deg_x10) {
        arm->error = ARM_ERR_SOFT_PASS_WARNING;
        advance_from_move(arm, (arm_state_t)arm->move_state_snapshot, now_ms);
    } else {
        if (arm->retry_count != 0u) {
            set_fault(arm, ARM_ERR_RETRY_FAILED);
            return;
        }

        arm->retry_count = 1u;
        arm->state = ARM_STATE_RETRY_ONCE;
        arm->move_sent = 0u;
        arm->confirm_count = 0u;
        arm->none_count = 0u;
        arm->deadline_ms = now_ms + move_timeout(arm);
        arm->next_poll_ms = now_ms;
    }
}

static void handle_gripper_state(arm_controller_t *arm, uint32_t now_ms)
{
    uint16_t target;

    if (!arm->ops || !arm->ops->set_gripper) {
        set_fault(arm, ARM_ERR_NO_TRANSPORT);
        return;
    }

    target = (arm->state == ARM_STATE_GRIP_CLOSE) ?
             arm->plan->gripper_closed : arm->plan->gripper_open;

    if (arm->ops->set_gripper(arm->user, target,
                              arm->plan->gripper_speed) != 0) {
        set_fault(arm, ARM_ERR_PROTOCOL_TIMEOUT);
        return;
    }

    if (arm->state == ARM_STATE_GRIP_CLOSE) {
        start_move(arm, ARM_STATE_PICK_LIFT, ARM_POINT_PICK_HOVER,
                   short_tol(arm), now_ms, move_timeout(arm));
    } else {
        start_move(arm, ARM_STATE_DROP_LIFT, ARM_POINT_DROP_HOVER,
                   short_tol(arm), now_ms, move_timeout(arm));
    }
}

void arm_controller_init(arm_controller_t *arm, uint8_t arm_id)
{
    if (!arm) {
        return;
    }
    arm->arm_id = arm_id;
    arm->state = ARM_STATE_IDLE;
    arm->error = ARM_ERR_NONE;
    arm->plan = 0;
    arm->ops = 0;
    arm->user = 0;
    arm->active_point = ARM_POINT_HOME;
    arm->active_tol_deg_x10 = 0;
    arm->deadline_ms = 0;
    arm->next_poll_ms = 0;
    arm->move_sent = 0;
    arm->confirm_count = 0;
    arm->none_count = 0;
    arm->retry_count = 0;
}

void arm_controller_configure(arm_controller_t *arm,
                              const arm_controller_plan_t *plan,
                              const arm_controller_ops_t *ops,
                              void *user)
{
    if (!arm) {
        return;
    }
    arm->plan = plan;
    arm->ops = ops;
    arm->user = user;
}

int arm_controller_plan_validate(const arm_controller_plan_t *plan)
{
    uint16_t short_delta;
    uint16_t return_delta;
    uint16_t home_ready_delta;
    uint16_t short_limit;
    uint16_t return_limit;
    uint16_t home_ready_limit;
    uint8_t i;

    if (!plan) {
        return -1;
    }

    if (plan->gripper_open > ARM_CONTROLLER_GRIPPER_VALUE_MAX ||
        plan->gripper_closed > ARM_CONTROLLER_GRIPPER_VALUE_MAX ||
        plan->gripper_speed == 0u ||
        plan->gripper_speed > ARM_CONTROLLER_SPEED_MAX ||
        plan->refine_speed > ARM_CONTROLLER_SPEED_MAX) {
        return -1;
    }

    for (i = 0; i < (uint8_t)ARM_POINT_COUNT; ++i) {
        if (validate_point(plan, (arm_point_id_t)i) != 0) {
            return -1;
        }
    }

    short_limit = cfg_u16(plan->short_arm_joint_max_delta_x10,
                          ARM_CONTROLLER_DEFAULT_SHORT_DELTA_X10);
    return_limit = cfg_u16(plan->return_arm_joint_max_delta_x10,
                           ARM_CONTROLLER_DEFAULT_RETURN_DELTA_X10);
    home_ready_limit = cfg_u16(plan->home_ready_arm_max_diff_x10,
                               ARM_CONTROLLER_DEFAULT_HOME_READY_X10);

    short_delta = max_pair_delta(&plan->points[ARM_POINT_PICK_HOVER],
                                 &plan->points[ARM_POINT_PICK], 0u, 5u);
    if (short_delta > short_limit) {
        return -1;
    }

    short_delta = max_pair_delta(&plan->points[ARM_POINT_DROP_HOVER],
                                 &plan->points[ARM_POINT_DROP], 0u, 5u);
    if (short_delta > short_limit) {
        return -1;
    }

    return_delta = max_pair_delta(&plan->points[ARM_POINT_DROP_HOVER],
                                  &plan->points[ARM_POINT_HOME_READY],
                                  0u, 5u);
    if (return_delta > return_limit) {
        return -1;
    }

    home_ready_delta = max_arm_delta_from_home(&plan->points[ARM_POINT_HOME_READY]);
    if (home_ready_delta > home_ready_limit) {
        return -1;
    }

    return 0;
}

int arm_controller_request_grab(arm_controller_t *arm)
{
    if (!arm) {
        return -1;
    }

    if (arm->state != ARM_STATE_IDLE && arm->state != ARM_STATE_DONE) {
        arm->error = ARM_ERR_BUSY;
        return -1;
    }

    if (!arm->plan) {
        set_fault(arm, ARM_ERR_NO_PLAN);
        return -1;
    }

    arm->state = ARM_STATE_PRECHECK;
    arm->error = ARM_ERR_NONE;
    arm->move_sent = 0u;
    arm->confirm_count = 0u;
    arm->none_count = 0u;
    arm->retry_count = 0u;
    return 0;
}

void arm_controller_tick(arm_controller_t *arm, uint32_t now_ms)
{
    if (!arm) {
        return;
    }

    switch (arm->state) {
    case ARM_STATE_PRECHECK:
        if (arm_controller_plan_validate(arm->plan) != 0) {
            set_fault(arm, ARM_ERR_PRECHECK_FAILED);
            return;
        }
        start_move(arm, ARM_STATE_PICK_HOVER, ARM_POINT_PICK_HOVER,
                   short_tol(arm), now_ms, move_timeout(arm));
        break;
    case ARM_STATE_GRIP_CLOSE:
    case ARM_STATE_GRIP_OPEN:
        handle_gripper_state(arm, now_ms);
        break;
    case ARM_STATE_POST_READBACK:
        handle_post_readback(arm, now_ms);
        break;
    case ARM_STATE_IDLE:
    case ARM_STATE_DONE:
    case ARM_STATE_FAULT:
    case ARM_STATE_ESTOP:
        break;
    default:
        if (is_move_state(arm->state)) {
            handle_move_state(arm, now_ms);
        } else {
            set_fault(arm, ARM_ERR_TARGET_INVALID);
        }
        break;
    }
}

void arm_controller_cancel(arm_controller_t *arm, arm_error_t reason)
{
    if (!arm) {
        return;
    }
    arm->state = ARM_STATE_ESTOP;
    arm->error = reason ? reason : ARM_ERR_UNSAFE_RELEASE_REQUIRED;
}

arm_state_t arm_controller_get_state(const arm_controller_t *arm)
{
    return arm ? arm->state : ARM_STATE_FAULT;
}

arm_error_t arm_controller_get_error(const arm_controller_t *arm)
{
    return arm ? arm->error : ARM_ERR_TARGET_INVALID;
}

uint16_t arm_controller_get_none_count(const arm_controller_t *arm)
{
    return arm ? arm->none_count : 0u;
}
