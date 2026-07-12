# A0 合成源可见基线冻结记录

> 日期：2026-07-12  
> 状态：A0 完成；A1 可开始只读 SoC 迁移勘查  
> 适用构建树：`D:\final_project`  
> 硬件观察：用户已确认 HDMI 显示灰底、居中正方形，并按红、蓝、黄、白、黑循环。

## 目标

冻结摄像头无稳定数据流期间的 HDMI 合成验证基线，确保后续 SoC 集成或摄像头恢复发生回归时，有明确的源码、产物、时序和回退对象。

该记录只证明合成源到 HDMI 的开发期验证链路可用，**不证明**真实 CSI、预处理特征、CPU/APB/OSD、四任务或机械臂闭环已经通过。

## D 盘已验证基线

### 源码与构建开关

| 项目 | 记录值 |
|---|---|
| 顶层 | `D:\final_project\fpga\rtl\top\top.v` |
| 合成源 | `D:\final_project\fpga\rtl\feature_extract\synthetic_2ppc_source.v` |
| 工程 | `D:\final_project\fpga\efinity\mem_test.xml` |
| 显示开关 | `HDMI_USE_SYNTHETIC_VERIFY = 1'b1` |
| 合成源实例 | `u_synthetic_preprocess_source` |
| 专用 CDC | `u_synthetic_hdmi_video_cdc` |
| 图形 | 960x1080 有效区内的 320x320 居中正方形，灰色背景 |
| 颜色顺序 | 红、蓝、黄、白、黑；每个颜色约 60 帧 |
| 输入格式 | RGB888 2ppc `{B1,G1,R1,B0,G0,R0}` |

### 哈希与时间戳

| 文件 | SHA-256 | 最后写入时间 |
|---|---|---|
| `fpga/rtl/top/top.v` | `A2DE2EB5413E859CC4F1A5D917B6E1E5B490701D66755929328CE99F55E2B49B` | 2026-07-12 15:38:44 |
| `fpga/efinity/mem_test.xml` | `41F8A67CF41FA03824A371B09C9A58ECB44DAF8D65943C1551401A2AAECCC71A` | 2026-07-12 16:08:54 |
| `fpga/rtl/feature_extract/synthetic_2ppc_source.v` | `0D91C669D570569EACD3ED51924A373C2355E69A1C3F2714374B451892F72812` | 2026-07-12 16:02:46 |
| `outflow/mem_test.bit` | `E28BE3E1AB454AF392F6EEAEB7BC1CEEF070B255A257620AB678D2A24C16D4DB` | 2026-07-12 16:08:52 |
| `outflow/mem_test.hex` | `290218007EACF6DAF08EDA8618EDB611A9A62E75088889F7C372E11D2C53B5D0` | 2026-07-12 16:08:53 |

`mem_test.xml` 在 bitstream 后写入属于 Efinity GUI 状态保存现象。本轮 map/PNR 日志均明确分析并展开了 `synthetic_2ppc_source`，且 `top.v` 的时间早于本轮 map/PNR/bitstream，因此本记录将该 bitstream 作为当前 D 盘合成 HDMI 基线。下次重新构建后必须重新记录哈希，不得沿用本条目。

### 构建证据

| 检查项 | 结果 | 证据 |
|---|---|---|
| Efinity 版本 | `2025.2.288.4.15` | `outflow/mem_test.pgm.out` |
| map | PASS；合成源已分析、编译和层级预综合 | `outflow/mem_test.map.out` |
| PNR | PASS；总耗时 103.221 s | `outflow/mem_test.route.out` |
| Setup 最小裕量 | `1.324 ns`（`axi0_ACLK`） | `outflow/mem_test.timing.rpt` |
| Hold 最小裕量 | `0.026 ns`（`i_sysclk_div2`、`mipi_rx_ck1_CLKOUT`） | `outflow/mem_test.timing.rpt` |
| CDC | 无 synchronizer warning | `outflow/mem_test.cdc.rpt` |
| bitstream | 已生成 `mem_test.bit` 与 `mem_test.hex` | `outflow/mem_test.pgm.out` |

已知工程仍含历史组合环 warning；本次未对其做“可忽略”裁定。该事实不影响本轮的合成源基线记录，但后续 SoC/CDC 改动必须单独审查新增 warning。

## C/D 差异与决策

| 项目 | C 盘协作树 | D 盘手动构建树 | 当前决策 |
|---|---|---|---|
| `synthetic_2ppc_source.v` | 与 D 盘哈希一致 | 已纳入并已构建 | 保持一致；后续改动必须双边显式审查 |
| `top.v` | 包含 `PREPROCESS_CH1_USE_SYNTHETIC_SOURCE`、`u_preprocess_ch1_tap` 和预处理快照 debug tap | 只含 HDMI 合成验证路径；不含预处理 tap | 不自动合并；D 盘保持可见 HDMI 基线，C 盘保持预处理开发基线 |
| `mem_test.xml` | 列出 ROI/预处理 6 个 RTL 和合成源 | 只列出合成源，不列预处理 RTL | 不自动合并；SoC 首次试验必须在隔离副本进行 |
| `D:\final_project` | 非 Git 协作树 | 实际手动构建/烧录树 | 禁止整树复制或覆盖；每次只提交最小差异清单 |

## 回退与使用规则

1. 若后续隔离 SoC 副本或 C 盘预处理开发导致 HDMI 回归，回退对象是本记录中 D 盘 `mem_test.bit` 和对应哈希，不覆盖 C 盘协作树。
2. 本基线仅用于开发期合成验证。真实摄像头验证或比赛构建前必须将 `HDMI_USE_SYNTHETIC_VERIFY` 改为 `1'b0`，并重新生成、记录、审核 bitstream。
3. D 盘当前未包含 `PREPROCESS_CH1_USE_SYNTHETIC_SOURCE` 或 `u_preprocess_ch1_tap`；不得把该基线描述为“合成源驱动预处理特征的板级验证”。
4. 本记录不授权机械臂、UART2、APB、OSD 或 CPU 接入。

## NOT VERIFIED

- 当前 D 盘已烧录到板的运行态与本条 bitstream SHA-256 的一一对应关系未由本次工具确认；仅有用户报告的 HDMI 画面。
- 合成源独立 testbench 未实跑。
- 合成源驱动 `vision_preprocess_channel` 的快照数值未板级采集。
- 真实摄像头的 CSI DE、Debayer 时序、预处理特征、CPU/APB/OSD 与机械臂均未验证。

## 下一步最小动作

执行 A1：在不修改 C/D 工程的前提下，只读扫描官方 RISC-V 例程，形成 QCRV32 SoC 的生成物、端口、时钟、复位、UART1、APB slave 和 `soc.h` 来源清单。

