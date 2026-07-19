# Goal4-I2 APB MAGIC Probe Review Packet

> 状态：`READY FOR QZS/CODEX STATIC REVIEW / BOARD NOT AUTHORIZED / BOARD NOT VERIFIED`
>
> 本 Packet 只申请审查专用只读 probe ELF 的静态身份和未来板级操作边界，不授权当前会话执行任何硬件动作。

## 任务目标与当前结论

目标是为 Goal4-I2 准备一个与 UART1 原子批次 BSP 匹配的最小 RISC-V probe：从同批生成的 `soc.h` 读取 `IO_APB_SLAVE_0_INPUT`，对 offset `0x000` 执行一次 32-bit volatile read，比较 `0x375A0001`，把地址、期望值、观察值和 PASS/FAIL 状态保存在片上 RAM 全局变量中，供 debugger 审计。

当前结论仅为 `OFFLINE STATIC EVIDENCE PASS`。USER2、CPU 取指、UART1、APB 实读和 MAGIC 结果仍为 `NOT VERIFIED`；Runbook 中 I2 在本 Packet 获批前继续 `BLOCKED`。

## 固定输入与身份

| 对象 | 值 |
|---|---|
| 上游 Runbook follow-up | `dev/wsc6090-goal4-runbook-20260719@8f5fc6cce4eecb2c2ddb0a32a0691786614f502b` |
| UART1 设计提交 | `6effdc3685d696cb4d33f3fbb1c449729ed72e33` |
| UART1 批次 | `I0_UART1_20260719_CLEAN_LF_FINAL` |
| `soc.h` SHA-256 | `25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B` |
| APB 基址来源 | `IO_APB_SLAVE_0_INPUT`，同批 `soc.h` |
| 同批生成值 | `0xE8100000`，4 KiB window |
| 唯一允许偏移 | `0x000` |
| 访问 | 一次 `READ32`；禁止 APB write |
| 期望值 | `0x375A0001` |
| ELF entry / memory | `0xF9000000` / 16 KiB on-chip RAM |

ELF、LOAD 末端和源码 hash 由 `artifacts/apb_magic_onchip.sha256.txt` 固定，以本提交生成后的实际文件为准。

## 修改文件与关键 diff

- `competition_project_single_camera/embedded_sw/apb_magic_onchip/src/main.c`
- `competition_project_single_camera/embedded_sw/apb_magic_onchip/makefile`
- `competition_project_single_camera/embedded_sw/apb_magic_onchip/build_apb_probe_evidence.ps1`
- `competition_project_single_camera/embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.elf`
- `competition_project_single_camera/embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.objdump.txt`
- `competition_project_single_camera/embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.readelf.txt`
- `competition_project_single_camera/embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.symbols.txt`
- `competition_project_single_camera/embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.undefined.txt`
- `competition_project_single_camera/embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.sha256.txt`
- 本 Review Packet。

关键实现约束：

1. 不硬编码 APB 基址；C 源码只引用 `IO_APB_SLAVE_0_INPUT`。
2. `main()` 中只解引用一次 `magic_register`。
3. 不初始化任何 UART，不依赖串口输出作为 I2 观察点。
4. 不存在 APB 写、地址扫描、其他偏移读取或重试循环。
5. `g_apb_probe_status` 使用 ASCII-like 32-bit 标记：`PASS=0x50415353`、`FAIL=0x4641494C`；这些变量仅在片上 RAM 中供 debugger 查看。

## 模块、信号、时钟、复位和双通道影响

- 不修改 RTL、顶层、XML、peri.xml、SDC、IP settings、时钟或复位。
- 不修改既有 `apb_reg_magic`，其硬件语义仍为 offset `0x000` 只读 `0x375A0001`。
- 不涉及视频 ch0/ch1 或双通道数据路径。
- 不实现 OSD、feature snapshot、CDC 或业务寄存器。
- probe 使用既有同批 BSP/linker/startup；没有新增硬件资源或 bitstream 变化。

