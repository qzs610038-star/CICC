# Phase 1 实施策划 — MCP 骨架与注册

日期：2026-07-02  
状态：待执行（本文件为 Phase 1 的执行说明书）  
依据：`机械臂MCP构建实施方案_v0.2_Codex修正版.md`

---

## 1. Phase 1 目标

1. **创建可运行的单 stdio MCP 服务 `fpga_robot_mcp`**，在 Python 3.10 环境（`pfmval_py310`）中用 FastMCP 实现。
2. **完成配置加载、安全门控、错误处理、stderr 日志等基础设施**。
3. **实现首批只读工具**（`fpga_robot_status`、`fpga_robot_get_config`、`efinity_locate_toolchain`、`efinity_check_project`、`efinity_check_install_prereq`、`board_list_uart_candidates`、`board_check_jtag_chain`、`mycobot280_check_env`）。
4. **注册到 Claude Code 项目级 `.claude/settings.json`**，使 Claude 可在当前仓库直接调用 MCP 工具。
5. **验证 stdio 启动**，确认所有工具可发现。

Phase 1 **不做** 烧录、机械臂动作、Efinity 构建、子进程实时控制。这些归 Phase 2+。

---

## 2. 文件树与职责

所有代码位于 `final_project/tools/mcp/fpga_robot_mcp/`：

```
fpga_robot_mcp/
├── README.md                        ← 本包说明（我已完成）
├── pyproject.toml                   ← pip install -e . 支持（我已完成）
├── src/
│   ├── __init__.py                  ← 包标识 + 版本号（我已完成）
│   ├── server.py                    ← FastMCP 入口，注册所有工具（我已完成）
│   ├── config.py                    ← 配置加载、路径解析、设备数据库（我已完成）
│   ├── safety.py                    ← 三级安全门控、confirm token、审计日志（我已完成）
│   ├── serial_probe.py              ← 串口枚举与分类 ⬅️ 留待经济模型填充
│   ├── efinity_tools.py             ← Efinity 路径/工程/构建产物校验（我已完成工具查找部分）
│   ├── board_tools.py               ← 开发板 UART/JTAG/接口契约 ⬅️ 留待经济模型填充
│   ├── mycobot_tools.py             ← myCobot 280 环境/端口/只读状态 ⬅️ 留待经济模型填充
│   └── review_packet.py             ← Review Packet 模板生成 ⬅️ 留待经济模型填充
├── configs/
│   └── fpga_robot.local.example.json ← 配置模板（我已完成）
├── tests/
│   ├── __init__.py                  ← 空（我已完成）
│   ├── test_config.py               ← 配置解析测试（我已完成基础版）
│   └── test_safety.py               ← 安全门控测试（我已完成基础版）
└── logs/
    └── .gitkeep                     ← 日志目录占位（我已完成）
```

**图例：**
- ✅ 我已完成 = 本次由我（当前模型）完成的骨架代码，可直接运行。
- ⬅️ 留待经济模型填充 = 函数签名、docstring、返回格式已写好，但函数体为 `raise NotImplementedError(...)`，需由经济模型逐函数填充。

---

## 3. 已完成的骨架说明

### 3.1 `config.py`（已完成）

使用 Python `dataclasses.dataclass` 定义配置数据结构，支持：

| 功能 | 说明 |
|------|------|
| `FpgaRobotConfig` | 顶层配置，含 `efinity`、`riscv_ide`、`board`、`mycobot280`、`safety` 子结构 |
| `load_config(path)` | 从 JSON 文件加载，支持 `FPGA_ROBOT_MCP_CONFIG` 环境变量 |
| `get_default_config()` | 返回硬编码默认值（对应 GA 安装执行记录后的实际路径） |
| `resolve_path(p)` | 相对/绝对路径解析，统一为 `Path` |
| `hide_sensitive(cfg)` | 隐藏 token、路径中的用户名等敏感字段 |

