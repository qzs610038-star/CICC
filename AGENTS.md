# 仓库指南

## 项目结构与模块组织
本仓库是第十届集创赛雄芯院方向的 TJ375N529/Efinity FPGA 资料包和分赛区决赛开发工程。赛方原始资料主要位于 `赛方提供材料/`，正式协作开发主线位于 `final_project/`。

- `final_project/`：分赛区决赛正式开发工程，包含 FPGA RTL/Efinity 工程、板上 CPU 程序、接口契约、测试和文档。
- `competition_project_single_camera/`：隔离的单摄候选 Efinity 工程。它保留已知可运行 Demo 的白名单源码和 M0 证据；在 `CURRENT_STATE.md` 记录的新构建、匹配 bitstream、烧录与板级复现门通过前，不替代 `final_project/` 的正式主线身份。
- `硬件文档/`：开发板说明、管脚定义、硬件框图及 TJ375 相关技术文档。
- `EDA软件/` 与 `EDA软件培训文档及视频/`：Efinity 安装说明、培训 PDF 和视频。
- `例程/`：赛方示例工程，包括 `RISC-V例程/` 压缩包和 `2ChMIPICSI_2ChMIPIDSI_Demo_Test/`。
- `TJ375N529_SC431HAI2LCD_Demo_V3/`：已解压的主要演示工程，包含 `src/` RTL、`ip/` 生成 IP、`constrain.sdc`、`mem_test.xml` 和 `outflow/` 输出。
- `初赛demo/2ChMIPICSI_2ChMIPIDSI_Demo_Test/`：已重新解压的初赛 demo，包含 `src/`、`sw/`、`embedded_sw/`、`ip/`、`outflow/` 和完整调试文档。该目录只作为经验库、问题库和链路参考，不作为决赛代码基线。
- `efinity-2025.2.288.4.15-windows-x64-patch/`：赛方工具补丁，除非任务明确涉及安装或补丁验证，否则按只读处理。
- `大象机械臂mycobot–280安装调试说明/`：myCobot 280 机械臂资料与软件包，包含 myBlockly、Python 3.10.4 安装包、CP210x USB-UART 驱动、安装调试说明和调试案例。
- `.codebase-memory/`：codebase-memory-mcp 共享图谱。默认图谱覆盖协作工程；Phase 2 精选资料库图谱位于 `.codebase-memory/phase2/`。

## Codebase Knowledge Graph
本项目已初始化 codebase-memory-mcp 图谱。Agent 做代码发现时应先使用图谱缩小范围，再回到真实文件核查。

- 默认项目：`D-cicc_cbm-main`（2026-07-14 双分支合并后完整重建，6078 nodes；精确边数以 `.codebase-memory/artifact.json` 为准）
- 兼容别名：`D-cicc_cbm_link` 是旧缓存项目，仅用于历史查询；它缺少本次 `arm_runtime` 和单摄候选符号，不再作为当前图谱真源。
- 默认入口：`D:\cicc_cbm_link` junction 指向本仓库真实路径。
- 主图谱 artifact：`.codebase-memory/graph.db.zst`
- Phase 2 资料库图谱：`.codebase-memory/phase2/official_demo/`、`.codebase-memory/phase2/prelim_src/`、`.codebase-memory/phase2/prelim_sw/`
- 使用顺序：`list_projects` / `index_status` -> `get_architecture` / `search_graph` / `search_code` -> 真实源码、工程 XML、日志和上板现象核查。

注意：CBM 对中文 Markdown 片段的返回可能出现编码替换字符；它适合做定位和结构发现，不替代原文阅读或 RTL/SoC 安全结论。

## 分赛区决赛系统架构硬边界

比赛任务、评分、时间限制和现场流程以 `final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md` 为最高比赛约束；当前完成度、阻塞和下一步以 `CURRENT_STATE.md` 为准。`分赛区决赛实施开发路线.md` 仅作为实施路线图和历史经验库，不再作为当前状态或最新赛题口径的唯一来源。

当前最高执行方案是 `final_project/docs/technical_plans/competition_score_maximization_execution_plan_20260712.md`（“决赛主方案”）。所有 Agent 的局部计划、实现顺序、阶段门和降级策略必须与其一致；它不覆盖官方细则、本文件的架构/安全硬边界，也不能把 `CURRENT_STATE.md` 中尚未完成的能力推断为已经闭环。

所有 Agent 在规划、修改和验收时必须遵守以下架构边界：

