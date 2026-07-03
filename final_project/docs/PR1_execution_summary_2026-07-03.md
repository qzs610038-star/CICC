# PR1 执行总结 — 视频链路迁移到 final_project（方案乙）

- 日期：2026-07-03
- 模式：Mode A → Mode B 执行（Codex 复核通过后落地）
- 方案：乙（DSI IP 照迁、`top.v` 零改动、MIPI 引脚悬空）
- 状态：**阶段 1 文件迁移与路径改写完成 → 代码评审修复完成，待用户在 Efinity 2025.2.288.4.15 打开验证**
- 关联文档：[review_packets/PR1_VideoPipeline_Migration_Plan_2026-07-03.md](review_packets/PR1_VideoPipeline_Migration_Plan_2026-07-03.md)（含 Codex 复核意见 + PR1-v2 修订执行计划）
- 迁移日志：[migration_log.md](migration_log.md)

> 本文件是本轮**执行**的快速索引。详细清单和 Codex 审查全量见 review_packet。后续对话先读本文件可快速进入状态。

---

## 一句话目标

把赛方 demo `赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/` 的"摄像头→HDMI 显示"最小链路迁入 `final_project/fpga/`，使 Efinity 2025.2.288.4.15 能打开工程、综合、生成 bitstream，手动烧录到 TJ375N529 板后经 HDMI 在电脑显示器/采集卡上看到摄像头实时画面。本轮**不引入** OSD / feature_extract / ROI / CPU 决策 / 机械臂协议。

## 来源与目标

| 来源 | 目标 |
|---|---|
| `赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/` | `final_project/fpga/`（efinity/ip_vendor/rtl） |

- 顶层宏天然对应目标链路：`FRAME_BUFFER` + `HDMI_OUT_EN` 开、`UVC_EN` 关。
- 器件 Titanium TJ375N529 / I3，与工程版本一致。
- 赛方原件被 `.gitignore` 第 86 行隔离不进版本库，迁移靠复制内容（非软链接）。

## 本轮落地改动清单

### 1. 工程文件（fpga/efinity/）
- `mem_test.xml`（**已改写路径**：design_file `src/`→`rtl/`、ip/include `ip/`→`ip_vendor/`；新增 3 条 design_file；共 56 条 design_file）
- `mem_test.peri.xml`（原样复制，未改）
- `constrain.sdc`（原样复制，未改；SDC 设备头红旗见疑点 R1）
- `debug_profile.wizard.json`（原样复制，无 src 引用，0 改）

### 2. IP（fpga/ip_vendor/，整份含 ipm/）
- `csi_rx_controller/`（settings.json + .sv + define + tmpl + ipm/）
- `dsi_tx/`（同上 + Testbench/）— `ipm/` 进 `.gitignore`

### 3. RTL（fpga/rtl/，56 个 .v/.sv）
- 新建目录：`axi/`、`mipi_csi/`、`mipi_dsi/`、`contract_bright/`、`uvc_src/`（原骨架无这些名）
- 新增文件（非原 XML design_file 但必迁，属 B1 修订）：
  - `Axi_Mux_Param.vh`、`timescale.v`、`i2c_master_defines.v`（HDL include）
  - `Panel_1080p_reg.mem`（`$readmem*` 初始化）
  - `debayer/true_dual_port_ram.v`（被 line_buffer 例化）
- 还有 2 处文件名/路径修正（B2）：`color_bar_checker.v`（非 `color_barchecker.v`）、`uvc_src/yuv_2rgb/yuv444_yuv422.v`（含 yuv_2rgb 层级）。
- `top.v` **零改动**（乙方案核心遵守）。

### 4. RTL 改写
- **`rtl/mipi_dsi/dsi_tx_top.v:144`**：`INITIAL_CODE` 由 `"/src/mipi_dsi/Panel_1080p_reg.mem"` 改为 `"rtl/mipi_dsi/Panel_1080p_reg.mem"`（纯英文、无空格、无绝对前缀）。**本轮唯一一行 RTL 修改**，不改逻辑只改路径字符串。

### 5. 两处架构目录重映射（用户已拍板）
- 新建 `rtl/axi/`：容纳 `axi_interconnect/rtl/` 3 文件 + `axi_mux/Axi_Mux.v`。
- `vid_info_det_v7.v` 放 `rtl/framebuffer/`（决策 2）。

### 6. `.gitignore` 增量（final_project/.gitignore）
- `fpga/ip_vendor/*/ipm/`、`fpga/ip_vendor/*/Testbench/modelsim/`（IP 生成产物不入版本库）

### 7. 迁移日志
- `migration_log.md` 追加 2026-07-03 PR1 条目（含风险摘要）

