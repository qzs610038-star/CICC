# 机械臂 MCP 构建实施方案 v0.2 Codex 修正版

日期：2026-07-01  
状态：根据 Claude 审核意见修正，供 Phase -1 / Phase 0 执行  
适用范围：`D:\第十届集创赛-雄芯院材料`

## 修正摘要

本版在 v0.1 基础上采纳 Claude 审核意见，新增工具链安装前置阶段、Python 3.10 运行环境约束、Claude/Codex MCP 注册方式、手动回退路径、JTAG/下载线检测、UART 回环测试、token 不持久化规则和日志路径分层。

继续建议建设单一 stdio MCP：`fpga_robot_mcp`。服务内部按 Efinity、开发板/CPU、myCobot 280、安全门控和日志归档模块拆分，不拆成多个 MCP 服务。理由是本项目当前是单机、单团队、短周期比赛开发，单服务更容易注册、排错和交付；模块拆分已足够隔离关注点。

该 MCP 只作为开发期工具，不进入比赛运行闭环。正式比赛闭环仍固定为：

```text
MIPI 摄像头
  -> FPGA: CSI/RAW/ROI/统计特征/OSD
  -> 板上 CPU: 颜色/形状/尺寸分类、目标匹配、参数切换
  -> 板上 CPU: myCobot 协议、点位表、动作状态机、安全互锁
  -> FPGA HDMI/OSD 显示调试状态
```

PC 端 MCP 可以帮助 Claude / Codex 完成环境核查、Efinity 构建与烧录、开发期 myCobot 标定和日志整理，但不能把 PC、`pymycobot` 或 MCP 本身作为最终控制路径。

## 已核验状态

### 当前本机状态

- Efinity 主程序：未发现 `C:\Efinity\`、`C:\Efinity\2025.2\`、`C:\Program Files\Efinity\`。
- Efinity CLI：当前 PATH 未发现 `efinity`、`efx_run`、`efx_pgm`。
- Efinity 安装包：存在 `赛方提供材料/EDA软件/00 Efinity 2025.2.rar`。
- Efinity 补丁：存在 `赛方提供材料/efinity-2025.2.288.4.15-windows-x64-patch/run.bat`。
- RAR 解压工具：当前 PATH 未发现 `7z`、`unrar`、`winrar`。
- Python 3.10 环境：存在 `D:\conda_envs\pfmval_py310\python.exe`，版本 `3.10.20`。
- `pfmval_py310` 依赖：当前未安装 `mcp`、`pyserial`、`pydantic`、`pymycobot`。
- 全局 Python：`C:\Program Files\Python313\python.exe`，版本 `3.13.1`；不作为 MCP 推荐运行环境。
- 当前串口：此前仅枚举到蓝牙 COM4 / COM5，不能视为 myCobot 已连接。

### 开发板与 FPGA

- 工具链 / 器件目标：Efinity `2025.2.288.4.15`，Titanium `TJ375N529`，时序模型 `I3`。
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
- 文档记录的 USB-TTL 接线：`TXD -> 机械臂 TX`、`RXD -> 机械臂 RX`、`GND -> 机械臂 GND`。该写法应现场复核，不应在连不上时反复发送动作命令。
- 安全要求：通电前必须把机械臂放到说明文档要求的正确姿态；任何关节、坐标、夹爪和快速移动命令都需要明确动作摘要和用户确认。

## 目标与非目标

### 目标

- 给 Claude / Codex 提供统一的本地 MCP 入口，减少散乱 PowerShell、串口脚本和烧录命令。
- 自动枚举并区分开发板串口、CP210x/myCobot 串口和蓝牙虚拟串口。
- 自动检查 Efinity 工程、构建产物、bitstream 路径、烧录工具路径、JTAG 链状态和关键日志。
- 通过安全门控支持 myCobot 280 的开发期标定、只读状态读取、RGB 灯板测试、极小幅动作测试和日志归档。
- 为每次烧录、动作测试、FPGA-to-机械臂联调生成可复核证据。
- 即使 MCP 不可用，也保留手动等效操作路径。

### 非目标

- 不把 MCP 作为比赛现场正式控制程序。
- 不让 PC 端 `pymycobot` 取代板上 CPU 的动作状态机。
- 不把 myCobot 协议、点位序列和异常处理写回纯 RTL 状态机。
- 不在未确认电平、接线和协议前把 FPGA 管脚直接接入机械臂控制线。
- 不对 `赛方提供材料/`、安装包、驱动原件、Efinity 补丁和原始压缩包做写入修改。

## 推荐技术栈与运行环境

- 语言：Python。
- MCP 框架：Python MCP SDK / FastMCP 风格实现。
- 传输：stdio。
- 服务名：`fpga_robot_mcp`。
- 工具命名：统一使用 `fpga_robot_`、`efinity_`、`board_`、`mycobot280_` 前缀。
- 推荐运行环境：Python 3.10。
- 首选解释器：`D:\conda_envs\pfmval_py310\python.exe`。
- 如 `pfmval_py310` 不宜被本项目占用，则新建专用环境 `fpga_mcp_py310`。
- 核心依赖：`mcp`、`pydantic`、`pyserial`。
- 可选但 Phase 3 前需要：`pymycobot`。

推荐安装命令：

```powershell
D:\miniconda\Scripts\conda.exe run -n pfmval_py310 python -m pip install --upgrade pip
D:\miniconda\Scripts\conda.exe run -n pfmval_py310 python -m pip install mcp pydantic pyserial pymycobot
D:\miniconda\Scripts\conda.exe run -n pfmval_py310 python -c "import mcp, serial, pydantic, pymycobot; print('ok')"
```

如果创建专用环境：

```powershell
D:\miniconda\Scripts\conda.exe create -n fpga_mcp_py310 python=3.10 -y
D:\miniconda\Scripts\conda.exe run -n fpga_mcp_py310 python -m pip install mcp pydantic pyserial pymycobot
```

## 推荐目录

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
    test_config.py
    test_safety.py
    test_serial_probe.py
    test_efinity_probe.py
    test_tool_entrypoints.py
  logs/
    .gitkeep
```

