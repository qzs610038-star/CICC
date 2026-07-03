# Codex 审查日志：MCP 注册与 Efinity Programmer CLI 对齐

> 日期：2026-07-03 16:48:08 +08:00  
> 范围：`final_project/tools/mcp/fpga_robot_mcp`、`.claude/settings.json`、Efinity 2025.2 Programmer CLI、初赛 demo 烧录可行性  
> 当前状态：服务端可用，注册已临时撤销；自动烧录 CLI 未完成对齐，首次烧录建议走 Efinity Programmer GUI。

## 1. 本次结论摘要

`fpga_robot_mcp` 服务端实现层已经达到 Phase 1 只读预检可用状态：工具可注册、配置可加载、Efinity 2025.2 与 RISC-V IDE 可识别、FT4232 四路串口可见、myCobot Python 环境可用。

但它还不能宣称具备“可靠自动烧录”能力。关键原因是 Efinity 2025.2 的真实下载链路与原 MCP 中的旧假设不同：

- `D:\Efinity\2025.2\bin\efx_pgm.exe` 是 bit/hex 生成器，不是直接上板下载器。
- Efinity Programmer GUI 通过 `D:\Efinity\2025.2\pgm\bin\efx_pgm\efinity_pgm.py` 启动。
- 实际 FTDI 下载后端候选是 `D:\Efinity\2025.2\pgm\bin\efx_pgm\ftdi_program.py`。
- 该后端必须先经过 Efinity 环境初始化：`call D:\Efinity\2025.2\bin\setup.bat`。
- 当前 Windows 设备层能看到 FTDI VCP 串口 `COM3/COM6/COM7/COM8`，但 `ftdi_program.py --list_usb` 和 `--scan_usb` 尚不能识别 USB target。

因此当前推荐边界是：MCP 负责状态探测、产物定位、证据归档和 Review Packet；第一次真实烧录仍由 Efinity Programmer GUI 完成。

## 2. Claude 自检结论核查

Claude 给出的两层判断基本成立，但有一个后续状态变化：

| 项 | Claude 结论 | Codex 核查 |
|---|---|---|
| 服务端实现层 | 可用，26 个工具可注册 | 确认。直接初始化 `server.mcp` 显示 26 个工具 |
| Python 环境 | `D:\conda_envs\pfmval_py310\python.exe` 可用 | 确认 |
| 配置加载 | `fpga_robot.local.json` 可解析 | 确认 |
| 只读预检 | `fpga_robot_status` 返回 Efinity/RISC-V/FT4232 状态 | 确认 |
| Claude 注册层 | `.claude/settings.json` 缺少 `mcpServers` | 当时成立；本次曾临时补上并验证 connected，随后按用户要求已撤销 |
| `conda run` 风险 | 建议绕开 `conda run`，直接用 python.exe | 认可。直接 python.exe + `PYTHONPATH` 更稳 |

本次撤销后，`.claude/settings.json` 又恢复为不含 `mcpServers` 的状态；这符合“先和队友确认关键约束后再补全”的安排。

## 3. 已验证事实

### 3.1 MCP 服务端

已验证：

```text
server.mcp.name = fpga_robot_mcp
工具数量 = 26
```

相关测试结果：

```text
test_efinity_probe.py: 14/14 通过
test_tool_entrypoints.py: 25/25 通过
```

`fpga_robot_status` 摘要：

```text
status = ok
Efinity installed = True
RISC-V IDE installed = True
FT4232 serial ports = 4
```

### 3.2 Efinity 与初赛 demo 产物

初赛 demo 工程：

```text
初赛demo\2ChMIPICSI_2ChMIPIDSI_Demo_Test\mem_test.xml
```

已确认：

- `last_run_state="pass"`
- 目标器件：`TJ375N529`
- 顶层模块：`top`
- `generate_bit=on`
- `generate_hex=on`

可用于 GUI 冒烟测试的产物：

```text
初赛demo\2ChMIPICSI_2ChMIPIDSI_Demo_Test\outflow\mem_test.bit
初赛demo\2ChMIPICSI_2ChMIPIDSI_Demo_Test\outflow\mem_test.hex
```

初赛 demo 自带的 `program_fpga.bat` 也只是启动 GUI：

```bat
start "" "%EFINITY_HOME%\bin\efinity.exe" --programmer
```

它没有提供可直接复用的无界面 CLI 烧录命令。

## 4. Efinity Programmer CLI 对齐现状

### 4.1 已确认不能用的旧命令

原 MCP 中存在类似以下旧假设：

```text
efx_pgm.exe -m ram -p 1 -b <bitstream>
```

该命令不应继续使用。`efx_pgm.exe --help` 显示其主选项是：

```text
--project-xml
--source
--periph
--dest
--device
--family
--mode
```

这些选项对应 bit/hex 生成流程，而不是 FTDI/JTAG 上板下载。

### 4.2 当前候选 CLI

只读帮助命令：

```powershell
cmd /d /c "call D:\Efinity\2025.2\bin\setup.bat && python D:\Efinity\2025.2\pgm\bin\efx_pgm\ftdi_program.py --help"
```

`ftdi_program.py` 支持的关键参数包括：

```text
--mode passive|active|jtag|jtag_chain|erase_flash|read_flash|jtag_bridge|jtag_bridge_x8
--board_profile "Generic Board Profile Using FT4232"
--list_usb
--scan_usb
--jtag_clock_freq
--verify_method
```