## 已运行命令、退出码、日志与 warning

构建入口：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  competition_project_single_camera\embedded_sw\apb_magic_onchip\build_apb_probe_evidence.ps1 `
  -ToolchainRoot <Efinity-RISC-V-IDE-root>
```

脚本执行：

- Efinity RISC-V GCC/make 构建；
- `readelf -h -l` 审计 entry 和唯一 LOAD；
- `objdump -d -S` 生成完整反汇编；
- `nm -u` 断言无未解析符号；
- 从 `main` 反汇编断言恰好一个 `lw`，且无 APB store；
- 固定 ELF、`soc.h`、源码、makefile SHA-256。

实际结果：

```text
build_exit=0
elf_sha256=6CB7E5E1CA5AF1B82FB99015925F13C2517B9332FD0FB0015DC1B4FDC3DFEAEC
entry=0xF9000000
load_segments=1
load_end=0xF90008F0
ram_used=2288_bytes_of_16384
undefined_symbols=0
main_lw_count=1
apb_base_materialized=0xE8100000
apb_offset=0x000
apb_write_count=0
compiler_warnings=0
```

反汇编关键序列：

```text
f90000a4: e81007b7  lui a5,0xe8100
f90000a8: 4398      lw  a4,0(a5)
```

`g_apb_probe_address`、`g_apb_probe_expected`、`g_apb_probe_status` 和
`g_apb_probe_observed` 均由符号审计确认位于 `0xF9000000..0xF9003FFF`。

## 未来板级观察点与失败分类

只有本 Packet 获批、Goal4-I0/I1 在同一批准窗口 PASS 后，才允许下载匹配 probe ELF。未来 debugger 观察点：

| 变量 | 期望 |
|---|---|
| `g_apb_probe_address` | `0xE8100000`，且来源可追溯到同批 `soc.h` |
| `g_apb_probe_expected` | `0x375A0001` |
| `g_apb_probe_observed` | `0x375A0001` 时才可能 PASS |
| `g_apb_probe_status` | `0x50415353` 为 PASS；`0x4641494C` 为 mismatch |

失败分类：下载/entry 失败为 `ELF_DOWNLOAD_FAIL`；load trap/hang 为 `APB_BUS_FAULT/APB_TIMEOUT`；返回值不符为 `APB_MAGIC_MISMATCH`；证据缺失为 `EVIDENCE_INCOMPLETE`。任何失败均禁止第二地址、写访问或地址扫描。

## 未验证项、风险假设与回退条件

- `NOT VERIFIED`：USER2、CPU 实际执行 probe、APB read、MAGIC 值、UART1、视频和 OSD。
- 静态反汇编只能证明编译结果，不证明总线时钟、复位、连线或板级返回值。
- 若 `soc.h`、linker、startup、RTL/XML/SDC/IP、bitstream 或 BSP 任一输入变化，ELF 和本 Packet 立即失效，须重新构建和审查。
- 若实际板级访问 trap/hang，halt CPU 并保存寄存器/PC；不得通过增加重试、猜地址或写寄存器规避。
- 回退为保持 Goal4-I2 `BLOCKED`，不修改 Hello ELF，不影响 I0/I1。

## 机械臂/外设状态与用户安全确认

- `ARM_ENABLED=0`；UART2/J52 与机械臂保持断开。
- 不发送 myCobot 帧，不执行动作。
- probe 不初始化 UART1 或其他外设。
- 本任务没有执行 Programmer、JTAG、串口或板卡操作。

## 希望 QZS/Codex 裁定的问题

1. 是否批准该 ELF 作为 Goal4-I2 的唯一专用 probe，并保持访问范围为单次 `READ32(IO_APB_SLAVE_0_INPUT + 0x000)`？
2. 是否接受片上 RAM 全局变量作为唯一观察面，避免将 APB probe 与 UART1 输出耦合？
3. 在 I0/I1 同批 PASS 后，是否允许按 Runbook 建立一次受控 I2 板级读取窗口？
