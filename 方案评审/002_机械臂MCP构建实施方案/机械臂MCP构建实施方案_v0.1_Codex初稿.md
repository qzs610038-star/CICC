# 机械臂 MCP 构建实施方案 v0.1 Codex 初稿

日期：2026-07-01  
状态：供 Claude 审核与实施拆解  
适用范围：`D:\第十届集创赛-雄芯院材料`

## 结论摘要

建议建设一个本地 stdio MCP：`fpga_robot_mcp`。它面向 Claude Code / Codex 暴露 Efinity 工程检查、构建、烧录、开发板串口枚举、myCobot 280 环境检查、机械臂安全联调和日志归档工具。

该 MCP 是开发期工具，不进入比赛运行闭环。正式比赛闭环仍固定为：

```text
MIPI 摄像头
  -> FPGA: CSI/RAW/ROI/统计特征/OSD
  -> 板上 CPU: 颜色/形状/尺寸分类、目标匹配、参数切换
  -> 板上 CPU: myCobot 协议、点位表、动作状态机、安全互锁
  -> FPGA HDMI/OSD 显示调试状态
```

PC 端 MCP 可以帮助 Claude / Codex 完成环境核查、Efinity 构建与烧录、开发期 myCobot 标定和日志整理，但不能把 PC、`pymycobot` 或 MCP 本身作为最终控制路径。

## 已确认的本地事实

### 开发板与 FPGA

- 工具链 / 器件：Efinity `2025.2.288.4.15`，Titanium `TJ375N529`，时序模型 `I3`。
- 主工程：`赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/mem_test.xml`。
- 主约束：`赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/constrain.sdc`。
- 顶层：`src/top.v` 中的 `top`。
- 决赛开发板口径：`WZZY_FPGA / TJ375N529`。
- 板上 CPU 路线：使用板上 CPU / QCRV32 经验链路承接分类、参数管理、myCobot 协议和动作状态机。
- 相关链路：JTAG/BSCAN、AXI/DDR、`axi_reg_file`、`results_cdc`、CPU 到 OSD 回写链路。
- 可探索外设通道：JTAG-IF UART、Type-C 转 UART、UART2 TO Peripherals、J13/J15 预留 3.3V GPIO。

### 机械臂与控制软件

- 机械臂：大象机器人 myCobot 280。
- 资料目录：`赛方提供材料/大象机械臂mycobot–280安装调试说明/`。
- 控制软件：`myblockly.Setup.1.3.6.exe`。
- Python 安装包：`python-3.10.4-amd64.exe`。
- 串口驱动：`CP210x_VCP_Windows`，设备识别关注 Silicon Labs / CP210x / `VID_10C4`。
- Python 控制库：`pymycobot`，资料建议命令为 `pip install pymycobot --upgrade --user`。
- myBlockly 初始化：型号选择 `MyCobot`，串口选择本机 COM，波特率 `1000000`。
- 文档记录的 USB-TTL 接线：`TXD -> 机械臂 TX`、`RXD -> 机械臂 RX`、`GND -> 机械臂 GND`。
- 安全要求：通电前必须把机械臂放到说明文档要求的正确姿态；任何关节、坐标、夹爪和快速移动命令都需要明确动作摘要和用户确认。

### 当前本机状态快照

- 当前 Python：`C:\Program Files\Python313\python.exe`，版本 `3.13.1`。
- `pyserial` 可导入。
- `pymycobot` 当前未安装。
- 当前枚举到的 COM 口只有蓝牙 COM4 / COM5，不能视为 myCobot 已连接。
- 当前命令行未发现 `efinity` / `efx_run` / `efx_pgm` 在 PATH 中，Efinity CLI 路径需要 Claude 首轮继续核验。

## 目标与非目标

### 目标