当前应先跑的只读预检：

```powershell
cmd /d /c "call D:\Efinity\2025.2\bin\setup.bat && python D:\Efinity\2025.2\pgm\bin\efx_pgm\ftdi_program.py --list_usb"
cmd /d /c "call D:\Efinity\2025.2\bin\setup.bat && python D:\Efinity\2025.2\pgm\bin\efx_pgm\ftdi_program.py --scan_usb"
```

本次结果：

```text
ERROR: No USB target detected, aborting!
```

这说明 FTDI VCP 串口可见，不等于 Efinity 的 pyftdi/libusb 下载后端可见。

## 5. 本次代码侧处理

为避免误触发错误烧录命令，已将 `efinity_program_bitstream` 降级为明确拒绝状态：

```text
status = programmer_cli_not_aligned
```

它现在会返回候选 CLI、前置预检命令和原因说明，但不会执行上板下载。

保留这个改动是必要的，因为它是安全修正，不依赖是否注册 MCP。

## 6. 已临时撤销的注册

本次曾验证过如下 Claude MCP 注册写法可用：

```json
{
  "mcpServers": {
    "fpga_robot_mcp": {
      "command": "D:\\conda_envs\\pfmval_py310\\python.exe",
      "args": ["-m", "fpga_robot_mcp.server"],
      "env": {
        "FPGA_ROBOT_MCP_CONFIG": "D:/第十届集创赛-雄芯院材料/final_project/tools/mcp/fpga_robot_mcp/configs/fpga_robot.local.json",
        "PYTHONPATH": "D:/第十届集创赛-雄芯院材料/final_project/tools/mcp/fpga_robot_mcp/src",
        "PYTHONUTF8": "1",
        "PYTHONIOENCODING": "utf-8"
      }
    }
  }
}
```

验证结果：

```text
claude mcp list
fpga_robot_mcp ... Connected
```

但按当前决策，`.claude/settings.json` 中的 `mcpServers` 块已撤销。后续与队友确认后再恢复。

撤销范围包括：

- 项目级 `.claude/settings.json` 中的 `mcpServers.fpga_robot_mcp`。
- Claude 全局 `C:\Users\33696\.claude.json` 中的项目级与全局 `fpga_robot_mcp` 注册。
- Codex 全局 `C:\Users\33696\.codex\config.toml` 中的 `[mcp_servers.fpga_robot_mcp]` 注册。

撤销只影响客户端自动加载，不删除 MCP 源码、本地配置文件或测试产物。

## 7. 需要和队友确认后补全的关键约束

### 7.1 是否允许改 FTDI 驱动栈

当前 `ftdi_program.py --list_usb` 识别不到 USB target。常见原因可能是 FTDI 设备当前使用 VCP/D2XX 驱动，而 pyftdi/libusb 后端需要 WinUSB/libusbK 等驱动。

需要队友确认：

- 是否允许对 FT4232 的某个 interface 改驱动。
- 哪个 interface 用于 JTAG，哪个 interface 保持串口。
- 是否允许使用 Zadig 或 Efinity 官方驱动工具。
- 改驱动是否会影响 RISC-V IDE、JTAG 调试、串口日志或 GUI Programmer。

在确认前，不建议为了 CLI 自动化擅自改驱动。

### 7.2 烧录模式边界

需要确认：

- 初次测试是 JTAG volatile/SRAM 下载，还是写 SPI Flash。
- `mem_test.bit` 与 `mem_test.hex` 分别在哪种 Programmer 模式下使用。
- 是否允许擦写板载 Flash。
- 是否需要保留板卡出厂 demo 或队友已有烧录内容。

建议默认：

- 冒烟测试优先 GUI + 非持久下载。
- 未确认前不做 erase_flash / active flash write。

### 7.3 MCP 硬件动作安全门

当前安全策略建议保持：

```text
allow_hardware_actions = false
require_confirm_token = true
```

需要确认后才能开启：

- 允许哪些工具从只读升级为硬件副作用。
- 烧录确认 token 的格式与使用人。
- 机械臂动作是否必须二次人工确认。
- 审计日志保存位置与脱敏规则。

### 7.4 初赛 demo 的定位

初赛 demo 可以用于板卡链路冒烟、MIPI/DDR/framebuffer/OSD 调试经验参考，但不应作为决赛正式代码基线。

需要队友确认：

- 本轮测试的可接受成功标准是什么。
- 没有摄像头/屏幕/MIPI 外设时，如何判定烧录成功。
- 是否要同步记录 Programmer 日志、屏幕现象和串口输出。

## 8. 推荐下一步

1. 先用 Efinity Programmer GUI 对 `mem_test.bit` 或 `mem_test.hex` 做一次手动冒烟测试。
2. 保存 GUI 成功日志或截图，记录实际选择的 mode、board profile、file type。
3. 若队友允许驱动调整，再解决 `ftdi_program.py --list_usb` 识别不到 USB target 的问题。
4. 只有当 `--list_usb/--scan_usb` 可稳定识别目标后，再把 `efinity_program_bitstream` 从 `programmer_cli_not_aligned` 升级为真实烧录工具。
5. 恢复 MCP 注册前，先把上述约束补进本评审包或 `CLAUDE.md`，避免后续 agent 误触发硬件动作。
