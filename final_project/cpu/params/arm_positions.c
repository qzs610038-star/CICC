#include "arm_positions.h"

/*
 * Source preset: mycobot_pc_tests/presets/teach_points_my_new_test.json
 * Fallback reference: mycobot_pc_tests/presets/teach_points_run12_3cm_inboard.json
 *
 * Units:
 * - joint_deg_x10: degrees * 10, rounded from the PC teach preset
 * - coord_x10: millimeters/degrees * 10, rounded from the PC teach preset
 * - radius_mm_x10: millimeters * 10
 *
 * This file is a no-motion parameter table. It is not wired to main.c,
 * UART, MMIO, or any real myCobot transport path.
 */

const arm_position_t g_arm_safe_position = {
    {0, 0, 0, 0, 0, 0},
    30u,
    80u
};

const arm_controller_plan_t g_arm_default_plan = {
    {
        {{0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, 30u, 0u, 0u},
        {{-38, -244, -93, -29, -1, 389}, {1758, -752, 3527, -1337, 302, -1196}, 20u, 1912u, 1u},
        {{439, -364, -691, 169, 12, 941}, {2120, 1173, 1588, 1789, 12, -1402}, 16u, 2423u, 1u},
        {{438, -600, -691, 374, -10, 925}, {2203, 1226, 862, -1789, -16, -1387}, 12u, 2521u, 1u},
        {{12, -409, -692, 219, -68, 545}, {2402, -636, 1465, -1734, -26, -1435}, 16u, 2485u, 1u},
        {{10, -611, -693, 420, -47, 511}, {2476, -621, 878, -1750, -13, -1401}, 12u, 2553u, 1u}
    },
    80u,
    20u,
    50u,
    ARM_CONTROLLER_DEFAULT_SHORT_TOL_X10,
    ARM_CONTROLLER_DEFAULT_HOME_TOL_X10,
    ARM_CONTROLLER_DEFAULT_CONFIRM,
    ARM_CONTROLLER_DEFAULT_POLL_MS,
    ARM_CONTROLLER_DEFAULT_MOVE_TIMEOUT_MS,
    ARM_CONTROLLER_DEFAULT_HOME_TIMEOUT_MS,
    2800u,
    ARM_CONTROLLER_DEFAULT_SHORT_DELTA_X10,
    ARM_CONTROLLER_DEFAULT_RETURN_DELTA_X10,
    ARM_CONTROLLER_DEFAULT_HOME_READY_X10,
    ARM_CONTROLLER_DEFAULT_REFINE_SPEED
};
