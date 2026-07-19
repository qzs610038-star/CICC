# P0/P1 qzs + wsc 并行开工索引

> 状态：`KICKOFF PREPARED / P0-A + P1 HOST APPROVED / P0-B PACKET ONLY / NO BOARD ACTION`
>
> 集合分支：`codex/qzs-wsc-libaoxun-integration-20260718`

## 1. 一次同步原则

本次准备全部放在同一集合分支。wsc 只需在保护现有 dirty 的前提下，精确 fetch 一次该分支并在个人开发分支合入该固定 kickoff 提交；不得全量 fetch 触发 Windows 大小写 ref 冲突，不得 stash/reset/clean 覆盖本地工作。qzs 本机直接从当前集合工作区开新对话，无需额外同步。

```powershell
git status --short --branch
git fetch origin refs/heads/codex/qzs-wsc-libaoxun-integration-20260718:refs/remotes/origin/codex/qzs-wsc-libaoxun-integration-20260718
git ls-remote --heads origin refs/heads/codex/qzs-wsc-libaoxun-integration-20260718
```

若当前工作树 dirty，使用新的 sibling worktree 或在现有个人分支只读取远端提交；不得强切。实际 kickoff SHA 以 qzs 最终推送回执为固定值，并在新对话第一条证据中记录。后续实现各自在个人分支提交，不直接推集合分支。

## 2. 必读顺序

1. `AGENTS.md`
2. `CURRENT_STATE.md`
3. `docs/agent_context/P0_P1_NO_UART_CPU_AND_F1_PARALLEL_IMPLEMENTATION_PLAN_20260719.md`
4. `docs/agent_context/P1_HOST_CONTRACT_AND_EVIDENCE_PREWORK_20260719.md`
5. `docs/agent_context/TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md`
6. 自己的提示词：`docs/agent_context/prompts/`

libaoxun 不在本轮派工名单内：不发送新提示词、不要求拉取、不要求审阅，保持其现有 UART1/USER2 对话和工作区不受影响。

## 3. 当前可开工与禁止项

| 角色 | 立即开工 | 条件触发/暂不做 |
|---|---|---|
| qzs | 不变量表、向量/采集/OSD/输入/证据模板、P0-A verifier 规格、集成核验 | P0-B 只维护 Packet；不构建、不改硬件 |
| wsc | P0-A canary/阶段码/有界 UART probe；P1 黄金模型、Host 协议、packer、输入事务、标定与20轮回放 | 不改冻结头文件、RTL/XML/SDC/IP |
| libaoxun | `OUT OF THIS DISPATCH`；继续既有 UART1/USER2 工作 | 本轮不拉取、不审阅、不接 P0/P1 新任务 |

共同禁止：纯 FPGA 分类迁移、UART0 回退、双摄恢复、UART2/J52、myCobot 查询/动作、无口令修改冻结接口、把 Host 结果写成板级 PASS。

## 4. 首批交付与 Gate

- 6 小时：wsc + qzs 提交 `P0-A-READY` 候选包；只有 fresh 编译、ELF/map/disassembly、TX 永不 ready 负例、canary 地址/范围/hash/停止条件齐备才可标 READY。
- 12 小时：qzs/wsc 只整理 P0-A 可供未来 PC/断点/RAM canary 使用的操作输入；是否与 libaoxun 协调另开窗口，待其现有 UART checkpoint 后再决定。本开工包不授权执行。
- 18 小时：USER2 仍阻塞时只申请裁定 P0-B；Review Packet 已准备，但保持 `HOLD`。
- P1 只能达到 `P1-HOST-READY`，其定义见前置包；不得冒充 F1-board。

## 5. 合并回收

wsc 回传个人分支、完整 SHA、范围、`git diff --check`、测试命令/exit code、原始日志索引、未验证项和安全状态。qzs 只审查 wsc CPU/Host 来源并刷新治理状态；本轮不回收或合并 libaoxun 工作。任何未来原子硬件输入变更必须独立 Review Packet，不与 Host 提交混合。
