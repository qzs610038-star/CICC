# Priority-3 myCobot CPU Migration Design

> Date: 2026-07-09
> Scope: board CPU side migration plan for myCobot 280 control.
> This document is a design handoff only. It does not modify C firmware or run arm motion.

## 1. Decision Basis

The migration must follow the current final-route boundary:

- `AGENTS.md` section "分赛区决赛主线": FPGA handles video front-end, ROI/statistics, OSD and necessary acceleration; board CPU handles recognition decision, parameter management and myCobot control.
- `分赛区决赛实施开发路线.md` section 5.5: PC plus `pymycobot` is only for early point calibration and safety verification; final control returns to board CPU.
- `CURRENT_STATE.md` route overrides dated 2026-07-08: V2.10 to V2.12 PC findings are now governed by Codex review plus Claude log cross-check.
- `mycobot_pc_tests/audit_logs/v2_codex_review_migrated_findings.md` section 5: Priority-3 migration rules are the authoritative precondition for this design.

Important correction carried into this design: normalized UART polling intervals, such as 50 to 100 ms, are an engineering pacing rule, not the proven root cause of run-23. Do not describe "serial overflow prevention" as the Codex ruling or as the direct fix for run-23.

## 2. Current Skeleton Readout

Current state after the 2026-07-09 skeleton pass:

| File | Current fact | Migration consequence |
| --- | --- | --- |
| `final_project/cpu/app/include/arm_controller.h` | New public API for states, errors, point table, callbacks and nonblocking controller context. | Treat this as the current merge boundary. Future work should extend this header deliberately instead of adding ad-hoc private structs in `.c`. |
| `final_project/cpu/app/src/arm_controller.c` | Implements init/configure, plan validation, one-shot grab request, cooperative `tick()`, soft-arrival confirm counting, `none_count`, gripper callback stages and fault states. | This is still a no-hardware skeleton: it can call injected callbacks, but `main.c` does not wire it to UART and no real arm motion is enabled. |
| `final_project/cpu/app/include/mycobot_protocol.h` | Public protocol frame API, parser status enum, local `pymycobot 4.0.5` command IDs and helper range constants. Current base frame is `0xFE 0xFE LEN CMD PAYLOAD 0xFA`, matching local `pymycobot.common` for the `MyCobot` class. | Core IDs/helpers are grounded in local PC evidence; official/manual or logic-analyzer evidence is still required before real board TX. CRC-class robots exist in `pymycobot`, but the PC scripts use `MyCobot`, not the CRC robot class. |
| `final_project/cpu/app/src/mycobot_protocol.c` | Builds bounded `0xFE 0xFE len cmd payload 0xFA` frames, parses complete RX frames, and now includes pure helpers for `SEND_ANGLES`, `GET_ANGLES`, gripper state and gripper value based on local `pymycobot 4.0.5` evidence. | Safe for unit tests and future UART RX buffering. It still does not touch UART/MMIO, and command completion semantics remain readback-driven rather than reply-code-driven. |
| `final_project/cpu/app/include/mycobot_transport.h` / `mycobot_transport.c` | Defines a pure-C RX ring buffer, frame extractor over `mycobot_parse_frame()`, and a TX frame cursor that lets a future UART adapter pop bytes according to FIFO availability. Counters cover RX malformed/overflow cases plus TX queued, popped, busy rejects and build failures. | This is the future UART ISR/polling handoff boundary. It currently has no `bsp.h`, no `soc.h`, no MMIO and no interrupt dependency. |
| `final_project/cpu/params/arm_positions.h` / `arm_positions.c` | Defines the no-motion CPU parameter table: `g_arm_safe_position` plus `g_arm_default_plan` with HOME, HOME_READY, PICK_HOVER, PICK, DROP_HOVER and DROP converted from `teach_points_my_new_test.json`. | Safe for pure-C tests and future main integration. It is not wired to `main.c`, UART, MMIO, or any real transport path. |
| `final_project/integration/mycobot_protocol_notes.md` | Records local `pymycobot 4.0.5` evidence for frame format, command IDs, payload lengths, byte order, scaling, and remaining official/manual/grab-bag uncertainties. | Use it as the current software protocol source of truth for pure-C helper work, while still requiring logic-analyzer/scope confirmation before board TX touches the real arm. |
| `final_project/cpu/tests/test_mycobot_arm_skeleton.c` | New host/QEMU-oriented test source for frame build/parse and controller state-machine skeleton. | It is intentionally not wired into the main firmware Makefile to avoid interfering with ongoing CPU development. Compile it explicitly when touching the arm skeleton. |

