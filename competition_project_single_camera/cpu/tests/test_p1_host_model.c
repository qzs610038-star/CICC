#include <stdio.h>
#include <string.h>

#include "p1_host_model.h"

static int checks;
static int failures;
#define CHECK(expr) do { checks++; if (!(expr)) { failures++; \
    printf("FAIL line=%d expr=%s\n", __LINE__, #expr); } } while (0)

static void test_result_staging_commit(void)
{
    p1_result_packer_t packer;
    p1_result_t result = {1u, 9u, 3u, 2u, 1u, 0u, 1u, 1u, 0u, 0x47u, 0u};

    p1_result_packer_init(&packer);
    CHECK(p1_result_stage(&packer, &result) == 0);
    CHECK(packer.has_active == 0u);
    CHECK(p1_result_commit(&packer, 1u) == 0);
    CHECK(packer.active_round_id == 1u);
    CHECK((packer.active[3] & 0xFFu) == 0u);
    CHECK(p1_result_commit(&packer, 1u) == -1);
    result.round_id = 0u;
    CHECK(p1_result_stage(&packer, &result) == 0);
    CHECK(p1_result_commit(&packer, 0u) == -1);
    result.round_id = 2u;
    result.arm_enabled = 1u;
    CHECK(p1_result_stage(&packer, &result) == -1);
    result.arm_enabled = 0u;
    result.round_id = 0u;
    packer.active_round_id = 0xFFFFu;
    CHECK(p1_result_stage(&packer, &result) == 0);
    CHECK(p1_result_commit(&packer, 0u) == 0);
}

static void test_input_events(void)
{
    p1_input_model_t model;
    p1_config_t config = {2u, 3u, 20u};
    uint16_t ack = 0u;

    p1_input_init(&model);
    p1_input_stage_config(&model, &config);
    CHECK(p1_input_event(&model, P1_INPUT_APPLY, 0xFFFEu, &ack) == P1_EVENT_ACCEPTED);
    CHECK(ack == 0xFFFEu && model.config_active == 1u);
    CHECK(p1_input_event(&model, P1_INPUT_PLACE, 0xFFFFu, &ack) == P1_EVENT_ACCEPTED);
    CHECK(model.object_present == 1u && model.round_id == 1u);
    CHECK(p1_input_event(&model, P1_INPUT_PLACE, 0xFFFFu, &ack) == P1_EVENT_DUPLICATE);
    CHECK(model.round_id == 1u);
    CHECK(p1_input_event(&model, P1_INPUT_REMOVE, 0xFFFDu, &ack) == P1_EVENT_STALE);
    CHECK(model.object_present == 1u);
    CHECK(p1_input_event(&model, P1_INPUT_REMOVE, 0u, &ack) == P1_EVENT_ACCEPTED);
    CHECK(model.object_present == 0u);
    CHECK(p1_input_event(&model, P1_INPUT_PLACE, 1u, &ack) == P1_EVENT_ACCEPTED);
    CHECK(p1_input_event(&model, P1_INPUT_ABANDON, 2u, &ack) == P1_EVENT_ACCEPTED);
    CHECK(model.object_present == 0u && model.round_id == 2u);
    CHECK(p1_input_event(&model, P1_INPUT_RESET, 3u, &ack) == P1_EVENT_ACCEPTED);
    CHECK(model.config_active == 0u && model.result_latched == 0u);
    CHECK(p1_input_event(&model, P1_INPUT_PLACE, 4u, &ack) == P1_EVENT_INVALID_STATE);
    CHECK(model.last_event_seq == 3u);
}

int main(void)
{
    test_result_staging_commit();
    test_input_events();
    printf("p1_host_model: %d/%d passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
