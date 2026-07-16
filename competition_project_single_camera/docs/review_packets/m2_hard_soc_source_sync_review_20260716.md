# M2 Hard SoC 系统真源补交 Review Packet

> 日期：2026-07-16
> 分支：`dev/libaoxun688-hard-soc-source-sync-20260716`
> 源码提交：`2d4b3b7b3d0c59d88ece0669534ae38de02ed938`
> 基线：`origin/main@4e35b05453c1cd30c943bb3d567fd0316ca6bdde`
> 当前裁定：系统真源补交、离线构建和最小板级 Gate 均已关闭；`USER2 JTAG / CPU EXECUTION / UART0 HELLO / UART0 ECHO / HDMI = PASS`。feature snapshot、CPU分类主循环、OSD、按键、UART2/J52和myCobot仍未进入本Gate。

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
| `ip/EfxSapphireHpSoc_slb/settings.json` | `80A4BF306641C5B26F8B27677E38AAD97E50992F426B8B89778033F73399478F` | `A22F4A2254D78B879733512BC40A0D68D02598D43B89615D4F947CC44411785C` |
| `cpu_bringup/uart_hello_onchip/build.ps1` | `CD4F8937E62C593FD3DA72F25D9EF1282F37F75D509E4C71FCCE804649F02CBB` | `0F89C19786DDCAAC9C97094B53880D545603F1E295C3B47353355E98C9695FF5` |
| `cpu_bringup/uart_hello_onchip/README.md` | `E75E48B87321BE5497C66EB94C3BDB0DFEC197FB6DE04C66F40ABB7392FFB6D6` | `9B6F37B6376DC002BB4A8F2DBDF4DE3BB30C3F6FC705F0D2DACEF99345C27AED` |

## 6. 第一个提交文件SHA-256

