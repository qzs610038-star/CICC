# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working in this repository.

## 仓库性质
本仓库是第十届集创赛雄芯院方向的 FPGA 资料包，不是单一的软件工程。实际开发主工程位于 `赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/`；`赛方提供材料/例程/2ChMIPICSI_2ChMIPIDSI_Demo_Test/` 是高度相关的参考工程，目录结构和部分 RTL 基本对应。

当前分赛区决赛方案以 `分赛区决赛实施开发路线.md` 为最高层路线文件。已经重新解压并复扫的 `初赛demo/2ChMIPICSI_2ChMIPIDSI_Demo_Test/` 只作为经验库、风险清单和链路参考，不作为决赛代码基线。任何方案都必须保持“FPGA 视频前端 / ROI / 统计特征 / OSD + 板上 CPU 识别决策与机械臂控制”的主线，不再回到纯 FPGA 视觉识别或纯 FPGA 机械臂控制。

除非任务明确要求，否则以下内容应按只读处理：
- 赛方补丁目录与安装包
- ZIP / RAR 原始压缩包
- myBlockly、Python、CP210x 等机械臂相关安装包和驱动原件
- Efinity / ModelSim 生成数据库
- 波形导出文件（`.vcd`、`.gtkw`、`.wlf` 等）
- `outflow/` 等构建产物

## 主工程入口
- 工具链 / 器件：Efinity `2025.2.288.4.15`，Titanium `TJ375N529`，时序模型 `I3`
- 主工程文件：`赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/mem_test.xml`
- 主约束文件：`赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/constrain.sdc`
- 顶层模块：`src/top.v` 中的 `top`
- 主要生成 IP 配置入口：
  - `ip/csi_rx_controller/settings.json`
  - `ip/dsi_tx/settings.json`
  - `ip/ram/settings.json`

## 分赛区决赛主线
本项目当前不是继续完善初赛纯 RTL 识别 demo，而是另起更稳的决赛闭环：

```text
MIPI 摄像头
  -> FPGA CSI/RAW/ROI/降采样/统计特征/OSD
  -> 板上 CPU: 颜色 + 形状 + 尺寸分类、目标匹配、参数切换
  -> 板上 CPU: myCobot 协议、点位表、动作状态机、安全互锁
  -> FPGA HDMI 输出调试画面，PC 仅显示/录制/开发期标定
```

硬性边界：
- 不再把颜色、形状、尺寸分类固化成大段纯 RTL 决策树。FPGA 可做像素流、ROI、直方图/均值/前景统计、低分辨率缓冲和必要硬件加速。
- 不让 CPU 全帧逐像素扫 DDR 作为常规路径。CPU 只读小 ROI、降采样帧或统计特征，避免与 HDMI/framebuffer 抢带宽。
- 不再把 myCobot 协议、点位序列、抓取判断和异常处理写成纯 RTL 状态机。RTL 只保留 UART/FIFO/寄存器等硬件通道。
- PC、外部 MCU、`pymycobot` 只用于开发期调试、标定和日志，不进入正式识别/控制闭环。

### 初赛 demo 可借鉴内容
`初赛demo/2ChMIPICSI_2ChMIPIDSI_Demo_Test/` 已包含 `src/`、`sw/`、`embedded_sw/`、`ip/`、`outflow/` 等完整树。可借鉴但不直接迁移：
- MIPI -> DDR/framebuffer -> debayer/gamma -> HDMI/OSD 的调试顺序。
- Bayer 相位、gamma、白平衡、AWB 压缩 HSV 饱和度、背景误分类等视觉工程经验。
- `sw/shape_detect.c` 中 bbox、面积、行宽轮廓、锥度、过渡行等轻量特征思路。
- QCRV32 JTAG/BSCAN、AXI/DDR、`axi_reg_file`、`results_cdc`、OSD 回写的闭环经验。

必须警惕：
- 初赛目录中 README、Work_Log、修正方案、源码和 `outflow` 之间存在版本差异；引用参数时必须回到当前源码和最新构建/上板结果交叉确认。
- `DEMO_MODE`、临时脚本、硬编码路径、debug 残留和旧日志不能进入正式工程。

## 机械臂与控制软件入口
本项目机械臂方案以大象机器人 myCobot 280 为准，资料位于 `赛方提供材料/大象机械臂mycobot–280安装调试说明/`。不要再默认套用 Dobot Magician 或其他机械臂流程。

- 控制软件：`大象机械臂mycobot–280安装调试说明/大象机械臂mycobot–280安装调试说明/相关软件/myblockly.Setup.1.3.6.exe`
- Python 安装包：`相关软件/python-3.10.4-amd64.exe`
- 串口驱动：`相关软件/CP210x_VCP_Windows/`
- 安装说明：`机械臂mycobot280的安装及调试说明.pdf`
- 调试案例：`机械臂mycobot280调试案例.docx` / `.pdf`
- Python 控制库：`pymycobot`，安装命令为 `pip install pymycobot --upgrade --user`

