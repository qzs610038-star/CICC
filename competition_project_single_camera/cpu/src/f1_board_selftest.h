#ifndef F1_BOARD_SELFTEST_H
#define F1_BOARD_SELFTEST_H

#include <stdint.h>

typedef int (*f1_selftest_event_sink_t)(void *context, const char *line);

typedef struct {
    uint32_t canary;
    uint32_t digest;
    uint16_t cases_run;
    uint16_t cases_passed;
    uint16_t event_count;
    uint16_t result_count;
    uint16_t ack_count;
    uint8_t arm_enabled;
    uint8_t arm_request_count;
    uint8_t arm_send_count;
    uint8_t sink_failed;
} f1_board_selftest_report_t;

#define F1_BOARD_SELFTEST_CANARY 0x51314631u
#define F1_BOARD_SELFTEST_CASES 8u

int f1_board_selftest_run(f1_selftest_event_sink_t sink, void *sink_context,
                          f1_board_selftest_report_t *report);

#endif