配置加载顺序：
1. 环境变量 `FPGA_ROBOT_MCP_CONFIG` → JSON 文件
2. 无环境变量 → 搜索 `configs/fpga_robot.local.json`
3. 均不可用 → 返回默认配置（工具可工作，但不指向真实工程）

### 3.2 `safety.py`（已完成）

| 类/函数 | 说明 |
|---------|------|
| `SafetyLevel` 枚举 | `READONLY` / `WRITE_FILE` / `HARDWARE_SIDE_EFFECT` 三级 |
| `SafetyManager` | 核心门控管理类 |
| `SafetyManager.check_tool_allowed(tool_name, level, confirm_token?)` | 返回 `(allowed: bool, message: str)` |
| `SafetyManager.generate_confirm_token(action_type)` | 生成 `I_CONFIRM_MYCOBOT280_SAFE_20260702` 格式 |
| `SafetyManager.validate_confirm_token(token, action_type)` | 校验 token 格式与日期 |
| `SafetyManager.log_audit(entry)` | 写入审计日志 |

门控规则：
- `READONLY` → 默认允许
- `WRITE_FILE` → 需要 `allow_hardware_actions=true` 或 dry-run
- `HARDWARE_SIDE_EFFECT` → 需要 `allow_hardware_actions=true` + `confirm_token` 匹配

### 3.3 `server.py`（已完成）

使用 `FastMCP` 创建 stdio 服务。关键设计：

```python
mcp = FastMCP("fpga_robot_mcp")
```

**工具注册方式**：每个工具调用 `_safe_call()` 包装器：

```python
def _safe_call(func, *args, **kwargs):
    """包装工具调用：捕获 NotImplementedError → 返回友好的"未实现"消息"""
    try:
        result = func(*args, **kwargs)
        return {"status": "ok", "data": result}
    except NotImplementedError as e:
        return {
            "status": "not_implemented",
            "message": str(e),
            "next_step": "该工具尚未实现，详见 Phase 1 实施策划 '留待经济模型填充' 章节"
        }
    except Exception as e:
        return {"status": "error", "message": str(e)}
```

**已注册的工具列表：**

| 工具名 | 实现状态 | 说明 |
|--------|---------|------|
| `fpga_robot_status` | ✅ 可用 | 聚合环境状态：Python/依赖/Efinity/串口/安全配置 |
| `fpga_robot_get_config` | ✅ 可用 | 返回当前 MCP 配置（隐藏敏感字段） |
| `fpga_robot_manual_fallback` | ✅ 可用 | 返回指定工具的手动 PowerShell 等效操作 |
| `efinity_locate_toolchain` | ✅ 可用 | 搜索 Efinity 安装路径、CLI 可执行文件 |
| `efinity_check_install_prereq` | ✅ 可用 | 检查 RAR 解压工具、安装包、补丁脚本 |
| `efinity_check_project` | ✅ 可用 | 校验 mem_test.xml、constrain.sdc、器件型号 |
| `efinity_list_artifacts` | ⬅️ 未实现 | 列出 outflow 产物 |
| `efinity_run_build` | ⬅️ 未实现 | 运行构建（Phase 2） |
| `efinity_check_programmer` | ⬅️ 未实现 | 检查烧录器/下载线 |
| `efinity_program_bitstream` | ⬅️ 未实现 | 烧录 bitstream（Phase 2） |
| `efinity_collect_logs` | ⬅️ 未实现 | 收集构建日志归档 |
| `board_list_uart_candidates` | ⬅️ 未实现 | 枚举 COM 口并分类 |
| `board_check_jtag_chain` | ⬅️ 未实现 | 探测 JTAG 链 |
| `board_uart_loopback_test` | ⬅️ 未实现 | UART 回环测试（低风险硬件） |
| `mycobot280_check_env` | ⬅️ 未实现 | 检查 myCobot 280 环境 |
| `mycobot280_list_ports` | ⬅️ 未实现 | 列出串口并标注 CP210x 候选 |
| `mycobot280_validate_connection` | ⬅️ 未实现 | 验证 myCobot 连接（只读） |
| `mycobot280_read_angles` | ⬅️ 未实现 | 读取关节角度（只读） |
| `mycobot280_set_rgb` | ⬅️ 未实现 | 设置 RGB 灯（低风险动作） |
| `mycobot280_read_coords` | ⬅️ 未实现 | 读取坐标（只读） |
| `mycobot280_plan_motion` | ⬅️ 未实现 | 规划动作（只读） |
| `mycobot280_execute_motion` | ⬅️ 未实现 | 执行动作（Phase 3） |
| `mycobot280_control_gripper` | ⬅️ 未实现 | 控制夹爪（Phase 3） |
| `mycobot280_stop` | ⬅️ 未实现 | 停止/释放（安全动作） |
| `mycobot280_export_session_log` | ⬅️ 未实现 | 导出会话日志 |
| `fpga_robot_write_review_packet` | ⬅️ 未实现 | 生成 Review Packet |