- 给 Claude / Codex 提供统一的本地 MCP 入口，避免散乱地手写 PowerShell、串口脚本和烧录命令。
- 自动枚举并区分开发板串口、CP210x/myCobot 串口和蓝牙虚拟串口。
- 自动检查 Efinity 工程、构建产物、bitstream 路径、烧录工具路径和关键日志。
- 通过安全门控支持 myCobot 280 的开发期标定、只读状态读取、RGB 灯板测试、极小幅动作测试和日志归档。
- 生成 Claude/Codex Review Packet，便于每次烧录、动作测试、FPGA-to-机械臂联调都有可复核证据。

### 非目标

- 不把 MCP 作为比赛现场正式控制程序。
- 不让 PC 端 `pymycobot` 取代板上 CPU 的动作状态机。
- 不把 myCobot 协议、点位序列和异常处理写回纯 RTL 状态机。
- 不在未确认电平、接线和协议前把 FPGA 管脚直接接入机械臂控制线。
- 不对 `赛方提供材料/`、安装包、驱动原件、Efinity 补丁和原始压缩包做写入修改。

## 推荐技术栈

- 语言：Python。
- MCP 框架：Python MCP SDK / FastMCP 风格实现。
- 传输：stdio。本项目是本机单用户开发工具，不需要远程 HTTP 服务。
- 核心依赖：`mcp`、`pydantic`、`pyserial`。
- 可选依赖：`pymycobot`，只在实际连接 myCobot 280 时启用。
- 服务名：`fpga_robot_mcp`。
- 工具命名：统一使用前缀 `fpga_robot_`、`efinity_`、`board_`、`mycobot280_`。
- 建议落点：`final_project/tools/mcp/fpga_robot_mcp/`。

建议目录：

```text
final_project/tools/mcp/fpga_robot_mcp/
  README.md
  pyproject.toml
  src/fpga_robot_mcp/
    __init__.py
    server.py
    config.py
    safety.py
    serial_probe.py
    efinity_tools.py
    board_tools.py
    mycobot_tools.py
    review_packet.py
  configs/
    fpga_robot.local.example.json
  tests/
    test_serial_probe.py
    test_safety.py
    test_config.py
  logs/
    .gitkeep
```

本机私有配置文件 `fpga_robot.local.json` 不应提交；只提交 `fpga_robot.local.example.json`。

## MCP 工具设计

所有工具必须返回结构化 JSON，同时提供适合人读的 Markdown 摘要。列表类工具必须支持 `limit` 和 `offset`，默认限制在 20 到 50 条。错误消息必须说明下一步怎么修。

### 项目与环境工具

| 工具名 | 读写 | 用途 |
|---|---:|---|
| `fpga_robot_status` | 只读 | 汇总 Python、依赖、Efinity 路径、工程路径、串口候选、机械臂安全配置。 |
| `fpga_robot_get_config` | 只读 | 返回当前 MCP 配置、路径解析结果和安全门控状态，隐藏本机私密字段。 |
| `fpga_robot_write_review_packet` | 写文件 | 按模板生成一次 Claude/Codex 交接包，写入 `final_project/docs/review_packets/`。 |

### Efinity / 烧录工具

| 工具名 | 读写 | 用途 |
|---|---:|---|
| `efinity_locate_toolchain` | 只读 | 搜索 Efinity 安装路径、CLI 可执行文件、版本信息。 |
| `efinity_check_project` | 只读 | 校验 `mem_test.xml`、`constrain.sdc`、器件型号、顶层模块和输出目录。 |
| `efinity_list_artifacts` | 只读 | 列出 bitstream、programming file、report、log，返回时间戳和大小。 |
| `efinity_run_build` | 写构建产物 | 运行综合/布局布线流程。必须支持 `dry_run=true`。 |
| `efinity_program_bitstream` | 硬件副作用 | 调用 Efinity 烧录工具。必须要求 `dry_run=false`、目标 bitstream、线缆/设备摘要、确认 token。 |
| `efinity_collect_logs` | 只读/写归档 | 收集构建和烧录日志到 `final_project/docs/evidence/`。 |