myCobot 280 文档指定 PC 端通过 USB 转 TTL 串口通信。接线记录为 `TXD -> 机械臂 TX`、`RXD -> 机械臂 RX`、`GND -> 机械臂 GND`；通电前必须把机械臂置于说明文档要求的正确姿态，避免损坏。myBlockly 初始化时型号选择 `MyCobot`，串口选择本机 COM，波特率选择 `1000000`。决赛正式方案中，PC 只用于开发期标定和安全验证，最终动作逻辑应迁移到板上 CPU，由 CPU 通过 UART 协议控制 myCobot。

## 常用命令
仓库根目录没有统一的 Makefile、包管理器、lint 命令或测试总入口。日常开发以 Efinity 工程和各子模块自带的 ModelSim / Questa 脚本为主。

打开主 Efinity 工程：
```powershell
Invoke-Item "赛方提供材料\TJ375N529_SC431HAI2LCD_Demo_V3\mem_test.xml"
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

若只想验证单个模块，优先去该模块附近查找 `modelsim.do` 或 `sim.do`。其中 AXI 仿真的 `run.bat` 依赖本机 ModelSim 安装路径，通常需要先把 `run.bat` 里的 `modelsim=` 改成本机实际路径。

myCobot 280 环境只读检查：
```powershell
python -c "import sys, importlib.util; print(sys.executable); print(sys.version); print('serial', bool(importlib.util.find_spec('serial'))); print('pymycobot', bool(importlib.util.find_spec('pymycobot')))"
python -c "import serial.tools.list_ports as p; [print(x.device, x.description, x.hwid) for x in p.comports()]"
Get-PnpDevice -Class Ports -ErrorAction SilentlyContinue | Select-Object Status,Class,FriendlyName,InstanceId
```

若仅看到蓝牙 COM 口，不能认为机械臂已连接；应优先检查 CP210x 驱动和 USB-TTL 线。资料包 CP210x 驱动说明建议优先安装高版本驱动，不行再尝试低版本。

## 整体架构
主设计是一个围绕 `src/top.v` 搭建的双路视频处理链路。顶层把以下几类模块串接在一起：

- 双路 MIPI CSI-2 图像输入与摄像头控制
- 基于 AXI 的帧缓存与外部存储访问
- Bayer 转 RGB 与图像增强处理
- MIPI DSI 显示输出
- HDMI / TMDS 输出
- 可选的 UVC / FX3 输出链路

决赛工程需要在视频链路旁保留 CPU 协同通路：FPGA 负责稳定产生 ROI/统计特征和 OSD 叠加，板上 CPU 负责分类、目标条件切换、动作状态机和机械臂协议。评估任何视觉或机械臂方案时，优先检查是否破坏了这个边界。

理解代码时，以下结构关系最重要：

- `src/top.v` 是系统集成中心。主要子模块都在这里例化，并通过宏控制功能开关，例如 `FRAME_BUFFER`、`HDMI_OUT_EN`。
- 设计明显按双通道组织，很多信号和模块都会成对出现，通常用 `0` / `1` 后缀区分两路。因此改一路逻辑时，要同步检查另一路是否也应保持一致。
- `src/framebuffer/` 是整套设计的存储与时序核心，负责视频流写入/读出、AXI burst 访问、数据打包拆包、显示时序对齐等。
- `src/axi_interconnect/rtl/` 是视频链路使用的 AXI 互连与仲裁逻辑。
- `src/mipi_csi/` 是 CSI 接收相关逻辑；其中 `cam_i2c_ctrl/` 下面是摄像头 I2C 配置与寄存器初始化链路。
- `src/mipi_dsi/` 是面板配置与 DSI 发射链路。
- `src/dvi_tx/` 是 HDMI / TMDS 输出通路。
- `src/debayer/` 负责 Bayer 原始图像到 RGB 的转换。
- `src/contract_bright/` 负责亮度 / 对比度调整。
- `src/uvc_src/` 是可选的 UVC / FX3 输出路径及其颜色格式处理辅助模块。

## FPGA 与机械臂联动边界
TJ375N529 开发板提供 3 路 UART：JTAG-IF UART、Type-C 转 UART、UART2 TO Peripherals，并有 J13/J15 预留 3.3V GPIO。FPGA-to-myCobot 联动应按“PC 端先验证 myCobot 串口控制，再让板上 CPU 输出 myCobot 协议，FPGA RTL 只提供串口/FIFO/寄存器通道”的顺序推进。

- 第一阶段：Claude 只能做 myCobot 环境检查、COM 口枚举、文档核对和只读状态读取方案。
- 第二阶段：在用户明确确认机械臂已固定、姿态安全、急停/断电方式明确后，才允许 RGB 灯板、读取角度或极小幅动作测试。
- 第三阶段：再讨论板上 CPU 通过 FPGA UART/GPIO 通道与机械臂控制器之间的协议桥接。未确认电平、接线和协议前，不要把 FPGA 管脚直接接入机械臂控制线。
- 默认波特率 `1000000` 仅用于 myCobot 串口控制；开发板 Type-C UART、JTAG-IF UART、UART2 的波特率必须根据对应固件或 RTL 另行确认。

## 关键文件的职责
- `mem_test.xml` 是主工程的权威清单，定义了实际参与构建的源码列表、器件信息、综合 / 布局布线参数以及关联 IP。
- `ip/*/settings.json` 是各生成 IP 的配置源；若只是改参数，优先改这里，不要直接手改 `ipm/` 下的生成产物。
- `debug_profile.wizard.json` 记录了工程里的在线调试 / 逻辑分析探针配置。
- `src/axi_interconnect/sim/`、`src/framebuffer/modelsim_tb/`、`src/uvc_src/color_bar_v3.0/` 这些目录保留了针对局部子系统的独立仿真资产，排查单块问题时很有用。

## 修改时的工作方式
- 只有在系统连线关系变化时才优先改 `src/top.v`；如果问题属于某个子模块内部，应尽量把改动限制在该子系统目录内。
- 对生成文件和赛方原始文件保持克制，尽量不要直接重写 `ipm/`、补丁内容、波形文件、仿真 work 数据库和原始压缩包。
- 凡是涉及视频时序、AXI 位宽、帧缓存地址、双通道对称结构的改动，都要联动检查参数定义、模块例化和另一路通道，而不是只改单点。

## Claude/Codex 协同工作流
本仓库采用“Claude 负责初设与执行，Codex 负责独立审核、纠偏和困难救场”的协作模式。全局多 Agent 协议仍以 `~/.agents/contracts/` 和 `~/.agents/protocols/` 为准；本节只定义本 FPGA 项目的裁剪规则。

### Mode A: 架构初设
用于重大结构调整、跨模块方案、视频链路/AXI/framebuffer/时序方案、顶层连线策略。

- 建议 Claude 使用高能力模型，例如 Opus 系列。
- 默认只读探索，不直接修改 RTL、约束或工程文件。
- 必须先定位真实工程入口：`mem_test.xml`、`src/top.v`、相关 `src/` 子目录和对应 IP `settings.json`。
- 输出必须包含：目标、涉及文件、信号链路、时钟/复位假设、双通道影响、备选方案、主要风险、验证计划。
- 方案结束时生成 Codex Review Packet，等待 Codex 复核后再进入执行。

### Mode B: 具体执行
用于局部 RTL 修改、testbench 或脚本调整、日志整理、常规调试。

- 建议 Claude 使用廉价执行模型。
- 每轮只处理一个明确问题，优先小 diff，不做无关重排。
- 修改前说明影响范围；修改后列出文件、命令、结果、仍存在的 warning 和未验证项。
- 不直接重写 `ipm/`、赛方补丁、原始压缩包、波形和 `outflow/` 等生成产物。
- 若连续两轮未解决同一问题，应停止继续试补丁，改为生成 Codex Review Packet。

### Codex Gate
以下情况必须交给 Codex 审查：

- 改动跨越两个以上子系统，或涉及 `src/top.v` 的系统级连接。
- 涉及时钟、复位、视频时序、AXI burst、帧缓存地址、位宽转换、双通道同步。
- 修改 `constrain.sdc`、`mem_test.xml`、`.peri.xml` 或 IP `settings.json`。
- Efinity / ModelSim / Questa warning 被判断为可忽略。
- Claude 连续两轮调试失败。
- 用户询问方案是否“合理可行”、是否可以作为比赛主路线或是否存在隐藏风险。
- 涉及 myCobot 280 实际动作、夹爪、快速移动、FPGA-to-机械臂 UART/GPIO 接线、CP210x 驱动安装或 `pymycobot` 控制脚本。
- 任何方案试图恢复纯 FPGA 视觉识别主线，或把 myCobot 协议/动作状态机放回纯 RTL。
- 改动 QCRV32、BSCAN/JTAG、CPU DDR/AXI、`axi_reg_file`、`results_cdc`、CPU/OSD 回写链路。

### Codex Review Packet 模板
交给 Codex 时，使用简洁、可复现的格式：

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

### Handoff
阶段结束、切换 Agent 或任务暂停时，按全局协议写入 `~/.agents/handoff/{agent}-handoff-{YYYYMMDD_HHMMSS}.json`，并追加一条短摘要到 `~/.agents/shared/today-summary.md`。交接内容只记录下一位 Agent 恢复所需的信息，不写密钥和无关长日志。
