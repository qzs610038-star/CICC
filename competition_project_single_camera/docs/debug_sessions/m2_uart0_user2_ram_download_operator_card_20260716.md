# M2 UART0 CPU Hello — USER2 片上 RAM 下载操作卡

> 目标：仅证明 QCRV32 可经 `JTAG_USER2` 下载并停在固定 Hello ELF 的片上 RAM 入口。
> 前置：`m2_uart0_volatile_config_acceptance_20260716.md` 已裁定 bitstream 易失性配置 PASS。
> 不包含：UART 收发、UART2/J52、机械臂、USER1、Flash、外部 DDR、OSD 或视频功能测试。

## 固定制品与硬边界

| 项目 | 固定值 |
|---|---|
| Hello ELF（仓库真源） | `cpu_bringup/uart_hello_onchip/build/uart_hello_onchip.elf` |
| ELF SHA-256 | `C99FD39DB437409A63A6061CD29698B5B60099B9E24A77B155B871E169BF5DA5` |
| ASCII 镜像路径 | `<operator-selected ASCII staging directory>/m2_cpuhello_20260716_1730/uart_hello_onchip.elf` |
| CPU 调试 TAP | FPGA `JTAG_USER2` |
| 入口与唯一 LOAD 段起点 | `0xF9000000` 片上 RAM（16 KiB） |
| 运行时 PC 强制验收范围 | `0xF9000000..0xF9003FFF`（必须在任何 Resume 前可见） |

机械臂、J52 Pin1–Pin4、UART2、外部 TX/VCC 保持不接。不得打开 COM11 或发送任何 UART 字节；本卡结束时也不打开 COM3/COM7/COM8。

### USER TAP 身份复核

当前 Hard SoC 的 `INTF_JTAG_TAP_SEL=9`；Efinity 2025.2 官方枚举将 `9` 映射为 `JTAG_USER2`，而当前 `mem_test.peri.xml` 也将 `soc_jtag_inst1` 实例化为 `JTAG_USER2`。离线三方交叉审计见 `evidence/m2_uart0_user2_profile_audit_20260716.md`。**官方 Ti375C529 样例的 `INTF_JTAG_TAP_SEL=8` 是 USER1，不能复制其 debug profile、OpenOCD 或 launch 文件。**

ASCII 手工目录的完整 BSP 已受控生成，但只允许 Efinity 2025.2 官方 **Titanium / TI 硬 TAP** 模板链：`ftdi_ti.cfg + debug_ti.cfg`。不得使用 `debug_softTap.cfg`、`external.cfg`、官方 `Ti375C529_devkit` 的 USER1 样例或自写 OpenOCD 参数。该硬 TAP 链的 USER2 依据与静态审计见 `evidence/m2_uart0_user2_profile_audit_20260716.md`。

该受控 BSP 没有生成可直接导入的 `.launch` 文件；不要为了“补齐”它而手工创建 launch、复制 devkit launch，或运行命令行 OpenOCD。必须通过 IDE 中名称明确的官方 TI/Titanium 硬 TAP 模板建立本次一次性会话；若找不到该模板，只截图并 STOP。

### TJ375 的会话级 CPUTAPID 覆盖（强制）

当前目标是 `TJ375N529`，其官方 JTAG ID 是 `0x006A0EF3`；Programmer 已实读同一 ID。官方 `ftdi_ti.cfg` 的默认 `CPUTAPID=0x006A0A79` 属于 **Ti375**，并不匹配 TJ375，而 `ti.ftl` 没有替当前 TJ375 自动写入覆盖。该差异会使 OpenOCD 在连接前以错误 ID 失败。

因此 IDE 会话的 GDB server/OpenOCD **Other options**（或同义的启动参数编辑框）必须在现有 `ftdi_ti.cfg` 行的**前面**新增且仅新增这一行：

```text
-c 'set CPUTAPID 0x006A0EF3'
```

这只是本次 OpenOCD 进程的 Tcl 变量，不修改 bitstream、仓库、`ftdi_ti.cfg`、Flash 或机械臂。保留模板已有的 `ftdi_ti.cfg + debug_ti.cfg` 两行，不得将它们替换为 `external.cfg` 或自行改写成命令行会话。若 IDE 没有可见/可编辑的 Other options 框，或已有两条 cfg 行不可见，截图并 STOP。

### 片上 RAM work area 冲突修正（强制）

固定 Hello ELF 的入口和已用末端是 `0xF9000000..0xF9000A30`，但生成的 `debug_ti.cfg` 初始 work area 也是 `0xF9000000..0xF90003FF`。OpenOCD 默认不备份 work area，且该区域可用于目标内存批量写入；因此不能让其与程序入口重叠。

