# R0 UART0 Synchronized Banner Blocker

Date: 2026-07-17 (Asia/Shanghai)

Decision: `R0 BLOCKED_AT_UART0_BANNER / R1-R5 NO-GO`

## Scope

This report closes the current R0 UART0 banner attempt only. It does not change RTL, XML, SDC, IP, feature capture, APB ABI, firmware, or `CURRENT_STATE.md`.

Allowed hardware scope used: volatile FPGA configuration already verified for this batch, `JTAG_USER2` only, on-chip RAM `0xF9000000..0xF9003FFF`, and two read-only candidate listeners. No USER1, Flash/SPI, external DDR, UART2/J52, myCobot, OSD, PC classification, or UART transmit was used.

## Frozen Batch

- Repository: `main@9acf4d8b2ec788ccd5777f3833a7bfb756c51cad`
- Bitstream SHA-256: `9F6F254E33C803C0F1B6D2F3CAB1929496477D9CBEE4655EC680A91B4028F320`
- Hello ELF SHA-256: `CD4CAB96D3C30ECDC085B5C5A36B175DCED0016FBD53929BB0E2D3741E29411B`
- USER2 tunnel IR: `9`
- FPGA device ID: `0x006A0EF3`
- Debug work area: `0xF9000C00..0xF9000FFF`

## Successful Preconditions

- Efinity cold build passed Map, Interface, PNR, bitstream, STA (`+1.321 ns` setup, `+0.026 ns` hold), and CDC without synchronizer warnings.
- The current FPGA was volatile-configured with the matching bitstream using the official JTAG flow.
- OpenOCD found four QCRV32 harts through USER2.
- The approved SRAM load showed `pc = 0xF9000000 <_start>` and read back the current ELF instructions.
- APB MAGIC was read as `0xE8100000 = 0x375A0001`.

## Synchronized Read-Only Attempt

The initial concurrent attempt at `17:52` is excluded from the UART decision: its staging-copy listener lacked Git metadata and failed before it could create serial evidence.

The decisive retry began at `20260717_175445`:

1. Started two repository-resident listeners before CPU execution: COM10 and COM13, both 115200 8N1, no flow control, `DTR=false`, `RTS=false`, ten-second window.
2. Started the approved OpenOCD chain: `set CPUTAPID 0x006A0EF3`, `ftdi_ti.cfg`, then `debug_ti_m2_safe.cfg`.
3. OpenOCD published GDB port 3333 and confirmed four harts through USER2 tunnel IR 9.
4. GDB loaded the matching ELF, reported `pc = 0xF9000000`, then ran `continue` for a bounded four seconds.
5. Only the OpenOCD/GDB processes created by this attempt were stopped after the capture interval. No serial write occurred.

Both valid listener evidence records reported:

| Port | Opened | RX bytes | TX bytes | Result |
|---|---:|---:|---:|---|
| COM10 | yes | 0 | 0 | `UART0_LISTEN_COMPLETE_NO_FULL_BANNER_NO_TX` |
| COM13 | yes | 0 | 0 | `UART0_LISTEN_COMPLETE_NO_FULL_BANNER_NO_TX` |

## Evidence Index

- `<external-r0-evidence>/uart/r0_uart_banner_com10_synchronized_retry_20260717_175445_20260717_175456.json`
- `<external-r0-evidence>/uart/r0_uart_banner_com13_synchronized_retry_20260717_175445_20260717_175456.json`
- `<external-r0-evidence>/runtime/gdb_user2_uart_sync_retry_20260717_175445.stdout.log`
- `<external-r0-evidence>/runtime/openocd_user2_uart_sync_retry_20260717_175445.stderr.log`
- `<external-r0-evidence>/approvals/r0_user2_ram_pc_gate_approved.json`

## Conclusion And Recovery Boundary

The current batch has evidence for CPU takeoff and APB MAGIC, but it has not produced the required UART0 Hello banner. R0 therefore does not pass, so R1 candidate probing and all R2-R5 implementation or board-recognition work remain forbidden.

Do not try other UART classes, COM11, UART2/J52, or a transmit/echo test. The next recovery action must remain inside R0 and first establish the physical mapping of the configured UART0 TX pin (`GPIOR_145/E10`) to a verified receive endpoint, or add an independently reviewed non-invasive observability method. Any new operation card must preserve the matching hashes and the USER2/SRAM-only boundary.