Claude 首轮必须先确认 Efinity CLI 的真实命令名和参数。当前不能把 `efx_run`、`efx_pgm` 等名称写死为已可用命令。

### 开发板 / CPU / 串口工具

| 工具名 | 读写 | 用途 |
|---|---:|---|
| `board_list_uart_candidates` | 只读 | 枚举 COM 口，按 CP210x、JTAG UART、Type-C UART、蓝牙虚拟串口分组。 |
| `board_check_interface_contract` | 只读 | 检查 `final_project/integration/` 中 FPGA-CPU-机械臂接口契约是否存在。 |
| `board_generate_uart_test_plan` | 写文档 | 生成板上 CPU 到 myCobot 串口桥接测试计划，不直接操作硬件。 |
| `board_collect_cpu_logs` | 只读/写归档 | 收集 CPU 串口日志，过滤敏感路径，写入证据目录。 |

开发板工具只负责发现、校验和日志采集。真正的 CPU 固件编译、烧录和 UART 协议联调应另行进入 Claude/Codex 审查门。

### myCobot 280 工具

| 工具名 | 读写 | 用途 |
|---|---:|---|
| `mycobot280_check_env` | 只读 | 检查 Python、`pyserial`、`pymycobot`、CP210x 驱动、COM 口。 |
| `mycobot280_list_ports` | 只读 | 列出串口并标注是否像 CP210x/myCobot 候选。 |
| `mycobot280_validate_connection` | 只读 | 在用户确认已连接后尝试打开串口并做只读握手/状态读取。 |
| `mycobot280_read_angles` | 只读 | 读取关节角度，失败时返回端口、波特率、驱动、供电排查建议。 |
| `mycobot280_read_coords` | 只读 | 读取坐标，只允许在已通过连接校验后执行。 |
| `mycobot280_set_rgb` | 低风险动作 | 设置 RGB 灯板，可作为第一类非运动测试；仍需确认 token。 |
| `mycobot280_plan_motion` | 只读 | 根据目标动作生成摘要、角度范围、速度、风险提示，不执行。 |
| `mycobot280_execute_motion` | 运动副作用 | 执行极小幅关节/坐标动作。必须要求确认 token、速度上限、动作边界和急停确认。 |
| `mycobot280_control_gripper` | 运动副作用 | 控制夹爪。必须要求确认 token 和速度上限。 |
| `mycobot280_stop` | 安全动作 | 发送停止/释放相关命令，允许高优先级调用并记录日志。 |
| `mycobot280_export_session_log` | 写归档 | 导出本次连接、动作摘要、返回值和错误记录。 |

## 安全门控规则

MCP 实现必须把工具分为三类：

1. 只读工具：默认允许，例如路径检查、串口枚举、工程解析、日志读取。
2. 写文件/构建工具：默认允许 dry-run；实际写构建产物前需要明确工程路径和输出路径。
3. 硬件副作用工具：烧录、机械臂动作、夹爪控制都必须显式确认。

硬件副作用工具必须满足：

- `dry_run=false`。
- 用户在本轮对话明确确认动作或烧录目标。
- MCP 配置中 `allow_hardware_actions=true`。
- 工具输入中包含确认字段，例如 `confirm_token`。
- 工具返回执行前摘要，并在日志中记录端口、波特率、目标文件、动作参数、速度上限、时间戳。
- myCobot 动作默认速度上限不超过资料调试案例中的安全范围；初次实测应使用极小幅动作。
- 如果串口只枚举到蓝牙 COM，必须拒绝把它当作 myCobot 端口。
- 如果端口不是 CP210x / Silicon Labs / 用户明确指定端口，必须要求再次确认。

示例确认 token 规则：

```text
confirm_token = "I_CONFIRM_MYCOBOT280_SAFE_<YYYYMMDD>"
confirm_token = "I_CONFIRM_EFINITY_PROGRAM_<YYYYMMDD>"
```

token 只用于降低误触发风险，不替代现场安全检查。

## 配置示例

