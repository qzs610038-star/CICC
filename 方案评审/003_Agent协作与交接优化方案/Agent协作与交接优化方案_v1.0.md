# Agent 协作与交接优化方案 v1.0

为了解决第十届集创赛雄芯院项目（TJ375N529/Efinity FPGA + myCobot）在多 Agent 协作开发中遇到的**旧方案过度约束**、**跨模块瞻前顾后**以及**长程任务交接重复验证**的问题，特制定本工作流优化方案。

---

## 1. 核心问题诊断与解决策略

### 1.1 问题一：旧方案与静态规划的过度约束
*   **现象**：Agent 受到 `分赛区决赛实施开发路线.md` 等历史文档约束太强。即使当前对话中用户给出了最新的修改指令，Agent 也容易在多轮对话后遗忘，重新倾向于历史文档中的旧方案。
*   **策略：建立“最新状态重载区”与“活文档机制”**。
    *   在工作区根目录下设立唯一的最新状态文件 `CURRENT_STATE.md`。
    *   在全局 `AGENTS.md` 中增加优先级规则：`CURRENT_STATE.md` 的优先级高于任何历史规划文件。
    *   **开发流转规则**：一旦发生路线调整或用户最新指令与旧文档冲突，Agent 必须**首先且立即**更新 `CURRENT_STATE.md`。

### 1.2 问题二：大一统规则导致 Agent 瞻前顾后
*   **现象**：由于 FPGA 时序、AXI 总线、CPU 固件、串口协议和机械臂控制全都写在同一个 `AGENTS.md` 里，Agent 在修改 FPGA RTL 时会盲目担心影响机械臂，反之亦然。这导致 Agent 动作保守，产生大量不必要的“安全确认”和提问。
*   **策略：利用 Antigravity 工作区 Custom Skills 机制实现自动按需装载**。
    *   不使用庞杂的单文件规则，而是利用 `.agents/skills/<skill_name>/SKILL.md` 机制。
    *   定义两个工作区专属 Skill：`fpga_vision`（FPGA 视频与图像处理）和 `cpu_mycobot`（CPU 固件与机械臂控制）。
    *   通过 Skill Frontmatter 的 `description` 和 `name` 触发机制，让 Agent 只有在处理相关任务时才自动加载对应模块的专业约束，从而在单一任务中保持高效与聚焦。

### 1.3 问题三：跨会话交接时，新 Agent 进行重复验证
*   **现象**：长程任务中，由于上下文限制，用户使用“对话上下文恢复日志”开启新对话。但新 Agent 读完日志后，出于谨慎本能，会使用 `rg`、`list_dir` 或 Efinity 编译等工具把已经完成的工作全部核对一遍，甚至把已修复的问题重新带回错误状态。
*   **策略：确立“强断言交接协议 (Zero-Trust-Bypass Protocol)”**。
    *   在全局规则中明确：交接文件 `SESSION_HANDOFF.md` 中的 `[绝对信任区 (Verified Facts)]` 具有最高置信度。
    *   **Agent 行为红线**：禁止 Agent 对 `Verified Facts` 下的内容做任何重复读取、编译或搜索验证，必须无条件接受其结论，直接运行 `Next Immediate Action`。
    *   **状态销毁机制**：新会话成功推进第一步后，Agent 必须将 `SESSION_HANDOFF.md` 归档为历史记录，避免后续对话混淆。

---

## 2. 拟议的具体变更内容 (待审核)

### 2.1 全局规则增量（拟修改 [AGENTS.md](file:///d:/第十届集创赛-雄芯院材料/AGENTS.md)）
在 `AGENTS.md` 底部增加以下规则段落：

```markdown
## Agent 协同与工作流重载规则

### 1. 活文档与最新共识优先
- 项目根目录下的 [CURRENT_STATE.md](file:///d:/第十届集创赛-雄芯院材料/CURRENT_STATE.md) 记录了最新开发共识和路线微调。
- 当用户要求偏离 `分赛区决赛实施开发路线.md` 等历史规划时，Agent 应当立即服从，并在执行前将该变更写入 `CURRENT_STATE.md`。
- `CURRENT_STATE.md` 的指示在任何时候均覆盖并优先于旧版路线文件。

### 2. 强断言跨会话接力
- 当项目根目录下存在 [SESSION_HANDOFF.md](file:///d:/第十届集创赛-雄芯院材料/SESSION_HANDOFF.md) 时，新对话中的 Agent 必须首先读取该文件。
- Agent **绝对禁止** 针对 `SESSION_HANDOFF.md` 中 `[Verified Facts]` (已验证事实) 栏目列出的内容执行任何重复验证操作（如搜索已改代码、重新编译、重复执行测试脚本等）。
- 必须无条件接受其结论，并直接接力执行 `[Next Immediate Action]` 中指定的任务。
- 接力任务成功迈出第一步后，Agent 必须立刻运行命令将该交接文件移动至 `debug_records/handoffs/` 目录下并重命名为 `SESSION_HANDOFF_YYYYMMDD_HHMMSS.md`，防止状态失效。
```

### 2.2 模块化 Skills 结构（拟在 `.agents/skills` 下创建）

#### (1) FPGA 视频前端模块专属 Skill
**文件路径**：`.agents/skills/fpga_vision/SKILL.md`
```yaml
---
name: fpga_vision
description: Handles FPGA top-level, RTL coding, video pipelines (CSI/DSI/OSD/ROI), SDC constraints, and ModelSim simulation.
---

# FPGA 视频前端与图像处理开发规范

当任务涉及 FPGA RTL 修改、约束调整或仿真时，必须遵守以下核心约束：
1. **仅关注 FPGA 边界**：本模块只负责视频接入、Debayer、GAMMA、ROI提取、OSD显示叠加与 UART/AXI 寄存器物理通道。
2. **拒绝过度设计**：不要在此处尝试编写复杂的视觉识别算法或机械臂轨迹算法，这些应当交给板上 CPU。
3. **时序与约束**：任何时钟或复位信号的修改必须同步检查 `constrain.sdc` 和 `.peri.xml`。
4. **编译与警告**：运行 Efinity 编译后，必须记录关键的时序违例（Setup/Hold Slack），不可忽略 Clock Domain Crossing (CDC) 的 Warning。
```

#### (2) CPU 与机械臂控制模块专属 Skill
**文件路径**：`.agents/skills/cpu_mycobot/SKILL.md`
```yaml
---
name: cpu_mycobot
description: Handles SoC RISC-V CPU firmware, UART registers, myCobot 280 serial communication, pose interpolation, and parameters management.
---

# SoC CPU 与 myCobot 机械臂开发规范

当任务涉及 CPU 软件、串口控制协议或机械臂联动时，必须遵守以下核心约束：
1. **控制逻辑下放**：识别决策（颜色、尺寸分类）、目标匹配、阈值管理以及 myCobot 串口发包、重试与互锁逻辑，必须完全在板上 CPU (C代码/固件) 中实现。
2. **硬件解耦**：本模块通过 UART/寄存器接口与 FPGA 交互。不要尝试修改底层 RTL 视频流或 OSD 渲染逻辑，仅通过 AXI 寄存器回写结果。
3. **机械臂安全第一**：
   - 任何涉及机械臂物理运动的代码修改，必须先进行只读状态读取（读关节角度、读夹爪状态）验证。
   - 禁止在未确认电平、接线和初始安全姿态的情况下，下发大角度或快速移动指令。
   - myCobot 280 的控制波特率固定为 `1000000`。
```

