# qzs 最终集成任务拆解与强 Goal 提示词

> 生成日期：2026-07-18（Asia/Shanghai）
> 角色：qzs / Integration Owner / 证据与安全门负责人
> 规划工作分支：`codex/qzs-final-integration-goals-20260718`
> 分支基线：`f47af290c2f014dfa8a131a3baebec1e9560ae21`
> 状态依据：`CURRENT_STATE.md`、`SESSION_HANDOFF.md`、`TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md`
> 使用方式：从 Goal 0 开始逐个执行。每个 Goal 使用一个新对话；前一个 Goal 的验收结果和固定 SHA 是后一个 Goal 的输入。

## 一、qzs 的任务边界

qzs 的核心产出不是代替 wsc 写 CPU，也不是代替 libaoxun 改 RTL/Hard SoC，而是让每一次推进都满足以下条件：

1. 来源、完整 SHA、允许范围和排除范围可追溯；
2. Host、构建、制品、USER2、UART1、APB、OSD、机械臂证据分级明确；
3. 失败、`BLOCKED`、`NOT VERIFIED`、`NOT RUN` 不被合并或报告措辞抹去；
4. libaoxun 固定 SHA → wsc 固定 SHA → qzs 最终刷新，顺序不倒置；
5. UART2/J52、myCobot 接线、查询和动作始终保持独立安全 Gate。

### qzs 允许写入

- `AGENTS.md`、`CLAUDE.md`、`CURRENT_STATE.md`、`SESSION_HANDOFF.md`
- `.agents/skills/**`
- `docs/**`、`final_project/docs/**`、`learning_guides/**`
- `tools/**`
- `competition_project_single_camera/README.md`
- `competition_project_single_camera/docs/**`
- `competition_project_single_camera/integration/**`
- `competition_project_single_camera/tools/**`

### qzs 当前禁止事项

- 不修改 CPU 实现、测试和 UART1 Hello 实现；这些归 wsc。
- 不修改 RTL、XML、SDC、Hard SoC IP/BSP 原子树；这些归 libaoxun。
- 没有完整口令 `确认接口文件修改，已经和wsc、libaoxun、qzs沟通。` 时，不修改冻结接口或其 manifest。
- 不执行 UART2/J52 接线、myCobot 查询、动作或真实 transport 初始化。
- 不把旧 UART0/R0、Host/mock、离线构建或 Hello 证据写成 UART1/APB/OSD/机械臂 PASS。
- 不推送、不合并到 `main`，除非用户在对应 Goal 中明确授权。

## 二、任务依赖与预计投入

| Goal | qzs 任务 | 依赖 | 预计时间 | 环境与空间 | 完成出口 |
|---|---|---|---:|---|---|
| 0 | 建立最终日控制面与证据骨架 | 当前分支 | 30–60 分钟 | PowerShell 7、Git；新增文档通常 < 5 MB | 控制板、证据清单、操作卡草案齐全 |
| 1 | 审查 libaoxun UART1 原子硬件批次 | libaoxun 固定 SHA 与证据包 | 45–90 分钟 | Git、Efinity 原始日志只读；仓库不收二进制制品 | `APPROVE / CHANGES_REQUESTED / BLOCKED` |
| 2 | 审查 wsc classifier/Host/UART1 Hello 批次 | wsc 固定 SHA；Goal 1 提供同批 `soc.h` | 已完成 | Git、MSVC/测试日志只读 | `APPROVE`；固定组合见下方执行状态 |
| 3 | 按固定 SHA 集成并跑离线总门 | Goal 1、2 均 APPROVE | 1–3 小时 | PowerShell、Git、VS2022/QEMU；临时目录使用系统 TEMP | 离线门结果和候选集成 SHA |
| 4 | 执行一次 I0-SMOKE 证据链 | Goal 3；匹配 hash；用户批准硬件窗口 | 30–90 分钟 | 板卡、Type-C UART1、JTAG；禁止 UART2/J52 | USER2→UART1→APB MAGIC 的 PASS 或单点 FAIL |
| 5 | 最终状态、handoff、报告/PPT索引 | Goal 3；通常再依赖 Goal 4 | 45–90 分钟 | PowerShell、Git、Markdown | 状态与证据一致、队友可直接接手 |
| 6 | I0 PASS 后发起 F1-ABI / I1–I4 Review Packet | Goal 4 PASS | 1–2 小时 | 文档/源码只读审查；不直接实现接口 | 下一原子批次的审查问题与 Gate 冻结 |

