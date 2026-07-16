# 单摄 Hard SoC 2026-07-16 全日改动与主分支合并交接

> 日期：2026-07-16（Asia/Shanghai）
> 当前工作树身份：本机 APB0/REG_MAGIC ASCII 隔离 worktree
> 当前分支：`codex/hard-soc-apb-magic-20260716`
> 当前已推送提交：`78547786584dc6b861bb530574ffa12fb01fe07e`
> 当前远端主线：`origin/main@07373042d1f84cdc048fc42b5752d0cbeb52c471`
> 用途：供队友审核、合并、继续开发和后续本机 Efinity 构建/烧录使用。

## 0. 合并最高原则：Hard SoC/IP/工程配置以本机内容为优先真源

**合并 `competition_project_single_camera/` 时，涉及 Hard SoC、IP 生成物、Interface Designer、顶层连线、时钟复位、JTAG、UART0、APB、BSP 和工程 XML 的冲突，优先保留本机当前内容；队友应把自己的 CPU、算法或功能修改适配到本机内容上，不能用队友分支中的旧 IP 配置反向覆盖本机。**

原因：

- 本机已经使用 Efinity `2025.2.288.4.15` 完成完整 Hard SoC IP Manager 生成、Interface Designer 适配、Map、Interface、PNR、STA、CDC 和 bitstream generation。
- 本机保存了当前最完整且同批匹配的 `settings.json + hard_ip_args.ini + wrapper/生成 RTL + .peri.xml + mem_test.xml + soc.h/BSP + top.v`。
- 本机已经完成多个真实板级批次的 FPGA SRAM 临时加载、HDMI、USER2、片上 RAM、CPU 取指和 UART0 验证；后续 bitstream 很可能继续由本机生成和上板。
- Hard SoC 生成文件必须同批使用。混用队友旧 `settings.json`、旧 wrapper、旧 `soc.h`、旧 `.peri.xml` 或旧 `top.v`，可能造成接口、地址、JTAG TAP、PLL、复位和 BSP 不一致，即使 Git 能合并也不能作为可烧录工程。

本机优先范围：

```text
competition_project_single_camera/mem_test.xml
competition_project_single_camera/mem_test.peri.xml
competition_project_single_camera/constrain.sdc
competition_project_single_camera/src/top.v
competition_project_single_camera/src/apb_reg_magic.v
competition_project_single_camera/ip/EfxSapphireHpSoc_slb/**
competition_project_single_camera/embedded_sw/efx_hard_soc/**
competition_project_single_camera/cpu_bringup/uart_hello_onchip/**
```

这不是“所有冲突无条件选 ours”：

- `CURRENT_STATE.md`、`WORK_LOG.md` 和 Review Packet 要按时间和证据语义合并，不能删除队友已经完成的新验证。
- 队友新增且不冲突的 CPU 分类器、Host 测试、任务状态机、协议或文档可以正常保留。
- 如果队友确实修改了上述系统级文件，先提取其功能意图，再把意图重新适配到本机工程，不能直接整文件覆盖。
- 本机当前 4 个文件的工作树 dirty 仅是 LF/CRLF 表示差异，`git diff --ignore-space-at-eol` 为零；它们不是功能改动，不应作为功能提交。

## 1. 今天的 Git 路线与提交总账

### 1.1 Hard SoC 可复现真源同步，已经进入 main

今天首先从 `origin/main@4e35b05453c1cd30c943bb3d567fd0316ca6bdde` 建立个人分支并同步完整可复现输入：

| 提交 | 内容 | 当前状态 |
|---|---|---|
| `2d4b3b7b3d0c59d88ece0669534ae38de02ed938` | 同步单摄 Hard SoC/IP/BSP/UART0 Hello/顶层/约束真源 | 已通过 PR #9 进入 main |
| `0604d33f3fe76851e1dfe738403875b4a7d0721c` | 记录来源、工程契约、构建证据和未验证门 | 已通过 PR #9 进入 main |
| `1fa7e769cf302e7265d942e2370e90c147889f7f` | 集成可复现 Hard SoC 真源 | 合并分支提交 |
| `f25dae4b2bced8f7bd815dde335fd90c5eb6e380` | 对齐 Hard SoC 集成门禁文档 | 合并分支提交 |
| `07373042d1f84cdc048fc42b5752d0cbeb52c471` | PR #9 merge commit | 当前远端 main |