Neighboring constraints:

- `final_project/cpu/app/include/board_io.h` already reserves `OFF_CPU_ARM_STATE = 0x064` and `OFF_CPU_ERROR_CODE = 0x068`, and exposes `board_io_write_global_state()`.
- `final_project/cpu/app/src/main.c` is still a heartbeat skeleton. It does not call classifier, task matcher or arm controller.
- `final_project/cpu/CPU_MODULE_PLAN.txt` states recognition side will output `match_action` and optional grab center; mechanical arm status, errors and execution belong to `arm_controller`.

Verification done for this skeleton pass:

```powershell
$gcc='D:\Efinity\efinity-riscv-ide-2025.2\toolchain\bin\riscv-none-embed-gcc.exe'
$out='final_project/cpu/tests/build_mycobot_skeleton'
New-Item -ItemType Directory -Force $out | Out-Null
& $gcc -march=rv32imac -mabi=ilp32 -O0 -g -Wall -Wextra -I'final_project/cpu/app/include' -c 'final_project/cpu/app/src/mycobot_protocol.c' -o "$out/mycobot_protocol.o"
& $gcc -march=rv32imac -mabi=ilp32 -O0 -g -Wall -Wextra -I'final_project/cpu/app/include' -c 'final_project/cpu/app/src/arm_controller.c' -o "$out/arm_controller.o"
& $gcc -march=rv32imac -mabi=ilp32 -O0 -g -Wall -Wextra -I'final_project/cpu/app/include' -c 'final_project/cpu/tests/test_mycobot_arm_skeleton.c' -o "$out/test_mycobot_arm_skeleton.o"
```

Initial result: all three objects compiled with the installed RISC-V toolchain. Later Batch 2 work added `final_project/cpu/tests/run_mycobot_arm_skeleton_host.ps1`, which executes the same skeleton tests through Efinity RISC-V GCC plus QEMU semihosting when no native C compiler is available.

## 2.1 Relationship With Type-C UART1 Debug Console

The UART1 debug console plan and this myCobot migration are related but should not be collapsed into one implementation.

Strict hardware order:

1. Finish the UART1 plan Phase 0/1/2 path first: HDMI checkpoint, minimal SoC, Type-C UART boot banner, `ping -> pong`.
2. Export the real `.peri.xml`, `soc.h`, linker and debug profile.
3. Only after that should any board-side myCobot UART path be wired or compiled as production firmware.

Safe software parallelism:

- While the hardware/FPGA side builds the SoC and UART1 console, this arm stack can advance in pure C using mocks.
- Keep tests callback-based and do not depend on `bsp.h`, `soc.h`, APB addresses or UART registers.
- Do not route UART2 or myCobot commands through the Type-C UART1 console. UART1 is for logs/control console at 115200; myCobot remains a separate future UART path at 1000000.

Merge rule:

- UART1 plan owns SoC/UART base infrastructure and generated address truth.
- myCobot plan owns protocol framing, arm state machine, point safety and motion policy.
- The first integration point is a narrow transport adapter that implements `arm_controller_ops_t` using the confirmed myCobot UART peripheral. Do not make `arm_controller.c` include `bsp.h` directly unless a review packet approves that dependency.

## 2.2 Review Of Gemini CPU-Arm Link Guide