```text
Goal 0
  ├── Goal 1：libaoxun I0-BUILD 审查 ──┐
  └── Goal 2：wsc CPU/Hello 审查 ──────┴── Goal 3：固定 SHA 集成与离线总门
                                                  ├── Goal 4：I0-SMOKE（需硬件批准）
                                                  │       └── Goal 6：F1-ABI / I1-I4
                                                  └── Goal 5：最终状态与材料索引
```

Goal 1 与 Goal 2 的队友产出可以并行准备；qzs 的最终接收与合并必须遵循 libaoxun → wsc → qzs 刷新顺序。

### Goal 1 / Goal 2 执行状态（2026-07-19 刷新）

- Goal 1：**已完成，结论 `APPROVE`**；固定为 `dev/libaoxun688-uart1-i0-20260719-cleanlf-final@72cc281bd104726d9db1e88cb2894facb1d5fd1a`、batch `I0_UART1_20260719_CLEAN_LF_FINAL`。
- Goal 2：**已完成复审，结论 `APPROVE`**；固定为 `dev/wsc6090-uart1-cpu-20260719@13419d9922f3f8e7585bd43b77491b81b4bc0681` 与上述 libaoxun 批次的组合输入。
- fresh 复验：MSVC `/W4 /WX` 下 classifier `54/54`、F1 `213/213`、feature adapter `33/33`、runtime/G2 `648/648` 均真实运行并 exit 0；`ARM_ENABLED=0`，无 UART2/J52 transport 初始化或 myCobot 帧发送。
- 离线批准不等于板级批准：USER2、Type-C UART1 Hello/回显、APB MAGIC 仍为 `NOT VERIFIED`。
- 审查记录：[Goal 1 最终审计](QZS_GOAL1_LIBAOXUN_UART1_I0_BUILD_FINAL_AUDIT_20260719.md)；[Goal 2 WSC CPU/UART1 审查及 2026-07-19 复审](QZS_GOAL2_WSC_CPU_UART1_AUDIT_20260718.md)。Goal 3 仅可消费这两个固定 SHA。

## 三、统一验收语言

- `PASS`：当前 Goal 要求的命令和证据在本轮真实完成。
- `PASS_WITH_WARNINGS`：硬门通过，但存在逐条列明且不被隐藏的 WARN。
- `FAIL`：已真实执行且观察到失败；必须保留原始证据。
- `BLOCKED`：缺少固定 SHA、匹配制品、必要口令、用户批准或队友输入，不能安全继续。
- `NOT VERIFIED`：没有足够证据证明。
- `NOT RUN`：本轮没有运行；不能继承历史数字。

任何 Goal 结束时都必须输出：分支、HEAD、dirty、实际修改文件、执行命令、exit code、PASS/FAIL/WARN、仍未验证项、下一 Goal 的固定输入。不得只说“已完成”。

## 四、强 Goal 提示词

### Goal 0：建立 qzs 最终日控制面与证据骨架

