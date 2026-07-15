# myCobot 上板 Goal 执行记录（2026-07-15）

> Goal：按 `mycobot_arm_board_control_advancement_plan_20260715.md` §13 推进阶段 0、B1、B3 与 A0；本记录只登记本次新鲜证据。

## 0. 会话健康与保护

- `tools/agent_handoff_health_check.ps1 -Handoff C:\Users\33696\.agents\handoff\codex-handoff-20260715_153346.json`：exit 0；一项 provenance absolute-path WARN，不含 FAIL。
- `git fetch --prune origin`：exit 0。
- `HEAD == origin/main == 64260b611a716e3c5d8a755a3d3e6c2b4cf15a04`；开始时已有 `.claude/settings.local.json`、技术计划 README、`.agents` 与推进方案等脏/未跟踪文件，未清理或覆盖。

## 1. B1–B3 纯软件结果

### 实现

- 新增无 UART/MMIO 的 `mycobot_transaction`，实现 750 ms deadline、single-flight、exact command/payload-length、域验证、超时与迟到/重复计数。
- 协议层明确官方 LEN 窗口、0x29/0x2B/0x69 命令、GET_ANGLES/夹爪/运动状态的精确返回长度，及 J1..J6 绝对限位。
- `SEND_ANGLES` 现在拒绝越界角度；不再静默饱和。
- B3 夹具验证 `FE FE 02 20 FA`、GET_ANGLES `LEN=0x0E`、LEN 2/16/1/17、六轴边界、错误命令/长度/域、超时、迟到/重复和 single-flight。

### 验证

```text
powershell -ExecutionPolicy Bypass -File final_project\cpu\tests\run_mycobot_arm_skeleton_host.ps1
exit=0
RESULT: PASS (QEMU asserts executed).
```

判定：B1/B3 软件子检查点 PASS；没有以此推断 G7、G8 或任何实机 Gate 已通过。

### B2 差分审计与 QEMU runner 修复

- 现有 transport 已拥有 happy/read failure/soft pass/retry success/retry failure 与 `set_settle_reads()`；不新增重复的“permanent fail”枚举。
- 新测试覆盖 N-poll 收敛、DONE 后下一请求、retry-failure/cancel 的 FAULT 保持以及 re-init 后 happy 恢复。
- `run_arm_runtime_qemu.ps1` 曾因 Windows PowerShell 将允许的 gcc stderr 提前终止、且 PS5 不支持 `Kill($true)` 留下 QEMU；runner 现在自行捕获 stderr、保留 warning allowlist，并按 PS 版本选择终止 API。

```text
powershell -ExecutionPolicy Bypass -File final_project\cpu\tests\run_arm_runtime_host.ps1
exit=0; HOST PASS backend=disabled; HOST PASS backend=simulated

powershell -ExecutionPolicy Bypass -File final_project\cpu\tests\run_arm_runtime_qemu.ps1 ...
exit=0; QEMU PASS backend=disabled; QEMU PASS backend=simulated; QEMU TIMEOUT PASS seconds=1
```

判定：B2 的纯软件 fail-closed 最小证据 PASS；它不等于 G5 的板上 simulated、G10 的动作前 fail-closed 实机证据或任何 UART2 证据。

## 2. 阶段 0 只读准备

- 本机 Python：`C:\Program Files\Python313\python.exe`；`pymycobot 4.0.5`。
- 已创建 `final_project/tools/mycobot_firmware_version_readonly.py`；仅允许显式端口的 `MyCobot280.get_system_version()`，脚本静态属性审计未发现动作、夹爪、舵机、电源、更新或刷写 API 调用。
- `COM10` 枚举为 `USB-SERIAL CH340`（VID:PID `1A86:7523`），但尚无设备身份/线缆证据证明它就是 myCobot，因此**没有向 COM10 或任何端口发送查询帧**。
- 固件版本仍为 `NOT_AVAILABLE`，待用户确认端口后才执行只读查询；不刷新固件。

### 阶段 0 执行结果（COM10 身份确认后）

```text
python final_project\tools\mycobot_firmware_version_readonly.py --port COM10
exit=0
mode=STAGE0_READ_ONLY
baudrate=1000000
permitted_api=MyCobot280.get_system_version
motion_or_firmware_api_called=false
system_version=7.3
query_status=OK
```

