# Review Packet: I0 UART1 Clean-LF USER2 Execution Configuration

## Goal And Requested Decision

Approve or reject the UART1-only pre-window execution configuration for the
already recovered `I0_UART1_20260719_CLEAN_LF_FINAL` artifacts. This packet
does not authorize a hardware window, JTAG action, serial open, APB access,
UART2/J52, or myCobot action.

Requested decision: whether the configuration is sufficiently isolated from
UART0 history and correctly constrained for a future user-approved I0 window.

## Fixed Inputs

| Input | Path | SHA-256 |
| --- | --- | --- |
| Bitstream | `C:\cicc_i0_uart1_design_lf_20260719_v4\competition_project_single_camera\outflow_i0_uart1_20260719_cleanlf_v4\mem_test.bit` | `D05EFD4EC91FDB51F2997483BA7F4A3634C3E50FF1C65DD7239B41E19E2BC544` |
| Hello ELF | `C:\cicc_i0_uart1_design_lf_20260719_v4\competition_project_single_camera\embedded_sw\uart1_hello_onchip\build\uart1_hello_onchip.elf` | `919B291A2B980F02507D1591EFBE5D8E395DC53F4A480AE5A8C577D74CF5F7FA` |
| Generated `soc.h` | `C:\cicc_i0_uart1_design_lf_20260719_v4\competition_project_single_camera\embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\include\soc.h` | `25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B` |

The artifacts remain in their clean-LF design root and are not copied into the
integration worktree. `soc.h` defines `IO_APB_SLAVE_0_INPUT=0xE8100000`.

## New Files

- `tools/i0_uart1_cleanlf_user2.cfg`
- `tools/i0_uart1_cleanlf_ram_halt.gdb`
- `tools/i0_uart1_cleanlf_apb_read.gdb`
- `tools/capture_i0_uart1_raw.ps1`
- `tools/verify_i0_uart1_execution_config.ps1`
- `docs/debug_sessions/I0_UART1_CLEANLF_USER2_OPERATION_CARD_20260719.md`

| New file | SHA-256 |
| --- | --- |
| `tools/i0_uart1_cleanlf_user2.cfg` | `B4B316B948664DD3BFEBBC99E3BF28B7BCB0D830F22B647A993B6791D9915ED6` |
| `tools/i0_uart1_cleanlf_ram_halt.gdb` | `FF8A1A048443FE7FA6770C29737E0027745BA7B18636A3754D02BBA25EC2C3E5` |
| `tools/i0_uart1_cleanlf_apb_read.gdb` | `9B4E2D6B97A36C112B50663AC472C6156CB6EC8A3C97A00CB68E8A94AC353103` |
| `tools/capture_i0_uart1_raw.ps1` | `7C0E0A2EB32CC1FD69C425DBDE31CC6B61CCC81B04207A1E7C66DFC2D1AFC365` |
| `tools/verify_i0_uart1_execution_config.ps1` | `6FD78E3936CF8DF24B8211C3D4DDA0CD6D5206BC5A284AAB9882735AB6AED18F` |
| Operation card | `7126CC4E973D88FD7D9F71AB9AFB9D4C3E45EE62F19B10AF17EDA9662FC6A419` |

## Ownership Exception

The standard ownership checker classifies `competition_project_single_camera/docs/**`
and `competition_project_single_camera/tools/**` as qzs scope. The user explicitly
assigned this one UART1 clean-LF configuration, capture, operation-card, and review
packet batch to libaoxun on 2026-07-19. `team_scope_check -Role libaoxun` therefore
reports these seven new paths as expected exceptions. This exception neither changes
the ownership table nor authorizes frozen-interface, RTL, XML, SDC, IP, CPU business,
hardware, UART2/J52, or myCobot modifications.

## Authoritative Configuration Sources

| Source | SHA-256 | Used Fact |
| --- | --- | --- |
| `D:\Efinity\2025.2\ipm\ip\efx_hard_soc\fpga\Ti375C529_devkit\embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\openocd\debug_ti.cfg` | `E91B792D40FDC67E0BF491187438E58092AF56CBEA4261B65071C88A52ED1828` | CPU TAP default `0x006A0A79`, BSCAN tunnel `6 1`, tunnel IR `8`, RAM work area `0xF9000000`, initial halt |
| `D:\Efinity\2025.2\ipm\ip\efx_hard_soc\fpga\Ti375C529_devkit\embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\openocd\ftdi_ti.cfg` | `934FF4B47010461DB627A74BF8D5124CCD21CDAB64FE9080B06CEB395C9298FE` | Official Ti375 FTDI JTAG interface |
| `D:\Efinity\2025.2\debugger\bin\efx_dbg\jtag.py` | `AE8075C1F1E3ED83C52B74835B3680D351A63BDE3D23883A88DF618BFAD91CF4` | Titanium 5-bit USER2 IR `01001` / `0x09` |

No G1/R0/M2 UART0 file is a source for this configuration.

## Signals, Clock, Reset, And CDC

No RTL, XML, SDC, IP, wrapper, BSP, clock, reset, video, CDC, or dual-channel
path is changed. The configuration uses the existing Hard SoC CPU debug bridge
only. It starts halted and requires a post-load halt plus PC range gate before
any CPU resume.

## Allowed Future Operations

1. Volatile FPGA configuration only, with expected FPGA JTAG ID `0x006A0EF3`.
2. USER2 BSCAN tunnel only, RAM-only ELF load, post-load halt, and PC read.
3. Type-C UART1 `115200 8N1` Hello/echo with full RX/TX byte accounting.
4. One APB read at `0xE8100000`, expected `0x375A0001`, only after UART PASS.

## Explicitly Excluded

USER1, SoftTap, Flash/SPI/PROM, DDR, UART0, CH340/COM17, UART2/J52,
myCobot, address scanning, APB writes, and any retry after first failure.

## Tooling And Raw Evidence

- Programmer: `D:\Efinity\2025.2\bin\efx_pgm.exe`, version `2025.2.288.4.15`.
- OpenOCD: `D:\Efinity\efinity-riscv-ide-2025.2\openocd\bin\openocd.exe`.
- GDB: `D:\Efinity\efinity-riscv-ide-2025.2\toolchain\bin\riscv-none-embed-gdb.exe`.
- Serial identity must be freshly enumerated in the next window. Existing
  `COM17` is CH340 and prohibited.

The static verifier must be run before requesting the next hardware window.
It passed after creation, as did PowerShell parser checks for the capture and verifier
scripts, `git diff --check`, executable-token prohibition scans, and APB literal scan
(`0xE8100000`, `0x375A0001` only).

## Remaining Risks And Stop Conditions

- The future Type-C UART1 COM/VID/PID/serial identity is not yet known.
- Runtime proof of USER2, CPU PC, UART Hello/echo, and APB MAGIC remains
  `NOT VERIFIED`.
- Any artifact/config/tool mismatch, wrong JTAG ID, PC outside RAM range,
  serial failure, or APB mismatch terminates the window immediately.

## Reviewer Questions

1. Does the official Ti375C529 `debug_ti.cfg` source adequately establish the
   USER2 tunnel, CPUTAPID, RAM, and halt parameters for this clean-LF batch?
2. Is the volatile-programmer UI step and its evidence requirement sufficiently
   explicit to exclude all nonvolatile programming paths?
3. Does the UART capture script preserve enough evidence for Hello/echo before
   the single permitted APB read?
