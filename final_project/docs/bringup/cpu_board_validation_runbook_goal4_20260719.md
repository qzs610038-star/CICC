# Goal4 CPU 上板验证 Runbook

> 路由：`UART1_TYPEC`
>
> 状态：操作与证据模板，不构成板级 PASS。本文件不授权任何硬件动作；实际执行仍需独立批准窗口。

## 1. 固定批次

本 Runbook 只接受以下集合批次和匹配制品。任何字段不一致均停止，不得替换制品或猜测地址。

| 对象 | 固定值 |
|---|---|
| 集合分支 | `codex/qzs-wsc-libaoxun-integration-20260718` |
| 集合提交 | `e72fb6a3ce1b7999f61cae1e0ed7b2773f1e4fda` |
| UART1 设计提交 | `6effdc3685d696cb4d33f3fbb1c449729ed72e33` |
| 批次 ID | `I0_UART1_20260719_CLEAN_LF_FINAL` |
| Efinity | `2025.2.288.4.15` |
| bitstream SHA-256 | `D05EFD4EC91FDB51F2997483BA7F4A3634C3E50FF1C65DD7239B41E19E2BC544` |
| UART1 Hello ELF SHA-256 | `919B291A2B980F02507D1591EFBE5D8E395DC53F4A480AE5A8C577D74CF5F7FA` |
| ELF entry | `0xF9000000` |
| 片上 RAM | `0xF9000000..0xF9003FFF` |
| CPU JTAG | FPGA `USER2` |
| UART 路由 | SoC UART1 → 板载 Type-C UART1 |
| RX / TX | `GPIOR_96/B12` / `GPIOR_100/D12` |
| UART 基址 | `SYSTEM_UART_1_IO_CTRL=0xe8011000` |
| 串口格式 | `115200 8N1`，无流控 |

匹配 `soc.h` 必须同时包含并由操作日志摘录：

```text
SYSTEM_UART_1_IO_CTRL=0xe8011000
SYSTEM_UART_1_IO_PARAMETER_UART_CTRL_CONFIG_RX_SAMPLE_PER_BIT=8
SYSTEM_UART_1_IO_PARAMETER_INIT_CONFIG_BAUDRATE=115200
SYSTEM_UART_1_IO_PARAMETER_INIT_CONFIG_DATA_LENGTH=7
SYSTEM_UART_1_IO_PARAMETER_INIT_CONFIG_PARITY=NONE
SYSTEM_UART_1_IO_PARAMETER_INIT_CONFIG_STOP=ONE
```

`DATA_LENGTH=7` 是 Efinity UART 寄存器对 8 data bits 的编码。固件必须使用以上生成宏计算 divider 和写入 frame config，不得使用手填常量替代生成参数。

## 2. 串行 Gate

```text
Goal4-I0 CPU first instruction
  -> Goal4-I1 UART1 Hello / echo
  -> Goal4-I2 APB MAGIC: BLOCKED
  -> Goal4-I3 OSD handshake: BLOCKED_CONTRACT_NOT_FROZEN
```

前一级未 PASS 时禁止进入后一级。I0/I1 的 PASS 不自动授权 I2/I3。

## 3. 全程输入与禁止动作

### 3.1 必备输入

1. §1 固定批次的 bitstream、Hello ELF、匹配 `soc.h` 和构建证据。
2. 板卡编号、供电限流、JTAG 适配器、Type-C UART1 线缆和枚举端口身份。
3. Efinity/JTAG 调试环境，明确选择 FPGA `USER2`。
4. 串口终端固定为 `115200 8N1`、无流控，开启原始字节和时间戳记录。
5. 屏幕录制，能够同时关联调试器、串口终端和板卡状态。
6. 操作员、观察员、开始时间、停止责任人和批准窗口编号。

### 3.2 禁止动作