```text
你现在在仓库 D:\第十届集创赛-雄芯院材料 中执行 qzs 的 Goal 0。

【唯一目标】
在分支 codex/qzs-final-integration-goals-20260718 上建立“最终日控制面与证据骨架”，让后续 libaoxun、wsc 固定 SHA 审查、I0-SMOKE 和最终刷新都有明确入口。不要实现 CPU、RTL 或硬件接口。

【开始前硬检查】
1. 实读 AGENTS.md、CURRENT_STATE.md、SESSION_HANDOFF.md、docs/agent_context/TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md、docs/agent_context/operations_runbook.md。
2. 运行 git status --short --branch、git rev-parse HEAD、tools/agent_handoff_health_check.ps1。
3. 必须确认当前分支恰为 codex/qzs-final-integration-goals-20260718；若不是，停止并报告 BLOCKED，不自动搬运或覆盖 dirty 修改。
4. 记录基线 SHA 和现有 dirty；保留所有不属于本 Goal 的修改。

【允许修改】
仅允许 qzs 文档/工具范围。优先创建或补齐：
- 最终日控制板：队友、候选 ref、完整 SHA、允许/排除范围、状态、依赖、下一 Gate；
- I0 UART1 新批次证据索引骨架：build ID、原子输入 SHA、Efinity 版本、Map/PNR/STA/CDC、warning、bitstream/ELF SHA-256、soc.h、USER2、UART1、APB MAGIC；
- 一次性 I0-SMOKE 操作卡草案：只描述批准窗口、证据字段和停止条件，不实际运行硬件；
- “集成五行报”模板。
复用已有模板和目录，不创建重复真源。

【强制边界】
- 禁止修改 CPU、RTL、XML、SDC、IP/BSP。
- 禁止修改冻结接口和 interface_freeze_manifest.json；若发现确需修改，只写 Review Packet 建议并停止，等待完整接口口令。
- 禁止任何 JTAG、USER2、串口写入、UART2/J52、myCobot 查询或动作。
- 不把 CURRENT_STATE.md 中尚未发生的未来结果预填为 PASS。

【验证】
运行 git diff --check、tools/team_scope_check.ps1 -Role qzs -BaseRef <本Goal开始SHA>、tools/project_freshness_check.ps1、tools/agent_context_budget.ps1、tools/interface_freeze_check.ps1。

【完成标准】
所有新增条目都有来源字段、证据字段、NOT VERIFIED 默认值和停止条件；范围检查 PASS；无冻结接口差分；输出下一 Goal 所需的 libaoxun/wsc 输入清单。不要提交、推送或合并，除非用户另行明确要求。
```

### Goal 1：审查 libaoxun 的 UART1 原子硬件批次

```text
你现在在仓库 D:\第十届集创赛-雄芯院材料 中执行 qzs 的 Goal 1。

【唯一目标】
只读审查 libaoxun 提供的 UART1 I0-BUILD 固定 SHA 和证据包，判断其是否可以进入 wsc UART1 Hello 构建与后续 qzs 集成。先审查，不直接合并，不替 libaoxun 修改硬件文件。

【输入】
- libaoxun 远端 ref：<LIBAOXUN_REF>
- libaoxun 完整 SHA：<LIBAOXUN_SHA>
- Review Packet/构建摘要：<PACKET_PATH>
- 原始证据位置：<EVIDENCE_LOCATION>
若任一项未提供，先从本地仓库和 git ls-remote --heads origin 中只读发现；仍不能唯一确定时报告 BLOCKED，禁止使用“最新分支”猜测。

【开始前】
实读 AGENTS.md、CURRENT_STATE.md、SESSION_HANDOFF.md、所有权文件、BRANCH_MERGE_GOVERNANCE.md。运行 handoff health check 和 git status。固定审查开始时的分支、HEAD、dirty、远端 ref 与完整 SHA。

【逐项审查】
1. 用 git diff、git diff-tree 或等价只读命令列出候选全部文件；按 libaoxun 白名单标注 IN_SCOPE / OUT_OF_SCOPE。
2. 把 mem_test.xml、mem_test.peri.xml、constrain.sdc、Hard SoC IP/wrapper、BSP/soc.h、顶层和 APB 视为一个原子批次；检查是否混入旧 R0/UART0 制品或手改生成文件。
3. 核对 UART1 已由 Efinity 正式生成：PERI_UART_1、Type-C UART1 管脚 RX=GPIOR_96/B12、TX=GPIOR_100/D12、115200 8N1；UART0 历史结论不得继承。
4. 核对 Efinity 版本、工程 SHA、原子输入 hash、Map/Interface/PNR/STA/CDC、warning、bitstream SHA-256、BSP/soc.h 证据是否同批、可追溯。
5. 检查状态文档是否把 BUILD 外推成 USER2/UART1/APB/OSD/板级 PASS。
6. 若硬件输入、hash 或工具版本变化，明确列出失效的旧证据和必须重跑的 Gate。

【强制边界】
禁止修改 RTL/XML/SDC/IP/BSP，禁止运行 Efinity 构建或硬件动作，禁止猜 APB 基址，禁止回退 UART0。不要合并候选。

【输出】
形成 qzs Review Packet 或审查记录，按 P0/P1/P2 Findings 给出且只给出一个结论：APPROVE、CHANGES_REQUESTED 或 BLOCKED。APPROVE 必须同时给出固定 SHA、允许纳入文件、明确排除文件、同批 soc.h 路径/身份、制品 hash、未验证项和 Goal 2/3 可消费的输入。运行 git diff --check 和 qzs scope check 验证本 Goal 自己的文档差分。不要提交或推送。
```

