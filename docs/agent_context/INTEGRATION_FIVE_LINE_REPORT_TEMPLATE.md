# 集成五行报模板

> 用途：每个 qzs 集成/最终刷新 Goal 结束时提交的事实快照。
> 填写规则：每行必须有完整 SHA 或明确 `NOT PROVIDED`、可复查来源/证据、状态和停止条件。未执行的项目填 `NOT RUN` 或 `NOT VERIFIED`，不填未来 `PASS`。

| Line | Required report text | Default status | Required source / evidence | Stop condition |
|---|---|---|---|---|
| 1. 今日系统事实 | `Branch=<full branch>; HEAD=<40-char SHA or NOT PROVIDED>; dirty=<preserved paths>; route=single-camera J48/ch0; I0=UART1 Type-C` | `NOT VERIFIED` until source is captured for this report | `git status --short --branch`, `git rev-parse HEAD`, [`CURRENT_STATE.md`](../../CURRENT_STATE.md) | Missing branch/HEAD/dirty record or source contradiction |
| 2. 今日关闭风险 | `Closed only=<real command/packet result>; open findings=<FAIL/BLOCKED/WARN list>` | `NOT VERIFIED` | Command log with exit code, fixed-SHA Review Packet, warning record | No raw result/exit code; do not call a risk closed |
| 3. 本轮仍未验证 | `NOT VERIFIED/NOT RUN=<I0 USER2, UART1, APB, OSD, I1-I4, UART2/J52, myCobot as applicable>` | `NOT VERIFIED` | Current evidence index and raw-log references | Item lacks matching current-batch evidence |
| 4. 对 wsc 的依赖 | `WSC ref=<full SHA or NOT PROVIDED>; required=<C4127 evidence, same-batch soc.h, UART1 Hello ELF identity>` | `BLOCKED` by default | Candidate diff + test/build packet + [`I0 new-batch index`](../../competition_project_single_camera/docs/review_packets/I0_UART1_NEW_BATCH_EVIDENCE_INDEX_20260718.md) | Missing SHA, unapproved libaoxun batch, guessed UART facts, or scope breach |
| 5. 对 libaoxun 的依赖 | `libaoxun ref=<full SHA or NOT PROVIDED>; required=<UART1 atomic tree, tool/input hashes, Map/PNR/STA/CDC/warnings, bitstream/soc.h identity>` | `BLOCKED` by default | Candidate diff + Efinity packet + [`I0 new-batch index`](../../competition_project_single_camera/docs/review_packets/I0_UART1_NEW_BATCH_EVIDENCE_INDEX_20260718.md) | Missing SHA/evidence, mixed batch, UART0 inheritance, or scope breach |

## Required footer

`Sources checked: <paths/commands>; Evidence locations: <relative paths>; Next Gate: <one gate>; Stop point: <one explicit condition>; No hardware action performed unless a separately approved I0-SMOKE record says otherwise.`