- 禁止执行任何实际硬件动作，直到获得独立批准窗口。
- 禁止 `USER1`、Flash 擦写/烧录、上电自启动和外部 DDR 初始化。
- 禁止 UART2/J52、机械臂接线、myCobot 帧和任何运动命令。
- 禁止使用不匹配 hash 的 bitstream、ELF、BSP 或 `soc.h`。
- 禁止扫描串口、轮换波特率、向未确认端口发送字符或复用历史端口结论。
- 禁止扫描 APB 地址、读取未批准偏移或用写访问探测寄存器。
- 禁止向候选 OSD 地址写入；禁止把设计契约当作已实现 wire ABI。
- 禁止把 attach、halt、旧终端内容、仿真或离线构建 PASS 记为板级 PASS。

### 3.3 立即停止条件

hash 或批次不符、TAP 不确定、Type-C UART1 身份不确定、PC 进入片上 RAM 以外区域、调试器失联、异常复位、板卡电流/温度异常、串口乱码、APB trap/hang、观察员无法确认当前操作时立即停止并保存原始证据。

## 4. S0 批次预检

| 步骤 | 明确输入/动作 | 观察点 | PASS 判据 | 失败分类 |
|---|---|---|---|---|
| S0.1 | 记录集合 SHA、UART1 设计 SHA、批次 ID | Git 与证据页原始输出 | 与 §1 完全一致 | `BATCH_IDENTITY_FAIL` |
| S0.2 | 计算 bitstream 和 ELF SHA-256 | hash 原始输出 | 与 §1 完全一致 | `ARTIFACT_HASH_FAIL` |
| S0.3 | 审计 ELF entry、LOAD 和未解析符号 | readelf/nm 原始输出 | entry=`0xF9000000`；唯一 LOAD 在片上 RAM；无 undefined | `ELF_LAYOUT_FAIL` |
| S0.4 | 从匹配 `soc.h` 摘录 UART1 基址和全部配置宏 | 文件路径、blob/hash、宏值 | 基址与参数符合 §1 | `SOC_HEADER_MISMATCH` |
| S0.5 | 核对 Type-C UART1 的 RX/TX 和枚举端口身份 | 引脚证据、线缆标签、端口描述 | RX/TX 与 §1 一致且端口唯一 | `UART1_ROUTE_IDENTITY_FAIL` |
| S0.6 | 配置终端并开始只读监听日志 | `115200 8N1`、无流控、开始时间 | 未发送任何字节且日志已打开 | `UART_CAPTURE_NOT_READY` |

PowerShell hash 命令：

```powershell
Get-FileHash -Algorithm SHA256 '<absolute-path-to-bitstream>'
Get-FileHash -Algorithm SHA256 '<absolute-path-to-uart1-hello-elf>'
```

## 5. Goal4-I0：CPU first instruction

**目标**：证明 CPU 通过 `USER2` 从匹配 UART1 Hello ELF 的 `0xF9000000` 入口实际执行第一条指令。本级不以串口输出作为 PASS 判据。

| 步骤 | 明确输入/动作 | 观察点 | PASS 判据 | 失败分类 |
|---|---|---|---|---|
| I0.1 | 在批准窗口内下载匹配 bitstream | Programmer 日志、板卡电流/温度 | 下载完成且板况正常 | `FPGA_CONFIG_FAIL` / `BOARD_POWER_FAIL` |
| I0.2 | 经 `USER2` attach 并 halt CPU | TAP、hart、halt reason、PC | 明确连接目标 CPU | `JTAG_USER2_ATTACH_FAIL` |
| I0.3 | 仅将匹配 ELF 下载到片上 RAM | 下载地址、字节数、verify | 无 Flash/DDR 写入且 verify 完成 | `ELF_DOWNLOAD_FAIL` / `FORBIDDEN_REGION_ACCESS` |
| I0.4 | 在 entry halt，读取首条指令 | PC、反汇编、机器码 | PC=`0xF9000000` 且指令可读 | `ENTRYPOINT_MISMATCH` / `MEMORY_READ_FAIL` |
| I0.5 | 执行一次 single-step | step 前后 PC、trap/reset 状态 | 首指令被执行且无 trap/reset | `FIRST_INSTRUCTION_TRAP` / `CPU_NO_PROGRESS` / `UNEXPECTED_RESET` |
| I0.6 | 保存 transcript、寄存器和视频时间点 | 原始证据路径 | 能复核 step 前 PC、指令、step 后 PC | `EVIDENCE_INCOMPLETE` |