`configs/fpga_robot.local.example.json`：

```json
{
  "project_root": "D:/第十届集创赛-雄芯院材料",
  "efinity": {
    "install_dir": null,
    "project_xml": "赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/mem_test.xml",
    "device": "TJ375N529",
    "timing_model": "I3"
  },
  "board": {
    "name": "WZZY_FPGA / TJ375N529",
    "cpu_route": "board_cpu_qcrv32",
    "uart_candidates": ["JTAG-IF UART", "Type-C UART", "UART2 TO Peripherals"],
    "gpio_notes": "J13/J15 are 3.3V GPIO candidates; do not connect to myCobot before voltage/protocol confirmation."
  },
  "mycobot280": {
    "baudrate": 1000000,
    "preferred_vid": "10C4",
    "model": "MyCobot",
    "allow_motion": false,
    "max_speed": 30,
    "default_port": null
  },
  "safety": {
    "allow_hardware_actions": false,
    "require_confirm_token": true,
    "log_dir": "final_project/docs/evidence/mcp_sessions"
  }
}
```

## Claude 实施顺序

### Phase 0：只读核验

- 读取 `AGENTS.md`、`CLAUDE.md`、`.claude/commands/robot-mycobot280.md` 和本方案。
- 核验 `方案评审/001_项目文件架构方案/项目文件架构方案_v1.0_定稿版.md`，确认 MCP 落点不破坏 `final_project/` 边界。
- 重新枚举 Python、串口、`pymycobot`、Efinity CLI。
- 输出首轮 Review Packet，不做安装、烧录或机械臂动作。

### Phase 1：MCP 骨架

- 在 `final_project/tools/mcp/fpga_robot_mcp/` 创建 Python 包。
- 实现配置加载、路径归一化、结构化错误、stderr 日志。
- 实现只读工具：`fpga_robot_status`、`efinity_locate_toolchain`、`efinity_check_project`、`board_list_uart_candidates`、`mycobot280_check_env`。
- 使用 MCP Inspector 或等价方式验证工具能被 Claude / Codex 调起。

### Phase 2：Efinity 构建与烧录

- 先确认本机 Efinity CLI 路径和命令参数。
- 实现 `efinity_list_artifacts`、`efinity_run_build(dry_run=true)`、`efinity_collect_logs`。
- 在 dry-run 和只读日志归档通过后，再实现 `efinity_program_bitstream`。
- 烧录工具必须默认拒绝执行，直到配置和 token 同时允许。

### Phase 3：myCobot 280 只读与低风险联调

- 安装或选择兼容的 Python 环境，优先不污染全局 Python。
- 实现 `mycobot280_list_ports`、`mycobot280_validate_connection`、`mycobot280_read_angles`。
- 只有在用户确认机械臂安全姿态、供电、急停/断电方式后，才开放 `mycobot280_set_rgb`。
- 关节动作、夹爪和快速移动继续保持禁用，直到 Codex 审查通过。

### Phase 4：板上 CPU 接管路线

- MCP 只生成和校验 CPU 侧 myCobot 协议测试计划、点位表、动作日志和接口契约。
- 实际 CPU 固件仍按正式工程流程开发，并通过 Efinity/CPU 工具链烧录。
- MCP 不直接生成未经审查的动作状态机代码；涉及 `arm_controller`、点位表、互锁、超时处理时必须进入 Codex Gate。

### Phase 5：评测与定稿

- 为 MCP 编写只读单元测试、串口枚举测试、安全门控测试。
- 形成 10 个只读评测问题，验证 Claude 是否能用 MCP 找到正确答案。
- 生成 `Claude审核意见.md` 和 `机械臂MCP构建实施方案_v1.0_定稿版.md`。

## 最低验收标准

