#include "mycobot_transaction.h"

static uint8_t deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (uint8_t)((int32_t)(now_ms - deadline_ms) >= 0);
}

void mycobot_transaction_init(mycobot_transaction_t *transaction)
{
    if (!transaction) {
        return;
    }

    transaction->active = 0u;
    transaction->expected_command = 0u;
    transaction->expected_payload_len = 0u;
    transaction->deadline_ms = 0u;
    transaction->counters.accepted = 0u;
    transaction->counters.bad_command = 0u;
    transaction->counters.bad_length = 0u;
    transaction->counters.bad_domain = 0u;
    transaction->counters.late_or_duplicate = 0u;
    transaction->counters.timeout = 0u;
    transaction->counters.single_flight_reject = 0u;
}

uint8_t mycobot_transaction_begin(mycobot_transaction_t *transaction,
                                  uint8_t expected_command,
                                  uint32_t now_ms)
{
    uint8_t payload_len;

    if (!transaction ||
        !mycobot_command_expected_response_payload_len(expected_command, &payload_len)) {
        return 0u;
    }

    if (transaction->active) {
        transaction->counters.single_flight_reject++;
        return 0u;
    }

    transaction->active = 1u;
    transaction->expected_command = expected_command;
    transaction->expected_payload_len = payload_len;
    transaction->deadline_ms = now_ms + MYCOBOT_TRANSACTION_TIMEOUT_MS;
    return 1u;
}

uint8_t mycobot_transaction_expire(mycobot_transaction_t *transaction,
                                   uint32_t now_ms)
{
    if (!transaction || !transaction->active ||
        !deadline_reached(now_ms, transaction->deadline_ms)) {
        return 0u;
    }

    transaction->active = 0u;
    transaction->counters.timeout++;
    return 1u;
}

mycobot_transaction_status_t mycobot_transaction_accept_frame(
    mycobot_transaction_t *transaction,
    const mycobot_frame_t *frame,
    uint32_t now_ms)
{
    if (!transaction || !frame) {
        return MYCOBOT_TRANSACTION_BAD_DOMAIN;
    }

    if (!transaction->active) {
        transaction->counters.late_or_duplicate++;
        return MYCOBOT_TRANSACTION_IDLE;
    }

    if (mycobot_transaction_expire(transaction, now_ms)) {
        return MYCOBOT_TRANSACTION_TIMEOUT;
    }

    if (!mycobot_wire_len_is_valid((uint8_t)(frame->payload_len + 2u)) ||
        frame->payload_len != transaction->expected_payload_len) {
        transaction->counters.bad_length++;
        return MYCOBOT_TRANSACTION_BAD_LENGTH;
    }

    if (frame->command != transaction->expected_command) {
        transaction->counters.bad_command++;
        return MYCOBOT_TRANSACTION_BAD_COMMAND;
    }

    if (mycobot_validate_response_payload(frame->command,
                                          frame->payload,
                                          frame->payload_len) != MYCOBOT_HELPER_OK) {
        transaction->counters.bad_domain++;
        return MYCOBOT_TRANSACTION_BAD_DOMAIN;
    }

    transaction->active = 0u;
    transaction->counters.accepted++;
    return MYCOBOT_TRANSACTION_ACCEPTED;
}

uint8_t mycobot_transaction_active(const mycobot_transaction_t *transaction)
{
    return transaction ? transaction->active : 0u;
}

const mycobot_transaction_counters_t *mycobot_transaction_get_counters(
    const mycobot_transaction_t *transaction)
{
    return transaction ? &transaction->counters : 0;
}