### 8. 方案文档
- `review_packets/PR1_VideoPipeline_Migration_Plan_2026-07-03.md` 含原始方案 + Codex 复核意见 + PR1-v2 修订执行计划，作为本轮真源档案。

## 关键决策与依据（Why）

| 决策 | 选项 | 依据 |
|---|---|---|
| D1 输出链路 | 只 HDMI（`dvi_tx`），不接 DSI 屏 | 比赛现场用 HDMI 调试，MIPI 屏不在闭环 |
| D2 DSI 处理 | 乙方案（IP 照迁、引脚悬空、`top.v` 不动） | **`pixel_data_en`（DSI 输出）被 axi/debayer/HDMI/framebuffer 多处当 rst 用，删 DSI 会断 HDMI。无基线下零改动迁移风险最低。** Codex Gate 因动 `top.v` 外其他分析需走审查门，乙方案避开了它 |
| D3 IP 复制 | 整份含 `ipm/`，`ipm/` 进 .gitignore | Efinity MCP 未就绪，重新 Generate IP 走 GUI 风险高；即开即综合 + 不污染版本库 |
| D4 AXI 目录 | 新建 `rtl/axi/`（vs 并入 framebuffer） | AXI 是 framebuffer 与将来 cpu_if 共享资源，独立边界清晰 |
| D5 时序检测位置 | `vid_info_det_v7.v` 放 framebuffer | 与 `frame_info_det.v` 同属帧时序，紧耦合帧缓存 |
| D6 目录命名 | 镜像源结构（`mipi_csi/`、`contract_bright/`）而非对齐原 README（`video_in/`、`wb_gamma/`） | 最小改写、最低迁移风险；README 对齐留后续单独一轮 |
| D7 `$readmem*` 路径策略 | 改 `dsi_tx_top.v:144` 字符串路径（解法 A） | Codex 已认可，风险低；比靠 Efinity 工作目录配置确定性高 |
| D8 include 搜索路径 | 同目录放置优先（策略 X'） | 大概率零改动；失败再加 `mem_test.xml` include dir 字段（**不**写 Synthesis Options，按官方规范） |
| D9 工程路径 | 开发期内部路径全英文无空格；烧录前用户挪 final_project 到无中文路径 | Efinity 路径不能含中文/空格/特殊字符（官方约束）；相对路径不变则挪迁后仍工作 |

## 路径问题插曲（执行期发现，记录供后续避坑）

执行期出现过一个**错位复制**：最早一批命令用 `cd` 到 demo 目录后以 `../final_project/fpga` 相对路径复制，因 bash 每次新调用 cwd 重置到仓库根，相对路径实际解析到 `赛方提供材料/final_project/fpga/`（demo 的"上两级"误指）。中途通过 `mv` 把 `efinity/`、`ip_vendor/` 从错位目录迁移到正确位置，并删除错位残留 `赛方提供材料/final_project/`。

**教训**：本仓库路径含中文，bash 变量赋值传输中文路径时曾出现编码乱码；后续脚本一律用**相对仓库根的字面量路径**（如 `"赛方提供材料/..."`、`"final_project/..."`），避免 `cd`+相对路径组合。

## 疑点与开放风险（Open Items，阶段 1 验证重点）

### R1 SDC 设备/时序模型头注释不匹配（红旗，非阻塞）
`constrain.sdc` 头注释写 `Device: Ti180J484 / I4 (final)`，但工程是 `TJ375N529 / I3`。Codex H2 判断：头部注释可暂按历史残留，**真判据改为迁移后 SDC 解析有无 unmatched `get_ports` / ignored constraint / invalid reference pin**。阶段 1 Efinity 打开需查 SDC 解析日志。

### R2/H3 DSI IP 版本低于工程版本
`dsi_tx/settings.json` 的 `sw_version` = `2025.2.288.3.8`，工程版本 `2025.2.288.4.15`。`csi_rx_controller` 版本一致。两 IP 都含本机绝对路径 `--base_path`（可移植性风险）。**本轮不预先重生成 IP**。阶段 1 若 DSI IP 挂红，按 H3 策略用 `settings.json` 同参数在 Efinity 2025.2.288.4.15 中重新生成，**不手改 ipm/pickle**（违 CLAUDE.md）。

### R3/H1 MIPI TX DPHY 实际驱动悬空引脚（原 R5 判断错误）
`.peri.xml:960` 起 `mipi_tx_ck1`、`mipi_tx_dp10..13` 均为 `ops_type="tx"` 真实 DPHY 输出；`MIPI_TX_PLL_LOCKED` 参与 `arst_n`（top.v:610）。**R5 不能写"可忽略 warning"**。H1 处理：阶段 1 全量归档 MIPI TX warning；上板前用户已确认 P1 MIPI/LCD 接口物理未接负载（你已确认 DSI 不接负载）；若功耗/干扰异常则升级方案丙'（保留 DSI 内部、隔离 TX OE，需改 top.v，单走 Codex Gate）。

