# M2 Hard SoC 系统真源补交 Review Packet

> 日期：2026-07-16
> 分支：`dev/libaoxun688-hard-soc-source-sync-20260716`
> 源码提交：`2d4b3b7b3d0c59d88ece0669534ae38de02ed938`
> 分支头（含 Review Packet / Work Log）：`0604d33f3fe76851e1dfe738403875b4a7d0721c`
> 基线：`origin/main@4e35b05453c1cd30c943bb3d567fd0316ca6bdde`
> 当前裁定：系统真源补交完成；CPU USER2 JTAG取指和UART0回显仍为 `NOT VERIFIED`。

## 1. 唯一来源副本

本次只使用完整工程副本：

```text
TJ375N529_SC431HAI2LCD_Demo_V3_cpu_video_joint_20260715_161500
```

未按目录时间选取，未从其他副本拼接XML、SDC、RTL、IP或BSP。该副本同时匹配以下历史证据：

- bitstream SHA-256：`AA133887F3D5CE19768C35C3E1775019D370AAC66FE95ABEE46503A63BA96F31`
- 最差 Setup/Hold：`+1.742ns/+0.018ns`
- CDC原文：`No Synchronizer warnings to report.`
- J48/ch0板级结果：真实摄像头HDMI画面正常，五个不同颜色方块可见；未出现黑屏、花屏、整帧冻结或颜色通道完全错位
- 板级截图证据 SHA-256：`412CEE704039DF063CBCFBFBE56003A37BF1EFFD1C13C39CB8A81A7C946D9659`

为避免泄露本机信息，本文只记录来源副本名和证据指纹，不记录盘符、用户目录或IDE workspace路径。

## 2. 工具与工程状态

- Efinity：`2025.2.288.4.15`
- 器件：Titanium `TJ375N529`，timing model `I3`
- `mem_test.xml`：`last_run_state=pass`，`last_run_flow=bitstream`
- Interface Designer / Check Design：`0 error / 4 warning`
- 4项warning均为来源视频基线已有物理距离项：
  - `mipi_rx_dp01 / mipi_ln_rule_rx_distance`
  - `mipi_tx_ck0 / mipi_ln_rule_tx_distance`
  - `mipi_tx_dp12 / mipi_ln_rule_tx_distance`
  - `tmds_data2 / lvds_rule_tx_distance`
- Map：完成，报告记录 `EFX_MAP` flow完成
- Placement：完成，耗时23秒
- Routing：完成，耗时12秒；最新PnR总耗时52秒
- Setup/Hold：所有报告列出的最差值均非负，最差为 `+1.742ns/+0.018ns`
- CDC：`No Synchronizer warnings to report.`
- bitstream generation：完成；匹配SHA-256见上文，不提交bitstream

Map/PNR来自Efinity automated flow完成报告；历史操作未单独保存shell进程exit code，因此本文不虚构数值exit code，以完成报告、STA、CDC和匹配bitstream作为同批证据。

## 3. 系统契约复核

- `mem_test.xml` 同时登记 `src/feature_stats/feature_stats_tap.v` 和 `EfxSapphireHpSoc_slb`
- `debugger.auto_instantiation=off`
- DDR AXI0保持启用，DDR AXI1为 `is_axi_enable=false`
- 原视频JTAG使用 `JTAG_USER1`
- QCRV32使用 `JTAG_USER2`
- UART0 RX/TX为 `GPIOR_165/GPIOR_145`，3.3V LVCMOS
- 独立SoC reset为 `GPIOL_79`，3.3V LVCMOS
- `soc_system_clk=594MHz`
- `soc_memory_clk=237.6MHz`
- 视频状态机继续唯一驱动DDR `CFG_START/RST/SEL`
- Hard SoC wrapper只读取DDR `CFG_DONE`，未形成配置端口双驱动

## 4. 可复现Hard SoC与最小BSP

提交的Hard SoC IP目录只包含管理员要求的10个文件，不含 `ipm/*.pickle`。提交的最小BSP共7个文件：

```text
embedded_sw/efx_hard_soc/software/standalone/common/bsp.mk
embedded_sw/efx_hard_soc/software/standalone/common/riscv64-unknown-elf.mk
embedded_sw/efx_hard_soc/software/standalone/common/standalone.mk
embedded_sw/efx_hard_soc/software/standalone/common/start.S
embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/include/soc.mk
embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/include/soc.h
embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/linker/default_i.ld
```

`build.ps1`会逐项检查这7个文件；缺少任何文件都会明确失败，并提示从同一 `settings.json` 使用Efinity `2025.2.288.4.15` 重生BSP。工具链通过显式参数、`EFINITY_RISCV_IDE`环境变量或PATH发现，不提交安装路径。

本分支使用最小BSP重新构建Hello成功：

