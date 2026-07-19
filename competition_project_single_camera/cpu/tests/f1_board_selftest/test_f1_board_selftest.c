#include <stdio.h>
#include <string.h>

#include "f1_board_selftest.h"

typedef struct { unsigned count; unsigned fail_after; } sink_state_t;

static int sink(void *context, const char *line)
{
    sink_state_t *state = (sink_state_t *)context;
    if (line == 0 || strncmp(line, "@E|v=1|", 7u) != 0) return -1;
    state->count++;
    return state->fail_after != 0u && state->count >= state->fail_after ? -1 : 0;
}

int main(void)
{
    f1_board_selftest_report_t first;
    f1_board_selftest_report_t second;
    f1_board_selftest_report_t failed_sink;
    sink_state_t state = {0u, 0u};
    sink_state_t fail_state = {0u, 1u};
    int rc;

    rc = f1_board_selftest_run(sink, &state, &first);
    if (rc != 0 || first.canary != F1_BOARD_SELFTEST_CANARY ||
        first.cases_run != F1_BOARD_SELFTEST_CASES ||
        first.cases_passed != F1_BOARD_SELFTEST_CASES ||
        first.arm_enabled != 0u || first.arm_request_count != 0u ||
        first.arm_send_count != 0u || first.result_count != 4u) return 1;
    if (f1_board_selftest_run(0, 0, &second) != 0 || first.digest != second.digest ||
        first.cases_passed != second.cases_passed) return 2;
    rc = f1_board_selftest_run(sink, &fail_state, &failed_sink);
    if (rc != 1 || !failed_sink.sink_failed || failed_sink.canary != F1_BOARD_SELFTEST_CANARY ||
        failed_sink.digest == 2166136261u || failed_sink.arm_enabled != 0u) return 3;
    (void)printf("QW-F1-BOARD-SELFTEST-v1 PASS cases=%u results=%u events=%u digest=%08X arm=0\n",
                 first.cases_passed, first.result_count, first.event_count, first.digest);
    (void)printf("NEGATIVE sink_failure=PASS duplicate=PASS old_frame=PASS bad_flags=PASS ack_failure=PASS timeout=PASS\n");
    return 0;
}
