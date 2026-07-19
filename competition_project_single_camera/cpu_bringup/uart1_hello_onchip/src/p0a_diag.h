#ifndef P0A_DIAG_H
#define P0A_DIAG_H

#include <stdint.h>

#ifndef P0A_BUILD_ID
#error "P0A_BUILD_ID must be injected from the normalized firmware input SHA-256"
#endif

#define P0A_CANARY_MAGIC 0x50304143u
#define P0A_CANARY_SCHEMA 1u
#define P0A_UART_POLL_BUDGET 4096u

#define P0A_STAGE_C001 0xC001u
#define P0A_STAGE_C002 0xC002u
#define P0A_STAGE_C003 0xC003u
#define P0A_STAGE_C004 0xC004u
#define P0A_STAGE_C005 0xC005u
#define P0A_STAGE_E101 0xE101u
#define P0A_STAGE_C0FF 0xC0FFu

#define P0A_UART_STATUS_NOT_PROBED 0u
#define P0A_UART_STATUS_TX_READY 1u
#define P0A_UART_STATUS_TX_TIMEOUT 2u

#define P0A_ERROR_NONE 0u
#define P0A_ERROR_UART_TX_TIMEOUT 0xE101u

typedef struct {
    uint32_t magic;
    uint32_t schema;
    uint32_t build_id;
    uint32_t write_count;
    uint32_t stage;
    uint32_t last_pc_tag;
    uint32_t uart_status;
    uint32_t error;
    uint32_t heartbeat;
    uint32_t checksum;
} p0a_canary_t;

typedef uint32_t (*p0a_read32_fn)(void *context, uint32_t address);
typedef void (*p0a_write32_fn)(void *context, uint32_t address, uint32_t value);
typedef void (*p0a_uart_init_fn)(void *context);

typedef struct {
    p0a_read32_fn read32;
    p0a_write32_fn write32;
    void *context;
    uint32_t uart_base;
    uint32_t poll_budget;
} p0a_uart_t;

extern volatile p0a_canary_t g_p0a_canary;

uint32_t p0a_canary_checksum(const volatile p0a_canary_t *canary);
void p0a_canary_init(uint32_t pc_tag);
void p0a_canary_mark(uint32_t stage, uint32_t pc_tag);
void p0a_canary_set_uart(uint32_t status, uint32_t error, uint32_t pc_tag);
void p0a_canary_heartbeat(uint32_t pc_tag);
int p0a_uart_putc_bounded(const p0a_uart_t *uart, uint8_t value);
int p0a_uart_write_bounded(const p0a_uart_t *uart, const char *text);
int p0a_run_uart_probe(const p0a_uart_t *uart, p0a_uart_init_fn init_fn,
                       void *init_context, const char *text);

#endif
