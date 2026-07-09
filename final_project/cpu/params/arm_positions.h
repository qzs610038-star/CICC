#ifndef ARM_POSITIONS_H
#define ARM_POSITIONS_H

#include <stdint.h>

#include "arm_controller.h"

#define ARM_POSITIONS_DEFAULT_PRESET_NAME "teach_points_my_new_test"
#define ARM_POSITIONS_DEFAULT_PRESET_PATH "mycobot_pc_tests/presets/teach_points_my_new_test.json"
#define ARM_POSITIONS_FALLBACK_PRESET_PATH "mycobot_pc_tests/presets/teach_points_run12_3cm_inboard.json"

typedef struct {
    int16_t joint_deg_x10[6];
    uint16_t speed;
    uint16_t gripper;
} arm_position_t;

extern const arm_position_t g_arm_safe_position;
extern const arm_controller_plan_t g_arm_default_plan;

#endif