### Goal 2：审查 wsc 的 classifier、Host 回归与 UART1 Hello 批次

```text
你现在在仓库 D:\第十届集创赛-雄芯院材料 中执行 qzs 的 Goal 2。

【唯一目标】
只读审查 wsc 的固定 SHA：确认 classifier MSVC C4127 已关闭、F1/adapter/runtime/G2 回归证据真实，并确认 UART1 Hello 只依据 Goal 1 批次的 SYSTEM_UART_1_* / soc.h 构建。先审查，不直接合并，不替 wsc 修改 CPU 文件。

【输入】
- wsc 远端 ref：<WSC_REF>
- wsc 完整 SHA：<WSC_SHA>
- Goal 1 APPROVE 的 libaoxun SHA：<LIBAOXUN_SHA>
- Goal 1 同批 soc.h/硬件 batch ID：<SOC_H_AND_BATCH_ID>
- wsc 测试/构建证据：<WSC_EVIDENCE>
Windows 若遇到 dev/wsc6090-CPU 与 dev/wsc6090-cpu 大小写 ref 冲突，必须以 git ls-remote --heads origin 的完整 SHA 为准，不迁移 refs 后端，不猜本地 origin/*。

【开始前】
实读 AGENTS.md、CURRENT_STATE.md、所有权文件和合并治理。运行 handoff health check、git status，固定当前分支/HEAD/dirty 和候选完整 SHA。

【逐项审查】
1. 列出候选全部差分，按 wsc 白名单标注 IN_SCOPE / OUT_OF_SCOPE；冻结头文件、状态治理、RTL/XML/SDC/IP 和 myCobot 变更一律阻断。
2. 审查 C4127 修复是否最小、是否保留测试语义；不能用降低 /W4 /WX、屏蔽 warning 或跳过可执行文件运行伪造 PASS。
3. 核对 classifier、F1、feature adapter、runtime/G2 bundle 的本轮命令、exit code和测试计数；旧 182/182、921/921 等历史数字不得继承。
4. 核对 UART1 Hello 使用 Goal 1 同批 soc.h 的 SYSTEM_UART_1_* 宏和片上 RAM布局；禁止硬编码旧 UART0 基址、猜 IRQ、复用旧 ELF。
5. 核对新 Hello ELF 的构建命令、输入 SHA、ELF SHA-256、大小/入口/段布局；它仍不证明板上 UART1 或 APB PASS。
6. 明确 ARM_ENABLED=0，真实 UART2/J52 transport 未初始化、未发送任何 myCobot 帧。

【强制边界】
禁止修改 CPU 代码或测试，禁止烧录/JTAG/串口操作，禁止修改硬件 ABI，禁止合并候选。

【输出】
形成 qzs 审查记录，按 P0/P1/P2 Findings 给出且只给出一个结论：APPROVE、CHANGES_REQUESTED 或 BLOCKED。APPROVE 必须给出固定 WSC SHA、纳入/排除范围、最新真实测试结果、匹配 batch ID 和 ELF SHA-256，以及 Goal 3 的固定输入。运行 git diff --check 和 qzs scope check验证本 Goal 文档差分。不要提交或推送。
```

### Goal 3：按 libaoxun → wsc 顺序集成固定 SHA 并执行离线总门

