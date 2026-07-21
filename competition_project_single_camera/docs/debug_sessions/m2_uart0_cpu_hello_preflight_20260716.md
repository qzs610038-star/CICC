# HISTORICAL / SUPERSEDED — UART0 preflight

> This record preserves historical observations only. It is not a current port-identification or hardware-operation card. The active receipt route is UART1 Type-C on `COM17`; `COM10` and `COM13` are prohibited as UART1 candidates.

# M2 UART0 CPU Hello — 新 bitstream 预飞行记录

> 日期：2026-07-16
> 执行范围：`competition_project_single_camera/` 的隔离 CPU UART0 Hello 候选。
> 初始裁定：`CONDITIONAL GO / PRE-FLIGHT HOLD`。本记录中的离线与枚举前提已验证；后续状态以第 8 节为准。
> 安全边界：本记录不关闭 `final_project/` 的 A0，不替代正式主线；机械臂、J52、UART2、USER1、Flash 和外部 DDR 全程未进入本批次。

## 1. 批次身份与来源

- Git：`HEAD=origin/main=07373042d1f84cdc048fc42b5752d0cbeb52c471`。
- 构建输入在 Efinity 启动前的 `mem_test.xml` SHA-256：`F428549DF9F87DC9A6CF0464F8C5F5FD92DABD2102CA8850F7C9DE21A0BAA060`；该值与 M2 Review Packet 的 Windows checkout 记录相同。
- Efinity 会自动重写 XML 格式和 `last_change` 元数据。本次构建后已仅恢复该工具生成的改写；复核时该文件无 Git diff 且恢复为上述 SHA-256。未保留任何 RTL、SDC、Periphery 或 I/O 改动。
- 输出目录（仓库忽略）：`outflow_m2_cpuhello_20260716_1730/`；工作目录（仓库忽略）：`work_m2_cpuhello_20260716_1730/`。

## 2. 离线构建与制品

执行的 Efinity 命令等价于：

```text
efx_run.bat mem_test.xml --flow compile \
  --output_dir outflow_m2_cpuhello_20260716_1730 \
  --work_dir work_m2_cpuhello_20260716_1730 --timeout 1800
```

`compile` 只运行 Map、Interface、PNR 与 bitstream generation；本批没有 Programmer 设备操作、USER TAP 选择或 Flash 命令。日志 `outflow_m2_cpuhello_20260716_1730/build.log` 依次记录：

```text
map       : PASS
interface : PASS
pnr       : PASS
pgm       : PASS
```

本次 Efinity 由后台 wrapper 启动，wrapper 只记录了成功启动而未单独落盘子进程退出码；不得把它写成伪造的 `efx_run exit 0`。本记录以四个子 flow 的 PASS、生成制品、STA/CDC 原始报告及下文的独立证据验证 exit 0 构成可复核离线证据；后续重建应同时落盘顶层退出码。

产物：

| 制品 | SHA-256 | 大小 | 说明 |
|---|---|---:|---|
| `outflow_m2_cpuhello_20260716_1730/mem_test.bit` | `2EA4AD287CCC6BFBF113E718B59EF6F5807222AFE1ECE3D55BD1E24D37FB8347` | 11,843,718 B | 仅候选 USER2 CPU Hello 板级批次使用；尚未配置 FPGA。 |
| `cpu_bringup/uart_hello_onchip/build/uart_hello_onchip.elf` | `C99FD39DB437409A63A6061CD29698B5B60099B9E24A77B155B871E169BF5DA5` | 31,040 B | 当前源重新构建的片上 RAM Hello ELF。 |

该 `.bit` hash 与先前 M2 批次 `1D697F0DBA62CEDA3A8877729FF29A314F9BBA1A24CDCDFEDB751C7CF4B8AECC` 不同。因此本记录建立新的制品身份，**不得**把 `2EA4...B8347` 写成或混用为 `1D697...AECC`；后续若执行板级操作，截图、终端记录和 hash 必须全部绑定本批次。