| 文件 | SHA-256 |
|---|---|
| `competition_project_single_camera/constrain.sdc` | `3C0A58F318A3981984D7C544F26F9844FE85E27E680BDFA4C17B0988D0360994` |
| `competition_project_single_camera/cpu_bringup/uart_hello_onchip/README.md` | `9B6F37B6376DC002BB4A8F2DBDF4DE3BB30C3F6FC705F0D2DACEF99345C27AED` |
| `competition_project_single_camera/cpu_bringup/uart_hello_onchip/build.ps1` | `0F89C19786DDCAAC9C97094B53880D545603F1E295C3B47353355E98C9695FF5` |
| `competition_project_single_camera/cpu_bringup/uart_hello_onchip/makefile` | `58BDE219C569D62EEBDB7FC57D77074EAF7D3AB9FC59BAB41E8D2E52E9E9D2CE` |
| `competition_project_single_camera/cpu_bringup/uart_hello_onchip/src/main.c` | `09A0C9451404AC5B85808F7E3124084BAC5EFB2F46B1FAA0C2B1905C8691F4E4` |
| `competition_project_single_camera/embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/include/soc.h` | `87A09A739226C2A46DF33F6F995758D1BF81DD33AE8DE3E41EE0B18143833629` |
| `competition_project_single_camera/embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/include/soc.mk` | `D3AD1C88A982CA77ADDF514906A5014441F5D248825904AA491F3FF7190271F7` |
| `competition_project_single_camera/embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/linker/default_i.ld` | `618FC75BB2D6D932E5FE27E3A7F8E263222095AC25A415F6814C59264D34D5D3` |
| `competition_project_single_camera/embedded_sw/efx_hard_soc/software/standalone/common/bsp.mk` | `B1CD52810F86A11E65E08FBACF1E8E1EA22B4C1E51812602D8F1FB5B449DE082` |
| `competition_project_single_camera/embedded_sw/efx_hard_soc/software/standalone/common/riscv64-unknown-elf.mk` | `776435752F4203DD575D5E09EAF8190CBBFDC9126F24B33E78543B09292D0521` |
| `competition_project_single_camera/embedded_sw/efx_hard_soc/software/standalone/common/standalone.mk` | `05600B2CBE0FB0B9F2877123EDEB237956536B144F9F49DF21E5139F74C9747B` |
| `competition_project_single_camera/embedded_sw/efx_hard_soc/software/standalone/common/start.S` | `1F766009DFADD1B2B7808433054E211CD78FF14EC1054AF85A8A61B6B413B462` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb.v` | `0A67C38D7E352CD2E4833E43D2815B81BECDF426FA7610A316C1A3CC6CEE2CC5` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb_define.vh` | `3A7E9026429C84B339B8A5379D7D137EA347DE76AD0FE90D5FFD8C03273214B0` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb_tmpl.v` | `20EC8803E290BD639C66EF0E8F623287943723F12FD2C242ADF7E99BEE107CE9` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb_tmpl.vhd` | `CCF6F09F8CAC3BBC3EAAB06C704BF9F4A9F424FF9304DDD30A467586A3B817BC` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_wrapper.v` | `C63628B93453A054B74F53EAE268441F400B7D749E10239D537053021A0AC428` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/hard_ip_args.ini` | `4B43AFC3680AE96F62C438D23FA9C8657B5961FECB74D30EB867F9FEB76C68BC` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/ipm_pt_map.json` | `02D931CC2B56BEE555AF5E2CF50EAEC89E7E364645322815943FC95276F81B41` |
| `competition_project_single_camera/ip/EfxSapphireHpSoc_slb/settings.json` | `A22F4A2254D78B879733512BC40A0D68D02598D43B89615D4F947CC44411785C` |
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

## 8. 板级 USER2 / 片上 RAM / UART0 Gate

### 8.1 身份与安全边界

- 板级日期：2026-07-16
- bitstream SHA-256：`AA133887F3D5CE19768C35C3E1775019D370AAC66FE95ABEE46503A63BA96F31`
- FPGA IDCODE：`0x006A0EF3`
- Hello ELF SHA-256：`4AD5CA14147D45B4594C498A3976BDD36EC3570E982B09DC21744669D91CC78A`
- ELF入口：`0xF9000000`
- JTAG：FTDI channel 1、FPGA BSCAN tunnel、`USER2` IR=9
- UART0：COM10，`115200 8N1`

bitstream仅通过JTAG临时加载到FPGA SRAM，Hello仅下载到片上RAM；未写Flash。测试未使用`USER1`，未初始化或访问外部DDR，未连接UART2/J52或机械臂，未发送myCobot帧。断电会丢失bitstream和Hello，必须重新加载，不能描述为持久烧录。

### 8.2 NDMRESET 根因与固定启动顺序

仅halt后写`pc=0xF9000000`会在第一条取指产生instruction access fault：

```text
pc=0x00000000
mcause=1
mepc=0xF9000000
mtval=0xF9000000
```

RAM下载/校验、program-buffer数据端读取、Machine mode、`satp=0`和PMP检查正常，`fence.i`不能解决。按标准RISC-V Debug Module执行以下NDMRESET序列后，首条取指和UART输出恢复：

```tcl
riscv dmi_write 0x10 0x00000003
sleep 100
riscv dmi_write 0x10 0x00000001
sleep 200
```

固定启动顺序为：开发板完全断电 -> 冷启动 -> JTAG SRAM加载匹配bitstream -> USER2连接 -> NDMRESET -> halt四核 -> 下载并校验Hello ELF -> CPU0设置`pc=0xF9000000` -> resume -> 捕获UART0横幅和回显。带电直接重配曾出现DDR横带花屏；完全断电冷启动后加载同一bitstream，用户确认真实摄像头HDMI画面恢复正常。

### 8.3 实测结果

- OpenOCD识别4个RV32 hart，XLEN=32，`misa=0x4004112d`
- 向`0xF9000000`下载并校验538 bytes成功
- 运行约8秒后：`pc=0xF900019A`、`mcause=0`、`mepc=0`、`mtval=0`
- COM10收到完整横幅：

```text
TJ375 CPU+VIDEO UART0 HELLO
ONCHIP_RAM=0xF9000000 UART0=115200 8N1
Type characters to verify echo.
```

- 发送ASCII `K`（hex `4B`）后收到`K`回显
- OpenOCD正常shutdown，无残留进程
- 用户确认测试后HDMI真实摄像头画面正常

裁定：

```text
USER2 JTAG=PASS
CPU EXECUTION=PASS
UART0 HELLO=PASS
UART0 ECHO=PASS
HDMI=PASS
```

### 8.4 仍未验证与下一门

- CPU读取feature snapshot：`NOT VERIFIED`
- CPU分类主循环与逐轮事务：`NOT VERIFIED`
- CPU到OSD回写、按键：`NOT VERIFIED`
- UART2/J52：`NOT VERIFIED`
- myCobot只读、协议或动作：`NOT VERIFIED`

下一门必须另行审核。本次Hello PASS只关闭最小CPU启动和UART0基础链路，不证明UART2/myCobot可用，也不证明正式比赛识别、OSD或机械臂闭环。管理员批准扩大范围前，不连接J52或机械臂，不执行任何myCobot命令。
