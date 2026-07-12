/* Host tests for the register-independent integration contract. */
#include <stdio.h>
#include <string.h>
#include "competition_contract.h"

static int failures;
static int checks;
#define CHECK(expr) do { checks++; if (!(expr)) { failures++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } } while (0)

static vision_result_t make_obs(uint8_t color, uint8_t shape, uint8_t size)
{
    vision_result_t value;
    memset(&value, 0, sizeof(value));
    value.color_id = color;
    value.shape_id = shape;
    value.size_cm_x10 = size;
    return value;
}

static target_config_t make_config(competition_task_mode_t mode, uint8_t color,
                                   uint8_t reference_size)
{
    target_config_t config;
    memset(&config, 0, sizeof(config));
    config.valid = 1u;
    config.target.mode = mode;
    config.target.target_color = color;
    config.target.reference_size_cm_x10 = reference_size;
    return config;
}

static void test_apply_lock_ack_remove(void)
{
    competition_contract_t contract;
    target_config_t red = make_config(COMP_TASK_COLOR_CUBE, COLOR_RED, 0);
    target_config_t blue = make_config(COMP_TASK_COLOR_CUBE, COLOR_BLUE, 0);
    operator_event_t place = { COMP_EVENT_PLACE, 1u };
    operator_event_t remove = { COMP_EVENT_REMOVE, 2u };
    vision_result_t red_cube = make_obs(COLOR_RED, SHAPE_CUBE, 0);

    competition_contract_init(&contract, SIZE_STATE_UNAVAILABLE);
    CHECK(competition_contract_stage_target(&contract, &red) == 0);
    CHECK(competition_contract_apply_target(&contract) == 0);
    CHECK(competition_contract_stage_target(&contract, &blue) == 0);
    CHECK(contract.active.target.target_color == COLOR_RED);
    CHECK(competition_contract_handle_event(&contract, &place, 0u, 50u) == 0);
    CHECK(competition_contract_handle_event(&contract, &place, 1u, 50u) == 0);
    CHECK(contract.transaction.event_seq == 1u &&
          contract.transaction.state == COMP_ROUND_TXN_STATE_WAIT_OBSERVATION);
    CHECK(competition_contract_observe(&contract, &red_cube, 2u) == 0);
    CHECK(contract.result.decision == COMP_DECISION_EXECUTE);
    CHECK(contract.result.reason == COMP_REASON_TARGET_MATCH);
    CHECK(contract.result.size_state == SIZE_STATE_UNAVAILABLE && contract.result.size_cm_x10 == 0u);
    CHECK(competition_contract_ack_result(&contract, 2u) == -1);
    CHECK(competition_contract_ack_result(&contract, 1u) == 0);
    CHECK(contract.transaction.state == COMP_ROUND_TXN_STATE_COMPLETE);
    CHECK(competition_contract_handle_event(&contract, &remove, 3u, 50u) == 0);
    CHECK(contract.transaction.state == COMP_ROUND_TXN_STATE_IDLE);
}

static void test_size_unavailable_and_events(void)
{
    competition_contract_t contract;
    target_config_t size_task = make_config(COMP_TASK_SIZE_DELTA_1CM_CUBE, 0, 20);
    operator_event_t place = { COMP_EVENT_PLACE, 1u };
    operator_event_t abandon = { COMP_EVENT_ABANDON, 2u };
    operator_event_t reset = { COMP_EVENT_RESET, 3u };
    vision_result_t cube = make_obs(COLOR_RED, SHAPE_CUBE, 30);

    competition_contract_init(&contract, SIZE_STATE_UNAVAILABLE);
    CHECK(competition_contract_stage_target(&contract, &size_task) == 0);
    CHECK(competition_contract_apply_target(&contract) == 0);
    CHECK(competition_contract_handle_event(&contract, &place, 0u, 20u) == 0);
    CHECK(competition_contract_observe(&contract, &cube, 1u) == 0);
    CHECK(contract.result.decision == COMP_DECISION_WAIT);
    CHECK(contract.result.reason == COMP_REASON_SIZE_UNAVAILABLE);
    CHECK(contract.transaction.state == COMP_ROUND_TXN_STATE_WAIT_OBSERVATION);
    CHECK(competition_contract_handle_event(&contract, &abandon, 2u, 20u) == 0);
    CHECK(contract.result.decision == COMP_DECISION_SKIP);
    CHECK(contract.result.reason == COMP_REASON_OPERATOR_ABANDONED);
    CHECK(competition_contract_handle_event(&contract, &reset, 3u, 20u) == 0);
    CHECK(!contract.active.valid &&
          contract.transaction.state == COMP_ROUND_TXN_STATE_IDLE);
}

static void test_timeout_and_stale_sequence(void)
{
    competition_contract_t contract;
    target_config_t red = make_config(COMP_TASK_COLOR_CUBE, COLOR_RED, 0);
    operator_event_t place = { COMP_EVENT_PLACE, 8u };
    operator_event_t stale = { COMP_EVENT_RESET, 7u };

    competition_contract_init(&contract, SIZE_STATE_UNAVAILABLE);
    CHECK(competition_contract_stage_target(&contract, &red) == 0);
    CHECK(competition_contract_apply_target(&contract) == 0);
    CHECK(competition_contract_handle_event(&contract, &place, 100u, 10u) == 0);
    CHECK(competition_contract_handle_event(&contract, &stale, 101u, 10u) == -1);
    CHECK(competition_contract_tick(&contract, 110u) == 1);
    CHECK(contract.result.reason == COMP_REASON_ROUND_TIMEOUT);
    CHECK(contract.transaction.state == COMP_ROUND_TXN_STATE_TIMEOUT);
}

int main(void)
{
    test_apply_lock_ack_remove();
    test_size_unavailable_and_events();
    test_timeout_and_stale_sequence();
    printf("competition_contract: %d/%d passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
