#ifndef ARM_CONTROLLER_H
#define ARM_CONTROLLER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARM_CONTROLLER_JOINTS                 6u
#define ARM_CONTROLLER_DEFAULT_SHORT_TOL_X10  30u
#define ARM_CONTROLLER_DEFAULT_HOME_TOL_X10   15u
#define ARM_CONTROLLER_DEFAULT_CONFIRM        2u
#define ARM_CONTROLLER_DEFAULT_POLL_MS        50u
#define ARM_CONTROLLER_DEFAULT_MOVE_TIMEOUT_MS 4000u
#define ARM_CONTROLLER_DEFAULT_HOME_TIMEOUT_MS 1500u
#define ARM_CONTROLLER_DEFAULT_R_MAX_X10      2800u
#define ARM_CONTROLLER_DEFAULT_SHORT_DELTA_X10 300u
#define ARM_CONTROLLER_DEFAULT_RETURN_DELTA_X10 900u
#define ARM_CONTROLLER_DEFAULT_HOME_READY_X10 450u
#define ARM_CONTROLLER_DEFAULT_REFINE_SPEED   8u
#define ARM_CONTROLLER_SPEED_MAX              100u
#define ARM_CONTROLLER_GRIPPER_VALUE_MAX      100u

typedef enum {
    ARM_STATE_IDLE = 0,
    ARM_STATE_PRECHECK,
    ARM_STATE_PICK_HOVER,
    ARM_STATE_PICK_DOWN,
    ARM_STATE_GRIP_CLOSE,
    ARM_STATE_PICK_LIFT,
    ARM_STATE_DROP_HOVER,
    ARM_STATE_DROP_DOWN,
    ARM_STATE_GRIP_OPEN,
    ARM_STATE_DROP_LIFT,
    ARM_STATE_RETURN_HOME_READY,
    ARM_STATE_RETURN_HOME,
    ARM_STATE_POST_READBACK,
    ARM_STATE_RETRY_ONCE,
    ARM_STATE_DONE,
    ARM_STATE_FAULT,
    ARM_STATE_ESTOP
} arm_state_t;

typedef enum {
    ARM_ERR_NONE = 0,
    ARM_ERR_NO_PLAN,
    ARM_ERR_NO_TRANSPORT,
    ARM_ERR_PROTOCOL_TIMEOUT,
    ARM_ERR_BAD_FRAME,
    ARM_ERR_TARGET_INVALID,
    ARM_ERR_PRECHECK_FAILED,
    ARM_ERR_SOFT_TIMEOUT,
    ARM_ERR_SOFT_PASS_WARNING,
    ARM_ERR_POST_READ_FAILED,
    ARM_ERR_RETRY_FAILED,
    ARM_ERR_UNSAFE_RELEASE_REQUIRED,
    ARM_ERR_BUSY
} arm_error_t;

typedef enum {
    ARM_POINT_HOME = 0,
    ARM_POINT_HOME_READY,
    ARM_POINT_PICK_HOVER,
    ARM_POINT_PICK,
    ARM_POINT_DROP_HOVER,
    ARM_POINT_DROP,
    ARM_POINT_COUNT
} arm_point_id_t;

typedef struct {
    int16_t joint_deg_x10[ARM_CONTROLLER_JOINTS];
    int16_t coord_x10[ARM_CONTROLLER_JOINTS];
    uint16_t speed;
    uint16_t radius_mm_x10;
    uint8_t has_coord;
} arm_controller_point_t;

typedef struct {
    arm_controller_point_t points[ARM_POINT_COUNT];
    uint16_t gripper_open;
    uint16_t gripper_closed;
    uint16_t gripper_speed;
    uint16_t short_tol_deg_x10;
    uint16_t home_tol_deg_x10;
    uint16_t confirm_required;
    uint16_t poll_interval_ms;
    uint32_t move_timeout_ms;
    uint32_t home_timeout_ms;
    uint16_t r_max_mm_x10;
    uint16_t short_arm_joint_max_delta_x10;
    uint16_t return_arm_joint_max_delta_x10;
    uint16_t home_ready_arm_max_diff_x10;
    uint16_t refine_speed;
} arm_controller_plan_t;

typedef int (*arm_send_angles_fn)(void *user,
                                  const int16_t joint_deg_x10[ARM_CONTROLLER_JOINTS],
                                  uint16_t speed);
typedef int (*arm_read_angles_fn)(void *user,
                                  int16_t joint_deg_x10[ARM_CONTROLLER_JOINTS]);
typedef int (*arm_set_gripper_fn)(void *user,
                                  uint16_t gripper,
                                  uint16_t speed);

typedef struct {
    arm_send_angles_fn send_angles;
    arm_read_angles_fn read_angles;
    arm_set_gripper_fn set_gripper;
} arm_controller_ops_t;

typedef struct {
    uint8_t arm_id;
    arm_state_t state;
    arm_error_t error;
    const arm_controller_plan_t *plan;
    const arm_controller_ops_t *ops;
    void *user;
    arm_point_id_t active_point;
    uint16_t active_tol_deg_x10;
    uint32_t deadline_ms;
    uint32_t next_poll_ms;
    uint8_t move_sent;
    uint8_t confirm_count;
    uint16_t none_count;
    uint8_t retry_count;
    uint8_t move_state_snapshot;
} arm_controller_t;

void arm_controller_init(arm_controller_t *arm, uint8_t arm_id);
void arm_controller_configure(arm_controller_t *arm,
                              const arm_controller_plan_t *plan,
                              const arm_controller_ops_t *ops,
                              void *user);
int arm_controller_plan_validate(const arm_controller_plan_t *plan);
int arm_controller_request_grab(arm_controller_t *arm);
void arm_controller_tick(arm_controller_t *arm, uint32_t now_ms);
void arm_controller_cancel(arm_controller_t *arm, arm_error_t reason);
arm_state_t arm_controller_get_state(const arm_controller_t *arm);
arm_error_t arm_controller_get_error(const arm_controller_t *arm);
uint16_t arm_controller_get_none_count(const arm_controller_t *arm);

#ifdef __cplusplus
}
#endif

#endif /* ARM_CONTROLLER_H */