### 2.3 交接模板定义（拟创建 [HANDOFF_TEMPLATE.md](file:///d:/第十届集创赛-雄芯院材料/HANDOFF_TEMPLATE.md)）

```markdown
# Session Handoff - [任务简短名称]

- 交接发起时间: YYYY-MM-DD HH:MM:SS
- 前序会话完成度: [xx]%
- 当前分支/commit: [main / dev_xxx]

## [Verified Facts] (绝对信任区 - 禁止新 Agent 重新验证)
1. [事实1]：例如 "fpga/rtl/debayer.v 文件第 45 行复位释放时序已由 active-low 改为 active-high"。
2. [事实2]：例如 "Efinity 综合已通过，生成 bitstream 位于 outflow/mem_test.hex，无 Setup Slack 违例"。
3. [事实3]：例如 "myCobot 280 串口在 COM3 已被成功识别，且 1,000,000 波特率握手成功"。

## [Context] (上下文与重要变量)
- `reg_base_addr` = 0x4000_0000
- 关键待改动文件：`final_project/sw/cpu/main.c`

## [Next Immediate Action] (新会话立即可执行任务)
- [ ] 第一步：...
- [ ] 第二步：...
```

---

## 3. 供其它 Agent 评审的引导提示词 (Review Prompts)

为了便于您请其他 Agent 评审本方案，推荐使用以下提示语：

> **提示词**：
> "请你以资深嵌入式/FPGA 项目系统架构师以及 Agent 专家团队的身份，评审 `方案评审/003_Agent协作与交接优化方案/Agent协作与交接优化方案_v1.0.md`。
> 重点检查：
> 1. 这套『强断言交接协议』和『动态状态活文档』是否能有效防止新 Agent 对已有成果的重复验证，有无逻辑漏洞（例如如果真的出现事实错误，如何安全跳出强断言机制）？
> 2. 基于 Custom Skills 的模块化指引是否能被 Antigravity 正确识别并按需触发？
> 3. 是否有必要在交接协议中引入一个『沙箱或快速健康度脚本校验』作为最低安全兜底，以防前序 Agent 给出了错误的 Verified Facts？"

---

## 4. Codex 评审意见与修正建议

> 来源：Codex
> 评审日期：2026-07-07
> 评审角色：资深嵌入式 / FPGA 项目系统架构师与 Agent 协作机制审查

### 4.1 总体结论

本方案的目标方向是合理的：通过 `CURRENT_STATE.md` 解决旧规划对新决策的反复干扰，通过模块化 Skills 降低单一 `AGENTS.md` 的上下文负担，通过交接模板减少新 Agent 对已完成工作的重复验证。

但 v1.0 不建议直接落地，核心原因是“强断言交接协议”表述过于绝对。当前文本要求新 Agent 对 `Verified Facts` “绝对禁止重复验证”并“无条件接受”，这会把前序 Agent 的潜在误判、过期状态或错误日志结论固化为不可纠偏事实。对于 FPGA / Efinity / myCobot 这类涉及硬件状态、时钟复位、约束、bitstream、串口和机械臂动作安全的项目，这一机制风险偏高。

建议将 v1.0 修改为“高置信接力协议”：默认接受前序 Verified Facts，禁止全量重跑和重型重复验证，但保留只读快速健康检查、显式异常触发条件和最小范围纠偏出口。

### 4.2 关于强断言交接协议

当前 `Verified Facts` 的设计可以减少重复验证，但不能写成“绝对信任区”。更合适的语义是：

- `Verified Facts` 是“高置信接力事实”，不是不可挑战事实。
- 新 Agent 默认不得重新执行完整 Efinity 编译、完整仿真、全仓库搜索、机械臂动作测试等重型验证。
- 新 Agent 必须允许执行只读、低成本、无副作用的健康检查，确认当前仓库状态与交接文件一致。
- 一旦发现直接矛盾，应允许跳出强断言机制，但纠偏范围必须限制在冲突事实本身，不能借机推翻整个交接包。

建议新增以下跳出条件：

1. 当前 `HEAD`、分支或 dirty 状态与 handoff 声明不一致。
2. `Verified Facts` 中缺少证据路径、命令记录、日志位置、时间戳或 commit 信息。
3. handoff 声明的关键文件、日志或输出产物不存在。
4. 快速健康度脚本失败。
5. 用户当前明确指令与 handoff 事实冲突。
6. 涉及机械臂运动、FPGA-to-机械臂接线、电平、Efinity bitstream、`constrain.sdc`、`mem_test.xml`、时钟复位、AXI / CDC、CPU 到 OSD 回写链路等安全关键边界。

建议将原规则：

```markdown
Agent 绝对禁止针对 Verified Facts 执行任何重复验证，必须无条件接受其结论。
```

改为：

```markdown
Agent 应默认接受 Verified Facts，不得主动执行重型重复验证；但必须先完成只读快速健康检查。若健康检查失败、当前用户指令冲突，或发现关键事实与当前仓库状态直接矛盾，Agent 应进入 Contradiction Report 流程，仅针对冲突事实做最小范围核查和纠偏。
```

### 4.3 关于 CURRENT_STATE.md 活文档

`CURRENT_STATE.md` 是必要的，但不应被定义为“任何时候覆盖一切旧文件”。它更适合作为“最新路线增量与当前状态索引”，而不是替代所有架构文件、约束文件和协作规则。

建议明确优先级：

1. 用户当前对话中的明确指令优先。
2. 安全红线、硬件动作限制、Git 提交规范和 Codex 审查门不可被 `CURRENT_STATE.md` 覆盖。
3. `CURRENT_STATE.md` 可覆盖旧规划中的路线选择、任务优先级和已废弃方案。
4. 真实源码、工程文件、构建日志和上板现象仍然是事实来源；`CURRENT_STATE.md` 只能记录它们的最新结论和证据位置。

建议 `CURRENT_STATE.md` 每条状态至少包含：

- 日期与来源 Agent。
- 适用范围。
- 最新结论。
- 替代了哪个旧结论。
- 证据路径或日志路径。
- 失效条件。

### 4.4 关于 Custom Skills 模块化

基于 `.agents/skills/<skill_name>/SKILL.md` 的模块化设计是合理方向，尤其适合把 FPGA 视频链路、CPU 固件、myCobot 安全、QMD / CBM 检索规则拆开，减少单一 `AGENTS.md` 的上下文压力。

但需要注意：

- 当前仓库尚未实际存在 `.agents/skills/` 目录，v1.0 只是方案设计，不是已验证的工作流。
- Antigravity 能识别 workspace custom skills，并不等于 Claude、Codex、Gemini 都会自动加载同一套 skills。
- `AGENTS.md` 仍应保留最小全局红线，例如决赛主线边界、机械臂安全、Codex 审查门、禁止提交本机私密配置等。
- Skills 应承载模块细则，而不是完全取代仓库根规则。

