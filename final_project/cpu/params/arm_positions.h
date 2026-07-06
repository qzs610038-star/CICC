#ifndef ARM_POSITIONS_H
#define ARM_POSITIONS_H

#include <stdint.h>

typedef struct {
    int16_t joint_deg_x10[6];
    uint16_t speed;
    uint16_t gripper;
} arm_position_t;

extern const arm_position_t g_arm_safe_position;

#endif
