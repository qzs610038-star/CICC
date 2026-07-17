# G3 Agent 入口瘦身与 freshness 门禁 Review Packet

## 目标与隔离

- 固定基线：`489ab5b0c1773bfcb776cedd9f39b3f088fb4a0f`
- 分支：`codex/g3-context-slim`
- worktree：`D:\CICC-g3-context-slim`
- 仅治理文档与工具；未修改 RTL、CPU 实现、XML、SDC、IP、bitstream/ELF、原始硬件证据或 `SESSION_HANDOFF.md`。
- 未 commit、未 push、未 merge、未删除 worktree。

## 修改前后上下文预算

| 文件 | 前 bytes/lines/tokens | 后 bytes/lines/tokens |
|---|---:|---:|
| AGENTS.md | 22585 / 197 / 6511 | 7153 / 81 / 2056 |
| CLAUDE.md | 10977 / 212 / 3115 | 2928 / 48 / 799 |
| CURRENT_STATE.md | 129522 / 566 / 40044 | 5197 / 70 / 1544 |
| 三入口合计 | 163084 / 975 / 49670 | 15278 / 199 / 4399 |
| 五文件强制阅读链 | — / — / 65241 | — / — / 19970 |

五文件链降低 `69.39%`；所有单文件、三入口与五文件预算均通过。tokenizer 为 `tiktoken 0.12.0 / o200k_base`。

## 约束迁移映射

完整表见 [migration_map.md](../../../docs/agent_context/migration_map.md)。没有直接丢弃未映射内容：稳定规则进入 AGENTS，动态事实进入 CURRENT_STATE，操作命令进入 runbook，模板进入单一模板，历史正文逐字归档。

## 历史归档

- 归档：[CURRENT_STATE_20260717_pre_g3.md](../../../debug_records/state_history/CURRENT_STATE_20260717_pre_g3.md)
- 原/归档 SHA-256：`FC022DBC9839C11C0A47F6FAD3AE21361B1FE10A17318E21C9FF5D41642C2D5B`
- bytes/lines：`129522 / 566`
- 行号映射：归档 1–566 对原 1–566；日期、措辞、证据路径、替代关系和失效条件逐字保留。
- 校验清单：[archive_manifest.md](../../../debug_records/state_history/archive_manifest.md)

## 保留的安全规则

- FPGA/CPU/PC 架构职责和 CPU→OSD / FPGA 渲染边界。
- 四任务×五轮、五色/三形状/三尺寸、十分钟、识别/判断/执行理由、25/25/50、轻取轻放/180°/最大臂展处。
- 当前 SHA 的 Efinity/bitstream/ELF、USER2、CPU 取指、UART0、APB 实读均未验证；旧批次证据不可继承。
- 冷构建后只允许匹配 bitstream + USER2 + `0xF9000000` UART0 Hello；禁止 USER1、Flash、DDR、UART2/J52、机械臂连接和动作。
- UART0 115200 与 myCobot 1000000 隔离。
- Codex Gate、handoff 矛盾报告、合并治理、机械臂动作用户确认。

## 删除的重复动态事实

- AGENTS/CLAUDE/Skills/README 中的具体节点/边数、旧测试计数、旧资源/IO/warning 数、旧构建 PASS、具体历史制品 hash 和随 SHA 变化的板级叙述。
- 这些内容未被销毁：历史 CURRENT_STATE 原文及对应 Review Packet/证据仍可追溯；当前入口只保留状态结论与索引。

## freshness 新增触发

- schema 2 context budgets：max_bytes/max_tokens、required markers、三入口/五文件总量与降低比例。
- 单摄 XML/peri.xml/SDC/src/cpu/cpu_bringup/integration 变化触发 CURRENT_STATE 刷新。
- 稳定入口动态 artifact 数字、过期测试计数、重复 hash/构建 PASS 扫描。
- CURRENT_STATE 必要分层字段、NEXT_GATE、POST_MERGE_REFRESH_REQUIRED、关键安全 marker。
- Markdown 相对链接、历史归档存在性/SHA-256/行数、现有 managed document 与 CBM 检查继续保留。

## README 修复

- `final_project/README.md` 删除旧测试总数、资源/IO 与构建状态副本，改为稳定索引。
- technical plans 与 review packet README 删除固定旧基线/版本状态复制，改指向 CURRENT_STATE 与统一模板。
- `final_project/docs/README.md` 未发现需修改的动态 artifact 数字，保持不变。

## 等价性复核（仅精简入口）

1. FPGA/CPU 职责：AGENTS 可直接回答，PASS。
2. 单摄下一 Gate：CURRENT_STATE 的 NEXT_GATE 可直接回答，PASS。
3. 禁止动作：CURRENT_STATE + AGENTS 可直接回答，PASS。
4. 四任务、轮数、时限：AGENTS 可直接回答并链接官方细则，PASS。
5. 当前 main 板级 PASS：明确没有；相关项 NOT VERIFIED，PASS。
6. UART0/myCobot：115200 / 1000000，PASS。
7. 旧 bitstream 可否继承：不可跨原子批次继承，PASS。

## 验证命令与结果

- 修改前 `agent_handoff_health_check.ps1`：exit 0。
- 修改前 `project_freshness_check.ps1`：exit 1，FAIL=20（旧 CURRENT_STATE 缺失证据路径）。
- Windows PowerShell 5.1 `agent_context_budget.ps1`：exit 0，PASS。
- PowerShell 7 `agent_context_budget.ps1`：exit 0，PASS。
- Windows PowerShell 5.1 `project_freshness_check.ps1`：exit 0，FAIL=0，WARN=3。
- PowerShell 7 `project_freshness_check.ps1`：exit 0，FAIL=0，WARN=3。
- WARN 解释：dirty worktree 是本 Goal 的未提交交付；两份既有维护方案被 manifest/checker 触发但不在允许修改范围，故未越权更新。
- 最终 `agent_handoff_health_check.ps1` exit 0；`git diff --check` exit 0；JSON、允许范围、禁止文件、SESSION 未改、required markers、Markdown 链接与归档 hash/行数检查全部 PASS。

## POST_MERGE_REFRESH_REQUIRED

本分支必须在 G1/G2 之后最后审查和处理。G1/G2 纳入最终 HEAD 后：

1. 在最终 HEAD 重新读取 AGENTS/CLAUDE/CURRENT_STATE/SESSION_HANDOFF/官方细则/主方案/manifest/checker。
2. 重跑 handoff health、freshness、context budget。
3. 仅根据最终 HEAD 的源码/XML/日志/板级证据刷新 CURRENT_STATE；不得继承 G1/G2 dirty worktree 叙述。
4. 更新 manifest `last_verified_commit`、归档/Review Packet 证据与预算表。
5. 重新执行七问等价性、Markdown、marker、archive hash、diff 和范围门后再合并 G3。
