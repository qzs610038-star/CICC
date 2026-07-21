# HISTORICAL / SUPERSEDED — UART0 operator card

> This document is retained only as a FAIL_STOP history index. It is not executable. The current route is UART1 Type-C on `COM17`; `COM10` and `COM13` are prohibited as UART1 candidates. See `CURRENT_STATE.md` and `../review_packets/CPU_HELLO_UART1_H0_H6_RECEIPT_20260721.md`.

# M2 UART0 CPU Hello — 板级操作卡

> 适用范围：只关闭 `competition_project_single_camera/` 的隔离 CPU UART0 Hello。
> 不是 `final_project/` A0/G4 关闭，不开放 UART2、J52、myCobot、外部 DDR、USER1 或 Flash。
> 执行者：现场操作者；Codex 在收到原始证据后复核。

## 固定批次

| 项目 | 固定值 |
|---|---|
| bitstream | `outflow_m2_cpuhello_20260716_1730/mem_test.bit` |
| bitstream SHA-256 | `2EA4AD287CCC6BFBF113E718B59EF6F5807222AFE1ECE3D55BD1E24D37FB8347` |
| Hello ELF | `cpu_bringup/uart_hello_onchip/build/uart_hello_onchip.elf` |
| ELF SHA-256 | `C99FD39DB437409A63A6061CD29698B5B60099B9E24A77B155B871E169BF5DA5` |
| CPU JTAG | `JTAG_USER2` |
| ELF load address | `0xF9000000` 片上 RAM（16 KiB） |
| UART0 | 115200 baud、8N1、无流控 |

在实际点击配置前，执行并截图保存输出：

```powershell
Get-FileHash .\outflow_m2_cpuhello_20260716_1730\mem_test.bit -Algorithm SHA256
Get-FileHash .\cpu_bringup\uart_hello_onchip\build\uart_hello_onchip.elf -Algorithm SHA256
```

两项 hash 任一不匹配，立即 STOP；不得“就近”换用旧 `1D697...AECC` bitstream。

### 本次操作员预检结果

操作员已回传两项预期 hash；Codex 已对同一现存文件独立重算，验证命令 exit 0。证据见 `evidence/m2_uart0_operator_hash_preflight_20260716.md`。随后已通过专属 ASCII 镜像完成匹配 bitstream 的易失性配置，验收见 `evidence/m2_uart0_volatile_config_acceptance_20260716.md`。第 2 节改由独立的 `m2_uart0_user2_ram_download_operator_card_20260716.md` 执行；第 3 节仍未授权。离线准备的 `tools/capture_m2_uart0_banner.ps1` 也不能替代此授权：它要求 Codex 在 USER2 RAM 下载 PC 范围截图复核后签发匹配本批 hash 的批准 JSON，未取得该 JSON 时即使显式 `-Listen` 也 fail-closed 且不打开串口。

### ASCII 手工烧录目录的制品镜像规则

若 Efinity Programmer 不能使用仓库中文路径，允许把**批准的单个 bitstream**镜像到操作员指定的 ASCII 暂存目录；这不是重新构建，也不是让 `outflow/` 中的旧文件“就近替代”。已发生一次 JTAG 传输成功但制品身份不匹配的尝试：暂存目录遗留 `outflow/mem_test.bit` 的 SHA-256 为 `9515FAC58CBE4DC07ADED28B6038C64485989C6BE650D09E5C07F6D5C4F1A169`，不等于固定批次的 `2EA4...B8347`，故该尝试不接受为本 Gate 配置。

使用操作员指定暂存根目录下的专属批次子目录 `m2_cpuhello_20260716_1730/mem_test.bit`，按 `evidence/m2_uart0_programmer_attempt_review_20260716.md` 的源/目标双 hash 命令复制和核对。`tools/sync_to_manual_burn_dir.ps1` 显式指定目标后仍会排除 `outflow`/`outflow_*` 且保留目标生成物，不能用它更新本 bitstream。只有目标 hash 完整等于本卡固定值，才可执行下方第 1 节。

