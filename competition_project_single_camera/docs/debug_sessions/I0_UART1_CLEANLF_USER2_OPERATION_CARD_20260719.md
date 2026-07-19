# I0 UART1 Clean-LF USER2 Operation Card

Status: DRAFT FOR REVIEW. No current hardware window. Do not execute this card
until the user supplies a new window tied to the fixed artifacts below.

## Fixed Batch And Stored Artifacts

- Batch: `I0_UART1_20260719_CLEAN_LF_FINAL`
- Integration SHA: `e72fb6a3ce1b7999f61cae1e0ed7b2773f1e4fda`
- Design SHA: `6effdc3685d696cb4d33f3fbb1c449729ed72e33`
- Design root: `C:\cicc_i0_uart1_design_lf_20260719_v4\competition_project_single_camera`
- Bitstream: `outflow_i0_uart1_20260719_cleanlf_v4\mem_test.bit`
  - SHA-256: `D05EFD4EC91FDB51F2997483BA7F4A3634C3E50FF1C65DD7239B41E19E2BC544`
- Hello ELF: `embedded_sw\uart1_hello_onchip\build\uart1_hello_onchip.elf`
  - SHA-256: `919B291A2B980F02507D1591EFBE5D8E395DC53F4A480AE5A8C577D74CF5F7FA`
- Generated `soc.h`: `embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\include\soc.h`
  - SHA-256: `25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B`
  - APB source macro: `IO_APB_SLAVE_0_INPUT=0xE8100000`

Do not copy these artifacts into the integration worktree. Recompute all three
hashes in the new window and stop before hardware activity on any mismatch.

## UART1-Only Configuration

- Repository target config: `tools/i0_uart1_cleanlf_user2.cfg`
- GDB RAM/halt/PC gate: `tools/i0_uart1_cleanlf_ram_halt.gdb`
- GDB APB final read: `tools/i0_uart1_cleanlf_apb_read.gdb`
- UART1 raw capture: `tools/capture_i0_uart1_raw.ps1`
- Static verifier: `tools/verify_i0_uart1_execution_config.ps1`

The target config is based on Efinity 2025.2 Ti375C529 Hard SoC
`openocd/debug_ti.cfg`, not on any repository UART0 configuration. It fixes:

- Titanium USER2 outer IR: five-bit `01001` (`0x09`).
- BSCAN tunnel: `riscv use_bscan_tunnel 6 1` and tunnel IR `8`.
- CPU TAP expected ID: `0x006A0A79`.
- On-chip RAM work area: `0xF9000000`; PC acceptance range:
  `0xF9000000..0xF9003FFF`.

## Prohibited Routes

Never use USER1, SoftTap, Flash, SPI, PROM, DDR, UART0, the CH340 `COM17`,
or any G1/R0/M2 checkpoint, card, approval JSON, or `prepare_m2...` script.
No cable, hash, TAP, or serial-port substitution is allowed after the window
starts.

## New-Window Execution Sequence

1. Run the static verifier and record all fixed-artifact and config hashes.
   Verify Efinity Programmer `D:\Efinity\2025.2\bin\efx_pgm.exe` version
   `2025.2.288.4.15`, OpenOCD, and RISC-V GDB identities. Enumerate Type-C
   UART1 identity without opening it; reject CH340/COM17.
2. In Efinity Programmer, select `TJ375N529` and the fixed bitstream. Use the
   volatile FPGA configuration action only. Do not select any Flash/SPI/PROM
   target. Confirm JTAG ID `0x006A0EF3`; otherwise stop.
3. Start OpenOCD only with the official Efinity Ti375 FTDI interface file and
   `tools/i0_uart1_cleanlf_user2.cfg`; then run
   `tools/i0_uart1_cleanlf_ram_halt.gdb`. This performs RAM-only ELF load,
   halts after load, and rejects a PC outside `0xF9000000..0xF9003FFF`.
4. Open Type-C UART1 at `115200 8N1` with `capture_i0_uart1_raw.ps1`.
   Record the enumerated unique identity, complete RX/TX transcript and byte
   counts. After Hello and one-character echo PASS, run the APB GDB script:
   it reads only `0xE8100000`; accept only `0x375A0001`.

At the first failure, preserve the raw log and stop the window. Do not retry,
fall back to UART0, or continue to APB after UART failure.

## Required Raw Logs

Use a new directory under `debug_records/i0_uart1_cleanlf/<new-window-id>/`:

- `preflight.txt`: hashes, tool paths/versions, board/JTAG identity, Type-C
  UART1 enumeration identity.
- `programmer_volatile.log`: UI screenshot or native log proving volatile FPGA
  configuration only.
- `openocd_user2.log`, `gdb_ram_halt_pc.log`, and `gdb_apb_read.log`.
- `uart1_raw.log`: timestamped bytes plus `rx_bytes` and `tx_bytes`.

No log means no PASS.