这一阶段新增或同步的核心文件：

- `mem_test.xml`、`mem_test.peri.xml`、`constrain.sdc`、`src/top.v`。
- `ip/EfxSapphireHpSoc_slb/` 下 Hard SoC `settings.json`、定义、模板、wrapper、生成 RTL 和参数文件。
- `embedded_sw/efx_hard_soc/` 下最小可复现 BSP。
- `cpu_bringup/uart_hello_onchip/` 下 Hello 源码、Makefile、`build.ps1` 和 README。
- 最小 BSP 依赖：`bsp.mk`、`riscv64-unknown-elf.mk`、`standalone.mk`、`start.S`、`soc.mk`、`soc.h`、`default_i.ld`。
- `WORK_LOG.md` 和 `m2_hard_soc_source_sync_review_20260716.md`。

因此，队友不需要再次合并 `2d4b3b7/0604d33`；它们已经进入 main。后续合并重点是 main 之后的板测证据和 APB0 `REG_MAGIC` 分支。

### 1.2 main 之后的板测证据与 APB 审核提交

当前 APB 分支相对 `origin/main@0737304` 还包含：

| 提交 | 内容 |
|---|---|
| `4ff885c4858f3e58dd77e3cca44b649f6f93ca22` | 记录来源匹配 bitstream 的 USER2/CPU/UART0 板级门 |
| `fb815b6ab9321124e8b311e1b6113e84f38be9d5` | 将板测证据与 main 合并对齐；父提交含 `0737304` |
| `108c1f2c9306174bb7e9bb013c39f89ce430a782` | 区分 bitstream 全文件哈希和配置载荷身份 |
| `d6f288ab6017bee15e14b25850d69b074ac16cc9` | 关闭精确 main 重建 bitstream 的 CPU/UART0 板级门 |
| `db94aa4a38a02c2ea3a738cf9d209f193b4b7526` | 审核并记录当前旧 bitstream 不存在 APB0/REG_MAGIC |
| `78547786584dc6b861bb530574ffa12fb01fe07e` | 启用 APB0、生成同批 IP/BSP、接入只读 REG_MAGIC 并通过完整离线构建 |

不要只 cherry-pick 最后的 `7854778` 后丢弃前面的事实提交，否则 `CURRENT_STATE.md` 和 `WORK_LOG.md` 会缺少必要的启动方法、bitstream 身份和板测证据。建议直接以整个远端分支 `origin/codex/hard-soc-apb-magic-20260716` 提 PR，再按最新 main 做语义冲突处理。

## 2. 今天修改的工程内容

### 2.1 可复现 Hard SoC 联合工程

完成了从“只有本地来源副本”到“仓库内可复现联合工程”的同步：

- `mem_test.xml` 同时登记 `feature_stats_tap.v` 和 `EfxSapphireHpSoc_slb`。
- `debugger.auto_instantiation=off`，防止 Efinity 自动生成另一套调试结构。
- 视频 JTAG 保持 `USER1`；QCRV32 固定 `USER2`。
- UART0 和 SoC reset 的 Interface Designer 管脚进入 `.peri.xml`。
- 保留视频 DDR 配置状态机为 DDR `CFG_START/RST/SEL` 的唯一驱动；Hard SoC 只观察共享 `CFG_DONE`，避免双驱动。
- AXI1 保持关闭，未添加第二套 DDR 控制器。
- Hard SoC IP 的 `--base_path` 从来源副本绝对路径改为仓库相对 `..`，这是唯一刻意的可移植路径修改。

### 2.2 最小片上 RAM UART0 Hello

新增并验证：

