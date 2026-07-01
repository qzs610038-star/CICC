#include <stdint.h>

typedef struct {
    uint8_t arm_id;
    uint8_t state;
} arm_controller_t;

void arm_controller_init(arm_controller_t *arm, uint8_t arm_id)
{
    if (!arm) {
        return;
    }
    arm->arm_id = arm_id;
    arm->state = 0;
}