Gemini's five-hop data-flow model is useful and should be kept as a debug checklist:

```text
C state machine/protocol
  -> MMIO write/read using generated soc.h addresses
  -> SoC AXI/APB fabric and UART FIFO
  -> UART shift register and baud divider
  -> FPGA IO pin, VCCIO, GND and crossed TX/RX wiring
  -> myCobot controller parser and motor execution
```

Disposition of its four patches:

| Gemini patch | Codex disposition | How to apply here |
| --- | --- | --- |
| PLIC/RX interrupt and ring buffer | Stage-gated accept, not an immediate hard requirement. | For production motion firmware, prefer UART RX interrupt -> ISR pushes bytes into a ring buffer -> parser drains frames cooperatively. For early UART1 boot banner and no-motion Batch 1/2, callback mocks and bounded polling are acceptable. If current SoC lacks confirmed UART interrupt/PLIC wiring, do not block pure-C work; instead record it as a board-integration gate before sustained motion tests. |
| Closed-loop wait-reach state | Accept. | Current skeleton already polls angles during motion states. Future fill-in should make this explicit as `SEND_TARGET -> WAIT_REACH -> REACHED/NEXT` or equivalent substates, with 10 Hz-class readback, consecutive-confirm timing and timeout. |
| FPU requirement for IK | Reject as a blanket requirement; accept as conditional risk. | The current plan intentionally uses fixed taught joint angles and avoids IK in the first board loop. Keep fixed-point/table math by default. If later vision-to-arm coordinate mapping or IK is introduced, first check generated `.peri.xml`, actual `-march/-mabi`, linker and math library support; current `app/Makefile` is `rv32imac`, so assuming F/D extensions would be unsafe. |
| Safety guard before first motion | Accept, with stricter wording. | First board-side test must not connect the real arm. Capture the FPGA TX waveform with logic analyzer/scope and verify header, length, command, payload, footer and baud rate. Only after voltage, VCCIO, GND and crossed TX/RX are confirmed should the real arm be connected for read-only testing. |

Important nuance: TX/RX FIFO pacing is a real transport requirement, but it is not the Codex-reviewed root cause of run-23. Keep "check UART FIFO availability/occupancy" in the transport layer, while keeping the run-23 root-cause language focused on firmware confirmation false failure and missing post-failure readback.

## 3. PC Behavior To Migrate

Only migrate behavior that belongs in the final board CPU loop:

| PC-side behavior | C-side target |
| --- | --- |
| Five-point sequence: `pick_hover`, `pick`, `drop_hover`, `drop`, `home_ready`, then HOME | `arm_positions` table plus `arm_controller` one-shot pick/drop state machine. |
| Safety gates: radius `R_MAX`, short-pair joint delta, return transition delta, `home_ready` arm diff gates | Preflight validation function before any motion command is accepted. |
| Nonblocking `send_angles` plus soft-arrival polling for short upward moves | Timer-driven C state that sends once, polls actual angles, and advances on soft arrival. |
| Confirm count equals consecutive valid OK readings | C poller keeps `confirm_count`; invalid reads increment `none_count` and do not become OK. |
| Soft timeout followed by post-failure readback and bounded retry | Failure policy in `arm_controller`: read angles/coords if possible, soft-pass if within tolerance, otherwise retry once at low speed, then fault. |
| EOF/Ctrl+C release fix | Not directly portable, but C side needs explicit fault classes and safe release/hold policy instead of silent exit. |

Do not migrate:

- PC interactive prompts.
- `pymycobot` itself.
- Tee/log file mechanics.
- Multi-run N loop as a board requirement. Board firmware should execute one debounced grab/drop transaction per stable `MATCH_ACTION_GRAB`.
- Any claim that V2.12 retry is runtime-proven. It is code-reviewed and happy-path safe, but the retry branch did not trigger in run-25.

## 4. Initial C Module Contract

### 4.1 `mycobot_protocol`

Required responsibilities:

- Build TX frames with complete payload copy and bounded output length.
- Define command IDs only after cross-checking with the official/myBlockly/pymycobot protocol source used by this arm. The current `GET_ANGLES`, `SEND_ANGLES`, `SET_GRIPPER_STATE` and `SET_GRIPPER_VALUE` IDs are grounded in local `pymycobot 4.0.5` source and recorded in `final_project/integration/mycobot_protocol_notes.md`.
- Parse RX frames from a byte stream: frame header, length, command, payload, malformed-frame rejection and timeout status.
- Provide typed helpers for:
  - send joint angles, nonblocking
  - read joint angles
  - read Cartesian coordinates, if the protocol and timing are stable enough
  - set gripper state
  - release servos only under an explicit safe-release state
  - power on, if required by final startup flow

Design rule: `mycobot_protocol.c` must stay protocol-only. It should not know pick/drop sequence states or target matching policy.

### 4.1.1 `mycobot_transport`

Current pure-C scope:

- Owns only byte buffering, frame extraction and parser counters.
- `mycobot_transport_rx_push_byte()` is the intended future handoff from UART RX polling or ISR.
- `mycobot_transport_next_frame()` drains complete `0xFE 0xFE LEN CMD ... 0xFA` frames into `mycobot_frame_t`.
- `mycobot_transport_tx_queue_frame()` builds one complete TX frame into an internal cursor.
- `mycobot_transport_tx_pop_byte()` lets a future adapter transmit one byte only when the hardware TX FIFO reports availability.
- Malformed bytes are resynchronized by dropping the minimum leading byte and preserving counters.
- It is safe for host/QEMU tests because it does not include `bsp.h`, `soc.h`, UART registers, PLIC headers or any board address.

Future board integration rule:

- A real UART adapter may feed this ring from polling or ISR, but the adapter must be a separate file, for example `mycobot_uart_transport.c`.
- ISR work should remain limited to draining hardware RX FIFO into the ring and updating overflow counters.
- TX FIFO availability checks belong in the future adapter. The adapter should call `mycobot_transport_tx_pending()` and pop at most the number of bytes the real FIFO can accept.
- Do not implement the real adapter until the latest teammate CPU/SoC artifacts provide generated `soc.h`, UART base address, FIFO status semantics, baud-divider clock and interrupt/PLIC facts.

### 4.2 `arm_positions`

Required positions:

- `HOME`
- `HOME_READY`
- `PICK_HOVER`
- `PICK`
- `DROP_HOVER`
- `DROP`

Recommended source for first migration:

- Prefer the newest stable preset `mycobot_pc_tests/presets/teach_points_my_new_test.json` for current pick/drop repeatability.
- Keep `teach_points_run12_3cm_inboard.json` as a known historical fallback.
Current C status:

- `final_project/cpu/params/arm_positions.c` now provides `g_arm_default_plan` from `mycobot_pc_tests/presets/teach_points_my_new_test.json`.
- The historical `teach_points_run12_3cm_inboard.json` remains recorded as fallback provenance, not an active default.
- Values are converted to fixed-point C units: joint angles `deg_x10`, coords `x10`, radii `mm_x10`.
- This table is still no-motion configuration only; `main.c` and UART/MMIO transport remain untouched.

Minimum metadata per point:

- six joint angles in `deg_x10`
- optional coords in mm and orientation `x10` if C side uses coordinate residuals
- speed class
- gripper target
- safety radius and validation notes

### 4.3 `arm_controller`

Proposed public API:

```c
typedef enum {
    ARM_STATE_IDLE = 0,
    ARM_STATE_PRECHECK,
    ARM_STATE_PICK_HOVER,
    ARM_STATE_PICK_DOWN,
    ARM_STATE_GRIP_CLOSE,
    ARM_STATE_PICK_LIFT,
    ARM_STATE_DROP_HOVER,
    ARM_STATE_DROP_DOWN,
    ARM_STATE_GRIP_OPEN,
    ARM_STATE_DROP_LIFT,
    ARM_STATE_RETURN_HOME_READY,
    ARM_STATE_RETURN_HOME,
    ARM_STATE_DONE,
    ARM_STATE_FAULT,
    ARM_STATE_ESTOP
} arm_state_t;

typedef enum {
    ARM_ERR_NONE = 0,
    ARM_ERR_PROTOCOL_TIMEOUT,
    ARM_ERR_BAD_FRAME,
    ARM_ERR_TARGET_INVALID,
    ARM_ERR_PRECHECK_FAILED,
    ARM_ERR_SOFT_TIMEOUT,
    ARM_ERR_POST_READ_FAILED,
    ARM_ERR_RETRY_FAILED,
    ARM_ERR_UNSAFE_RELEASE_REQUIRED
} arm_error_t;
```

Initial callable shape:

- `arm_controller_init(ctx, arm_id)`
- `arm_controller_configure(ctx, plan, ops, user)`
- `arm_controller_plan_validate(plan)`
- `arm_controller_request_grab(ctx)`
- `arm_controller_tick(ctx, now_ms)`
- `arm_controller_cancel(ctx, reason)`
- `arm_controller_get_state(ctx)`
- `arm_controller_get_error(ctx)`
- `arm_controller_get_none_count(ctx)`

The controller should be nonblocking. `main()` can call `arm_controller_tick()` every loop after `board_io_heartbeat()` and write `ARM_STATE/ERROR_CODE` through `board_io_write_global_state()`. The current skeleton deliberately does not accept `fused_result` or `grab_center`; that handoff should be added only after the recognition main loop contract is stable.

## 5. Motion Policy

### 5.1 Soft Arrival

For short upward moves, mirror the reviewed PC strategy:

- Send target angles once with a nonblocking command.
- Poll actual angles on a timer.
- Use soft tolerance:
  - short moves: about `3.0 deg`
  - HOME return: about `1.5 deg`
- Require two consecutive valid OK readings.
- On invalid read, increment `none_count`. Do not count it as success. Do not claim this as serial-overflow mitigation.
- Poll interval should be timer-based and bounded, initially 50 to 100 ms unless later board UART tests justify a faster value.

Implementation detail for the next fill-in pass:

- Split each moving command into explicit send and wait phases, even if the enum names stay compact:
  - `SEND_TARGET`: enqueue/send the `SEND_ANGLES` frame once.
  - `WAIT_REACH`: periodically request `GET_ANGLES`, consume parser results, and update current angles.
  - `REACHED`: advance only after the tolerance condition stays true for the configured confirm window, initially equivalent to about 200 ms or two valid samples, whichever is implemented first.
- The parser must be nonblocking. It consumes bytes already buffered by the transport layer; it must never busy-wait inside `arm_controller_tick()`.
- A wait phase must always carry a deadline. Timeout enters the post-failure readback/retry policy instead of hanging the state machine.

### 5.2 Soft Timeout And Retry

When soft arrival times out or the protocol-level completion/confirm path fails:

1. Read actual angles and, if available, coords.
2. If post-failure readback is inside soft tolerance, soft-pass with a warning state. Do not fault.
3. If outside tolerance, retry the same angle target once at a low speed derived from PC `SOFT_REFINE_SPEED = 8`.
4. Read back again.
5. If still outside tolerance, enter `ARM_STATE_FAULT` and preserve a conservative failure boundary.

This is the C-side form of Codex (C). It must be documented as "code-review-derived strategy; V2.12 retry branch was not runtime-triggered in run-25".

### 5.3 Release And Hold Policy

Avoid unconditional servo release during automatic-phase failures. PC logs repeatedly note that direct release while the arm is raised or carrying load can cause drop/sag risk.

Board CPU policy should be:

- For communication or arrival failure during raised/loaded states: hold torque if possible, publish fault state, wait for human intervention.
- Only execute release when the state is explicitly `ESTOP_RELEASE_ALLOWED` or after user-confirmed safe posture in a separate service mode.
- Record whether the gripper is assumed closed/open in the controller state.

## 6. Integration With Recognition Loop

Recommended one-shot flow:

1. Recognition produces stable fused result.
2. `task_matcher_evaluate()` returns `MATCH_ACTION_GRAB`.
3. `main()` latches one transaction if `arm_controller` is idle.
4. `arm_controller` owns the full sequence until `DONE` or `FAULT`.
5. While busy, repeated `MATCH_ACTION_GRAB` should not start another transaction.
6. On `DONE`, require a new object-present edge or target-valid refresh before accepting another transaction.

This prevents repeated triggers while the same object remains visible.

Status writeback:

- Map `arm_state_t` to `OFF_CPU_ARM_STATE`.
- Map `arm_error_t` to `OFF_CPU_ERROR_CODE`.
- Commit global state through the existing `CAM_COMMIT_GLOBAL` path.

## 7. Protocol And Hardware Open Items

Do not write production C motion code until these facts are resolved:

- Which board UART reaches myCobot 280, at what voltage level and connector.
- Whether the UART is a dedicated SapphireSoC UART, a custom FIFO/register bridge, or another peripheral.
- Final baud rate for myCobot remains `1000000`; terminal/debug UART can remain separate at 115200.
- True SoC address map must come from generated `soc.h`; current `bsp.h` is provisional.
- Protocol command IDs, payload scale and response format must be verified against the exact myCobot 280 control interface used in PC testing.
- Whether C side can reliably read coords. If coords are too slow or unreliable, post-failure soft pass may initially use angles only and mark coord residual unavailable.
- UART TX path must check hardware FIFO availability before writing each byte. Long frames such as six joint angles must not assume the FIFO can accept the whole frame at once.
- UART RX path must drain hardware FIFO into a software ring buffer. Production motion firmware should use UART RX interrupt/PLIC if the generated SoC confirms it; otherwise a polling fallback must prove bounded latency and no RX overflow before motion is allowed.
- UART baud divider must be computed from the actual generated bus/peripheral clock, not the placeholder `SYSTEM_CLINT_HZ` in `bsp.h`.
- Physical wiring must be verified from board IO documentation and the final constraint file: VCCIO level, GND common reference, FPGA TX -> myCobot RX, FPGA RX -> myCobot TX.
- Do not assume CRC for this arm. Local `pymycobot.common` shows `MyCobot` frames use `0xFA` footer, while CRC is used for other robot classes. The exact deployed protocol variant must be recorded in `mycobot_protocol_notes.md`.
- Do not assume hardware FPU. Current C skeleton and first board loop must avoid IK and heavy floating point. If IK or vision-to-arm coordinate transform is introduced, review `.peri.xml`, `Makefile` `-march/-mabi`, ABI, and `-lm` support first.

## 8. First Implementation Batches

Batch 1, no arm motion: started by this skeleton pass.

- Done: public headers for `mycobot_protocol` and `arm_controller`.
- Done: bounded `0xFE 0xFE LEN CMD PAYLOAD 0xFA` frame builder that copies payload and rejects undersized output buffers.
- Done: minimal RX parser for complete frames.
- Done: standalone test source covering valid, truncated and malformed frames.
- Done: confirmed core command IDs, payload scaling, byte order and no-CRC `MyCobot` frame path from local `pymycobot 4.0.5`; evidence recorded in `final_project/integration/mycobot_protocol_notes.md`.
- Done: typed pure-C helpers for `SEND_ANGLES`, `GET_ANGLES`, gripper state and gripper value payloads, including speed/value range rejection based on the recorded local `pymycobot` evidence.
- Remaining: confirm official/manual or logic-analyzer evidence before any board TX path is allowed to connect to the real arm; gripper release semantics remain deliberately disabled until the exact firmware variant is known.
- Done: added `final_project/cpu/params/arm_positions.c` with `g_arm_default_plan` converted from `teach_points_my_new_test.json`; the table is compiled only by the standalone test runner and is not wired into production `main.c`.
- Done: test coverage checks the default point table passes `arm_controller_plan_validate()` and that tightened radius, short-delta, return-delta and home-ready gates fail as expected.

