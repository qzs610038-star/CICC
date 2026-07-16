#include <stdint.h>

#include "soc.h"

#define UART_DATA_OFFSET          0x00U
#define UART_STATUS_OFFSET        0x04U
#define UART_CLOCK_DIVIDER_OFFSET 0x08U
#define UART_FRAME_CONFIG_OFFSET  0x0CU
#define UART_DATA_BITS            8U
#define UART_BAUDRATE             115200U

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

int main(void)
{
    uart_init();

    uart_puts("\r\nTJ375 CPU+VIDEO UART0 HELLO\r\n");
    uart_puts("ONCHIP_RAM=0xF9000000 UART0=115200 8N1\r\n");
    uart_puts("Type characters to verify echo.\r\n");

    for (;;) {
        while ((mmio_read32(SYSTEM_UART_0_IO_CTRL + UART_STATUS_OFFSET) >> 24U)
               != 0U) {
            const uint8_t value = (uint8_t)mmio_read32(
                SYSTEM_UART_0_IO_CTRL + UART_DATA_OFFSET);
            uart_putc(value);
        }
    }
}