### R4/B3 `P1_lcd_rstp` vs `P1_o_lcd_rstn` 隐式 net
顶层端口 `P1_o_lcd_rstn`（top.v:194），DSI 例化连 `P1_lcd_rstp`（top.v:1405），靠 Verilog 隐式 net 通过。**本轮不修 `top.v`**。阶段 1 观察 synthesis log：无 implicit net warning 则原工程方式保留；有 warning 则升级为阻塞项，单独 Codex Gate 讨论是否改名（属顶层逻辑改动，违反"零改动"）。

### R5 include 搜索路径（策略 X' 待验证）
`Axi_Mux_Param.vh` 与 `Axi_Mux.v` 同 `rtl/axi/`；`timescale.v`、`i2c_master_defines.v` 与引用者同 `i2c_master/` 目录。期待"同目录查找"被 Efinity 支持。**若阶段 1 报 include 缺失，补救是在 `mem_test.xml` include dir 字段加路径（不写 Synthesis Options/Dynamic Parameter，按官方规范）**。

### R6 README 目录命名与实际不符
现有 `rtl/video_in/`、`rtl/wb_gamma/`、`rtl/raw_unpack/` 等骨架描述的功能，本轮迁入文件在 `rtl/mipi_csi/`、`rtl/contract_bright/`，团队读代码易混淆。**本轮不对齐**，留后续单独一轮补 README 说明。

### R7 长期架构（A1，缺码块留作后续轮）
视频链路复位从 DSI `pixel_data_en` 解耦 → 独立 reset controller / video-ready 组合。**视频链路点亮后下一轮 Mode A 做**，不在本轮。

## 验证状态

| 项 | 状态 |
|---|---|
| 文件复制 | ✅ 完成（efinity 4 文件 + IP 2 块 + RTL 56 文件） |
| XML 路径改写 | ✅ 完成（design_file 56:56 与磁盘对齐，无残留 src/ 或 ip/） |
| `INITIAL_CODE` 改写 | ✅ 完成（纯英文相对路径） |
| `.gitignore` 增量 | ✅ 完成 |
| migration_log | ✅ 追加 |
| 硬件就绪 | ✅ CSI J48/J49 接摄像头、DSI 不接负载（H1 前提满足） |
| Efinity 打开验证 | ⏳ **待用户操作**（阶段 1 判据 8 条） |
| 综合 / 布局布线 | ⏳ 阶段 2，待阶段 1 通过 |
| 上板烧录验证 | ⏳ 阶段 3，待阶段 2 通过 |

## 阶段 1 Efinity 验证判据（请用户按序核对）

1. 打开工程无 missing file
2. HDL include（`Axi_Mux_Param.vh`、`timescale.v`、`i2c_master_defines.v`）无缺失
3. `$readmem*` 初始化文件（`Panel_1080p_reg.mem`）解析成功
4. 两块 IP 不挂红（注意 dsi_tx 版本差异，H3）
5. SDC 无 unmatched get_ports / ignored constraint / invalid reference pin（H2 真判据）
6. Synthesis 通过且 blackbox 为零
7. MIPI TX / DSI warning **全量截图发我**（H1，不预设可忽略）
8. `P1_lcd_rstp` implicit net warning 是否存在（B3，有则升级阻塞）

## 下一步（待用户反馈）

收到阶段 1 验证结果后：
- 全通过 → 进入阶段 2（综合 + 布局布线，关注时序闭合、PLL/DPHY 配置、资源利用率）
- 有 warning/报错 → 按对应风险项处理（DSI 挂红走 H3 重生成；include 缺失走 R5 加 include dir；SDC unmatched 走 H2 重导出 SDC；implicit net 走 R4 论是否改 top.v）
- 上板成功 → 进入决赛侧轮（OSD / feature_extract / cpu_if / 机械臂），那是独立 Mode A 节

## 给后续对话的快速进入指引

1. 先读本文件 + review_packet 末尾的 PR1-v2 修订执行计划 + Codex 复核意见。
2. 用户反馈 Efinity 打开结果后，对照"阶段 1 判据 8 条"逐条记录通过/失败。
3. 任何改动 `top.v`、约束、IP 的提议属 Codex Gate，先出 Codex Review Packet 再执行。
4. 路径相关脚本一律用相对仓库根字面量，避免中文路径在变量传输中乱码。

---

## 代码评审修复记录（2026-07-03, /code-review max effort）

### 评审范围与方式
对 PR1 迁移所有修改（mem_test.xml 路径改写、dsi_tx_top.v:144 INITIAL_CODE、.gitignore 增量、RTL/工程文件复制、migration_log、两份新文档）运行了 7 角度 × 最大努力的全召回代码评审。评审发现 9 项确认问题（7 correctness + 3 simplification），其中 2 项严重（阻塞阶段 1 Efinity 打开）。

