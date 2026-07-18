# I0 UART1 Build Evidence

Status: BUILD COMPLETE, BOARD GATES NOT VERIFIED

Batch ID: `I0_UART1_20260719_FINAL`

Design commit: `6effdc3685d696cb4d33f3fbb1c449729ed72e33`

Design parent: `f47af290c2f014dfa8a131a3baebec1e9560ae21`

Executor: libaoxun (Codex)

Build date: 2026-07-19 Asia/Shanghai

## Scope

This packet binds the UART1 I0 source commit to the offline Efinity build
artifacts listed in `I0_UART1_BUILD_MANIFEST.json`. It is a BUILD evidence
packet only. It does not claim USER2, UART1 terminal I/O, APB MAGIC, ch0/HDMI
board regression, UART2/J52, or any myCobot connection or action.

UART0/R0 bitstreams, ELFs, warnings, and board records are historical and were
not used as evidence for this batch.

## Atomic Inputs

The complete 73-file HDL/IP/XML/SDC/debug-profile atomic input SHA-256 inventory is in
`I0_UART1_BUILD_INPUTS.sha256`. It includes `mem_test.xml`, `mem_test.peri.xml`,
`constrain.sdc`, `src/apb_reg_magic.v`, every HDL file listed by the project,
and all generated Hard SoC IP inputs. It intentionally includes unchanged
`constrain.sdc` and `src/apb_reg_magic.v`; unchanged source does not permit
reuse of a prior batch's build result.

Key generated inputs:

| Input | SHA-256 |
| --- | --- |
| `mem_test.xml` | `22B99A7060E52DC8C9664AF1A3540439BE12ADEB7E8EF9FE44BB766A1C79CB6D` |
| `mem_test.peri.xml` | `87FBBC6F98F9052A4B4187AC676B41E162B09007BE59BBFB9E718AFEB4F27080` |
| `constrain.sdc` | `3C0A58F318A3981984D7C544F26F9844FE85E27E680BDFA4C17B0988D0360994` |
| `src/apb_reg_magic.v` | `3CE1F02465B2288604B9A6AF322F9146B3FE6E412F3F0548B3EF480036D249E1` |
| `ip/EfxSapphireHpSoc_slb/settings.json` | `0645A91CCEE2631939C9B2A573BC4801FEDD9E50E9A7EA78E5CEAC6C8F2E5D5A` |
| `ip/EfxSapphireHpSoc_slb/hard_ip_args.ini` | `62F59A6DC3BE61B39149272889E7F826C71A49EC24745D4B06BD1AF9277F9ECF` |
| generated `bsp/.../soc.h` | `18C13BB5AD0EE920AE6F0D5EB97C07EE0896A0C645A97B4AF518D4CA776D6891` |

## Tool Invocation

Efinity version: `2025.2.288.4.15`, Titanium `TJ375N529`, timing model `I3`.

The official entry point was `D:\Efinity\2025.2\bin\efx_run.bat`. In an
ASCII junction working directory, the command was:

```powershell
Remove-Item Env:EFINITY_HOME -ErrorAction SilentlyContinue
Remove-Item Env:PYTHONHOME -ErrorAction SilentlyContinue
Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue
$env:PYTHONUTF8='1'
& 'D:\Efinity\2025.2\bin\efx_run.bat' mem_test --prj -f compile `
  --family Titanium --device TJ375N529 --timing_model I3 `
  --dir 'D:\cicc_cbm_link\competition_project_single_camera\ip\csi_rx_controller' `
  --dir 'D:\cicc_cbm_link\competition_project_single_camera\ip\dsi_tx' `
  --dir 'D:\cicc_cbm_link\competition_project_single_camera\ip\EfxSapphireHpSoc_slb' `
  --output_dir 'outflow_i0_uart1_20260719_final' `
  --work_dir 'work_i0_uart1_20260719_final' --timeout 7200
```

The wrapper reported `map: PASS`, `interface: PASS`, `pnr: PASS`, and
`pgm: PASS`. The raw build log is hash-bound in the manifest. The project XML
after the run reports `last_run_state=pass`, `last_run_flow=bitstream`, and
`config_result_in_sync=design_ood=place_ood=route_ood=sync`.

## Generated Hardware Facts

