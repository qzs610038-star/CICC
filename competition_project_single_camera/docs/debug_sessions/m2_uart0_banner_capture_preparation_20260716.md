# M2 UART0 横幅监听守卫 — 离线准备记录

> 日期：2026-07-16
> 适用范围：仅为 `competition_project_single_camera/` 的隔离 CPU UART0 Hello 准备后续**只读**监听工具。
> 本记录不是 UART0 操作卡，不授权打开 COM3/COM7/COM8，不代表 USER2、CPU、UART0 或机械臂任何 Gate 已通过。

## 目的与边界

`tools/capture_m2_uart0_banner.ps1` 用于在 USER2 RAM 下载 PC 范围证据被 Codex 复核后，对一个已确认的 FTDI UART0 通道做 3–10 秒、115200 8N1、无流控的只读横幅捕获。

脚本不会实现或自动执行单字符回显；即使后续完整横幅被捕获，也必须由 Codex 单独审查并发出一次性 `x` 回显操作卡。

## Fail-closed 条件

只有同时满足下列条件，显式 `-Listen` 才可能调用串口 `Open()`：

1. 有一份 Codex 签发的 JSON，`result=M2_USER2_RAM_PC_GATE_APPROVED`；
2. JSON 绑定当前 bitstream SHA-256 `2EA4AD287CCC6BFBF113E718B59EF6F5807222AFE1ECE3D55BD1E24D37FB8347`；
3. JSON 绑定当前 ELF SHA-256 `C99FD39DB437409A63A6061CD29698B5B60099B9E24A77B155B871E169BF5DA5`；
4. JSON 记载 PC 范围 `0xF9000000..0xF9003FFF`；
5. 端口是当前可枚举的 COM3、COM7 或 COM8 之一。

缺少或不匹配任一项时结果为 `HOLD_PC_GATE_APPROVAL_REQUIRED_NO_SERIAL_OPEN`，不尝试打开串口。脚本始终固定 `DTR=false`、`RTS=false`、无流控、`uart_bytes_sent=0`；不接受 COM11，不访问 UART2/J52/myCobot、Programmer、OpenOCD、GDB 或 Flash。

## 离线验证

| 验证 | 结果 | 证据 |
|---|---|---|
| PowerShell AST parse | `PARSE_ERROR_COUNT=0` | 当前会话命令输出 |
| 默认 dry-run | `DRY_RUN_NO_SERIAL_OPEN`，`serial_port_opened=false`，`uart_bytes_sent=0` | `evidence/m2_uart0_banner_capture_postpatch_dryrun_20260716_164813.json` |
| 强制 `-Listen` 但无批准 JSON | exit 2 / `HOLD_PC_GATE_APPROVAL_REQUIRED_NO_SERIAL_OPEN`，`serial_open_attempted=false`，`serial_port_opened=false`，`uart_bytes_sent=0` | `evidence/m2_uart0_banner_capture_postpatch_unapproved_20260716_164814.json` |

这些验证仅证明守卫在离线/未授权状态不触碰串口；没有捕获 UART0 数据，也没有证明 CPU 已运行。

## 当前下一步

执行 [USER2 RAM 下载操作卡](m2_uart0_user2_ram_download_operator_card_20260716.md) 并回传配置页、完整 Console、PC/反汇编停点截图。Codex 复核其 PC 范围后，才会创建批准 JSON 和单独的 UART0 监听操作卡。
