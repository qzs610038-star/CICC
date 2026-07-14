static void qemu_semihost_exit(int code)
{
    volatile int block[2];
    register int a0 asm("a0") = 0x20;
    register void *a1 asm("a1") = (void *)block;

    block[0] = 0x20026;
    block[1] = code;
    asm volatile (
        ".option push\n"
        ".option norvc\n"
        "slli zero, zero, 0x1f\n"
        "ebreak\n"
        "srai zero, zero, 7\n"
        ".option pop\n"
        : : "r"(a0), "r"(a1) : "memory");
    for (;;) {
    }
}

void __assert_func(const char *file, int line, const char *func, const char *expr)
{
    (void)file;
    (void)line;
    (void)func;
    (void)expr;
    qemu_semihost_exit(1);
}

int arm_runtime_test_entry(void);

int main(void)
{
    qemu_semihost_exit(arm_runtime_test_entry());
    return 0;
}