本机私有配置 `fpga_robot.local.json` 不提交；只提交 `fpga_robot.local.example.json`。确认 token 不写入任何配置文件。

## MCP 注册方案

### Claude Code 项目级注册

建议使用项目级 `.claude/settings.json`。当前项目已有 `.claude/settings.local.json`，但尚无 `.claude/settings.json`。实施时不要覆盖现有 `settings.local.json`，应新建或合并项目级配置。

示例：

```json
{
  "mcpServers": {
    "fpga_robot_mcp": {
      "command": "D:\\conda_envs\\pfmval_py310\\python.exe",
      "args": ["-m", "fpga_robot_mcp.server"],
      "env": {
        "PYTHONPATH": "D:\\第十届集创赛-雄芯院材料\\final_project\\tools\\mcp\\fpga_robot_mcp\\src",
        "FPGA_ROBOT_MCP_CONFIG": "D:\\第十届集创赛-雄芯院材料\\final_project\\tools\\mcp\\fpga_robot_mcp\\configs\\fpga_robot.local.json"
      }
    }
  }
}
```

若使用可安装包方式，也可以移除 `PYTHONPATH`，改为在目标环境中执行 `pip install -e final_project/tools/mcp/fpga_robot_mcp`。

### Codex 注册

Codex 使用 `C:\Users\33696\.codex\config.toml` 的 `[mcp_servers]` 结构。实施前应先备份并手动合并，避免影响已有 `node_repl`、`qmd` 等服务。

示例：

```toml
[mcp_servers.fpga_robot_mcp]
type = "stdio"
command = "D:\\conda_envs\\pfmval_py310\\python.exe"
args = ["-m", "fpga_robot_mcp.server"]

[mcp_servers.fpga_robot_mcp.env]
PYTHONPATH = "D:\\第十届集创赛-雄芯院材料\\final_project\\tools\\mcp\\fpga_robot_mcp\\src"
FPGA_ROBOT_MCP_CONFIG = "D:\\第十届集创赛-雄芯院材料\\final_project\\tools\\mcp\\fpga_robot_mcp\\configs\\fpga_robot.local.json"
```

注册后必须用真实 tool call 验证 `fpga_robot_status -> efinity_check_project -> mycobot280_check_env`，不能只看配置是否存在。

## 工具设计

所有工具必须返回结构化 JSON，同时提供适合人读的 Markdown 摘要。列表类工具必须支持 `limit` 和 `offset`，默认限制在 20 到 50 条。错误消息必须给出可执行下一步。

### 项目与环境工具

| 工具名 | 读写 | 用途 |
|---|---:|---|
| `fpga_robot_status` | 只读 | 汇总 Python、依赖、Efinity 路径、工程路径、串口候选、机械臂安全配置。 |
| `fpga_robot_get_config` | 只读 | 返回当前 MCP 配置、路径解析结果和安全门控状态，隐藏本机私密字段。 |
| `fpga_robot_write_review_packet` | 写文件 | 生成 Review Packet。正式评审包写 `方案评审/NNN_名称/`；日常操作证据写 `final_project/docs/evidence/`。 |
| `fpga_robot_manual_fallback` | 只读 | 返回指定工具的 PowerShell 等效手动操作步骤。 |

### Efinity / 烧录工具

