# Codex Review Packet — PR1 视频链路迁移至 final_project（方案乙）

- 日期：2026-07-03
- 模式：Mode A 架构初设 → 待 Codex 复核
- 方案：乙（DSI IP 照迁、`top.v` 零改动、引脚悬空）
- 状态：**只读扫描完成，未改动任何工程文件**

---

## 任务目标

把赛方 demo `TJ375N529_SC431HAI2LCD_Demo_V3/` 的"摄像头 → HDMI 显示"最小链路迁入 `final_project/fpga/`，使 Efinity 2025.2.288.4.15 能打开 `final_project/fpga/efinity/mem_test.xml`、完成综合与布局布线、生成 bitstream，手动烧录到 TJ375N529 板后，通过 HDMI 在电脑显示器/采集卡上看到摄像头实时画面。

本轮**不引入** CPU 识别/决策、OSD、feature_extract、ROI、机械臂协议。这些决赛专属模块保留现有 `final_project/fpga/rtl/` 空骨架，待视频链路点亮后另起一轮。

## 当前结论

### 选定方案：乙
- `top.v` **一字不改**，`dsi_tx_top_inst1` 例化保留，`pixel_data_en` 仍由 DSI 正常产生。
- `ip/dsi_tx/`（含 `ipm/`）照迁到 `fpga/ip_vendor/dsi_tx/`。
- MIPI 屏物理引脚在 `.peri.xml` 中保留但板子上不接（悬空）。
- 综合会产生"未连接 MIPI 引脚"warning，记为可忽略，归档在风险清单。

### Fanout 扫描结论（甲方案被否决的依据）
`dsi_tx_top_inst1` 的输出 `pixel_data_en`（`src/top.v:1402`）并非 DSI 私有，它被整条视频后级当复位/使能源消费：
- `axi_interconnect` rst（1131, 1186 行）
- `debayer_top_2to1` in_rstn（1253, 1273 行）
- `white_balance` rst_n（1302, 1315 行）
- `dvi_tx` 段 rst_n（1371 行，**HDMI 链路本身**）
- framebuffer 写/读路径 rst_n（892, 1044 行）

结论：裸删 DSI（甲）或裸 ifdef 关 DSI（丙）会使 `pixel_data_en` 悬空，**HDMI 点不亮**。甲'（补产生逻辑）和丙'（保留例化+引脚 ifdef）虽可兼容但都改 `top.v` 顶层逻辑、命中 Codex Gate 且有时序风险。乙方案不动 `top.v`，是当前无可运行基线下风险最低的路径。

### 架构目录决策（用户已拍板）
1. **新建 `fpga/rtl/axi/` 目录**：容纳 `src/axi_interconnect/rtl/` + `src/axi_mux/`。理由：AXI 互连是 framebuffer 与将来 `cpu_if` 的共享资源，独立成目录边界更清晰。
2. **`vid_info_det_v7.v` 放入 `fpga/rtl/framebuffer/`**：理由：与 demo 中同类 `frame_info_det.v` 同属帧时序检测，紧耦合于帧缓存。
3. **`raw_unpack/` 保留空骨架**：demo 的 RAW 解包内嵌在 `soft_mipi_rx_top.v`，本轮不强行拆分。
4. **DSI 按乙方案处理**：源文件与 IP 全迁，`top.v` 不动。
5. **目录命名采用"镜像源结构 + 上述 2 处结构重映射"**：`src/mipi_csi/` → `rtl/mipi_csi/`、`src/contract_bright/` → `rtl/contract_bright/`、`src/mipi_dsi/` → `rtl/mipi_dsi/`、`src/uvc_src/` → `rtl/uvc_src/`。**不**重命名为现有 README 的 `video_in/`、`wb_gamma/`，原因：最小改写、最低迁移风险。现有 `video_in/`、`wb_gamma/`、`roi_crop/`、`feature_extract/`、`osd/`、`cpu_if/`、`uart_bridge/`、`debug/`、`common/` 空骨架保留为决赛预留，README 后续单独一轮对齐（见"未验证项"）。

## 修改或计划涉及的文件

### A. 新增工程文件（复制 + 路径改写）
| 来源 | 目标 | 处理 |
|---|---|---|
| `mem_test.xml` | `fpga/efinity/mem_test.xml` | 改写 design_file 路径、ip_info 路径、include 参数（见 D 节） |
| `mem_test.peri.xml` | `fpga/efinity/mem_test.peri.xml` | 原样复制，无路径改写（不含 src 路径） |
| `constrain.sdc` | `fpga/efinity/constrain.sdc` | 原样复制，无路径改写（引用端口名非文件路径）；**设备头注释红旗见风险项** |
| `debug_profile.wizard.json` | `fpga/efinity/debug_profile.wizard.json` | 复制后扫 src/ 引用并改写 |

### B. IP 迁移（整份含 ipm，ipm 进 .gitignore）
| 来源 | 目标 |
|---|---|
| `ip/csi_rx_controller/`（含 `settings.json`、`csi_rx_controller.sv`、`*_define.*`、`*_tmpl.*`、`ipm/`） | `fpga/ip_vendor/csi_rx_controller/` |
| `ip/dsi_tx/`（含 `settings.json`、`dsi_tx.sv`、`*_define.*`、`*_tmpl.*`、`ipm/`、`Testbench/`） | `fpga/ip_vendor/dsi_tx/` |

`ipm/` 体积约 224K + 248K，不大但含生成产物，进 `.gitignore` 不入版本库。

### C. RTL 迁移（严格按 `mem_test.xml` design_file 列表，非 src 全量）

`mem_test.xml` 的 design_file 列表是权威编译清单（demo `last_run_state="pass"`）。`src/` 中**不在**该列表的文件（`sim/`、`modelsim_tb/`、`*_tb.v`、`axi_connector.v`、`efx_axi_interconnect.v`、`Interrupt.v`、`lumi_change_v2.v`、`debayer/true_dual_port_ram.v`、`serdes_10_to_1.v`、`framebuffer/color_bar_rgb.v` 等）本轮**不迁**（testbench 或未用 alternates）。

