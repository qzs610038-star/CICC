# fpga_robot_mcp 队友 Agent 配置与闭环测试 SOP

适用场景：队友从云端 `main` 同步本仓库后，在自己的 Windows 设备上配置 `fpga_robot_mcp`，让 Agent 能执行 Efinity 构建、JTAG 烧录、日志收集和后续修复迭代。

当前能力边界：

- 已实现并实测：Efinity `compile` 构建生成 `bit/hex`、FTDI/JTAG 只读探测、JTAG 烧录、构建/烧录日志落盘。
- 已加安全判定：JTAG 烧录不能只看 `return_code=0`，必须看到有效 `Device ID` 和 `finished with JTAG programming`。
- 尚未封装成单一总控工具：完整 loop 由 Agent 按本 SOP 调用多个 MCP 工具完成。
- 本 MCP 只用于开发期辅助，不进入比赛正式闭环。
- 兼容保留：历史脚本 `final_project/tools/sync_to_burn_dir.ps1` 已共享给队友，仍可作为“复制到独立英文目录后构建/烧录”的旧流程。不要删除或重命名该脚本。

## 0. 路径变量约定

本文用变量标注本机 D 盘一级目录。队友设备的仓库目录不一定等于本机示例路径，仓库内部相对路径基本相同。下表第三列只是本机示例，不可直接照抄到队友设备。

| 变量 | 含义 | 本机示例，不可照抄 |
| --- | --- | --- |
| `<REPO_ROOT>` | 队友真实仓库根目录 | `D:\第十届集创赛-雄芯院材料` |
| `<ASCII_REPO_ROOT>` | 指向 `<REPO_ROOT>` 的英文/ASCII junction | `D:\cicc_cbm_link` |
| `<BURN_PROJECT_ROOT>` | 旧同步脚本使用的独立英文烧录目录 | `D:\final_project_shaolu` |
| `<EFINITY_HOME>` | Efinity 安装根目录 | `D:\Efinity\2025.2` |
| `<CONDA_EXE>` | 可选 Conda 可执行文件路径 | `D:\miniconda\Scripts\conda.exe` |

在 JSON 中使用 Windows 路径时请写成双反斜杠，或直接使用 `/`：

```json
"project_root": "D:/cicc_cbm_link"
```

实际配置时，把 `D:/cicc_cbm_link` 替换成队友自己的 `<ASCII_REPO_ROOT>`。

## 1. 同步后的人类一次性准备

### 1.1 拉取仓库

```powershell
git pull origin main
```

以下示例假设真实仓库路径为 `<REPO_ROOT>`。本机参考值是：

```text
<REPO_ROOT>
```

如果队友路径不同，替换为自己的真实仓库路径；后续 `final_project/...` 等仓库内相对路径保持不变。

### 1.2 创建 ASCII junction

Efinity 2025.2 CLI 在中文路径下可能报：

```text
filesystem error: Cannot convert character sequence: Illegal byte sequence
```

因此不要让 Efinity 直接看到中文路径。创建一个 ASCII 路径指向真实仓库：

```powershell
New-Item -ItemType Junction `
  -Path "<ASCII_REPO_ROOT>" `
  -Target "<REPO_ROOT>"
```

检查：

```powershell
Get-Item "<ASCII_REPO_ROOT>" | Format-List FullName,LinkType,Target
```

后续 Efinity 构建入口统一使用：

```text
<ASCII_REPO_ROOT>\final_project\fpga\efinity\mem_test.xml
```

### 1.3 安装 Python 包

进入 MCP 目录：

```powershell
cd "<ASCII_REPO_ROOT>\final_project\tools\mcp\fpga_robot_mcp"
python -m pip install -e .
```

如使用 Conda 环境，先激活对应环境，或在 MCP 注册时用 `conda run -n <env_name>`。

### 1.4 配置本机 JSON

复制模板：

```powershell
Copy-Item configs\fpga_robot.local.example.json configs\fpga_robot.local.json
```

编辑 `configs\fpga_robot.local.json`，至少确认：

