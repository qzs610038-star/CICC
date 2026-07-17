# G1 Review Packet: 当前 SHA 单摄 Hard SoC Efinity 冷构建与完整证据冻结

创建时间：2026-07-17 03:07（Asia/Shanghai）

## 1. 固定对象与工作区

- 源仓库：D:\第十届集创赛-雄芯院材料
- 源仓库分支：main
- 源仓库 HEAD：489ab5b0c1773bfcb776cedd9f39b3f088fb4a0f
- 冷构建工作树：D:\CICC-g1-efinity
- 工作树分支：codex/g1-efinity-evidence
- 工作树 HEAD：489ab5b0c1773bfcb776cedd9f39b3f088fb4a0f
- 外部证据根目录：D:\CICC-runs\g1-efinity-evidence
- 有效证据批次：D:\CICC-runs\g1-efinity-evidence\20260717_025725
- 构建输入目录：D:\CICC-g1-efinity\competition_project_single_camera
- 构建前工作树状态：clean；构建后、写入本 Packet 前输入文件仍未修改。

本 Packet 是本次工作树唯一预期的 Git 工作区新增物。没有修改 CURRENT_STATE.md、工程 XML、SDC、RTL、Hard SoC IP 或 CPU 输入文件。

## 2. 冷构建前置证据

### Efinity

- 工具：D:\Efinity\2025.2\bin\efx_run.bat
- 版本：2025.2.288.4.15
- 工具 SHA256：46CCF2D36DB0FB6CAA50778E69E11391687DEB5B5BDA079B6541482853A1BE1A
- 有效构建日志：D:\CICC-runs\g1-efinity-evidence\20260717_025725\logs\efinity_compile_forward_wrapper.log
- 有效原始日志：D:\CICC-runs\g1-efinity-evidence\20260717_025725\logs\efinity_compile_forward_raw.log
- 有效批次开始：2026-07-17 02:57:52.5451901 +08:00
- 有效批次结束：2026-07-17 03:00:32.0366344 +08:00
- 构建耗时：约 160.1 s
- 顶层退出码：0

构建通过一个进程内 Efinity runner 调用 efx_run.py 的 run_flow，参数为当前工作树中的 mem_test.xml、compile flow、外部 output/work 目录和 3600 s timeout。runner 有意不调用 update_project_file(args)，因此没有让工具回写工程 XML。有效 raw 日志中的 IP、RTL、工程和输出路径均指向本工作树或外部证据目录。

### 固定输入 SHA256

以下哈希是在有效批次前的 validated baseline 中记录，构建后复核一致：