**入口点**：`python -m fpga_robot_mcp.server`（通过 `__main__` 保护调用 `mcp.run()`）

### 3.4 `efinity_tools.py`（我已完成部分工具）

| 函数 | 状态 | 行为 |
|------|------|------|
| `locate_toolchain()` | ✅ 可用 | 检查 `D:\Efinity\2025.2\bin\efx_*.exe` 存在性 |
| `check_install_prereq()` | ✅ 可用 | 检查 WinRAR、安装包、补丁脚本、磁盘空间 |
| `check_project()` | ✅ 可用 | 校验主工程 XML、SDC 约束、器件型号 |
| `list_artifacts()` | ⬅️ 未实现 | `Get-ChildItem outflow` 近似 |
| `run_build(project_xml, dry_run)` | ⬅️ 未实现 | 调用 `efx_run` 子进程 |
| `check_programmer()` | ⬅️ 未实现 | 检查 JTAG 下载线 |
| `program_bitstream(...)` | ⬅️ 未实现 | 调用 `efx_pgm` |
| `collect_logs()` | ⬅️ 未实现 | 日志复制归档 |

---

## 4. 留待经济模型填充（详细说明）

### 4.1 `serial_probe.py` — 串口枚举与分类

**估算工作量**：约 40 行 Python

**入口函数**：

```python
def list_ports() -> list[dict]:
    """
    枚举本机所有 COM 口，按类型分类标注。
    
    实现说明：
    1. 调用 serial.tools.list_ports.comports() 获取所有串口
    2. 对每个端口，检查 hwid / description 字段：
       - 包含 "10C4" 或 "CP210x" 或 "Silicon Labs" → type="cp210x" (myCobot 候选)
       - 包含 "VID_10C4" → type="cp210x"
       - 不包含上述 → 按描述判断：JTAG UART / Type-C UART / 蓝牙 / 其他
    3. 返回列表，每个元素格式：
       {
           "device": "COM3",
           "description": "Silicon Labs CP210x...",
           "hwid": "USB VID:PID=10C4:EA60...",
           "type": "cp210x" | "bluetooth" | "jtag_uart" | "type_c_uart" | "unknown",
           "is_mycobot_candidate": True/False
       }
    4. 如果 serial 模块未安装 → 返回带错误标记的列表
    
    成功示例返回值：
    [
        {"device": "COM4", "description": "Bluetooth...", "hwid": "BTHENUM...", 
         "type": "bluetooth", "is_mycobot_candidate": False},
        {"device": "COM3", "description": "Silicon Labs CP210x USB to UART Bridge", 
         "hwid": "USB VID:PID=10C4:EA60 SER=1234",
         "type": "cp210x", "is_mycobot_candidate": True}
    ]
    """
    raise NotImplementedError("serial_probe.list_ports() 未实现")
```

### 4.2 `mycobot_tools.py` — myCobot 280 环境工具

**估算工作量**：约 80 行 Python（5 个函数）

**需要实现的函数：**