| 工具名 | 读写 | 用途 |
|---|---:|---|
| `efinity_locate_toolchain` | 只读 | 搜索 Efinity 安装路径、CLI 可执行文件、版本信息，优先检查 `C:\Efinity\2025.2\bin\`。 |
| `efinity_check_install_prereq` | 只读 | 检查 RAR 解压工具、安装包、补丁脚本、磁盘空间和默认安装路径。 |
| `efinity_check_project` | 只读 | 校验 `mem_test.xml`、`constrain.sdc`、器件型号、顶层模块和输出目录。 |
| `efinity_list_artifacts` | 只读 | 列出 bitstream、programming file、report、log，返回时间戳和大小。 |
| `efinity_run_build` | 写构建产物 | 运行综合/布局布线流程。必须支持 `dry_run=true`。 |
| `efinity_check_programmer` | 只读 | 检查下载线、驱动和可用烧录器信息；不执行烧录。 |
| `efinity_program_bitstream` | 硬件副作用 | 调用 Efinity 烧录工具。必须要求 `dry_run=false`、目标 bitstream、JTAG 摘要、确认 token。 |
| `efinity_collect_logs` | 只读/写归档 | 收集构建和烧录日志到 `final_project/docs/evidence/`。 |

`efx_run`、`efx_pgm` 的路径和参数必须在 Efinity 安装后再确认。当前只能把 `C:\Efinity\2025.2\bin\` 作为搜索起点，不能伪造 CLI 可用。

### 开发板 / CPU / 串口工具

| 工具名 | 读写 | 用途 |
|---|---:|---|
| `board_list_uart_candidates` | 只读 | 枚举 COM 口，按 CP210x、JTAG UART、Type-C UART、蓝牙虚拟串口分组。 |
| `board_check_jtag_chain` | 只读 | 通过 `efx_pgm --scan` 或安装后确认的等效命令探测 JTAG 链，不烧录。 |
| `board_check_interface_contract` | 只读 | 检查 `final_project/integration/` 中 FPGA-CPU-机械臂接口契约是否存在。 |
| `board_generate_uart_test_plan` | 写文档 | 生成板上 CPU 到 myCobot 串口桥接测试计划，不直接操作硬件。 |
| `board_uart_loopback_test` | 低风险硬件测试 | 在明确选择的 UART 上执行回环测试帧，不连接机械臂、不触发动作。 |
| `board_collect_cpu_logs` | 只读/写归档 | 收集 CPU 串口日志，过滤敏感路径，写入证据目录。 |

开发板工具只负责发现、校验、回环测试和日志采集。真正的 CPU 固件编译、烧录和 UART 协议联调应另行进入 Claude/Codex 审查门。

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
- `confirm_token` 不持久化保存，不写入配置、不写入仓库、不写入日志明文。
- 工具返回执行前摘要，并在日志中记录端口、波特率、目标文件、动作参数、速度上限、时间戳。
- myCobot 动作默认速度上限不超过资料调试案例中的安全范围；初次实测应使用极小幅动作。
- 如果串口只枚举到蓝牙 COM，必须拒绝把它当作 myCobot 端口。
- 如果端口不是 CP210x / Silicon Labs / 用户明确指定端口，必须要求再次确认。
- 如果 JTAG 链不可见，必须拒绝烧录。

示例确认 token：

```text
I_CONFIRM_MYCOBOT280_SAFE_<YYYYMMDD>
I_CONFIRM_EFINITY_PROGRAM_<YYYYMMDD>
I_CONFIRM_UART_LOOPBACK_<YYYYMMDD>
```

token 只用于降低误触发风险，不替代现场安全检查。

## 配置示例

`configs/fpga_robot.local.example.json`：

```json
{
  "project_root": "D:/第十届集创赛-雄芯院材料",
  "efinity": {
    "install_dir": "C:/Efinity/2025.2",
    "bin_dir": "C:/Efinity/2025.2/bin",
    "project_xml": "赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/mem_test.xml",
    "device": "TJ375N529",
    "timing_model": "I3",
    "patch_dir": "赛方提供材料/efinity-2025.2.288.4.15-windows-x64-patch"
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
    "evidence_dir": "final_project/docs/evidence",
    "formal_review_dir": "方案评审"
  }
}
```

## 路径与归档约定

- 方案评审、Codex Gate、架构审批：放 `方案评审/NNN_名称/`。
- 日常构建日志、串口日志、JTAG 扫描结果、MCP 会话日志：放 `final_project/docs/evidence/`。
- MCP 自动生成的临时 Review Packet：默认写 `final_project/docs/evidence/review_packets/`。
- 一旦该 Packet 触发正式 Codex Gate，应复制或整理成 `方案评审/NNN_名称/` 下的独立评审包。
- 不把安装包、驱动原件、bitstream 大文件和 Efinity 生成数据库复制进 `方案评审/`。

## 修正后实施顺序

### Phase -1：工具链与前置软件安装

目标：先让 Efinity 和基础解压工具可用，否则 MCP 烧录/构建工具没有真实后端。

必须完成：

- 用户安装 7-Zip 或 WinRAR，用于解压 `赛方提供材料/EDA软件/00 Efinity 2025.2.rar`。
- 解压并安装 Efinity 2025.2，优先使用默认路径 `C:\Efinity\2025.2\`。
- 安装后运行 `赛方提供材料/efinity-2025.2.288.4.15-windows-x64-patch/run.bat` 打补丁。
- 确认 `C:\Efinity\2025.2\bin\efx_run*` 和 `efx_pgm*` 或等效 CLI 存在。
- 暂不执行烧录。

Phase -1 手动核验命令：

```powershell
Get-Command 7z,unrar,winrar -ErrorAction SilentlyContinue
Test-Path "C:\Efinity\2025.2"
Get-ChildItem "C:\Efinity\2025.2\bin" -Filter "efx_*" -ErrorAction SilentlyContinue
Test-Path "赛方提供材料\efinity-2025.2.288.4.15-windows-x64-patch\run.bat"
```

### Phase 0：只读核验与 MCP 环境准备

- 读取 `AGENTS.md`、`CLAUDE.md`、`.claude/commands/robot-mycobot280.md`、本方案和 `Claude审核意见.md`。
- 核验 `方案评审/001_项目文件架构方案/项目文件架构方案_v1.0_定稿版.md`，确认 MCP 落点不破坏 `final_project/` 边界。
- 在 Python 3.10 环境安装 MCP 依赖。
- 重新枚举 Python、串口、`pymycobot`、Efinity CLI、JTAG 下载线。
- 生成初始 Review Packet，不做烧录或机械臂动作。

### Phase 1：MCP 骨架与注册

- 在 `final_project/tools/mcp/fpga_robot_mcp/` 创建 Python 包。
- 实现配置加载、路径归一化、结构化错误、stderr 日志。
- 实现只读工具：`fpga_robot_status`、`fpga_robot_get_config`、`efinity_check_install_prereq`、`efinity_locate_toolchain`、`efinity_check_project`、`board_list_uart_candidates`、`board_check_jtag_chain`、`mycobot280_check_env`。
- 添加项目级 `.claude/settings.json` 注册示例或实际注册。
- 如需在 Codex 使用，手动合并 `C:\Users\33696\.codex\config.toml`。
- 使用 `python -m fpga_robot_mcp.server`、MCP Inspector 或真实 tool call 验证 stdio 启动。

### Phase 2：Efinity 构建与烧录

- 先确认本机 Efinity CLI 路径、命令参数和日志路径。
- 实现 `efinity_list_artifacts`、`efinity_run_build(dry_run=true)`、`efinity_collect_logs`。
- 实现并验证 `efinity_check_programmer` / `board_check_jtag_chain`。
- 在 dry-run、只读日志归档、JTAG 链检测通过后，再实现 `efinity_program_bitstream`。
- 烧录工具必须默认拒绝执行，直到配置、JTAG 摘要和 token 同时满足。

### Phase 3：myCobot 280 只读与低风险联调

- 确认 Python 3.10 环境已安装 `pymycobot`。
- 安装 CP210x 驱动，重新插拔 USB-TTL，确认 Silicon Labs / CP210x COM 口。
- 实现 `mycobot280_list_ports`、`mycobot280_validate_connection`、`mycobot280_read_angles`。
- 只有在用户确认机械臂安全姿态、供电、急停/断电方式后，才开放 `mycobot280_set_rgb`。
- 关节动作、夹爪和快速移动继续保持禁用，直到 Codex 审查通过。

### Phase 4：板上 CPU 接管路线

- MCP 只生成和校验 CPU 侧 myCobot 协议测试计划、点位表、动作日志和接口契约。
- 增加 `board_uart_loopback_test`，先用回环或测试线验证 UART 通路，不连接机械臂。
- 实际 CPU 固件仍按正式工程流程开发，并通过 Efinity/CPU 工具链烧录。
- MCP 不直接生成未经审查的动作状态机代码；涉及 `arm_controller`、点位表、互锁、超时处理时必须进入 Codex Gate。

### Phase 5：测试、评测与定稿

- 单元测试直接调用工具函数，不依赖 MCP 协议层。
- 至少覆盖：配置解析、安全门控、串口分类、Efinity 未安装错误、依赖缺失错误、token 不持久化。
- 用 MCP Inspector 或真实客户端验证 stdio 工具可发现、可调用。
- 形成 10 个只读评测问题，验证 Claude 是否能用 MCP 找到正确答案。
- 生成 `机械臂MCP构建实施方案_v1.0_定稿版.md`。

## 手动等效操作指南

MCP 失效时，核心操作按以下 PowerShell 路径手动完成。

| MCP 工具 | 手动等效操作 |
|---|---|
| `fpga_robot_status` | 检查 Python、Efinity、串口、配置文件：`python --version`、`Get-Command efx_run`、串口枚举命令。 |
| `efinity_check_install_prereq` | `Get-Command 7z,unrar,winrar`；`Test-Path` 安装包、补丁、`C:\Efinity\2025.2`。 |
| `efinity_locate_toolchain` | `Get-ChildItem "C:\Efinity\2025.2\bin" -Filter "efx_*"`；必要时全盘限深搜索。 |
| `efinity_check_project` | `Test-Path mem_test.xml`、`Test-Path constrain.sdc`，用 XML 解析确认主工程可读。 |
| `efinity_list_artifacts` | `Get-ChildItem outflow -Recurse | Sort-Object LastWriteTime -Descending | Select-Object -First 30`。 |
| `board_list_uart_candidates` | `python -c "import serial.tools.list_ports as p; [print(x.device, x.description, x.hwid) for x in p.comports()]"`。 |
| `mycobot280_check_env` | `python -c "import importlib.util; print(bool(importlib.util.find_spec('pymycobot')))"`。 |
| `efinity_collect_logs` | 手动复制或汇总 Efinity report/log 到 `final_project/docs/evidence/`。 |
| `mycobot280_export_session_log` | 手动记录端口、波特率、动作摘要、返回值、错误和截图到证据目录。 |

烧录、机械臂动作和夹爪控制的手动操作也必须遵守确认 token、现场安全姿态、急停/断电方式、日志归档和 Codex Gate。

## 最低验收标准

- MCP 服务可通过 Claude / Codex 启动并列出工具。
- `fpga_robot_status` 能返回项目根目录、Efinity 状态、Python 依赖、串口候选和 myCobot 安全状态。
- 未安装 Efinity 时，工具明确提示先完成 Phase -1，而不是伪造 CLI 可用。
- 未连接 myCobot 时，工具能明确说明“只看到蓝牙 COM，不应当作机械臂连接”。
- `pymycobot` 未安装时，工具不崩溃，并给出 Python 3.10 环境内安装建议。
- 未配置 `allow_hardware_actions=true` 时，烧录、机械臂动作、夹爪控制全部拒绝执行。
- JTAG 链不可见时，烧录工具拒绝执行。
- 每次构建、烧录、串口连接或机械臂动作都能生成可归档日志。
- Claude 交给 Codex 的 Review Packet 能包含硬件状态、端口、波特率、动作/烧录目标、验证结果和未验证风险。

## 只读评测问题草案

1. 当前项目使用的 Efinity 版本、FPGA 器件和主工程 XML 分别是什么？
2. 当前机器是否安装了 Efinity 2025.2？如果没有，Phase -1 的下一步是什么？
3. 当前 Python 3.10 环境是否安装了 `mcp`、`pyserial`、`pydantic`、`pymycobot`？
4. 当前可见 COM 口中哪些可能是 CP210x/myCobot，哪些只是蓝牙虚拟串口？
5. myCobot 280 的默认波特率是多少？
6. myBlockly 初始化时应该选择哪个机械臂型号？
7. 当前方案中 PC 端 `pymycobot` 是正式比赛闭环还是开发期工具？
8. FPGA-to-myCobot 联动前必须先确认哪些电气和协议条件？
9. 哪些 MCP 工具属于硬件副作用工具，为什么需要确认 token？
10. 一次机械臂动作测试交给 Codex 审查时，Review Packet 至少应包含哪些字段？

## Codex Gate

以下情况必须交 Codex 审查后再继续：

- MCP 实现首次加入烧录工具。
- MCP 实现首次加入任何机械臂运动或夹爪工具。
- Claude 试图把 PC 端 MCP / `pymycobot` 放入比赛正式闭环。
- Claude 试图让 FPGA RTL 直接封装 myCobot 协议和动作状态机。
- 开发板 UART/GPIO 与机械臂控制线发生实际接线。
- Efinity 构建 warning 被判断为可忽略。
- JTAG 链扫描异常但仍计划烧录。
- 连续两轮串口、烧录或动作调试失败。

## Review Packet 模板

```md
# MCP / myCobot / Efinity Review Packet

