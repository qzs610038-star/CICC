#include <stdint.h>

#include "f1_board_selftest.h"

volatile f1_board_selftest_report_t g_f1_selftest_profile_report;
volatile uint32_t g_f1_selftest_profile_status;

void _start(void)
{
    g_f1_selftest_profile_status = 0x53544152u;
    g_f1_selftest_profile_status =
        (uint32_t)f1_board_selftest_run(0, 0,
            (f1_board_selftest_report_t *)&g_f1_selftest_profile_report);
    for (;;) { }
}
