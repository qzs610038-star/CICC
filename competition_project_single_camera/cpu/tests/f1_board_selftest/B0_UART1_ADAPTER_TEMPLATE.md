# B0 UART1 adapter template

Status: `TEMPLATE / DO NOT BUILD AGAINST ACTIVE OR INTERMEDIATE UART1 WORK`.

Activation requires a libaoxun-provided immutable UART1 full-chain PASS package containing
the hardware SHA, batch identity, bitstream, `soc.h`, linker, BSP and evidence hashes. Until
then, this directory has no UART register address, BSP include or linker binding.

The B0 adapter may only:

1. call `f1_board_selftest_run()` without changing the business core;
2. bind its event callback to the same-batch verified UART1 API;
3. emit one final `@F1SELFTEST|v=1|build=<id>|cases=8|pass=8|digest=<hex>|arm=0` line;
4. produce a new ELF/map/readelf/objdump/hash identity and pass qzs verification.

That result can upgrade only `BOARD_CPU_F1_SELFTEST`. Feature/APB/CDC/OSD remain
`NOT_VERIFIED`; `ARM_ENABLED=0`, UART2/J52 and myCobot remain excluded.