| 函数 | 说明 | 关键 API |
|------|------|----------|
| `check_env()` | 检查 pyserial/pymycobot 是否可导入，Python 版本 | `importlib.util.find_spec()` |
| `list_ports()` | 遍历 COM 口，标注 myCobot 候选（调用 `serial_probe.list_ports()` 并过滤） | 调用已实现的 `list_ports` |
| `validate_connection(port, baudrate)` | 尝试 `pymycobot.MyCobot(port, baudrate)` 并读版本号 | `MyCobot(port, baudrate).get_robot_version()` |
| `read_angles(port, baudrate)` | 调用 `get_angles()` 返回 6 关节角度 | `MyCobot.get_angles()` |
| `read_coords(port, baudrate)` | 调用 `get_coords()` 返回坐标 | `MyCobot.get_coords()` |
| `set_rgb(port, baudrate, r, g, b)` | 设置 RGB 灯板颜色 | `MyCobot.set_color(r, g, b)` |

**注意事项**：
- 所有函数必须先检查 pymycobot 是否可导入，不可用时应返回友好消息而非崩溃。
- `validate_connection` 必须包含超时保护，默认超时 5 秒。
- `read_angles` / `read_coords` 失败时应给出具体排查建议（端口、驱动、供电、波特率）。
- `set_rgb` 需要 `confirm_token` 参数，且在调用前验证 token。

**read_angles 函数的 docstring（参考）：**

```python
def read_angles(port: str, baudrate: int = 1000000) -> dict:
    """
    读取 myCobot 280 的 6 个关节角度。
    
    参数:
        port: COM 口名称，如 "COM3"
        baudrate: 波特率，默认 1000000
    
    返回:
        {
            "status": "ok" | "error",
            "angles": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0] | None,
            "message": "..."
        }
    
    失败原因排查:
    - 端口不存在 → 提示检查 USB-TTL 连接
    - 端口存在但连接失败 → 提示检查驱动(CP210x)、供电
    - 超时 → 提示检查波特率(1000000)和机械臂电源
    """
    raise NotImplementedError("mycobot_tools.read_angles() 未实现")
```

### 4.3 `board_tools.py` — 开发板工具

**估算工作量**：约 50 行 Python

| 函数 | 说明 |
|------|------|
| `list_uart_candidates()` | 枚举开发板 UART 候选（结合 `serial_probe.list_ports()` 过滤蓝牙） |
| `check_jtag_chain()` | 探测 JTAG 链（调用 `efx_pgm --scan` 子进程解析输出） |
| `generate_uart_test_plan()` | 生成 UART 桥接测试计划文档 |

### 4.4 `review_packet.py` — Review Packet 模板

**估算工作量**：约 40 行 Python

```python
def generate_review_packet(context: dict) -> str:
    """
    根据当前环境状态生成 Review Packet Markdown。
    
    context 包含:
    - task: str — 任务目标
    - action_type: str — 动作类型
    - config: dict — 当前配置
    - status: dict — 环境状态
    - hardware_state: dict — 硬件状态
    - risks: list[str] — 风险项
    - questions: list[str] — 希望 Codex 判断的问题
    
    返回: 格式化的 Markdown 字符串（按 v0.2 §Review Packet 模板）
    """
    raise NotImplementedError("review_packet.generate_review_packet() 未实现")
```

### 4.5 单元测试

**估算工作量**：约 60 行 Python

| 文件 | 测试目标 |
|------|---------|
| `test_serial_probe.py` | `list_ports()` 返回格式正确、不崩溃、端口类型分类正确 |
| `test_efinity_probe.py` | `locate_toolchain()` 返回路径存在性检查结果 |
| `test_tool_entrypoints.py` | 每个工具函数可调用并返回字典 |

---

## 5. 如何调用经济模型填充

### 5.1 推荐模型

- **Claude Haiku 4.5**（快速、低成本）或
- **本地模型**（Ollama + Qwen2.5 7B 或类似）
- 需要模型理解：Python、pyserial、pymycobot、`os.path` / `pathlib`

