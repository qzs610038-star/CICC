# M2 Hard SoC 缺件补交审核清单

> 发布角色：项目审核管理员 / Codex
> 日期：2026-07-16
> 对应已审提交：`dev/libaoxun688@e129885`
> 当前裁定：feature tap、framebuffer 诊断和 CPU Host 内容可保留；完整 Hard SoC 系统真源同步为 `HOLD`。

## 1. 为什么需要补交

`e129885` 的标题和交接文档描述了 Hard SoC 联合集成，但固定提交中的实际仓库树没有满足交接文档自己的 Gate A：

- `mem_test.xml` 没有登记 `EfxSapphireHpSoc_slb`；
- `debugger.auto_instantiation` 仍为 `on`；
- 缺少 `ip/EfxSapphireHpSoc_slb/settings.json` 和可复现生成源码；
- 缺少 `cpu_bringup/uart_hello_onchip/build.ps1`；
- `mem_test.peri.xml` 未找到交接声明的 UART0/SoC 资源标记；
- `constrain.sdc` 仍有有效 `axi1_*`、`CLK_5M`、`pll_inst1_CLKOUT0` 约束。

本次补交的目标不是再写一份说明，而是把与已通过联合构建证据匹配的、可复现的系统真源交到仓库中。

## 2. 先锁定正确副本，禁止多副本拼接

请不要按目录名或“修改时间最新”猜测来源，也不要从多个副本分别拿 XML、SDC、IP 和 BSP。正确来源必须是同一个联合构建批次，并同时满足：

1. 该批次生成的 `mem_test.bit` SHA-256 为
   `AA133887F3D5CE19768C35C3E1775019D370AAC66FE95ABEE46503A63BA96F31`；
2. 同批报告最差 Setup/Hold 为 `+1.742ns/+0.018ns`；
3. 同批 CDC 结论为 `No Synchronizer warnings to report.`；
4. 同批板级现象是 J48/ch0 HDMI 真实摄像头画面，无黑屏、花屏、冻结或颜色通道完全错位；
5. 若包含 UART0 Hello，ELF SHA-256 应为
   `30A499F5EA2E91AF531FA3991EEB8F2CC9757847E27977EE8238060ADF7A2757`，入口和唯一 LOAD 段位于 `0xF9000000` 的 16KB 片上 RAM。

如果没有任何单一副本能同时对应上述证据，请停止复制，回到 Interface Designer/IP 生成输入重新生成并完整构建；不要人工拼接 `.peri.xml`。

## 3. 必须提交的工程真源

以下路径必须来自同一匹配批次，并落到仓库中的对应位置：

```text
competition_project_single_camera/mem_test.xml
competition_project_single_camera/mem_test.peri.xml
competition_project_single_camera/constrain.sdc
competition_project_single_camera/src/top.v

competition_project_single_camera/ip/EfxSapphireHpSoc_slb/settings.json
competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb.v
competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb_define.vh
competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb_tmpl.v
competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_slb_tmpl.vhd
competition_project_single_camera/ip/EfxSapphireHpSoc_slb/EfxSapphireHpSoc_wrapper.v
competition_project_single_camera/ip/EfxSapphireHpSoc_slb/hard_ip_args.ini
competition_project_single_camera/ip/EfxSapphireHpSoc_slb/ipm_pt_map.json
competition_project_single_camera/ip/EfxSapphireHpSoc_slb/source/Axi4PeripheralTop.v
competition_project_single_camera/ip/EfxSapphireHpSoc_slb/source/peri_config
```

`mem_test.xml` 合入后必须同时满足：

- 登记 `feature_stats_tap.v`；
- 登记 `EfxSapphireHpSoc_slb`；
- 保留 Hard SoC include path；
- `debugger.auto_instantiation=off`；
- 不创建第二个 USER2 Debug Wizard 实例。

`.peri.xml`、`top.v` 和 SDC 必须作为同一组审查，至少保持：DDR AXI0 启用、AXI1 关闭、视频占 USER1、QCRV32 占 USER2、UART0 RX/TX=`GPIOR_165/GPIOR_145`、SoC reset=`GPIOL_79`；不得恢复旧 `axi1_*`、`CLK_5M`、`pll_inst1_CLKOUT0` 或 `jtag_inst2_*` 有效路径。

## 4. 必须提交的 UART0 Hello 可复现入口

```text
competition_project_single_camera/cpu_bringup/uart_hello_onchip/src/main.c
competition_project_single_camera/cpu_bringup/uart_hello_onchip/makefile
competition_project_single_camera/cpu_bringup/uart_hello_onchip/build.ps1
competition_project_single_camera/cpu_bringup/uart_hello_onchip/README.md
```

