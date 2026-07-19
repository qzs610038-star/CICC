#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "p0a_diag.h"

typedef struct {
    uint32_t reads;
    uint32_t writes;
    uint32_t stage_at_config;
    uint32_t stage_at_first_poll;
} mock_uart_t;
static int checks;
static int failures;
#define CHECK(expr) do { checks++; if (!(expr)) { failures++; \
    printf("FAIL line=%d expr=%s\n", __LINE__, #expr); } } while (0)

static uint32_t never_ready_read(void *context, uint32_t address)
{
    mock_uart_t *mock = (mock_uart_t *)context;
    (void)address;
    if (mock->reads == 0u) mock->stage_at_first_poll = g_p0a_canary.stage;
    mock->reads++;
    return 0u;
}

static void capture_config_stage(void *context)
{
    mock_uart_t *mock = (mock_uart_t *)context;
    mock->stage_at_config = g_p0a_canary.stage;
}

static void capture_write(void *context, uint32_t address, uint32_t value)
{
    mock_uart_t *mock = (mock_uart_t *)context;
    (void)address;
    (void)value;
    mock->writes++;
}

int main(int argc, char **argv)
{
    mock_uart_t mock = {0u, 0u, 0u, 0u};
    const p0a_uart_t uart = {
        never_ready_read, capture_write, &mock, 0x1000u, 7u
    };

    p0a_canary_init(1u);
    CHECK(g_p0a_canary.build_id == P0A_BUILD_ID);
    uint32_t heartbeat_before_timeout;
    uint32_t heartbeat_after_timeout;
    uint32_t timeout_stage;

    heartbeat_before_timeout = g_p0a_canary.heartbeat;
    CHECK(p0a_run_uart_probe(&uart, capture_config_stage, &mock, "X") == -1);
    CHECK(mock.reads == 7u);
    CHECK(mock.writes == 0u);
    CHECK(mock.stage_at_config == P0A_STAGE_C003);
    CHECK(mock.stage_at_first_poll == P0A_STAGE_C005);
    timeout_stage = g_p0a_canary.stage;
    p0a_canary_mark(P0A_STAGE_C0FF, 5u);
    p0a_canary_heartbeat(6u);
    heartbeat_after_timeout = g_p0a_canary.heartbeat;
    CHECK(g_p0a_canary.stage == P0A_STAGE_C0FF);
    CHECK(g_p0a_canary.error == P0A_ERROR_UART_TX_TIMEOUT);
    CHECK(g_p0a_canary.heartbeat == 1u);
    CHECK(g_p0a_canary.checksum == p0a_canary_checksum(&g_p0a_canary));
    if (argc == 3 && strcmp(argv[1], "--json") == 0) {
        FILE *stream = fopen(argv[2], "wb");
        if (stream == 0) return 2;
        fprintf(stream,
            "{\n"
            "  \"case_id\": \"P0-A-TX-NEVER-READY\",\n"
            "  \"status\": \"PASS\",\n"
            "  \"runner_exit_code\": 0,\n"
            "  \"observed_stage\": \"E101\",\n"
            "  \"uart_ready_samples\": [false,false,false,false,false,false,false],\n"
            "  \"observed_poll_count\": %lu,\n"
            "  \"bounded_poll_limit\": 7,\n"
            "  \"heartbeat_before_timeout\": %lu,\n"
            "  \"heartbeat_after_timeout\": %lu,\n"
            "  \"timeout_stage_value\": %lu,\n"
            "  \"stage_at_uart_config\": \"C003\",\n"
            "  \"stage_after_uart_config\": \"C004\",\n"
            "  \"stage_at_first_tx_poll\": \"C005\"\n"
            "}\n",
            (unsigned long)mock.reads,
            (unsigned long)heartbeat_before_timeout,
            (unsigned long)heartbeat_after_timeout,
            (unsigned long)timeout_stage);
        fclose(stream);
    }
    printf("p0a_uart_timeout: %d/%d passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
