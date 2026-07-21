# QZS Goal 1 — libaoxun UART1 I0-BUILD fixed-SHA audit

**Conclusion: APPROVE**

This approval is limited to the offline UART1 I0-BUILD evidence chain.  It does
not approve USER2, a UART1 terminal/echo result, APB MAGIC, ch0/HDMI, UART2/J52,
myCobot, programming, wiring, or any board action.

## Fixed review identity

| Item | Fixed value |
| --- | --- |
| Remote ref | `dev/libaoxun688-uart1-i0-20260719-cleanlf-final` |
| Evidence commit | `72cc281bd104726d9db1e88cb2894facb1d5fd1a` |
| Evidence parent | `1130e4fdb4d1a2d246443bea13991870a0353d5a` |
| Design commit | `6effdc3685d696cb4d33f3fbb1c449729ed72e33` |
| Design parent | `f47af290c2f014dfa8a131a3baebec1e9560ae21` |
| Batch | `I0_UART1_20260719_CLEAN_LF_FINAL` |
| Tool identity | Efinity `2025.2.288.4.15`, Titanium `TJ375N529`, timing model `I3` |

Review start state: local branch
`codex/qzs-final-integration-goals-20260718`, HEAD
`4ec9ca1`, clean.  `tools/agent_handoff_health_check.ps1` passed.  The remote
head above was re-read with `git ls-remote --heads origin`; no branch-name or
"latest" inference was used.

## Scope and atomicity

The full diff from the fixed design SHA to the evidence commit has exactly the
following five additions.  They are **IN_SCOPE** libaoxun I0-BUILD evidence:

1. `competition_project_single_camera/embedded_sw/uart1_hello_onchip/I0_UART1_BUILD_EVIDENCE.md`
2. `competition_project_single_camera/embedded_sw/uart1_hello_onchip/I0_UART1_BUILD_INPUTS.sha256`
3. `competition_project_single_camera/embedded_sw/uart1_hello_onchip/I0_UART1_BUILD_MANIFEST.json`
4. `competition_project_single_camera/embedded_sw/uart1_hello_onchip/rebuild_i0_uart1_clean.ps1`
5. `competition_project_single_camera/embedded_sw/uart1_hello_onchip/verify_i0_uart1_build_evidence.ps1`

No RTL/XML/SDC/IP/wrapper/BSP/APB/top file is changed by this evidence commit;
all are bound to design SHA `6effdc...`.  The committed 82-entry LF input list
includes `mem_test.xml`, `mem_test.peri.xml`, `constrain.sdc`, `src/top.v`,
`src/apb_reg_magic.v`, the consumed Hard SoC IP/wrapper/peri inputs, generated
`soc.h`, Hello `main.c`/makefile, `start.S`, the consumed `.mk` files and
`default_i.ld`.  QZS independently checked all 82 source bytes from a clean
archive of design SHA against that list; the result was PASS.  In particular,
the former wrapper CRLF hash has been replaced by the clean-LF Git byte hash
`53E63128C9E76CD46AD11E979215E13F663F98BB617BB05290404FF343ACB597`.

**Explicitly excluded:** prior R0/UART0 artifacts and conclusions; all
`outflow*`, `work_*`, bitstreams, ELF files and raw logs; unrelated
`final_project/` artifacts; and every USER2, UART1 board, APB, video, UART2 or
myCobot claim.  The build-output/ELF paths used by the rebuild are covered by
the repository ignore rules, so the evidence commit cannot silently include
generated outputs.

## UART1 and build evidence review

- Consumed Efinity Hard SoC `ip/EfxSapphireHpSoc_slb/settings.json` has
  `PERI_UART_0=0`, `PERI_UART_1=1`, `PERI_UART_2=0`; this is a newly generated
  UART1 configuration, not an inherited UART0 conclusion.  `mem_test.peri.xml`
  binds `system_uart_1_io_rxd` to `GPIOR_96` and `system_uart_1_io_txd` to
  `GPIOR_100`; the same-batch Interface/pinout evidence records RX
  `B12 / GPIOR_96_CLK13` and TX `D12 / GPIOR_100`.
- The same-batch generated BSP identity is
  `competition_project_single_camera/embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/include/soc.h`,
  SHA-256 `25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B`.
  It declares UART1 at `0xe8011000`, baud `115200`, parity `NONE`, stop `ONE`;
  data length `7` is the Efinity driver encoding for eight data bits.
- The manifest binds 21 artifacts by relative path, size and SHA-256, including
  Map, Interface/pinout, PNR, STA, CDC, warnings, bitstream/work LBF and the
  Hello ELF.  Its fail-closed verifier checks clean design SHA, every input,
  every artifact, tool versions/hashes, path containment and required BSP/Hello
  inputs.
- Published same-batch results are Map/Interface/PNR/PGM PASS; STA setup
  `1.511 ns`, hold `0.026 ns`; CDC has no synchronizer warning; warnings
  EFX-0011 `198`, EFX-0200 `20`, EFX-0201 `10`, EFX-0256 `14`, and
  ERROR/FATAL `0`.
- Bitstream SHA-256:
  `D05EFD4EC91FDB51F2997483BA7F4A3634C3E50FF1C65DD7239B41E19E2BC544`.
  Hello ELF SHA-256:
  `919B291A2B980F02507D1591EFBE5D8E395DC53F4A480AE5A8C577D74CF5F7FA`.
  The libaoxun-host verifier record is
  `I0_UART1_BUILD_EVIDENCE=PASS ... inputs=82 artifacts=21`.

## Findings

- **P0: none.** No unbound atomic hardware input, stale CRLF hash, UART0
  fallback, hand-edited generated BSP identity, or out-of-scope candidate file
  was found.
- **P1: none.** The evidence's supplied verifier is fail-closed and the
  candidate's static source/manifest binding is reproducible from the fixed
  design Git tree.
- **P2: declared non-claims.** Raw Efinity outputs remain on the libaoxun
  evidence host by policy and were not rebuilt, programmed, or rehashed on the
  QZS host.  The approval relies on the published libaoxun-host verifier PASS
  and immutable manifest hashes, as explicitly permitted for this review;
  it is not an independent local Efinity execution.

## Consumable handoff

WSC may consume only the fixed design SHA, the generated `soc.h` identity
above, and the UART1 Hello build inputs listed in the manifest.  A later qzs
integration step may consume the fixed bitstream/ELF hashes and the manifest
verifier with the original artifact roots.  It must re-open the affected gates
if any atomic input, tool hash/version, artifact hash, wiring, or observed
failure changes.  Goal 2/3 must obtain separate evidence for USER2, UART1
Hello/echo, APB MAGIC and every board-level item.

## QZS checks

- `git diff --check 6effdc...72cc281...`: PASS.
- Fixed-design archive input-hash audit: PASS (`82` inputs, `21` artifacts
  declared).
- Candidate review-document scan: no local absolute-path disclosure; all
  artifact locations are verifier-supplied relative roots.
- No merge, checkout, Efinity invocation, hardware action, RTL/XML/SDC/IP/BSP
  edit, commit or push was performed by QZS.
