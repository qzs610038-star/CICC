# fpga_robot_mcp

Efinity / 开发板 / myCobot 280 的本地 MCP 工具，用于 FPGA 比赛项目开发期自动化。

**定位**：开发期辅助工具，不进入比赛正式闭环。

队友从云端 `main` 同步后，优先阅读：

- [docs/TEAMMATE_AGENT_SETUP.md](docs/TEAMMATE_AGENT_SETUP.md) — 本机配置、ASCII junction、Efinity 构建、JTAG 烧录和闭环测试 SOP。

## 快速开始

```bash
# 1. 安装依赖
pip install mcp pyserial pymycobot pydantic

# 2. 复制配置模板
cp configs/fpga_robot.local.example.json configs/fpga_robot.local.json
# 编辑 fpga_robot.local.json 填入本机路径

# 3. 安装包（可选，推荐）
pip install -e .

# 4. 启动服务器（stdio 模式）
python -m fpga_robot_mcp.server

# 5. 在 Claude Code / Codex 中注册后即可使用工具
```

## 目录结构

```
src/fpga_robot_mcp/
  __init__.py     — 包标识 + 版本号
  config.py       — 配置加载与路径解析
  safety.py       — 三级安全门控 + audit 日志
  server.py       — FastMCP 入口 + 工具注册
  serial_probe.py — 串口枚举与分类 ⬅️ 待实现
  efinity_tools.py— Efinity 工具链校验 ✅ 已实现基础
  board_tools.py  — 开发板 UART/JTAG 工具 ⬅️ 待实现
  mycobot_tools.py— myCobot 280 环境/端口/只读状态 ⬅️ 待实现
  review_packet.py— Review Packet 模板生成 ⬅️ 待实现
```

## MCP 注册（Claude Code）

在 `.claude/settings.json` 中添加：

```json
{
  "mcpServers": {
    "fpga_robot_mcp": {
      "command": "D:\\miniconda\\Scripts\\conda.exe",
      "args": ["run", "-n", "pfmval_py310", "python", "-m", "fpga_robot_mcp.server"],
      "env": {
        "FPGA_ROBOT_MCP_CONFIG": "absolute_path/configs/fpga_robot.local.json"
      }
    }
  }
}
```

若已 `pip install -e .` 可移除 PYTHONPATH。

## 版本

当前版本 0.2.0 — Phase 1 骨架。