| 输入 | SHA256 |
|---|---|
| competition_project_single_camera\mem_test.xml | 98D295223A98A7D21CC2EE27F801F8AF921CB5096B78A82C59930B43CDAB776C |
| competition_project_single_camera\mem_test.peri.xml | 61D1E69DBB4355FC4F1B649420CF3C1E4EA651D8B94F1445426701769E5C6898 |
| competition_project_single_camera\constrain.sdc | 3C0A58F318A3981984D7C544F26F9844FE85E27E680BDFA4C17B0988D0360994 |
| competition_project_single_camera\src\top.v | B985B85C149AA8B335958885E2E340332D6B25F68607DB2199E439EDCA3A6052 |
| competition_project_single_camera\src\apb_reg_magic.v | 3CE1F02465B2288604B9A6AF322F9146B3FE6E412F3F0548B3EF480036D249E1 |
| ip\EfxSapphireHpSoc_slb\EfxSapphireHpSoc_slb_define.vh | D9DAE6E1264492FA99BC0C44A717D9431D6C92B3A08019303765E548D8E96E95 |
| ip\EfxSapphireHpSoc_slb\EfxSapphireHpSoc_slb_tmpl.v | B118B2920F13DA69659CAB47EE3F673E5972854D05C785B4BA6306B79E215F68 |
| ip\EfxSapphireHpSoc_slb\EfxSapphireHpSoc_slb_tmpl.vhd | 0E3C0EF75A91A59B4274A0362A643914B11ABBF2781404F006E6D7715D5A9D6C |
| ip\EfxSapphireHpSoc_slb\EfxSapphireHpSoc_slb.v | E7CCBDCE5EA5E365CE46773FB23D7A307756D8321D5123CE7B2378C42C169BAD |
| ip\EfxSapphireHpSoc_slb\EfxSapphireHpSoc_wrapper.v | C6C64F332FC476D258BCA9A9794FC7980FCB2372A5A0DBB3756609195ED2AD92 |
| ip\EfxSapphireHpSoc_slb\hard_ip_args.ini | 8D7E2C1A25455B582515D53583CD24E31DA7D7BC113F276571CDA93ECDCB2285 |
| ip\EfxSapphireHpSoc_slb\ipm_pt_map.json | 302D87F3CD3A8B58AD4868181AD986797CC4FAEEDCC61434FB6210DEE1CB0F66 |
| ip\EfxSapphireHpSoc_slb\settings.json | C494B71EBBBBFCC4A50EE42741E57A0D14BF3FC99D103DA8B536B5AB499AC5CC |
| ip\EfxSapphireHpSoc_slb\source\Axi4PeripheralTop.v | 2BD429D43185F34E6ACC19CFC66A7ED955A3CF96D8686DBE677743DE47544BD3 |
| ip\EfxSapphireHpSoc_slb\source\peri_config | 21F1A9E8352125B7BDE63912CB6BE701CC76C57383C36245BE5B1E3E5108F47F |
| embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\include\soc.h | D67D3FBB70595CDCEB03E1AFAC4B1B80BACA92B1BFDE8CC8AB1920F05C8CB664 |
| embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\include\soc.mk | D3AD1C88A982CA77ADDF514906A5014441F5D248825904AA491F3FF7190271F7 |
| embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\linker\default_i.ld | 21FF3E0B6B741BA12639045BF3CF4D85FB95C6C4FA869D5401BE6CBD3BB5ACC5 |
| embedded_sw\efx_hard_soc\software\standalone\common\bsp.mk | B1CD52810F86A11E65E08FBACF1E8E1EA22B4C1E51812602D8F1FB5B449DE082 |
| embedded_sw\efx_hard_soc\software\standalone\common\riscv64-unknown-elf.mk | 776435752F4203DD575D5E09EAF8190CBBFDC9126F24B33E78543B09292D0521 |
| embedded_sw\efx_hard_soc\software\standalone\common\standalone.mk | 05600B2CBE0FB0B9F2877123EDEB237956536B144F9F49DF21E5139F74C9747B |
| embedded_sw\efx_hard_soc\software\standalone\common\start.S | 4E3F410DF3F18CE659F770C2450134187F5FC7407171039947CE10B4A633FA4A |
| cpu_bringup\uart_hello_onchip\build.ps1 | CC675C2223F65EAAAB041C760BBBA232E26CEE4CFECB2FC73E9441E901CA7DB3 |
| cpu_bringup\uart_hello_onchip\makefile | 58BDE219C569D62EEBDB7FC57D77074EAF7D3AB9FC59BAB41E8D2E52E9E9D2CE |
| cpu_bringup\uart_hello_onchip\README.md | 9B6F37B6376DC002BB4A8F2DBDF4DE3BB30C3F6FC705F0D2DACEF99345C27AED |
| cpu_bringup\uart_hello_onchip\src\main.c | 09A0C9451404AC5B85808F7E3124084BAC5EFB2F46B1FAA0C2B1905C8691F4E4 |

有效批次的 work/output 目录均为新建空目录；仓库中既有 competition_project_single_camera\outflow 未被使用。

## 3. FPGA 冷构建结果