仅观察到 entry PC 不证明执行，结果记为 `INCONCLUSIVE`。

## 6. Goal4-I1：UART1 Hello

**前置**：I0 PASS，Type-C UART1 路由身份已确认。

预期输出：

```text
I0 UART1 HELLO
UART1=115200 8N1 RX=GPIOR_96 TX=GPIOR_100
Type characters to verify echo.
```

| 步骤 | 明确输入/动作 | 观察点 | PASS 判据 | 失败分类 |
|---|---|---|---|---|
| I1.1 | 再次核对终端配置并保持原始日志连续 | 端口身份、`115200 8N1`、无流控 | 配置与匹配 `soc.h` 一致 | `UART1_CONFIG_FAIL` |
| I1.2 | 从已验证 entry free-run | 首字节时间、完整 RX 字节、CPU run 状态 | 10 s 内出现三行完整横幅 | `UART1_NO_OUTPUT` / `UART1_GARBLED` / `UART1_PARTIAL_BANNER` |
| I1.3 | 横幅完整后发送 `G4I1-<session-id>\r\n` 一次 | TX/RX 十六进制和时间差 | 每字节按顺序回显一次，无增删重排 | `UART1_TX_FAIL` / `UART1_ECHO_MISMATCH` / `UART1_DUPLICATE_ECHO` |
| I1.4 | halt CPU 并关闭日志 | PC、halt reason、日志结束时间 | CPU 可控停止且证据落盘 | `CPU_HALT_FAIL` / `EVIDENCE_INCOMPLETE` |

乱码、只有部分横幅、只有回显或重复回显均不构成 PASS。相同失败连续两轮后停止试错并提交 Review Packet。

## 7. Goal4-I2：APB MAGIC

**状态：`BLOCKED`。**

解锁条件：完成专用只读 probe ELF 和对应 Review Packet，并将 probe 的源码、ELF hash、entry/LOAD、反汇编、唯一 MMIO 地址与访问宽度绑定到同一 UART1 原子批次。在此之前：

- 不得修改 UART1 Hello ELF 临时加入 APB 访问；
- 不得用 debugger memory view 或命令行直接读取 APB；
- 不得读取候选基址或任何偏移；
- 不得将历史 APB 结果继承为当前 PASS。

日志固定填写：

```text
Goal4-I2=BLOCKED
failure_code=BLOCKED_I2_PROBE_AND_REVIEW_PACKET_REQUIRED
```

## 8. Goal4-I3：OSD handshake

**状态：`BLOCKED_CONTRACT_NOT_FROZEN`。**

解锁至少需要：正式 OSD wire ABI、APB staging/commit/confirmed 地址和编码、CDC 所有权、VSYNC 原子切换、单 outstanding、ACK 规则、两域复位与 RECOVERING 协议、RTL 仿真、新原子构建证据及匹配测试 ELF。未全部冻结前不得写入任何候选寄存器。

日志固定填写：

```text
Goal4-I3=BLOCKED_CONTRACT_NOT_FROZEN
failure_code=BLOCKED_CONTRACT_NOT_FROZEN
```

## 9. 失败分类

| 类别 | 代码示例 | 处置 |
|---|---|---|
| 批次/证据 | `BATCH_IDENTITY_FAIL`、`ARTIFACT_HASH_FAIL`、`SOC_HEADER_MISMATCH` | 不执行硬件动作；退回批次生产者核查 |
| 路由/端口 | `UART1_ROUTE_IDENTITY_FAIL`、`UART_CAPTURE_NOT_READY` | 不发送字符；核对 Type-C UART1、引脚和枚举身份 |
| FPGA/JTAG | `FPGA_CONFIG_FAIL`、`JTAG_USER2_ATTACH_FAIL` | 保存工具日志；不扩大到 CPU/UART |
| CPU 启动 | `ENTRYPOINT_MISMATCH`、`FIRST_INSTRUCTION_TRAP`、`CPU_NO_PROGRESS` | 保存 PC、寄存器和反汇编；不进入 I1 |
| UART1 | `UART1_NO_OUTPUT`、`UART1_GARBLED`、`UART1_ECHO_MISMATCH` | 保持固定端口和参数；不扫描、不换路由 |
| 阻断 Gate | `BLOCKED_I2_PROBE_AND_REVIEW_PACKET_REQUIRED`、`BLOCKED_CONTRACT_NOT_FROZEN` | 不执行对应访问；完成 Review/契约后重开 |
| 证据不足 | `EVIDENCE_INCOMPLETE`、`INCONCLUSIVE` | 不记 PASS；补齐原始证据后新建会话 |