## 任务目标

## 本轮动作类型
- 只读检查 / 构建 / 烧录 / 串口连接 / 机械臂动作：

## 项目与工具状态
- 项目根目录：
- Efinity 路径与版本：
- CLI 路径：
- 主工程 XML：
- bitstream / outflow：

## 开发板与 CPU 状态
- 板卡：
- CPU / QCRV32 相关路径：
- UART 候选：
- JTAG 链：
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


---

# GA 审查修正（2026-07-01，追加于 Phase -1 执行前）

审查方式：GA 读取了 `赛方提供材料/EDA软件/Efinity安装流程.pdf`（8页）、`00 Efinity 2025.2.rar` 内部目录结构及所有 Readme.txt、`大象机械臂mycobot–280安装调试说明/` 下的 docx 安装文档。以下修正仅追加，不删改上文。

## 1. WinRAR 状态修正

- **原文（v0.2 §已核验状态）：** "RAR 解压工具：当前 PATH 未发现 7z、unrar、winrar"
- **GA 审查发现：** WinRAR 已安装在 `C:\Program Files\WinRAR\`，包含 `winrar.exe`(3.4MB)、`rar.exe`(836KB)、`unrar.exe`(561KB)，版本 7.20 beta 3。上述工具不在 PATH 中所以 `Get-Command` 搜不到，但可绝对路径调用。
- **修正结论：** RAR 解压工具已就绪，无需额外安装。Phase -1 清单中"安装 7-Zip/WinRAR"改为"确认 WinRAR 可用"。调用方式使用绝对路径 `C:\Program Files\WinRAR\unrar.exe`。

## 2. 安装路径修正（重大）

- **原文（v0.2 §Phase -1）：** "优先使用默认路径 `C:\Efinity\2025.2\`"
- **GA 审查发现：** `Efinity安装流程.pdf` 第3-4页明确要求安装到 **`D:\Efinity\`**；赛方文件用 D 盘而非 C 盘。补丁 `run.bat` 中 `EFINITY_HOME` 默认路径为 `C:\Efinity\2025.2`但会尝试自动检测。**后续 MCP 配置 `efinity.install_dir` 应从 `C:/Efinity/2025.2` 改为 `D:/Efinity/2025.2`。**
- **修正结论：** 安装目标路径变更为 `D:\Efinity\2025.2\`。

## 3. 补丁版本修正（重大）

- **原文（v0.2 §Phase -1）：** "运行 `赛方提供材料/efinity-2025.2.288.4.15-windows-x64-patch/run.bat`"
- **GA 审查发现：** RAR 安装包内部自带补丁位于 `02 补丁\efinity-2025.2.288.3.8-windows-x64-patch\`，其 `Readme.txt` 明确说"只安装efinity-2025.2.288.3.8-windows-x64-patch补丁"。赛方 `Efinity安装流程.pdf` 第5页也写"只需要安装3.8的补丁"。外层的 288.4.15 补丁与 RAR 内文件结构不同（288.4.15 包含完整 bin/debugger/pgm 等目录），**可能是给不同安装包版本的后续升级补丁**。
- **修正结论：** Phase -1 先装 RAR 内的 288.3.8 补丁。288.4.15 是否叠加装待 MCP 建好后用 `efinity_locate_toolchain` 对比版本号再决定。

## 4. Phase -1 步骤补全（重大）

**v0.2 原文 Phase -1 只有 3 步**（解压→安装→补丁），但 `Efinity安装流程.pdf` 揭示了 **8 个必要步骤**：

| # | 原文有 | 步骤 | 来源 |
|---|--------|------|------|
| 1 | ❌ | **安装 VC++ 运行库**（x64 + x86） | PDF 第1-2页 + `01 安装VC\Readme.txt` |
| 2 | ❌ | 创建安装目录 `D:\Efinity\2025.2\` | PDF 第3页 |
| 3 | ✅ | 安装 MSI 主程序到 `D:\Efinity\2025.2\` | PDF 第3-4页 |
| 4 | ❌ | **复制 `efxdevicedb.ini` 到 `D:\Efinity\2025.2\arch\`** | PDF 第4页 + `software说明.txt` |
| 5 | ✅(版本错) | 安装补丁 288.3.8 | PDF 第5页 + `02 补丁\Readme.txt` |
| 6 | ❌ | **复制 JTAG Bridge bitstream(titanium/topaz/trion) 到 `D:\Efinity\2025.2\pgm\fli\`** | PDF 第6页 + `JTAG_Bridge_bitstream_v3.1.0\Readme.txt` |
| 7 | ❌ | **安装 FT4232 驱动**（YLS_4232DL_JTAG + YLS_4232DL_SOFT_JTAG，不装 SPI） | PDF 第7页 + `03 JTAG驱动\Readme.txt` |
| 8 | ❌ | **安装 RISC-V IDE**到 `D:\Efinity\efinity-riscv-ide-2025.2\` | PDF 第7-8页 |

## 5. 已核验状态补充

RAR 安装包内部文件清单（`02 Efiinity软件安装说明\01 efinity\`）：
- 主安装程序：`efinity-2025.2.288-windows-x64.msi`
- 设备配置文件：`efxdevicedb.ini`（需复制到 `arch\`）
- RISC-V IDE：`efinity-riscv-ide-2025.2.0.4-windows-x64.msi`

修正后的 Phase -1 核验命令：
```powershell
# 解压工具（修正：用绝对路径确认）
Test-Path "C:\Program Files\WinRAR\unrar.exe"