### 5.2 调用协议

经济模型只需要做以下操作：

1. **读取本实施策划**（特别是第 4 节）
2. **读取目标文件**（如 `serial_probe.py`）— 函数签名和 docstring 已写好
3. **填充函数体** — 将 `raise NotImplementedError(...)` 替换为实际实现
4. **读取 `server.py` 中的 `_safe_call` 包装器** — 理解返回值格式
5. **不要修改已有的函数签名、docstring、导入结构**
6. **不要添加新的外部依赖** — 只使用 `pyserial`、`pymycobot`、`pydantic`、标准库
7. **遵循已有错误处理模式** — 使用 `try/except` 返回 `{"status": "error", "message": ...}`

### 5.3 验证步骤

每填充一个模块后，运行：

```powershell
D:\miniconda\Scripts\conda.exe run -n pfmval_py310 python -c "from fpga_robot_mcp.serial_probe import list_ports; print(list_ports())"
```

如果 MCP 服务器正在运行，重启后测试：

```powershell
# 从命令行启动测试
D:\miniconda\Scripts\conda.exe run -n pfmval_py310 python -m fpga_robot_mcp.server
```

然后在另一个终端用 Claude Code 调用工具，或运行 pytest：

```powershell
D:\miniconda\Scripts\conda.exe run -n pfmval_py310 python -m pytest tests/ -v
```

---

## 6. 注册到 Claude Code

### 6.1 项目级注册（推荐）

创建 `.claude/settings.json`（如不存在）：

```json
{
  "mcpServers": {
    "fpga_robot_mcp": {
      "command": "D:\\miniconda\\Scripts\\conda.exe",
      "args": ["run", "-n", "pfmval_py310", "python", "-m", "fpga_robot_mcp.server"],
      "env": {
        "PYTHONPATH": "${workspaceRoot}\\final_project\\tools\\mcp\\fpga_robot_mcp\\src",
        "FPGA_ROBOT_MCP_CONFIG": "${workspaceRoot}\\final_project\\tools\\mcp\\fpga_robot_mcp\\configs\\fpga_robot.local.json"
      }
    }
  }
}
```

**注意**：`${workspaceRoot}` 是 Claude Code 自动替换的工作区根目录变量，不依赖硬编码路径。

如果 Claude Code 不支持 `${workspaceRoot}`，则使用绝对路径：

```json
{
  "mcpServers": {
    "fpga_robot_mcp": {
      "command": "D:\\miniconda\\Scripts\\conda.exe",
      "args": ["run", "-n", "pfmval_py310", "python", "-m", "fpga_robot_mcp.server"],
      "env": {
        "PYTHONPATH": "D:\\第十届集创赛-雄芯院材料\\final_project\\tools\\mcp\\fpga_robot_mcp\\src",
        "FPGA_ROBOT_MCP_CONFIG": "D:\\第十届集创赛-雄芯院材料\\final_project\\tools\\mcp\\fpga_robot_mcp\\configs\\fpga_robot.local.json"
      }
    }
  }
}
```

### 6.2 pip install -e 安装（备选）

避免每次设置 PYTHONPATH，可以直接安装为包：

```powershell
D:\miniconda\Scripts\conda.exe run -n pfmval_py310 pip install -e final_project\tools\mcp\fpga_robot_mcp
```

安装后注册文件中的 PYTHONPATH 可移除：

```json
{
  "mcpServers": {
    "fpga_robot_mcp": {
      "command": "D:\\miniconda\\Scripts\\conda.exe",
      "args": ["run", "-n", "pfmval_py310", "python", "-m", "fpga_robot_mcp.server"],
      "env": {
        "FPGA_ROBOT_MCP_CONFIG": "D:\\第十届集创赛-雄芯院材料\\final_project\\tools\\mcp\\fpga_robot_mcp\\configs\\fpga_robot.local.json"
      }
    }
  }
}
```

