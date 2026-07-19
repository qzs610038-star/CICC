# 最终日控制板（qzs Goal 0）

> 状态：`ACTIVE CONTROL PLANE / NO HARDWARE ACTION AUTHORIZED`
>
> 建立日期：2026-07-18（Asia/Shanghai）
> Goal 0 基线：`codex/qzs-final-integration-goals-20260718@f47af290c2f014dfa8a131a3baebec1e9560ae21`
> 本 Goal 既有 dirty（排除，不归属本次骨架）：`docs/agent_context/QZS_TASK_BREAKDOWN_AND_STRONG_GOALS_20260718.md`
> 冻结真源不在本文件复制：[`TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md`](TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md)、[`I0_UART1_INTERFACE_FREEZE.md`](../../competition_project_single_camera/integration/I0_UART1_INTERFACE_FREEZE.md)。

## 使用规则

- 仅接受候选 ref 与**完整 40 位 SHA**；`最新`、短 SHA、口头状态均不是可审查输入。
- 固定顺序为：libaoxun 固定 SHA 审查 → wsc 固定 SHA 审查（消费同批 `soc.h`）→ qzs 集成/最终刷新。任何越序条目保持 `BLOCKED`。
- `I0-SMOKE` 只在同一批输入、bitstream、ELF、`soc.h` 和用户批准窗口齐全时进入；没有原始证据时不得写 `PASS`。
- UART2/J52、myCobot 查询/接线/动作始终不在本板范围内。

## 控制板

| Owner / work item | Candidate ref / complete SHA | Allowed scope | Excluded scope | Current status | Dependencies | Required evidence | Next Gate | Stop condition | Sources |
|---|---|---|---|---|---|---|---|---|---|
| libaoxun / I0-BUILD atomic UART1 hardware batch | `NOT PROVIDED / NOT VERIFIED` | Single-camera RTL/test scope plus `mem_test.xml`、`mem_test.peri.xml`、`constrain.sdc`、Hard SoC IP/BSP atomic tree, per ownership table | CPU business/test, qzs docs/tools, frozen semantics, UART2/J52/myCobot | `BLOCKED — fixed ref/SHA and packet absent` | Route freeze; Efinity-generated UART1 atomic tree | Candidate diff list; Efinity version; atomic-input SHA; Map/PNR/STA/CDC; warnings; bitstream SHA-256; same-batch `soc.h` | Goal 1 qzs read-only review | Any missing/mixed-batch evidence, generated-file hand edit, UART0/R0 inheritance, or scope breach | Ownership §3–5; [I0 freeze](../../competition_project_single_camera/integration/I0_UART1_INTERFACE_FREEZE.md); [new-batch index](../../competition_project_single_camera/docs/review_packets/I0_UART1_NEW_BATCH_EVIDENCE_INDEX_20260718.md) |
| wsc / classifier regression + UART1 Hello | `NOT PROVIDED / NOT VERIFIED` | `cpu/src/**`, `cpu/tests/**`, `cpu/README.md`, `cpu_bringup/uart1_hello_onchip/**` only | Frozen headers, RTL/XML/SDC/IP, qzs state/governance docs, UART2/J52/myCobot | `BLOCKED — fixed ref/SHA and approved same-batch soc.h absent` | Goal 1 `APPROVE`; `SYSTEM_UART_1_*` from same-batch `soc.h` | Candidate diff list; `/W4 /WX` actual runs/exit codes; test counts; same-batch `soc.h` identity; ELF SHA-256 and layout | Goal 2 qzs read-only review | C4127 not actually closed, skipped executable, guessed UART address/IRQ, hash mismatch, or scope breach | Ownership §3–5; [CURRENT_STATE](../../CURRENT_STATE.md); [new-batch index](../../competition_project_single_camera/docs/review_packets/I0_UART1_NEW_BATCH_EVIDENCE_INDEX_20260718.md) |
| qzs / fixed-SHA semantic integration and final refresh | `NOT PROVIDED / NOT VERIFIED` | Governance/status/evidence/tools/report paths assigned to qzs | CPU/RTL/XML/SDC/IP/BSP; frozen files and manifest without full user phrase; all hardware operations | `BLOCKED — Goal 1/2 approval inputs absent` | Approved libaoxun SHA, approved wsc SHA, cleanly separable unrelated dirty | Goal 1/2 packets; actual inclusion/exclusion list; offline gate log; final freshness/scope/freeze records | Goal 3, then Goal 5 refresh | Either review is not `APPROVE`, scope cannot be isolated, interface change is needed without phrase, or validation FAIL | Ownership §3–5; [review template](review_packet_template.md); [five-line template](INTEGRATION_FIVE_LINE_REPORT_TEMPLATE.md) |
| qzs + user-approved operator / one I0-SMOKE window | `NOT PROVIDED / NOT VERIFIED` | Matching bitstream + on-chip Hello ELF + USER2 + Type-C UART1 `115200 8N1` Hello/echo + read-only APB MAGIC | USER1, Flash, DDR, I1-I4 business probes, UART2/J52, myCobot transport/query/action | `NOT VERIFIED — no approved window or current UART1 batch` | Goal 3 result; user confirms board, artifacts, connection, stop plan and window | User approval record; batch/bitstream/ELF hashes; same-batch `soc.h`; USER2/PC/UART1/APB raw evidence | Goal 4 only after all evidence preflight checks pass | Stop at first mismatch/failure; do not fallback to UART0 or continue to APB after UART failure | [Runbook](operations_runbook.md); [I0-SMOKE draft](../../competition_project_single_camera/docs/debug_sessions/I0_SMOKE_OPERATION_CARD_DRAFT_20260718.md) |
| qzs / post-I0 report and handoff refresh | `NOT PROVIDED / NOT VERIFIED` | `CURRENT_STATE.md`, `SESSION_HANDOFF.md`, qzs evidence/report material after real inputs exist | Any CPU/RTL/Hard-SoC change; report claims unsupported by matched evidence | `NOT VERIFIED — awaits Goal 3/4 evidence` | Goal 3, plus Goal 4 if run | Current branch/HEAD/dirty; packets; source logs; PASS/FAIL/WARN; explicit remaining `NOT VERIFIED` | Goal 5 | Missing source record, contradictory current conclusion, or future result presented as PASS | Ownership §4/6; [CURRENT_STATE](../../CURRENT_STATE.md); [five-line template](INTEGRATION_FIVE_LINE_REPORT_TEMPLATE.md) |

## Required handoff tuple

Every later Goal adds one immutable tuple to its review packet before qzs changes any current-status text:

`ref | full SHA | source repo state | allowed/excluded paths | batch ID | source/evidence location | conclusion | remaining NOT VERIFIED | stop point`

If a frozen interface or `interface_freeze_manifest.json` would need a difference, record the proposed difference in a Review Packet and stop for the complete user phrase; this control board grants no such authority.