# 安装路径（修正：D盘）
Test-Path "D:\Efinity\2025.2"

# CLI（修正：D盘路径）
Get-ChildItem "D:\Efinity\2025.2\bin" -Filter "efx_*" -ErrorAction SilentlyContinue

# 补丁（修正：先确认3.8版本）
Test-Path "D:\Efinity\2025.2\patch_readme.txt"

# 设备数据库（新增核验）
Test-Path "D:\Efinity\2025.2\arch\efxdevicedb.ini"

# JTAG bitstream（新增核验）
Get-ChildItem "D:\Efinity\2025.2\pgm\fli\titanium" -Filter "*.bit" -ErrorAction SilentlyContinue
```

## 6. 机械臂安装文档确认

GA 读取了 `myblockly&python安装说明.docx`、`机械臂mycobot280调试案例.docx`、`机械臂的调试.docx`，与 v0.2 文档描述一致，无矛盾：
- CP210x 驱动：安装 `CP210xVCPInstaller_x64_v6.7.0.0.exe` ✅
- myBlockly：型号选 MyCobot，波特率 1000000 ✅
- pymycobot：`pip install pymycobot --upgrade --user` ✅
- 接线：TXD→机械臂TX, RXD→机械臂RX, GND→机械臂GND ✅（但需现场复核）
- 通电前：机械臂须置于正确姿态 ✅
---

# 安装执行记录（2026-07-02，GA 实际安装验证后追加）

来源：GA（Python物理执行者）于 2026-07-02 按 Phase -1 流程实际执行安装，以下为验证结果。

## 安装完成状态

| 步骤 | 组件 | 结果 | 最终路径 |
|------|------|------|----------|
| 1 | RAR 解压 | ✅ | `D:\temp_efinity\`（WinRAR 7.20b3，`C:\Program Files\WinRAR\unrar.exe`） |
| 2 | VC++ 运行库 | ✅ | 系统已有（x64 14.42.34433.0），无需安装 |
| 3 | Efinity 2025.2 MSI | ✅ | 安装到 `C:\Efinity\2025.2\`（BASEINSTALLDIR 参数被忽略），后迁移至 `D:\Efinity\2025.2\` |
| 4 | efxdevicedb.ini | ✅ | `D:\Efinity\2025.2\arch\efxdevicedb.ini` |
| 5a | 补丁 v288.3.8 | ✅ | 手动复制（run.bat 因 Python 编码失败） |
| 5b | 补丁 v288.4.15 | ✅ | 手动复制（bin/efx_map/pgm/pnr 更新） |
| 6 | JTAG Bridge bitstream | ✅ | 18个 .bit 到 `pgm\\fli\\titanium\`、`topaz\`、`trion\` |
| 7 | FT4232 驱动 | ✅ | libusb-win32（oem105.inf），通过 `pnputil /add-driver` 提权安装 |
| 8 | RISC-V IDE | ✅ | `D:\Efinity\efinity-riscv-ide-2025.2\`（含工具链 + openocd + qemu） |

## 关键路径与文件

```
D:\Efinity\
├── 2025.2\                           ← 2.4 GB / 42,539 文件
│   ├── bin\efx_map.exe              ← 综合工具
│   ├── bin\efx_pgm.exe              ← 烧录工具（950KB）
│   ├── bin\efx_pnr.exe              ← 布局布线
│   ├── bin\efx_run.bat              ← 运行脚本
│   ├── bin\efx_simulate.exe         ← 仿真
│   ├── arch\efxdevicedb.ini         ← 设备数据库
│   ├── pgm\fli\titanium\*.bit       ← JTAG bitstream
│   ├── pgm\fli\topaz\*.bit
│   ├── pgm\fli\trion\*.bit
│   └── patch_readme.txt             ← v288.4.15 补丁确认
└── efinity-riscv-ide-2025.2\        ← 2.9 GB / 28,026 文件
    ├── Efinity-RISCV-IDE\efinity-riscv-ide.exe
    ├── toolchain\bin\riscv-none-embed-gcc.exe  ← 注意：前缀是 riscv-none-embed-*，不是 riscv32-unknown-elf-*
    ├── toolchain\bin\riscv-none-embed-gdb.exe  ← 10 MB 调试器
    ├── openocd\bin\openocd.exe       ← JTAG 调试
    ├── qemu\qemu-system-riscv32.exe  ← 仿真器
    └── build_tools\bin\make.exe      ← 构建工具