- 应用入口和唯一 LOAD 段位于片上 RAM `0xF9000000`。
- 片上 RAM 大小 16 KiB，末地址 `0xF9003FFF`。
- UART0 MMIO `0xE8010000`，115200 8N1。
- 构建脚本会检查 BSP 完整性、ELF 入口、LOAD 段越界、未解析符号和本机工具链。
- 使用 `-DSMP` 启动代码，但调试时不得使用 OpenOCD SMP 聚合 resume；只运行 CPU0，CPU1..3 保持 halt。
- 不访问外部 DDR、UART2 或机械臂。

### 2.3 APB0 `REG_MAGIC`

本机通过 Efinity IP Manager 在现有 Hard SoC 上执行受控 Generate：

- 启用 APB3 interface 0。
- APB0 窗口为 4096 bytes。
- APB1-4 继续关闭。
- UART0、USER2、PLL、DDR、AXI 和其他管脚保持原配置。
- Generate 后生成器把 `Periphery Controller Clock Pin Name` 重置，本机已恢复为 `axi0_ACLK`。
- BSP 新增 `IO_APB_SLAVE_0_INPUT=0xE8100000` 和大小 4096。

新增 `src/apb_reg_magic.v`：

| 访问 | 返回/行为 |
|---|---|
| `0xE8100000` 只读 | `0x375A0001`，`PREADY=1`，无 error |
| 偏移 `0x000` 写 | 无副作用，access phase 返回 `PSLVERROR=1` |
| 其他偏移读/写 | 返回 0，access phase 返回 `PSLVERROR=1` |

顶层 `src/top.v` 将 APB0 的 12 位窗口内地址接到从机；`PWDATA` 对只读从机显式不接，消除了首轮 Map 的 32 条无用 `pwdata[x]` warning。

新增 `tests/apb_reg_magic/tb_apb_reg_magic.v`，覆盖 idle、SETUP、合法读、非法写和非法偏移。由于本机当前没有 Icarus、Verilator 或 ModelSim/Questa，状态仍是 `NOT RUN`，不能写成仿真 PASS。

### 2.4 文档和配置索引

今天新增或持续更新：

- `CURRENT_STATE.md`
- `competition_project_single_camera/WORK_LOG.md`
- `docs/review_packets/m2_hard_soc_source_sync_review_20260716.md`
- `docs/review_packets/m2_hard_soc_apb_magic_offline_review_20260716.md`
- `docs/debug_sessions/hard_soc_board_config.md`
- 本交接文件 `docs/review_packets/m2_hard_soc_daily_merge_handoff_20260716.md`

`hard_soc_board_config.md` 已集中保存 bitstream/ELF 哈希、FTDI、USER2、4 hart、NDMRESET、片上 RAM、UART0、APB0、验收顺序和禁止项。后续联调先读该文件，不再从聊天记录反复翻配置。

## 3. 本机当前完整 IP 与接口配置

### 3.1 Hard SoC IP Manager 生成参数

| 类别 | 本机当前值 |
|---|---|
| IP | `efinixinc.com:soc:efx_hard_soc:1.22.0` |
| Efinity | `2025.2.288.4.15` |
| `PERI_APB_0` | 开启 |
| `PERI_APB_0_SIZE` | 4096 |
| `PERI_APB_1..4` | 关闭 |
| `PERI_UART_0` | 开启 |
| `PERI_UART_1/2` | 关闭 |
| GPIO/SPI/I2C/WDT/SDHC/TSEMAC | 关闭 |
| FPGA JTAG TAP | `USER2`，`INTF_JTAG_TAP_SEL=9` |
| `PERI_FREQ` | 200 MHz |
| AXI stream interface | 开启 |
| AXI pipeline/write buffer | 关闭 |
| DDR | 32-bit、8G、LPDDR4x、1 rank |
| DDR AXI1 | 关闭 |
| System PLL resource | `PLL_BL1` |
| Memory/peripheral PLL resource | `PLL_TR0` 字段保留，但不允许自动生成另一套冲突 PLL |
| LPDDR4 PLL resource | `PLL_BL2` |
| FTDI target channel | 1 |
| Board | `Ti375C529 Development Kit` |
| package/family | 529 / TITANIUM |