## 10. 日志模板

### 10.1 会话头

```text
[CPU_BOARD_VALIDATION_SESSION]
session_id=
approval_window_id=
date_time_start=
timezone=Asia/Shanghai
operator=
observer=
stop_owner=
board_model=
board_serial=
power_supply_limit=
jtag_adapter=
typec_uart1_adapter=
typec_uart1_port=
typec_uart1_identity_evidence=

collection_commit=e72fb6a3ce1b7999f61cae1e0ed7b2773f1e4fda
uart1_design_commit=6effdc3685d696cb4d33f3fbb1c449729ed72e33
batch_id=I0_UART1_20260719_CLEAN_LF_FINAL
bitstream_path=
bitstream_sha256=
elf_path=
elf_sha256=
elf_entry=
elf_load_range=
soc_h_path=
soc_h_sha256=

runbook_route=UART1_TYPEC
uart_base=0xe8011000
uart_baud=115200
uart_format=8N1
uart_rx=GPIOR_96/B12
uart_tx=GPIOR_100/D12
selected_jtag_tap=USER2
flash_write_disabled=yes/no
external_ddr_disabled=yes/no
uart2_j52_disconnected=yes/no
session_go_no_go=GO/NO_GO
```

### 10.2 单步记录

```text
[GOAL_STEP]
goal=Goal4-I0/Goal4-I1/Goal4-I2/Goal4-I3
step_id=
preconditions=
input_hashes=
start_time=
end_time=
action=
expected_observation=
actual_observation=
debugger_pc_before=
debugger_instruction=
debugger_pc_after=
uart_tx_hex=
uart_rx_hex=
raw_log_path=
screenshot_or_video_path=
result=PASS/FAIL/BLOCKED/INCONCLUSIVE
failure_code=
retry_count=
forbidden_action_check=PASS/FAIL
operator_signature=
observer_signature=
```

### 10.3 会话结论

```text
[SESSION_RESULT]
RUNBOOK_ROUTE=UART1_TYPEC
UART_BASE=0xe8011000
Goal4-I0=PASS/FAIL/NOT_RUN/INCONCLUSIVE
Goal4-I1=PASS/FAIL/NOT_RUN/INCONCLUSIVE
Goal4-I2=BLOCKED
Goal4-I3=BLOCKED_CONTRACT_NOT_FROZEN
highest_closed_gate=
first_open_or_failed_gate=
failure_class=
evidence_root=
unverified_items=
prohibited_scope_remained_untouched=yes/no
date_time_end=
operator_signature=
observer_signature=
```

## 11. 当前静态状态

```text
RUNBOOK_ROUTE=UART1_TYPEC
UART_BASE=0xe8011000
Goal4-I0=NOT_RUN
Goal4-I1=NOT_RUN
Goal4-I2=BLOCKED
Goal4-I3=BLOCKED_CONTRACT_NOT_FROZEN
```

## 12. 当前证据入口

- `CURRENT_STATE.md` at collection commit `e72fb6a3ce1b7999f61cae1e0ed7b2773f1e4fda`
- `competition_project_single_camera/integration/I0_UART1_INTERFACE_FREEZE.md`
- `competition_project_single_camera/embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/include/soc.h`
- `competition_project_single_camera/embedded_sw/uart1_hello_onchip/I0_UART1_BUILD_EVIDENCE.md`
- `competition_project_single_camera/embedded_sw/uart1_hello_onchip/I0_UART1_BUILD_MANIFEST.json`
- `competition_project_single_camera/embedded_sw/uart1_hello_onchip/src/main.c`
