# qzs + wsc P0-A / P1 Host 双人集成记录

> 日期：2026-07-20（Asia/Shanghai）
>
> 分支：`codex/qzs-wsc-p0a-p1-integration-20260720`
>
> qzs 基线：`7e0149e7becb3a57767a868c1720aecba34157ae`
>
> wsc 固定来源：`aaf2058e7e05b8cda905b8096db309c65e0da5ef`
>
> 实际 merge commit：`8cadb77dc409e5cb9f311a784148dc4bc44facae`

## 命名与范围

分支名显式使用 `qzs-wsc`，区别于三人分支
`codex/qzs-wsc-libaoxun-integration-20260718`。本分支只集成 qzs 治理侧和 wsc 的
P0-A/P1 Host 固定来源；没有合入、切换、修改或要求同步 libaoxun 的活动分支。

## 实际合并与所有权

- `git merge --no-ff aaf2058...` 由 ort 无冲突完成。
- merge commit 的父提交依次为 qzs `7e0149e...` 与 wsc `aaf2058...`。
- 相对第一父提交的 41 个路径全部通过 `team_scope_check -Role wsc`。
- `cpu/include/**`、`integration/**`、RTL、XML/peri.xml、SDC 与 IP 差分为空。
- 正式 `main` 未修改；本记录不是 main 的 `MERGE_REGISTER.md` 登记。

## 合并后 fresh 验证

| Gate | 结果 |
|---|---|
| P0-A Host | `10/10 PASS` |
| P1 model / adapter / classifier / F1 | `37/37`、`39/39`、`54/54`、`213/213 PASS` |
| runtime/G2 | `648/648 PASS` |
| P1 manifest / tamper | LF-normalized 4 文件 PASS；篡改 `rounds.jsonl` 被拒绝 |
| fresh P1 replay | vectors `11`、rounds `20`、tamper PASS、`ARM=0` |
| interface freeze | `PASS files=8 surfaces=1 route=UART1_TYPEC` |
| offline presubmit | `PASS_WITH_WARNINGS`；freshness `WARN=8 / FAIL=0` |
| `git diff --check` | PASS |

首次并发复验调用中，G2 runner 因漏传必填 `-RunDir` 返回参数错误；随后用独立 TEMP
输出目录按正确命令重跑，得到 `648/648 PASS`。该调用错误没有改动仓库或硬件。

## 裁定边界

```text
TWO_PERSON_INTEGRATION=PASS
P0_A_READY=YES
P1_HOST_READY=YES
BOARD_VERIFIED=NO
USER2_BOARD_GATE=NOT_OPENED
P0_B=HOLD
ARM_ENABLED=0
LIBAOXUN_TOUCHED=NO
MAIN_MERGED=NO
```

P0-A READY 不等于板上 CPU 取指或 UART1 成功；P1 Host READY 不等于真实
MMIO/APB/CDC/OSD 或机械臂闭环完成。
