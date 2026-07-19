#include "p0a_diag.h"

#define UART_DATA_OFFSET 0x00u
#define UART_STATUS_OFFSET 0x04u
#define UART_TX_AVAIL_SHIFT 16u
#define UART_TX_AVAIL_MASK 0xFFu

#if defined(__GNUC__)
#define P0A_CANARY_SECTION __attribute__((section(".p0a_canary"), used, aligned(16)))
#else
#define P0A_CANARY_SECTION
#endif

volatile p0a_canary_t g_p0a_canary P0A_CANARY_SECTION;

uint32_t p0a_canary_checksum(const volatile p0a_canary_t *canary)
{
    return canary->magic ^ canary->schema ^ canary->build_id ^
           canary->write_count ^ canary->stage ^ canary->last_pc_tag ^
           canary->uart_status ^ canary->error ^ canary->heartbeat ^
           0xA5A55A5Au;
}

static void canary_commit(void)
{
    g_p0a_canary.write_count++;
    g_p0a_canary.checksum = p0a_canary_checksum(&g_p0a_canary);
}

void p0a_canary_init(uint32_t pc_tag)
{
    g_p0a_canary.magic = P0A_CANARY_MAGIC;
    g_p0a_canary.schema = P0A_CANARY_SCHEMA;
    g_p0a_canary.build_id = P0A_BUILD_ID;
    g_p0a_canary.write_count = 0u;
    g_p0a_canary.stage = P0A_STAGE_C001;
    g_p0a_canary.last_pc_tag = pc_tag;
    g_p0a_canary.uart_status = P0A_UART_STATUS_NOT_PROBED;
    g_p0a_canary.error = P0A_ERROR_NONE;
    g_p0a_canary.heartbeat = 0u;
    canary_commit();
}

void p0a_canary_mark(uint32_t stage, uint32_t pc_tag)
{
    g_p0a_canary.stage = stage;
    g_p0a_canary.last_pc_tag = pc_tag;
    canary_commit();
}

void p0a_canary_set_uart(uint32_t status, uint32_t error, uint32_t pc_tag)
{
    g_p0a_canary.uart_status = status;
    g_p0a_canary.error = error;
    g_p0a_canary.last_pc_tag = pc_tag;
    canary_commit();
}

void p0a_canary_heartbeat(uint32_t pc_tag)
{
    g_p0a_canary.heartbeat++;
    g_p0a_canary.last_pc_tag = pc_tag;
    canary_commit();
}

int p0a_uart_putc_bounded(const p0a_uart_t *uart, uint8_t value)
{
    uint32_t remaining;

    if (uart == 0 || uart->read32 == 0 || uart->write32 == 0) return -1;
    remaining = uart->poll_budget;
    while (remaining != 0u) {
        const uint32_t status = uart->read32(
            uart->context, uart->uart_base + UART_STATUS_OFFSET);
        if (((status >> UART_TX_AVAIL_SHIFT) & UART_TX_AVAIL_MASK) != 0u) {
            uart->write32(uart->context, uart->uart_base + UART_DATA_OFFSET,
                          (uint32_t)value);
            return 0;
        }
        remaining--;
    }
    return -1;
}

int p0a_uart_write_bounded(const p0a_uart_t *uart, const char *text)
{
    if (text == 0) return -1;
    while (*text != '\0') {
        if (p0a_uart_putc_bounded(uart, (uint8_t)*text) != 0) return -1;
        text++;
    }
    return 0;
}
