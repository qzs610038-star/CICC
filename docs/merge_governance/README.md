# main 合并治理与记录

此目录是跨成员合并到本地 `main` 的唯一治理入口。它保存长期裁决规则、合并登记册和逐次记录；不保存 bitstream、ELF、outflow、work、原始截图、未脱敏日志或本机配置。

## 先读什么

1. [`BRANCH_MERGE_GOVERNANCE.md`](BRANCH_MERGE_GOVERNANCE.md)：成员领域优先级、固定 SHA、硬件原子性与安全边界。
2. [`MERGE_REGISTER.md`](MERGE_REGISTER.md)：已合并记录索引与当前顺序。
3. `records/` 中与候选分支最接近的一份记录：复用其已知舍弃项与未关闭 Gate，但重新固定本次 SHA。

## 每次合并的最低记录要求

每次合并到 `main` 必须同时新增一份 `records/YYYY-MM-DD_main_merge_<scope>.md`，并更新登记册。记录必须明确：

- 合并前 `main` SHA、候选远端 ref 和完整 SHA；合并提交本身由 Git 历史定位，避免在提交内记录不可自洽的自身 SHA；
- 实际纳入的文件或提交范围；
- 明确舍弃的文件、提交或结论及原因；
- P0/P1/P2 Findings（无问题也要写“无”）；
- 合并后能成立的新结论，以及仍为 `NOT VERIFIED`、`HOLD` 或 `BLOCKED` 的事项；
- 实际运行的验证命令、结果、未能运行的环境原因，以及下一 Gate。

先按 `BRANCH_MERGE_GOVERNANCE.md` 审查，再按 `tools/agent_handoff_health_check.ps1`、`git status --short --branch`、固定 SHA、`git merge-tree --write-tree` 和受影响构建/测试执行。`CURRENT_STATE.md` 仅追加语义化的当前结论，禁止整文件覆盖。