### 已修复的阻塞问题

| # | 文件:行 | 问题 | 修复 |
|---|---|---|---|
| 1 | `mem_test.xml:10`（全部56条）| **基准面路径错误**：`mem_test.xml` 在 `fpga/efinity/`，但 design_file 写为 `rtl/...`（解析为 `efinity/rtl/...` 不存在），实际位置在兄弟目录 `fpga/rtl/`。Efinity 打开全部 missing file。 | sed 加 `../` 前缀：`rtl/` → `../rtl/`、`ip_vendor/` → `../ip_vendor/`（56条 design_file + 2条 ip_info + 2条 include）|
| 2 | `dsi_tx_top.v:144` | **`$readmemh` 路径基准面错误**：`INITIAL_CODE` 写为 `"rtl/mipi_dsi/Panel_1080p_reg.mem"`，但 `$readmemh` 在 `$true_dual_port_ram.v:52` 中相对于 synthesis work_dir（`efinity/work_syn/`）解析，不存在于 `work_syn/rtl/...`。DSI 初始化 ROM 空→`pixel_data_en` 不拉高→HDMI 无画面。 | 改为 `"../rtl/mipi_dsi/Panel_1080p_reg.mem"`（对齐 `../rtl/` 约定；阶段 1 仍需确认 Efinity `$readmemh` CWD 是否确为 `work_syn/`，若解析为 `efinity/` 根则 `../rtl/` 正确） |

### 已知但未修复的风险（记录为观察项，待阶段 1 Efinity 验证）

| # | 文件 | 问题 | 建议处理 |
|---|---|---|---|
| R-IP1 | `ip_vendor/dsi_tx/settings.json:6` | `--base_path` = `D:\Project\...`（原开发者机器绝对路径，已失效）；`sw_version` = `2025.2.288.3.8` 低于工程 `2025.2.288.4.15` | 阶段 1 若 DSI IP 挂红：用 settings.json 同参数在 Efinity 2025.2.288.4.15 重生成（不手改 ipm/pickle）；不挂红则暂沿用（ipm 二进制快照有效） |
| R-IP2 | `ip_vendor/csi_rx_controller/settings.json:6` | `--base_path` = `D:\Computer\FPGA_Prj\...`（另一台机器路径，已失效）；版本与工程一致 | 同 R-IP1 策略 |
| R-DUP | `rtl/debayer/true_dual_port_ram.v` vs `rtl/mipi_dsi/true_dual_port_ram.v` | 两个完全相同的144行模块文件（debayer 用空 init、mipi_dsi 用 .mem 初始化）。将来一方修改另一方未同步会静默分歧。 | 低优先级；视频链路点亮后合并到 `rtl/common/` 或将 debayer 版本并入 mipi_dsi |
| R-README | `rtl/wb_gamma/README.md` 等骨架 | 骨架 README 描述的功能与实际代码位置不一致（wb_gamma→contract_bright、video_in→mipi_csi），新成员读架构文档会被误导 | 低优先级；补一个 `rtl/MIGRATION_NOTE.md` 说明迁移代码暂按源结构放置、决赛目标分层参见各 README |
| R-MEM-DEFAULT | `rtl/debayer/true_dual_port_ram.v:22` | 默认 `RAM_INIT_FILE = "ram_init_file.mem"`（该文件不存在），任何忘记传空字符串的例化都会导致 `$readmemh` 失败 | 当前安全（line_buffer 传空），但留下技术债务；建议改默认值为空字符串 |

### 非阻塞的简化/对齐建议

- `.gitignore` 重复：Efinity 输出模式（`outflow/`、`work_syn/` 等）在根 `.gitignore` 和 `final_project/.gitignore` 中各有一份（约16行重叠）。下一步合并到 final_project/.gitignore 用 `**/` 前缀统一覆盖，根 `.gitignore` 只保留非 final_project 项。
- `migration_log.md` PR1 行的风险列：三个不相关的风险（DSI 版本、DPHY 悬空、隐式 net）堆在一个单元格，后续排查不易扫描。建议拆成三条子行或 bullet。

### 评审结论
PR1 迁移清单和 Codex 修订执行计划整体方向正确。基准面路径错误是**迁移的结构性失误**——把 XML 从源项目根迁移到子目录时，忘记所有设计文件路径需要加 `../` 前缀。**已修复**。`$readmemh` 路径同样基准面错也**已修复**。IP `--base_path` 绝对路径属于源材料的已知可移植性债务（Codex H3），本轮不修等待阶段 1 Efinity 结果。其余发现为低优先级简化/维护性建议，不阻塞阶段 1。