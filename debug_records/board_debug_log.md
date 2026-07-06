# Board Debug Log

## 2026-07-04 初始化 debug 记录目录

- 日期时间：2026-07-04
- 测试场景：上板测试准备
- 现象 / 报错：暂无，提前建立记录目录
- 影响范围：后续 FPGA 上板、视频链路、CPU 识别决策、myCobot 联调的问题追踪
- 相关文件 / 模块 / 信号：`debug_records/`
- 初步判断：需要统一保存报错、定位证据、修复内容和验证结果，避免问题只散落在聊天记录中
- 定位证据：用户要求在本地仓库创建文件夹用于 debug 记录
- 修复内容：创建 `debug_records/`，并添加 `README.md` 与本日志文件
- 验证命令 / 上板结果：已创建目录和记录模板，尚未开始上板验证
- 剩余风险 / 后续动作：后续每次报错或修复都追加到本目录中对应日志文件

## 2026-07-04 VERI-1206 true_dual_port_ram 重复定义（上板前综合）

- 日期时间：2026-07-04
- 测试场景：上板前 Efinity 综合（mem_test.xml 工程）
- 现象 / 报错：`rtl/debayer/true_dual_port_ram.v(144): overwriting previous definition of module 'true_dual_port_ram' [VERI-1206]`
- 影响范围：综合 warning，覆盖模块定义；潜在影响 debayer line buffer 与 mipi_dsi panel_config 两个例化点行为一致性
- 相关文件 / 模块 / 信号：
  - `final_project/fpga/rtl/debayer/true_dual_port_ram.v`（副本，已删）
  - `final_project/fpga/rtl/mipi_dsi/true_dual_port_ram.v`（副本，已删）
  - `final_project/fpga/rtl/common/true_dual_port_ram.v`（新增唯一定义）
  - `final_project/fpga/efinity/mem_test.xml`（工程清单，L28 改路径 + L43 删除）
  - 例化点 A：`rtl/mipi_dsi/panel_config.v:60` `inst_piv2_reg`（40bit/9addr/WRITE_FIRST）
  - 例化点 B：`rtl/debayer/line_buffer.v:88` `inst_y_buffer`（PW / X_CNT_WIDTH+1 / READ_FIRST）
  - IP testbench 副本：`final_project/fpga/ip_vendor/dsi_tx/Testbench/true_dual_port_ram.v`（不进综合，保持原样）
- 初步判断：`true_dual_port_ram` 是 Efinix DPRAM 宏，在工程里被复制成两份完全相同的源文件，两份都被 `mem_test.xml` 列入 default library 综合，第二个覆盖第一个触发 VERI-1206。两份 `diff` 完全一致，逻辑行为暂未分叉，但是工程隐患。
- 定位证据：
  - `grep -rln "module true_dual_port_ram" final_project/` 命中 4 处：debayer、mipi_dsi、common(本任务新增)、ip_vendor/dsi_tx/Testbench
  - `grep -n "true_dual_port_ram" mem_test.xml` → L28 与 L43 两条 `design_file` 同 library="default"
  - `diff debayer/true_dual_port_ram.v mip_dsi/true_dual_port_ram.v` → 完全一致
- 修复内容：single source of truth 统一到 `rtl/common/`：
  1. 新建 `rtl/common/true_dual_port_ram.v`（内容=原 debayer 副本）
  2. 删除 `rtl/debayer/true_dual_port_ram.v`
  3. 删除 `rtl/mipi_dsi/true_dual_port_ram.v`
  4. `mem_test.xml`：L28 `../rtl/mipi_dsi/true_dual_port_ram.v` → `../rtl/common/true_dual_port_ram.v`；L43 `../rtl/debayer/true_dual_port_ram.v` 删除
  5. 两个例化点 `panel_config.v` / `line_buffer.v` 不改（按模块名例化，default library 全局可见）
  6. `ip_vendor/dsi_tx/Testbench/` 副本保持原样（赛方原始文件只读，不进综合清单）