```json
{
  "project_root": "D:/cicc_cbm_link",
  "efinity": {
    "home": "D:/Efinity/2025.2",
    "bin": "D:/Efinity/2025.2/bin",
    "project_xml": "final_project/fpga/efinity/mem_test.xml",
    "constrain_sdc": "final_project/fpga/efinity/constrain.sdc",
    "device": "TJ375N529"
  },
  "programmer": {
    "setup_bat": "D:/Efinity/2025.2/bin/setup.bat",
    "ftdi_program_py": "D:/Efinity/2025.2/pgm/bin/efx_pgm/ftdi_program.py",
    "default_mode": "jtag",
    "require_jtag_idcode": true
  },
  "safety": {
    "allow_hardware_actions": false,
    "require_confirm_token": true
  }
}
```

上面 JSON 中的 `D:/cicc_cbm_link` 和 `D:/Efinity/2025.2` 是本机示例；队友应替换为自己的 `<ASCII_REPO_ROOT>` 与 `<EFINITY_HOME>`。

注意：

- `configs/fpga_robot.local.json` 是本机私有配置，已被 `.gitignore` 排除，不要提交。
- 只做 dry-run 或构建前，`allow_hardware_actions=false` 即可。
- 需要真实烧录时，可临时改成 `true`，烧完再改回 `false`。

## 2. MCP 注册

### 2.1 通用 stdio 启动命令

```powershell
$env:FPGA_ROBOT_MCP_CONFIG="<ASCII_REPO_ROOT>\final_project\tools\mcp\fpga_robot_mcp\configs\fpga_robot.local.json"
python -m fpga_robot_mcp.server
```

如果能启动且不报 import/config 错误，说明本地服务入口可用。

### 2.2 Agent 客户端注册示例

```json
{
  "mcpServers": {
    "fpga_robot_mcp": {
      "command": "python",
      "args": ["-m", "fpga_robot_mcp.server"],
      "env": {
        "FPGA_ROBOT_MCP_CONFIG": "D:\\cicc_cbm_link\\final_project\\tools\\mcp\\fpga_robot_mcp\\configs\\fpga_robot.local.json"
      }
    }
  }
}
```

上面 JSON 中的 `D:\\cicc_cbm_link` 是本机示例；队友应替换为自己的 `<ASCII_REPO_ROOT>`，并保留 JSON 所需的双反斜杠。

Conda 示例：

```json
{
  "mcpServers": {
    "fpga_robot_mcp": {
      "command": "D:\\miniconda\\Scripts\\conda.exe",
      "args": ["run", "-n", "<env_name>", "python", "-m", "fpga_robot_mcp.server"],
      "env": {
        "FPGA_ROBOT_MCP_CONFIG": "D:\\cicc_cbm_link\\final_project\\tools\\mcp\\fpga_robot_mcp\\configs\\fpga_robot.local.json"
      }
    }
  }
}
```

Conda 示例中的 `D:\\miniconda\\Scripts\\conda.exe` 也是本机示例；队友应替换为自己的 `<CONDA_EXE>`。

## 2.5 与旧同步脚本的关系

仓库中仍保留：

```text
final_project/tools/sync_to_burn_dir.ps1
```

它的作用是把当前 `final_project/` 同步到独立英文目录，默认目标：

```text
<BURN_PROJECT_ROOT>
```

历史用法：

```powershell
powershell -ExecutionPolicy Bypass -File .\final_project\tools\sync_to_burn_dir.ps1
powershell -ExecutionPolicy Bypass -File .\final_project\tools\sync_to_burn_dir.ps1 -DryRun
powershell -ExecutionPolicy Bypass -File .\final_project\tools\sync_to_burn_dir.ps1 -TargetPath "<BURN_PROJECT_ROOT>"
```

两条路径的定位：

```text
推荐新流程：<ASCII_REPO_ROOT> junction -> 直接在同一份仓库上构建/烧录
兼容旧流程：sync_to_burn_dir.ps1 -> 复制 final_project 到 <BURN_PROJECT_ROOT> 再构建/烧录
```

二者不是同一条工作流。队友如果继续使用旧脚本，Agent 必须显式把构建和烧录路径都指向同步目标，例如：