| 阶段 | 结果 | 原始证据 |
|---|---|---|
| Map | PASS | D:\CICC-runs\g1-efinity-evidence\20260717_025725\output\mem_test.log |
| Interface / Pt | PASS，含 4 条接口设计 warning | D:\CICC-runs\g1-efinity-evidence\20260717_025725\output\mem_test.pt.rpt |
| PNR | PASS | D:\CICC-runs\g1-efinity-evidence\20260717_025725\output\mem_test.route.rpt |
| STA | PASS，最差 setup/hold 均为正 | D:\CICC-runs\g1-efinity-evidence\20260717_025725\output\mem_test.timing.rpt |
| CDC | PASS，无 synchronizer warning | D:\CICC-runs\g1-efinity-evidence\20260717_025725\output\mem_test.cdc.rpt |
| PGM bitstream generation | PASS | D:\CICC-runs\g1-efinity-evidence\20260717_025725\output\mem_test.pgm.out |

有效输出目录：D:\CICC-runs\g1-efinity-evidence\20260717_025725\output

关键结果：

- bitstream：D:\CICC-runs\g1-efinity-evidence\20260717_025725\output\mem_test.bit
- bitstream 大小：11,847,132 bytes
- bitstream SHA256：A897E33514A1079BB1B46C02C464B0BD679AF551CEFCB67C4A0EBD5B8FCD1ACD
- route XML 资源：Inputs 732/4370，Outputs 1051/5891，XLRs 17796/362880，Clocks 12/96，Memory Blocks 169/2688，DSP Blocks 0/1344
- Map 资源摘要：EFX_ADD 2065，EFX_LUT4 11385，EFX_FF 10104，EFX_RAM10 165，EFX_DPRAM10 4；DSP 0
- STA setup WNS：1.321 ns，最差路径 axi0_ACLK -> axi0_ACLK
- STA hold WHS：0.026 ns，最差路径 i_sysclk_div2 -> i_sysclk_div2
- CDC：min_clock_period 5000 ps；No Synchronizer warnings to report
- Interface 4 条设计 warning：mipi_rx_dp01 的 mipi_ln_rule_rx_distance、mipi_tx_ck0 的 mipi_ln_rule_tx_distance、mipi_tx_dp12 的 mipi_ln_rule_tx_distance、tmds_data2 的 lvds_rule_tx_distance；详见 mem_test.pt.rpt

warning 只按阶段分开记录，不将 Interface、综合/布局后 warning、SDC warning 合并为一个数字：

- mem_test.warn.log：242 条 warning record，分类为 EFX-0011 198、EFX-0200 20、EFX-0256 14、EFX-0201 10。
- mem_test.place.out：报告 Found 118 warnings in the post-synthesis netlist。
- mem_test.route.out：1912 条以 WARNING 开头的原始报告行，其中 696 条包含 SDC 约束 warning、416 条为 No ports matched、800 条为 Timer cuts combinational loop；报告中未见 ERROR/FATAL 前缀。原始文件是最终核查依据，部分条目由工具报告展开产生重复。
- 构建 raw 输出另有 WARNING: cannot find correct IV value；未将其伪装为零 warning。

FPGA 判断：当前固定 SHA 的 Efinity Map、Interface、PNR、STA、CDC 和 bitstream 生成已完成并有外部 raw/report 证据；接口/SDC/综合后的 warning 仍是 WARN，不能据此宣称板级闭环。

## 4. UART0 Hello / ELF 冷构建结果

执行目录：D:\CICC-g1-efinity\competition_project_single_camera\cpu_bringup\uart_hello_onchip

实际命令：build.ps1 -Clean -ToolchainRoot D:\Efinity\efinity-riscv-ide-2025.2

工具链探测到：

- D:\Efinity\efinity-riscv-ide-2025.2\build_tools\bin\make.exe
- D:\Efinity\efinity-riscv-ide-2025.2\toolchain\bin\riscv-none-embed-readelf.exe
- D:\Efinity\efinity-riscv-ide-2025.2\toolchain\bin\riscv-none-embed-size.exe
- D:\Efinity\efinity-riscv-ide-2025.2\toolchain\bin\riscv-none-embed-nm.exe

