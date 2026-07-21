#include <stdint.h>

#include "soc.h"

#define UART_DATA_OFFSET 0x00U
#define UART_STATUS_OFFSET 0x04U
#define UART_CLOCK_DIVIDER_OFFSET 0x08U
#define UART_FRAME_CONFIG_OFFSET 0x0CU

static uint32_t mmio_read32(uint32_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}

static void mmio_write32(uint32_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
}

static void uart1_init(void)
{
    const uint32_t sample_per_bit =
        SYSTEM_UART_1_IO_PARAMETER_UART_CTRL_CONFIG_RX_SAMPLE_PER_BIT;
    const uint32_t divider =
        (SYSTEM_CLINT_HZ /
         (SYSTEM_UART_1_IO_PARAMETER_INIT_CONFIG_BAUDRATE * sample_per_bit)) -
        1U;

    mmio_write32(SYSTEM_UART_1_IO_CTRL + UART_CLOCK_DIVIDER_OFFSET, divider);
    mmio_write32(SYSTEM_UART_1_IO_CTRL + UART_FRAME_CONFIG_OFFSET,
                 SYSTEM_UART_1_IO_PARAMETER_INIT_CONFIG_DATA_LENGTH);
}

static void uart1_putc(uint8_t value)
{
    while (((mmio_read32(SYSTEM_UART_1_IO_CTRL + UART_STATUS_OFFSET) >> 16U)
            & 0xFFU) == 0U) {
    }

    mmio_write32(SYSTEM_UART_1_IO_CTRL + UART_DATA_OFFSET, value);
}

static void uart1_puts(const char *text)
{
    while (*text != '\0') {
        uart1_putc((uint8_t)*text);
        ++text;
    }
}

int main(void)
{
    uart1_init();
    uart1_puts("\r\nI0 UART1 HELLO\r\n");
    uart1_puts("UART1=115200 8N1 RX=GPIOR_96 TX=GPIOR_100\r\n");
    uart1_puts("Type characters to verify echo.\r\n");

    for (;;) {
        while ((mmio_read32(SYSTEM_UART_1_IO_CTRL + UART_STATUS_OFFSET) >> 24U)
               != 0U) {
            uart1_putc((uint8_t)mmio_read32(
                SYSTEM_UART_1_IO_CTRL + UART_DATA_OFFSET));
        }
    }
}