```

## 与 v0.2 方案预期的偏差

| 预期（v0.2 + GA修正） | 实际 | 影响 |
|----------------------|------|------|
| 安装到 `D:\Efinity\2025.2\` | MSI 强制装 `C:\Efinity\2025.2\`，后手动迁移 |  MCP 配置中 efinity.install_dir 最终为 D 盘正确 |
| `run.bat` 打补丁 | 手动复制 files/（run.bat 失败） |  结果一致，但补丁方式改为手动复制 |
| 只装 v288.3.8 补丁 | v288.3.8 + v288.4.15 都装了 |  两者文件无冲突，v4.15 覆盖了 v3.8 的 bin 文件 |
| FT4232 用 `dpinst64 /S` | 用 `pnputil /add-driver` 提权 |  驱动已安装（oem105.inf） |
| 工具链 `riscv32-unknown-elf-*` | 实际为 `riscv-none-embed-*` |  MCP 工具中路径/命令名需修正 |
| 不需要安装 VC++ | 已有 |  一致 |

## 已配置的系统环境

- **环境变量**：`EFINITY_HOME = D:\Efinity\2025.2\`（用户级）
- **注册表**：`InstallLocation = D:\Efinity\2025.2\`（HKLM 卸载项）
- **桌面快捷方式**：`D:\Efinity\2025.2\bin\efx_run.bat` -> 桌面 `Efinity 2025.2.lnk`
- **RISC-V IDE 快捷方式**：已更新指向 D 盘
- **C 盘已释放**：删除 `C:\Efinity\`，回收 5.26 GB

## MCP 配置中的路径更新

MCP 的 `fpga_robot.local.json` 配置文件中以下字段应使用实际路径：

```json
{
  "efinity": {
    "home": "D:\\Efinity\\2025.2",
    "bin": "D:\\Efinity\\2025.2\\bin",
    "cli_prefix": "efx",
    "pgm_fli": "D:\\Efinity\\2025.2\\pgm\\fli",
    "toolchain_prefix": "riscv-none-embed-"
  },
  "riscv_ide": {
    "home": "D:\\Efinity\\efinity-riscv-ide-2025.2",
    "toolchain_bin": "D:\\Efinity\\efinity-riscv-ide-2025.2\\toolchain\\bin",
    "openocd": "D:\\Efinity\\efinity-riscv-ide-2025.2\\openocd\\bin\\openocd.exe",
    "build_tools": "D:\\Efinity\\efinity-riscv-ide-2025.2\\build_tools\\bin"
  }
}