- 验证命令 / 上板结果：
  - `grep -rln "module true_dual_port_ram" final_project/` → 剩 `rtl/common/` + `ip_vendor/dsi_tx/Testbench/`（后者仅 testbench）
  - `grep -n "true_dual_port_ram" mem_test.xml` → 仅 L28 指向 common
  - Codex Review 已通过（GATE: PASS，见下"Codex 审查结果"）
  - **Efinity 2025.2.288.4.1 重新综合尚未执行，VERI-1206 是否消除待上板确认**
- Codex 审查结果（2026-07-04，codex-cli 0.142.5 / gpt-5.5 / high / 165,190 tokens）：
  - GATE: PASS（无 [P1] critical，1 条 [P2] advisory）
  - 确认 1：`rtl/common/` 优于保留 mip_dsi 副本；`mem_test.xml` 不对 common 做 OOC 特殊处理，等同普通 default-library 文件
  - 确认 2：`panel_config.v` / `line_buffer.v` 无需改；均按模块名例化，无 include / library 路径依赖
  - 确认 3：保留 `ip_vendor/dsi_tx/Testbench/true_dual_port_ram.v` 可接受；不在主综合清单
  - 确认 4：`mem_test.xml` 改动完整；`.peri.xml`、`debug_profile.wizard.json` 均未引用被删路径
  - [P2] 未来风险：`ip_vendor/dsi_tx/settings.json` 在 `external_testbench_testbench` 下仍列着 `dsi_tx\Testbench\true_dual_port_ram.v`，若将来把 DSI testbench 源拉进与工程 RTL 同一 default sim library，VERI-1206 会重现。当前 `<efx:sim_info/>` 为空、IP 项只含 `dsi_tx.sv`，不受影响
