# Codex 修正说明

修正日期：2026-07-01  
修正对象：`机械臂MCP构建实施方案_v0.1_Codex初稿.md`  
依据文件：`Claude审核意见.md`

## 采纳结论

已采纳 Claude 审核意见中的 4 个重大遗漏和 7 个中等风险项，并形成 `机械臂MCP构建实施方案_v0.2_Codex修正版.md`。

## 主要修正

- 新增 `Phase -1：工具链与前置软件安装`，明确 Efinity 2025.2 需先解压、安装、打补丁，再进入 MCP 骨架开发。
- 写明当前本机已核验状态：未发现 Efinity CLI，未发现 7z/unrar/winrar，`pfmval_py310` 存在但 MCP 依赖未安装。
- 固定 MCP 推荐运行环境为 Python 3.10，优先使用 `D:\conda_envs\pfmval_py310\python.exe`；如需隔离则新建 `fpga_mcp_py310`。
- 补充 Claude Code 与 Codex 的 MCP 注册方案：Claude 使用 `.claude/settings.json` 的 `mcpServers`，Codex 使用 `C:\Users\33696\.codex\config.toml` 的 `[mcp_servers.fpga_robot_mcp]`。
- 增加手动等效操作指南，确保 MCP 失效时仍能手动完成 Efinity 路径检查、串口枚举、依赖检查、日志归档和后续烧录核查。
- 新增 `board_check_jtag_chain`、`board_uart_loopback_test`、`efinity_check_programmer` 等只读/低风险工具规划。
- 明确 token 不持久化保存，每次硬件副作用操作由用户在对话中显式提供。
- 调整 Review Packet 与日志路径：正式方案评审包放 `方案评审/NNN_名称/`，日常构建/串口/动作证据放 `final_project/docs/evidence/`。
- 接受 Claude 关于单服务的建议：保持 `fpga_robot_mcp` 单服务，代码内部按 `efinity_tools.py`、`board_tools.py`、`mycobot_tools.py` 模块拆分。

## 需要后续 Claude 继续核验

- Efinity 安装后的真实 CLI 命令、参数和日志路径。
- Efinity patch `run.bat` 的运行前提、是否需要管理员权限、是否需要从安装根目录执行。
- `pymycobot` 在 Python 3.10 环境中的安装与导入状态。
- CP210x 驱动安装后的真实 COM 口、VID/PID 和设备管理器显示名称。
- JTAG 下载线 / 仿真器是否可被 `efx_pgm` 或等效命令扫描到。
- myCobot 文档中的 TX/RX 接线写法需要现场复核后再作为默认接线建议。
