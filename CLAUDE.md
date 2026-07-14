# CLAUDE.md

This file provides Claude Code specific guidance for this repository. Shared project rules, safety boundaries, and cross-agent handoff rules are maintained in `AGENTS.md`; current route overrides and mutable phase state are maintained in `CURRENT_STATE.md`.

## 必读入口

Claude 开始任何任务前按以下顺序读取：

1. `AGENTS.md`：仓库结构、系统架构硬边界、机械臂安全、Codex Gate、Review Packet、交接规则。
2. `final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md`：最新官方任务、评分、时间和现场流程硬约束。
3. `CURRENT_STATE.md`：最新路线增量、当前阶段、已降级或废弃的旧结论。
4. `final_project/docs/technical_plans/competition_score_maximization_execution_plan_20260712.md`：当前“决赛主方案”，定义拿分顺序、阶段门和回退策略。
5. 本文件：Claude 专属执行方式、常用命令和交接命令。

若这些入口冲突，以 `AGENTS.md`「文档优先级与交接规则」为准。不要在本文件重复维护官方细则、主线边界、机械臂安全三阶段或 Codex Gate 清单。

## 仓库性质

本仓库是第十届集创赛雄芯院方向的 FPGA 资料包和分赛区决赛开发工程，不是单一的软件工程。当前正式协作开发主工程位于 `final_project/`；`competition_project_single_camera/` 是等待 M0 新构建、匹配 bitstream、烧录和板级复现的隔离候选工程，未过 Gate 前不得替代正式主线。`赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/` 和初赛 demo 是来源参考、对照工程和经验库，不再作为直接修改的决赛代码基线。

除非任务明确要求，否则以下内容按只读处理：

- 赛方补丁目录与安装包
- ZIP / RAR 原始压缩包
- myBlockly、Python、CP210x 等机械臂相关安装包和驱动原件
- Efinity / ModelSim 生成数据库
- 波形导出文件（`.vcd`、`.gtkw`、`.wlf` 等）
- `outflow/` 等构建产物

## 主工程入口

正式开发主工程：

- FPGA 工程：`final_project/fpga/efinity/mem_test.xml`
- FPGA 约束：`final_project/fpga/efinity/constrain.sdc`
- FPGA 顶层：`final_project/fpga/rtl/top/top.v` 中的 `top`
- 板上 CPU 程序：`final_project/cpu/app/src/*.c`
- CPU BSP / MMIO 边界：`final_project/cpu/app/include/bsp.h`
- 接口契约和文档：`final_project/integration/`、`final_project/docs/`

候选单摄工程：`competition_project_single_camera/mem_test.xml`。只有 `CURRENT_STATE.md` 明确记录 M0 板级复现通过后，才可更新本节并讨论升格；在此之前不得把历史 bitstream 归因于候选仓库源码。

赛方对照工程：

- 工具链 / 器件：Efinity `2025.2.288.4.15`，Titanium `TJ375N529`，时序模型 `I3`
- 原始主工程文件：`赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/mem_test.xml`
- 原始主约束文件：`赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/constrain.sdc`
- 原始顶层模块：`src/top.v` 中的 `top`
- 主要生成 IP 配置入口：`ip/csi_rx_controller/settings.json`、`ip/dsi_tx/settings.json`、`ip/ram/settings.json`

## Codebase Memory 图谱

本项目已启用 codebase-memory-mcp，用于让 Claude/Codex/队友 Agent 在阅读代码前快速定位真实入口。

- 默认图谱项目：`D-cicc_cbm_link`
- 默认索引入口：`D:\cicc_cbm_link`
- 主 artifact：`.codebase-memory/graph.db.zst`
- Phase 2 资料库 artifact：`.codebase-memory/phase2/official_demo/`、`.codebase-memory/phase2/prelim_src/`、`.codebase-memory/phase2/prelim_sw/`
- 详细维护说明：`CBM_CONFIG_GUIDE.md`

推荐使用顺序：

```text
list_projects / index_status
-> get_architecture / search_graph / search_code
-> 回到真实源码、Efinity XML、日志和上板现象核查
```

图谱只负责定位和上下文压缩。涉及 RTL 连线、时钟复位、AXI/framebuffer、QCRV32、myCobot 实机安全或 warning 取舍时，必须以真实文件和验证日志为准。

## 权威层级与执行定位

Claude 必须按以下职责使用文档，不得把它们混成同一个“路线源”：

- 最新官方细则：定义四项任务、评分、10 分钟实操、现场操作与结果确认要求。
- `AGENTS.md`「分赛区决赛系统架构硬边界」：定义 FPGA、板上 CPU、PC/外部 MCU 的长期职责和安全红线。
- `CURRENT_STATE.md`：定义当前已完成项、阻塞、占位事实、暂缓验证项和下一步最小闭环。
- `final_project/docs/technical_plans/competition_score_maximization_execution_plan_20260712.md`：当前最高执行方案；局部方案、实现顺序和降级选择不得与其冲突，但不能覆盖官方细则、架构安全边界或真实进度。
- `分赛区决赛实施开发路线.md`：仅作历史实施路线与经验库；被官方细则或 `CURRENT_STATE.md` 覆盖的内容不得恢复为当前目标。