- 剩余风险 / 后续动作：
  1. Efinity 重新综合确认 VERI-1206 消失（待上板）
  2. 后续修改 DPRAM 宏只允许改 `rtl/common/true_dual_port_ram.v` 一份
  3. 若未来把 DSI IP testbench 引入主仿真库，需先处理 `ip_vendor/dsi_tx/Testbench/` 同名模块（Codex [P2]）
  4. `$HOME` 在本机 git bash 被改写为 `SPB_Data`，导致 codex / gstack 按 `~/.codex` 找认证失败；本会话用临时 `export HOME=/c/Users/20306` + `export CODEX_HOME=/c/Users/20306/.codex` 绕过，长期建议设 Windows 用户环境变量 `CODEX_HOME=C:\Users\20306\.codex`
  5. **工程存在两份 `final_project` 副本**：C 盘 `c:\Users\20306\Desktop\赛题资料\CICC\final_project\`（git 仓库）与 D 盘 `D:\final_project\`（Efinity 实际综合副本，非 git）。第一轮修复只改了 C 盘，D 盘综合仍报同一 VERI-1206。此后所有涉及 `final_project/` 的改动必须 **C/D 两处同步**。D 盘本轮已同步：`rtl/common/true_dual_port_ram.v` 已建（用 D 原副本 `cp`，与原文件字符级一致），`rtl/debayer/` 与 `rtl/mipi_dsi/` 两份副本已删，`mem_test.xml` L28→common、L43 删除
  6. 首次对 D 盘 `mem_test.xml` 用 Python 脚本写盘未真正落盘（写后 grep 仍为旧路径，疑似 cwd 被重置或 Efinity IDE 若打开会回写工程文件）；改用 Edit 工具按 old_string 精确替换后才真正落盘。**改 D 盘工程文件优先用 Edit 工具，写完用 grep 复核**
  7. 若 Efinity 重新综合仍报 `(144)` 同一行 VERI-1206，大概率是增量编译缓存了旧文件清单 —— 在 Efinity 里执行 Clean 后再综合

## 2026-07-04 HDMI 显示全屏循环色（红/蓝/黄/白/绿/粉 几秒一换），非摄像头图像

- 日期时间：2026-07-04
- 测试场景：上板烧 bit 后，HDMI 外接显示器
- 现象 / 报错：HDMI 全屏显示单色，几秒切到下一色；出现过红、蓝、黄、白、绿、粉，无明显规律；不是摄像头摄取图像
- 影响范围：HDMI 视频输出通路；摄像头 CSI 链路疑似未生效
- 相关文件 / 模块 / 信号：
  - `final_project/fpga/rtl/top/top.v` L1455-1499（`HDMI_OUT_EN` 宏内 hdmi_top_inst 例化）
  - `final_project/fpga/rtl/top/top.v` L1466-1476（HDMI 输入选取 always 块，本次改动落点）
  - `final_project/fpga/rtl/top/top.v` L1300-1311 `u0_white_balance`（S0 路白平衡，wb0_*）
  - `final_project/fpga/rtl/top/top.v` L1313-1324 `u1_white_balance`（S1 路白平衡，wb1_*）
  - `final_project/fpga/rtl/dvi_tx/hdmi_top.v` L102-121 `vid_info_det`（i_stable 检测）+ L124-151 内部 `color_bar_rgb(TEST_MODE=2'd1)` + L153-163 `dvi_encoder` 三元选择
  - `final_project/fpga/rtl/uvc_src/color_bar_v3.0/color_bar_rgb.v` L192-275 `TEST_MODE==2'b01`（Clor 分支：白黄青绿紫红蓝循环，每 64 帧 / 约 1 秒切一色）
  - `final_project/fpga/rtl/top/top.v` L1352-1378 `color_bar_rgb_inst`（dangling 副本，未接进 DSI/HDMI 数据通路，但被综合）
  - `final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v` L210 各路例化 `i2c_master_ctrl_top`（摄像头 I2C init）
  - `.peri.xml` S0_io_cam_* 在 GPIOT_P_08/N_07/N_08；S1_io_cam_* 在 GPIOT_N_05/P_05/P_07；J48 板载连接器到 S0/S1 物理映射未在仓库内找到
- 初步判断（错 → 修正）：起初怀疑是 top.v L1352 dangling `color_bar_rgb_inst(TEST_MODE=1)` 漏接进 DSI/HDMI，但读 top.v L1398-1401 确认 `dout` 只在注释 `//({16'd0,dout})` 出现，未真正接入。Codex 审查发现真根因 —— `hdmi_top` 模块内部自带 fallback color bar
- 定位证据：
  - `hdmi_top.v` L102-121：`vid_info_det_inst` 检测 `i_hs/i_vs/i_de` 是否稳定 → 输出 `frame_stable(i_stable)`
  - `hdmi_top.v` L124-151：内部例化 `color_bar_rgb #(.TEST_MODE(2'd1)) color_bar_rgb_inst` → 输出 `video_r/g/b/hs/vs/de`
  - `hdmi_top.v` L158-163：`dvi_encoder` 输入用 `i_stable ? i_* : video_*`。当 `i_stable=0`，HDMI 输出内部 color bar 的 6 色循环
  - `color_bar_rgb.v` L192-275 `TEST_MODE==2'b01` Clor 分支：`frame_cnt` 6-bit 计满（64 帧 ≈ 1 秒）按 `color_state` 循环 白→黄→青→绿→紫→红→蓝；与现象精确吻合
  - 触发链：摄像头只接一路 + HDMI 输出端取 wb1（第二路）→ 若摄像头实际在 S0，wb1 通路无有效帧 → `vid_info_det` 判 `i_stable=0` → `dvi_encoder` fallback 输出内部 color bar → 6 色循环
- 修复内容（双盲定位，最小验证改动）：
  - `final_project/fpga/rtl/top/top.v` L1468-1474：HDMI 输入选取 `wb1_*` → `wb0_*`（5 个信号引用：`wb1_vs_out/wb1_hs_out/wb1_de_out/wb1_data_out[47:24]/wb1_data_out[23:0]` → `wb0_*`），`sel` 乒乓逻辑与 `hdmi_top` 端口不动，DSI 通路（L1398-1401 仍接 wb1）不动
  - C 盘 git 仓库 `c:\...\CICC\final_project\fpga\rtl/top/top.v` 同步修改
  - dangling `color_bar_rgb_inst`(L1352) 本次不删（避免扩大变量混入定位 bit，留后续清理）
- Codex 审查结果（2026-07-04，codex-cli 0.142.5 / gpt-5.5 / high / 416,523 tokens）：
  - GATE: PASS（改动方案合理），但发现 Claude 漏掉的真根因
  - [P1] 真根因：`hdmi_top.v` L102-121/L124-151/L158-163 内部自带 `color_bar_rgb(TEST_MODE=1)` 作 fallback，当 `i_stable=0`（HS/VS/DE 不稳定或无有效帧）由 `dvi_encoder` 三元选择接管输出 → 6/7 色循环。修正了 Packet 候选根因 A（路由错接）与 B（sensor test pattern）的不足
  - [P2] wb1→wb0 只需改 top.v:1465-1471 的 5 个信号引用；`sel` 与 `hdmi_top` 端口无需改；`pixel_data_en` 来自 `dsi_tx_top` 的 `vid_rst_n` 面板配置延时，不依赖传入像素，故此次改动不会 break；两路 `debayer_top_2to1`/`white_balance` 结构对称、时钟/复位/48bit 双像素格式一致
  - [P2] 本次不改 DSI、不删 dangling `color_bar_rgb_inst`，避免扩大变量
  - 双盲定位建议：下一版 bit 可同时把 `hdmi_top` fallback 改成明显不同静态色或临时强制外部输入，让循环色来源更明确
- 验证命令 / 上板结果：
  - `grep -n "wb[01]_vs_out\|wb[01]_data_out" top.v` 确认改后 L1468-1474 全为 wb0
  - C/D 两处 top.v 已 grep 比对一致
  - **待用户重新综合 + 烧 bit + 上电**：
    - 若 HDMI 出图 → 摄像头在 S0，原 wb1 取错路由，fallback 已被正常输入取代（`i_stable=1`）
    - 若仍循环色 → wb0 也无有效帧，**不能**直接推断 sensor test pattern，只能说明 `i_stable` 仍为 0；下一步需查 S0 路 CSI/I2C init 是否成功（`soft_mipi_rx_top_inst` 例化与 `i2c_master_ctrl_top` 的 sensor 寄存器序列）
- 剩余风险 / 后续动作：
  1. 若改 wb0 后仍循环色，下一步排查方向：S0 路 I2C init 是否真的把 sensor 配通（可用 SignalTap/ila 抓 `i2c_master_ctrl_top` 的 scl/sda、ack 信号），以及 CSI RX 是否收到 LP→HS 切换与 byte/word clock
  2. `hdmi_top.v` 内部 fallback color bar 是赛方原始设计（`hdmi_top.v` 由 Ramsey Wang 2022 写），属于调试辅助；正式工程若不需要可在 `hdmi_top.v` 内把 `i_stable` 强制拉 1 或删 fallback 分支，避免无输入时误显为 color bar 造成误判。但属赛方原始文件只读，本次不动，留待后续讨论
  3. `top.v` L1352 dangling `color_bar_rgb_inst` 是工程残留，违反 CLAUDE.md "DEMO_MODE/debug 残留不应进正式工程"，应列入后续清理清单（与上一条 VERI-1206 一并清理时一起处理）
  4. J48 物理连接器到 S0/S1 的映射本次未在仓内找到，若需精确确认请查赛方开发板 schematic 或上板后看哪路 I2C 应答
  5. DSI 通路（L1398-1401 接 wb1）未改，若后续切回 DSI 面板需同步切 wb0 或确认 S1 路可用
