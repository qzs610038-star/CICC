# qzs F1 Preboard 新对话启动提示词

> 使用时把 `{{PUBLISHED_PLAN_SHA}}` 替换为本次 qzs 交接消息给出的完整 SHA。

```text
你现在负责第十届集创赛单摄项目的 qzs F1 Preboard 数据/证据流。请直接执行，不要只给方案。

固定种子：
- repo: D:\第十届集创赛-雄芯院材料
- remote branch: codex/qzs-wsc-p0a-p1-integration-20260720
- exact plan SHA: {{PUBLISHED_PLAN_SHA}}
- personal branch: codex/qzs-f1-preboard-data-evidence-20260720
- recommended worktree: D:\CICC-qzs-f1-preboard-data-evidence-20260720

第一步先读完整 AGENTS.md、CURRENT_STATE.md、以下三份文件：
1) docs/agent_context/QZS_WSC_F1_PREBOARD_KICKOFF_INDEX_20260720.md
2) docs/agent_context/QZS_F1_PREBOARD_PERSONAL_EXECUTION_PLAN_20260720.md
3) docs/agent_context/QZS_WSC_F1_PREBOARD_INTERFACE_CONTRACT_20260720.md
并读冻结/所有权文件。运行 handoff health check、git status、git worktree list。确认 exact plan SHA 存在；若个人分支/工作区不存在，从该 SHA 新建，若已存在则只核验后继续。禁止 reset/clean/stash 覆盖现有修改。

然后按 Q0->Q5 连续实施；Q2 实物采集若当前没有相机/样本，不能伪造，先完成 Q1 schema/validator、Q3 可审计黄金提取器及合成边界测试、Q5 B0 verifier/H1 Packet，把 Q2 标为明确阻塞并继续其他可做项。每个 checkpoint 都运行 team_scope(qzs)、interface freeze、diff-check 和相应正负例，形成 repo 内 evidence。不要重复制造已有 37/39/54/213、648/648、20轮合成证据。

硬边界：只写 qzs 范围；不改 CPU/RTL/top/XML/SDC/IP/BSP/soc.h/embedded_sw/冻结 integration；不猜 APB；P0_B=HOLD；ARM_ENABLED=0；不碰 UART2/J52/myCobot。不要切换、fetch 到、merge 到或写入 libaoxun 的活动 UART1/USER2 分支/工作区，不要求他暂停或审阅。

完成后在个人分支提交并推送，回报完整 SHA、文件范围、命令/exit code、数据 batch/hash、未验证项和下一交接输入。除非用户另行要求，不合并 main，也不直接推双人种子分支。
```