重要：`settings.json`/`hard_ip_args.ini` 中的 `SYS_FREQ=1000`、`MEM_FREQ=250`、`PERI_FREQ=200` 是 Hard SoC 生成器参数；它们不能替代 Interface Designer 的真实外部时钟连接。队友不得只根据单个 JSON 数值重配 PLL。

### 3.2 Interface Designer 和实际工程契约

| 项目 | 本机实际连接 |
|---|---|
| 器件 | TJ375N529，IDCODE `0x006A0EF3` |
| 视频 JTAG | `JTAG_USER1`，保留给视频工程 |
| CPU JTAG | `JTAG_USER2`，IR=9 |
| UART0 RX | `GPIOR_165`，3.3 V LVCMOS |
| UART0 TX | `GPIOR_145`，3.3 V LVCMOS |
| SoC reset 输入 | `GPIOL_79`，3.3 V LVCMOS |
| `soc_system_clk` | 594 MHz，来自现有 `PLL_BL1 CLKOUT2` |
| `soc_memory_clk` | 237.6 MHz，来自现有 `PLL_BL1 CLKOUT1` |
| `io_peripheralClk` | 复用现有 `axi0_ACLK=200 MHz` |
| `pll_system_locked` | `sys_pll_lock` |
| `pll_peripheral_locked` | `ddr_pll_lock` |
| DDR 配置控制 | 视频状态机唯一驱动 `CFG_START/RST/SEL`；SoC 只读 `CFG_DONE` |

曾尝试的 `soc_memory_clk=297 MHz` 已因超过 QCRV32 250 MHz 规格上限废止；当前合法值是 237.6 MHz。任何队友分支中的 297 MHz、自动生成 System PLL/Peripheral PLL 或第二套 LPDDR4 controller 都不得合入。

### 3.3 BSP/MMIO 契约

| 名称 | 地址/参数 |
|---|---|
| 片上 RAM | `0xF9000000`，16 KiB |
| UART0 | `0xE8010000`，115200 8N1 |
| APB0 | `0xE8100000`，4 KiB |
| `REG_MAGIC` | `0xE8100000 + 0x000`，期望 `0x375A0001` |
| CLINT | 200 MHz |
| AXI-A/BMB | `0xE8000000` |

### 3.4 JTAG/OpenOCD 已验证配置

| 项目 | 当前值 |
|---|---|
| FTDI VID:PID | `0403:6011` |
| OpenOCD channel | 1 |
| layout | `ftdi layout_init 0x08 0x0b` |
| BSCAN tunnel | `riscv use_bscan_tunnel 6 1` |
| USER2 IR | `riscv set_bscan_tunnel_ir 9` |
| CPU | 4 个独立 RV32 hart，XLEN=32，`misa=0x4004112d` |
| CPU0 dbgbase | `0x10B80000` |
| CPU1/2/3 dbgbase | `0x10B81000/0x10B82000/0x10B83000` |
| 当前 UART0 | COM10；USB 重插后必须重新枚举 |

启动前必须执行 NDMRESET：

```tcl
riscv dmi_write 0x10 0x00000003
sleep 100
riscv dmi_write 0x10 0x00000001
sleep 200
```

随后 halt 4 核、下载/校验 ELF 到 `0xF9000000`、保持 CPU1..3 halt、设置 CPU0 PC、只 resume CPU0。禁止使用 `RUN_SMP_APP` 聚合 resume；它已实测会出现 `pc=0`、`mcause=1`。

## 4. 今天已经完成的验证

### 4.1 可复现真源与离线构建

- 完整 Hard SoC/IP/BSP/顶层/约束已落库，不再存在“Hard SoC 真源缺失”阻塞。
- 精确 main 重建 Map、Interface、PNR、STA、CDC 和 bitstream generation 已通过。
- 精确 main 重建 bitstream `6B672889...` 已通过真实摄像头 HDMI、USER2、4 hart、NDMRESET、片上 RAM、CPU0 取指、UART0 完整横幅和 ASCII `K` 回显。
- 由此已经证明单摄视频和 Hard SoC CPU 能在同一 bitstream 中联合运行。