- 工具链：Efinity RISC-V GCC 8.3.0
- RAM占用：2608B / 16KiB，15.92%
- ELF入口：`0xF9000000`
- 唯一LOAD段：`0xF9000000..0xF9000A30`
- 未解析符号：无
- `ELF_LOAD_AUDIT=PASS`

该ELF是本地忽略产物，未提交。由于调试信息会包含构建环境差异，本次重建ELF哈希不作为来源bitstream身份依据。

## 5. 有意的可移植化差异

除以下3个文件外，本文清单中的其余文件与唯一来源副本逐文件SHA-256一致：

- `settings.json`：只把 `--base_path` 的来源副本绝对路径改为相对 `..`；所有Hard SoC配置参数保持不变
- `build.ps1`：移除本机工具安装硬编码，增加最小BSP缺件检查和可移植工具发现
- Hello `README.md`：补充最小BSP清单、重生步骤和无本机路径的构建命令

对应来源与分支哈希：

| 文件 | 来源SHA-256 | 分支SHA-256 |
|---|---|---|
| `ip/EfxSapphireHpSoc_slb/settings.json` | `80A4BF306641C5B26F8B27677E38AAD97E50992F426B8B89778033F73399478F` | `36BFB1FAC9B3B55A169EA409FE0716D9A845773734B00B5DA0ED4E9140D531E7` |
| `cpu_bringup/uart_hello_onchip/build.ps1` | `CD4F8937E62C593FD3DA72F25D9EF1282F37F75D509E4C71FCCE804649F02CBB` | `CC675C2223F65EAAAB041C760BBBA232E26CEE4CFECB2FC73E9441E901CA7DB3` |
| `cpu_bringup/uart_hello_onchip/README.md` | `E75E48B87321BE5497C66EB94C3BDB0DFEC197FB6DE04C66F40ABB7392FFB6D6` | `9B6F37B6376DC002BB4A8F2DBDF4DE3BB30C3F6FC705F0D2DACEF99345C27AED` |

## 6. 第一个提交文件SHA-256

下表按本次 Codex 审核时 Windows checkout 的实际文件字节重新计算。Git 的 EOL 转换可能使另一平台 checkout 的字节 SHA 不同；跨机器身份以固定 commit + path 为主，哈希比较前必须先统一 EOL 约定。

| 文件 | SHA-256 |
|---|---|
| `competition_project_single_camera/constrain.sdc` | `3C0A58F318A3981984D7C544F26F9844FE85E27E680BDFA4C17B0988D0360994` |
| `competition_project_single_camera/cpu_bringup/uart_hello_onchip/README.md` | `9B6F37B6376DC002BB4A8F2DBDF4DE3BB30C3F6FC705F0D2DACEF99345C27AED` |
| `competition_project_single_camera/cpu_bringup/uart_hello_onchip/build.ps1` | `CC675C2223F65EAAAB041C760BBBA232E26CEE4CFECB2FC73E9441E901CA7DB3` |
| `competition_project_single_camera/cpu_bringup/uart_hello_onchip/makefile` | `58BDE219C569D62EEBDB7FC57D77074EAF7D3AB9FC59BAB41E8D2E52E9E9D2CE` |
| `competition_project_single_camera/cpu_bringup/uart_hello_onchip/src/main.c` | `09A0C9451404AC5B85808F7E3124084BAC5EFB2F46B1FAA0C2B1905C8691F4E4` |
| `competition_project_single_camera/embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/include/soc.h` | `2CD1FEB33BD3E2C41D27E99DC3632C03F1C4CC91300C9BE6FF40D39FFE91615E` |
| `competition_project_single_camera/embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/include/soc.mk` | `D3AD1C88A982CA77ADDF514906A5014441F5D248825904AA491F3FF7190271F7` |
| `competition_project_single_camera/embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/linker/default_i.ld` | `21FF3E0B6B741BA12639045BF3CF4D85FB95C6C4FA869D5401BE6CBD3BB5ACC5` |
| `competition_project_single_camera/embedded_sw/efx_hard_soc/software/standalone/common/bsp.mk` | `B1CD52810F86A11E65E08FBACF1E8E1EA22B4C1E51812602D8F1FB5B449DE082` |
| `competition_project_single_camera/embedded_sw/efx_hard_soc/software/standalone/common/riscv64-unknown-elf.mk` | `776435752F4203DD575D5E09EAF8190CBBFDC9126F24B33E78543B09292D0521` |
| `competition_project_single_camera/embedded_sw/efx_hard_soc/software/standalone/common/standalone.mk` | `05600B2CBE0FB0B9F2877123EDEB237956536B144F9F49DF21E5139F74C9747B` |
| `competition_project_single_camera/embedded_sw/efx_hard_soc/software/standalone/common/start.S` | `4E3F410DF3F18CE659F770C2450134187F5FC7407171039947CE10B4A633FA4A` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb.v` | `90F65AAFF402137BBED4DD7C78496DA831E3DAD12CD06BEDA38A9F87BF8E0DC3` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb_define.vh` | `D9DAE6E1264492FA99BC0C44A717D9431D6C92B3A08019303765E548D8E96E95` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb_tmpl.v` | `714896DC45B939B12BDFC642B368F4CD15912364145918E1D3E1F4C50B760BE6` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb_tmpl.vhd` | `CCF6F09F8CAC3BBC3EAAB06C704BF9F4A9F424FF9304DDD30A467586A3B817BC` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_wrapper.v` | `39E32FA75DE315D7F2C9969799208F63E284CD0A31AAA54BD03A70C809617DE4` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/hard_ip_args.ini` | `4B43AFC3680AE96F62C438D23FA9C8657B5961FECB74D30EB867F9FEB76C68BC` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/ipm_pt_map.json` | `302D87F3CD3A8B58AD4868181AD986797CC4FAEEDCC61434FB6210DEE1CB0F66` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/settings.json` | `36BFB1FAC9B3B55A169EA409FE0716D9A845773734B00B5DA0ED4E9140D531E7` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/source/Axi4PeripheralTop.v` | `47E9266AB97DDB4C50D524E5B62FA8032D6CD959553B1432423C2386A6C43DFC` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/source/peri_config` | `236A8DDF3D20B5861F9F9812C0F0E5EC962E6DB76A0A4EF5F6F4864A6DB1A040` |
| `competition_project_single_camera/mem_test.peri.xml` | `5B530FD3F7FCDAEE1F6429482DE275C1014CB40988662A57926768E7A38B424D` |
| `competition_project_single_camera/mem_test.xml` | `F428549DF9F87DC9A6CF0464F8C5F5FD92DABD2102CA8850F7C9DE21A0BAA060` |
| `competition_project_single_camera/src/top.v` | `E79CB53A9960455B3B033AC2C329E90FCF81F3299BD36AA5C34DA221F6CB6834` |

