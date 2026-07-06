#include "bsp.h"

static void put_string(const char *s)
{
    while (*s) {
        bsp_putChar(*s++);
    }
}

int main(void)
{
    bsp_init();
    put_string("\r\nTJ375 final decision app skeleton\r\n");
    put_string("Next checks: register read, classifier result writeback, arm state output.\r\n");

    for (;;) {
        bsp_uDelay(1000000);
        put_string(".");
    }

    return 0;
}