建议新增一个轻量“Agent 能力识别”段落：

```markdown
若当前 Agent 支持 `.agents/skills/` 自动加载，则按任务触发对应 Skill；若当前 Agent 不支持自动加载，应先阅读 `AGENTS.md` 中的 Skill 索引，并按任务手动读取对应 `SKILL.md`。
```

### 4.5 关于沙箱或快速健康度脚本

建议必须引入只读快速健康度脚本，作为交接协议的最低安全兜底。这不是重复验证，而是“接力安全握手”。

建议创建：

```text
tools/agent_handoff_health_check.ps1
```

默认只做以下无副作用检查：

1. 确认当前路径是本仓库。
2. 输出当前分支、`HEAD`、dirty 状态。
3. 检查 `SESSION_HANDOFF.md` 声明的关键文件是否存在。
4. 可选检查关键文件 hash 或 mtime。
5. 检查工程 XML / JSON / Markdown 是否基本可读。
6. myCobot 仅允许枚举串口和检查 `pymycobot` 是否可 import，不允许任何运动命令。
7. FPGA 默认不运行完整 Efinity，只检查工程文件、约束文件和最近日志路径是否存在；完整综合必须由任务显式要求。

脚本通过后，新 Agent 应直接执行 `Next Immediate Action`；脚本失败时，进入 `Contradiction Report`，由 Agent 说明失败项、影响范围和建议的最小纠偏动作。

### 4.6 关于 SESSION_HANDOFF.md 归档时机

v1.0 中“接力任务成功迈出第一步后立刻归档”的规则过早，可能导致交接上下文还没有完全吸收就被移走。

建议改为：

- 完成 `Next Immediate Action` 的第一个可验证 checkpoint。
- 将新的事实、风险或下一步写入 `CURRENT_STATE.md` 或任务日志。
- 确认没有仍依赖 handoff 原文的未完成动作。
- 再将 `SESSION_HANDOFF.md` 归档到 `debug_records/handoffs/`。

### 4.7 建议的 v1.1 版本定位

建议 v1.1 将核心机制改为：

```text
动态状态活文档 + 模块化 Skills + 高置信交接 + 只读健康握手 + 明确纠偏出口
```

不建议继续使用“绝对信任区”或“无条件接受”作为正式规则。更稳妥的命名是：

```markdown
## [Verified Facts] 高置信接力事实区
```

并配套：

```markdown
## [Evidence Index] 证据索引
## [Health Check] 接力健康检查
## [Contradiction Report] 矛盾与纠偏记录
## [Next Immediate Action] 下一步立即动作
```

这样既能减少新 Agent 的重复验证，又不会牺牲交接纠错能力和硬件安全边界。

---

## 5. 项目实机探测结果

> 探测执行：Claude Fable 5
> 探测日期：2026-07-07
> 探测目的：在落地 v1.1 前，核查本仓库当前真实结构、已有协作资产与交接实践，确认 v1.0 痛点是否真实存在、Codex 4.x 评审意见是否成立。

### 5.1 探测范围与已有资产盘点

对仓库根 `D:\第十届集创赛-雄芯院材料` 的关键协作资产做了只读核查，结论如下：

| 资产 | 路径 | 现状 |
|------|------|------|
| 全局 Agent 规则 | `AGENTS.md` | 存在，116 行，含主线边界、机械臂安全、Codex Gate、Review Packet 要求 |
| Claude 专属上下文 | `CLAUDE.md` | 存在，约 200 行，与 `AGENTS.md` 在主线边界/机械臂安全/Codex Gate 上**大量重复** |
| 高层路线文件 | `分赛区决赛实施开发路线.md` | 存在，v0.3，2026-07-01，当前最高层路线源 |
| 自动记忆索引 | `MEMORY.md` | 存在，仅 1 行（指向 CLAUDE.md），未承载路线状态 |
| 交接命令 | `.claude/commands/fpga-handoff.md` | 存在，JSON 模板 + 摘要格式，**无 Verified Facts / Evidence / Health Check 段** |
| 架构/执行命令 | `.claude/commands/fpga-plan.md`、`fpga-exec.md` | 存在，已固化"先读 CLAUDE.md/AGENTS.md、先查 CBM、不改 ipm/outflow"等规则 |
| 本机 QMD 规则 | `.agents/local/qmd-cicc-competition-rules.md` | 存在，本机私有，不提交 Git |
| 调试记录目录 | `debug_records/` | 存在，含 `README.md` 规范 + 多份上板/相机/HDMI 日志 + 1 份 handoff |
| 实机交接样本 | `debug_records/camera_hdmi_handoff_2026-07-06.md` | 存在，正是 v1.0 想规范化的交接实践 |
| `.agents/skills/` 目录 | — | **不存在**，v1.0 §2.2 仅为设计，未落地 |
| `CURRENT_STATE.md` | — | **不存在** |
| `SESSION_HANDOFF.md` | — | **不存在**（交接走 `debug_records/` 和全局 `~/.agents/handoff/`） |
| `tools/` 脚本目录 | — | **不存在** |
| 健康检查脚本 | — | **不存在** |
| `.claude/settings.json` | 存在 | allow 列表仅含 `git status/diff/log`、`codebase-memory-mcp`、`python`，**未含健康检查脚本放行** |

### 5.2 关键发现与佐证

**发现一：交接事实确实会过期，"绝对信任区"不可落地（直接佐证 Codex 4.2）。**
实机交接样本 `debug_records/camera_hdmi_handoff_2026-07-06.md` 中记录的仓库根是 `C:\Users\20306\Desktop\赛题资料\CICC`，烧录树是 `D:\final_project`，Efinity 安装是 `D:\Efinity\2025.2`——这些路径与当前工作根 `D:\第十届集创赛-雄芯院材料` **完全不同**（跨机器/跨路径）。该 handoff 自己在第 13-21 行就要求新窗口"先 verify current files before editing"，并警告"Do not assume the old chat state is current"。这证明：前序 Agent 写入的 Verified Facts 在跨会话、跨机器后必然存在过期风险，v1.0 原文"绝对禁止重复验证/无条件接受"会直接固化这种过期路径为不可纠偏事实。Codex 4.2 的"高置信接力 + 只读健康检查 + 纠偏出口"是必要的，不是可选的。

**发现二：AGENTS.md 与 CLAUDE.md 重复严重，是"瞻前顾后"的结构性根因（佐证 v1.0 §1.2 + Codex 4.4）。**
主线边界（FPGA 前端 + 板上 CPU 决策/控制）、机械臂安全三阶段、Codex Gate 触发清单、Review Packet 模板，在 `AGENTS.md` 和 `CLAUDE.md` 中**各写了一遍**，措辞略有差异但语义重叠。Agent 每次加载上下文时被两份重叠规则同时约束，且其中还混入了"当前阶段/当前宏开关"这类**易变状态**（如 DSI 已停用、`FRAME_BUFFER`/`HDMI_OUT_EN` 当前生效），导致 Agent 无法区分"结构性红线"与"当前状态",在改 FPGA RTL 时也会联想到机械臂条款。这印证了 v1.0 §1.2 的诊断，也说明 Codex 4.4"AGENTS.md 保留最小全局红线、Skills 承载模块细则"的方向正确。