## 已满足的连接前置项

- FTDI `0403:6011` 的 A/C/D 三个已启动通道：COM3、COM7、COM8；原始证据为 `docs/debug_sessions/evidence/m2_ftdi_live_verify_20260716_152234.json`。
- 用户已确认机械臂端口与当前电脑物理断开。J52 Pin1–Pin4、UART2、外部 TX/VCC 必须继续不接。
- 当前另有 COM11，VID:PID 为 `1A86:7523`（CH340），但本卡不依据 VID/PID 给它归属；**不得打开 COM11**。

## 操作步骤

### 1. 仅易失性 FPGA 配置

1. 打开候选工程 `mem_test.xml`，不编辑 XML、Periphery、SDC、top 或 IP 设置。
2. 在 Efinity Programmer 中确认目标器件为 `TJ375N529`，选择上表的 `mem_test.bit`；若使用 ASCII 镜像，文件必须是操作员指定暂存根目录下 `m2_cpuhello_20260716_1730/mem_test.bit`，且其 SHA-256 已通过上文双重核对。
3. 只选择 GUI 明确标为 **volatile / SRAM / JTAG FPGA configure** 的配置路径。
4. 执行配置，保存 Programmer 成功页截图；此步骤不选择 USER1/USER2，也不加载 ELF。

**STOP：**器件不匹配、未发现 JTAG 设备、GUI 出现 SPI/Flash/PROM 擦写或持久化编程选项、或 Programmer 报错/超时。停止在此层，不尝试 USER1、DDR、UART2 或 J52。

### 2. 仅 CPU USER2 片上 RAM 下载

1. 打开 Efinity RISC-V IDE/调试器，目标必须选择 QCRV32 的 **`JTAG_USER2`**。
2. 只加载上表 Hello ELF；确认 ELF LOAD 基址为 `0xF9000000`，选择下载并运行到片上 RAM。
3. 保存含 USER2 选择、ELF 路径/地址及下载成功的截图。

**STOP：**调试器只能选择 USER1、要写 Flash、改用 `default.ld`/外部 DDR、ELF 下载地址不是 `0xF9000000`、下载失败或 CPU 调试报错。不得降级为 USER1 或任意其他固件。

### 3. UART0 横幅与单字符回显

**当前禁止执行。**只有 Codex 复核第 2 节的 USER2/ELF/PC 范围证据、另行签发 UART0 监听操作卡后，才可以使用本节；不得因本卡保留了后续步骤或脚本已存在而提前打开 COM3/COM7/COM8。

1. 仅在步骤 2 成功后，按顺序对 COM3、COM7、COM8 **一次只打开一个**终端：115200、8N1、无流控、DTR/RTS 关闭；每个端口先监听 3 秒且不发送字节。
2. 出现完整横幅的端口即为候选 UART0，保存完整原始文本：

```text
TJ375 CPU+VIDEO UART0 HELLO
ONCHIP_RAM=0xF9000000 UART0=115200 8N1
Type characters to verify echo.
```

3. 只在已出现该完整横幅的端口发送一次可见字符 `x`，确认仅一次 `x` 回显；关闭终端。

**STOP：**任一端口出现乱码、异常连续输出、无横幅、未知命令响应、无法关闭 DTR/RTS，或操作员准备在 COM11/其他非 FTDI 端口发送数据。无横幅时停止并返回当前层，不试 UART2/J52/机械臂。

## 证据与回传

保存到 `docs/debug_sessions/evidence/m2_uart0_hello_board_YYYYMMDD_HHMMSS/`：

1. bit 与 ELF hash 命令输出；
2. Programmer 配置成功截图；
3. USER2/`0xF9000000` 下载成功截图；
4. 完整横幅与 `x` 回显原始终端文本/截图；
5. 记录实际 UART0 是 COM3、COM7 或 COM8 中的哪一个，以及每一步的时间和 exit/status。

只回传证据，不根据本卡自行进入 feature snapshot、OSD、UART2 或 myCobot。Codex 复核后才判断 CPU Hello Gate。
