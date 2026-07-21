# R0 Review Packet：当前 HEAD 单摄 Hard SoC 冷构建与 USER2 前置制品冻结

> 日期：2026-07-17（Asia/Shanghai）
> Gate：R0 `USER2 + UART0 + APB MAGIC` 基础验证的构建前置
> 裁定：`OFFLINE BUILD PASS / BOARD ACTION NOT STARTED`

## 1. 目标、范围与固定对象

本 Packet 仅为 R0 重新生成与当前 `main` 匹配的 FPGA bitstream、UART0 Hello ELF 及离线证据。它不证明 FPGA 已配置、`JTAG_USER2` 已连接、CPU 已取指、UART0 已输出或 APB 已实读。

- 源仓库 HEAD：`main@9acf4d8b2ec788ccd5777f3833a7bfb756c51cad`
- 隔离构建分支：`codex/r0-current-batch-build-20260717`
- 隔离 worktree：`<external-r0-worktree>`
- Efinity 可见 ASCII junction：`<external-r0-source-junction>`
- 外部证据目录：`<external-r0-evidence>`
- FPGA 工具：Efinity `2025.2.288.4.15`
- `efx_run.bat` SHA-256：`46CCF2D36DB0FB6CAA50778E69E11391687DEB5B5BDA079B6541482853A1BE1A`

构建输出和 work 目录均在外部证据目录；没有使用仓库中的旧 `outflow/` 或 `outflow_*` 制品。

## 2. 输入与 XML 回写审计

下列硬件/固件输入在构建后与 `HEAD` 的 Git blob 一致：

| 输入 | 结论 |
|---|---|
| `mem_test.peri.xml` | HEAD 一致 |
| `constrain.sdc` | HEAD 一致 |
| `src/top.v` | HEAD 一致 |
| `src/apb_reg_magic.v` | HEAD 一致 |
| `ip/EfxSapphireHpSoc_slb/settings.json` | HEAD 一致 |
| `cpu_bringup/uart_hello_onchip/build.ps1` | HEAD 一致 |
| `cpu_bringup/uart_hello_onchip/src/main.c` | HEAD 一致 |

`mem_test.xml` 被 Efinity CLI 在成功后改写了唯一属性 `last_change`，从 `1784209310` 改为 `1784278783`。没有设计文件、IP、约束、顶层或生成参数语义变化。该工具元数据差异保留在隔离 worktree 供审计，不得提交，也不得作为新的工程输入混入后续批次。

## 3. FPGA 冷构建结果

实际调用的 Efinity flow 为 `compile`，输出目录与 work 目录均为 `<external-r0-evidence>` 下新建目录；顶层退出码为 `0`。

| 阶段 | 结果 | 证据 |
|---|---|---|
| Map | PASS | `logs/efinity_compile_current_head_retry1.log`、`output/mem_test.map.out` |
| Interface/Pt | PASS | `output/mem_test.pt.rpt` |
| PNR | PASS | `output/mem_test.route.rpt` |
| STA | PASS | `output/mem_test.timing.rpt` |
| CDC | PASS，无 synchronizer warning | `output/mem_test.cdc.rpt` |
| bitstream | PASS | `output/mem_test.pgm.out`、`output/mem_test.bit` |

STA 最差 setup 为 `+1.321 ns`（`axi0_ACLK -> axi0_ACLK`）；最差 hold 为 `+0.026 ns`（`i_sysclk_div2 -> i_sysclk_div2`）。

本批 warning 分阶段记录，不能合并成“零 warning”：

- Interface：4 条物理距离 design warning。
- `mem_test.warn.log`：248 条记录，`EFX-0011=198`、`EFX-0200=20`、`EFX-0201=10`、`EFX-0256=14`。
- post-synthesis：118 条 warning。
- route 原始报告：1912 条 `WARNING` 行、696 条 SDC warning 匹配、416 条 `No ports matched`；无 `ERROR` 或 `FATAL` 行。
- flow 原始输出包含 `WARNING: cannot find correct IV value`。

上述 warning 与当前 R0 构建同批记录；本 Packet 不裁定其可忽略，也不将离线 PASS 升级为板级 PASS。

## 4. 当前批次制品身份

| 制品 | 大小 | SHA-256 |
|---|---:|---|
| `output/mem_test.bit` | 11,847,132 B | `9F6F254E33C803C0F1B6D2F3CAB1929496477D9CBEE4655EC680A91B4028F320` |
| `output/mem_test.hex` | 11,847,192 B | `9C42E403ACCC8DFD8E7738A3B70F1E86ED01BDCF22B90B218DD898AE303234C4` |
| `cpu_bringup/uart_hello_onchip/build/uart_hello_onchip.elf` | 31,148 B | `CD4CAB96D3C30ECDC085B5C5A36B175DCED0016FBD53929BB0E2D3741E29411B` |

Hello ELF 构建通过：入口 `0xF9000000`，唯一 LOAD 段终点 `0xF9000A30`，RAM 使用 `2608 B / 16 KiB`，无未解析符号。

该组 hash 是新的 `R0-current-head` 批次；不等于旧 G1 的 `A897.../E5BC...` 或历史 M2 的 `2EA4.../C99...`，因此不继承任一旧批次的 JTAG、UART、APB 或板级结论。

## 5. 原始证据索引

| 项目 | SHA-256 |
|---|---|
| `logs/efinity_compile_current_head_retry1.log` | `19FEDE8EDBA5D775B24A861FBC86EF85C1E0FAB534A80987BFE6B9FCD1BD1C47` |
| `logs/riscv_hello_build_current_head.log` | `17EFC1106822F3611806B678BCC815D2C6A09D9319A93B5E12F96ABAB122633C` |
| `output/mem_test.timing.rpt` | `3E825CB27258E13D01F681D3FB626126B161C2DA333F0FE2DD91950375B15EFE` |
| `output/mem_test.cdc.rpt` | `0C4DEDEF53C5E37AAFB83940864046F70ABA77E0AFBCF0A11B815C7982637031` |
| `output/mem_test.pt.rpt` | `4038AD300FEEB9DE9571121D0EADACE2DDB93360A63D6B085B29B1B39D6D7D2D` |
| `output/mem_test.warn.log` | `4B3BBBFF9606B295E7DBBBC454B00420508E907E6ECEA8240155DD20B7E18DCE` |
| `output/mem_test.place.out` | `A885CB8837FE556EE6664F64E7A723AEFFD128BA3EDE4F33E4CFF80ED1D93DC2` |
| `output/mem_test.route.out` | `F3569ED848C0FE4153649D6B4ECEA34A303A366C7CBF0289C7C16AD9724E9F69` |

## 6. NOT VERIFIED 与下一 Gate

仍为 `NOT VERIFIED`：FPGA 易失性配置、`JTAG_USER2` 连接、四 hart halt、ELF 到 `0xF9000000` 的下载、运行时 PC、UART0 115200 横幅/回显、APB `0xE8100000` 的 `MAGIC=0x375A0001` 实读、视频和识别闭环。

下一步只能使用本 Packet 的匹配 bitstream/ELF，经 FPGA `USER2` 和片上 RAM 进入 R0 板级验证。继续禁止 `USER1`、Flash、外部 DDR、UART2/J52、机械臂、OSD 与任何动作。