**发现三：交接协议已部分存在但缺证据索引和健康握手（佐证 Codex 4.5、4.7）。**
`.claude/commands/fpga-handoff.md` 已有 JSON 模板（含 `modified_files`、`verification`、`open_items`、`codex_review_packet` 等键）和摘要格式，但**没有** `verified_facts`、`evidence_index`、`health_check`、`contradiction_report` 段。实机 handoff 样本虽然靠作者自觉写了"Hard Constraints Known So Far"和"verify before editing"，但属于个案习惯，不是协议强制项。若按 v1.0 另起 `SESSION_HANDOFF.md` 机制，会与现有 `fpga-handoff.md` + `~/.agents/handoff/` 形成两套并存协议。正确做法是**把五段式合并进现有模板**，而非另立。

**发现四：`.agents/skills/` 目录尚未存在，Skills 自动加载不能假设（佐证 Codex 4.4）。**
仓库当前没有 `.agents/skills/` 目录，也没有任何已验证的 Skill 自动加载实践。Antigravity 工作区能识别 custom skills，不等于 Claude Code、Codex、Gemini 在本仓库都会自动加载同一套 skills。因此 v1.1 必须保留"不支持自动加载时手动读取 SKILL.md"的降级路径，并把全局红线仍留在 `AGENTS.md`，Skills 只承载模块细则。

**发现五：MEMORY.md 当前几乎空置，可作为活文档落点。**
`MEMORY.md` 仅 1 行，未承载任何路线状态。这反而说明项目缺少"当前状态"单一入口，`CURRENT_STATE.md` 有真实需求；同时要注意 `MEMORY.md` 是本机自动记忆索引（属 `~/.claude/projects/...` 体系），与项目内 `CURRENT_STATE.md` 不是一个东西，不能合并。

### 5.3 痛点复核结论

v1.0 三个痛点在当前仓库**全部真实存在**，Codex 4.x 评审意见**方向全部成立**。实机 handoff 样本的过期路径是最有力的反例：它同时证明了"交接重复验证"痛点真实（新窗口确实需要先 verify）、以及"绝对信任"不可行（若无条件接受过期路径，会直接改错目录）。

落地原则修正为：

```text
动态状态活文档 + 模块化 Skills + 高置信交接（非绝对信任）
+ 只读健康握手 + 明确纠偏出口 + 单一事实源（AGENTS.md/CLAUDE.md 去重）
```

---

## 6. v1.1 详细落实方案

> 落实撰写：Claude GLM 5.2
> 落实日期：2026-07-07
> 依据：Codex 4.x 评审意见 + 第 5 节 Fable 5 探测结果
> 定位：将 v1.0 的"绝对信任区/无条件接受"修正为"高置信接力 + 只读健康握手 + 纠偏出口"，并给出可直接部署的文件原文。

### 6.1 部署清单总览

| # | 动作 | 文件 | 类型 | 解决痛点 |
|---|------|------|------|----------|
| 1 | 新增增量段 | `AGENTS.md` | 改（追加 ~30 行） | 旧方案过度约束、优先级不清 |
| 2 | 新建 | `CURRENT_STATE.md` | 新建 | 旧方案过度约束 |
| 3 | 合并五段式 | `.claude/commands/fpga-handoff.md` | 改（扩展 JSON 模板） | 重复验证 |
| 4 | 新建模板 | `HANDOFF_TEMPLATE.md` | 新建（人读版） | 重复验证 |
| 5 | 新建脚本 | `tools/agent_handoff_health_check.ps1` | 新建 | 交接安全兜底 |
| 6 | 新建 Skill | `.agents/skills/fpga_vision/SKILL.md` | 新建 | 瞻前顾后 |
| 7 | 新建 Skill | `.agents/skills/cpu_mycobot/SKILL.md` | 新建 | 瞻前顾后 |
| 8 | 去重维护 | `CLAUDE.md` | 改（结构性瘦身） | 瞻前顾后（根因） |
| 9 | 权限放行 | `.claude/settings.json` | 改（追加 1 条 allow） | 健康检查可执行 |

### 6.2 文件一：AGENTS.md 增量段

在 `AGENTS.md` 现有"## Claude/Codex 协同分工"章节**之前**插入以下整段。此段是 v1.1 的协议中枢，定义优先级阶梯、交接规则、Skill 索引，**不重复已有红线条款**，只引用。

