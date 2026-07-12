# Review Packet: 预处理合成数据源接入

> 日期：2026-07-12  
> 状态：已按用户指令修改 RTL；独立 map 与正式工程 map 已通过，仿真/PNR/bitstream/板级仍为 `NOT VERIFIED`

## 目标

在摄像头尚未产生稳定 CSI 数据流时，为已有 `vision_preprocess_channel` 提供确定的 RGB888 2ppc 输入，推进预处理特征与后续 CPU 接口开发。

## 修改范围

- 新增 `fpga/rtl/feature_extract/synthetic_2ppc_source.v`。
- 在 `fpga/rtl/top/top.v` 增加 `u_synthetic_preprocess_source`、预处理输入 mux，以及只用于可见板级验证的 `u_synthetic_hdmi_video_cdc` 和 HDMI 输入 mux。
- `fpga/efinity/mem_test.xml` 加入新源文件。

## 信号、时钟与复位

| 项目 | 结论 |
|---|---|
| 时钟 | `i_sysclk_div2`，与现有 ch1 Debayer 预处理 tap 相同 |
| 复位 | `pixel_data_en`，与现有 Debayer/预处理 tap 相同 |
| 输入格式 | 2ppc `{B1,G1,R1,B0,G0,R0}` |
| 图形 | 960x1080 中央方块，灰背景，每约一秒轮换红/蓝/黄/白/黑 |
| 切换 | `PREPROCESS_CH1_USE_SYNTHETIC_SOURCE=1` 选择预处理输入；`HDMI_USE_SYNTHETIC_VERIFY=1` 仅在验证时选择专用合成 HDMI CDC 输出 |

## 不改变的路径

- 不修改 `soft_mipi_rx_top`、I2C、MIPI、Framebuffer、DDR 或 AXI；HDMI 仅新增验证输入选择，原 ch0/ch1 CDC 实例和接线不变。
- `rgb1_*`、`rgb1_datax2` 仍照常驱动白平衡与 HDMI 原路径。
- 不新增 APB/CDC、OSD、CPU、UART 或机械臂控制。
- 不修改 `constrain.sdc`、`.peri.xml`、IP 设置。

## 安全与复位要求

- 两个验证开关默认开启只用于开发验证；实时摄像头验证或比赛构建前必须都设为 `0`。
- 当前 `i_snapshot_ack=1` 仍是像素域调试旁路行为，不代表 CPU `frame_id` ACK 已接通。
- 合成源不可触发真实机械臂；正式动作仍受 CPU/UART/接线/安全门约束。

## 验证计划

1. `HOST/RTL SIM NOT VERIFIED`：本机未找到 Icarus、Verilator 或 ModelSim；`tb_synthetic_2ppc_source.v` 已保存但未执行。
2. `MAP VERIFIED`：Efinity 2025.2 对 `synthetic_2ppc_source` 独立 map 通过，资源为 `EFX_ADD=30`、`EFX_LUT4=56`、`EFX_FF=52`。仅有 `object_rgb` 函数中常量位被综合优化的冗余信号 warning。
3. `MAP VERIFIED`：以 `D:\cicc_cbm_link\final_project\fpga\efinity\mem_test.xml` 做完整正式工程 map 通过。资源为 `EFX_ADD=2081`、`EFX_LUT4=11969`、`EFX_FF=10492`、`EFX_RAM10=250`、`EFX_DPRAM10=8`。新增模块未引入语法、展开、组合环或未连接端口错误；工程仍报告 586 条既有/IP 后综合网表 warning，不能忽略，未做时序签核。
4. PNR、bitstream、烧录和板级特征采集继续为 `NOT VERIFIED`，并仍受已知 Debugger VDB 与未约束 I/O 门禁阻塞。
