# M2 UART0 CPU Hello — USER2 调试配置离线审计

> 日期：2026-07-16
> 范围：仅验证当前候选 Hard SoC 的调试 TAP 选择依据；不连接 OpenOCD/GDB、不加载 ELF、不打开串口。

## 结论

当前候选工程的 QCRV32 调试 TAP 是 **`JTAG_USER2`**，不是由历史操作卡推测得出，而是由当前生成参数、Efinity 官方参数枚举和当前 Periphery 三处一致证明。

| 层 | 当前证据 | 含义 |
|---|---|---|
| Hard SoC 生成参数 | `ip/EfxSapphireHpSoc_slb/settings.json` 与 `hard_ip_args.ini` 均为 `INTF_JTAG_TAP_SEL=9` | 当前工程输入 |
| Efinity 2025.2 官方 IP | `ip_component.xml` 的 `TAP_SEL` 枚举与 `generator/hard_ip_generation.py` 都映射 `9 → JTAG_USER2` | 官方解释 |
| 当前 Periphery | `mem_test.peri.xml` 的 `soc_jtag_inst1` 为 `jtag_def="JTAG_USER2"` | 当前工程实际资源 |

因此 `m2_uart0_user2_ram_download_operator_card_20260716.md` 中 USER2 是强制值；出现 USER1、USER3 或 USER4 都不能继续。

## 不可复用的官方样例

Efinity 安装目录下的 `Ti375C529_devkit` Hard SoC 示例的 `debug_profile_hard.cfg` 为 `INTF_JTAG_TAP_SEL=8`。官方同一枚举明确 `8 → JTAG_USER1`；该示例不是当前 `TJ375N529` 候选的启动配置，禁止复制其 `debug_profile_hard.cfg`、`.launch` 或 OpenOCD 文件来“补齐”本 Gate。

## 最小 BSP 的边界

仓库提交物只保留 Hello 构建所需的最小 BSP，未包含生成的 `openocd/` 目录、`debug_profile_hard.cfg` 或可直接导入的 `.launch` 文件。这不是 USER2 未生成的反证：这些文件不在提交的最小 BSP 清单内。ASCII 手工目录随后已通过本记录的受控生成步骤补齐调试支持；该外部目录不改变仓库最小 BSP 的提交身份。

现场应在 Efinity RISC-V IDE 中使用与当前 Target/USER2 显式匹配的硬件调试界面建立一次性会话。若界面要求手工指定 OpenOCD/launch 配置文件，而不能显示 `TJ375/QCRV32 + USER2 + Debug in RAM` 的可核对组合，立即 STOP 并回传配置类型列表；不得用上述 USER1 官方样例替代。

## 预检器调用边界

`capture_m2_user2_elf_preflight.ps1` 位于候选工程的 `tools/` 下，而仓库根目录没有同名工具。操作卡使用该脚本的绝对候选工程路径，避免从仓库根目录执行相对 `./tools/` 时得到“找不到脚本”的误导性失败。该修正仅影响操作卡定位，不改变预检器、ELF 或硬件状态。

## 官方完整调试 BSP 的受控生成

当前 ASCII 手工目录已通过 `prepare_m2_user2_debug_bsp.ps1 -Apply` 使用 Efinity 2025.2 的官方 `sw_script.py` 生成完整调试 BSP；生成器 exit 0。该执行只写入手工目录的 `embedded_sw/`，没有启动 Programmer、OpenOCD、GDB 或串口。

生成结果：

- `debug_profile_hard.cfg`：`INTF_JTAG_TYPE=0`、`INTF_JTAG_TAP_SEL=9`、`DEVICE=TJ375N529`；SHA-256 `498B4BF68D02452914F987C31AC0348E8576D501A6A3E2AC1DF7D67328EA2040`。
- 硬 TAP 的 `debug_ti.cfg`：含 `riscv set_bscan_tunnel_ir 9` 且无 `?` 占位符；SHA-256 `60B754D1FC0D3F38B0939D8087A481627E0AC31C39CED2A43834A48BF0F60DE2`。
- **关键更正：**`debug_ti.cfg` 和 `debug_softTap.cfg` 都含历史声明 `set instr_addr 0x00001000`；不能再把该字段仅归因于 SoftTap。两份 cfg 的 SHA-256 分别为 `60B754D1FC0D3F38B0939D8087A481627E0AC31C39CED2A43834A48BF0F60DE2` / `22C0F75743AB4B454794C072F7904DFECF20C96A1C49260B92F6050FAAB08332`。
- `ftdi_ti.cfg` 是官方 Titanium 硬 TAP 模板实际引用的前段，包含 `vid_pid 0403:6011` 与 `channel 1`；SHA-256 `934FF4B47010461DB627A74BF8D5124CCD21CDAB64FE9080B06CEB395C9298FE`。`external.cfg` 不属于该模板链；`external.cfg` / `jtag_daemon.cfg` 分别为 `80B9328C72266C02C4BB8C376D3A9FDD3FF99F3438329843B6DF62884C1121F5` / `DAC0F2648186800008E4D09753D7AF7D4CE249DEFF09940A0D38C0C0E54979A0`。

## 官方硬 TAP 模板链与运行时 PC 门

Efinity RISC-V IDE 2025.2 的官方 `templates/launch_configurations/ti.ftl` 组合 `ftdi_ti.cfg` 与 `debug_ti.cfg`，且静态属性为：`Debug in RAM=true`、加载 image/symbols、`setPcRegister=false`、`stop at main`。该模板不是 `Ti375C529_devkit` USER1 示例；它通过本批生成的 `debug_ti.cfg` 中 `riscv set_bscan_tunnel_ir 9` 进入 USER2。

