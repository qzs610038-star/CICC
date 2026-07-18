# I0 UART1 Clean-LF Build Evidence

Status: OFFLINE BUILD COMPLETE. BOARD GATES NOT VERIFIED.

## Fixed Identity

- Evidence ref: `dev/libaoxun688-uart1-i0-20260719-cleanlf-final`
- Design SHA: `6effdc3685d696cb4d33f3fbb1c449729ed72e33`
- Design parent: `f47af290c2f014dfa8a131a3baebec1e9560ae21`
- Batch ID: `I0_UART1_20260719_CLEAN_LF_FINAL`
- Efinity: `2025.2.288.4.15`, Titanium `TJ375N529`, timing model `I3`

## Build Contract

The build ran in a new ASCII-path checkout created with `core.autocrlf=false`.
At build start and post-build, `HEAD` equaled the design SHA, the design worktree
was clean, `git diff --check` passed, and all 82 declared atomic-input hashes
matched. Efinity's project-XML run metadata was restored before post-build drift
checking. The committed input list binds actual checkout bytes, including Hard SoC
XML/peri/SDC/IP/top/APB, generated `soc.h`, Hello source/makefile, `start.S`,
included common `.mk` files, BSP `soc.mk`, and `default_i.ld`.

## Offline Results

- Map, Interface, PNR, and PGM: PASS.
- STA: worst setup slack `1.511 ns`; worst reported hold slack `0.026 ns`.
- CDC: `No Synchronizer warnings to report.`
- Pinout: RX `B12 / GPIOR_96_CLK13`, TX `D12 / GPIOR_100`.
- Bitstream SHA-256: `2738A7FBC3737CB88FA893422C3B08088DD8125D8F0FC3FE99C18D741FBE9B67`.
- Hello ELF SHA-256: `C2C5EAF354D5FE7D9292D1B96188FF89267054CCBBE8CD2B222F2205F26E3CA8`.
- Hello ELF entry: `0xF9000000`; one LOAD segment; no undefined symbols.
- Warnings: EFX-0011 `198`, EFX-0200 `20`, EFX-0201 `10`, EFX-0256 `14`, ERROR/FATAL `0`.

`SYSTEM_UART_1_IO_PARAMETER_INIT_CONFIG_DATA_LENGTH=7` encodes eight data bits:
the verified Efinity UART driver writes `dataLength - 1` to the frame field.
The generated settings are 115200, parity NONE, stop ONE.

## Verifier Output

```text
I0_UART1_BUILD_EVIDENCE=PASS batch=I0_UART1_20260719_CLEAN_LF_FINAL design_sha=6effdc3685d696cb4d33f3fbb1c449729ed72e33 inputs=82 artifacts=21
```

## Non-Claims

USER2, UART1 Hello/echo, APB MAGIC, ch0/HDMI board regression, UART2/J52, and
myCobot are all `NOT VERIFIED`. This packet grants no board or motion gate.