Batch 2, still no motion: partially started by this skeleton pass.

- Done: `arm_controller` state enum, error enum and cooperative tick skeleton.
- Done: preflight validation for radii, pick/drop short-pair deltas, drop_hover->home_ready return delta and home_ready arm diff.
- Done: preflight validation now rejects invalid point speed, gripper speed and gripper open/closed values before any transport callback can run.
- Done: mocked test source for happy path, soft-timeout/`none_count`, post-readback soft pass and bounded retry/failure paths.
- Done: explicit post-failure readback and one bounded retry state. `ARM_ERR_SOFT_TIMEOUT` is preserved as the pre-retry event, retry success clears it back to `ARM_ERR_NONE`, and retry evidence failure enters `ARM_ERR_RETRY_FAILED`.
- Done: warning/soft-pass status when post-failure readback is already inside tolerance.
- Done: regression coverage that a failed angle read resets `confirm_count`, preserving consecutive-confirm semantics.
- Done: added `final_project/cpu/tests/run_mycobot_arm_skeleton_host.ps1`.
- Done: current machine has no native compiler available in PATH, so the test runner falls back to the Efinity RISC-V GCC plus QEMU semihosting branch. The branch now uses a generated semihosting exit startup plus a test-only `__assert_func` override, runs the assertions, and returns the QEMU exit code.
- Done: added `mycobot_transport` pure-C RX ring/parser with tests for noise discard, partial frame wait, bad-footer resync, ring wrap and overflow counters.
- Done: added `mycobot_transport` pure-C TX frame cursor with tests for frame byte order, busy rejection, completion, abort, invalid args and payload-too-long failure.

Batch 3, board read-only:

- Build with real `soc.h`.
- Verify separate debug UART print and myCobot UART register access without sending motion commands.
- If safe and user confirms hardware setup, read arm angles only.

Current safe stop line before teammate CPU handoff:

- Batch 1 and Batch 2 pure-C work can continue only for tests, docs and non-MMIO helpers.
- The next implementation step that would add `mycobot_uart_transport.c`, compile it into production firmware, touch `main.c`, or choose UART register/status-bit semantics is blocked until the latest teammate CPU/SoC artifacts are available.
- Required teammate artifacts: generated `.peri.xml`, generated `soc.h`, UART instance ownership, UART base address, TX FIFO availability semantics, RX FIFO drain semantics, baud-divider clock, interrupt/PLIC route, linker/debug profile, and the board-level pin/voltage mapping for the myCobot UART.
- Without those artifacts, any code that reads or writes real UART registers would be guessing and must not be merged.

Batch 4, controlled motion after Codex review:

- Send one nonblocking HOME or already-near HOME command at safe speed.
- Validate soft-arrival poller.
- Only then enable pick/drop sequence.

## 8.1 Fill-In Checklist For A Low-Cost Agent

Work in this order. Do not skip review gates.

1. Protocol evidence
   - Start from the current `final_project/integration/mycobot_protocol_notes.md` evidence table, not from memory.
   - Keep `GET_ANGLES`, `SEND_ANGLES`, `SET_GRIPPER_STATE` and `SET_GRIPPER_VALUE` aligned with the local `pymycobot 4.0.5` source unless newer official/manual/logic-analyzer evidence contradicts it.
   - If adding a new command, update `mycobot_protocol_notes.md` first with command ID, payload bytes, units, response format and evidence path, then add constants/helpers/tests.
   - Do not enable gripper release or servo release from automatic motion states until the exact firmware value and safe-release state are reviewed.