- 正式系统主线是 `FPGA 视频采集/视频前端/ROI/统计特征/OSD 渲染/硬件通道 + 板上 CPU 分类决策/四任务匹配/逐轮状态机/参数管理/myCobot 控制`。
- FPGA 负责 MIPI/RAW/Debayer/Gamma 等视频前端、ROI 与基础统计特征、OSD 像素渲染以及 AXI/UART/FIFO 等硬件通道；不得把颜色、形状、尺寸分类、四任务关系判定、阈值管理或机械臂动作状态机重新放入纯 RTL。
- 板上 CPU 负责五色、三形状、三尺寸分类，任务一至任务四的目标配置与关系判定，每轮“识别—判断—执行”事务、结果回写、参数管理，以及 myCobot 协议、点位表、动作序列、互锁、超时、有限重试和异常停机。
- OSD 职责按“CPU 产生结果语义、FPGA 完成画面渲染”划分。每轮必须能够明确输出识别结果、目标/非目标判断、执行或不执行及理由，不得只输出无法解释的寄存器值。
- PC、外部 MCU、`pymycobot`、myBlockly 只用于开发期调试、标定、健康检查、日志和录像，不进入正式识别/控制闭环。
- 当前尚未完成或未经验证的接口、地址、bitstream、板级现象和机械臂实机结果，不得因为符合总体架构方向而描述为已闭环；具体状态必须引用 `CURRENT_STATE.md` 和真实证据。

### 初赛 demo 与历史资料使用边界

- 初赛 demo 只用于借鉴 MIPI/DDR/framebuffer/debayer/gamma/OSD 调试顺序、AWB/背景误判经验、轻量几何特征以及 QCRV32/JTAG/AXI/OSD 回写经验。
- 不得直接迁移初赛识别 RTL、`DEMO_MODE`、临时脚本、硬编码路径或旧 `outflow` 结论。
- 初赛 README、Work_Log、修正方案、源码和构建日志存在版本差异；引用参数前必须以真实源码、最新构建日志和上板现象交叉确认。

## 官方分赛区决赛细则（核心目标与约束）

最新官方细则的仓库内可检索版本是 `final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md`，原始证据是 `赛方提供材料/第十届集创赛分赛区决赛“雄芯院”企业命题比赛细则-0710新.pdf`。所有 Agent 在规划任务、设计接口、定义验收和判断优先级前必须先读该 Markdown；疑义回到原 PDF 和现场企业专家口径。

以下内容注册为不可由旧规划、旧 handoff 或历史 demo 覆盖的核心比赛约束：

- 完整演示包含四项任务，每项 5 轮：指定颜色正方体；指定形状（正方体）+颜色；与 2cm/3cm 参考正方体边长差等于 1cm；与 2cm/3cm 目标正方体边长差≤0.5cm。
- 被测物体覆盖正方体、圆柱体、锥体，颜色覆盖白、黑、红、蓝、黄，尺寸覆盖 2cm、2.5cm、3cm。系统的目标输入、识别结果和测试集不得遗漏白/黑色或相对尺寸关系。
- 四任务实操总时限不超过 10 分钟；部署调试批次为 10 分钟且应达到一键启动。超时任务不计分。
- 每轮必须按评委随机顺序完成摆放、识别、目标判断、执行分拣或不分拣，并准确无异议地输出识别结果、判断结果和执行/不执行理由；顺序确定后不得调换。
- 单轮评分按识别 25%、判断 25%、执行 50% 串行计算；识别错误会使后续环节不得分。正确识别后，机械臂必须给出明确且唯一的响应。
- 目标物须轻取轻放到相对起点旋转 180°（允许正负 10°）且机械臂最大臂展处；任何提前释放、未夹稳并与台面碰撞（含轻微碰撞）均按跌落处理。
- 演示全程自行录像；现场签字确认后不接受复议。现场规则疑义必须当场确认。

项目对照差距和建议验收顺序见 `final_project/docs/competition_manual/细则对照项目优化建议_20260712.md`。该建议文件可随工程事实更新，但不得改写官方条款。

## 构建、测试与开发命令
仓库根目录没有统一的包管理器、Makefile 或自动化构建脚本。正式工程优先使用 `final_project/`；`competition_project_single_camera/` 仅按其 M0 Gate 做候选工程复现；赛方主 demo 只作为来源参考和必要时的对照工程。FPGA 构建以 Efinity 2025.2 为准。

