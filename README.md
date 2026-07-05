# CICC

第十届集创赛雄芯院方向项目资料与分赛区决赛开发工程。

## 项目成员

- @qzs610038-star (管理员)
- @libaoxun688 (开发者)
- @wsc6090-CPU (开发者)

## 开发规范

1. 各自在 `dev/用户名` 分支开发。
2. 完成后向 `main` 提 Pull Request。
3. 需要至少 1 人审查通过后合并。

## 队友拉取后需要自行配置

以下内容与本机环境、工具安装路径或外设连接有关，拉取后请按自己的电脑重新配置，不要直接照抄路径：

- Efinity / RISC-V IDE 安装路径：CPU Makefile、IDE 导入和 OpenOCD/debug profile 需要按本机 Efinity 2025.2 安装位置确认。
- Efinity 生成物：`soc.h`、linker、OpenOCD/debug profile 必须来自最终工程重新生成的 SoC 配置，不能硬抄官方 RISC-V 示例里的地址。
- MCP 本地配置：复制 `final_project/tools/mcp/fpga_robot_mcp/configs/fpga_robot.local.example.json` 为本机 `fpga_robot.local.json`，填写 Efinity、工程路径、串口、日志目录。
- Claude/Codex 本地设置：`.claude/settings.local.json` 是开发机权限/工具配置示例，队友应按自己的本机路径和安全策略调整。
- myCobot 串口：用设备管理器或串口枚举确认 COM 口；myCobot 资料要求波特率 `1000000`，不要和 115200 的调试 UART 混用。
- Python 环境：MCP 工具依赖 `mcp`、`pyserial`、`pymycobot`、`pydantic`，建议在个人虚拟环境中安装。

正式比赛闭环仍以 FPGA 平台内 CPU 为准；PC、MCP、Claude/Codex、pymycobot 只用于开发期调试、标定和审查。
