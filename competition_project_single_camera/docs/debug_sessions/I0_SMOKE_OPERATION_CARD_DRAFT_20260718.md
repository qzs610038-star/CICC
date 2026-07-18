# I0-SMOKE 一次性操作卡（草案，未授权执行）

> 状态：`DRAFT / NOT APPROVED / NO HARDWARE ACTION PERFORMED`
>
> 目标链仅为：匹配制品 → USER2 → 片上 RAM CPU 生命 → Type-C UART1 Hello/单字符回显 → 同批 `soc.h` 来源的只读 APB MAGIC。
> 本卡不提供可执行命令、不枚举端口、不打开串口、不写入 JTAG/USER2，也不触发 UART2/J52 或任何 myCobot 查询/动作。

## 批准窗口登记（未填即停止）

| Required field | Default | Evidence field to retain | Stop condition | Source |
|---|---|---|---|---|
| User explicit approval for this one window | `NOT PROVIDED` | User message/approval record, timestamp, operator | No explicit approval: do not start | [AGENTS UART safety](../../../AGENTS.md) |
| Board identity, approved artifacts and physical connection confirmation | `NOT PROVIDED` | Board identifier; batch ID; bitstream/ELF SHA-256; connection statement | Any artifact/connection changes after approval: close window | [I0 new-batch index](../review_packets/I0_UART1_NEW_BATCH_EVIDENCE_INDEX_20260718.md) |
| UART2/J52 and myCobot boundary confirmed out of scope | `NOT VERIFIED` | Operator safety checklist record | Boundary not confirmed: do not start | [Runbook](../../../docs/agent_context/operations_runbook.md) |
| Emergency stop / power-off owner and immediate-stop plan | `NOT PROVIDED` | Named responsible person and confirmation | Missing plan: do not start | [AGENTS UART safety](../../../AGENTS.md) |

## Evidence-only execution ledger

The approved operator fills a row immediately after each event. `PASS` is allowed only for the row’s raw evidence; until then every status remains `NOT VERIFIED`.

| Ordered checkpoint | Default status | Evidence fields | Stop condition | Source |
|---|---|---|---|---|
| 0. Same-batch preflight | `NOT VERIFIED` | Time, operator, repo SHA, batch ID, Efinity identity, bitstream/ELF/`soc.h` SHA-256, input-SHA list | Any mismatch/missing value; no USER2 | [New-batch index](../review_packets/I0_UART1_NEW_BATCH_EVIDENCE_INDEX_20260718.md) |
| 1. USER2 + on-chip RAM PC | `NOT VERIFIED` | Tool/board identity, original console/log/screenshot, PC value, legal range source, observed result | USER2 failure, PC not in approved range, reset anomaly; stop and preserve raw evidence | [Operations runbook](../../../docs/agent_context/operations_runbook.md) |
| 2. Type-C UART1 Hello / one-character echo | `NOT VERIFIED` | Enumerated port identity, `115200 8N1`, complete raw transcript, timestamps, sent/received byte count | No hello, garble, reset, wrong port/format, or echo failure; do not continue to APB | [I0 freeze](../../integration/I0_UART1_INTERFACE_FREEZE.md) |
| 3. Read-only APB MAGIC | `NOT VERIFIED` | Address provenance from same-batch `soc.h`, original read output, expected `0x375A0001`, timestamp | No `soc.h` provenance, access fault, or unexpected result; stop without write/probe | [Operations runbook](../../../docs/agent_context/operations_runbook.md) |
| 4. I0-SMOKE conclusion | `NOT VERIFIED` | Links/hashes for checkpoints 0–3, explicit result, warnings, remaining NOT VERIFIED | Any preceding checkpoint is not evidenced PASS; record specific failure only | [New-batch index](../review_packets/I0_UART1_NEW_BATCH_EVIDENCE_INDEX_20260718.md) |

## Non-negotiable stopping boundary

At the first stop condition, record the observed single failure and the evidence references, then end the window. Do not retry through UART0, change a hash or cable mid-chain, use USER1/Flash/DDR, enter I1-I4, or access UART2/J52/myCobot. A later retry is a new approval window and a new evidence record.
