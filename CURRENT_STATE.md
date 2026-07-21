# CURRENT_STATE — CPU Hello UART1 三人候选

> 当前候选分支：`codex/qzs-wsc-libaoxun-cpuhello-candidate-20260721`。
> 本文件只记录当前候选的事实、接收门和禁止项；历史失败索引保留在 `competition_project_single_camera/docs/debug_sessions/UART0_R0_HISTORICAL_INDEX_20260718.md`。

- 证据索引：`competition_project_single_camera/docs/review_packets/CPU_HELLO_UART1_H0_H6_RECEIPT_20260721.md`
- 证据路径：`competition_project_single_camera/integration/I0_UART1_INTERFACE_FREEZE.md`
- 证据路径：`docs/agent_context/LIBAOXUN_UART1_NEXT_STAGE_HANDOFF_20260721.md`

## CURRENT_SNAPSHOT

- 当前集成来源：`origin/codex/qzs-wsc-libaoxun-integration-20260718@0e5ab490559c58642734b0095753c6cf8787c709`。
- 已保留本候选 P1 Host 模型、20 轮回放、schema、tamper 负例及 feature-adapter fail-closed 回归。
- 当前 CPU Hello 路线由全体队员确认：SoC UART1 → Type-C UART1，`115200 8N1`，RX=`GPIOR_96/B12`，TX=`GPIOR_100/D12`，片上 RAM=`0xF9000000..0xF9003FFF`。
- 当前端口身份由全体队员确认：`COM17` 为 Type-C UART1 CH340；`COM10`、`COM13` 为 J44/USER FTDI，禁止作为 UART1 候选。
- 当前 H3 路由由全体队员确认：物理 TAP ID=`0x006A0EF3`，BSCAN=`6,1`，inner IR=`9`。旧 inner IR=`8`、期待 CPU TAP=`0x006A0A79` 和“H3 永久失败”均为历史失败索引，不定义当前路线。
- `UART0/G1/R0`、旧 TAP、旧 COM 映射和旧批次制品均为 `HISTORICAL / SUPERSEDED`，不得作为执行入口或证据继承来源。

## GATE_LAYERS

| 层级 | 当前状态 | 不可外推的结论 |
|---|---|---|
| I0-BUILD | `ROUTE_CONFIRMED_PENDING_FIXED_COMMIT` | 不等于板级配置或 CPU 执行 |
| H0 静态合同 | `WAITING_LIBAOXUN_FIXED_SHA` | 不等于端口身份 |
| H1 端口身份 | `TEAM_CONFIRMED_PENDING_FIXED_EVIDENCE` | 不等于 UART 收发 |
| H2 易失 FPGA 配置 | `WAITING_LIBAOXUN_FIXED_SHA_AND_EVIDENCE` | 不等于 USER2 可达 |
| H3 USER2/DTM | `TEAM_CONFIRMED_ROUTE_PENDING_FIXED_EVIDENCE` | 不等于 RAM load 或 CPU resume |
| H4 RAM load、PC gate、resume、UART1 TX Hello | `WAITING_LIBAOXUN_FIXED_SHA_AND_BOARD_EVIDENCE` | 不等于 RX echo 或 APB |
| H5 UART1 RX echo | `WAITING_LIBAOXUN_FIXED_SHA_AND_BOARD_EVIDENCE` | 不等于 APB |
| APB MAGIC | `NOT_VERIFIED` | 不等于 I1/APB/CDC |
| I1/APB/CDC | `NOT_VERIFIED` | 不等于 CPU→OSD |
| CPU→OSD | `NOT_VERIFIED` | 不等于机械臂许可 |
| UART2/J52/myCobot | `NO_GO` | 不受 CPU Hello Gate 放宽 |

## CURRENT_BLOCKERS

1. 唯一等待输入是 libaoxun 的固定提交 SHA、同批源码/制品 identity、H0–H5 原始证据索引和脱敏摘要；接收要求见 `competition_project_single_camera/docs/review_packets/CPU_HELLO_UART1_H0_H6_RECEIPT_20260721.md`。
2. 在固定 SHA 到达前，不得把 H4/H5、`CPU_HELLO`、`BOARD_PASS`、`APB_MAGIC`、`I1_APB_CDC` 或 `CPU_TO_OSD` 写为 PASS。
3. P1 Host 与所有 Host/mock 回归只证明离线软件行为；不得作为 RISC-V、UART1、APB、OSD 或板级闭环证明。
4. H4/H5 与所有后续板级 Gate 保持 `NOT VERIFIED`，直到固定 SHA 的证据接收表闭合。
5. myCobot 链路速率 `1000000` 与 UART1 CPU Hello 路线独立；当前仍为 NO-GO，禁止外推或执行动作。

## NEXT_GATE

1. qzs 按完整 SHA 审查 libaoxun 提交的父提交、实际文件、原子输入、manifest/verifier 和 H0–H5 证据。
2. 若 XML、IP、SDC、top、BSP、`soc.h` 或 APB 输入变更，作为完整原子批次审查；旧 bitstream、ELF、Map/PNR/STA/CDC、warning 和板级记录全部失效。
3. 合并后 fresh 重跑 P1、adapter、classifier、F1、runtime/G2、manifest/tamper、offline presubmit、freeze、freshness、handoff 与 `git diff --check`；结果只以新合并 HEAD 实跑值记录。
4. 仅当 H0–H5 证据闭合后，才可写 `CPU_HELLO=PASS`。H5 PASS 仍不等于 APB PASS。

## PENDING_DECISIONS

- 是否在收到 libaoxun 固定 SHA 后接受其完整原子批次，取决于 H0–H5 证据、构建输入和 scope 声明的审查结果。
- 在 H5 关闭前，CPU Hello、APB MAGIC、I1/APB/CDC 和 CPU→OSD 均不得标记 PASS。

## DEPRECATED_ROUTES

- UART0/G1/R0、旧 COM 映射、inner IR=8、期望 CPU TAP `0x006A0A79` 与“H3 永久失败”均为 `HISTORICAL / SUPERSEDED`，不得定义当前 I0 路线。

## HISTORY_ARCHIVE_INDEX

- `debug_records/state_history/archive_manifest.md`
- `competition_project_single_camera/docs/debug_sessions/UART0_R0_HISTORICAL_INDEX_20260718.md`

## POST_MERGE_REFRESH_REQUIRED

- 收到固定 SHA 后，重跑 freeze、scope、freshness、handoff、P1、adapter、classifier、F1、runtime/G2、manifest/tamper、offline presubmit 与 `git diff --check`。
- 仅以新合并 HEAD 的实跑结果刷新计数、hash、warning 和最终状态。

## FINAL_GATE_STATUS

```text
CPU_HELLO=WAITING_LIBAOXUN_FIXED_SHA
BOARD_PASS=NOT_CLAIMED
APB_MAGIC=NOT_VERIFIED
I1_APB_CDC=NOT_VERIFIED
CPU_TO_OSD=NOT_VERIFIED
UART2_J52_MYCOBOT=NO_GO
```