The Efinity-generated Hard SoC settings are `PERI_UART_0=0`,
`PERI_UART_1=1`, and `PERI_UART_2=0`. The generated `peri_config` records
`system_uart_1_io`, offset `0x11000`, interrupt ID `1`; generated `soc.h`
exposes `SYSTEM_UART_1_IO_CTRL=0xE8011000`,
`SYSTEM_UART_1_IO_PARAMETER_INIT_CONFIG_BAUDRATE=115200`, and
`SYSTEM_PLIC_SYSTEM_UART_1_IO_INTERRUPT=SYSTEM_PLIC_USER_INTERRUPT_A_INTERRUPT`.

Raw Efinity pinout evidence binds:

| Signal | GPIO / ball | Direction |
| --- | --- | --- |
| `system_uart_1_io_rxd` | `GPIOR_96_CLK13 / B12` | Input |
| `system_uart_1_io_txd` | `GPIOR_100 / D12` | Output |

`SYSTEM_UART_1_IO_PARAMETER_INIT_CONFIG_DATA_LENGTH=7` is the generated
frame-register encoding. The official Efinity UART driver writes
`dataLength - 1` to the data-length field, so encoding `7` represents 8 data
bits. The generated parity is `NONE` and stop setting is `ONE`; this is 115200
8N1. The driver reference and its SHA-256 are listed in the manifest.

## Results

| Gate | Result | Raw evidence |
| --- | --- | --- |
| Map | PASS | `mem_test.map.out`, `mem_test.map.rpt` |
| Interface | PASS | `mem_test.pt.rpt`, `mem_test.interface.csv`, `mem_test.pinout.rpt` |
| PNR | PASS | `mem_test.route.rpt` |
| STA | PASS, worst setup `1.511 ns`, worst hold `0.026 ns` | `mem_test.timing.rpt` |
| CDC | PASS, no synchronizer warnings | `mem_test.cdc.rpt` |
| Bitstream | PASS | `mem_test.bit`, `mem_test.hex`, `mem_test.pgm.out` |

The bitstream is `11809029` bytes with SHA-256
`8F02D32335C0D4DD70256C2A7D37B84F0A8DB59D3506374256B8D8F6514864E1`.

The separately compiled UART1 Hello ELF is `30672` bytes with SHA-256
`36EBB1BB06E73479897DA834DED6E4E8A06175EEBF31421F1C47CF5F4CE83241`.
It has exactly one `LOAD` segment at `0xF9000000`, `MemSiz=0x9e0`, an entry at
`0xF9000000`, and no undefined symbols. This is offline source-build evidence,
not a Goal 2 approval or a board-test result.

## Warning Inventory

The full raw warning file is `mem_test.warn.log`, with SHA-256 and external
path in the manifest. It has no EFX ERROR or FATAL record. It contains repeated
records because the complete flow logs multiple stages:

| Classification | Count | Disposition |
| --- | ---: | --- |
| `EFX-0011 VERI-WARNING` | 396 | Raw inventory retained; no waiver claimed. |
| `EFX-0200 WARNING` | 40 | Raw inventory retained; no waiver claimed. |
| `EFX-0201 WARNING` | 20 | Raw inventory retained; no waiver claimed. |
| `EFX-0256 WARNING` | 28 | Raw inventory retained; no waiver claimed. |
| Interface physical-distance warnings | 4 | Reported separately in `mem_test.pt.rpt`; no waiver claimed. |
| Post-synthesis warning summary | 118 | Reported by Map; separate from Interface warnings. |

The warning inventory is presented for review, not suppressed. No warning is
used to claim board behavior.

## External Evidence Retention

Raw reports and generated files are intentionally not committed. They are
located at:

```text
D:\cicc_cbm_link\competition_project_single_camera\outflow_i0_uart1_20260719_final
D:\cicc_cbm_link\competition_project_single_camera\work_i0_uart1_20260719_final
```

Their names, sizes, timestamps, and SHA-256 values are fixed in the manifest.
Any missing file or SHA mismatch invalidates this packet.

## Explicit Non-Claims

- No old UART0/R0 bitstream, ELF, warning report, or board PASS was used.
- USER2, Type-C UART1 terminal echo, APB MAGIC, and ch0/HDMI board regression
  are not run and remain `NOT VERIFIED`.
- UART2/J52 and all myCobot wiring, queries, and motions are out of scope and
  were not performed.
