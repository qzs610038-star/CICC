#include <stdint.h>

#include "soc.h"

#define APB_MAGIC_EXPECTED 0x375A0001U
#define APB_PROBE_PENDING  0x00000000U
#define APB_PROBE_PASS     0x50415353U
#define APB_PROBE_FAIL     0x4641494CU

volatile uint32_t g_apb_probe_address = IO_APB_SLAVE_0_INPUT;
volatile uint32_t g_apb_probe_expected = APB_MAGIC_EXPECTED;
volatile uint32_t g_apb_probe_observed;
volatile uint32_t g_apb_probe_status = APB_PROBE_PENDING;

int main(void)
{
    const volatile uint32_t *const magic_register =
        (const volatile uint32_t *)(uintptr_t)IO_APB_SLAVE_0_INPUT;
    const uint32_t observed = *magic_register;

    g_apb_probe_observed = observed;
    g_apb_probe_status =
        (observed == APB_MAGIC_EXPECTED) ? APB_PROBE_PASS : APB_PROBE_FAIL;

    for (;;) {
        __asm__ volatile ("" ::: "memory");
    }
}
