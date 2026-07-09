# fpga_robot_mcp Phase 2A 实施记录

日期：2026-07-09
执行者：Codex
范围：`final_project/tools/mcp/fpga_robot_mcp/`
状态：已完成真实 Efinity/JTAG 探测、artifact 定位和 dry-run 烧录命令生成；未执行真实烧录。

## 本轮目标

把此前方案中的“自动烧录 MCP”从占位接口推进到可运行的项目级开发工具，服务于以下 agent 迭代闭环：

```text
RTL/CPU 代码修改
  -> Efinity 构建/产物定位
  -> FPGA dry-run 烧录计划
  -> 显式确认后真实烧录
  -> 运行日志/报告归档
  -> agent 读取证据并继续修复
```

该 MCP 仍只属于开发期工具，不进入比赛正式运行闭环。比赛主线仍按 `AGENTS.md`「分赛区决赛主线」执行：FPGA 负责视频前端、ROI、统计特征、OSD 和必要硬件加速；板上 CPU 负责识别决策、参数管理和 myCobot 控制。

## 已完成改动

### 1. Efinity 工程口径切回正式工程

- 默认工程从赛方 demo 改为 `final_project/fpga/efinity/mem_test.xml`。
- 默认约束从赛方 demo 改为 `final_project/fpga/efinity/constrain.sdc`。
- 增加外部烧录工程根目录 `D:/final_project_shaolu`，用于兼容当前已有 `outflow` 产物。

涉及文件：

- `final_project/tools/mcp/fpga_robot_mcp/src/fpga_robot_mcp/config.py`
- `final_project/tools/mcp/fpga_robot_mcp/configs/fpga_robot.local.example.json`
- `final_project/tools/mcp/fpga_robot_mcp/configs/fpga_robot.local.json`

### 2. 接入真实 Efinity programmer CLI

确认本机可用下载工具不是旧假设的 `efx_pgm.exe --scan`，而是：

```powershell
cmd /c "call D:\Efinity\2025.2\bin\setup.bat && python D:\Efinity\2025.2\pgm\bin\efx_pgm\ftdi_program.py --list_usb"
cmd /c "call D:\Efinity\2025.2\bin\setup.bat && python D:\Efinity\2025.2\pgm\bin\efx_pgm\ftdi_program.py --scan_usb"
```

MCP 现在会通过 `ftdi_program.py` 做只读 USB/JTAG 探测，并从输出中解析推荐 URL。

本轮早些时候曾探测到：

- USB target: `YLS_4232DL`
- serial: `FTBI7G42`
- recommended URL: `ftdi://0x0403:0x6011:2:d/2`
- JTAG IDCODE 曾成功读出：`0x006A0EF3`

最终复测时下载器已不再稳定可见，`ftdi_program.py` 返回 `ERROR: No USB target detected, aborting!`。MCP 已修正该边界：错误文本不会再被误判为 USB target；只有真实 USB target 或有效 scan JSON target 才会使 `usb_visible=true`。

注意：`--scan_usb` 在现场可能出现三类状态，MCP 会分别上报：

- `ok`：USB 与 JTAG IDCODE 均可见，可进入真实烧录前置。
- `usb_visible_no_idcode`：USB 可见，但 JTAG IDCODE 不可见，不允许自动真实烧录。
- `no_hardware`：未发现可用 USB target。

### 3. 实现 dry-run 烧录计划与真实烧录门控

`efinity_program_bitstream` 现在默认 `dry_run=true`，只生成命令，不触碰硬件。

示例 dry-run 命令：

```powershell
call "D:\Efinity\2025.2\bin\setup.bat" >nul && "python" "D:\Efinity\2025.2\pgm\bin\efx_pgm\ftdi_program.py" "D:\final_project_shaolu\fpga\efinity\outflow\mem_test.bit" "-m" "jtag" "--url" "ftdi://0x0403:0x6011:2:d/2"
```

真实烧录需要同时满足：

- `dry_run=false`
- `allow_hardware_actions=true`
- 提供当前安全 token
- USB 可见
- JTAG IDCODE 可见
- `.bit`/`.hex` 与烧录模式匹配

模式限制：

- `jtag` / `jtag_chain`：只接受 `.bit`
- `active` / `passive`：只接受 `.hex`

### 4. 日志与 artifact 定位覆盖正式工程和烧录副本

`efinity_list_artifacts` 和 `efinity_collect_logs` 现在同时扫描：

- `final_project/fpga/efinity/outflow`
- `D:/final_project_shaolu/fpga/efinity/outflow`

当前正式工程 outflow 尚不存在；烧录副本 outflow 中可定位到历史 `mem_test.bit`、`mem_test.hex`、report 和 log。

### 5. 安全审计不再落盘 token

此前 token 可能通过 audit detail 泄露。现在硬件确认 token 只记录为：

```text
confirm_token=present_redacted
```

dry-run 硬件工具也会进入审计日志，但不会要求真实硬件权限打开。

## 验证结果

在 `D:\第十届集创赛-雄芯院材料` 下完成以下验证：

