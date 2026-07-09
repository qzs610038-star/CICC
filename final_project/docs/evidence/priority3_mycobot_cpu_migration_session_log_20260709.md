# Priority-3 myCobot CPU Migration Session Log

> Date: 2026-07-09 20:15 +08:00
> Owner: Codex
> Scope: CPU-side myCobot migration, pure-C/no-motion work only.

## 0. Safety Boundary

This session did not connect to a real arm, did not send UART bytes to hardware, and did not modify board-level SoC/FPGA integration.

No-touch boundaries preserved:

- No `main.c` production integration.
- No `Makefile` production wiring.
- No `bsp.h`, generated `soc.h`, `.peri.xml`, RTL, SDC, Efinity XML or pin assignment edits.
- No `pymycobot` dependency added to final CPU firmware.
- No real mechanical-arm motion path added.

The current work stays inside pure C helpers, mocks, tests and design documentation.

## 1. Protocol Evidence And Helpers

Protocol evidence was consolidated from the local `pymycobot 4.0.5` installation and recorded in:

- `final_project/integration/mycobot_protocol_notes.md`
- `final_project/docs/technical_plans/priority3_mycobot_cpu_migration_design.md`

Confirmed local software facts:

- Frame: `FE FE LEN CMD PAYLOAD... FA`
- `LEN = payload_len + 2`
- `MyCobot` path is non-CRC; CRC robot classes are separate in `pymycobot`.
- `GET_ANGLES = 0x20`
- `SEND_ANGLES = 0x22`
- `GET_GRIPPER_VALUE = 0x65`
- `SET_GRIPPER_STATE = 0x66`
- `SET_GRIPPER_VALUE = 0x67`
- Multi-byte values use big-endian signed int16.
- Angles are encoded as `deg * 100`; CPU internal helper takes `deg_x10` and multiplies by 10 when building payload.

Code changes:

- `final_project/cpu/app/include/mycobot_protocol.h`
  - Added command constants and helper range constants.
- `final_project/cpu/app/src/mycobot_protocol.c`
  - `SEND_ANGLES` helper rejects invalid speed instead of silently clamping.
  - Gripper state/value helpers reject invalid speed/value.
  - Gripper release remains deliberately disabled in the automatic helper path until firmware variant is confirmed.
- `final_project/cpu/tests/test_mycobot_arm_skeleton.c`
  - Added protocol helper boundary tests for invalid speed/value/state.

Remaining protocol uncertainties:

- Official/manual or logic-analyzer confirmation is still required before real board TX.
- `SET_GRIPPER_STATE` release semantics differ across docs/classes (`10` vs other variants); do not enable automatic release yet.
- `GET_GRIPPER_VALUE` response payload semantics are not finalized.

## 2. Controller Safety And Retry Skeleton

Existing `arm_controller` skeleton was preserved and tightened.

Current behavior:

- Nonblocking cooperative `arm_controller_tick()`.
- Injected callbacks only: `send_angles`, `read_angles`, `set_gripper`.
- Soft arrival confirmation with consecutive valid reads.
- Read failure resets `confirm_count`.
- Soft timeout enters post-readback.
- Post-readback can soft-pass with warning if already in tolerance.
- Otherwise one bounded retry at `refine_speed`.
- Retry success clears stale `ARM_ERR_SOFT_TIMEOUT`.
- Retry failure enters `ARM_ERR_RETRY_FAILED`.

Additional validation added in this session:

- `arm_controller_plan_validate()` rejects:
  - point speed `0` or `>100`
  - gripper speed `0` or `>100`
  - gripper open/closed value `>100`
  - existing radius/delta/home-ready safety violations

Touched files:

- `final_project/cpu/app/include/arm_controller.h`
- `final_project/cpu/app/src/arm_controller.c`
- `final_project/cpu/tests/test_mycobot_arm_skeleton.c`

## 3. Point Table Migration

The current stable PC teach preset was converted into CPU fixed-point constants.

Source preset:

- `mycobot_pc_tests/presets/teach_points_my_new_test.json`

Fallback provenance:

- `mycobot_pc_tests/presets/teach_points_run12_3cm_inboard.json`

New/updated files:

- `final_project/cpu/params/arm_positions.h`
- `final_project/cpu/params/arm_positions.c`
- `final_project/cpu/params/README.md`

Exported data:

- `g_arm_safe_position`
- `g_arm_default_plan`

Point coverage:

- `HOME`
- `HOME_READY`
- `PICK_HOVER`
- `PICK`
- `DROP_HOVER`
- `DROP`

Unit conversion:

- joints: `deg_x10`
- coords: `x10`
- radii: `mm_x10`

Test coverage:

- Default point table passes `arm_controller_plan_validate()`.
- Tightened radius gate fails as expected.
- Tightened pick/drop short-delta gate fails as expected.
- Tightened drop-hover to home-ready return-delta gate fails as expected.
- Tightened home-ready arm-diff gate fails as expected.

Important: the point table is not wired into production `main.c` or any UART/MMIO path.

## 4. Pure-C Transport Layer

Added a hardware-free transport staging layer:

- `final_project/cpu/app/include/mycobot_transport.h`
- `final_project/cpu/app/src/mycobot_transport.c`

RX side:

- 128-byte RX ring buffer.
- `mycobot_transport_rx_push_byte()`
- `mycobot_transport_rx_push()`
- `mycobot_transport_next_frame()`
- Resynchronizes malformed bytes by dropping the minimum leading byte.
- Reuses `mycobot_parse_frame()`.

