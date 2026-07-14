#include "arm_sim_transport.h"

#include <string.h>

static void copy_target_with_offset(arm_sim_transport_t *sim, uint8_t offset)
{
    uint8_t i;

    for (i = 0u; i < ARM_CONTROLLER_JOINTS; ++i) {
        sim->current[i] = (int16_t)(sim->target[i] + (offset ? 300 : 0));
    }
}

static int sim_send_angles(void *user,
                           const int16_t joint_deg_x10[ARM_CONTROLLER_JOINTS],
                           uint16_t speed)
{
    arm_sim_transport_t *sim = (arm_sim_transport_t *)user;
    uint8_t mismatch;

    if (!sim || !joint_deg_x10) {
        return -1;
    }

    memcpy(sim->target, joint_deg_x10, sizeof(sim->target));
    mismatch = sim->mismatch_always ||
               (sim->mismatch_first_send_only && sim->send_count == 0u);
    sim->mismatch_active = mismatch;
    copy_target_with_offset(sim, mismatch);
    sim->last_speed = speed;
    if (sim->send_count == 1u) {
        sim->retry_speed = speed;
    }
    sim->send_count++;
    return 0;
}

static int sim_read_angles(void *user,
                           int16_t joint_deg_x10[ARM_CONTROLLER_JOINTS])
{
    arm_sim_transport_t *sim = (arm_sim_transport_t *)user;

    if (!sim || !joint_deg_x10) {
        return -1;
    }

    sim->read_count++;
    if (sim->read_fail_always) {
        return -1;
    }
    if (sim->read_failures_remaining != 0u) {
        sim->read_failures_remaining--;
        return -1;
    }
    if (sim->settle_reads_remaining != 0u) {
        sim->settle_reads_remaining--;
        copy_target_with_offset(sim, 1u);
    } else if (!sim->mismatch_active) {
        copy_target_with_offset(sim, 0u);
    }
    memcpy(joint_deg_x10, sim->current, sizeof(sim->current));
    return 0;
}

static int sim_set_gripper(void *user, uint16_t gripper, uint16_t speed)
{
    arm_sim_transport_t *sim = (arm_sim_transport_t *)user;

    if (!sim) {
        return -1;
    }
    sim->gripper = gripper;
    sim->last_speed = speed;
    sim->gripper_count++;
    return 0;
}

static const arm_controller_ops_t g_arm_sim_ops = {
    sim_send_angles,
    sim_read_angles,
    sim_set_gripper
};

void arm_sim_transport_init(arm_sim_transport_t *sim)
{
    if (!sim) {
        return;
    }
    memset(sim, 0, sizeof(*sim));
}

void arm_sim_transport_set_scenario(arm_sim_transport_t *sim,
                                    arm_sim_scenario_t scenario)
{
    if (!sim) {
        return;
    }

    arm_sim_transport_init(sim);
    switch (scenario) {
    case ARM_SIM_SCENARIO_READ_FAILURE:
        sim->read_fail_always = 1u;
        break;
    case ARM_SIM_SCENARIO_SOFT_PASS:
        /* Three failed polls make the first move time out; the post-readback
         * then sees the target and exercises the soft-pass branch. */
        sim->read_failures_remaining = 3u;
        break;
    case ARM_SIM_SCENARIO_RETRY_SUCCESS:
        sim->mismatch_first_send_only = 1u;
        break;
    case ARM_SIM_SCENARIO_RETRY_FAILURE:
        sim->mismatch_always = 1u;
        break;
    case ARM_SIM_SCENARIO_HAPPY:
    default:
        break;
    }
}

void arm_sim_transport_set_settle_reads(arm_sim_transport_t *sim,
                                        uint8_t reads)
{
    if (sim) {
        sim->settle_reads_remaining = reads;
    }
}

const arm_controller_ops_t *arm_sim_transport_ops(void)
{
    return &g_arm_sim_ops;
}