```powershell
D:\conda_envs\pfmval_py310\python.exe -m py_compile final_project/tools/mcp/fpga_robot_mcp/src/fpga_robot_mcp/config.py final_project/tools/mcp/fpga_robot_mcp/src/fpga_robot_mcp/safety.py final_project/tools/mcp/fpga_robot_mcp/src/fpga_robot_mcp/server.py final_project/tools/mcp/fpga_robot_mcp/src/fpga_robot_mcp/efinity_tools.py final_project/tools/mcp/fpga_robot_mcp/src/fpga_robot_mcp/board_tools.py
D:\conda_envs\pfmval_py310\python.exe final_project/tools/mcp/fpga_robot_mcp/tests/test_config.py
D:\conda_envs\pfmval_py310\python.exe final_project/tools/mcp/fpga_robot_mcp/tests/test_safety.py
D:\conda_envs\pfmval_py310\python.exe final_project/tools/mcp/fpga_robot_mcp/tests/test_serial_probe.py
D:\conda_envs\pfmval_py310\python.exe final_project/tools/mcp/fpga_robot_mcp/tests/test_efinity_probe.py
D:\conda_envs\pfmval_py310\python.exe final_project/tools/mcp/fpga_robot_mcp/tests/test_tool_entrypoints.py
```

结果：

- `py_compile` 通过。
- `test_config.py`：10/10 通过。
- `test_safety.py`：12/12 通过。
- `test_serial_probe.py`：13/13 通过。
- `test_efinity_probe.py`：18/18 通过。
- `test_tool_entrypoints.py`：23/23 通过。
- `test_tool_entrypoints.py` 已覆盖 MCP 工具入口可调用性，包括 `fpga_robot_status`、Efinity、board、serial、myCobot 和 review packet 工具。
- `python -m pytest` 未运行成功，原因是 `pfmval_py310` 环境未安装 `pytest`；已用仓库现有脚本测试入口逐个替代。

现场只读探测最终复测结果：

```text
programmer_status = no_hardware
usb_visible = false
jtag_idcode_visible = false
recommended_url =
artifact_total = 31
dry_run_status = dry_run
dry_run_ready = false
```

## 仍未完成

### 阻塞 1：尚未生成正式工程本轮 outflow

当前可用 bitstream 来自 `D:/final_project_shaolu/fpga/efinity/outflow/mem_test.bit`。要进入项目级自动迭代，应先把 `final_project/fpga/efinity` 的构建产物稳定生成到正式工程 outflow，或明确烧录副本与正式工程的同步策略。

### 注意 2：真实烧录需逐次授权

已于 2026-07-09 20:01 在用户明确确认后完成一次真实 JTAG 烧录验证，见下方「真实烧录验证」。后续仍应保持同样规则：真实烧录必须由用户明确确认目标 bitstream、板卡连接状态和安全 token 后再做。

### 阻塞 3：FPGA 运行日志抓取还需要按实际观测通道落地

当前 MCP 已能收集 Efinity outflow/report/log，但“FPGA 运行日志”需要进一步确定来源：

- Type-C UART
- JTAG-IF UART
- UART2 TO Peripherals
- CPU 写回 OSD/状态寄存器
- HDMI/OSD 人工截图记录

建议优先把 Type-C UART 或 JTAG-IF UART 做成 `serial_capture_session` 级别的可复现工具。

## 下一步建议

1. 做 Phase 2B：在用户确认后执行一次 `efinity_program_bitstream(..., dry_run=false)`，把真实命令、返回码和 stdout/stderr 归档到 evidence。
2. 做 Phase 2C：增加“烧录后日志抓取 profile”，先支持串口按秒采集、关键字提取、超时停止和日志归档。
3. 做 Phase 2D：把 build -> artifact -> dry-run/program -> log collect 串成一个 `fpga_iteration_session` 工具，但真实烧录仍保持 token 门控。
4. 做 Phase 3：接入 agent 修复闭环，让失败日志自动生成最小 review packet，而不是直接让 agent 大范围改 RTL。

## 真实烧录验证

用户明确授权：

```text
允许真实烧录 D:\final_project_shaolu\fpga\efinity\outflow\mem_test.bit 到当前板卡
```

执行路径：

- 先调用 MCP 工具入口 `server.efinity_program_bitstream(..., dry_run=true)` 做 preflight。
- 当前进程内按用户授权临时打开 `allow_hardware_actions`，生成 `EFINITY_PROGRAM` confirm token。
- 再调用同一 MCP 工具入口 `server.efinity_program_bitstream(..., dry_run=false)` 执行真实烧录。

烧录结果：

```text
status = ok
return_code = 0
duration_seconds = 23.6
mode = jtag
url = ftdi://0x0403:0x6011:2:11/2
bitstream = D:\final_project_shaolu\fpga\efinity\outflow\mem_test.bit
log_path = final_project/docs/evidence/program_20260709_200151.log
```

日志关键内容：

```text
jtag programming started!
JTAG Programming on ftdi://0x0403:0x6011:2:11/2
Programming 'D:\final_project_shaolu\fpga\efinity\outflow\mem_test.bit' via JTAG at freq 6.0 MHz
Device ID read from JTAG: 0x006A0EF3
... finished with JTAG programming
```

烧录后复查：

```text
programmer_status = ok
usb_visible = true
jtag_idcode_visible = true
ready_for_jtag_program = true
idcode = 0x006A0EF3
```