```text
你现在在仓库 D:\第十届集创赛-雄芯院材料 中执行 qzs 的 Goal 3。

【唯一目标】
在 qzs 个人集成分支上，按“libaoxun APPROVE 固定 SHA → wsc APPROVE 固定 SHA → qzs 状态刷新”的顺序完成语义集成和离线验证。不得推送或合入 main。

【输入硬门】
- Goal 1 结论必须为 APPROVE，固定 SHA=<LIBAOXUN_SHA>。
- Goal 2 结论必须为 APPROVE，固定 SHA=<WSC_SHA>。
- 两个 Review Packet 必须列出纳入/排除范围、失效证据和匹配 batch ID。
任一条件不满足，报告 BLOCKED，不得合并。

【开始前】
1. 实读 AGENTS.md、CURRENT_STATE.md、SESSION_HANDOFF.md、所有权文件、合并治理和 MERGE_REGISTER。
2. 运行 handoff health check、git status --short --branch、git rev-parse HEAD，保存 dirty 清单。
3. 刷新远端引用；若 fetch 因 WSC 大小写 ref 冲突失败，记录 WARN，改用 git ls-remote 获取 SHA。
4. 用 git rev-list、git diff、git merge-tree 对两个固定 SHA 做合并风险探测；先报告文本、语义、证据和硬件原子性风险。

【集成顺序】
1. 只纳入 Goal 1 APPROVE 的 libaoxun 范围；不得拆散 Hard SoC 原子批次。
2. 再纳入 Goal 2 APPROVE 的 wsc 范围；CPU 必须服从同批硬件 ABI。
3. 冲突按文件中的领域条目裁决，不整文件覆盖 CURRENT_STATE.md、Review Packet 或历史证据。
4. 最后由 qzs 更新状态/证据索引；未经真实执行的项目保持 NOT VERIFIED / NOT RUN。
5. 保留所有无关 dirty；若无法安全分离，停止并报告 BLOCKED。

【强制验证】
- git diff --check
- tools/team_scope_check.ps1 分别核对来源范围，并对 qzs 最终差分执行 -Role qzs
- tools/interface_freeze_check.ps1
- tools/agent_handoff_health_check.ps1
- tools/project_freshness_check.ps1
- tools/agent_context_budget.ps1
- tools/offline_presubmit.ps1
对每条命令记录完整命令、exit code、关键计数和 WARN。若脚本要求沙箱外真实运行，应按工具提示申请批准；不能把未运行写成 PASS。

【硬件边界】
本 Goal 纯离线：禁止 Efinity 重建、JTAG、USER2、串口、UART2/J52、myCobot 查询或动作。离线 PASS 只证明集成候选的离线门。

【完成标准】
输出候选集成 SHA/工作树状态、两个来源 SHA、实际纳入/排除文件、测试矩阵、失效证据、仍未验证项和 Goal 4 的匹配 bitstream/ELF/batch 输入。除非用户明确要求，否则不提交、不推送、不切 main。
```

### Goal 4：在一次批准窗口内完成 I0-SMOKE 证据链

```text
你现在在仓库 D:\第十届集创赛-雄芯院材料 中执行 qzs 的 Goal 4。这是硬件 Gate，不是普通离线任务。

【唯一目标】
只在匹配输入和用户明确批准后，在一次连续批准窗口内完成：匹配 bitstream → USER2 → 片上 RAM UART1 Hello ELF → PC 范围核对 → Type-C UART1 115200 Hello/单字符回显 → 按同批 soc.h 只读 APB MAGIC 0x375A0001，并保存原始证据。任何单点失败立即停止。

【前置硬门】
1. Goal 3 离线集成已通过，候选 SHA=<INTEGRATION_SHA>。
2. bitstream SHA-256=<BITSTREAM_SHA256>，ELF SHA-256=<ELF_SHA256>，batch ID=<BATCH_ID>，同批 soc.h=<SOC_H_PATH>。
3. Review Packet 已核对 Efinity 版本、原子输入、Map/PNR/STA/CDC/warning、USER2、RAM范围、UART1管脚和 Type-C 路由。
4. 用户明确确认本次 I0-SMOKE 的板卡、制品、接线、失败停止策略和批准窗口。
5. UART2/J52 信号线保持断开；不执行机械臂查询或动作。
若任何一项缺失，先输出完整缺口并等待用户批准，状态为 BLOCKED；不得用旧 UART0/R0 制品代替。

【执行规则】
- 只使用仓库已审查的操作卡和工具；不临时拼接命令，不猜 USER、地址、COM 口或基址。
- 固定 hash 后，同一窗口连续执行，不为相同事实反复索取确认。
- PC 必须位于同批批准的片上 RAM 范围；不符立即停止。
- UART1 无 Hello、乱码、reset 异常时立即停止，保留枚举、115200 日志、截图/console和 hash；不回退 UART0，不进入 APB。
- APB MAGIC 只按同批 soc.h 只读；异常立即停止，不写业务寄存器，不用 0/FFFFFFFF 猜根因。
- 禁止 USER1、Flash、DDR、业务 I1-I4 试探、UART2/J52、myCobot transport 和动作。

【证据要求】
记录时间、执行人、仓库 SHA、工具/板卡身份、原子输入 hash、bitstream/ELF SHA-256、PC、Type-C UART1 枚举、完整 Hello/回显原始输出、APB 地址来源和 MAGIC 原始输出。原始机器日志可留本地；Git 中只提交脱敏摘要、hash、Review Packet 和可复查索引。

【结论边界】
- 全链通过：I0-SMOKE=PASS，但 I1-I4、OSD、UART2/myCobot 仍为 NOT VERIFIED/NO-GO。
- 任一环节失败：准确标为 USER2_FAIL、PC_RANGE_FAIL、UART1_FAIL 或 APB_MAGIC_FAIL，只处理当前故障，不扩大范围。

结束时更新 qzs 证据索引与状态候选，运行 git diff --check、qzs scope check、freshness 和 handoff health。不要推送或合入 main，除非用户另行明确要求。
```

