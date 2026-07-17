# 2026-07-17 无板调试强 Goal 包

> 分支：`codex/no-board-debug-plan-20260717`（基线 `9acf4d8`）。
>
> 本文只定义可在无板、无串口、无 FPGA 下载条件下独立完成并验收的工作。当前硬件事实、禁止项和下一 Gate 以根目录 `CURRENT_STATE.md` 为准；本文不改变任何 Gate。

## 共用边界

- 禁止：打开 COM、发送 UART 字节、Efinity 构建/下载、Programmer/JTAG、Flash、USER1、DDR、UART2/J52、myCobot 帧及动作。
- 可用：Host C 测试、Python 单测、Markdown/PowerShell 静态检查，以及写入 `%TEMP%` 的测试制品。
- 不把 Host/Mock、compile-only 或日志 bundle 表述为板级、APB、UART 或机械臂通过。

## G-NB-01：myCobot 查询事务的时间边界回归

**Agentmemory action**：`act_mrok18kg_b4e416aa84e7`
**优先级**：P0；预计 60–90 分钟；可独立执行。

### 成功定义

只扩展 `final_project/cpu/tests/test_mycobot_arm_skeleton.c` 的 Host 测试，证明 `mycobot_transaction` 在下列输入下保持 fail-closed：

1. 749 ms 不超时、750 ms 超时；
2. `uint32_t` 时基回绕后 deadline 比较正确；
3. 错误命令、错误长度、错误域的帧不会结束在途事务；
4. 首个合法响应结束事务，迟到/重复帧只增加计数；
5. 未结束事务拒绝第二次 `begin`（single-flight）。

验收必须同时满足：

```powershell
powershell -ExecutionPolicy Bypass -File final_project\cpu\tests\run_mycobot_arm_skeleton_host.ps1
git diff --check
git diff -- final_project/cpu/tests/test_mycobot_arm_skeleton.c
```

- Host runner 退出码为 `0`；
- 新断言均覆盖上述五类场景；
- diff 仅限测试和必要的测试说明。若现有实现不能满足新增断言，停止在 red-test 证据处，先开 Review Packet；不得为了通过测试启用 UART2 或 REAL backend。

### 实施步骤

| 步骤 | 内容 | 依赖 | 风险与缓解 |
|---|---|---|---|
| 1 | 读取 `mycobot_transaction.c` 和既有 200–273 行测试，写出输入/状态/计数预期表 | — | 低；以源码而非旧计划为准 |
| 2 | 仅添加五类边界测试 | 1 | 中；保持单文件、小 diff |
| 3 | 运行 Host runner 与 `git diff --check` | 2 | 低；运行目录必须在 `%TEMP%` |
| 4 | 将命令、退出码、断言数和未验证边界写入 Review Packet 草稿 | 3 | 低；明确 `HOST_ONLY` |

### 关联阅读

- `final_project/cpu/app/src/mycobot_transaction.c`
- `final_project/cpu/tests/test_mycobot_arm_skeleton.c`
- `final_project/docs/review_packets/mycobot_b1_b3_protocol_transaction_review_20260715.md`

## G-NB-02：纯离线 presubmit 安全通道

**Agentmemory action**：`act_mrok18l2_398fadf9ae38`
**优先级**：P0；预计 60–90 分钟；可与 G-NB-01 并行。

### 成功定义

新增 `tools/offline_presubmit.ps1`，只聚合下列无硬件检查，并以一个清晰的退出码和摘要报告结果：

1. `tools/agent_handoff_health_check.ps1`；
2. `tools/project_freshness_check.ps1`；
3. `tools/agent_context_budget.ps1`；
4. `python -m unittest final_project/tools/board_observability/tests/test_g2_run_bundle.py`；
5. `competition_project_single_camera/cpu/tests/run_g2_host_evidence.ps1`（run dir 只能在 `%TEMP%`）；
6. `git diff --check`。

脚本必须显式拒绝或完全不包含 serial/COM、Efinity、Programmer/JTAG、Flash、USER1、DDR、UART2/J52、myCobot 或运动命令。三次连续运行均 exit `0` 才可称为通过。

### 实施步骤

| 步骤 | 内容 | 依赖 | 风险与缓解 |
|---|---|---|---|
| 1 | 列出既有 runner 的参数、产物目录与是否硬件安全 | — | 低；不执行未审命令 |
| 2 | 实现 fail-fast PowerShell 聚合器与临时 run dir | 1 | 中；不写源码目录、不吞退出码 |
| 3 | 添加最小自检：脚本文本禁止词与 `%TEMP%` 路径断言 | 2 | 低；避免未来误扩范围 |
| 4 | 连续运行三次并记录 exit code/摘要 | 2 | 中；任一 WARN/FAIL 需如实保留 |

### 不在本 Goal 内

- GitHub Actions、Efinity 构建、RISC-V 真实 ELF、APB/MMIO、UART2、板级或机械臂测试；
- 修复 `CURRENT_STATE.md`、merge register 或 handoff 的共享治理内容（见外部协作计划）。

## 执行顺序

```text
G-NB-01（协议边界） ─┐
                    ├─> 各自 Host 验收 ─> 独立 Review Packet/提交候选
G-NB-02（presubmit） ─┘
```

两项互不依赖。若共享工作区出现队友硬件证据或 `CURRENT_STATE.md` 更新，暂停修改共享入口，保留本分支的测试/工具小范围 diff。