```python
efinity_run_build(
    project_xml=r"<BURN_PROJECT_ROOT>\fpga\efinity\mem_test.xml",
    dry_run=False,
    flow="compile",
    disable_debug=True,
    timeout_seconds=3600,
)

efinity_program_bitstream(
    bitstream_path=r"<BURN_PROJECT_ROOT>\fpga\efinity\outflow\mem_test.bit",
    mode="jtag",
    dry_run=False,
    confirm_token="I_CONFIRM_EFINITY_PROGRAM_YYYYMMDD",
)
```

不要在同一次 loop 中“用 junction 构建、用 `<BURN_PROJECT_ROOT>` 烧录”，除非已经明确同步过且确认 bit 时间戳/大小一致。

旧同步脚本的风险点：

- 默认 `/E` 会复制/更新文件但保留目标目录已有生成物；`<BURN_PROJECT_ROOT>` 可能残留旧 `outflow/`。
- `-Mirror` 会镜像删除目标侧多余文件，谨慎使用。
- 当前脚本会同步 `outflow/`、`work_syn/`、`work_pnr/` 等生成目录；Agent 选择 bit 时必须看路径、时间戳、大小和来源。
- 如果在 `<BURN_PROJECT_ROOT>` 手工改代码，这些改动不会自动回到 Git 仓库，容易形成双副本分歧。

当前 MCP 的 `efinity_list_artifacts()` 会同时列出正式工程和 `burn_project_root` 的 outflow，来源字段分别是：

```text
configured_project
burn_project
```

Agent 选择烧录文件时必须显式指定 `bitstream_path`，不要只拿列表中第一个 `.bit`。

## 3. Agent 启动后的健康检查

Agent 连接 MCP 后，先按顺序调用：

```python
fpga_robot_status()
efinity_locate_toolchain()
efinity_check_project()
```

最低验收：

```text
Efinity home/bin/setup.bat/efx_run.bat 存在
project_xml 存在
constrain_sdc 存在
device = TJ375N529
```

如果 `project_xml` 仍显示中文真实路径，优先检查 `project_root` 是否设置为 `<ASCII_REPO_ROOT>`，以及 `FPGA_ROBOT_MCP_CONFIG` 是否指向正确配置文件。

## 4. 完整闭环测试

### 4.1 构建 dry-run

```python
efinity_run_build(
    project_xml=r"<ASCII_REPO_ROOT>\final_project\fpga\efinity\mem_test.xml",
    dry_run=True,
    flow="compile",
    disable_debug=True,
)
```

验收：

```text
status = ok
flow = compile
command 中包含 <ASCII_REPO_ROOT>
command 中包含 efx_run.bat "mem_test.codex_nodebug.xml" "--prj" "-f" "compile"
```

### 4.2 真实构建 bit/hex

真实构建会写 `outflow/`，需要用户授权 Agent 执行写文件操作。

```python
efinity_run_build(
    project_xml=r"<ASCII_REPO_ROOT>\final_project\fpga\efinity\mem_test.xml",
    dry_run=False,
    flow="compile",
    disable_debug=True,
    timeout_seconds=3600,
)
```

验收：

```text
return_code = 0
map       = PASS
interface = PASS
pnr       = PASS
pgm       = PASS
```

再调用：

```python
efinity_list_artifacts(limit=10)
```

确认最新产物包含：

```text
final_project/fpga/efinity/outflow/mem_test.bit
final_project/fpga/efinity/outflow/mem_test.hex
```

当前本机已验证过的参考构建结果：

```text
mem_test.bit size = 11683743
setup/max path min slack = 1.296 ns
hold/min path min slack  = 0.018 ns
negative slack count     = 0
CDC = No Synchronizer warnings to report.
```

### 4.3 JTAG 只读检查

给 FPGA 板卡上电并连接下载器后：

```python
efinity_check_programmer()
```

有效验收：

```text
status = ok
ready_for_jtag_program = True
idcode 包含 0x006A0EF3
recommended_url 类似 ftdi://0x0403:0x6011:2:<bus>/2
```

若出现以下情况，不要烧录：

```text
idcode = []
idcode = 0xFFFFFFFF
idcode = 0x00000000
```

这些通常表示板卡未上电、JTAG 链路浮空、接线/下载器通道不对，或电源/复位状态异常。

### 4.4 烧录 dry-run