- MCP 服务可通过 Claude / Codex 启动并列出工具。
- `fpga_robot_status` 能返回项目根目录、Efinity 状态、Python 依赖、串口候选和 myCobot 安全状态。
- 未连接 myCobot 时，工具能明确说明“只看到蓝牙 COM，不应当作机械臂连接”。
- `pymycobot` 未安装时，工具不崩溃，并给出安装建议。
- 未配置 `allow_hardware_actions=true` 时，烧录、机械臂动作、夹爪控制全部拒绝执行。
- Efinity CLI 未确认时，构建/烧录工具只返回核验建议，不伪造成功。
- 每次构建、烧录、串口连接或机械臂动作都能生成可归档日志。
- Claude 交给 Codex 的 Review Packet 能包含硬件状态、端口、波特率、动作/烧录目标、验证结果和未验证风险。

## 只读评测问题草案

这些问题用于 MCP 完成后验证 Claude 能否有效使用工具，全部不需要硬件副作用：

1. 当前项目使用的 Efinity 版本、FPGA 器件和主工程 XML 分别是什么？
2. 当前机器是否安装了 `pymycobot`？如果没有，推荐安装命令是什么？
3. 当前可见 COM 口中哪些可能是 CP210x/myCobot，哪些只是蓝牙虚拟串口？
4. myCobot 280 的默认波特率是多少？
5. myBlockly 初始化时应该选择哪个机械臂型号？
6. 当前方案中 PC 端 `pymycobot` 是正式比赛闭环还是开发期工具？
7. FPGA-to-myCobot 联动前必须先确认哪些电气和协议条件？
8. 哪些 MCP 工具属于硬件副作用工具，为什么需要确认 token？
9. 如果 Efinity CLI 不在 PATH 中，MCP 应该给出什么下一步建议？
10. 一次机械臂动作测试交给 Codex 审查时，Review Packet 至少应包含哪些字段？

## Claude 审核重点

Claude 审核时请优先判断：

- `fpga_robot_mcp` 是否应拆成 `efinity_mcp` 和 `mycobot280_mcp` 两个服务，还是单服务更利于比赛开发。
- Efinity 2025.2 的真实 CLI 命令、烧录参数和日志路径是什么。
- `pymycobot` 对当前 Python 3.13 是否兼容；是否应为 MCP 固定 Python 3.10 虚拟环境。
- myCobot 文档中 USB-TTL 的 TX/RX 接线写法是否需要现场复核后再进入工具默认提示。
- 开发板 UART 到 myCobot 控制器之间是否存在电平、隔离、供电地线、协议方向和波特率风险。
- `efinity_program_bitstream`、`mycobot280_execute_motion`、`mycobot280_control_gripper` 的默认拒绝策略是否足够严格。

## 需要 Codex 复核的门槛

以下情况必须交 Codex 审查后再继续：

- MCP 实现首次加入烧录工具。
- MCP 实现首次加入任何机械臂运动或夹爪工具。
- Claude 试图把 PC 端 MCP / `pymycobot` 放入比赛正式闭环。
- Claude 试图让 FPGA RTL 直接封装 myCobot 协议和动作状态机。
- 开发板 UART/GPIO 与机械臂控制线发生实际接线。
- Efinity 构建 warning 被判断为可忽略。
- 连续两轮串口、烧录或动作调试失败。

## 初始 Review Packet 模板

```md
# MCP / myCobot / Efinity Review Packet

## 任务目标

## 本轮动作类型
- 只读检查 / 构建 / 烧录 / 串口连接 / 机械臂动作：

## 项目与工具状态
- 项目根目录：
- Efinity 路径与版本：
- 主工程 XML：
- bitstream / outflow：

## 开发板与 CPU 状态
- 板卡：
- CPU / QCRV32 相关路径：
- UART 候选：
- FPGA-to-机械臂接线是否涉及：

## myCobot 280 状态
- 端口：
- CP210x / VID_10C4：
- 波特率：
- `pymycobot`：
- 是否执行动作：
- 安全确认：

## 已运行命令或 MCP 工具
- 工具：
- 参数：
- 结果：
- 日志：

## 风险与未验证项

## 希望 Codex 判断的问题
```