受控生成器已在 ASCII 目录创建只含四个 `target configure` 语句的覆盖文件：

```text
<operator-selected ASCII staging directory>/m2_cpuhello_20260716_1730/m2_user2_safe_workarea.cfg
SHA-256 56A4AC97773095CCCE5EC5653D36A784F57119792353963F750F3AE96C4C0E0D
```

它把四个 hart 的 work area 改到未使用尾部 `0xF9000C00..0xF9000FFF`，不含 init/halt/resume/reset/Flash/下载命令。它必须作为 **第三个** cfg 在 `debug_ti.cfg` **之后**加载；不得修改 `debug_ti.cfg` 本身。

## 现场操作

### 1. 镜像并核对 ELF

在 PowerShell 执行；两次输出必须相同且等于表中的完整 ELF SHA-256：

```powershell
$repoRoot = '<repository root>'
$stagingRoot = '<operator-selected ASCII staging directory>'
$src = Join-Path $repoRoot 'competition_project_single_camera\cpu_bringup\uart_hello_onchip\build\uart_hello_onchip.elf'
$dst = Join-Path $stagingRoot 'm2_cpuhello_20260716_1730\uart_hello_onchip.elf'

Get-FileHash -LiteralPath $src -Algorithm SHA256
Copy-Item -LiteralPath $src -Destination $dst -Force
Get-FileHash -LiteralPath $dst -Algorithm SHA256
```

**STOP：**任一 hash 不符，或 ELF 不在上表专属目录。不得换用旧 `30A499...` ELF、任意 `default.ld` 链接产物或其他工程固件。