还必须二选一：

1. 提交最小 BSP 依赖集，至少覆盖匹配批次的 `soc.h`、`default_i.ld`、`start.S` 和官方 standalone make 片段；或
2. 在 README 中给出从 `settings.json` 重新生成 BSP 的明确步骤，并让 `build.ps1` 在 BSP 缺失或批次不匹配时 fail closed。

只提交 `main.c` 或已构建 ELF 不合格。

## 5. 必须提交的来源与验证文档

请新增：

```text
competition_project_single_camera/docs/review_packets/m2_hard_soc_source_sync_review_20260716.md
```

并在 `competition_project_single_camera/WORK_LOG.md` 追加条目。Review Packet 至少记录：

- 实际来源副本的绝对路径仅作 provenance，不写进工程配置；
- Efinity 精确版本；
- 本次提交 SHA；
- 上述工程真源文件的 SHA-256 清单；
- Interface Designer Check Design 结果及全部 warning；
- Map/PNR/bitstream 的退出码；
- 最差 Setup/Hold；
- CDC 原文结论；
- bitstream SHA-256，但不提交 bitstream；
- J48/ch0 板级视频回归结果；
- CPU Hello 仍为 `NOT VERIFIED` 或真实 USER2/UART0 验收结果，二者必须如实二选一。

## 6. 禁止提交

```text
.mcp.json
outflow*/
work_*/
.metadata/
*.bit
*.hex
*.elf
*.bin
*.o
*.vdb
*.qdb
*.db
*.map
*.rpt
*.log
ip/EfxSapphireHpSoc_slb/ipm/*.pickle
许可证、本机 IDE workspace、本机 Efinity 安装路径、串口枚举缓存
```

报告中的结论应转写进 Review Packet，不直接提交生成报告或构建产物。

## 7. 分支和提交方式

待本轮主线发布完成后，从最新云端 `main` 建新分支，不要继续在多个旧副本分支上叠加：

```powershell
git fetch --prune origin
git switch main
git pull --ff-only origin main
git switch -c dev/libaoxun688-hard-soc-source-sync-20260716
```

建议拆成两个提交：

```text
feat(single_camera): sync reproducible hard soc project sources
docs(single_camera): record hard soc source provenance and gates
```

提交前必须运行 `git diff --cached --check`，并确认禁止列表无任何命中。推送个人分支后，请只回传：分支名、两个 commit SHA、Review Packet 路径和构建证据摘要；不要发本机整个工程压缩包。

## 8. 可直接复制到微信的文字

```text
请你的本地 Agent 按项目审核管理员要求补交 Hard SoC 真源，不要再按“哪个副本最新”猜，也不要从多个副本拼 XML/SDC/IP/BSP。先锁定同一个联合构建批次：bitstream SHA-256 必须是 AA133887F3D5CE19768C35C3E1775019D370AAC66FE95ABEE46503A63BA96F31，Setup/Hold +1.742ns/+0.018ns，CDC 为 No Synchronizer warnings to report.，且该 bitstream 已验证 J48/ch0 HDMI 真实摄像头画面正常。若没有单一副本同时匹配这些证据，就停止复制，回 Interface Designer 重新生成和构建。

请从最新云端 main 新建 dev/libaoxun688-hard-soc-source-sync-20260716，提交以下内容：1）mem_test.xml、mem_test.peri.xml、constrain.sdc、src/top.v；2）ip/EfxSapphireHpSoc_slb 下 settings.json、顶层/包装 Verilog、define/tmpl、hard_ip_args.ini、ipm_pt_map.json、source/Axi4PeripheralTop.v 和 source/peri_config；3）cpu_bringup/uart_hello_onchip 下 main.c、makefile、build.ps1、README，以及匹配的最小 BSP，或可从 settings.json 重生 BSP 且缺件时 fail closed 的步骤；4）新增 m2_hard_soc_source_sync_review_20260716.md 并追加 WORK_LOG，记录来源、各真源 SHA-256、Efinity 版本、Check Design、Map/PNR、Setup/Hold、CDC、bitstream 哈希和板级视频结果。

mem_test.xml 必须同时登记 feature_stats_tap 和 EfxSapphireHpSoc_slb，auto_instantiation=off；AXI1 关闭，视频 USER1、QCRV32 USER2，UART0=GPIOR_165/GPIOR_145，SoC reset=GPIOL_79。禁止提交 outflow/work/.metadata、bit/elf/map/rpt/log、pickle、许可证和本机路径配置。建议分两次 commit：feat(single_camera): sync reproducible hard soc project sources；docs(single_camera): record hard soc source provenance and gates。推送后只把分支名、两个 SHA 和 Review Packet 路径发回来。
```
