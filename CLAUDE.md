# CLAUDE.md

This file provides Claude Code specific guidance for this repository. Shared project rules, safety boundaries, and cross-agent handoff rules are maintained in `AGENTS.md`; current route overrides and mutable phase state are maintained in `CURRENT_STATE.md`.

## 必读入口

Claude 开始任何任务前按以下顺序读取：

1. `AGENTS.md`：仓库结构、决赛主线硬边界、机械臂安全、Codex Gate、Review Packet、交接规则。
2. `CURRENT_STATE.md`：最新路线增量、当前阶段、已降级或废弃的旧结论。
3. 本文件：Claude 专属执行方式、常用命令和交接命令。

若三者冲突，以 `AGENTS.md`「文档优先级与交接规则」为准。不要在本文件重复维护主线边界、机械臂安全三阶段或 Codex Gate 清单。

## 仓库性质

本仓库是第十届集创赛雄芯院方向的 FPGA 资料包和分赛区决赛开发工程，不是单一的软件工程。当前正式协作开发主工程位于 `final_project/`；`赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/` 和初赛 demo 是来源参考、对照工程和经验库，不再作为直接修改的决赛代码基线。

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

## 决赛主线与协作规则

见 `AGENTS.md`「分赛区决赛主线」「机械臂与外设联调提示」「文档优先级与交接规则」「Claude/Codex 协同分工」。本文件不重复条款，只补充 Claude 专属上下文。

当前阶段、旧方案降级、当前宏开关、当前烧录树等易变内容见 `CURRENT_STATE.md`。不要从旧 handoff 或旧保底方案自动推断当前核心目标。

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
- 输出必须包含：目标、涉及文件、信号链路、时钟/复位假设、双通道影响、备选方案、主要风险、验证计划。
- 方案结束时生成 Codex Review Packet，等待 Codex 复核后再进入执行。

### Mode B: 具体执行

用于局部 RTL 修改、testbench 或脚本调整、日志整理、常规调试。

- 建议使用廉价执行模型。
- 每轮只处理一个明确问题，优先小 diff，不做无关重排。
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