对 ASCII 手工目录的文件清单检索未发现 `.launch` 或 Eclipse 项目元数据。此结果与生成器只产出 BSP/OpenOCD 支持文件的职责一致：本 Gate 不创建、不导入、不复制任何 launch 文件，而是在 IDE 内选择官方 TI/Titanium 硬 TAP 模板；若该模板在当前 IDE UI 中不可见，必须停止并回传 UI 截图。

对完整硬 TAP 配置链的文本检索显示：上述 `instr_addr` 仅是 `debug_ti.cfg` 中的声明，`debug_ti.cfg`、`ftdi_ti.cfg` 和 `ti.ftl` 均没有 `$instr_addr` / `${instr_addr}` 消费点。**这是静态事实，不是 OpenOCD 运行时行为的证明。**因此禁止手工删除或改写该声明；唯一可接受的闭环是在加载固定 ELF 后、任何 Resume 或串口动作前，实际截图确认调试器 PC/反汇编落在 `0xF9000000..0xF9003FFF`。出现 `0x00001000` 或其他地址必须停止，不得改用 SoftTap、External、USER1 或另一份 ELF。

## TJ375 CPUTAPID 差异与受控覆盖

当前仓库与 ASCII 手工目录的 `mem_test.xml` 都是 `TJ375N529`。Efinity 2025.2 的 `device_db.xml` 将 `TJ375` 映射为 `0x006A0EF3`，官方 `EFX_JTAG_CTRL.v` 也对 `TJ375N529` 设置 `IDCODE_value = 32'h006A0EF3`；这与操作员 Programmer 截图实读的 `0x006A0EF3` 一致。

但官方 Hard SoC BSP 的 `ftdi_ti.cfg` 默认值是 `set _CPUTAPID 0x006A0A79`。Efinity 的 JTAG ID 表将该值列为 **Ti375**，不是 TJ375。`debug_ti.cfg` 使用该变量作为 `jtag newtap ... -expected-id $_CPUTAPID`，而官方 `ti.ftl` 只加载 `ftdi_ti.cfg + debug_ti.cfg`，没有 `CPUTAPID` 设置。因此，若不覆盖，硬 TAP 会按 Ti375 ID 连接 TJ375，静态上存在明确的不匹配风险。

`ftdi_ti.cfg` 本身提供了官方覆盖钩子：若 Tcl 变量 `CPUTAPID` 已存在，就以它设置 `_CPUTAPID`。本 Gate 只允许在 IDE 的官方 TI/Titanium 模板 `Other options` 第一行加入 `-c 'set CPUTAPID 0x006A0EF3'`，随后加载 `ftdi_ti.cfg` 与本记录后述的 `debug_ti_m2_safe.cfg`。这个覆盖仅作用于本次 OpenOCD 进程；不得编辑生成 cfg、改用 External/SoftTap，或运行命令行 OpenOCD。覆盖后仍必须通过运行时 PC 门，不能把“可连接”误作 CPU 取指成功。

## 片上 RAM work area 冲突与受控尾部覆盖

固定 Hello 的当前 linker map 显示 `_start=0xF9000000`、`__freertos_irq_stack_top=0xF9000A30`。但官方生成的 `debug_ti.cfg` 的四-hart 控制流含三处文本 `target create`（cpu0、SMP 的 cpu1..3 循环、非 SMP 的 cpu0..3 循环），三处均设定 `-work-area-phys 0xF9000000 -work-area-size 1024`，并且在其对应分支的 `init/halt` 之前执行；这会与 Hello 入口范围重叠。OpenOCD 官方文档说明 work area 可用于包括目标内存批量写入在内的操作，且默认不备份；它还特别建议不要选择通常会被构建系统使用的 SRAM 起始部分。因此不能把该初始 work area 留在 Hello 入口。

初版 `m2_user2_safe_workarea.cfg`（SHA-256 `56A4AC97773095CCCE5EC5653D36A784F57119792353963F750F3AE96C4C0E0D`）是后加载的第三个 cfg；它只能在原始 `debug_ti.cfg` 的 `init/halt` 之后重新配置目标，存在加载时序窗口，故现已**作废且不得加载**。受控生成器现在保留官方 `debug_ti.cfg` 不变，同时在 ASCII BSP 目录生成 `debug_ti_m2_safe.cfg`（SHA-256 `9CAD838541A9E9F8CA489F90693AECEAA5E0CB6ED75745E7EBDA72EBD52686D5`）。该文件与源 `debug_ti.cfg`（SHA-256 `60B754D1FC0D3F38B0939D8087A481627E0AC31C39CED2A43834A48BF0F60DE2`）逐字节等价，唯一差异是把上述**三处** work area 从 `0xF9000000` 替换为 `0xF9000C00`；因此所有 `target create` 在 `init/halt` 前即使用尾部地址。`0xF9000C00..0xF9000FFF` 位于 16 KiB RAM 内、在 Hello 已用范围之后。IDE 必须只加载 `ftdi_ti.cfg` 后接 `debug_ti_m2_safe.cfg`，不得加载原始 `debug_ti.cfg` 或旧的后置文件。

该派生不会运行 CPU 或触碰 Flash，但只有配置实际加载并在下载后通过 PC 取证，才证明 RAM 下载没有受到 work area 覆盖。若用户更新 ELF、其 map 高水位超过 `0xF9000C00`，或生成器/模板变更，必须重新审计该区间。

本记录只证明硬 TAP 调试支持文件、TJ375 CPUTAPID 覆盖、安全 RAM work area 与官方模板链和当前 USER2 参数静态匹配，并要求运行时 PC 门；不证明调试器已连接、ELF 已下载、CPU 已执行或 UART0 已输出。