- 原始结构化证据：`final_project/docs/debug_sessions/evidence/mycobot_stage0_readonly_20260715_090247.json`。
- 端口身份由用户明确为 myCobot；HWID 为 `USB VID:PID=1A86:7523`，描述 `USB-SERIAL CH340 (COM10)`。
- 查询脚本 SHA-256：`490B7B8A1CBB5CFA4181919B9FC236C84653F3D5FBDEFC364A233D8D04E5B740`。
- 设备查询不能读出“已刷入文件 hash”，该项仍为 `NOT_AVAILABLE`；未执行固件刷新、动作、夹爪、扭矩或电源 API。
- 判定：**阶段 0 只读版本核验 PASS**；它不构成 A0/G4 或任一真实控制 Gate 的通过。

## 3. A0 只读预审

- 当前 `mem_test.peri.xml` 仍声明 `PLL_BL1=pll_inst1`、`PLL_BL0=lpddr4_pll`、`PLL_TR0=MIPI_TX_PLL`、`PLL_BL2=pll_inst4` 与 `JTAG_USER1=jtag_inst1`。
- 既有 GUI 审计 `a9_gui_pll_jtag_resource_audit_20260712.md` 记录：硬 SoC system PLL 仅可用 BL0/BL1/BL2，而三者均已被视频/DDR/MIPI 时钟链占用；JTAG_USER2 不能消除 PLL 冲突。
- 旧隔离树 `C:\fpga_soc_isolated\tj375_video_soc_gui_a8\fpga` 当前不存在，无法用 hash 证明它与当前工程相同。故旧 GUI 证据只可作为风险定位，**不能关闭本次 A0**。
- 本次未改 `mem_test.xml`、`.peri.xml`、`constrain.sdc` 或 `top.v`，未运行 Map/PNR。

### A0 当前源快照（GUI 前）

- GUI：`D:\Efinity\2025.2\bin\efinity.exe`，文件版本 `2025.2.288.4.15`。
- 器件/顶层：`TJ375N529` / `top`（来自当前 `mem_test.xml`）。

| 文件 | SHA-256 |
|---|---|
| `fpga/efinity/mem_test.xml` | `38A5BDDDAC2241853499AE37BC8F878463CD84382B09A0E2B877A1DE586240BB` |
| `fpga/efinity/mem_test.peri.xml` | `E765844AF0911D2AC330E27E1CDBD4F2B84E393B60D7E2558F7FC701679EA689` |
| `fpga/efinity/constrain.sdc` | `0431D74FDCDEF064ADFA24A9B138E758EE2A0DBE288091DD7DC0B7CEA38C5775` |
| `fpga/rtl/top/top.v` | `3787B89CAD7A5CEE096BE6E5168339F26CE19D3374701134D66532789E99E2E4` |

这些 hash 是隔离副本创建后的首项核对；若任何一个不一致，截图必须标注为不同批次，不能用于当前 A0。

## 4. A0 操作卡（等待 GUI 结果）

| 项目 | 内容 |
|---|---|
| 目标 | 在受控副本中让 Interface Designer 直接显示 system PLL、JTAG 和 pad 的合法资源组合；判定是否存在不破坏视频/DDR/MIPI 时钟链的硬 SoC 候选。 |
| 接线状态 | 机械臂、J52 Pin1–Pin4、外部 TX/VCC 全断开；不运行 Programmer。 |
| 操作 | 复制当前 `final_project/fpga` 为新的隔离目录；GUI 打开副本 `efinity/mem_test.xml`；分别截取 `pll_inst1/lpddr4_pll/MIPI_TX_PLL/pll_inst4` 的资源页、硬 SoC system PLL 合法资源页、JTAG 选择页及 UART0/pad 合法性页。只查看/使用 GUI 的合法候选，不手改 XML。 |
| 保存证据 | 将截图、Efinity 版本、隔离副本绝对路径与四个源文件 SHA-256 写回本文件的 A0 小节；若 GUI 允许候选，只保存新隔离候选，不覆盖正式树。 |
| STOP | GUI 未提供合法 system-PLL/pad 组合、任意候选要求改正式树、发现 pad/PLL 冲突、或准备 Map/PNR/Programmer 时停止并回传截图。 |

## 5. 下一步

1. 用户回传 A0 GUI 截图/资源页与明确端口身份后，先记录证据再判断 G4 或执行阶段 0 的只读版本查询。
2. 在 A0 未关闭前，不改 FPGA 工程、不跑联合 PNR、不产生 board artifact。
3. G5–G11 仍保持未开始；任何真实接线/动作遵循 §13.3。