### 6.3 验证注册成功

重启 Claude Code 后，在对话中输入：

```
你的 MCP 工具有哪些？
```

Claude 应列出所有 `fpga_robot_*`、`efinity_*`、`board_*`、`mycobot280_*` 工具。

或者直接调用：

```
请运行 fpga_robot_status 查看环境状态
```

---

## 7. 队友分享方案

### 7.1 队友已具备的条件

假设队友已完成 Phase -1 + Phase 0 安装（与第一台机器一致）：

| 组件 | 状态 | 备注 |
|------|------|------|
| WinRAR / 解压工具 | ✅ 已安装 | 解压 Efinity RAR 包 |
| VC++ 运行库 | ✅ 系统已装 | Efinity 安装前提 |
| Efinity 2025.2 | ✅ 已安装 | 安装到 `D:\Efinity\2025.2\` |
| efxdevicedb.ini | ✅ 已复制 | 设备数据库 |
| 补丁 v288.3.8 + v288.4.15 | ✅ 已打 | 手动复制或 run.bat |
| JTAG Bridge bitstream | ✅ 已复制 | 烧录 JTAG 链必需 |
| FT4232 驱动 | ✅ 已安装 | libusb-win32 / oem105.inf |
| RISC-V IDE | ✅ 已安装 | `D:\Efinity\efinity-riscv-ide-2025.2\` |
| Python 3.10 环境 | ✅ 已创建 | `pfmval_py310` 或 `fpga_mcp_py310` |
| mcp/pyserial/pymycobot | ✅ 已安装 | pip install 完成 |
| EFINITY_HOME 环境变量 | ✅ 已设置 | 用户级 `D:\Efinity\2025.2\` |

### 7.2 分享步骤（逐条）

**第 1 步：commit & push 代码**

```powershell
git add final_project/tools/mcp/fpga_robot_mcp/
git add 方案评审/002_机械臂MCP构建实施方案/机械臂MCP构建实施方案_Phase1_实施策划.md
git commit -m "feat(fpga_robot_mcp): Phase 1 骨架 — 配置/安全/服务器/首批只读工具"
git push
```

**第 2 步：队友 clone/pull**

```powershell
git pull
```

**第 3 步：安装 MCP 包（二选一）**

选项 A（推荐）：开发模式安装，代码修改即时生效：

```powershell
D:\miniconda\Scripts\conda.exe run -n pfmval_py310 pip install -e final_project\tools\mcp\fpga_robot_mcp
```

选项 B：纯路径注册（不安装，省一步）：

```powershell
# 依赖 PYTHONPATH 环境变量指向 src 目录
```

**第 4 步：复制配置模板**

```powershell
copy final_project\tools\mcp\fpga_robot_mcp\configs\fpga_robot.local.example.json ^
     final_project\tools\mcp\fpga_robot_mcp\configs\fpga_robot.local.json
```

如果队友的安装路径与本机不一致，修改 `fpga_robot.local.json` 中的路径。

**第 5 步：注册到 Claude Code**

在项目根 `.claude/settings.json`（不存在则创建）中添加：

```json
{
  "mcpServers": {
    "fpga_robot_mcp": {
      "command": "D:\\miniconda\\Scripts\\conda.exe",
      "args": ["run", "-n", "pfmval_py310", "python", "-m", "fpga_robot_mcp.server"],
      "env": {
        "FPGA_ROBOT_MCP_CONFIG": "D:\\队友本机路径\\final_project\\tools\\mcp\\fpga_robot_mcp\\configs\\fpga_robot.local.json"
      }
    }
  }
}
```

**第 6 步：验证**

```powershell
# 方法 1：命令行直接启动
D:\miniconda\Scripts\conda.exe run -n pfmval_py310 python -m fpga_robot_mcp.server