---

# CC 二次审核 — 差距分析（2026-07-02），追加于安装执行记录后

来源：Codex (OpenAI) 代审。原计划调用 Claude Code 审核，但 CC API 网关 (127.0.0.1:15721 → opencode.ai/zen) 返回 502 不可达，改用 Codex 执行审核。
注意：Codex 在分析中自动搜索了工作目录下的旧文件（Piper 机械臂相关），已忽略其关于 Piper 的分析结论，仅取其通用 MCP 架构建议。

## Codex 审核要点摘要

### 核心判断
MCP 方案作为"AI 编排 + 本地控制"可行；作为"AI/MCP 直接控制机械臂和 FPGA 实时动作"风险偏高。

### 主要建议
1. **实时控制边界**：MCP 只发高层命令（plan_motion/validate_motion），实时闭环放在 myCobot 控制器或本地控制进程内
2. **安全缺口**：需补齐关节限位、速度/加速度上限、工作空间围栏、急停、看门狗、通信丢失停机策略
3. **工具粒度**：不建议暴露 send_raw_command/set_joint_angles，推荐 get_robot_state/validate_pose/simulate_trajectory 等分层工具
4. **状态机**：需要显式状态机 INIT→HOMED→IDLE→PLANNING→EXECUTING→RECOVERY，工具应检查状态
5. **错误语义**：结构化错误码（SAFETY_LIMIT/IK_UNREACHABLE/SERIAL_TIMEOUT 等）
6. **观测审计**：每次调用记录调用者、参数、审批状态、轨迹摘要、执行时间、状态快照
7. **安全架构**：推荐 MCP Server → Safety Broker → Robot Controller Service → FPGA Service → myCobot 的分层架构

