# fpga_robot_mcp Phase 2B Bitstream Build 实施记录

日期：2026-07-09
执行者：Codex
范围：`final_project/tools/mcp/fpga_robot_mcp/` 与 `final_project/fpga/efinity/`
状态：MCP 已可调用 Efinity 2025.2 生成当前正式工程 bit/hex，并已在板卡上电后完成新 bit 的 JTAG 烧录验证。

## 背景

用户准备烧录当前本地最新 FPGA 程序。旧烧录产物位于：

```text
D:\final_project_shaolu\fpga\efinity\outflow\mem_test.bit
```

旧产物早于当前正式工程 RTL/工程文件，因此本阶段目标是让 `fpga_robot_mcp` 直接驱动 Efinity 对正式工程生成新的 bitstream。

正式工程入口：

```text
final_project/fpga/efinity/mem_test.xml
```

## MCP 修改

### 1. Efinity build 命令

`efinity_tools.run_build()` 现在支持：

```python
run_build(
    project_xml="",
    dry_run=True,
    flow="compile",
    output_dir="",
    work_dir="",
    timeout_seconds=1800,
    disable_debug=False,
)
```

默认 `flow="compile"`。该 flow 会运行 `map -> interface -> pnr -> pgm` 并生成 bit/hex，不跑仿真。不要默认使用 `flow="full"`，因为 Efinity 2025.2 的 `full` 会先跑 RTL/map/PNR sim。

实际命令格式已修正为：

```text
cd /d "<project_dir>" && call "D:\Efinity\2025.2\bin\setup.bat" >nul && "D:\Efinity\2025.2\bin\efx_run.bat" "mem_test.xml" "--prj" "-f" "compile"
```

同时清理了 Efinity 子进程环境中的 `PYTHONPATH` / `PYTHONHOME`，避免 MCP 自身 Python 包路径污染 Efinity 内置 Python。

### 2. Debug profile 旁路

当前 `mem_test.xml` 包含 debugger profile。直接 `compile` 会进入 `efx_run_dbg.py`，因缺少 `work_dbg/debug_top.v` 失败。

新增 `disable_debug=True`：构建时临时生成同目录 `mem_test.codex_nodebug.xml`，只在临时 XML 中设置：

```text
debugger auto_instantiation = off
debugger profile = NONE
synthesis enable-mark-debug = 0
```

真实工程 `mem_test.xml` 不写入这些功能性改动；临时 XML 在构建结束后由 MCP 自动清理。Efinity 自身运行时可能会更新 `mem_test.xml` 的 `last_change` 时间戳，这是工具副作用，不代表工程功能设置变更。

### 3. MCP 服务层入口

`server.efinity_run_build()` 已暴露同样参数：

```python
efinity_run_build(
    project_xml="",
    dry_run=True,
    flow="compile",
    output_dir="",
    work_dir="",
    timeout_seconds=1800,
    disable_debug=False,
)
```

`dry_run=True` 可只做预检查；`dry_run=False` 属于写文件/生成产物操作，需用户授权后执行。

## 路径结论

直接在中文路径 `D:\第十届集创赛-雄芯院材料\...` 下运行 Efinity 会报：

```text
filesystem error: Cannot convert character sequence: Illegal byte sequence
```

使用 ASCII junction：

```text
D:\cicc_cbm_link
```

指向当前仓库后，Efinity CLI 可以稳定运行。因此自动构建推荐使用：

```text
D:\cicc_cbm_link\final_project\fpga\efinity\mem_test.xml
```

## 已验证构建

MCP 调用参数：

```python
server.efinity_run_build(
    project_xml=r"D:\cicc_cbm_link\final_project\fpga\efinity\mem_test.xml",
    dry_run=False,
    flow="compile",
    disable_debug=True,
    timeout_seconds=3600,
)
```

结果：

```text
return_code = 0
map       = PASS
interface = PASS
pnr       = PASS
pgm       = PASS
```

证据日志：

```text
final_project/docs/evidence/build_compile_20260709_203027.log
```

生成产物：

```text
final_project/fpga/efinity/outflow/mem_test.bit  size=11683743  time=2026-07-09 20:30:24
final_project/fpga/efinity/outflow/mem_test.hex  size=11683803  time=2026-07-09 20:30:26
```

`mem_test.pgm.out` 确认：

```text
Generating bit file: "outflow\mem_test.bit"
Finished generating "outflow\mem_test.hex"
```

## 报告摘要

Timing：

```text
setup/max path min slack = 1.296 ns
hold/min path min slack  = 0.018 ns
negative slack count     = 0
```

CDC：

```text
No Synchronizer warnings to report.
```

PNR：

```text
Elapsed time for Latest PnR Run: 0 hours 1 minutes 45 seconds
```

## 单测

已运行：

```powershell
$env:PYTHONPATH='final_project\tools\mcp\fpga_robot_mcp\src'
python final_project\tools\mcp\fpga_robot_mcp\tests\test_efinity_probe.py
python final_project\tools\mcp\fpga_robot_mcp\tests\test_tool_entrypoints.py
```

最新结果：

```text
test_efinity_probe.py = 23/23 passed
test_tool_entrypoints.py = 24/24 passed
```

## 上板烧录验证

板卡未上电时曾出现一次 Efinity 返回码为 0 但 JTAG Device ID 为全 1 的可疑日志：

```text
Device ID read from JTAG: 0xFFFFFFFF
```

该情况不能算有效烧录。MCP 已补充判定：JTAG 模式下仅返回码为 0 不够，还必须解析到非 `0xFFFFFFFF` / `0x00000000` 的 Device ID，并看到 `finished with JTAG programming`。

板卡上电后，只读扫描恢复正常：

```text
status = ok
ready = True
idcode = ["0x006A0EF3"]
ir_width = [5]
url = ftdi://0x0403:0x6011:2:1a/2
```

真实烧录结果：

```text
bitstream = final_project/fpga/efinity/outflow/mem_test.bit
return_code = 0
duration = 17.5s
Device ID read from JTAG: 0x006A0EF3
... finished with JTAG programming
```

证据日志：

```text
final_project/docs/evidence/program_20260709_204902.log
```

## 后续 loop 验证路径

1. 只读确认下载器：

```python
server.efinity_check_programmer()
```

2. 对新 bit 做烧录 dry-run：

```python
server.efinity_program_bitstream(
    bitstream_path=r"D:\第十届集创赛-雄芯院材料\final_project\fpga\efinity\outflow\mem_test.bit",
    mode="jtag",
    dry_run=True,
)
```

3. 用户明确授权后真实烧录：

```python
server.efinity_program_bitstream(
    bitstream_path=r"D:\第十届集创赛-雄芯院材料\final_project\fpga\efinity\outflow\mem_test.bit",
    mode="jtag",
    dry_run=False,
    confirm_token="I_CONFIRM_EFINITY_PROGRAM_YYYYMMDD",
)
```

4. 烧录成功后再进入运行日志抓取、代码修复、再次 build/burn 的闭环。