2. Protocol typed helpers
   - Add encode/decode helpers in `mycobot_protocol.c`.
   - Keep helpers pure: input struct -> payload bytes, RX frame -> output struct.
   - Add tests in `test_mycobot_arm_skeleton.c` for edge angles, invalid lengths and malformed responses.

3. Retry state details
   - Extend `arm_state_t` with internal retry/readback substates only if needed.
   - Add callbacks for optional read coords if protocol evidence supports it.
   - Implement Codex (C): post-failure readback -> soft-pass warning if in tolerance -> one low-speed retry -> final fault.
   - Keep retry count bounded to 1 by default.

4. Position table
   - Current default is already converted from `mycobot_pc_tests/presets/teach_points_my_new_test.json` into `final_project/cpu/params/arm_positions.c`.
   - If changing points later, update the JSON preset provenance, `g_arm_default_plan`, and the skeleton tests in one patch.
   - Preserve source preset name, radii and safety gates in comments.
   - Keep the old run12 preset as fallback documentation, not an active default unless explicitly chosen.

5. Transport adapter
   - Wait for UART1 plan Phase 1/2 to produce real `soc.h` and UART facts.
   - Reuse the existing pure-C `mycobot_transport` RX ring/parser and TX cursor instead of parsing or buffering UART bytes inside the adapter.
   - Add a small adapter file, for example `mycobot_uart_transport.c`, that implements `arm_controller_ops_t`.
   - Do not add direct MMIO or UART polling to `arm_controller.c`.
   - Implement TX byte writing with FIFO availability checks.
   - Implement RX as UART FIFO -> ring buffer -> frame parser. Prefer PLIC/interrupt once hardware confirms the UART interrupt route; keep ISR work limited to reading bytes and updating ring indices.
   - Add counters for RX overflow, bad header, bad footer, malformed length and parser resync.

6. Main integration
   - Integrate only after recognition main loop and target matcher handoff are stable.
   - Latch one `MATCH_ACTION_GRAB` transaction while idle.
   - Publish `ARM_STATE/ERROR_CODE` through `board_io_write_global_state()`.
   - Do not trigger motion from UART1 console commands.

7. First board safety test
   - Do not connect the mechanical arm.
   - Configure the confirmed myCobot UART at `1000000` and transmit a harmless test/read frame.
   - Capture FPGA TX with logic analyzer/scope; verify baud, `0xFE 0xFE`, `LEN`, `CMD`, payload and `0xFA`.
   - Confirm VCCIO and cable crossing before connecting the arm for read-only tests.
   - Run read-only status/angle requests before any command that can move motors, gripper or servos.

## 9. Required Review Packet Before C Motion

Before any code path can move the arm, prepare a Codex Review Packet containing:

- Modified files and diff summary.
- UART path, connector, voltage and baud-rate evidence.
- Protocol command IDs and payload scaling evidence.
- Protocol frame variant evidence: footer `0xFA` vs CRC-class protocol, with source path.
- TX FIFO availability strategy and RX ring-buffer strategy, including whether PLIC/UART RX interrupt is enabled or why a bounded polling fallback is acceptable for the current phase.
- State machine diagram or table.
- Full point table source and safety validation output.
- Unit test results for protocol parser and arm state machine.
- Explicit note that V2.12 retry branch is not runtime-proven, unless a later run has triggered it.
- Fault/release policy and how a human can stop the arm.
- Logic analyzer/scope evidence for first board-side TX frames before connecting the real arm.
- If IK or floating point is introduced: generated CPU extension evidence, `-march/-mabi`, ABI and math library/linker evidence.

## 10. Step-One Conclusion

Step one confirms the C-side arm stack is still at contract-skeleton stage. The next correct action is not to edit motion logic immediately, but to implement and test protocol/state-machine scaffolding in non-motion batches. The PC V2.10 to V2.12 work gives strong behavior requirements, especially soft arrival and post-failure readback, but the final board CPU implementation still needs protocol verification, UART wiring confirmation and Codex review before any real movement.
