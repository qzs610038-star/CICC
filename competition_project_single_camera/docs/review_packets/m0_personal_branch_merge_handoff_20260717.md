# M0 Personal Branch Merge Handoff

> Date: 2026-07-17
> Source branch: `dev/wsc6090-cpu` on `origin`
> Review target: `82892d3ba1251d6a1eeefbe195e60c08f6688f3d`
> Subject: `docs(m0): checkpoint map path fix evidence`
> Status: **CODEX MERGE REVIEW REQUIRED / PNR HOLD / BOARD NOT VERIFIED**

## 1. Purpose

This packet tells the teammate Codex exactly what was uploaded to the personal
branch, what can be accepted as evidence, and what must still be reviewed before
any part is integrated into `main`.

The target commit contains one RTL portability fix plus its M0 Map evidence and
review records. It does **not** claim M0 PASS, PNR PASS, a usable bitstream, or
board validation.

## 2. Branch Topology Warning

The personal branch must not be merged as a whole without first reconciling it
with the latest `main`.

Local facts at handoff time:

| Item | Value |
|---|---|
| Personal branch HEAD | `82892d3ba1251d6a1eeefbe195e60c08f6688f3d` |
| Target commit parent | `c1651444d5a84be2eaa7dacb6a66d43dccfdf121` |
| Local `origin/main` snapshot | `07373042d1f84cdc048fc42b5752d0cbeb52c471` |
| Merge base | `df45b5ac0362ef1da8cd54a9ceecfd238ffcea3e` |
| Ahead/behind at handoff | personal branch 2 ahead / 28 behind |
| Is target parent an ancestor of `origin/main`? | **No** |

Therefore, review `82892d3` as a single change unit. The safest integration
method is a fresh branch from the then-current `main`, followed by a no-commit
cherry-pick or equivalent patch application so documentation conflicts can be
reviewed before committing. Do not use the old personal branch tip as a new
baseline.

## 3. Exact Scope of `82892d3`

The commit changes exactly 10 files: 5 modified and 5 added, with 1490
insertions and 2 deletions.

| Status | File | Purpose |
|---|---|---|
| M | `CURRENT_STATE.md` | Records the failed Map run, path fix, successful Map retest, warning inventory, and PNR hold. |
| M | `competition_project_single_camera/WORK_LOG.md` | Adds the M0-10 work-log entry. |
| M | `competition_project_single_camera/docs/baseline/m0_post_baseline_delta_20260714.csv` | Adds the path-fix delta and pre/post SHA identities. |
| A | `competition_project_single_camera/docs/debug_sessions/m0_efx0256_interface_audit_20260715.md` | Classifies the 785 undriven-output warnings and preserves PNR hold. |
| A | `competition_project_single_camera/docs/debug_sessions/m0_headless_build_20260715.md` | Preserves the approved pre-fix Map failure record. |
| A | `competition_project_single_camera/docs/debug_sessions/m0_path_fix_map_retest_20260715.md` | Records the post-fix Map PASS and its strict boundary. |
| M | `competition_project_single_camera/docs/review_packets/m0_demo_baseline_review_packet_20260714.md` | Links the new M0 evidence into the baseline packet. |
| A | `competition_project_single_camera/docs/review_packets/m0_pnr_interface_resolution_plan_20260715.md` | Proposes separate gates for LED, JTAG, DSI ch0, and AXI1 ownership. |
| M | `competition_project_single_camera/src/mipi_dsi/dsi_tx_top.v` | Replaces one root-absolute memory path with a repository-relative path. |
| A | `final_project/docs/review_packets/m0_board_execution_packet_20260715.md` | Provides a future board checklist; every board item remains pending. |

No G4-A or G4-B file is part of this commit.

## 4. RTL Change to Review

The only RTL semantic change is the `INITIAL_CODE` argument in
`competition_project_single_camera/src/mipi_dsi/dsi_tx_top.v`:

```diff
-.INITIAL_CODE("/src/mipi_dsi/Panel_1080p_reg.mem")
+.INITIAL_CODE("src/mipi_dsi/Panel_1080p_reg.mem")
```

Intent: remove a drive-root-dependent path and use the memory file already
contained in the candidate project. No Verilog logic, timing, reset, clock,
interface, XML, SDC, or IP setting is changed.

## 5. Evidence That May Be Accepted

Subject to independent diff review, the submitted records support only these
claims:

1. The pre-fix command-line Map run stopped at elaboration because the effective
   `$readmemh` path was `/src/mipi_dsi/Panel_1080p_reg.mem`.
2. The memory file existed inside the project; the failure was not evidence that
   the `.mem` file was absent.
3. After changing the effective path to
   `src/mipi_dsi/Panel_1080p_reg.mem`, the recorded Efinity 2025.2 Map run exited
   successfully and the old `$readmemh` error disappeared.
4. The successful Map exposed 785 `EFX-0256` undriven-output events and 223
   coded VERI/VDB warnings across 15 categories. These warnings are not approved
   as ignorable.
5. The submitted review documents intentionally retain **PNR HOLD**.

The post-fix source SHA-256 recorded by the packet is:

`DDAF952AE26A599A988235FBE5EB2815AA9C8FDE66B7346EB359A24996F031D7`

## 6. Evidence Availability Limitation

The source worktree still contains the referenced Map outputs and the archived
pre-fix evidence directory, but they are ignored or untracked and are **not**
contained in commit `82892d3`:

- `competition_project_single_camera/outflow/mem_test.map.out`
- `competition_project_single_camera/outflow/mem_test.warn.log`
- `competition_project_single_camera/outflow/mem_test.err.log`
- `competition_project_single_camera/outflow/mem_test.map.rpt`
- `competition_project_single_camera/docs/debug_sessions/evidence/m0_map_fail_pre_path_fix_20260715/`

Consequently, a reviewer can review the committed summaries and the RTL diff,
but cannot independently reproduce every raw-log hash from the Git commit alone.
If raw evidence is required for merge approval, stop and request a deliberate
evidence-transfer decision. Do not add generated `outflow` content blindly.

## 7. Required Codex Review

Review findings should be reported before an integration commit. At minimum:

1. Confirm the integration base is the latest `main`, not `c165144`.
2. Review `git show 82892d3 --` and confirm the scope is exactly the 10 files
   listed above.
3. Confirm the relative memory path resolves under the real Efinity project
   working-directory semantics; do not infer this only from Verilog syntax.
4. Reconcile `CURRENT_STATE.md`, `WORK_LOG.md`, the delta CSV, and the two
   existing review packets with newer `main` entries instead of accepting
   conflict resolution by wholesale replacement.
5. Check the corrected warning counts and the EFX-0256 family allocation against
   the available raw logs if those logs are supplied.
6. Keep AXI1, JTAG, DSI ch0, and LED decisions separate. Do not approve manual
   edits to `mem_test.peri.xml`, `mem_test.xml`, `constrain.sdc`, or generated IP
   settings through this packet.
7. Run `git diff --check` on the resolved integration patch.
8. Do not proceed to PNR until the interface-ownership findings are independently
   accepted by Codex.

Suggested inspection workflow:

```powershell
git fetch origin
git switch -c codex/review-m0-path-fix origin/main
git show --stat --oneline 82892d3
git show --name-status --format= 82892d3
git cherry-pick -n 82892d3
git diff --cached --check
git diff --cached --name-status
```

The `cherry-pick -n` step is for inspection only. Resolve or reject conflicts
before creating an integration commit. The reviewer may choose an equivalent
patch-based workflow.

## 8. Explicitly Not Verified

- PNR, placement, routing, and IO binding
- STA and the safety of historical negative-slack paths
- CDC
- bitstream generation and bitstream/source identity
- FPGA programming
- camera input, HDMI output, cold boots, or a 10-minute board run
- CPU APB/OSD integration
- USER2, UART0, UART2/J52, or myCobot operation
- overall M0 or G4 board gates

## 9. Merge Decision Boundary

Accepting this packet may justify integrating the one-line portable path fix and
its accurately bounded historical records. It must not be interpreted as
approval to merge the entire stale personal branch, continue PNR, generate or
flash a competition bitstream, or mark any board-level gate PASS.

Expected Codex verdict format:

```text
[Verdict]
APPROVE / CHANGES_REQUESTED / BLOCKED

[Findings]
- Findings ordered by P0/P1/P2 with file and line evidence.

[Integration Scope]
- Exact files accepted, rejected, or requiring manual reconciliation.

[Evidence Gaps]
- Whether raw untracked Map evidence is required before merge.

[Gate Statement]
- MAP evidence status
- PNR status
- board status
```