RX counters:

- `rx_overflow`
- `noise_bytes`
- `bad_header`
- `bad_length`
- `bad_footer`
- `payload_too_long`
- `frames_ok`

TX side:

- `mycobot_transport_tx_queue_frame()`
- `mycobot_transport_tx_pop_byte()`
- `mycobot_transport_tx_pending()`
- `mycobot_transport_tx_busy()`
- `mycobot_transport_tx_abort()`

TX counters:

- `tx_frames_queued`
- `tx_bytes_popped`
- `tx_busy_reject`
- `tx_build_failed`

Test coverage:

- RX noise discard.
- RX partial frame wait.
- RX bad-footer resync.
- RX ring wrap.
- RX overflow counter.
- TX frame byte order.
- TX busy rejection.
- TX completion and pending count.
- TX abort.
- TX invalid args.
- TX payload-too-long failure.

Important: this is not a real UART adapter. It has no `bsp.h`, no `soc.h`, no MMIO, no PLIC, and no interrupt dependency.

## 5. Test Runner

Updated:

- `final_project/cpu/tests/run_mycobot_arm_skeleton_host.ps1`

Current behavior:

- Tries native C compilers if available.
- On this machine, no native compiler is in PATH.
- Falls back to Efinity RISC-V GCC plus QEMU semihosting.
- Generates a test-only semihosting startup and `__assert_func` override.
- Runs assertions in QEMU and returns QEMU exit code.
- Compiles:
  - `mycobot_protocol.c`
  - `mycobot_transport.c`
  - `arm_controller.c`
  - `arm_positions.c`
  - `test_mycobot_arm_skeleton.c`

Fresh verification commands run during the session:

```powershell
& 'final_project\cpu\tests\run_mycobot_arm_skeleton_host.ps1' -QemuTimeoutSeconds 5
```

Observed result:

```text
RESULT: PASS (QEMU asserts executed).
SCRIPT_EXIT=0
```

Whitespace verification:

```powershell
git diff --check
```

Observed result:

- No whitespace errors.
- Only unrelated CRLF/LF warnings from pre-existing dirty files outside this task scope.

Build cleanup:

- The runner creates `final_project/cpu/build/mycobot_arm_skeleton_host`.
- That generated build directory was removed after verification.

## 6. Subagent Usage Notes

A read-only worker subagent was used for protocol evidence cross-checking.

Useful result:

- It independently confirmed local `pymycobot` command IDs, frame format, length semantics, big-endian int16 behavior and non-CRC `MyCobot` path.

Practical difficulty:

- The subagent output was natural-language evidence, not a patch.
- Some cited line ranges were approximate, so the main model rechecked critical facts before changing code.
- Gripper release semantics were not uniform across sources; final decision was to document the uncertainty and keep automatic release disabled.

Agentmemory status:

- `agentmemory` MCP tools were not exposed in the current Codex tool list.
- `tool_search` for `memory_recall`, `memory_smart_search` and `memory_save` returned no callable tools.
- This was treated as WARN, not a blocker.

## 7. Current Safe Stop Line

Pure-C Batch 1/2 work is now largely exhausted for the current known facts.

Do not implement or merge the real `mycobot_uart_transport.c` yet.

The next implementation step is blocked until the latest teammate CPU/SoC artifacts are available:

- generated `.peri.xml`
- generated `soc.h`
- UART instance ownership
- UART base address
- TX FIFO availability/status semantics
- RX FIFO drain/status semantics
- baud-divider clock
- interrupt/PLIC route
- linker/debug profile
- board-level pin and voltage mapping for the myCobot UART

Without those artifacts, code that reads/writes real UART registers would be guessing.

## 8. Recommended Next Handoff Packet From Teammates

Ask the CPU/SoC teammate to provide:

1. Exact Efinity/Sapphire/Ruby SoC export files.
2. Generated `soc.h` and `.peri.xml`.
3. UART instance chosen for myCobot, separate from UART1 debug console.
4. UART baud clock source and formula for `1000000`.
5. TX FIFO availability register or bitfield.
6. RX FIFO non-empty register or bitfield.
7. Interrupt/PLIC wiring status, or explicit statement that initial board read-only test will use bounded polling.
8. Pin/connector/VCCIO/GND/TX-RX crossing evidence.
9. Confirmation that UART1 debug console bring-up reached `ping -> pong` before myCobot UART integration.

## 9. Files To Review Together

Core CPU files:

- `final_project/cpu/app/include/arm_controller.h`
- `final_project/cpu/app/src/arm_controller.c`
- `final_project/cpu/app/include/mycobot_protocol.h`
- `final_project/cpu/app/src/mycobot_protocol.c`
- `final_project/cpu/app/include/mycobot_transport.h`
- `final_project/cpu/app/src/mycobot_transport.c`
- `final_project/cpu/params/arm_positions.h`
- `final_project/cpu/params/arm_positions.c`
- `final_project/cpu/tests/test_mycobot_arm_skeleton.c`
- `final_project/cpu/tests/run_mycobot_arm_skeleton_host.ps1`

Core docs:

- `final_project/integration/mycobot_protocol_notes.md`
- `final_project/docs/technical_plans/priority3_mycobot_cpu_migration_design.md`
- `final_project/docs/evidence/priority3_mycobot_cpu_migration_session_log_20260709.md`

Do not treat unrelated dirty files in the workspace as part of this CPU/myCobot session unless their owners confirm them.
