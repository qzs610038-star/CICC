# 仓库指南

## 项目结构与模块组织
本仓库是第十届集创赛雄芯院方向的 TJ375N529/Efinity FPGA 资料包，主要内容位于 `赛方提供材料/`。

- `硬件文档/`：开发板说明、管脚定义、硬件框图及 TJ375 相关技术文档。
- `EDA软件/` 与 `EDA软件培训文档及视频/`：Efinity 安装说明、培训 PDF 和视频。
- `例程/`：赛方示例工程，包括 `RISC-V例程/` 压缩包和 `2ChMIPICSI_2ChMIPIDSI_Demo_Test/`。
- `TJ375N529_SC431HAI2LCD_Demo_V3/`：已解压的主要演示工程，包含 `src/` RTL、`ip/` 生成 IP、`constrain.sdc`、`mem_test.xml` 和 `outflow/` 输出。
- `初赛demo/2ChMIPICSI_2ChMIPIDSI_Demo_Test/`：已重新解压的初赛 demo，包含 `src/`、`sw/`、`embedded_sw/`、`ip/`、`outflow/` 和完整调试文档。该目录只作为经验库、问题库和链路参考，不作为决赛代码基线。
- `efinity-2025.2.288.4.15-windows-x64-patch/`：赛方工具补丁，除非任务明确涉及安装或补丁验证，否则按只读处理。
- `大象机械臂mycobot–280安装调试说明/`：myCobot 280 机械臂资料与软件包，包含 myBlockly、Python 3.10.4 安装包、CP210x USB-UART 驱动、安装调试说明和调试案例。

## 分赛区决赛主线
当前最高层路线文件是 `分赛区决赛实施开发路线.md`。所有代理在规划或修改时都应遵守以下边界：

- 决赛主线是 `FPGA 视频前端/ROI/统计特征/OSD + 板上 CPU 识别决策/参数管理/myCobot 控制`。
- 不再走纯 FPGA 视觉识别路线。颜色、形状、尺寸分类、目标匹配和阈值管理应放到板上 CPU；FPGA 只做高速视频接入、RAW/ROI/统计特征、显示叠加和必要硬件加速。
- 不再走纯 FPGA 机械臂控制主线。myCobot 协议封包、点位表、动作序列、互锁、超时和异常处理应放到板上 CPU；RTL 只保留 UART/FIFO/寄存器等硬件通道。
- PC、外部 MCU、`pymycobot` 只用于开发期调试、标定和日志观察，不进入正式识别/控制闭环。
- 初赛 demo 可借鉴 MIPI/DDR/framebuffer/debayer/gamma/OSD 调试顺序、AWB/背景误判经验、轻量几何特征、QCRV32/JTAG/AXI/OSD 回写经验；不得直接迁移其中的识别 RTL、`DEMO_MODE`、临时脚本、硬编码路径或旧 `outflow` 结论。
- 初赛 demo 的 README、Work_Log、修正方案、源码和构建日志存在版本差异；引用任何参数前必须以当前源码、最新构建日志和上板现象交叉确认。

## 构建、测试与开发命令
仓库根目录没有统一的包管理器、Makefile 或自动化构建脚本。FPGA 构建以 Efinity 2025.2 为准。

```powershell
rg --files "赛方提供材料\TJ375N529_SC431HAI2LCD_Demo_V3\src"
Invoke-Item "赛方提供材料\TJ375N529_SC431HAI2LCD_Demo_V3\mem_test.xml"
Set-Location "赛方提供材料\TJ375N529_SC431HAI2LCD_Demo_V3\ip\ram\Testbench"
vsim -do modelsim.do
```

第一条命令用于快速清点 RTL 源文件；第二条打开主要 Efinity 工程；后两条在已安装 ModelSim/Questa 时运行 RAM IP 仿真。

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
当前目录没有 Git 历史，因此无法推断既有提交规范。纳入版本管理后，建议使用简短祈使句提交信息，例如 `fix: correct CSI reset timing` 或 `docs: add board pin notes`。PR 应说明硬件目标、修改的 demo 或工程路径、完成的验证步骤，并附上 Efinity 构建、仿真或上板测试日志/截图。

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
- 方案试图恢复纯 FPGA 视觉识别主线，或把 myCobot 协议/动作状态机放回纯 RTL。
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