### 4.2 APB0/REG_MAGIC 离线门

- APB0 由当前工程的 IP Manager 正式生成，不是从旧 A3 或其他工程拼接。
- Map/Interface/PNR/STA/CDC/bitstream generation 全通过。
- Worst Setup/Hold：`+1.321 ns / +0.026 ns`。
- CDC：`No Synchronizer warnings to report.`。
- Interface：`0 error / 4` 个既有 MIPI/LVDS 距离 warning。
- post-synthesis netlist 仍有 118 个继承 warning；不能与 Interface 4 项混写。
- APB 资源增量：LUT4 `+181`、FF `+147`，ADD/RAM10/DPRAM10 不变。

### 4.3 当前 APB bitstream 的板级进度

当前本地产物：

```text
mem_test.bit
size: 11847132 bytes
SHA-256: 138F435C7F2B6CA2EDA2605CABCBA1C44D3C0BD6E3A7AE1D54D244DA12277D15
```

对应 Hello ELF：

```text
SHA-256: 67A1AE3CFB7D8388CD3C8D14906FBF6F4128EF2722AEC0A8BF145F27C6050B26
entry: 0xF9000000
LOAD: 0xF9000000..0xF9000A30
size: 2608 B / 16 KiB
ELF_LOAD_AUDIT=PASS
```

本轮板级已经完成：

- 开发板完全断电后冷启动。
- JTAG 链识别 1 个 TJ375，IDCODE `0x006A0EF3`。
- `138F435C...` 仅通过官方 JTAG programmer 临时加载到 FPGA SRAM，未写 Flash。
- 用户确认真实摄像头 HDMI 画面正常。
- 用户确认 CDONE 点亮。

当前状态：

```text
APB BITSTREAM JTAG SRAM LOAD = PASS
HDMI = PASS
CDONE = PASS
CURRENT 138F... USER2/CPU/UART0 = NOT VERIFIED
CURRENT 138F... APB REG_MAGIC READ = NOT VERIFIED
```

## 5. 合并 main 的具体注意事项

### 5.1 推荐合并方式

1. 从最新 `origin/main` 建立新的审核/集成分支。
2. 合并整个 `origin/codex/hard-soc-apb-magic-20260716`，不要只复制最后几个文件。
3. 对 `CURRENT_STATE.md`、`WORK_LOG.md` 和 Review Packet 做语义合并，保留 main 上更新的团队事实。
4. 对 Hard SoC/IP/工程配置冲突按第 0 节执行：本机内容优先，队友功能适配本机。
5. 合并后在本机重新生成批次哈希并运行完整 Efinity，不继承合并前 bitstream 的板级结论。
6. 只提交源码、配置和文档；bitstream、ELF、hex、bin、map、rpt、log、outflow、work 和本机路径继续不提交。

### 5.2 必须原子保留的一组文件

以下文件是同一次 IP Manager Generate 的匹配集合，禁止逐个从不同分支挑选：

```text
ip/EfxSapphireHpSoc_slb/settings.json
ip/EfxSapphireHpSoc_slb/hard_ip_args.ini
ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb.v
ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_wrapper.v
ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb_tmpl.v
ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb_tmpl.vhd
ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb_define.vh
ip/EfxSapphireHpSoc_slb/source/Axi4PeripheralTop.v
ip/EfxSapphireHpSoc_slb/source/peri_config
embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/include/soc.h
mem_test.peri.xml
mem_test.xml
src/top.v
```

如果必须重新 Generate，应在本机当前工程中使用同一 Efinity 版本重新生成整组，再复核：

