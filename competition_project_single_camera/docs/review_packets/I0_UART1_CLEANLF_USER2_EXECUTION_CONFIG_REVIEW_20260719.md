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
- `tools/capture_i0_uart1_raw.ps1`
- `tools/wsc_i0_apb_probe_contract.json`
- `tools/i0_uart1_wsc_probe_gate.ps1`
- `tools/i0_uart1_wsc_apb_probe.gdb`
- `tools/verify_i0_uart1_execution_config.ps1`
- `docs/debug_sessions/I0_UART1_CLEANLF_USER2_OPERATION_CARD_20260719.md`

| New file | SHA-256 |
| --- | --- |
| `tools/i0_uart1_cleanlf_user2.cfg` | See hash-locked manifest |
| `tools/i0_uart1_cleanlf_ram_halt.gdb` | See hash-locked manifest |
| `tools/capture_i0_uart1_raw.ps1` | See hash-locked manifest |
| `tools/wsc_i0_apb_probe_contract.json` | See hash-locked manifest |
| `tools/i0_uart1_wsc_probe_gate.ps1` | See hash-locked manifest |
| `tools/i0_uart1_wsc_apb_probe.gdb` | See hash-locked manifest |
| `tools/verify_i0_uart1_execution_config.ps1` | See hash-locked manifest |
| Operation card and this Packet | See hash-locked manifest |

The manifest is `tools/i0_uart1_execution_manifest.json`. The verifier and
Packet bind each non-self-referential entry below to its actual SHA-256; the
manifest and this Packet have their actual SHA-256 printed by the verifier at
each run, avoiding a circular self-hash claim.

| Hash-locked path | SHA-256 |
| --- | --- |
| `embedded_sw/apb_magic_onchip/APB_PROBE_DEBUGGER_CONTRACT.md` | `C4FA7C531526AFCE48F7B503AE8808ECDCB0440B9E97F49B824D33154B095824` |
| `embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.contract.txt` | `119C260FEAB1D0A3C6A63E022C141D6FB1D70FF57646F6E40F1F71FBFB1BEFA0` |
| `embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.elf` | `6CB7E5E1CA5AF1B82FB99015925F13C2517B9332FD0FB0015DC1B4FDC3DFEAEC` |
| `embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.objdump.txt` | `F00613ED776DB74B74B06BED195524E17F3B9B3A4190616064673EC6980D3A83` |
| `embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.readelf.txt` | `C001CABA534C57770DE4D4B25AB59B0A245364274A4AE8F81BB4BB30A9252F63` |
| `embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.sha256.txt` | `F67AB5202D7AB697FB5A67BF4A7ABE769B61A5ABC39E9DC435343C787002A1E2` |
| `embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.symbols.txt` | `385739A8C3337EE07380C646FD37D8E202A94CE1AC5E0D284FD11537CEA49318` |
| `embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.undefined.txt` | `E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855` |
| `embedded_sw/apb_magic_onchip/build_apb_probe_evidence.ps1` | `9EB1B4A74C841FD3BDBFFEBEDE4AF7385FE98FEF15AE86C2C185193BE0C2BCCF` |
| `embedded_sw/apb_magic_onchip/makefile` | `5E385A248F4ABF15DCF5FEC203CB8F6E000F163C6A2B0141956154CEFF38559F` |
| `embedded_sw/apb_magic_onchip/src/main.c` | `E6D2BC8045828DC6D78937FDAC203263D8D7B36B29DEE7B460115E9441C2D45F` |
| `embedded_sw/apb_magic_onchip/verify_apb_probe_contract.ps1` | `6BE3760399D805D9E9502B5EF26F8BA3C47EB6A7CAE3D5E0A63A6C86E89CE218` |
| `tools/capture_i0_uart1_raw.ps1` | `26A7445BAD531E5622A400D6B0290DFAC1FAC903BB9F8FB9E4F1BE10549D41E4` |
| `tools/i0_uart1_cleanlf_ram_halt.gdb` | `2FB550EFB7D4C0316A22D77CA73BF1E35FBBADBF1AFBC9E0960C5FC985909BD9` |
| `tools/i0_uart1_cleanlf_user2.cfg` | `85D413A4728DABD264AA6CC13FE5393BBD6A8634422D2BA0638853EE26C7DDD6` |
| `tools/i0_uart1_wsc_apb_probe.gdb` | `37DF08A5D9D7D9005F16CF614EFF1497DC8BD968A6528EC498A5B30FFE803AD2` |
| `tools/i0_uart1_wsc_probe_gate.ps1` | `ADD0BA68582823DC6E7ED9D7E726362161D3769081836D0FAE1A6E9F2C21846B` |
| `tools/wsc_i0_apb_probe_contract.json` | `898E80B9BD821648E0231CA1EE2415BE020E274C988B72FB6451DF7E3C07C013` |
| `tools/verify_i0_uart1_execution_config.ps1` | `874FD231B423B2AE9783435B60D099CDE2D7ACA42B167FA826663137E6D26895` |
| `docs/debug_sessions/I0_UART1_CLEANLF_USER2_OPERATION_CARD_20260719.md` | `D00DEE4967D785BA46F43F7E4F3460167D8397AF7DA35AF6D31FFFCE4D3DC713` |

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
3. Type-C UART1 `115200 8N1` complete three-line Hello followed by exactly one
   printable-byte echo, with PnP-bound identity and timestamped RX/TX counts.
   The Hello ELF receives one controlled resume only after PC PASS; after UART
   PASS it is halted and logged before the separate WSC probe session begins.
4. WSC's hash-locked RAM-only APB probe only after UART PASS. The CPU performs
   exactly one read at `0xE8100000`; debugger direct APB access is prohibited.

## Explicitly Excluded

USER1, SoftTap, Flash/SPI/PROM, DDR, UART0, CH340/COM17, UART2/J52,
myCobot, address scanning, APB writes, and any retry after first failure.

## Tooling And Raw Evidence

- Programmer: `D:\Efinity\2025.2\bin\efx_pgm.exe`, version `2025.2.288.4.15`.
- OpenOCD: `D:\Efinity\efinity-riscv-ide-2025.2\openocd\bin\openocd.exe`.
- GDB: `D:\Efinity\efinity-riscv-ide-2025.2\toolchain\bin\riscv-none-embed-gdb.exe`.
- Serial identity must be freshly enumerated in the next window. Existing
  `COM17` is CH340 and prohibited.

The static verifier must be run before requesting the next hardware window. It
recomputes every listed execution/card/Packet hash from
`tools/i0_uart1_execution_manifest.json` and rejects Packet/manifest drift. The
WSC probe chain is pinned to cloud contract commit
`a840f0869c11bab0915757d64c56a167f6d4f917` and probe commit
`15908b32475f6ce80b645a728c25a5e7a2db749f`; its ELF SHA-256 is
`6CB7E5E1CA5AF1B82FB99015925F13C2517B9332FD0FB0015DC1B4FDC3DFEAEC`.

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
4. Is the WSC CPU-owned one-read probe plus debugger RAM-only evidence surface
   accepted in place of the removed direct `x/wx 0xE8100000` route?
