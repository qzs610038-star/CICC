/*==========================================================================
 *  bsp.h  -  Provisional Board Support Package for SapphireSoC on TJ375N529
 *
 *  These addresses are placeholders for the CPU skeleton. Replace them with
 *  the final Efinity-generated soc.h values after the SoC configuration is
 *  locked. The official RISC-V examples contain multiple incompatible maps,
 *  so do not treat this file as the address source of truth.
 *
 *  UART register layout matches SapphireSoC UART IP:
 *    +0x00 DATA
 *    +0x04 STATUS  [31:24]=RX_occupancy, [23:16]=TX_availability
 *    +0x08 CLOCK_DIVIDER
 *    +0x0C FRAME_CONFIG
 *==========================================================================*/

#ifndef BSP_H
#define BSP_H

#include <stdint.h>

/*--------------------------------------------------------------------------
 *  Memory-mapped I/O helpers
 *------------------------------------------------------------------------*/
static inline void write_u32(uint32_t val, uint32_t addr)
{
    *(volatile uint32_t *)addr = val;
}

static inline uint32_t read_u32(uint32_t addr)
{
    return *(volatile uint32_t *)addr;
}

/*--------------------------------------------------------------------------
 *  Provisional SoC memory map. Final values must come from generated soc.h.
 *------------------------------------------------------------------------*/
#define SYSTEM_UART_0_IO_CTRL   0xF8010000u
#define SYSTEM_GPIO_0_IO_CTRL   0xF8015000u
#define SYSTEM_SPI_0_IO_CTRL    0xF8014000u
#define SYSTEM_I2C_0_IO_CTRL    0xF8016000u
#define SYSTEM_CLINT_CTRL       0xF8B00000u
#define SYSTEM_PLIC_CTRL        0xF8C00000u
#define IO_APB_SLAVE_0_INPUT    0xF8100000u

/* CLINT clock frequency. Final value must come from generated soc.h. */
#define SYSTEM_CLINT_HZ         100000000u  /* 100 MHz */

/* DDR memory base */
#define DDR_BASE                0x00000000u

/* Frame buffer base (downscaler output) - 64MB offset */
#define FRAME_BASE              0x04000000u

/*--------------------------------------------------------------------------
 *  UART driver (matches SapphireSoC UART IP)
 *------------------------------------------------------------------------*/
#define UART_DATA           0x00
#define UART_STATUS         0x04
#define UART_CLOCK_DIVIDER  0x08
#define UART_FRAME_CONFIG   0x0C

#define BSP_UART_TERMINAL   SYSTEM_UART_0_IO_CTRL
#define BSP_UART_BAUDRATE   115200
#define BSP_UART_DATA_LEN   8

static inline uint32_t uart_writeAvailability(uint32_t reg)
{
    return (read_u32(reg + UART_STATUS) >> 16) & 0xFF;
}

static inline void uart_write(uint32_t reg, char data)
{
    while (uart_writeAvailability(reg) == 0)
        ;
    write_u32((uint32_t)data, reg + UART_DATA);
}

#define bsp_putChar(c) uart_write(BSP_UART_TERMINAL, c)

/*--------------------------------------------------------------------------
 *  UART initialization
 *------------------------------------------------------------------------*/
static inline void bsp_init(void)
{
    /* clockDivider = CLINT_HZ / (baud * dataLen) - 1 */
    uint32_t div = SYSTEM_CLINT_HZ / (BSP_UART_BAUDRATE * BSP_UART_DATA_LEN) - 1;
    write_u32(div, BSP_UART_TERMINAL + UART_CLOCK_DIVIDER);
    /* dataLength=8-1=7, parity=NONE(0), stop=ONE(0) */
    write_u32(7 << 0, BSP_UART_TERMINAL + UART_FRAME_CONFIG);
}

/*--------------------------------------------------------------------------
 *  Simple microsecond delay using CLINT mtime counter
 *------------------------------------------------------------------------*/
static inline void clint_uDelay(uint32_t usec, uint32_t hz, uint32_t reg)
{
    uint64_t mtime0, mtime1;
    uint64_t ticks = ((uint64_t)hz * usec) / 1000000u;
    /* mtime is at offset 0xBFF8 in CLINT */
    mtime0 = *(volatile uint64_t *)(reg + 0xBFF8);
    do {
        mtime1 = *(volatile uint64_t *)(reg + 0xBFF8);
    } while ((mtime1 - mtime0) < ticks);
}

#define bsp_uDelay(usec) clint_uDelay(usec, SYSTEM_CLINT_HZ, SYSTEM_CLINT_CTRL)

#endif /* BSP_H */