- `PERI_APB_0=1`、size=4096、APB1-4=0。
- `IO_APB_SLAVE_0_INPUT=0xE8100000`。
- `USER2` IR=9。
- UART0 RX/TX 管脚不变。
- `io_peripheralClk=axi0_ACLK`。
- `soc_system_clk=594 MHz`、`soc_memory_clk=237.6 MHz`。
- DDR 配置无双驱动，AXI1 关闭。
- `src/apb_reg_magic.v` 仍登记在 `mem_test.xml`。

### 5.3 `mem_test.xml` 格式噪声

Efinity 保存 APB 工程时重排了 `mem_test.xml` 的 XML 属性和空元素格式，因此该文件 diff 较大；真正功能变化是登记 `src/apb_reg_magic.v`、保留 Hard SoC IP 和更新工程状态。审核时不要因为格式行数多就删除整个本机 XML，也不要用旧 XML 覆盖。

### 5.4 生成 wrapper 的 warning

官方外层 wrapper 导出 `PADDR[31:0]`，内部 4 KiB APB0 为 `[11:0]`，产生 1 条 32->12 位 warning。BSP 窗口、内部接口和顶层低 12 位译码一致，已记录并通过 Map/PNR/STA/CDC。禁止手改生成 wrapper 只为消除 warning；重新 Generate 会覆盖且可能破坏生成一致性。

### 5.5 本次交接提交范围与本机换行标记

本次交接提交只应包含：

- `CURRENT_STATE.md` 的当前 APB bitstream 板级状态。
- `WORK_LOG.md` M2-40。
- APB Review Packet 的板级增量。
- `docs/debug_sessions/hard_soc_board_config.md`。
- 本交接文件和 `docs/README.md` 入口。

本机另有 4 个仅 LF/CRLF 工作树标记、无功能文本差异的文件：`default_i.ld`、`start.S`、`EfxSapphireHpSoc_slb_define.vh`、`ipm_pt_map.json`。它们不得混成“IP 功能更新”提交，也不得为得到干净状态而执行破坏性清理。

## 6. 距离 CPU 识别算法和比赛任务闭环还有什么

当前不是“只差把识别 ELF 烧进去”。已经闭环的是视频 + Hard SoC CPU 启动底座；完整比赛闭环至少还需要以下 5 个工程 Gate。

### Gate 1：关闭当前 APB MAGIC 板级门

对当前 `138F435C...` 执行：

1. USER2 识别 4 hart。
2. NDMRESET。
3. 下载并校验当前 `67A1AE...` Hello ELF 到片上 RAM。
4. 只 resume CPU0，确认 UART0 三行横幅和 ASCII `K` 回显。
5. 仅执行一次 `mdw 0xE8100000 1`，必须得到 `0x375A0001`。

在此 Gate 通过前，不进入 feature、OSD、UART2 或机械臂。

### Gate 2：feature snapshot APB/CDC

当前真实断点：

- `feature_stats_tap.i_capture_enable = 1'b0`。
- `i_ack_valid = 1'b0`。
- 所有 feature 输出均连接到 `*_unused`。
- 当前 APB0 只有 `REG_MAGIC`，没有 snapshot、frame_id、color area、foreground、luma、bbox、source flags 或 ACK 寄存器。

需要完成：

- 定义正式 feature 寄存器表和版本/状态字段。
- 实现视频时钟域到 APB/CPU 时钟域的 snapshot/valid/ack CDC。
- 保证一帧一致快照，避免 CPU 读到跨帧撕裂数据。
- 加入 timeout、overrun、frame_id/config_seq 和 fail-closed 语义。
- 仿真、Map/PNR/STA/CDC 后上板，由 CPU 连续读取真实特征。

### Gate 3：板上 CPU 识别固件

当前仓库有 Host 侧 `single_camera_classifier` 和 feature adapter 基础，但还没有把它们接入当前 Hard SoC 的板上识别 `main`。需要：

- 将 APB snapshot 适配到分类器输入。
- 覆盖红、黄、蓝、黑、白五色；不能只做现有红/蓝/黄面积。
- 覆盖正方体、圆柱体、锥体三形状。
- 覆盖 2 cm、2.5 cm、3 cm 和任务三/四相对尺寸关系。
- 在真实背景、光照、摆放位置下完成阈值和尺寸标定。
- 保留可解释输出：识别结果、目标/非目标、执行/不执行及原因。
- 构建为当前 BSP 的片上 RAM 固件并做 UART 证据；后续是否需要更大内存必须单独评审，不能直接启用外部 DDR。