## 3. 本批次离线复核

| 检查 | 结果 | 原始证据 |
|---|---|---|
| Map / Interface / PNR / bitstream | 日志记录均为 PASS | `outflow_m2_cpuhello_20260716_1730/build.log` |
| STA | 最差 Setup `+1.742 ns`；最差 Hold `+0.018 ns` | `mem_test.timing.rpt` 第 61、74 行 |
| CDC | `No Synchronizer warnings to report.` | `mem_test.cdc.rpt` 第 5 行 |
| Interface Design | 0 error / 4 个既有物理距离 warning | `mem_test.pt.rpt` 第 899–902 行 |
| post-synthesis | 118 个 warning；不能与 4 个 Interface warning 混称为零 warning | `mem_test.warn.log` |
| Hello ELF | 2608 B / 16 KiB；entry `0xF9000000`；唯一 LOAD `0xF9000000..0xF9000A30` | `uart_hello_build.log`，含 `ELF_LOAD_AUDIT=PASS` |

一次独立证据验证命令以 exit 0 完成，检查 HEAD 对齐、XML 恢复、四个 Efinity PASS 标记、STA、CDC、ELF LOAD 审计和两项 SHA-256。该验证器的输出值为：`validation_exit_code=0`；它验证已有构建证据，不替代物理上板现象。

## 4. 板卡枚举前置项

最近一次只读枚举仅看到：

| 端口 | 身份 | 可否用于 UART0 Hello |
|---|---|---|
| COM4、COM5 | 蓝牙串口 | 否 |
| COM10 | `USB-SERIAL CH340`，VID:PID `1A86:7523`，已确认属 myCobot | **否，禁止打开或用于候选板** |
| FTDI `0403:6011` | 未在 connected Ports 中出现 | 尚不能进入配置或 UART0 验证 |

因此，USER2 实际连接、片上 RAM 取指、UART0 横幅及回显全部仍为 `NOT VERIFIED`。旧 COM9/COM10/COM13 映射失效，不能据此选择端口。

## 5. 下一张操作卡：只关闭 FTDI 枚举前置项

- 目标：让已连接的板卡 FTDI `VID_0403&PID_6011` 出现在 Windows **connected** Ports 中，并用插拔前后差分识别其新增 COM 端口；本卡不配置 FPGA、不发送 UART 字节。
- 接线状态：myCobot 保持与候选板隔离；J52 Pin1–Pin4、UART2 与外部 TX/VCC 均不接；不打开 COM10。只接正常的开发板供电/USB/JTAG-IF 链路。
- 参数：无 UART 终端、无 Programmer 操作、无 USER1/USER2 选择、无 Flash 操作。
- 参数：无 UART 终端、无 Programmer 操作、无 USER1/USER2 选择、无 Flash 操作。建议在插拔后运行：`powershell -ExecutionPolicy Bypass -File .\tools\capture_m2_ftdi_preflight.ps1 -RequireFtdi`。该采集器只调用 `pnputil` 与 `git rev-parse`，不打开串口、不发送 UART 字节、不调用 Programmer；成功时生成 JSON，未枚举 FTDI 时以 exit 2 保持 HOLD。
- 预期现象：插拔差分后，`pnputil /enum-devices /connected /class Ports` 能显示一个或多个 `VID_0403&PID_6011` 通道；采集器保存 JSON 到 `docs/debug_sessions/evidence/m2_ftdi_enumeration_YYYYMMDD_HHMMSS.json`，另保存设备管理器截图。
- STOP：仍无 FTDI、端口身份不明确、误把 COM10 当板卡端口、发现 J52/机械臂已接入，或任何人准备选择 USER1/Flash 时，立即停止并返回本记录。不得猜测 UART0 是哪一条 FTDI 通道。