### Goal 5：最终状态、handoff、技术报告与 PPT 证据索引

```text
你现在在仓库 D:\第十届集创赛-雄芯院材料 中执行 qzs 的 Goal 5。

【唯一目标】
把 Goal 1–4 的真实结果编译为唯一一致的 CURRENT_STATE、SESSION_HANDOFF、证据索引、最终日五行报，以及技术报告/PPT 可引用材料索引。只汇编已存在证据，不补写推测结果。

【开始前】
实读 AGENTS.md、官方 0710 比赛细则、CURRENT_STATE.md、决赛主方案、所有权文件、Goal 1–4 Review Packet/日志和 git status。固定最终候选 SHA、libaoxun SHA、wsc SHA、batch ID、bitstream/ELF hash。

【必须完成】
1. CURRENT_STATE.md：刷新 CURRENT_SNAPSHOT、BLOCKERS、NEXT_GATE、PENDING_DECISIONS、POST_MERGE_REFRESH；保留历史批次和失败，不整页覆盖。
2. SESSION_HANDOFF.md：写清分支/HEAD/dirty、来源 SHA、已执行命令、PASS/FAIL/WARN、未验证项、下一执行者第一步。
3. 证据索引：每个结论链接到匹配 Review Packet、同批构建摘要或原始日志索引；本机路径和敏感信息不得进入 Git。
4. 报告/PPT索引：按“架构设计 / 已实现 / 已离线验证 / 已板级验证 / 未验证 / 回退方案”分类；任何 NOT VERIFIED 在报告/PPT 中保持同一状态。
5. 输出队友可直接使用的集成五行报：今日系统事实、今日关闭风险、本轮仍未验证、对 wsc 依赖、对 libaoxun 依赖。

【强制边界】
- 不修改 CPU、RTL、XML、SDC、IP/BSP。
- 无完整口令不修改冻结接口/manifest。
- 不执行硬件或机械臂动作。
- 不把设计目标、Host PASS、BUILD PASS、Hello PASS 相互替代。
- 若 Goal 4 失败或未运行，准确写 FAIL/NOT RUN，仍可完成本 Goal 的状态汇编，但不得声称 I0 PASS。

【验证】
运行 git diff --check、qzs scope check、interface freeze check、project freshness、context budget、handoff health；如集成代码自 Goal 3 后未变，不重复制造新的硬件结论。列出所有 WARN 的来源和是否阻塞。

【完成标准】
仓库内不存在互相矛盾的当前结论；报告/PPT每个重要陈述都能回到证据；下一位队友无需口头补充即可继续。不要提交、推送或合入 main，除非用户明确要求。
```

### Goal 6：I0 PASS 后组织 F1-ABI 与 I1–I4 Review Packet