```markdown
## 文档优先级与交接规则

### 优先级阶梯（高 → 低）
1. 用户当前对话中的明确指令。
2. 本文件中的安全红线：机械臂动作限制、Codex 审查门、决赛主线边界、Git 提交规范。任何其他文件不可覆盖。其中“决赛主线边界”具体指 `AGENTS.md`「分赛区决赛主线」章节（当前版本约第 27-35 行）与 `分赛区决赛实施开发路线.md` 中未被 `CURRENT_STATE.md` 明确降级/覆盖的路线定义。
3. `CURRENT_STATE.md`：最新路线增量与当前状态索引，可覆盖旧规划中的路线选择、任务优先级和已废弃方案。
4. `SESSION_HANDOFF.md` 或 `debug_records/*handoff*.md`：高置信接力事实，需先跑 `tools/agent_handoff_health_check.ps1`。
5. `分赛区决赛实施开发路线.md` 等历史规划文件。
6. 初赛 demo 文档与旧日志：仅作经验库，不作事实来源。

真实源码、工程 XML、构建日志和上板现象始终是最终事实来源；上述文件只记录对它们的最新结论和证据位置。

### 决赛主线边界引用规则
- 引用“决赛主线边界”时必须写明具体来源文件和章节，优先写作：`AGENTS.md`「分赛区决赛主线」+ `分赛区决赛实施开发路线.md`。
- `AGENTS.md`「分赛区决赛主线」是硬边界：FPGA 负责视频前端/ROI/统计特征/OSD/必要硬件加速；板上 CPU 负责识别决策、参数管理和 myCobot 控制；不得恢复纯 FPGA 视觉识别或纯 FPGA 机械臂控制。
- `分赛区决赛实施开发路线.md` 是路线图和经验库；若其中某个阶段性“保底方案”已被 `CURRENT_STATE.md` 标为历史参考或旧结论，则不得再把它当作当前核心目标。

### 高置信交接规则（非绝对信任）
- 新会话发现交接文件时，必须先运行 `tools/agent_handoff_health_check.ps1`，再做任何修改。
- 默认接受 `[Verified Facts]`，禁止重型重复验证：不跑完整 Efinity 综合、不跑完整仿真、不做全仓库 `rg`、不做机械臂动作测试。
- 允许且要求只读、低成本、无副作用的健康检查（git 状态、关键文件存在性、串口枚举）。
- 出现以下任一跳出条件时，进入 `[Contradiction Report]` 流程，仅针对冲突事实做最小范围核查与纠偏，不得借机推翻整个交接包：
  1. 当前 `HEAD`、分支或 dirty 状态与 handoff 声明不一致。
  2. `Verified Facts` 缺少证据路径、命令记录、日志位置、时间戳或 commit 信息。
  3. handoff 声明的关键文件、日志或产物不存在。
  4. 健康检查脚本失败。
  5. 用户当前明确指令与 handoff 事实冲突。
  6. 涉及机械臂运动、FPGA-to-机械臂接线/电平、bitstream、`constrain.sdc`、`mem_test.xml`、时钟复位、AXI/CDC、CPU→OSD 回写等安全关键边界。

### 归档时机
完成 `[Next Immediate Action]` 的首个可验证 checkpoint，且新事实/风险/下一步已写入 `CURRENT_STATE.md` 或任务日志，确认无仍依赖 handoff 原文的未完成动作后，将交接文件移入 `debug_records/handoffs/` 并按 `SESSION_HANDOFF_YYYYMMDD_HHMMSS.md` 重命名。

### Skill 索引
- `.agents/skills/fpga_vision/SKILL.md`：FPGA 顶层、RTL、视频链路（CSI/DSI/OSD/ROI）、SDC 约束、ModelSim 仿真。
- `.agents/skills/cpu_mycobot/SKILL.md`：板上 CPU 固件、UART 寄存器、myCobot 280 串口通信、姿态插值、参数管理。
- 若当前 Agent 支持 `.agents/skills/` 自动加载，按任务触发对应 Skill；若不支持，按任务手动读取对应 `SKILL.md`。
- Skill 承载模块细则，不取代本文件的全局红线。安全红线、Codex 审查门、决赛主线边界仍以 `AGENTS.md`「分赛区决赛主线」和 `分赛区决赛实施开发路线.md`（未被 `CURRENT_STATE.md` 降级/覆盖的部分）为准。
```

### 6.3 文件二：CURRENT_STATE.md（新建）

**职责**：最新路线增量与当前状态索引，**不是**替代所有架构/约束/协作文件。每条状态至少六字段（Codex 4.3）。初始内容如下，后续随路线调整追加。

```markdown
# CURRENT_STATE — 最新路线增量与当前状态索引

> 本文件记录对历史规划文件的最新覆盖项与当前阶段状态。优先级见 AGENTS.md「文档优先级与交接规则」。
> 真实源码、工程 XML、构建日志和上板现象始终是最终事实来源；本文件只记录对它们的最新结论和证据位置。
> 安全红线（机械臂动作、Codex 审查门、决赛主线边界、Git 规范）不可被本文件覆盖。

## 状态条目格式
每条至少包含：
- 日期与来源 Agent
- 适用范围
- 最新结论
- 替代了哪个旧结论
- 证据路径或日志路径
- 失效条件

## 当前阶段
- 阶段：分赛区决赛主线实施（当前核心目标由用户当轮指令、最新交接包或本文件最新状态条目声明）
- 截止：2026-07-20
- 来源 Agent：用户批注 / Gemini 回答 / Codex 二次校验
- 最新结论：`HDMI 双摄透传 bring-up / 分赛区决赛保底方案` 已降级为历史基础链路或旧结论，不再作为默认当前核心攻关任务。后续当前目标必须由用户最新指令或最新 handoff 明确声明，例如 ROI/统计特征与 OSD、CPU 分类参数、CPU 与 myCobot UART 控制协议联调等；Agent 不得从旧保底方案自动推断当前阶段。
- 替代旧结论：`分赛区决赛实施开发路线.md` 中以 HDMI 双摄透传保底为当前核心目标的阶段性表述，以及 `debug_records/camera_hdmi_handoff_2026-07-06.md` 中的阶段性 bring-up 状态。
- 证据路径：`AGENTS.md`「分赛区决赛主线」、`分赛区决赛实施开发路线.md`、`debug_records/camera_hdmi_handoff_2026-07-06.md`、用户 2026-07-07 批注。
- 失效条件：用户明确恢复 HDMI 双摄透传为当前攻关目标，或新的 `CURRENT_STATE.md` 状态条目给出更高优先级结论。

## 路线覆盖项
（随路线调整追加，每条按六字段填写。例：）
- 日期：YYYY-MM-DD，来源 Agent：Claude
- 适用范围：颜色分类阈值管理
- 最新结论：阈值表放到板上 CPU 可调参数区，FPGA 只出 ROI 统计特征
- 替代旧结论：初赛 demo 的纯 FPGA HSV 阈值路线
- 证据路径：`final_project/integration/register_map.md`
- 失效条件：评委现场要求改回 FPGA 硬件阈值（需经 Codex Gate）

## 已废弃 / 历史参考方案
- 纯 FPGA 视觉识别主线（废弃，不可恢复，除非经 Codex Gate + 用户明确指令）
- 纯 FPGA myCobot 控制主线（废弃，同上）
- PC 端 pymycobot 进入正式识别/控制闭环（仅保留开发期调试，见 `mycobot_pc_tests/` 归档说明）
- HDMI 双摄透传保底方案（历史基础链路/旧结论；除非用户重新指定，否则不作为当前核心攻关目标）
```

### 6.4 文件三：fpga-handoff.md 模板扩展（合并五段式）

不另立 `SESSION_HANDOFF.md` 协议，而是把五段式合并进现有 `.claude/commands/fpga-handoff.md` 的 JSON 模板。在现有 JSON 模板中**新增**以下键（保留原有键不变）：

```jsonc
{
  // ... 原有键保留：version, agent, model, project, session_id, session_end,
  //     mode, cbm_status, completed_tasks, key_decisions, architecture_boundary,
  //     modified_files, verification, open_items, codex_review_needed,
  //     codex_review_packet, shared_update ...

  "repo_root_declared": "<current_workspace_root_resolved_at_handoff_generation>",
  "burn_tree_declared": "<optional_current_burn_tree_or_empty>",
  "path_policy": {
    "repo_internal_paths": "relative_to_repo_root",
    "absolute_paths": "provenance_only_not_execution_target"
  },
  "head_declared": { "branch": "main", "commit": "<sha>", "dirty": false },

  "verified_facts": [
    {
      "fact": "例：debayer.v 第 45 行复位已由 active-low 改为 active-high",
      "evidence": "final_project/fpga/rtl/debayer/debayer.v:45; git <sha>",
      "timestamp": "2026-07-07T12:00:00"
    }
  ],
  "evidence_index": [
    { "kind": "build_log|sim_log|board_photo|git_commit|file_path", "path": "", "note": "" }
  ],
  "health_check": {
    "script": "tools/agent_handoff_health_check.ps1",
    "ran": false,
    "result": "pass|fail|skipped",
    "fail_items": []
  },
  "contradiction_report": [
    { "conflict": "", "scope": "minimal", "resolution": "" }
  ],
  "next_immediate_action": [
    { "step": 1, "action": "", "checkpoint": "" }
  ]
}
```

**强制要求**：`repo_root_declared`、`burn_tree_declared`、`head_declared` 三项必填，但不得在模板中硬编码某一位队友的本机目录。生成 handoff 时由脚本动态写入当前工作区根路径；仓库内部证据路径必须统一写成相对项目根的路径（如 `final_project/...`）。绝对路径只用于来源追溯，不能作为接力 Agent 的执行目标。`verified_facts` 每条必须带 `evidence` 和 `timestamp`，缺则触发跳出条件 2。

### 6.5 文件四：HANDOFF_TEMPLATE.md（人读版，新建）

供不使用 `.claude/commands/fpga-handoff.md` 的 Agent（如 Codex、Gemini）使用的人读 Markdown 模板。内容如下：

```markdown
# Session Handoff - [任务简短名称]

- 交接发起时间: YYYY-MM-DD HH:MM:SS
- 前序会话完成度: [xx]%
- 当前分支/commit/dirty: [main / <sha> / clean]
- 仓库根（声明，仅作来源追溯）: <current_workspace_root>
- 烧录树（声明，如有，仅作来源追溯）: <current_burn_tree_or_empty>
- 路径规范：仓库内路径必须相对项目根，如 `final_project/...`；绝对路径仅作本机来源记录，不得作为接力执行路径。

## [Verified Facts] 高置信接力事实区
> 默认接受，禁止重型重复验证。每条必须带证据路径与时间戳。
1. [事实] — 证据：`path:line` / git <sha> / 日志 `path` — 时间：YYYY-MM-DD HH:MM
2. ...

## [Evidence Index] 证据索引
- 构建日志：
- 仿真日志：
- 上板截图/录像：
- git commit：
- 关键文件路径：

## [Health Check] 接力健康检查
- 脚本：`tools/agent_handoff_health_check.ps1`
- 运行结果：pass / fail / skipped
- 失败项（如有）：

## [Contradiction Report] 矛盾与纠偏记录
> 仅在健康检查失败或事实冲突时填写。只针对冲突事实做最小范围纠偏。
- 冲突项：
- 纠偏范围：
- 建议动作：

## [Next Immediate Action] 下一步立即动作
- [ ] 第一步：... （checkpoint：...）
- [ ] 第二步：...
```

### 6.6 文件五：tools/agent_handoff_health_check.ps1（新建）

只读、无副作用。接受可选的 handoff 文件路径参数；未传参时做基础检查。**严禁**任何机械臂运动命令、**严禁**跑完整 Efinity 综合。

```powershell
# tools/agent_handoff_health_check.ps1
# 接力健康握手脚本（只读、无副作用）
# 用法: pwsh -File tools/agent_handoff_health_check.ps1 [-Handoff <path>]
# 严禁: 机械臂运动命令、完整 Efinity 综合/布局布线、完整仿真重跑。
# 失败时退出码非 0，新 Agent 应进入 Contradiction Report 流程。

param(
  [string]$Handoff = ""
)

$ErrorActionPreference = "Continue"
$failItems = @()
$warnItems = @()
function Fail($msg) { $script:failItems += $msg; Write-Host "FAIL: $msg" -ForegroundColor Red }
function Warn($msg) { $script:warnItems += $msg; Write-Host "WARN: $msg" -ForegroundColor Yellow }
function Ok($msg)   { Write-Host "OK  : $msg" -ForegroundColor Green }

# 1. 仓库根确认：动态获取当前工作区，不硬编码任何队友本机路径
function Get-RepoRoot {
  try {
    $root = git rev-parse --show-toplevel 2>$null
    if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($root)) {
      return (Resolve-Path $root).Path
    }
  } catch {}
  return (Resolve-Path (Get-Location)).Path
}
$repo = Get-RepoRoot
if (-not (Test-Path $repo)) { Fail "当前仓库根不存在: $repo" } else { Ok "当前仓库根: $repo" }

# 2. git 状态（分支/HEAD/dirty）
try {
  $branch = git -C $repo rev-parse --abbrev-ref HEAD 2>$null
  $head   = git -C $repo rev-parse --short HEAD 2>$null
  $dirty  = git -C $repo status --porcelain 2>$null
  $isDirty = [bool]$dirty
  Ok "git: branch=$branch head=$head dirty=$isDirty"
} catch { Fail "git 状态读取失败: $_" }

# 3. 关键工程文件存在性
$keyFiles = @(
  "AGENTS.md",
  "CLAUDE.md",
  "分赛区决赛实施开发路线.md",
  "final_project/fpga/efinity/mem_test.xml",
  "final_project/fpga/efinity/constrain.sdc",
  "final_project/fpga/rtl/top/top.v",
  "final_project/cpu/app/include/bsp.h"
)
foreach ($f in $keyFiles) {
  $p = Join-Path $repo $f
  if (-not (Test-Path $p)) { Fail "关键文件缺失: $f" } else { Ok "存在: $f" }
}

# 4. 基本可读性（XML/JSON/MD 不是二进制乱码）
foreach ($f in @("final_project/fpga/efinity/mem_test.xml", "AGENTS.md")) {
  $p = Join-Path $repo $f
  if (Test-Path $p) {
    $first = Get-Content $p -TotalCount 1 -ErrorAction SilentlyContinue
    if ([string]::IsNullOrWhiteSpace($first)) { Fail "文件疑似空或乱码: $f" } else { Ok "可读: $f" }
  }
}

# 5. 若指定 handoff，核对其中声明的仓库根/烧录树/关键文件
if ($Handoff -ne "" -and (Test-Path $Handoff)) {
  $content = Get-Content $Handoff -Raw -ErrorAction SilentlyContinue
  if ($content -match "仓库根.*?(:|：)\s*([^\r\n]+)") {
    $declaredRoot = $Matches[2].Trim()
    if ($declaredRoot -ne "" -and $declaredRoot -notmatch "^<.*>$") {
      if (Test-Path $declaredRoot) {
        $declaredResolved = (Resolve-Path $declaredRoot).Path
        if ($declaredResolved -ne $repo) {
          Warn "handoff 声明仓库根与当前不同: 声明=$declaredResolved 当前=$repo。跨机器交接允许不同，后续以相对路径解析。"
        } else { Ok "handoff 仓库根与当前一致" }
      } else {
        Warn "handoff 声明仓库根在本机不存在: $declaredRoot。跨机器交接允许不同，后续以相对路径解析。"
      }
    }
  }
  # 提取 handoff 中出现的 repo-relative 路径并抽查；绝对路径只作来源追溯，不作为执行目标
  $absolutePaths = [regex]::Matches($content, '[A-Za-z]:\\[^\s\)`"\]]+') | ForEach-Object { $_.Value }
  foreach ($p in $absolutePaths | Select-Object -Unique) {
    Warn "handoff 含本机绝对路径，仅作来源追溯，不作为执行路径: $p"
  }
  $paths = [regex]::Matches($content, '(final_project[\\/][^\s\)`"\]]+|AGENTS\.md|CLAUDE\.md|分赛区决赛实施开发路线\.md)') | ForEach-Object { $_.Value }
  foreach ($p in $paths | Select-Object -Unique) {
    $rel = $p -replace '/', '\'
    $check = Join-Path $repo $rel
    if (-not (Test-Path $check)) { Fail "handoff 引用路径不存在: $p" }
  }
  Ok "handoff 路径核对完成"
}

# 6. myCobot 只读检查：仅枚举串口 + 检查 pymycobot 可 import，禁止任何运动命令
try {
  $pyCode = @'
import importlib.util
serial_spec = importlib.util.find_spec("serial")
print("serial", bool(serial_spec))
print("pymycobot", bool(importlib.util.find_spec("pymycobot")))
if serial_spec:
    import serial.tools.list_ports as p
    for x in p.comports():
        print(x.device, x.description)
'@
  $py = $pyCode | & python - 2>$null
  Ok "myCobot 只读检查:`n$py"
} catch { Fail "myCobot 只读检查失败: $_" }

# 7. FPGA：不跑综合，只确认工程/约束/最近日志路径存在
$logDirs = @("debug_records", "final_project/docs/evidence", "final_project/docs/review_packets")
foreach ($d in $logDirs) {
  $p = Join-Path $repo $d
  if (-not (Test-Path $p)) { Fail "日志/证据目录缺失: $d" } else { Ok "目录存在: $d" }
}

if ($failItems.Count -gt 0) {
  Write-Host "`n健康检查失败 $($failItems.Count) 项，进入 Contradiction Report 流程。" -ForegroundColor Yellow
  exit 1
} else {
  Write-Host "`n健康检查通过，可直接执行 Next Immediate Action。" -ForegroundColor Green
  exit 0
}
```

### 6.7 文件六：.agents/skills/fpga_vision/SKILL.md（新建）

在 v1.0 §2.2 原文基础上，**顶部加一行红线引用**，并补一条双通道检查要求。

```markdown
---
name: fpga_vision
description: Handles FPGA top-level, RTL coding, video pipelines (CSI/DSI/OSD/ROI), SDC constraints, and ModelSim simulation. Trigger when editing Verilog/RTL, constrain.sdc, mem_test.xml, .peri.xml, IP settings.json, or running Efinity/ModelSim.
---

# FPGA 视频前端与图像处理开发规范

> 全局安全红线、Codex 审查门、决赛主线边界以 `AGENTS.md`「分赛区决赛主线」和 `分赛区决赛实施开发路线.md`（未被 `CURRENT_STATE.md` 降级/覆盖的部分）为准；本 Skill 只承载 FPGA 模块细则。

当任务涉及 FPGA RTL 修改、约束调整或仿真时，必须遵守以下核心约束：

1. **仅关注 FPGA 边界**：本模块只负责视频接入、Debayer、GAMMA、ROI 提取、OSD 显示叠加与 UART/AXI 寄存器物理通道。不做颜色/形状/尺寸分类、目标匹配、阈值管理和 myCobot 协议——这些归板上 CPU（见 `cpu_mycobot` Skill）。
2. **拒绝过度设计**：不在此处编写复杂视觉识别算法或机械臂轨迹算法。
3. **时序与约束联动**：任何时钟或复位信号修改必须同步检查 `constrain.sdc` 和 `.peri.xml`；改 `mem_test.xml` 或 IP `settings.json` 触发 Codex Gate。
4. **双通道对称**：改一路逻辑时，必须同步检查通道 0 / 通道 1 是否需要一致改动（信号和模块常用 `0`/`1` 后缀区分）。
5. **编译与警告**：Efinity 编译后必须记录 Setup/Hold Slack，不可忽略 CDC Warning；warning 被判断为可忽略时触发 Codex Gate。
6. **生成产物只读**：不直接改 `ipm/`、赛方补丁、原始压缩包、波形、`outflow/`。
7. **初赛 demo 仅作经验库**：不直接迁移其中的识别 RTL、`DEMO_MODE`、临时脚本或硬编码路径。
```

### 6.8 文件七：.agents/skills/cpu_mycobot/SKILL.md（新建）

```markdown
---
name: cpu_mycobot
description: Handles SoC RISC-V CPU firmware, UART registers, myCobot 280 serial communication, pose interpolation, and parameters management. Trigger when editing final_project/cpu/app code, bsp.h MMIO, mycobot protocol, or running arm read-only checks.
---

# SoC CPU 与 myCobot 机械臂开发规范

> 全局安全红线、Codex 审查门、决赛主线边界以 `AGENTS.md`「分赛区决赛主线」和 `分赛区决赛实施开发路线.md`（未被 `CURRENT_STATE.md` 降级/覆盖的部分）为准；本 Skill 只承载 CPU/机械臂模块细则。

当任务涉及 CPU 软件、串口控制协议或机械臂联动时，必须遵守以下核心约束：

1. **控制逻辑下放**：识别决策（颜色、尺寸分类）、目标匹配、阈值管理以及 myCobot 串口发包、重试与互锁逻辑，必须完全在板上 CPU（C 代码/固件）中实现。
2. **硬件解耦**：本模块通过 UART/寄存器接口与 FPGA 交互。不修改底层 RTL 视频流或 OSD 渲染逻辑，仅通过 AXI 寄存器回写结果（见 `fpga_vision` Skill）。
3. **机械臂安全第一（三阶段）**：
   - 第一阶段：只做环境检查、COM 口枚举、文档核对、只读状态读取方案。
   - 第二阶段：用户明确确认机械臂已固定、姿态安全、急停/断电方式明确后，才允许 RGB 灯板、读角度或极小幅动作测试。
   - 第三阶段：再讨论板上 CPU 通过 FPGA UART/GPIO 与机械臂控制器的协议桥接。未确认电平、接线和协议前，不把 FPGA 管脚直接接入机械臂控制线。
4. **波特率**：myCobot 280 串口控制固定 `1000000`；开发板 Type-C UART、JTAG-IF UART、UART2 波特率须按对应固件/RTL 另行确认。
5. **PC 仅开发期**：`pymycobot`、myBlockly、PC 只用于调试/标定/日志，不进正式识别控制闭环（见 `mycobot_pc_tests/` 归档说明）。
6. **动作类修改触发 Codex Gate**：涉及实际动作、夹爪、快速移动、FPGA-to-机械臂接线、CP210x 驱动安装或 `pymycobot` 控制脚本时，必须生成 Codex Review Packet。
```

### 6.9 文件八：CLAUDE.md 维护策略（结构性瘦身）

这是"瞻前顾后"的**根因治理**。原则：**单一事实源 + 易变内容外迁**。

- **跨 Agent 规则去重**：主线边界、机械臂安全三阶段、Codex Gate 触发清单、Review Packet 模板——这些**只在 `AGENTS.md` 维护**；`CLAUDE.md` 对应段落压缩为一行引用，例如：
  ```markdown
  ## 决赛主线与协作规则
  见 [AGENTS.md](AGENTS.md)「分赛区决赛主线」「Claude/Codex 协同分工」「机械臂与外设联调提示」。本文件不重复条款，只补充 Claude 专属上下文。
  ```
- **易变内容外迁**：`CLAUDE.md` 中"当前阶段/当前宏开关/DSI 已停用/当前烧录树"等会随阶段变化的描述，**移到 `CURRENT_STATE.md`**；`CLAUDE.md` 只留结构性、半年不变的内容（目录结构、工程入口、常用命令、CBM 配置）。
- **更新触发器**（写进 `AGENTS.md` 末尾，3 行即可）：
  - 路线/阶段/当前宏开关变化 → 改 `CURRENT_STATE.md`（会话内立即改）。
  - 新增目录/工程入口/常用命令 → 改 `AGENTS.md` + `CLAUDE.md` 对应段（当天）。
  - 安全红线/审查门变化 → 必须过 Codex Gate 才能改 `AGENTS.md` 对应段。

### 6.10 文件九：.claude/settings.json 权限放行

追加 1 条 allow，避免健康检查脚本每次人工放行：

```json
{
  "permissions": {
    "allow": [
      "Bash(git status *)",
      "Bash(git diff *)",
      "Bash(git log *)",
      "Bash(codebase-memory-mcp *)",
      "Bash(python *)",
      "PowerShell(tools/agent_handoff_health_check.ps1*)"
    ]
  }
}
```

### 6.11 落地顺序与验证

建议按收益降序分四步落地，每步可独立验证：

1. **第一步（10 分钟，收益最大）**：`AGENTS.md` 增量段 + `CURRENT_STATE.md` 初始版。解决"旧方案过度约束"。
   - 验证：新会话 Agent 读到优先级阶梯后，遇用户新指令时先改 `CURRENT_STATE.md` 而非回退旧路线。
2. **第二步**：`fpga-handoff.md` 模板扩展 + `HANDOFF_TEMPLATE.md` + `tools/agent_handoff_health_check.ps1` + `.claude/settings.json` 放行。解决"重复验证"。
   - 验证：用 `debug_records/camera_hdmi_handoff_2026-07-06.md` 跑一次健康检查；若 handoff 含队友本机绝对路径，脚本应给出 `WARN` 并继续按相对路径核查。只有仓库内相对路径缺失、关键工程文件缺失或只读检查失败时才进入 `Contradiction Report`。
3. **第三步**：两个 `SKILL.md`。解决"瞻前顾后"。
   - 验证：改 FPGA RTL 时只触发 `fpga_vision`，不联想到机械臂条款。
4. **第四步**：`CLAUDE.md` 结构性瘦身。根因治理，收益长期。
   - 验证：`CLAUDE.md` 行数下降，与 `AGENTS.md` 不再有大段重复；易变状态全部在 `CURRENT_STATE.md`。

---

## 7. 与 Codex 4.x 评审意见的对应关系

| Codex 评审意见 | 本 v1.1 落实项 |
|----------------|----------------|
| 4.2 强断言改为高置信接力 + 跳出条件 | 6.2 AGENTS.md「高置信交接规则」六条跳出条件 |
| 4.3 CURRENT_STATE.md 定位为增量索引 + 六字段 | 6.3 CURRENT_STATE.md 模板 |
| 4.4 Skills 承载细则、保留全局红线、加能力识别段 | 6.7/6.8 Skill 顶部红线引用 + 6.2 Skill 索引段 |
| 4.5 只读快速健康度脚本 | 6.6 `tools/agent_handoff_health_check.ps1` |
| 4.6 归档时机改为首个可验证 checkpoint | 6.2 AGENTS.md「归档时机」 |
| 4.7 五段式命名 | 6.4/6.5 Verified Facts / Evidence Index / Health Check / Contradiction Report / Next Immediate Action |
| 4.1 总体：不建议 v1.0 直接落地 | v1.1 全面采纳"高置信接力"定位 |

---

## 8. 仍需 Codex 复核的开放项

本 v1.1 落实方案属协作工作流层面的文档/脚本变更，不触及 RTL、约束、IP、机械臂动作等安全关键边界，按 `AGENTS.md` 不强制触发 Codex Gate。但建议 Codex 就以下三点做轻量复核：

1. `tools/agent_handoff_health_check.ps1` 的只读性是否彻底（确认无任何写入/运动/综合副作用）。
2. `AGENTS.md` 优先级阶梯第 2 项"安全红线"的列举是否完整，是否存在被 `CURRENT_STATE.md` 误覆盖的漏洞。
3. `CLAUDE.md` 瘦身后，是否存在 Claude 专属上下文被误删的风险（如本机 Efinity 路径、CBM junction 路径）。

复核通过后，按 6.11 落地顺序部署。

---

## 9. Gemini 批注处理与 Codex 二次校验

> 来源：Codex
> 日期：2026-07-07
> 处理范围：仅修订本协作方案；不修改 `分赛区决赛实施开发路线.md`、旧 HDMI 保底方案文件、RTL、约束或机械臂相关脚本。

### 9.1 批注 1：明确“决赛主线边界”来源

已解决。方案在 6.2 中新增“决赛主线边界引用规则”，并把优先级阶梯中的泛称改为具体文件：

- `AGENTS.md`「分赛区决赛主线」章节（当前版本约第 27-35 行）是硬边界。
- `分赛区决赛实施开发路线.md` 是路线图和经验库；其中未被 `CURRENT_STATE.md` 降级/覆盖的部分仍可作为路线依据。

后续任何 Agent 引用“决赛主线边界”时，都必须写明这两个来源，而不是只写抽象名词。

### 9.2 批注 2：HDMI 双摄透传保底方案已降级为旧结论

已解决。`CURRENT_STATE.md` 初始模板不再把“分赛区决赛保底方案实施 / 双摄 HDMI 透传 bring-up 进行中”作为默认当前阶段，而是改为：

- 当前阶段：分赛区决赛主线实施。
- 当前核心目标：由用户当轮指令、最新 handoff 或 `CURRENT_STATE.md` 最新状态条目声明。
- `HDMI 双摄透传 bring-up / 分赛区决赛保底方案`：降级为历史基础链路或旧结论，除非用户重新指定，否则不作为当前核心攻关目标。

本次未修改旧保底方案文件，只在协作方案中规定 Agent 不得再从旧保底方案自动推断当前阶段。

### 9.3 批注 3：本机绝对路径不可写入跨队友模板

已解决。方案选择“相对路径为主 + 动态脚本生成 + 绝对路径仅作来源追溯”的组合策略：

- JSON 模板中的 `repo_root_declared` 改为 `<current_workspace_root_resolved_at_handoff_generation>`。
- `HANDOFF_TEMPLATE.md` 中仓库根和烧录树改为占位符。
- 仓库内证据路径统一要求写成相对项目根路径，如 `final_project/...`。
- `tools/agent_handoff_health_check.ps1` 示例脚本改为用 `git rev-parse --show-toplevel` 动态获取当前仓库根。
- handoff 中出现队友本机绝对路径时，脚本只给 `WARN` 并继续按相对路径核查；绝对路径不得作为接力执行路径。

### 9.4 二次校验结论

本轮校验后，三处 Gemini 批注均已转化为实施规则。剩余绝对路径只保留在第 5 节“反例证据”中，用于说明跨机器路径风险；第 6 节的模板和脚本已去除对 `D:\第十届集创赛-雄芯院材料` / `D:\final_project` 的硬编码依赖。