| 源文件 | 目标路径 | 说明 |
|---|---|---|
| `src/top.v` | `fpga/rtl/top/top.v` | 顶层，零改动 |
| `src/vid_info_det_v7.v` | `fpga/rtl/framebuffer/vid_info_det_v7.v` | 决策 2 重映射 |
| `src/framebuffer/fifo_d512t128.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/ddr_rd_buffer.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/frame_info_det.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/ddr_wr_buffer.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/frame_buffer.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/ddr_buffer.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/data_tx.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/vid_rx_align_v1.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/ser2par_24_128_v1.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/ser2par.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/bank_switch.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/par2ser_512t128.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/par2ser_parse.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/color_barchecker.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/rst_n_piple.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/elt_dcfifo_v10.v` | `fpga/rtl/framebuffer/` | |
| `src/framebuffer/vid_par2ser.v` | `fpga/rtl/framebuffer/` | |
| `src/axi_interconnect/rtl/axi_interconnect.v` | `fpga/rtl/axi/` | 决策 1 重映射 |
| `src/axi_interconnect/rtl/priority_encoder.v` | `fpga/rtl/axi/` | 决策 1 重映射 |
| `src/axi_interconnect/rtl/arbiter.v` | `fpga/rtl/axi/` | 决策 1 重映射 |
| `src/axi_mux/Axi_Mux.v` | `fpga/rtl/axi/` | 决策 1 合并 |
| `src/debayer/debayer_top_2to1.v` | `fpga/rtl/debayer/` | |
| `src/debayer/raw_to_rgb.v` | `fpga/rtl/debayer/` | |
| `src/debayer/line_buffer.v` | `fpga/rtl/debayer/` | |
| `src/debayer/rgb_gain.v` | `fpga/rtl/debayer/` | |
| `src/contract_bright/contrast_bright_top.v` | `fpga/rtl/contract_bright/` | 镜像源名 |
| `src/contract_bright/Contrast_Adj.v` | `fpga/rtl/contract_bright/` | |
| `src/contract_bright/Reciprocal.v` | `fpga/rtl/contract_bright/` | |
| `src/dvi_tx/encode.v` | `fpga/rtl/dvi_tx/` | |
| `src/dvi_tx/hdmi_top.v` | `fpga/rtl/dvi_tx/` | |
| `src/dvi_tx/dvi_encoder.v` | `fpga/rtl/dvi_tx/` | |
| `src/mipi_csi/mipi_csi_top.sv` | `fpga/rtl/mipi_csi/` | |
| `src/mipi_csi/soft_mipi_rx_top.v` | `fpga/rtl/mipi_csi/` | |
| `src/mipi_csi/cam_i2c_ctrl/i2c/i2c_16addr_8data.v` | `fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/` | |
| `src/mipi_csi/cam_i2c_ctrl/i2c/i2c_8addr_8data.v` | `fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/` | |
| `src/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_ctrl_top.v` | `fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/` | |
| `src/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_reg_rom.v` | `fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/` | |
| `src/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_reg_set.v` | `fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/` | |
| `src/mipi_csi/cam_i2c_ctrl/i2c_master/i2c_master_bit_ctrl.v` | `fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c_master/` | |
| `src/mipi_csi/cam_i2c_ctrl/i2c_master/i2c_master_byte_ctrl.v` | `fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c_master/` | |
| `src/mipi_csi/cam_i2c_ctrl/i2c_master/i2c_master_top.v` | `fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c_master/` | |
| `src/mipi_csi/cam_i2c_ctrl/i2c_master/oc_i2c_master.v` | `fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c_master/` | |
| `src/mipi_dsi/dsi_tx_top.v` | `fpga/rtl/mipi_dsi/` | 乙方案保留 |
| `src/mipi_dsi/panel_config.v` | `fpga/rtl/mipi_dsi/` | 乙方案保留 |
| `src/mipi_dsi/reset_ctrl.v` | `fpga/rtl/mipi_dsi/` | 乙方案保留 |
| `src/mipi_dsi/true_dual_port_ram.v` | `fpga/rtl/mipi_dsi/` | 乙方案保留 |
| `src/uvc_src/uvc_top.v` | `fpga/rtl/uvc_src/` | `UVC_EN` 关，模块编译但未例化 |
| `src/uvc_src/yuv_2rgb/.../rgb_to_ycbcr.v` | `fpga/rtl/uvc_src/yuv_2rgb/.../` | |
| `src/uvc_src/yuv444_yuv422.v` | `fpga/rtl/uvc_src/` | |
| `src/uvc_src/color_bar_v3.0/color_bar_rgb.v` | `fpga/rtl/uvc_src/color_bar_v3.0/` | |
| `src/uvc_src/white_balance.v` | `fpga/rtl/uvc_src/` | |

**说明**：`src/mipi_dsi/dsi_top.sv`、`efx_dsi_tx_top.sv` 不在 XML design_file 列表，本轮不迁（DSI 实际走 `dsi_tx_top.v` + IP `dsi_tx.sv`）。

### D. `mem_test.xml` 路径改写规则
- 每个 `<efx:design_file name="src/X">` → `name="rtl/X"`（X 按上表映射，含决策 1/2 的 2 处重映射）。
- `<efx:ip path="ip/csi_rx_controller/settings.json">` → `path="ip_vendor/csi_rx_controller/settings.json"`。
- `<efx:ip path="ip/dsi_tx/settings.json">` → `path="ip_vendor/dsi_tx/settings.json"`。
- `<efx:param name="include" value="ip/csi_rx_controller">` → `value="ip_vendor/csi_rx_controller"`。
- `<efx:param name="include" value="ip/dsi_tx">` → `value="ip_vendor/dsi_tx"`。
- 其余参数（综合/PnR/bitstream）原样保留。

### E. `.gitignore` 增量（`final_project/.gitignore`）
新增：
```
fpga/ip_vendor/*/ipm/
fpga/efinity/outflow/
fpga/efinity/work_syn/
fpga/efinity/work_pnr/
fpga/efinity/work_dbg/
```

## 关键模块与信号链路

```text
摄像头 ─ MIPI CSI lanes
  → csi_rx_controller IP (ip_vendor/csi_rx_controller)
  → soft_mipi_rx_top + mipi_csi_top (rtl/mipi_csi, 含 RAW 解包)
  → cam_i2c_ctrl (rtl/mipi_csi/cam_i2c_ctrl, 摄像头寄存器初始化)
  → vid_info_det_v7 (rtl/framebuffer, 行场时序检测)
  → frame_buffer + bank_switch + ddr_wr/rd_buffer (rtl/framebuffer)
  → axi_interconnect + Axi_Mux (rtl/axi) → DDR
  → debayer_top_2to1 + raw_to_rgb + rgb_gain (rtl/debayer, Bayer→RGB)
  → white_balance + contrast_bright_top (rtl/contract_bright, 对比度亮度)
  → hdmi_top + dvi_encoder + encode (rtl/dvi_tx, TMDS 编码 → HDMI)
  → dsi_tx_top (rtl/mipi_dsi, 乙方案保留, 输出 pixel_data_en 给整条链路当复位)
```