## 6. G4 构建前交叉编译与制品边界审计

```text
D:\Efinity\efinity-riscv-ide-2025.2\toolchain\bin\riscv-none-embed-gcc.exe
  -march=rv32imac -mabi=ilp32 -mcmodel=medlow -O2 -g -Wall -Wextra -Werror
  -Ifinal_project\cpu\app\include -fsyntax-only
  final_project\cpu\app\src\mycobot_protocol.c
  final_project\cpu\app\src\mycobot_transaction.c
exit=0; RISCV_SYNTAX_PASS
```

- B1 的协议和事务实现可按目标 `rv32imac/ilp32` ABI 严格编译；此检查没有产生 ELF/HEX/BIN。
- 当前 `cpu/build_tools/build_arm_profile.ps1` 的 `-BoardBuild` 仍明确抛错，且其验证器要求 source/map/listing 中**不得**出现 `mycobot_protocol`、`mycobot_uart` 或 `uart2`。这是 G0–G3 的防误烧录护栏，不能被改称为 G4 builder。
- 当前源码检索仅在 B1 源和测试中命中 `mycobot_transaction`/`mycobot_protocol`；它们尚未进入可烧录构建图。因此本项是 G4 的准备证据，**不是**同批次 SoC/BSP/ELF/bitstream、PNR/STA 或 CPU Hello 的证据。

## 7. G4 同批次证据契约预检

- 新增 `final_project/tools/verify_mycobot_g4_batch.py`。它只读取一个 JSON manifest 及其所列文件；不打开 Efinity、不访问串口、不运行 Programmer、不创建 FPGA/CPU 制品。
- 它要求同一 manifest 记录并逐一 SHA-256 核验：project/periphery/SDC/top、`soc.h`/linker/startup、ELF/bitstream、map/PNR/STA 日志和三次独立复位的 UART0 日志；还要求日志内的 PASS 断言、每次 `CPU HELLO` 与相同 `build_id`。
- 对 BSP/startup/linker 强制拒绝 `STANDALONE_TEST`、`0xF0000000` 与 `APB_VISION_BASE_PLACEHOLDER`，以免 G0–G3 临时输入被误入 G4。

```text
python -m py_compile final_project\tools\verify_mycobot_g4_batch.py \
  final_project\tools\tests\test_verify_mycobot_g4_batch.py
python -m unittest final_project.tools.tests.test_verify_mycobot_g4_batch -v
exit=0; 2 tests PASS
```

实际 A3 批次完成后，执行：

```text
python final_project\tools\verify_mycobot_g4_batch.py \
  --manifest <G4 批次 manifest.json> \
  --report final_project\docs\debug_sessions\evidence\mycobot_g4_batch_verification.json
```

只有该命令 exit 0、且人工复核原始 GUI/Programmer/PNR/STA/UART0 证据后，才可把 G4 标为 PASS。当前没有真实 manifest，故本项只关闭“证据采集工具准备”，**G4 仍未通过**。

## 8. UART0 CPU Hello 的 build-id 可追溯性

- `arm_build_profile.h` 增加 `ARM_BUILD_ID` 的 fail-closed 默认值 `UNPROVISIONED_NOT_FOR_FLASH`；`arm_bringup_main.c` 的启动 banner 现在形如 `[ARM_BRINGUP] CPU HELLO build=<id> profile=arm_bringup backend=...`。
- G0–G3 构建器在 compile flags 中注入其 manifest 的 build id；它仍拒绝 `-BoardBuild`，且输出中继续明确 `NOT_FOR_FLASH`。未来 G4 builder 必须使用真实同批次 id 替换该标识，不能复用 G0–G3 制品。

```text
powershell -ExecutionPolicy Bypass -File final_project\cpu\build_tools\build_arm_profile.ps1 \
  -Profile arm_bringup -Backend disabled \
  -OutputRoot final_project\cpu\build\arm_profiles\g0_hello_regression \
  -ToolchainPath D:\Efinity\efinity-riscv-ide-2025.2\toolchain
exit=0; ELF PASS profile=arm_bringup backend=disabled entry=_start not_for_flash=True uart2=excluded

ELF_BUILD_ID_EMBEDDED=20260715_165504_64260b6
```

此为相关源码变化后的定向 G0–G3 回归，不重复宣告已关闭 Gate；没有 Programmer、复位或 UART0 实机日志，故不算 G4 的三次 CPU Hello。