### Gate 4：逐轮任务、目标输入和 OSD

需要实现：

- 四任务目标配置和每项 5 轮的事务状态机。
- 同一物体只触发一次，非目标明确不动作并给出理由。
- CPU 生成语义结果，FPGA OSD 渲染；不能让 FPGA RTL承担分类或四任务判断。
- 评委随机顺序、识别/判断/执行记录、异常和超时处理。
- 一键启动、10 分钟内完成全部四任务的现场流程。

### Gate 5：UART2/J52 与 myCobot 正式闭环

UART0 Hello 通过不代表机械臂 UART 可用。仍需：

- 审核 UART2/J52 电平、管脚、接线和 1,000,000 baud。
- 先做板上 CPU 的只读协议/状态门，再进入动作门。
- 将现有 Host/PC 端 myCobot 协议和事务逻辑移植到板上 CPU；PC 不进入正式闭环。
- 实现点位表、夹爪、超时、有限重试、互锁、异常停机和防重复触发。
- 在明确安全姿态、急停/断电方式和用户逐步批准后，验证轻取轻放、旋转 180°（允许正负 10°）、最大臂展放置且不跌落。

### 最终比赛闭环标准

必须完成四项任务、每项 5 轮：

- 任务一：指定颜色正方体。
- 任务二：指定颜色 + 正方体形状。
- 任务三：与 2 cm/3 cm 参考正方体边长差 1 cm。
- 任务四：与 2 cm/3 cm 目标正方体边长差不超过 0.5 cm。

最终还要有 20 轮顺序测试、唯一响应、结果/理由可视化、机械臂安全动作、10 分钟总时限、一键启动和全程录像证据。

## 7. 当前完成度结论

已经完成的是“可复现并能联合运行的视频 + Hard SoC CPU 基础平台”，包括工程真源、完整 IP/BSP、Efinity 离线门、匹配 bitstream、USER2 启动方法、片上 RAM Hello、UART0 和最小 APB0 `REG_MAGIC` 通道。

尚未完成的是“真实视频特征进入 CPU -> CPU 完成五色/三形状/三尺寸与四任务判断 -> 结果回写 OSD -> UART2 驱动机械臂 -> 20 轮现场闭环”。因此距离完整任务闭环仍有 5 个明确 Gate；最大的技术断点是 feature snapshot APB/CDC，而不是 CPU 能否启动。

## 8. 当前安全边界

- 当前仅允许继续 `USER2 + 片上 RAM + UART0 + 单次 REG_MAGIC 只读`。
- 禁止写 Flash、Flash erase/program。
- 禁止使用 `USER1`。
- 禁止初始化或访问外部 DDR。
- 禁止当前阶段连接 UART2/J52 或机械臂。
- 禁止从其他工程副本拼接 wrapper、BSP、`.peri.xml`、RTL 或 OpenOCD 配置。
- 禁止把 Host 测试、离线构建或相邻 bitstream 的 PASS 冒充当前 bitstream 的板级 PASS。

## 9. 队友开始前应先读

1. 仓库根目录 `AGENTS.md`。
2. 仓库根目录 `CURRENT_STATE.md`。
3. `competition_project_single_camera/docs/debug_sessions/hard_soc_board_config.md`。
4. `competition_project_single_camera/docs/review_packets/m2_hard_soc_source_sync_review_20260716.md`。
5. `competition_project_single_camera/docs/review_packets/m2_hard_soc_apb_magic_offline_review_20260716.md`。
6. 本交接文件。

随后运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\agent_handoff_health_check.ps1
```

若当前 branch/HEAD、dirty 状态、IP 生成批次或板级现象与本文不一致，应停止扩大修改，先形成最小 Contradiction Report。