作为可复核证据，复制后先运行下列只读采集器；只有输出 `ELF_MIRROR_READY_FOR_USER2_RAM_LOAD` 才进入第 2 步。该工具只计算两份 ELF 的 SHA-256 并记录 Git HEAD；不会启动 IDE/OpenOCD/GDB/Programmer、不会打开串口或发送字节。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot 'competition_project_single_camera\tools\capture_m2_user2_elf_preflight.ps1') -MirrorRoot $stagingRoot -RequireMirror
```

当前 ASCII 手工目录已在 2026-07-16 完成镜像与预检：源/目标 SHA-256 均为固定值，最新预检器 `m2_user2_elf_preflight_v2` 的结果为 `ELF_MIRROR_READY_FOR_USER2_RAM_LOAD`。该工具已修复 PowerShell 参数默认值中 `$PSScriptRoot` 过早求值的问题；进入 IDE 前无需重复复制，只有镜像目录被覆盖/重建时才重跑本节。

### 1.5. 准备与当前 USER2 参数一致的完整调试 BSP

仓库提交物只含最小 BSP；在第 2 步前运行下列脚本，使用 Efinity 自带生成器和当前 `settings.json` 向 ASCII 手工目录补齐**调试支持文件与加载前安全的 `debug_ti` 派生文件**。它不会改仓库、XML、RTL、bitstream 或 ELF，不启动 Programmer/OpenOCD/GDB/串口/Flash；运行时若发现 RISC-V IDE、OpenOCD、GDB 或 jtag daemon 已在运行则拒绝写入。

```powershell
$efinityRoot = '<Efinity 2025.2 installation root>'
powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot 'competition_project_single_camera\tools\prepare_m2_user2_debug_bsp.ps1') -TargetRoot $stagingRoot -EfinityRoot $efinityRoot -Apply
```

只有输出 `USER2_HARD_TAP_DEBUG_SUPPORT_READY_TJ375_CPUTAPID_SAFE_DEBUG_CFG_AND_RUNTIME_PC_GATE_REQUIRED` 才可继续。该输出必须同时证明 `INTF_JTAG_TAP_SEL=9`、`DEVICE=TJ375N529`、TJ375 的 `0x006A0EF3` 官方 ID、`debug_ti.cfg` 的 `riscv set_bscan_tunnel_ir 9`、`ftdi_ti.cfg` 的 `0403:6011/channel 1`、预定义 `CPUTAPID` 的支持、Hello map 的 `0xF9000000..0xF9000A30` 占用范围、以及官方 `ti.ftl` 的 `Debug in RAM + load image/symbols + 不手设 PC + stop at main` 组合。更关键的是，生成的 `debug_ti_m2_safe.cfg` 必须与官方 `debug_ti.cfg` 完全相同，**仅**将三处 `work-area` 从 `0xF9000000` 改到 `0xF9000C00`，因而这一地址在其 `init/halt` 之前已经生效。

**重要修正：**生成的 `debug_ti.cfg` 与 `debug_softTap.cfg` 都含历史声明 `set instr_addr 0x00001000`；静态检索确认 `debug_ti.cfg` 与 `ftdi_ti.cfg` 没有引用该变量，官方 `ti.ftl` 也没有消费它，但这不是运行时安全证明。不得手改生成文件来删除该字段；必须在第 3 步下载后、任何 Resume 前实际读取 PC 并按范围验收。

当前 ASCII 手工目录已在 2026-07-16 完成一次受控生成。进入 IDE 前无需重跑；只有手工目录被覆盖/重建且尚未启动调试工具时，才重新执行本步骤。

### 2. 建立硬件调试会话

1. 启动 Efinity RISC-V IDE，打开 `Run → Debug Configurations...`。
2. 仅选择名称明确为 **TI / Titanium（硬 TAP）** 的 Efinix/QCRV32 硬件调试模板；若只出现 SoftTap、External、Ti375C529 devkit/USER1，或名称无法区分，先截图并 STOP，不猜选。
3. 选择 TI/Titanium 硬 TAP 模板后，在其可编辑的 OpenOCD config entries / Other options 中，令**完整配置链只包含**下列顺序。不要保留或隐含加载原始 `debug_ti.cfg`；不要手工换成 `external.cfg`，不要只加载单个 cfg，也不运行命令行 OpenOCD。

   ```text
   -c 'set CPUTAPID 0x006A0EF3'
   -f '<operator-selected ASCII staging directory>/embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/openocd/ftdi_ti.cfg'
   -f '<operator-selected ASCII staging directory>/embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/openocd/debug_ti_m2_safe.cfg'
   ```

4. 第二个 `-f` 必须是下列派生文件，不是官方原始 `debug_ti.cfg`：

   ```text
   <operator-selected ASCII staging directory>/embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/openocd/debug_ti_m2_safe.cfg
   ```

   它的 SHA-256 必须为 `9CAD838541A9E9F8CA489F90693AECEAA5E0CB6ED75745E7EBDA72EBD52686D5`。`C/C++ Application` / `ELF` 字段选择上表 ASCII 镜像 ELF。确认配置语义为 `Debug in RAM`、加载 image 与 symbols、不手工设置 PC、`stop at main`。
5. 确认不勾选 Flash erase/program、SPI/PROM、boot image、外部 DDR initialization 或用户自定义启动地址覆盖。
6. 保存配置页截图：必须同时可读到 ELF 路径、TI/Titanium 硬 TAP、RAM/debug 语义、完整 Other options（含 TJ375 覆盖、`ftdi_ti.cfg`、`debug_ti_m2_safe.cfg`）；若 UI 显示 USER TAP，必须为 `USER2`。

**STOP：**显示 `USER1`、SoftTap、External、Ti375C529 devkit、目标不是 TJ375/QCRV32、缺少或改写了 `-c 'set CPUTAPID 0x006A0EF3'`、原始 `debug_ti.cfg` 被加载、未加载 `debug_ti_m2_safe.cfg`、加载旧 `m2_user2_safe_workarea.cfg`、出现 Flash/DDR 选项，或要求手工把 PC/入口改为 `0x00001000`。

### 3. 只下载并停机取证

1. 点击 Debug/连接；允许 IDE 按官方模板加载 ELF 和 symbols。
2. 仅接受下载到 RAM 后，调试器自动停在 `_start` 或 `main`，且 PC/反汇编地址为 `0xF9000000..0xF9003FFF`。此项是旧 `instr_addr` 声明的**运行时验证**，不是推测。
3. 不点击 Resume，不打开任何串口，不发送字节。
4. 保存下载成功的 Console 全文和调试器截图（含 USER2、ELF 路径、PC 或当前源码位置）。

**STOP：**OpenOCD/GDB 连接失败、下载失败、PC 为 `0x00001000` 或任何非 `0xF9000000..0xF9003FFF` 地址、自动切至 USER1/SoftTap/External、或任何 Flash/DDR 行为。停止后仅回传日志/截图；不降级为其他 TAP 或加载其他 ELF。

## 回传证据与本卡验收

保存到 `docs/debug_sessions/evidence/m2_uart0_user2_ram_YYYYMMDD_HHMMSS/`：

1. 源/镜像 ELF hash 输出；
2. `capture_m2_user2_elf_preflight.ps1 -RequireMirror` 生成的 JSON；
3. `prepare_m2_user2_debug_bsp.ps1 -Apply` 生成的 JSON；
4. USER2/Target/RAM 配置截图；
5. 下载成功 Console 与暂停在 `_start`/`main` 的截图；
6. 发生时间、实际状态、错误（如有）。

只有 Codex 复核这三项证据后，才另行发出 UART0 的“监听横幅”操作卡；本卡通过也不等同于 UART0、CPU Hello 完整 Gate 或任何机械臂 Gate 通过。