原始日志：D:\CICC-runs\g1-efinity-evidence\20260717_025725\logs\riscv_hello_build.log

- 构建退出码：0
- RAM 使用：2608 bytes / 16 KiB，15.92%
- 唯一 LOAD 段：start 0xF9000000，end 0xF9000A30；脚本 ELF_LOAD_AUDIT=PASS
- ELF 入口：0xF9000000
- ELF：D:\CICC-g1-efinity\competition_project_single_camera\cpu_bringup\uart_hello_onchip\build\uart_hello_onchip.elf
- ELF SHA256：E5BC80A2F18A7E2951D53DA539BE2FC61AAECFA90C5CDADB29E65FFC6141928A
- HEX SHA256：8F4AF6178128CB72CD84DB5B0F048B1A35B7939DC045A08EB461F51E052616F4
- BIN SHA256：C4C499DECD9349770C1703C2056825C25D02147FA95656CBA4A7EE80D6A76647

ELF 判断：当前固定 SHA 的 UART0 Hello ELF 已从当前源码和当前工具链冷构建通过；没有使用旧 ELF。该结论只覆盖生成与静态地址审计，不覆盖 CPU 实际取指、UART 输出或 APB 实读。

## 5. 已丢弃的构建尝试

- 20260717_024403：第一次尝试的进程 cwd 错在工作树根目录，导致 IP 路径错误；其 PASS 不能作为 G1 证据。该尝试生成的工作树根临时 mem_test.peri.xml 已确认是本次工具产物并删除，输入工作树恢复 clean。
- 20260717_025249：cwd 已修正，但使用反斜杠 EFINITY_HOME 时 PNR 加载 Tcl 路径变成 D:Efinity...，PNR 失败；该批次只保留为环境/调用失败证据，不能继承其输出。
- 仅 20260717_025725 作为有效固定 SHA 冷构建证据。

## 6. 板级和硬件边界

以下项目均为 NOT VERIFIED：

- bitstream 下载、Efinity Programmer、JTAG USER2 访问
- CPU 实际取指和 UART0 115200 bps Hello/回显
- APB0 0xE8100000 实读、CPU 到寄存器写回
- 视频输入、DDR、DSI/显示、实际时钟/复位板级现象
- UART2/J52、FPGA 到机械臂链路、myCobot 协议、点位、动作和夹爪

本次只进行了文件、工具、构建和报告核查；没有调用 Programmer/OpenOCD/GDB，没有 USER1、Flash、DDR 初始化、UART2/J52、串口打开或 myCobot 动作。构建中的 efx_pgm 仅是 Efinity flow 的 bitstream 生成阶段，不是硬件 Programmer 下载。构建前后进程审计均未匹配 efinity_programmer、efx_program、openocd、gdb、pyocd 或 mycobot。

下一步若获准进入板级 Gate，只能使用与当前 bitstream/ELF 匹配的产物，通过 FPGA USER2 将 ELF 放入 0xF9000000 片上 RAM，先验证 UART0 115200；在 Hello 通过前禁止 USER1、Flash、外部 DDR、UART2/J52、机械臂接线或动作。

## 7. 复核入口

- 有效 FPGA wrapper/raw：D:\CICC-runs\g1-efinity-evidence\20260717_025725\logs\efinity_compile_forward_wrapper.log、efinity_compile_forward_raw.log
- 有效输入基线：D:\CICC-runs\g1-efinity-evidence\20260717_025725\logs\phase0_validated_baseline.txt
- FPGA output：D:\CICC-runs\g1-efinity-evidence\20260717_025725\output
- ELF raw：D:\CICC-runs\g1-efinity-evidence\20260717_025725\logs\riscv_hello_build.log
- 当前工作树：D:\CICC-g1-efinity
- 当前源仓库：D:\第十届集创赛-雄芯院材料

结论：G1 的固定 SHA 冷构建和证据冻结完成。FPGA/ELF 的生成与静态验证通过；所有板级动作和实机闭环仍保持 NOT VERIFIED，不能从本 Packet 推断已上板。