**关键信号**：
- `pixel_data_en`（DSI 输出）：整条视频后级 rst_n/rst 源，乙方案保留其产生。
- `arst_n`（610 行）：`sys_pll_lock & ddr_pll_lock & pll_byteclk_locked & MIPI_TX_PLL_LOCKED`，全局异步复位。
- `wb1_vs_out/wb1_hs_out/wb1_de_out/wb1_data_out`：白平衡后视频流，分两路喂 HDMI（`hdmi_top_inst`）和 DSI（`dsi_tx_top_inst1`）。
- `i_sysclk_div2`：主像素/AXI 时钟域（14.286ns period）。
- `axi0_ACLK`：AXI 时钟域（5ns period）。
- `hdmi_tx_slow_clk` / `hdmi_tx_fast_clk`：TMDS 输出时钟。

## 时钟、复位、AXI、framebuffer、双通道影响

### 时钟
- 顶层时钟均为 `input`（`syn_peri_port=0`），由 `.peri.xml` Interface Designer 的 PLL 配置产生，**非 RTL 例化**。因此 `.peri.xml` 必须完整迁移，是 HDMI 点亮的关键。
- SDC 定义了 14 个时钟约束，含 `set_clock_groups` 异步分组。迁移后时钟名/端口名不变（顶层 IO 名不变），**SDC 无需改写**。

### 复位
- `arst_n` 由 4 个 PLL lock 信号相与产生。
- `pixel_data_en` 是 DSI 输出的"链路稳定"信号，乙方案保留。
- 乙方案不改任何复位拓扑。

### AXI / framebuffer
- `axi_interconnect`（rtl/axi）连接 S_COUNT=3 个 master 到 M_COUNT=1 个 DDR slave。
- 决策 1 把 `axi_interconnect/rtl/` 3 文件 + `axi_mux/Axi_Mux.v` 放进 `rtl/axi/`，**只改目录不改模块名/信号名**，综合无影响。
- DDR 地址宽度 33bit，START_ADDR=0，3 buffer（FB_NUM=3）。

### 双通道对称
- demo 按 S0/S1 双通道组织（`S0_io_cam_*` / `S1_io_cam_*`）。本轮迁入的是双通道完整顶层，**双通道结构保持不动**。
- 乙方案 `top.v` 零改动，双通道对称性完全保留。

## 已运行验证

本轮全部为只读扫描，未执行任何综合/仿真。

- 命令：
  - `ls` / `find` / `grep` / `awk` 扫描 `赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/` 与 `final_project/`
  - `Read` 读取 `mem_test.xml`、`top.v`、各 README、`constrain.sdc`、`mem_test.peri.xml` 摘要
- 结果：
  - 来源工程存在且完整：`mem_test.xml`（`last_run_state="pass"`，`last_run_flow="bitstream"`，`sw_version="2025.2.288.4.15"`）+ `.peri.xml` + `constrain.sdc` + `src/`（76 .v/.sv）+ `ip/`（csi_rx_controller + dsi_tx，各带 ipm）
  - `final_project/fpga/` 为纯骨架：`rtl/` 仅含 README，`efinity/`/`ip_local/`/`ip_vendor/` 仅 README，无 .v、无工程文件
  - `mem_test.xml` IP 列表确认：仅 `csi_rx_controller` + `dsi_tx` 两块（`ip/ram/` 未列入，本轮不迁）
  - `top.v` 宏：`FRAME_BUFFER` 定义、`HDMI_OUT_EN` 定义、`UVC_EN` 注释、`CONTRAST_BRIGHT_EN` 注释 → 天然就是摄像头→HDMI 链路
  - `dsi_tx_top_inst1` 在 1392 行，顶层无条件例化（不在 `FRAME_BUFFER` ifdef 内）
  - `pixel_data_en` fanout 扫描确认：被 axi_interconnect/debayer/white_balance/dvi_tx/framebuffer 共 11 处消费
  - `constrain.sdc` 无绝对路径、`mem_test.xml` 无绝对路径、`mem_test.peri.xml` 无绝对路径
- 日志位置：本轮无新增日志文件，扫描输出在会话中
- 关键 warning：
  - **`constrain.sdc` 头部注释 `Device: Ti180J484` / `Timing Model: I4 (final)` 与 `mem_test.xml` 声明的 `TJ375N529` / `I3` 不匹配**（见风险项 R1）
  - `赛方提供材料/` 被 `.gitignore:86` 隔离，原件不进版本库（预期行为）

## 机械臂 / 外设状态

- myCobot 是否涉及：**否**。本轮纯视频链路迁移，不涉及机械臂。
- COM 口 / CP210x 状态：未检查（与本轮无关）。
- 波特率：不涉及。
- 是否执行动作：否。
- 安全确认：不涉及机械臂动作。

## 未验证项和风险假设

### R1（红旗，需 Codex 判断）SDC 设备/时序模型不匹配
`constrain.sdc` 头部 auto-generated 注释写 `Device: Ti180J484`、`Timing Model: I4 (final)`、`Version: 2024.1.163.4.11`，但 `mem_test.xml` 声明 `TJ375N529` / `I3` / `2025.2.288.4.15`。
- 可能性 A：SDC 是从旧 Ti180J484 工程拷来的残留注释，但实际时钟约束按 TJ375N529 端口写、可正常工作（demo `last_run_state=pass` 支持此判断）。
- 可能性 B：SDC 约束目标与 TJ375N529 实际可用资源/时序不匹配，在更严格 PnR 下可能失败。
- 建议 Codex 判断：是否需要重新从 Efinity Interface Designer 生成 TJ375N529/I3 对应的 SDC，还是当前 SDC 可直接沿用。