## 7. 禁止项审计

第一个提交共25个文件。提交前检查结果：

- 无 `outflow*/`、`work_*/`、`.metadata/`
- 无 bit/hex/elf/bin/o/vdb/qdb/db/map/rpt/log
- 无 `ipm/*.pickle`
- 无 `.mcp.json`
- 无许可证文件
- 无用户目录、Efinity安装目录、来源副本绝对路径或串口枚举缓存

管理员要求提交的Efinity生成Verilog/模板保留其原始版权和免责声明头；这些是源文件组成部分，不是额外许可证文件。

## 8. 尚未验证与下一门

- USER2 JTAG实际连接：`NOT VERIFIED`
- CPU从片上RAM实际取指：`NOT VERIFIED`
- UART0完整Hello横幅：`NOT VERIFIED`
- UART0单字符回显：`NOT VERIFIED`
- CPU读取feature snapshot、OSD、按键、UART2、myCobot：均未进入本Gate

下一门仍是：使用匹配bitstream，选择FPGA `USER2`，只把Hello ELF下载到 `0xF9000000` 片上RAM；禁止USER1、Flash擦写和外部DDR初始化。CPU Hello通过前，不得宣称板上CPU闭环。

## 9. Codex 合并复核（2026-07-16）

Codex 在集成分支对同一仓库真源重新执行了离线验证；未复用来源副本 outflow，也未烧录板卡：

- UART0 Hello：Efinity RISC-V GCC 8.3.0 构建 PASS；2608 B / 16 KiB，入口 `0xF9000000`，唯一 LOAD 段 `0xF9000000..0xF9000A30`，`ELF_LOAD_AUDIT=PASS`。
- Efinity 2025.2 全流程：Map PASS、Interface PASS、PNR PASS、PGM/bitstream generation PASS。
- STA：最差 Setup/Hold `+1.742ns/+0.018ns`。
- CDC：`No Synchronizer warnings to report.`
- Interface Design Issues：4 个既有物理距离 warning（`mipi_rx_dp01`、`mipi_tx_ck0`、`mipi_tx_dp12`、`tmds_data2`）。
- Map/post-synthesis：另报告 118 个 warning；它们与上述 4 个 Interface warning 是不同集合，不能写作“全工程只有 4 个 warning”。本次未发现导致 Map/PNR/STA/CDC 失败的新增 fatal。
- 新生成 `mem_test.bit` SHA-256：`1D697F0DBA62CEDA3A8877729FF29A314F9BBA1A24CDCDFEDB751C7CF4B8AECC`。
- 新 bitstream 板级视频、USER2、CPU 取指和 UART0 回显：全部仍为 `NOT VERIFIED`。旧 `AA1338...` bitstream 的板级截图不能替代本次新 bitstream 的上板证据。

路径兼容说明：Efinity map 在中文绝对路径下会报 `filesystem error: Cannot convert character sequence: Illegal byte sequence`；本次通过仓库既有 ASCII junction 对同一工作树重跑后全流程 PASS。该问题属于工具路径兼容，不是 RTL 失败，也不允许将本机绝对路径写入工程配置。