# 方法 2：重启 Claude Code，运行：
#   请运行 fpga_robot_status
#   请列出所有可用的 MCP 工具
```

### 7.3 队友间差异处理

| 差异项 | 处理方式 |
|--------|---------|
| Conda 环境名不同（如 `fpga_mcp_py310`） | 改 `.claude/settings.json` 中的 `-n` 参数 |
| 安装路径不同（如装到 `C:\Efinity`） | 改 `fpga_robot.local.json` 中的 `efinity.home` |
| Python 3.10 路径不同 | 改用绝对路径 `python.exe` 代替 conda run |
| 串口号不同 | MCP 工具自动枚举，不硬编码 |
| 蓝牙 COM 干扰 | MCP 工具自动分类，标注 non-candidate |
| 未连机械臂 | MCP 工具提示无 CP210x 端口，不崩溃 |
| 未连 JTAG | MCP 工具提示未检测到 JTAG 链，不崩溃 |

### 7.4 `.gitignore` 建议

确保以下文件不被提交：

```gitignore
# MCP 本地配置（含本机路径）
final_project/tools/mcp/fpga_robot_mcp/configs/fpga_robot.local.json

# MCP 日志
final_project/tools/mcp/fpga_robot_mcp/logs/

# Python 缓存
__pycache__/
*.pyc
*.egg-info/
```

### 7.5 协作工作流

```
[队友A: 本机] git commit → git push
[仓库] main 分支
[队友B: 其他机] git pull → 验证工具可用 → 开发自己的部分

MCP 配置独立于 git（.local.json 不提交），每个人只需维护自己的配置。
代码修改通过 PR/直接 push 共享。MCP 服务的 API 不变，底层实现可独立改进。
```

---

## 8. 验证清单

| 验证项 | 命令/操作 | 预期结果 |
|--------|----------|---------|
| 包可导入 | `python -c "from fpga_robot_mcp import config; print(config.__doc__)"` | 不报 ImportError |
| 配置可加载 | `python -c "from fpga_robot_mcp.config import get_default_config; print(get_default_config().efinity.home)"` | 输出 D:\Efinity\2025.2 |
| 安全门控基础 | `python -c "from fpga_robot_mcp.safety import SafetyManager, SafetyLevel; s=SafetyManager(...)"` | 不报错 |
| 服务器可启动 | `python -m fpga_robot_mcp.server --help 2>&1` | FastMCP 正常初始化 |
| 工具可发现 | 重启 CC → "列出你的 MCP 工具" | 看到 fpga_robot_status 等 |
| efinity_locate_toolchain | 调用该工具 | 返回 efx_pgm/efx_run 路径 |
| fpga_robot_status | 调用该工具 | 返回完整环境摘要 |
| pytest 通过 | `python -m pytest tests/ -v` | 至少 test_config、test_safety 通过 |

---

## 9. 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| FastMCP API 变化 | 服务器无法启动 | 使用 mcp 1.28.1 固定版本；`pyproject.toml` 中写 `mcp>=1.28.0` |
| pip install -e 路径中文/空格 | 安装失败 | conda run + 引号包裹；路径已包含中文，已验证 WSL Git Bash 路径处理 |
| 队友 conda 环境名不同 | 注册文件不通用 | 文档中给出多种方案注册方式 |
| 队友未装 pymycobot | myCobot 工具崩溃 | 所有 myCobot 工具先检查 import 可用性 |
| Claude Code 不支持 `${workspaceRoot}` | 注册时路径需写死 | 提供写死绝对路径的备选注册 JSON |

---

## 10. 下一步（Phase 1 执行顺序）

1. **Phase 1 策划方案定稿**（本文件） ✅
2. **MCP 骨架写入仓库**（本文件配套的代码文件） ← **当前步骤**
3. **经济模型填充** `serial_probe.py`、`mycobot_tools.py`、`board_tools.py`、`review_packet.py`
4. **注册到 `.claude/settings.json`**
5. **重启 Claude Code 验证**
6. **通知队友 pull 代码**
7. **队友按第 7 节步骤各自注册**
8. **Phase 1 完成 → 进入 Phase 2**