```powershell
codebase-memory-mcp cli index_status "{`"project`":`"D-cicc_cbm-main`"}"
Invoke-Item "final_project\fpga\efinity\mem_test.xml"
rg --files "final_project\fpga\rtl"
Set-Location "赛方提供材料\TJ375N529_SC431HAI2LCD_Demo_V3\ip\ram\Testbench"
vsim -do modelsim.do
```

第一条命令用于确认 CBM 主图谱可用；第二条打开正式 Efinity 工程；第三条快速清点正式 RTL；后两条在已安装 ModelSim/Questa 时运行赛方 RAM IP 对照仿真。

myCobot 280 机械臂联调优先走只读环境检查和串口探测，不要直接执行动作命令：

```powershell
python -c "import serial.tools.list_ports as p; [print(x.device, x.description, x.hwid) for x in p.comports()]"
python -c "import importlib.util; print('pymycobot', bool(importlib.util.find_spec('pymycobot')))"
```

机械臂文档指定 myBlockly 初始化型号选择 `MyCobot`，串口选择本机 COM，波特率使用 `1000000`。若需安装 Python 控制库，使用 `pip install pymycobot --upgrade --user`；安装前先确认当前 Python 和 PATH，避免污染 Efinity 或其他项目环境。

## 编码风格与命名约定
Verilog/SystemVerilog 代码应沿用现有风格：文件名使用 snake_case，通常一个文件对应一个主要模块，保留赛方和 IP 生成器已有模块名。新增 RTL 建议使用 4 空格缩进，端口列表保持对齐。不要无意义重排生成 IP、补丁文件或赛方原始源码。约束集中维护在 `constrain.sdc`，工程设置集中维护在 `mem_test.xml` 与 `.peri.xml` 文件中。

## 测试规范
修改 RTL 前，先确认受影响的顶层模块、子模块或 IP 位于 `src/` 还是 `ip/`。可行时补充或更新本地 testbench，优先复用现有 `Testbench/`、`.do`、`.mem`、`.vh` 文件组织方式。至少应对修改后的工程运行 Efinity 综合、布局布线，并记录 Efinity 版本、目标开发板和仍然存在的关键 warning。

## 提交与 Pull Request 规范
本仓库已纳入 Git。默认协作方式是各自在 `dev/用户名` 分支开发，完成后向 `main` 提 Pull Request，并至少经过 1 人审查后合并。提交信息使用简短祈使句，例如 `fix: correct CSI reset timing`、`docs: add board pin notes`、`chore: refresh codebase graph`。PR 应说明硬件目标、修改的工程路径、完成的验证步骤，并附上 Efinity 构建、仿真或上板测试日志/截图。

不要提交 `.claude/settings.json`、`.claude/scheduled_tasks.lock`、本机路径、许可证、临时数据库或板卡私密配置。`.claude/settings.local.json` 若作为示例保留，提交前必须检查是否包含个人路径或过宽权限。

## 安全与配置提示
不要提交许可证文件、本机 Efinity 安装路径、临时数据库或板卡相关私密配置。大型赛方原始压缩包、安装器和补丁包应保持不变；派生修改应落在源码、约束文件或单独说明文档中。

## 机械臂与外设联调提示
本项目当前机械臂控制软件与硬件链路以大象机器人 myCobot 280 为准，不再默认按 Dobot 或其他机械臂处理。

- 控制软件：`myblockly.Setup.1.3.6.exe`；Python 推荐按资料包使用 Python 3.10.4 或当前可用 Python，但必须能安装 `pymycobot`。
- 串口驱动：资料包提供 CP210x USB-UART 驱动，优先安装高版本驱动；设备识别可关注 `VID_10C4` / Silicon Labs / CP210x。
- 接线规则：USB 转 TTL 模块按资料写法为 `TXD -> 机械臂 TX`、`RXD -> 机械臂 RX`、`GND -> 机械臂 GND`；通电前机械臂必须置于说明文档要求的安全姿态。
- 通信参数：myBlockly 选择 `MyCobot` 型号，本机 COM 口，波特率 `1000000`。
- 调试动作：先做只读状态读取、RGB 灯板或极小幅安全演示，再考虑关节、夹爪或快速移动。任何会导致机械臂运动的命令都必须显式确认目标、速度、角度范围和急停/断电方式。
- FPGA 联动：TJ375N529 侧可通过 Type-C UART、JTAG-IF UART、UART2 TO Peripherals 或预留 3.3V GPIO/J13/J15 做链路探索；正式方案应由板上 CPU 通过 UART 协议控制 myCobot，FPGA RTL 只提供硬件通道。在未确认电平、接线和协议前，不要把 FPGA 输出直接接入机械臂控制线。

## 文档优先级与交接规则

### 优先级阶梯（高 → 低）
1. 用户当前对话中的明确指令。
2. 最新官方比赛细则（`final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md`）与本文件中的安全红线：机械臂动作限制、Codex 审查门、系统架构硬边界、Git 提交规范。两者均为硬约束：官方细则定义比赛目标、任务语义、评分和现场流程；安全红线约束实现与验证方式。若两者疑似冲突，停止扩展修改并请求用户或现场专家确认，不得自行弱化任一方。
3. `CURRENT_STATE.md`：最新路线增量与当前状态索引，可覆盖旧规划中的路线选择、任务优先级和已废弃方案，但不可覆盖第 2 级硬约束。
4. `final_project/docs/technical_plans/competition_score_maximization_execution_plan_20260712.md`：当前“决赛主方案”，定义拿分顺序、阶段门和回退策略；不得覆盖前三级的硬约束与真实状态。
5. `SESSION_HANDOFF.md` 或 `debug_records/*handoff*.md`：高置信接力事实，需先跑 `tools/agent_handoff_health_check.ps1`。
6. `分赛区决赛实施开发路线.md` 等历史规划文件。
7. 初赛 demo 文档与旧日志：仅作经验库，不作事实来源。

真实源码、工程 XML、构建日志和上板现象始终是最终事实来源；上述文件只记录对它们的最新结论和证据位置。

### 比赛目标与架构边界引用规则
- 引用比赛任务、评分、时限或现场流程时，必须引用 `final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md` 的具体章节；不得用旧路线摘要代替官方细则。
- 引用系统职责边界时，必须引用 `AGENTS.md`「分赛区决赛系统架构硬边界」；FPGA 负责视频前端/ROI/统计特征/OSD 渲染/硬件通道，板上 CPU 负责分类决策、四任务关系判定、逐轮状态机、参数管理和 myCobot 控制。
- 引用当前进度、阻塞、占位地址、验证结论或下一步时，必须引用 `CURRENT_STATE.md` 和对应真实文件/日志；不得从架构目标推断完成状态。
- `分赛区决赛实施开发路线.md` 只作为路线图和经验库；其中已被官方细则或 `CURRENT_STATE.md` 覆盖的旧目标不得再作为当前验收标准。

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
- Skill 承载模块细则，不取代本文件的全局红线。比赛任务以最新官方细则为准，系统职责边界以 `AGENTS.md`「分赛区决赛系统架构硬边界」为准，当前状态以 `CURRENT_STATE.md` 为准。

## Claude/Codex 协同分工
本项目采用 Claude 执行、Codex 复核的双工具协作方式。Claude 侧详细工程上下文以 `CLAUDE.md` 为准；Codex 侧在本文件基础上承担独立审查、纠偏和困难救场职责。

### 推荐角色
- **Claude 高能力模型**：用于重大架构初步方案设计，例如跨子系统的视频链路调整、AXI / framebuffer 方案、顶层连接策略、时序风险评估。默认只读探索，不直接改 RTL。
- **Claude 廉价模型**：用于具体执行，例如局部 RTL 修改、脚本/testbench 调整、日志整理和常规调试。执行必须小步推进，保留验证命令和结果。
- **Codex 高能力模型**：用于方案审核、diff 审查、验证闭环检查、风险降级和困难救场。Codex 不应默认相信 Claude 的方案或日志解释，必须回到真实文件、工程清单和可运行命令核实。

### Codex 审查门
以下情况必须由 Codex 复核后再继续扩大修改范围：
- 修改跨越两个以上子系统，或涉及 `src/top.v` 的系统级连线。
- 涉及时钟、复位、视频时序、AXI burst、帧缓存地址、位宽转换、双通道同步。
- 修改 `constrain.sdc`、`mem_test.xml`、`.peri.xml` 或 IP `settings.json`。
- 涉及 myCobot 280 实际动作、夹爪控制、FPGA-to-机械臂串口/GPIO 接线、CP210x 驱动安装或 `pymycobot` 控制脚本。
- 涉及 QCRV32、BSCAN/JTAG、CPU DDR/AXI、`axi_reg_file`、`results_cdc`、CPU 到 OSD 回写链路。
- 方案试图恢复纯 FPGA 视觉识别主线，或把四任务关系判定、逐轮事务、myCobot 协议/动作状态机放回纯 RTL。
- Efinity / ModelSim / Questa 的 warning 被标记为可忽略。
- Claude 连续两轮调试失败，或同一错误被反复用不同补丁尝试。
- 需要判断一个高层方案是否“合理可行”。

### 交接包要求
Claude 交给 Codex 审查时，应提供精简但可复现的 Review Packet：
- 任务目标与当前结论。
- 修改文件列表和关键 diff 摘要。
- 涉及的模块、信号、时钟域、复位链路和双通道对应关系。
- 已运行的命令、日志位置、通过/失败结果和关键 warning。
- 未验证项、风险假设和希望 Codex 判断的问题。

Codex 审查输出应优先给出阻塞问题、证据路径和下一步最小修复建议；不要只做泛泛评价。

## myCobot PC端联调归档说明
- 前期通过 PC 端 Python (`pymycobot`) 进行的机械臂本体和夹爪健康度测试脚本已归档至 `mycobot_pc_tests/` 目录下（包含安全微动、急停变软、状态读取和夹爪控制）。
- **禁止依赖**：这些 Python 脚本仅作为硬件健康证明和底层的指令序列参考。任何正式的识别/控制闭环代码均不得依赖这些脚本或运行在 PC 端的 `pymycobot`，必须按决赛主线规划，将机械臂的控制逻辑（如协议解析、串口发包）下放至 FPGA 板上 CPU 运行。
