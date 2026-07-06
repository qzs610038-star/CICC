/*==========================================================================
 *  main.c  --  RISC-V shape recognition entry point
 *
 *  Runs on the SapphireSoC embedded RISC-V core.
 *  Continuously reads the downscaled frame from DDR, detects colour + shape,
 *  and prints the result to UART.
 *
 *  Build with Efinity RISC-V Embedded Software IDE (BSP auto-generated).
 *  This file should be added to the IDE project's src/ folder.
 *==========================================================================*/

#include <stdint.h>
#include "bsp.h"
#include "shape_detect.h"

/*--------------------------------------------------------------------------
 *  UART driver (uses BSP API from bsp.h)
 *------------------------------------------------------------------------*/
static void uart_putc(char c)
{
    bsp_putChar(c);
}

static void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

static void uart_put_dec(int val)
{
    char buf[12];
    int  neg = 0;
    int  i = 0;

    if (val < 0) { neg = 1; val = -val; }
    if (val == 0) { buf[i++] = '0'; }
    else {
        while (val > 0) {
            buf[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    if (neg) uart_putc('-');
    while (i > 0) uart_putc(buf[--i]);
}

/*--------------------------------------------------------------------------
 *  Simple delay (busy-wait)
 *------------------------------------------------------------------------*/
static void delay_ms(int ms)
{
    /* Use BSP's microsecond delay if available, otherwise busy-wait */
    volatile int cnt = ms * 40000;
    while (cnt-- > 0) ;
}

/*--------------------------------------------------------------------------
 *  Main
 *------------------------------------------------------------------------*/
int main(void)
{
    bsp_init();  /* Initialize UART baudrate and frame config */

    const volatile pixel_t *frame = (const volatile pixel_t *)FRAME_BASE;
    detect_result_t result;
    int frame_cnt = 0;

    uart_puts("\r\n=== TJ375 Shape Recogniser (RISC-V) ===\r\n");
    uart_puts("Frame size: 240x135, base: 0x04000000\r\n\r\n");

    for (;;) {
        /* Wait ~100 ms between detections (6 fps) to allow
         * the downscaler to write a stable frame.                */
        delay_ms(100);

        int found = detect_object(frame, &result);

        frame_cnt++;

        if (found && result.shape_id != SHAPE_NONE) {
            uart_puts("[");
            uart_put_dec(frame_cnt);
            uart_puts("] ");
            uart_puts(color_name(result.color_id));
            uart_putc(' ');
            uart_puts(shape_name(result.shape_id));

            uart_puts("  bbox(");
            uart_put_dec(result.bbox.x);
            uart_putc(',');
            uart_put_dec(result.bbox.y);
            uart_putc(',');
            uart_put_dec(result.bbox.w);
            uart_putc('x');
            uart_put_dec(result.bbox.h);
            uart_puts(")  fg=");
            uart_put_dec(result.fg_count);
            uart_puts("\r\n");
        } else {
            /* No object detected -- print a dot every 10 frames */
            if ((frame_cnt % 10) == 0)
                uart_putc('.');
        }
    }

    return 0;
}
