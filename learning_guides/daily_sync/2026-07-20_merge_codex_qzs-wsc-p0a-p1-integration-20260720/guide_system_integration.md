# 角色 C：双人分支的固定 SHA、证据分层与三方门

> 10–15 分钟目标：学会恢复、核验和继续使用专用集成分支。

## 分支名就是第一道防呆门

- 双人：`codex/qzs-wsc-p0a-p1-integration-20260720`
- 三人：`codex/qzs-wsc-libaoxun-integration-20260718`

前者只表示 qzs + wsc，不暗示 libaoxun 的 UART1/USER2 成果已被吸收。

## 可追溯合并链

实际 merge commit 为 `8cadb77dc409e5cb9f311a784148dc4bc44facae`，父提交是 qzs
`7e0149e...` 与 wsc `aaf2058...`。先前 no-commit 结论见
[跨成员审查](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/docs/review_packets/WSC_QZS_CROSS_MEMBER_INTEGRATION_REVIEW_20260720.md)，
落地结果见
[双人集成记录](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/docs/merge_governance/records/2026-07-20_qzs_wsc_p0a_p1_integration.md)。

证据分三层：来源层（SHA/父提交/所有权）、组合层（P0/P1/648/20轮/篡改负例）、
系统层（USER2/UART1/APB/OSD/机械臂）。第三层尚未通过。

## 队友最少同步

```powershell
git fetch origin codex/qzs-wsc-p0a-p1-integration-20260720
git switch --create codex/qzs-wsc-p0a-p1-integration-20260720 --track origin/codex/qzs-wsc-p0a-p1-integration-20260720
```

已有同名本地分支时只需 fetch 后 fast-forward。不要在 dirty 工作树强切、stash、reset
或 clean；正式 `main` 尚未合入。

## 下一 Gate

- qzs + wsc 可继续 Host 集成和证据收口。
- libaoxun 完成实验后应主动给出固定 SHA 和原子批次证据。
- 届时另开三方 Review Packet；不能让本双人分支动态追随其活动分支。
- P0-B 仍 `HOLD`，机械臂 `ARM=0`。

## 自学入口

### 优先赛方资料

- [分支合并治理](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/docs/merge_governance/BRANCH_MERGE_GOVERNANCE.md)
- [当前状态](file:///D:/CICC-qzs-wsc-p0a-p1-integration-20260720/CURRENT_STATE.md)

### 拓展基础知识

- Git merge commit、first-parent、merge-base 与固定 SHA 审查。
- 软件证据、板级证据和安全授权的独立性。

## 快速验收题

1. 为什么本分支不能叫三人集成？
2. `P1_HOST_READY=YES` 能否推出 `BOARD_VERIFIED=YES`？
3. libaoxun 新 SHA 到达后，为什么必须重新审查？