### R2 `.peri.xml` 是 PLL / 引脚配置真源，迁移后需 Efinity 重新打开验证
- PLL 配置（`pll_inst1_CLKOUT0`、`axi0_ACLK`、`mipi_dphy_tx_*` 等）由 peri.xml 定义。若 Efinity 打开后 peri.xml 与器件库版本不兼容，PLL 可能挂红。
- 未验证：本机 Efinity 2025.2.288.4.15 打开迁移后工程，PLL 是否全部就绪。这是阶段 1 判据。

### R3 `debug_profile.wizard.json` 可能含 src/ 探针路径
- 未扫该 JSON 内部是否引用 `src/` 路径。若引用，需同步改写。
- 阶段 1 执行时先扫一遍，有引用则按 D 节规则改写。

### R4 `ipm/` 可能含本机绝对路径或 Efinity 版本绑定内容
- 复制后首次在 Efinity 打开，IP 若挂红，可能因 `ipm/` 内绝对路径残留或版本绑定。
- 阶段 1 执行时扫 `ip/*/ipm/` 内是否有 `C:\` 或 `/home/` 绝对路径，有则报告但不擅自手改（属 Codex Gate）。

### R5 MIPI 引脚悬空 warning
- 乙方案 `dsi_tx_top_inst1` 物理输出（`P1_lcd_power_en`、`P1_lcd_rstp`、`mipi_tx_ck1_*`、`mipi_tx_dp1*_*`，约 36 个）在板子上不接。
- 综合会产生未连接引脚 warning。需在 `.peri.xml` 中确认这些引脚的 OE/拉接配置，避免悬空导致异常功耗。Codex 需判断是否需要在 peri.xml 中显式禁用这些 IO。

### R6 现有 README 目录命名与迁移后实际不一致
- 现有 `rtl/video_in/`、`rtl/wb_gamma/`、`rtl/raw_unpack/` 等 README 描述的功能，本轮迁入文件放在 `rtl/mipi_csi/`、`rtl/contract_bright/`、（raw_unpack 留空）。团队读代码时易混淆。
- 本轮**不**重命名对齐，优先降低迁移风险。后续单独一轮更新 README 分层说明。Codex 需判断此取舍是否可接受。

### R7 `UVC_EN` 关但 uvc_src 文件参与编译
- `uvc_top.v` 等 5 文件在 XML design_file 列表中，但 `UVC_EN` 注释掉，模块定义存在但顶层未例化。综合无害但占编译时间。保留以最小化 XML 改动。Codex 需确认是否可接受。

### R8 上板验证依赖硬件就绪
- 用户已确认：板子、摄像头、HDMI 接电脑、Efinity 2025.2 就绪；Efinity MCP 未就绪，手动烧录。
- 阶段 3 需用户手动操作 Efinity Programmer，本 Agent 不直接烧录。

## 希望 Codex 判断的问题

1. **方案乙整体是否可行**：在 `top.v` 零改动、DSI IP 照迁引脚悬空的前提下，迁移路径改写是否真能让 Efinity 打开并通过综合？是否有遗漏的依赖（如 `ipm/` 内部互引、`include` 参数之外的其他 IP 引用）？
2. **R1 SDC 设备不匹配**：`constrain.sdc` 的 Ti180J484/I4 注释与 TJ375N529/I3 工程不匹配，是否可直接沿用当前 SDC 综合到 TJ375N529/I3？还是必须重新生成？若沿用，是否有时序闭合风险？
3. **R5 MIPI 悬空引脚**：乙方案下 `.peri.xml` 中 DSI 物理引脚应如何配置（保留 OE、下拉、还是显式禁用）才不会在 TJ375N529 板上造成异常功耗或综合错误？
4. **R6 目录命名取舍**：本轮采用"镜像源结构 + 2 处重映射"，保留 `rtl/mipi_csi/`、`rtl/contract_bright/` 而非对齐现有 README 的 `video_in/`、`wb_gamma/`。此取舍是否合理？还是应在本轮一并重命名对齐？
5. **R7 uvc_src 保留**：`UVC_EN` 关但 uvc_src 文件保留在编译列表，是否应从 XML design_file 列表移除这 5 个文件以精简工程？移除是否影响 `top.v` 中 ifdef 守护的例化？
6. **R2 PLL 配置**：`.peri.xml` 迁移后，Efinity 2025.2.288.4.15 是否需要重新生成 PLL IP，还是 peri.xml 原样加载即可？
7. **`ip/ram/` 不迁**：`mem_test.xml` IP 列表不含 `ram`，`top.v` 也未引用 `ram.v`。确认 `ip/ram/` 确实不参与本工程编译，不迁无风险？
8. **阶段 1 完成判据**：除"Efinity 打开无缺文件、IP 不挂红、Synthesis 通过"外，是否还应增加"灰盒报错清零"或"资源利用率初评"作为判据？

## 执行顺序（Codex 复核通过后）

1. 复制工程文件（A 节）+ IP（B 节）+ RTL（C 节）到 `final_project/fpga/`
2. 按 D 节改写 `mem_test.xml` 路径
3. 扫并改写 `debug_profile.wizard.json`（R3）
4. 扫 `ip/*/ipm/` 绝对路径残留（R4），有则报告
5. 更新 `final_project/.gitignore`（E 节）
6. 在 `final_project/docs/migration_log.md` 追加本轮迁移条目
7. 交付阶段 1，由用户在 Efinity 打开验证（R2），反馈结果后再进阶段 2 综合

---

## Codex 复核意见（2026-07-03）

### 复核结论

当前方案乙的方向可以作为最小迁移路径继续推进，但**不能按现有执行顺序直接开工**。主要原因不是 `top.v` 零改动本身，而是迁移清单遗漏了 XML `design_file` 之外的 HDL include 和 ROM 初始化依赖；如果照当前 C/D 节执行，迁移后大概率在综合阶段出现 include 缺失、初始化文件找不到，或 DSI 配置未完成导致 `pixel_data_en` 永远不拉高。

建议状态：**有条件通过架构方向，阻塞执行清单需先修订**。

### 阻塞问题

#### B1 XML design_file 不是完整迁移清单，遗漏 include / mem 依赖

证据：
- `src/axi_mux/Axi_Mux.v:45` 有 `` `include "Axi_Mux_Param.vh"``，但 `src/axi_mux/Axi_Mux_Param.vh` 不在 `mem_test.xml` 的 `design_file` 列表，也未列入 C 节迁移表。
- `src/mipi_csi/cam_i2c_ctrl/i2c_master/i2c_master_bit_ctrl.v:141/144`、`i2c_master_top.v:87/90`、`i2c_master_byte_ctrl.v:83/86` 均 include `timescale.v` 和 `i2c_master_defines.v`，这两个文件同样不在 XML design_file 列表，也未列入迁移表。
- `src/mipi_dsi/dsi_tx_top.v:144` 将 `panel_config.INITIAL_CODE` 设为 `"/src/mipi_dsi/Panel_1080p_reg.mem"`；`panel_config.v` 通过 `true_dual_port_ram.RAM_INIT_FILE` 最终调用 `$readmemh/$readmemb` 读取该文件。`Panel_1080p_reg.mem` 未列入迁移表。

影响：
- `Axi_Mux_Param.vh` 缺失会导致编译直接失败。
- I2C include 缺失会导致编译直接失败。
- `Panel_1080p_reg.mem` 缺失或路径错误会导致 DSI 初始化 ROM 为空/读取失败；`w_confdone` 不可靠，`pixel_data_en = vid_rst_n = dly_cnt[26] after w_confdone` 可能永远不释放，进而 framebuffer、AXI、debayer、white_balance、HDMI 均被复位卡住。

最小修复建议：
- 在 C 节新增“非 design_file 但必迁依赖”小节，至少迁移：
  - `src/axi_mux/Axi_Mux_Param.vh` -> `fpga/rtl/axi/Axi_Mux_Param.vh`
  - `src/mipi_csi/cam_i2c_ctrl/i2c_master/timescale.v` -> `fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c_master/timescale.v`
  - `src/mipi_csi/cam_i2c_ctrl/i2c_master/i2c_master_defines.v` -> `fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c_master/i2c_master_defines.v`
  - `src/mipi_dsi/Panel_1080p_reg.mem` -> `fpga/rtl/mipi_dsi/Panel_1080p_reg.mem`
- 迁移后必须确认 Efinity 的 include 搜索路径能找到 `rtl/axi` 和 `rtl/mipi_csi/cam_i2c_ctrl/i2c_master`。如果只保留 `ip_vendor/*` include 参数，HDL 本地 include 可能找不到。
- `dsi_tx_top.v` 中 `"/src/mipi_dsi/Panel_1080p_reg.mem"` 是危险路径。不要保留绝对样式 `/src/...`。应改成迁移后可解析的相对路径，例如 `"rtl/mipi_dsi/Panel_1080p_reg.mem"`，或确认 Efinity 对 `$readmem*` 的当前工作目录规则后写入稳定路径。该项会触碰 `top.v` 以外 RTL，但不改变功能逻辑，风险低于让初始化文件不可控。

#### B2 C 节迁移表存在真实文件名/路径错误

证据：
- XML 真实条目是 `src/framebuffer/color_bar_checker.v`，方案表写成 `color_barchecker.v`。
- XML 真实条目是 `src/uvc_src/yuv_2rgb/yuv444_yuv422.v`，方案表把 `yuv444_yuv422.v` 写到 `fpga/rtl/uvc_src/`，少了 `yuv_2rgb/` 目录层级。

影响：
- 如果按表手工复制/改写 XML，会产生缺文件或 XML 指向错误路径。

最小修复建议：
- 以 `mem_test.xml` 真实 `design_file` 为准生成迁移表，不要以人工表为准。
- 修正 `color_bar_checker.v` 拼写。
- 修正 `yuv444_yuv422.v` 的目标路径为 `fpga/rtl/uvc_src/yuv_2rgb/yuv444_yuv422.v`，除非同步改写 XML 到实际放置位置。

#### B3 `P1_lcd_rstp` 与顶层/peri 端口名不一致，需先确认原工程如何通过

证据：
- `top.v:194` 顶层端口是 `P1_o_lcd_rstn`。
- `top.v:1405` DSI 例化连接 `.LCD_RST_P(P1_lcd_rstp)`。
- `.peri.xml:116-117` 也定义的是 `P1_o_lcd_rstn`。
- 扫描未发现 `P1_lcd_rstp` 的顶层端口或显式 wire 声明。

影响：
- 在 Verilog 2001 默认网线规则下，`P1_lcd_rstp` 可能被隐式声明为内部 wire，导致 DSI reset 输出没有接到 `.peri.xml` 的真实端口 `P1_o_lcd_rstn`。这解释不了原工程 bitstream pass，但会影响“保留 DSI 引脚”的判断：方案中列的 `P1_lcd_rstp` 并不等于实际物理端口名。
- 若工具启用隐式 net warning/error 策略，迁移后会出现 warning；若后续禁用隐式 net，会变成编译错误。

最小修复建议：
- 执行前先查原工程 synthesis log 中是否有 implicit wire / undeclared net warning。
- 不建议本轮顺手修 top，除非 Efinity 明确报错或上板 DSI 初始化依赖该 reset 端口。若必须修，最小改动应是把 `P1_lcd_rstp` 改为 `P1_o_lcd_rstn`，但这会违反“top.v 一字不改”，需要单独 Gate。
- 在 R5 风险描述中把 `P1_lcd_rstp` 改为“源码连接名 `P1_lcd_rstp` 与 peri 端口 `P1_o_lcd_rstn` 不一致，需核实”。

### 高风险问题

#### H1 DSI 悬空不是简单的未连接 warning，可能持续驱动 TX DPHY

证据：
- `.peri.xml` 中 `mipi_tx_ck1`、`mipi_tx_dp10` 至 `mipi_tx_dp13` 均为 `ops_type="tx"` 的 MIPI DPHY，并有 `HS_OE`、`LP_P_OE`、`LP_N_OE`、`HS_OUT`、`LP_*_OUT` 等输出。
- `top.v:1404-1448` 中 `dsi_tx_top_inst1` 实际连接了 P1 的 DSI TX lane，且 DSI IP 配置 `ENABLE_BIDIR=1'b1`、`DPHY_CLOCK_MODE="Continuous"`。
- `MIPI_TX_PLL_LOCKED` 参与 `arst_n`，`MIPI_TX_PLL` 不只是“无害保留”，而是全局复位释放条件之一。

判断：
- 保留 DSI IP 是让 `pixel_data_en` 生成的低改动路线，可以接受；但 R5 不能写成“综合会产生未连接引脚 warning，可忽略”。这些是实际 top-level 输出和 peri TX DPHY 配置，工具可能不会把它们当“未连接”，而是正常实现并持续驱动未接接口。

最小修复建议：
- 阶段 1 必须记录 MIPI TX 相关 warning 的完整文本，不能预设可忽略。
- 上板前确认 LCD/MIPI P1 接口物理上确实未外接任何负载，不与其他板载电路短接。
- 若功耗/发热/干扰异常，优先考虑方案丙'：保留 DSI 内部初始化和 `pixel_data_en`，但隔离/常态禁用物理 TX OE。这会改 `top.v`，但比保留活跃 DPHY 输出更可控。

#### H2 SDC 设备头注释不匹配本身不是最大问题，约束内容与迁移后端口/自动生成名一致性才是重点

证据：
- `constrain.sdc:12/14` 注释是 `Ti180J484` / `I4`，但 `mem_test.xml` 与 `.peri.xml` 均为 `TJ375N529` / 2025.2。
- SDC 里大量约束引用自动生成端口名，例如 `mipi_rx_ck0_CLKOUT~CLKOUT~25~963` 这类带位置/后缀的 reference pin。

判断：
- 如果原工程在同一套源码/peri 下已经 `last_run_flow="bitstream"`，头部注释可以暂按历史残留处理，不需要先重生成整个 SDC。
- 真正风险是迁移后 `.peri.xml` 重新加载/生成时，自动生成的 reference pin 名称是否保持一致。若不一致，SDC 会出现 unmatched get_ports 或约束失效。

最小修复建议：
- 迁移后第一轮 Efinity 打开时必须检查 SDC 解析日志：`get_ports` unmatched、ignored constraint、invalid reference pin 均不可忽略。
- 不建议在未跑通前重生成 SDC。先保持原 SDC，记录约束解析结果；只有在 Efinity 报 reference pin 不匹配或 PLL/DPHY 重新生成后，再重导出 SDC。

#### H3 IP settings 中存在绝对路径和版本差异，ipm 不能直接忽略

证据：
- `ip/dsi_tx/settings.json` 的 `--base_path` 是 `D:\Project\...`，`sw_version` 是 `2025.2.288.3.8`。
- `ip/csi_rx_controller/settings.json` 的 `--base_path` 是 `D:\Computer\...`，`sw_version` 是 `2025.2.288.4.15`。

判断：
- 绝对路径位于 settings 的生成参数里，未必被 Efinity 打开工程时实际使用，但这是必须记录的可移植性风险。
- DSI IP 生成版本低于工程版本，且本方案依赖 DSI 产生 `pixel_data_en`，所以不能只扫 `ipm/`，还要扫 `settings.json`。

最小修复建议：
- R4 扩展为扫描 `ip_vendor/*/settings.json` 与 `ip_vendor/*/ipm/*` 的绝对路径和版本。
- 若 IP 打开挂红，优先在 Efinity 2025.2.288.4.15 中重新生成同参数 IP，而不是手改 pickle/ipm。

### 可接受取舍

#### A1 `top.v` 零改动方向可接受，但不是长期架构

`pixel_data_en` 当前确实被 framebuffer、AXI、debayer、white_balance、HDMI 多处消费。裸删 DSI 会卡住 HDMI，这一点方案判断正确。为了快速建立可运行基线，本轮保留 DSI 例化可以接受。

但这只是迁移保底策略，不应固化为决赛架构。视频链路点亮后，下一轮应把“视频后级复位释放/链路稳定”从 DSI 初始化完成中解耦，改为明确的视频/DDR/PLL ready 组合或独立 reset controller。

#### A2 `ip/ram/` 不迁可接受

`mem_test.xml` 的 IP 列表只有 `csi_rx_controller` 和 `dsi_tx`；`top.v` 未直接引用 `ram.v`。当前不迁 `ip/ram/` 可接受。

注意：这不等于“不迁任何 mem”。`src/mipi_dsi/Panel_1080p_reg.mem` 是 `$readmem*` 初始化文件，必须迁移。

#### A3 `uvc_src` 保留在编译列表可接受

`UVC_EN` 关闭但 XML 保留 `uvc_src` 文件，属于原工程清单的一部分。为降低迁移变量，本轮保留可接受。后续清理应在成功综合和上板后单独做，避免把“工程精简”和“视频链路迁移”耦合。

#### A4 目录命名采用镜像源结构可接受

本轮不强行把 `mipi_csi` 改名成 `video_in`、不把 `contract_bright` 改名成 `wb_gamma`，这个取舍合理。现阶段稳定重现 demo 比目录语义更重要。

需要补一个短 README 或迁移说明：说明 `video_in/`、`wb_gamma/` 等仍是决赛目标分层骨架，PR1 迁移进来的 demo 代码暂按源结构放置，避免团队误解为目录规划被推翻。

### 修订后的执行前置条件

在执行原“执行顺序”前，必须先完成以下修订：

1. 修正迁移表中的 `color_bar_checker.v`、`yuv_2rgb/yuv444_yuv422.v` 路径。
2. 增加非 XML design_file 依赖迁移清单：`Axi_Mux_Param.vh`、`timescale.v`、`i2c_master_defines.v`、`Panel_1080p_reg.mem`。
3. 明确 `$readmem*` 文件路径策略，不能保留 `"/src/mipi_dsi/Panel_1080p_reg.mem"` 这种迁移后不稳定路径。
4. 明确本地 HDL include 搜索路径策略，不能只改 IP include。
5. 将 R4 扩展为扫描 `settings.json` 和 `ipm` 的绝对路径、版本差异。
6. 将阶段 1 判据扩展为：
   - Efinity 打开无 missing file。
   - HDL include 无缺失。
   - `$readmem*` 初始化文件解析成功。
   - IP 不挂红。
   - SDC 无 unmatched/ignored 关键约束。
   - synthesis 通过，且 blackbox 为零。
   - MIPI TX / DSI 悬空相关 warning 全量归档，不能预设忽略。

### 对“希望 Codex 判断的问题”的逐项答复

1. **方案乙整体是否可行**：方向可行，但当前迁移清单不完整，未修 B1/B2 前不建议执行。
2. **R1 SDC 设备不匹配**：头部注释可暂不作为阻塞；阻塞判据应改为迁移后 SDC 解析是否有 unmatched/ignored 约束。先沿用原 SDC，不先重生成。
3. **R5 MIPI 悬空引脚**：不能简单归类为可忽略 warning。当前设计会实现并驱动 DSI TX DPHY。阶段 1 必须保留并审查相关 warning；上板前确认物理接口无负载。异常时改走“保留内部 `pixel_data_en`、隔离 TX OE”的小改方案。
4. **R6 目录命名取舍**：可接受，后续补 README 对齐即可。
5. **R7 uvc_src 保留**：可接受，暂不从 XML 删除。
6. **R2 PLL 配置**：先原样迁移 `.peri.xml`，不要预先重生成。打开后以 PLL/IP 是否挂红、端口/SDC 是否匹配为准。
7. **`ip/ram/` 不迁**：可接受；但 `Panel_1080p_reg.mem` 必须迁。
8. **阶段 1 完成判据**：需要扩展，见“修订后的执行前置条件”第 6 点。

---

## PR1-v2 修订执行计划（Claude 基于 Codex 复核意见修订，2026-07-03）

> Codex 的 B1/B2/B3 阻塞问题和 H1/H2/H3 高风险已全部经只读核实确认属实。本节是修订后的最终执行计划，取代原文“执行顺序”一节。

### 已核实确认的遗漏依赖（B1 修订）

原 C 节迁移表只覆盖 `mem_test.xml` 的 `design_file` 列表，遗漏了以下"非 design_file 但必迁"的 HDL include / ROM 初始化文件。已逐条核实存在：

| 源文件 | 被引用处 | 目标路径 | 引用类型 |
|---|---|---|---|
| `src/axi_mux/Axi_Mux_Param.vh` | `Axi_Mux.v:45` `` `include `` | `fpga/rtl/axi/Axi_Mux_Param.vh` | HDL include |
| `src/mipi_csi/cam_i2c_ctrl/i2c_master/timescale.v` | `i2c_master_bit/byte/top.v` 多处 include | `fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c_master/timescale.v` | HDL include |
| `src/mipi_csi/cam_i2c_ctrl/i2c_master/i2c_master_defines.v` | 同上 | `fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c_master/i2c_master_defines.v` | HDL include |
| `src/mipi_dsi/Panel_1080p_reg.mem` | `dsi_tx_top.v:144` INITIAL_CODE + `panel_config.v` 经 `true_dual_port_ram` 走 `$readmem*` | `fpga/rtl/mipi_dsi/Panel_1080p_reg.mem` | `$readmem*` 初始化 |

**注意**：`src/mipi_dsi/IPhone_7p_1080p_reg.mem` 同目录另一份 mem 不在引用链路上，本轮不迁。

### 已修正的文件名/路径错误（B2 修订）

原 C 节迁移表两处错误，已按 `mem_test.xml` 真实条目修正：

- `src/framebuffer/color_bar_checker.v`（原表误写 `color_barchecker.v`）→ `fpga/rtl/framebuffer/color_bar_checker.v`
- `src/uvc_src/yuv_2rgb/yuv444_yuv422.v`（原表漏 `yuv_2rgb/` 层级）→ `fpga/rtl/uvc_src/yuv_2rgb/yuv444_yuv422.v`

### B3 端口名不一致：本轮处理策略

已核实：顶层端口 `P1_o_lcd_rstn`（top.v:194）与 DSI 例化连接名 `P1_lcd_rstp`（top.v:1405）不一致，靠 Verilog 隐式 net 通过。

**本轮策略**：维持 `top.v` 零改动（乙方案核心），不去碰这个端口名。阶段 1 Efinity 打开后：
- 若 synthesis log 无 implicit net warning → 按原工程方式保留，不动。
- 若 log 报 implicit net warning 或 error → 记录为阻塞项，单独走 Codex Gate 讨论是否把 `P1_lcd_rstp` 改为 `P1_o_lcd_rstn`（属顶层逻辑改动，违反"零改动"，需单独审批）。
- R5 风险描述同步更正为："DSI 物理引脚 `P1_lcd_rstp`（源码连接名）与 peri 端口 `P1_o_lcd_rstn` 不一致，本轮不修，阶段 1 观察 implicit net warning。"

### H1 风险更正：DSI 悬空不是无害 warning

已核实 `.peri.xml:960` 起 `mipi_tx_ck1`、`mipi_tx_dp10..13` 均为 `ops_type="tx"` 的真实 MIPI DPHY 输出端口，且 `MIPI_TX_PLL_LOCKED` 参与 `arst_n`（top.v:610）。原 R5"可忽略"判断错误。

**修订处理**：
- 阶段 1 不把 MIPI TX warning 预设为可忽略。综合后全量归档这类 warning 文本。
- 上板前由用户确认 TJ375N529 板 P1 MIPI/LCD 接口物理上确未外接任何负载、不与板载电路短接（这是硬件确认项，需用户回答）。
- 若上板出现功耗/发热/干扰异常，择期升级为方案丙'（保留 DSI 内部 `pixel_data_en`、隔离 TX OE 输出），该方案改 `top.v`，需单独 Codex Gate。

### H3 风险更正：IP `settings.json` 的版本与绝对路径

已核实：
- `ip/csi_rx_controller/settings.json` 的 `sw_version` = `2025.2.288.4.15`（与工程一致），`--base_path` 含本机绝对路径。
- `ip/dsi_tx/settings.json` 的 `sw_version` = `2025.2.288.3.8`（**低于工程版本**），`--base_path` 含 `D:\Project\...` 绝对路径。

**修订处理**：
- R4 由"只扫 `ipm/`"扩展为扫描 `ip_vendor/*/settings.json` 与 `ip_vendor/*/ipm/*` 的绝对路径和版本差异，复制后逐 IP 报告。
- DSI IP 版本低于工程版本是真实风险点，但**本轮不预先重生成 IP**（重生成要走 Efinity GUI、且会触发 Codex Gate）。先原样迁移，阶段 1 打开后观察 DSI IP 是否挂红。
- 若 DSI IP 挂红，优先策略是"在 Efinity 2025.2.288.4.15 中用同参数重新生成 IP"（用 `settings.json` 恢复参数），而非手改 `ipm/`/`pickle`。手改生成产物违反 CLAUDE.md 规则。

### 修订后的执行顺序（取代原文）

**前置扫描（执行前，只读）**：
0a. 扫 `src/` 全量 `` `include `` 与 `$readmem*` 引用，核对 B1 清单是否还有遗漏（本轮已核到 4 个，但不排除边角）。
0b. 扫 `ip/*/settings.json` 与 `ip/*/ipm/*` 的绝对路径、版本差异，生成 IP 风险表（H3）。
0c. 扫 `debug_profile.wizard.json` 是否引用 `src/` 路径（R3）。

**执行步骤**：

1. **复制工程文件**（A 节）：`mem_test.xml`、`mem_test.peri.xml`、`constrain.sdc`、`debug_profile.wizard.json` → `fpga/efinity/`。
2. **复制 IP**（B 节）：`ip/csi_rx_controller/`、`ip/dsi_tx/`（整份含 `ipm/`）→ `fpga/ip_vendor/`。
3. **复制 RTL**（C 节修正版，含 B1 修订的 4 个 include/.mem 依赖 + B2 路径修正）→ `fpga/rtl/` 对应子目录。
4. **改写 `mem_test.xml`**（D 节）：design_file `src/` → `rtl/`（按 C 节修正映射，含 `color_bar_checker.v`、`yuv_2rgb/yuv444_yuv422.v` 正确路径；`yuv444_yuv422.v` 目标含 `yuv_2rgb/` 层级）；ip_info `ip/` → `ip_vendor/`；include 参数同改。
5. **改写 `dsi_tx_top.v:144` 的 `INITIAL_CODE` 路径**：`"/src/mipi_dsi/Panel_1080p_reg.mem"` → 迁移后可解析的相对路径（候选 `"rtl/mipi_dsi/Panel_1080p_reg.mem"`，需确认 Efinity `$readmem*` 工作目录规则后定稿）。**此项触碰 `top.v` 以外 RTL，不改变功能逻辑，风险低，但属 Codex Gate 触发点，改写前在迁移日志记录**。
6. **确定 HDL include 搜索路径策略**：Efinity 工程需能解析 `` `include "Axi_Mux_Param.vh" `` 和 `` `include "timescale.v" `` 等。检查 `mem_test.xml` 是否有 include path 参数（如 `--include_path` 或 `search_path`）；若无，方案是：把 include 文件放在与引用文件同目录（`timescale.v`、`i2c_master_defines.v` 天然同目录于 `i2c_master/`，`Axi_Mux_Param.vh` 需与 `Axi_Mux.v` 同放 `rtl/axi/`）。若 Efinity include 搜索规则不支持"同目录"，再考虑加 include path 参数（属工程配置改动，记 Codex Gate）。
7. **改写 `debug_profile.wizard.json`**（若 0c 发现 src/ 引用）。
8. **更新 `final_project/.gitignore`**（E 节）：`fpga/ip_vendor/*/ipm/`、`fpga/efinity/outflow/`、`work_syn/`、`work_pnr/`、`work_dbg/`。
9. **追加 `final_project/docs/migration_log.md`** 本轮条目，记录 B1/B2/B3/H1/H3 处理方式与 0a/0b/0c 扫描结果。
10. **交付阶段 1**，由用户在 Efinity 2025.2.288.4.15 打开工程，按下方判据检查。

### 修订后的阶段 1 完成判据（扩展版）

阶段 1 由用户在 Efinity 打开工程后反馈以下结果，**全部满足**才算通过：

1. Efinity 打开工程 **无 missing file** 错误。
2. 所有 HDL include（`Axi_Mux_Param.vh`、`timescale.v`、`i2c_master_defines.v` 等）**无缺失**。
3. `$readmem*` 初始化文件（`Panel_1080p_reg.mem`）**解析成功**（DSI 初始化 ROM 非空）。
4. 两块 IP（`csi_rx_controller`、`dsi_tx`）**不挂红**；注意 DSI IP 生成版本 `2025.2.288.3.8` 低于工程版本，若挂红需按 H3 处理流程重新生成。
5. SDC 解析日志 **无 unmatched `get_ports`、无 ignored constraint、无 invalid reference pin**（H2 判据，取代原 R1 的"设备头注释"判据）。
6. Synthesis **通过，且 blackbox 为零**（无未解析模块）。
7. MIPI TX / DSI 相关 warning **全量文本归档**到 `docs/evidence/`，不预设可忽略（H1 判据）。
8. Implicit net warning（关于 `P1_lcd_rstp`）是否存在，明确记录；存在则升级为阻塞项走 B3 流程。

### 上板前硬件确认项（交付用户）

进入阶段 3 上板前，用户需确认：
- TJ375N529 板 P1 MIPI/LCD 接口物理上**确实未外接任何负载**，不与板载其他电路短接。（H1：DSI TX DPHY 会真实驱动这些引脚。）
- 摄像头、HDMI 接电脑就绪。

### 仍需后续单独处理（不在本轮）

- A1 长期架构：视频链路复位从 DSI `pixel_data_en` 解耦 → 独立 reset controller / video-ready 组合。下一轮 Mode A。
- A4 README 对齐：补短 README 说明 `video_in/`、`wb_gamma/` 等是决赛目标骨架，PR1 迁入的 demo 代码暂按源结构（`mipi_csi/`、`contract_bright/`）放置。
- uvc_src 清理：综合 + 上板通过后单独一轮，不与迁移耦合。

### 本轮执行前仍需用户确认的点

1. **H1 硬件确认**：TJ375N529 板 P1 MIPI/LCD 接口是否物理未接负载？（决定乙方案能否安全上板）
2. **步骤 5 的 `$readmem*` 路径策略**：是否接受改写 `dsi_tx_top.v:144` 的 `INITIAL_CODE` 字符串路径（触碰 RTL、不改逻辑）？还是要求我不改 RTL、而是通过 Efinity 工作目录/include path 配置解决？（前者风险低、直接；后者不动 RTL 但依赖 Efinity 配置，不确定能否覆盖）
3. **步骤 6 的 include 搜索路径**：若把 `Axi_Mux_Param.vh` 和引用它的 `Axi_Mux.v` 同放 `rtl/axi/`，且 `timescale.v`/`i2c_master_defines.v` 与引用文件同目录，是否就能被 Efinity 解析？还是需要在 `mem_test.xml` 加显式 include path（属工程配置改动，Codex Gate）？这点我倾向**先按"同目录放置 + 不加 include path"执行，阶段 1 打开后如有 include 缺失再处理**，请确认是否接受这个"先试后补"顺序。
