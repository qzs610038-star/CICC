#include <stdint.h>

#include "p0a_diag.h"
#include "soc.h"

#define UART_CLOCK_DIVIDER_OFFSET 0x08u
#define UART_FRAME_CONFIG_OFFSET 0x0Cu

static uint32_t mmio_read32(void *context, uint32_t address)
{
    (void)context;
    return *(volatile uint32_t *)(uintptr_t)address;
}

static void mmio_write32(void *context, uint32_t address, uint32_t value)
{
    (void)context;
    *(volatile uint32_t *)(uintptr_t)address = value;
}

static void uart1_init(void)
{
    const uint32_t sample_per_bit =
        SYSTEM_UART_1_IO_PARAMETER_UART_CTRL_CONFIG_RX_SAMPLE_PER_BIT;
    const uint32_t divider =
        (SYSTEM_CLINT_HZ /
         (SYSTEM_UART_1_IO_PARAMETER_INIT_CONFIG_BAUDRATE * sample_per_bit)) -
        1u;

    mmio_write32(0, SYSTEM_UART_1_IO_CTRL + UART_CLOCK_DIVIDER_OFFSET, divider);
    mmio_write32(0, SYSTEM_UART_1_IO_CTRL + UART_FRAME_CONFIG_OFFSET,
                 SYSTEM_UART_1_IO_PARAMETER_INIT_CONFIG_DATA_LENGTH);
}

int main(void)
{
    const p0a_uart_t uart = {
        mmio_read32, mmio_write32, 0, SYSTEM_UART_1_IO_CTRL,
        P0A_UART_POLL_BUDGET
    };

    p0a_canary_init(0x0001u);
    p0a_canary_mark(P0A_STAGE_C002, 0x0002u);
    uart1_init();
    p0a_canary_mark(P0A_STAGE_C003, 0x0003u);

    /* Canary stages precede every potentially blocking UART operation. */
    p0a_canary_mark(P0A_STAGE_C004, 0x0004u);
    if (p0a_uart_write_bounded(&uart,
            "\r\nP0-A UART1 DIAG\r\nARM=0 BOARD=NOT_VERIFIED\r\n") != 0) {
        p0a_canary_mark(P0A_STAGE_E101, 0x0101u);
        p0a_canary_set_uart(P0A_UART_STATUS_TX_TIMEOUT,
                            P0A_ERROR_UART_TX_TIMEOUT, 0x0102u);
    } else {
        p0a_canary_set_uart(P0A_UART_STATUS_TX_READY, P0A_ERROR_NONE,
                            0x0005u);
        p0a_canary_mark(P0A_STAGE_C005, 0x0006u);
    }

    p0a_canary_mark(P0A_STAGE_C0FF, 0x00FFu);
    for (;;) p0a_canary_heartbeat(0x0F00u);
}