FTDI 枚举证据经复核后，才可另发“volatile USER2 配置 + `0xF9000000` ELF 下载 + 115200 8N1 横幅/回显”的操作卡；该未来卡也不会开放 UART2/J52/myCobot。

## 6. 采集器离线自测（无板卡）

`tools/capture_m2_ftdi_preflight.ps1` 已在当前无板卡状态自测，测试证据保存在忽略目录 `outflow_m2_cpuhello_20260716_1730/ftdi_preflight_selftest/`：

- 默认采集器 exit 0，记录 `HOLD_NO_CONNECTED_FTDI`、`ftdi_0403_6011_connected_count=0`、`ch340_1a86_7523_ports=["COM10"]`；安全字段为 `serial_port_opened=false`、`uart_bytes_sent=0`、`programmer_invoked=false`、`flash_operation_invoked=false`。VID:PID `1A86:7523` 只说明 CH340，不能脱离当次阶段 0 物理证据就把后续同 VID/PID 端口自动归属为 myCobot。
- `-RequireFtdi` 的负向自测按设计返回 exit 2；外层断言 harness exit 0，并验证相同的 HOLD 与零副作用字段。因此，FTDI 未枚举时不能静默继续到配置阶段。

这只验证采集器的 fail-closed 行为，不是 FTDI 已连接或 UART0 已可用的证据。

## 7. 板卡归还后的 FTDI 复核

- 当前本地 `main` 是 `118be9da1f3cdf63c34624e7ef9cf3b8f9171335`。其中 LBS/libaoxun688 合并 `041116c` 相对其第一父提交只更新 `.gitignore` 的本地 MCP/Efinity 输出忽略项，未改动候选工程的 USER2、UART0、BSP、XML、SDC 或 RTL 契约。
- 初次实时采集的 `HOLD_NO_CONNECTED_FTDI` 是脚本匹配缺陷：Windows Ports 的 FTDI 子设备实例使用 `FTDIBUS\\VID_0403+PID_6011+...`，而旧模式只接受 `&`。原始 PnP 与 pyserial 已显示 COM3/COM7/COM8 的 VID:PID 均为 `0403:6011`；将匹配扩展为 `[&+]` 后，`m2_ftdi_live_verify_20260716_152234.json` 的采集器 exit 0，结果为 `FTDI_ENUMERATED`，三条通道均为 Started。
- 本次全程 `serial_port_opened=false`、`uart_bytes_sent=0`、`programmer_invoked=false`、`flash_operation_invoked=false`。COM11 仍显示 CH340 `1A86:7523`，但用户已确认机械臂与当前电脑物理断开；VID/PID 无法独立确认归属，因此它被记录为未归属 CH340，且未打开。
- 该项只关闭“FTDI 已连接且候选端口集合可见”的前置项；不证明哪一条是 UART0，不证明 USER2、CPU 取指、横幅或回显。
- 下一步改由 `m2_uart0_cpu_hello_board_operator_card_20260716.md` 执行易失性配置、USER2 片上 RAM 下载与受控 UART0 证据采集。

## 8. 状态更新：匹配 bitstream 已易失性配置

- 操作员第二次 Efinity Programmer 截图实际使用操作员指定的专属 ASCII 暂存批次镜像，并记录 JTAG 完成和 `Device is in user mode!`。
- Codex 对该 ASCII 文件与仓库批准制品新鲜重算的 SHA-256 均为 `2EA4AD287CCC6BFBF113E718B59EF6F5807222AFE1ECE3D55BD1E24D37FB8347`；因此“匹配 M2 bitstream 易失性配置”已关闭。此前误用的 `outflow/mem_test.bit`（`9515...A169`）保持为历史失败证据，不混入当前批次。
- 下一步只能按 `m2_uart0_user2_ram_download_operator_card_20260716.md` 通过 `JTAG_USER2` 将固定 Hello ELF 下载到 `0xF9000000`。USER2、CPU 取指、UART0 和回显仍为 `NOT VERIFIED`。
