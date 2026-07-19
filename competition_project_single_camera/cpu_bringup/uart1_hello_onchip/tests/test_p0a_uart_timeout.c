#include <stdint.h>
#include <stdio.h>

#include "p0a_diag.h"

typedef struct { uint32_t reads; uint32_t writes; } mock_uart_t;
static int checks;
static int failures;
#define CHECK(expr) do { checks++; if (!(expr)) { failures++; \
    printf("FAIL line=%d expr=%s\n", __LINE__, #expr); } } while (0)

static uint32_t never_ready_read(void *context, uint32_t address)
{
    mock_uart_t *mock = (mock_uart_t *)context;
    (void)address;
    mock->reads++;
    return 0u;
}

static void capture_write(void *context, uint32_t address, uint32_t value)
{
    mock_uart_t *mock = (mock_uart_t *)context;
    (void)address;
    (void)value;
    mock->writes++;
}

int main(void)
{
    mock_uart_t mock = {0u, 0u};
    const p0a_uart_t uart = {
        never_ready_read, capture_write, &mock, 0x1000u, 7u
    };

    p0a_canary_init(1u);
    p0a_canary_mark(P0A_STAGE_C004, 2u);
    CHECK(p0a_uart_write_bounded(&uart, "X") == -1);
    CHECK(mock.reads == 7u);
    CHECK(mock.writes == 0u);
    p0a_canary_mark(P0A_STAGE_E101, 3u);
    p0a_canary_set_uart(P0A_UART_STATUS_TX_TIMEOUT,
                        P0A_ERROR_UART_TX_TIMEOUT, 4u);
    p0a_canary_mark(P0A_STAGE_C0FF, 5u);
    p0a_canary_heartbeat(6u);
    CHECK(g_p0a_canary.stage == P0A_STAGE_C0FF);
    CHECK(g_p0a_canary.error == P0A_ERROR_UART_TX_TIMEOUT);
    CHECK(g_p0a_canary.heartbeat == 1u);
    CHECK(g_p0a_canary.checksum == p0a_canary_checksum(&g_p0a_canary));
    printf("p0a_uart_timeout: %d/%d passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