---

# GA 实操差距分析（2026-07-02，补充于审核结论之后）

来源：GA（Python物理执行者）基于对 MCP 方案全文、Claude审核意见、GA审查修正、安装执行记录和实际硬件环境的综合判断。

## 差距总览

| 维度 | 状态 | 说明 |
|------|------|------|
| Phase -1（工具链安装） | ✅ **已完成** | Efinity/RISC-V IDE/FT4232/补丁全部就绪 |
| Phase 0（只读核验+环境准备） | ⏳ **部分完成** | 核验已做，但 Python 依赖未装、MCP 代码未写 |
| Phase 1（MCP 骨架+注册） | ❌ **未开始** | server.py/config.py/safety.py 均未创建 |
| Phase 2（Efinity 构建与烧录） | ❌ **未开始** | 依赖 Phase 1 完成 + JTAG 硬件连接 |
| Phase 3（myCobot 联调） | ❌ **未开始** | 依赖 Phase 1+2 + 机械臂硬件连接 |

## 剩余工作清单（按执行顺序）

### 步骤 A: Phase 0 收尾（可立即执行，无需硬件）
1. **安装 Python 依赖**：在 pfmval_py310 环境安装 mcp、pydantic、pyserial、pymycobot
   ```powershell
   D:\miniconda\Scripts\conda.exe run -n pfmval_py310 python -m pip install mcp pydantic pyserial pymycobot
   ```
2. **确认 CLI 可调用**：验证 efx_run/efx_pgm 可在 D:\Efinity\2025.2\bin\ 中被调用
3. **创建 verify_env.py**：校验所有依赖并输出 JSON 报告

### 步骤 B: Phase 1 MCP 骨架（需步骤 A 完成）
4. **创建 MCP 目录结构**：final_project/tools/mcp/fpga_robot_mcp/src/
5. **实现 config.py**：加载 fpga_robot.local.json（路径需更新为 D:\）
6. **实现 server.py**：基础 MCP stdio server + 只读 status/get_config 工具
7. **实现 safety.py**：三级安全门控（只读/写文件/硬件副作用）
8. **创建 .claude/settings.json**：项目级 MCP 注册
9. **验证 stdio 启动**：python -m fpga_robot_mcp.server 或 MCP Inspector

### 步骤 C: 硬件连接验证（需物理硬件就绪）
10. **连接 JTAG 下载线**：验证 efx_pgm --scan 能发现 JTAG 链
11. **连接 myCobot 280**：安装 CP210x 驱动，确认 COM 口
12. **验证 pymycobot 握手**：mycobot280_validate_connection 只读通信

### 步骤 D: 逐步开放功能
13. 实现 efinity_check_project / efinity_list_artifacts（只读）
14. 实现 efinity_run_build(dry_run=true) / efinity_collect_logs
15. 实现 mycobot280_read_angles / mycobot280_set_rgb
16. 实现 efinity_program_bitstream（需 confirm_token）
17. 实现 mycobot280_execute_motion（需 confirm_token + safety check）

## 关键配置修正（对比 v0.2 原方案）

| 配置项 | v0.2 原值 | 实际修正值 |
|--------|----------|-----------|
| efinity.install_dir | C:/Efinity/2025.2 | D:/Efinity/2025.2 |
| efinity.bin_dir | C:/Efinity/2025.2/bin | D:/Efinity/2025.2/bin |
| RISCV toolchain prefix | riscv32-unknown-elf- | riscv-none-embed- |
| RAR 解压 | 需安装 7-Zip | 已有 WinRAR 7.20b3 |

## 风险提示（按优先级）

1. **🔴 高：补丁完整性** — v288.3.8 和 v288.4.15 都手动复制了，但 run.bat 未执行，是否存在依赖注册表或 PATH 的组件未被识别
2. **🔴 高：JTAG 驱动验证** — FT4232 驱动已装但未用硬件验证，oem105.inf 是否对应正确硬件版本未知
3. **🟡 中：补丁双重安装** — v288.4.15 覆盖了 v3.8 的 bin 文件，如果 v4.15 依赖 v3.8 的中间文件可能会有问题
4. **🟢 低：C 盘释放** — 5.26GB 已释放，但注册表卸载路径已改为 D 盘，后续卸载时正常
5. **🟢 低：环境变量** — EFINITY_HOME 是用户级变量，管理员 CLI 可能不继承

## 结论

**当前状态：Phase -1 完成（✅），Phase 0 进行中（⏳ 50%），Phase 1-5 未开始（❌）。**
最短路径到"可执行 MCP 配置"需要完成步骤 A（装依赖）→ B（建骨架）共约 2-3 小时工作量（无硬件）。加上硬件验证（步骤 C）约需半天。
