#include <stdint.h>

#include "soc.h"

#ifndef BUILD_ID
#error "BUILD_ID must be defined via -DBUILD_ID=\"...\" in the build command"
#endif

#define UART_DATA_OFFSET          0x00U
#define UART_STATUS_OFFSET        0x04U
#define UART_CLOCK_DIVIDER_OFFSET 0x08U
#define UART_FRAME_CONFIG_OFFSET  0x0CU
#define UART_DATA_BITS            8U
#define UART_BAUDRATE             115200U

/* Non-precision activity marker. The loop count is an arbitrary large
   value; the actual period depends on CPU clock, MMIO latency, and
   compiler output.  This marker only helps observe UART liveness — it
   is NOT a CPU precision timer or an independent liveness proof. */
#define ACTIVITY_LOOP_COUNT       50000000U

static uint32_t mmio_read32(uint32_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}

static void mmio_write32(uint32_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
}

static void uart_init(void)
{
    const uint32_t divider =
        (SYSTEM_CLINT_HZ / (UART_BAUDRATE * UART_DATA_BITS)) - 1U;

    mmio_write32(SYSTEM_UART_0_IO_CTRL + UART_CLOCK_DIVIDER_OFFSET, divider);
    mmio_write32(SYSTEM_UART_0_IO_CTRL + UART_FRAME_CONFIG_OFFSET,
                 UART_DATA_BITS - 1U);
}

static void uart_putc(uint8_t value)
{
    while (((mmio_read32(SYSTEM_UART_0_IO_CTRL + UART_STATUS_OFFSET) >> 16U)
            & 0xFFU) == 0U) {
    }

    mmio_write32(SYSTEM_UART_0_IO_CTRL + UART_DATA_OFFSET, value);
}

static void uart_puts(const char *text)
{
    while (*text != '\0') {
        uart_putc((uint8_t)*text);
        ++text;
    }
}

static int32_t uart_getc(void)
{
    if ((mmio_read32(SYSTEM_UART_0_IO_CTRL + UART_STATUS_OFFSET) >> 24U) != 0U) {
        return (int32_t)(uint8_t)mmio_read32(
            SYSTEM_UART_0_IO_CTRL + UART_DATA_OFFSET);
    }
    return -1;
}

int main(void)
{
    uint32_t activity = 0U;

    uart_init();

    uart_puts("\r\nTJ375 CPU+VIDEO UART0 HELLO\r\n");
    uart_puts("PROFILE=UART0_HELLO_ONCHIP BACKEND=NONE\r\n");
    uart_puts("BUILD_ID=" BUILD_ID "\r\n");
    uart_puts("Type characters to verify echo.\r\n");

    for (;;) {
        const int32_t ch = uart_getc();
        if (ch >= 0) {
            uart_putc((uint8_t)ch);
        }

        ++activity;
        if (activity >= ACTIVITY_LOOP_COUNT) {
            activity = 0U;
            uart_putc('.');
        }
    }
}