```text
你现在在仓库 D:\第十届集创赛-雄芯院材料 中执行 qzs 的 Goal 6。

【唯一目标】
仅在 I0-SMOKE 已有当前批次 PASS 时，组织 F1-ABI / I1–I4 Review Packet，冻结下一原子批次需要回答的接口、CDC、ACK、OSD和证据问题。当前 Goal 只审查和形成决策包，不直接实现 CPU/RTL/SoC 接口。

【前置硬门】
CURRENT_STATE.md 必须记录当前候选 SHA、batch ID、bitstream/ELF hash，以及 USER2、UART1 Hello/echo、APB MAGIC 同批 PASS。否则输出 BLOCKED，不得从 Host/BUILD 推断 I0 PASS。

【审查范围】
1. I1：单槽特征快照、frame_id、stable/valid、overrun、release ACK、CDC 与 CPU 消费时序。
2. I2：任务配置、config_revision、APPLY/锁存、防抖和跨轮更新规则。
3. I3：逐轮事务、结果锁存、超时/ABANDON/RESET、重复触发防护。
4. I4：round_id + frame_id + config_revision + color/shape/size/decision/reason 的 staging-commit 语义、完整帧边界与 OSD 可解释显示。
5. I5：继续保持 OUT OF F1 / BLOCKED，ARM_ENABLED=0，UART2/J52 物理断开，不初始化真实 transport。

【必须在 Review Packet 中回答】
- 每个字段的唯一真源、方向、位宽、复位值、valid/ack 生命周期；
- CPU 与 FPGA 各自所有权；时钟、复位、CDC、跨帧/跨轮不变量；
- 同批 XML/peri.xml/SDC/IP/wrapper/top/BSP/soc.h/固件原子集合；
- 离线、仿真、Map/PNR/STA/CDC、板级原始证据与失败停止条件；
- 哪些旧证据因接口/输入变化失效；
- 哪些问题必须由用户、wsc、libaoxun、qzs共同裁决。

【冻结接口口令】
若结论要求修改冻结接口或 manifest，必须停止并向用户索取完整口令：确认接口文件修改，已经和wsc、libaoxun、qzs沟通。没有口令时只能形成 proposed diff/Review Packet，不能改真源。

【强制边界】
不改 CPU/RTL/XML/SDC/IP/BSP，不执行板级试探，不接 UART2/J52，不发送 myCobot 查询或动作。OSD 只能写 SEMANTICS PROPOSED，直到有同批实现与板级证据。

【完成标准】
输出一个可由三人逐项签字的 Review Packet，以及 APPROVE / CHANGES_REQUESTED / BLOCKED 结论、文件所有权、实施顺序、验证矩阵和回退点。运行 git diff --check、qzs scope check、freshness、handoff health。不要提交或推送。
```

## 五、风险矩阵

| 风险 | 概率 | 影响 | qzs 的处理 |
|---|---|---|---|
| 候选分支名称变化或 WSC 大小写 ref 冲突 | 高 | 误审错误提交 | 只认 `git ls-remote` 与完整 SHA |
| 状态文档被候选整文件覆盖 | 中 | 丢失失败/历史批次 | 按事实条目语义合并，禁止整页采用 |
| UART1 原子批次混入旧 UART0/R0 制品 | 中 | 板测结论无效 | 核对八类输入、batch ID、bitstream/ELF/soc.h 同批身份 |
| classifier 用降 warning 或跳测试伪装修复 | 中 | 离线门假 PASS | 保留 `/W4 /WX`，核对可执行文件真实运行和 exit code |
| 离线 PASS 被写成板级 PASS | 高 | 错误推进 Gate | 分列 BUILD、USER2、CPU、UART、APB、OSD、机械臂证据 |
| I0-SMOKE 中途换 hash/接线/现象 | 中 | 证据链断裂 | 立即停止，重开对应 Gate，不拼接不同批次证据 |
| 比赛临近导致提前接 UART2/J52 | 中 | 人身/设备风险 | I5 独立 Gate，物理断开，任何查询/动作均需新 Review Packet 和用户确认 |
| 报告/PPT用目标替代结果 | 高 | 现场陈述不可审计 | 每条结论绑定证据路径；缺证据即 NOT VERIFIED |

## 六、推荐执行节奏

1. Goal 0、Goal 1、Goal 2 已完成；Goal 1/2 当前均为固定 `APPROVE`。
2. Goal 3 已完成固定 SHA 集成与离线总门：`offline_presubmit=PASS_WITH_WARNINGS`，freshness `WARN=8 / FAIL=0`；用户授权的 3 个 `ppt_doc_outlines/**` 文件作为旧 qzs 白名单外的显式治理例外随最终刷新提交。
3. Goal 3 提交并核对远端集合 SHA 后，用户明确批准匹配制品的硬件窗口，再执行 Goal 4。
4. 无论 Goal 4 PASS 或 FAIL，都执行 Goal 5，保持真实状态。
5. 只有 Goal 4 当前批次 PASS 才执行 Goal 6。

如果任一 Goal 连续失败，不要通过扩大修改范围“救火”；保留单一故障、原始证据和停止点，回到对应负责人处理。
