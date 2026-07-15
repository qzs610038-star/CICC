#ifndef MYCOBOT_TRANSACTION_H
#define MYCOBOT_TRANSACTION_H

#include <stdint.h>

#include "mycobot_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MYCOBOT_TRANSACTION_TIMEOUT_MS 750u

typedef enum {
    MYCOBOT_TRANSACTION_ACCEPTED = 0,
    MYCOBOT_TRANSACTION_IDLE,
    MYCOBOT_TRANSACTION_BUSY,
    MYCOBOT_TRANSACTION_BAD_COMMAND,
    MYCOBOT_TRANSACTION_BAD_LENGTH,
    MYCOBOT_TRANSACTION_BAD_DOMAIN,
    MYCOBOT_TRANSACTION_TIMEOUT
} mycobot_transaction_status_t;

typedef struct {
    uint32_t accepted;
    uint32_t bad_command;
    uint32_t bad_length;
    uint32_t bad_domain;
    uint32_t late_or_duplicate;
    uint32_t timeout;
    uint32_t single_flight_reject;
} mycobot_transaction_counters_t;

typedef struct {
    uint8_t active;
    uint8_t expected_command;
    uint8_t expected_payload_len;
    uint32_t deadline_ms;
    mycobot_transaction_counters_t counters;
} mycobot_transaction_t;

void mycobot_transaction_init(mycobot_transaction_t *transaction);
uint8_t mycobot_transaction_begin(mycobot_transaction_t *transaction,
                                  uint8_t expected_command,
                                  uint32_t now_ms);
uint8_t mycobot_transaction_expire(mycobot_transaction_t *transaction,
                                   uint32_t now_ms);
mycobot_transaction_status_t mycobot_transaction_accept_frame(
    mycobot_transaction_t *transaction,
    const mycobot_frame_t *frame,
    uint32_t now_ms);
uint8_t mycobot_transaction_active(const mycobot_transaction_t *transaction);
const mycobot_transaction_counters_t *mycobot_transaction_get_counters(
    const mycobot_transaction_t *transaction);

#ifdef __cplusplus
}
#endif

#endif /* MYCOBOT_TRANSACTION_H */