开始任务时，先把请求归类为“官方要求、稳定架构边界、当前状态或历史经验”中的一种或多种，再回到真实源码、工程 XML、日志和上板现象核查。不要从旧 handoff、旧保底方案、架构目标或 host 单测推断板级闭环。

与正式比赛闭环有关的方案或实现，必须显式说明是否覆盖：五色/三形状/三尺寸、四任务关系判定、逐轮识别—判断—执行事务、明确结果输出、唯一机械臂响应和 20 轮时限。具体缺口以 `CURRENT_STATE.md` 最新条目为准，不在本文件复制易过期的完成度清单。

## 常用命令

打开主 Efinity 工程：

```powershell
Invoke-Item "final_project\fpga\efinity\mem_test.xml"
```

确认 codebase-memory 主图谱：

```powershell
codebase-memory-mcp cli index_status "{`"project`":`"D-cicc_cbm_link`"}"
```

运行 RAM IP 仿真：

```powershell
Set-Location "赛方提供材料\TJ375N529_SC431HAI2LCD_Demo_V3\ip\ram\Testbench"
vsim -do modelsim.do
```

运行 DSI TX IP 仿真：

```powershell
Set-Location "赛方提供材料\TJ375N529_SC431HAI2LCD_Demo_V3\ip\dsi_tx\Testbench"
vsim -do modelsim.do
```

运行 AXI interconnect 仿真：

```powershell
Set-Location "赛方提供材料\TJ375N529_SC431HAI2LCD_Demo_V3\src\axi_interconnect\sim"
cmd /c run.bat
```

myCobot 280 环境只读检查：

```powershell
python -c "import sys, importlib.util; print(sys.executable); print(sys.version); print('serial', bool(importlib.util.find_spec('serial'))); print('pymycobot', bool(importlib.util.find_spec('pymycobot')))"
python -c "import serial.tools.list_ports as p; [print(x.device, x.description, x.hwid) for x in p.comports()]"
Get-PnpDevice -Class Ports -ErrorAction SilentlyContinue | Select-Object Status,Class,FriendlyName,InstanceId
```

交接健康检查：

```powershell
powershell -ExecutionPolicy Bypass -File "tools\agent_handoff_health_check.ps1"
powershell -ExecutionPolicy Bypass -File "tools\agent_handoff_health_check.ps1" -Handoff "debug_records\some_handoff.md"
```

若只想验证单个模块，优先去该模块附近查找 `modelsim.do` 或 `sim.do`。其中 AXI 仿真的 `run.bat` 依赖本机 ModelSim 安装路径，通常需要先把 `run.bat` 里的 `modelsim=` 改成本机实际路径。

## Claude 工作模式

### Mode A: 架构初设

用于重大结构调整、跨模块方案、视频链路/AXI/framebuffer/时序方案、顶层连线策略。

- 建议使用高能力模型，例如 Opus 系列。
- 默认只读探索，不直接修改 RTL、约束或工程文件。
- 必须先定位真实工程入口：`mem_test.xml`、`top.v`、相关 RTL 子目录和对应 IP `settings.json`。
- 必须分别列出：官方细则约束、`AGENTS.md` 架构边界、`CURRENT_STATE.md` 当前事实；不得把 `分赛区决赛实施开发路线.md` 单独当作当前权威来源。
- 输出必须包含：目标、涉及文件、信号链路、时钟/复位假设、双通道影响、备选方案、主要风险、验证计划。
- 方案结束时生成 Codex Review Packet，等待 Codex 复核后再进入执行。

### Mode B: 具体执行

用于局部 RTL 修改、testbench 或脚本调整、日志整理、常规调试。

- 建议使用廉价执行模型。
- 每轮只处理一个明确问题，优先小 diff，不做无关重排。
- 修改前必须核对 `CURRENT_STATE.md` 的当前阻塞和暂缓边界，并说明本轮是否改变四任务、逐轮事务、OSD 结果语义或机械臂唯一响应链路。
- 修改前说明影响范围；修改后列出文件、命令、结果、仍存在的 warning 和未验证项。
- 不直接重写 `ipm/`、赛方补丁、原始压缩包、波形和 `outflow/` 等生成产物。
- 若连续两轮未解决同一问题，应停止继续试补丁，改为生成 Codex Review Packet。

## Codex Review Packet 模板

```md
# Codex Review Packet

## 任务目标

## 当前结论

## 修改或计划涉及的文件

## 关键模块与信号链路

## 时钟、复位、AXI、framebuffer、双通道影响

## 已运行验证
- 命令：
- 结果：
- 日志位置：
- 关键 warning：

## 机械臂 / 外设状态
- myCobot 是否涉及：
- COM 口 / CP210x 状态：
- 波特率：
- 是否执行动作：
- 安全确认：

## 未验证项和风险假设

## 希望 Codex 判断的问题
```

## Handoff

阶段结束、切换 Agent 或任务暂停时，优先使用 `.claude/commands/fpga-handoff.md` 的 v1.1 JSON 模板，并同步全局协议：

- 写入 `~/.agents/handoff/{agent}-handoff-{YYYYMMDD_HHMMSS}.json`
- 向 `~/.agents/shared/today-summary.md` 追加一条短摘要
- 交接内容只记录下一位 Agent 恢复所需的信息，不写密钥和无关长日志
- 仓库内路径写相对项目根路径，绝对路径只作来源追溯