```python
efinity_program_bitstream(
    bitstream_path=r"<ASCII_REPO_ROOT>\final_project\fpga\efinity\outflow\mem_test.bit",
    mode="jtag",
    dry_run=True,
)
```

验收：

```text
status = dry_run
programmer_ready = True
command 指向当前 mem_test.bit
```

### 4.5 真实烧录

真实烧录前确认：

- 板卡已上电。
- `efinity_check_programmer()` 已读到 `0x006A0EF3`。
- `safety.allow_hardware_actions=true`，或由当前 Agent 会话明确临时授权。
- 使用当天 token，格式为 `I_CONFIRM_EFINITY_PROGRAM_YYYYMMDD`。

示例：

```python
efinity_program_bitstream(
    bitstream_path=r"<ASCII_REPO_ROOT>\final_project\fpga\efinity\outflow\mem_test.bit",
    mode="jtag",
    dry_run=False,
    confirm_token="I_CONFIRM_EFINITY_PROGRAM_YYYYMMDD",
)
```

有效验收必须同时满足：

```text
status = ok
return_code = 0
valid_device_ids 包含 0X006A0EF3
jtag_finished = True
```

证据日志也必须包含：

```text
Return code: 0
Device ID read from JTAG: 0x006A0EF3
... finished with JTAG programming
```

如果日志里是 `0xFFFFFFFF`，即便 Efinity 返回 0，也不能算成功。MCP 应返回 `program_suspicious`。

### 4.6 日志收集与修复迭代

```python
efinity_collect_logs()
```

Agent 接下来读取：

```text
final_project/docs/evidence/build_*.log
final_project/docs/evidence/program_*.log
final_project/fpga/efinity/outflow/*.rpt
```

然后按实际失败原因修代码，再重复：

```text
修改代码 -> efinity_run_build -> efinity_check_programmer -> efinity_program_bitstream -> collect/read logs
```

## 5. 常见故障与处理

### 5.1 中文路径报 Illegal byte sequence

现象：

```text
filesystem error: Cannot convert character sequence: Illegal byte sequence
```

处理：

- 确认 junction 存在。
- `project_root` 使用 `<ASCII_REPO_ROOT>`。
- `efinity_run_build(project_xml=...)` 使用 `<ASCII_REPO_ROOT>\...`。

### 5.2 `full` flow 先跑仿真

不要默认使用：

```text
flow = full
```

本工程生成 bit/hex 使用：

```text
flow = compile
```

### 5.3 Debug profile 导致 `work_dbg/debug_top.v` 缺失

构建时带：

```python
disable_debug=True
```

MCP 会临时生成 `mem_test.codex_nodebug.xml`，构建结束后清理，不改原 `mem_test.xml` 的 debugger 配置。

### 5.4 USB 可见但 JTAG ID 为空

现象：

```text
usb_visible = True
jtag_idcode_visible = False
```

处理：

- 确认板卡已上电。
- 重新插拔下载线。
- 检查 FTDI URL 是否来自 `recommended_url`。
- 不要直接绕过预检烧录。

### 5.5 Efinity 返回 0 但 Device ID 是全 F

现象：

```text
Device ID read from JTAG: 0xFFFFFFFF
```

判定：无效烧录。通常是板卡未上电或 JTAG 链路浮空。

处理：上电后重新 `efinity_check_programmer()`，必须读到 `0x006A0EF3` 再烧录。

## 6. 推荐给队友 Agent 的开场提示

队友可以把下面这段发给自己的 Agent：

```text
我已从 main 同步 <REPO_ROOT>，请按
final_project/tools/mcp/fpga_robot_mcp/docs/TEAMMATE_AGENT_SETUP.md
配置 fpga_robot_mcp。先创建/检查 <ASCII_REPO_ROOT> junction，复制并编辑
configs/fpga_robot.local.json，注册 MCP 后执行健康检查。构建时使用
efinity_run_build(flow="compile", disable_debug=True)，project_xml 必须走
<ASCII_REPO_ROOT>。烧录前必须 efinity_check_programmer 读到 0x006A0EF3；
烧录成功不能只看 return_code=0，还要验证 Device ID 和 finished 标志。
```
