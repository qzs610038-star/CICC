# HDMI 横纹调试上下文 — Phase 2

> 日期: 2026-07-07 (更新)
> 分支: dev/libaoxun688
> 目标: 摄像头画面通过 FPGA → HDMI → 电脑正常显示
> 上板状态: HDMI 有画面但有横纹，LED24 + LED27 持续亮（读 FIFO 下溢 + CDC 桥下溢）

---

## 1. 板上 Debug LED 定义（关键排障工具）

### 1.1 板卡 LED 物理映射（2026-07-07 用户确认）

| LED 编号 | 信号名称 | 管脚号 | 所属 Bank |
|----------|----------|--------|-----------|
| LED11 | USER_LED_2 | T4 | BANKBL3 |
| LED12 | USER_LED_3 | C13 | BANKTR1 |
| LED13 | USER_LED_6 | C12 | BANKTR1 |
| LED16 | USER_LED_10 | D13 | BANKTR2 |
| LED17 | USER_LED_11 | E13 | BANKTR2 |
| LED18 | USER_LED_12 | B2 | BANK4C |
| LED19 | USER_LED_13 | E3 | BANK4C |
| LED20 | USER_LED_14 | F3 | BANK4C |
| LED21 | USER_LED_15 | F2 | BANK4C |
| LED22 | USER_LED_16 | G2 | BANK4C |
| LED23 | USER_LED_17 | K6 | BANK4C |
| LED24 | USER_LED_18 | J3 | BANK4C |
| LED25 | USER_LED_19 | L6 | BANK4C |
| LED26 | USER_LED_20 | K4 | BANK4C |
| LED27 | USER_LED_21 | K3 | BANK4C |
| LED28 | USER_LED_22 | M5 | BANK4C |
| LED29 | USER_LED_23 | M6 | BANK4C |
| LED30 | USER_LED_24 | N7 | BANK4D |
| LED31 | USER_LED_25 | P7 | BANK4D |
| LED32 | USER_LED_26 | P6 | BANK4D |
| LED33 | USER_LED_27 | R6 | BANK4D |

Efinity GPIO 资源名以 outflow `Ti375C529_devkit.pinout.rpt` 核对：LED18=B2=`GPIOB_N_30_CDI8`，LED19=E3=`GPIOB_P_24_CDI15`，LED20=F3=`GPIOB_N_24_CDI14`，LED21=F2=`GPIOB_P_17_CLK4_P`，LED22=G2=`GPIOB_N_17_CLK4_N`，LED23=K6=`GPIOB_P_25_CDI13`，LED24=J3=`GPIOB_N_28`，LED25=L6=`GPIOB_N_25_CDI12`，LED26=K4=`GPIOB_N_29_CDI10`，LED27=K3=`GPIOB_P_29_CDI11_EXTFB`，LED28=M5=`GPIOB_P_22`，LED29=M6=`GPIOB_N_22`。

### 1.2 Phase 2 Debug LED 映射

| LED | 管脚 | 信号 | 含义 | 1= | 0= |
|-----|------|------|------|----|----|
| LED18 | B2 | led[0] | DDR 配置完成 | OK | BAD |
| LED19 | E3 | led[1] | 当前选择通道 framebuffer ready | OK | BAD |
| **LED20** | **F3** | **led[2]** | **当前选择通道 framebuffer 读 FIFO 下溢** | **BAD** | OK |
| LED21 | F2 | led[3] | 当前选择通道 CSI RAW10/4ppc 确认 | OK | BAD |
| LED22 | G2 | dbg_ddr_ok | 当前选择通道 framebuffer 读周期 active | OK | BAD |
| LED23 | K6 | dbg_fb0_ready | 当前选择通道 DDR 读数据曾到达 | OK | BAD |
| **LED24** | **J3** | **dbg_fb0_underflow** | **当前选择通道 DDR 读数据 gap 过长（锁存）** | **BAD** | OK |
| LED25 | L6 | dbg_csi_fmt_ok | 当前选择通道 CDC FIFO 达到启动水位 | OK | BAD |
| LED26 | K4 | dbg_bridge_active | 当前选择通道 CDC 2pix→1pix 桥输出 active | OK | BAD |
| **LED27** | **K3** | **dbg_bridge_under** | **当前选择通道 CDC 桥 FIFO 下溢（锁存）** | **BAD** | OK |
| LED28 | M5 | dbg_video_ready | i_video_ready → hdmi_top | OK | BAD |
| LED29 | M6 | dbg_input_stable | hdmi_top 接受输入时序且 CSI 格式 OK | OK | BAD |

代码位置: `final_project/fpga/rtl/top/top.v` 端口声明约 L205-214，驱动逻辑约 L1814-L1900；内部探针来自 `frame_buffer.v` 和 `video_2pix_to_1pix_cdc.v`。

**LED 排障原则**:
- LED20 = 1 表示当前选择通道 framebuffer 内部的读 FIFO（`data_tx` 模块中的 `DC_FIFO`）在 `fifo_rd_empty & fifo_rd_en` 时发生了下溢
- LED24 = 1 表示当前选择通道 AXI/DDR 读数据出现超过 1024 个 AXI 时钟周期的无 `ddr_rd_valid` gap
- LED27 = 1 表示 CDC 桥（`video_2pix_to_1pix_cdc`）在输出端空了，即写入速度跟不上读出速度
- 两个下溢信号都是锁存的——只要发生过就会保持亮，按 SW4 切换通道时清零
- 如果按 SW4 清零后立即重新亮起 → 对应错误是持续性的（每帧都在发生）
- 如果按 SW4 清零后灭掉一段时间再亮起 → 对应错误是间歇性的

---

## 2. 已排除的假设

### 假设 A: DDR 读带宽不足 ❌ 已排除

**原假设**: 两个 framebuffer 同时竞争 DDR 读写，实际可用读带宽不够 1920×1080 @60fps。

**验证手段**:
- Patch 1: 关闭 ch1 framebuffer 的 CSI 输入（`.i_de = 1'b0`, `.vin = 32'd0`），只保留 ch0 独立使用 DDR
- Patch 2: 将 framebuffer 输出水平有效像素从 960 dual-pixel（对应 1920 single-pixel）降到 480 dual-pixel（对应 960 single-pixel），DDR 读带宽需求减半
- Patch 2b: `hdmi_top.v` 的 `MAX_HRES` 从 1920 同步改为 960

**验证结果（2026-07-07 上板）**:
- LED22-29 **全部亮起**，与改之前完全一致
- LED24 (fb0_underflow) = 亮（持续）
- LED27 (bridge_underflow) = 亮（持续）
- HDMI 画面横纹依旧存在
- 即使 DDR 只服务一路 960×1080 输出，下溢仍然持续

**结论**: DDR 读带宽**不是**根因。问题在别处。

### 假设 B: ch1 竞争 DDR 写入带宽 ❌ 已排除

Patch 1 已把 ch1 的 CSI 写输入全部清零，ch1 不写 DDR。下溢依旧。

---

## 3. 未被排除的假设（下一步排查方向）

### 假设 C: 时钟域不匹配 / CDC FIFO 深度不足

**现象证据**: LED27（CDC 桥下溢）持续性亮。CDC 桥是 `video_2pix_to_1pix_cdc`，写侧时钟 `i_sysclk_div2`（约束 14.286ns ≈ 70MHz），读侧时钟 `hdmi_tx_slow_clk`（约束 7.143ns ≈ 140MHz）。

**可疑点**:
- 70MHz 写侧以 2 pixels/clock 产生数据（对应 140M pixels/s）
- 140MHz 读侧以 1 pixel/clock 消耗数据（对应 140M pixels/s）
- 带宽理论上匹配，但实际像素吞吐可能因为消隐期占比不同而导致写/读速率不均
- `START_LEVEL = 16` 可能太浅，无法吸收突发抖动

**验证方法**:

**Step 1: 增大 CDC FIFO 深度和启动水位**
- 修改 `video_2pix_to_1pix_cdc` 例化参数: `FIFO_DEPTH` 从 1024 改为 4096，`START_LEVEL` 从 16 改为 256
- 位置: `top.v` L1695-1698 (ch0) 和 L1714-1717 (ch1)
- 编译烧录 → 观察 LED27 是否熄灭

**Step 2: 在 CDC 桥内部增加 debug 探针**
- 把 `fifo_wr_usedw` / `fifo_rd_usedw` 引出到 spare LED，观察 FIFO 水位是否持续走低
- 如果写侧 always 满（wr_usedw 接近 1024）→ 读不够快
- 如果读侧 always 空 → 写不够快

### 假设 D: Framebuffer 输出时序参数与实际不匹配

**现象证据**: LED24（framebuffer 读 FIFO 下溢）持续性亮。framebuffer 内部 `data_tx` 模块从 `par2ser_parse → DC_FIFO → 时序生成器` 的路径中，如果 `H_VALID` 参数与实际像素数据速率不匹配，会导致 FIFO 读空。

**可疑点**:
- `frame_buffer.v` 中 `fifo_rd_period` 信号控制 DDR 读端的使能。如果这个信号的占空比计算有误（如认为消隐期比实际长），DDR 读端会停太久，导致读 FIFO 被掏空
- `data_tx` 模块输出的时序参数（h_front_porch / h_sync / h_valid 等）是从 `frame_buffer` 的输入端口锁存后传递的，确认这些值在 dual-pixel 域下是否正确

**验证方法**:

**Step 1: 确认 `fifo_rd_period` 的实际行为**
- `fifo_rd_period` 由 `data_tx.v` L424 产生: `fifo_rd_period <= ~(v_state == S_V_SYNC && v_cnt == 0);`
- 即除了 VSYNC 的第 0 行之外都是 1，表示 DDR 读在绝大部分时间都在工作
- 这逻辑看起来正确，但需要确认 `v_state` 机是否真的在正常跳转
- **建议添加 LED**: 取 `fifo_rd_period` 的反（即 VSYNC 期间短暂亮一下），如果这个 LED 常亮 → v_state 卡在 S_V_SYNC

**Step 2: 检查 h_valid / v_valid 参数寄存器传递**
- `frame_buffer.v` L293-303: `h_valid` 等参数在 `o_clk` 域用 `always @(posedge o_clk)` 锁存
- 但 `H_VALID` 输入来自 `top.v` 的 `HDMI_H_VALID` localparam，是常量
- 确认 `i_clk` 和 `o_clk` 是否同源（两者都是 `i_sysclk_div2`）→ 是
- 确认 `i_sysclk_div2` 实际频率是否为 70MHz → 需要 scope 确认

### 假设 E: AXI 读通道仲裁或响应延迟导致 DDR 读数据断流

**现象证据**: 即使 DDR 带宽足够，如果 AXI 读请求的响应延迟过大，DDR 读数据到达 `ddr_rd_buffer` 的间隔会拉长，导致下游 FIFO 被掏空。

**可疑点**:
- `ddr_rd_buffer.v` L259: `assign rready = 1'b1` — 读通道来者不拒，但这不代表 AXI 端的 `arvalid→rvalid` 延迟低
- 如果 DDR controller 的读延迟（CAS latency + controller pipeline）导致相邻 burst 之间出现 gap，下游 FIFO 可能来不及补水

**验证方法**:

**Step 1: 用 LED 探测 DDR 读有效信号间隔**
- 在 `ddr_rd_buffer` 中取 `ddr_rd_valid`（即 `rvalid & rready`），驱动一个计数器
- 如果 `ddr_rd_valid` 在 burst 之间有长时间（>100 cycles @ 200MHz = >500ns）的 gap → AXI 读延迟过大
- 简化版: 取 `~ddr_rd_valid` 驱动 LED，如果 LED 明显亮（不止是短暂闪烁）→ 读数据断流

**Step 2: 确认 axi0_ACLK 频率**
- 约束文件: `create_clock -period 5.000 -name axi0_ACLK` → 200MHz
- `i_fb_clk`: `create_clock -period 40.000` → 25MHz（DDR PHY 参考时钟？需确认）
- 如果 `axi0_ACLK` 实际不是 200MHz → DDR 带宽显著下降

### 假设 F: CSI 输入端的帧率/分辨率与 framebuffer 期望不匹配

**可疑点**: framebuffer 的 `MAX_VID_WIDTH = 1920`, `MAX_VID_HIGHT = 1080`，但 CSI 实际进来的可能是不同分辨率。如果 `frame_stable` 不满足某条件导致写端不工作，但读端仍在跑 → 读空。

**验证方法**:
- LED23 (fb0_ready) = 亮说明 `frame_ready = frame_en & out_sync & ~fifo_rd_underflow_latched` 的第 1 项 `frame_en` 为 1
- `frame_en = rd_frame_en_r[1] & rd_frame_available`，其中 `rd_frame_en_r[1]` 来自 `frame_stable`
- 但 `frame_ready` 亮了不代表写端在正常工作

---

## 4. 推荐的排障执行顺序

按由简到繁、由外到内的原则:

### 第 1 步: 增加 CDC FIFO 深度（假设 C，低风险，1 次编译）

修改 `top.v` 中两处 `video_2pix_to_1pix_cdc` 的例化:
```verilog
// L1695-1698 和 L1714-1717
video_2pix_to_1pix_cdc #(
    .FIFO_DEPTH(4096),     // 原来是 1024
    .START_LEVEL(256)      // 原来是 16
) u_hdmi0_video_cdc (...)
```

**预期**: 如果 LED27 熄灭 → CDC FIFO 深度不够是根因；否则继续。

### 第 2 步: 用 LED 探查 framebuffer 内部关键信号（假设 D/E，1-2 次编译）

**2a: 暴露 `fifo_rd_period` 反信号到 LED**
- 在 `top.v` 中把 `frame_buffer` 输出的某个未用信号（或新增 port）接到 LED
- `fifo_rd_period` = 0 只在 VSYNC 期间，如果 LED 常亮 → v_state 卡死

**2b: 暴露 DDR 读有效信号到 LED**
- 把 `ddr_rd_valid`（或 `~ddr_rd_valid`）直接输出到 LED
- 如果 LED 明显不是微弱闪烁而是持续亮 → DDR 读断流严重

**2c: 用 scope 实测时钟频率**
- 量 `i_sysclk_div2` 实际频率（期望 70MHz）
- 量 `hdmi_tx_slow_clk` 实际频率（期望 140MHz）
- 量 `axi0_ACLK` 实际频率（期望 200MHz）
- 如果实际频率与约束不符 → PLL 配置有问题

### 第 3 步: 绕过 DDR 做最小环路测试（假设全部，中风险，1-2 次编译）

临时在 `top.v` 中新增一个 color bar 发生器（已在代码中被注释掉，如 L1128-1155 的 `color_bar_rgb`），直接产生 1920×1080 的彩条 → debayer bypass → CDC 桥 → HDMI。

**预期**:
- 如果彩条正常无横纹 → 问题在 DDR/framebuffer 读写链路
- 如果彩条也有横纹 → 问题在 CDC 桥或 HDMI TX 时钟域

---

## 5. 修改涉及的文件清单

| 文件 | 路径 |
|------|------|
| 顶层 | `final_project/fpga/rtl/top/top.v` |
| CDC 桥 | `final_project/fpga/rtl/dvi_tx/video_2pix_to_1pix_cdc.v` |
| HDMI TOP | `final_project/fpga/rtl/dvi_tx/hdmi_top.v` |
| Framebuffer | `final_project/fpga/rtl/framebuffer/frame_buffer.v` |
| DDR 读缓冲 | `final_project/fpga/rtl/framebuffer/ddr_rd_buffer.v` |
| 读端时序生成 | `final_project/fpga/rtl/framebuffer/data_tx.v` |
| 约束 | `final_project/fpga/efinity/constrain.sdc` |
| 管脚映射 | `final_project/fpga/efinity/mem_test.peri.xml` |

**每次修改 C 盘后务必同步到 D 盘**:
```powershell
Copy-Item "C:\...\top.v" "D:\final_project\fpga\rtl\top\top.v" -Force
```

---

## 6. 当前代码修改状态（2026-07-07）

以下修改**已在代码中生效**，不要回退除非确认不需要:

1. `top.v` L629: `HDMI_H_VALID = 13'd480`（已从 960 减半）
2. `top.v` L1190-1191: ch1 framebuffer `.i_de = 1'b0`, `.vin = 32'd0`
3. `hdmi_top.v` L50: `MAX_HRES = 12'd960`（已从 1920 减半）
4. `top.v` L206-216 + L1801-1868: 8 路 debug LED 驱动逻辑

**如果要做第 3 步（color bar 测试），建议先回退 Patch 1+2 的 H_VALID 和 MAX_HRES 修改，恢复到 1920×1080 输出**。

---

## 7. 时钟域速查

```
i_sysclk_div2     ~70MHz     CSI 像素域 / framebuffer 读写共用
hdmi_tx_slow_clk  ~140MHz    HDMI TX 像素域 / CDC 桥读侧
hdmi_tx_fast_clk  ~700MHz    HDMI TX 串行域 (5x)
axi0_ACLK         ~200MHz    AXI / DDR 控制器域
i_fb_clk           ~25MHz    DDR PHY 参考时钟
```

CDC 桥从 70MHz (2pix/clk = 140Mpix/s) → 140MHz (1pix/clk = 140Mpix/s)，理论带宽刚好匹配，但 **消隐期占比在两侧可能不同**（dual-pixel 域和 single-pixel 域的 H 消隐参数换算可能不一致），导致平均写入速率 < 平均读出速率。

---

## 8. 2026-07-07 本轮执行记录（Codex）

### 修改目标

按 Phase 2 推荐顺序执行“增大 CDC FIFO 深度”，并同步加入可上板判读的 LED 探针，避免只凭 HDMI 横纹现象猜测。

### 已修改

1. `top.v`
   - `u_hdmi0_video_cdc` / `u_hdmi1_video_cdc`: `FIFO_DEPTH 1024 -> 4096`，`START_LEVEL 16 -> 256`。
   - LED18-29 重新作为 HDMI 横纹排障灯组，覆盖 DDR ready、framebuffer ready、framebuffer underflow、CSI 格式、framebuffer 读周期、DDR 读数据到达、DDR 长 gap、CDC FIFO 水位、CDC active、CDC underflow、HDMI video ready、HDMI input stable。
2. `frame_buffer.v`
   - 新增只读 debug 输出：`dbg_fifo_rd_period`、`dbg_ddr_rd_seen`、`dbg_ddr_read_gap`。
   - `dbg_ddr_read_gap` 表示读周期内连续 1024 个 AXI 时钟周期未见 `ddr_rd_valid`，用于粗略定位 AXI/DDR 读数据断流。
3. `video_2pix_to_1pix_cdc.v`
   - 新增只读 debug 输出：`o_level_ready`、`o_level_low`。
   - 本轮 LED 使用 `o_level_ready`，表示 CDC FIFO 已达到 `START_LEVEL`。
4. `mem_test.peri.xml`
   - 显式绑定 LED22-29 的 `dbg_*` 顶层端口到 BANK4C 管脚，避免依赖手工 Interface Designer 状态。

### 上板判读重点

- 若 LED18 亮、LED19 亮、LED20 亮：framebuffer 已 ready 但读端仍下溢，优先看 LED22/23/24。
- 若 LED22 常灭：`data_tx` 读周期没有进入正常 active，怀疑读端时序状态机或 frame_en/out_sync。
- 若 LED23 灭：读周期内从未收到 DDR 读数据，优先查 AXI 读请求/DDR 响应。
- 若 LED23 亮但 LED24 亮：DDR 数据能到，但存在长 gap，优先查 AXI 仲裁/读延迟/burst 连续性。
- 若 LED25 灭或 LED27 亮：CDC FIFO 没有维持足够水位，优先查 70MHz 写侧平均吞吐、消隐换算或 CDC 读写策略。
- 若 LED28 灭：`hdmi_top` 仍未接收到 video ready，通常由 framebuffer/CDC 错误门控导致。
- 若 LED29 灭但 LED28 亮：HDMI 输入时序/尺寸检测或 CSI 格式条件未满足。

### 待验证

- `efx_map` 前端综合已通过：
  - 命令：`D:\Efinity\2025.2\bin\efx_map.exe --project-xml mem_test.xml --root top --family Titanium --device TJ375N529 --work-dir work_syn_codex_hdmi_led_probe_v1 --output-dir work_syn_codex_hdmi_led_probe_v1`
  - 目录：`D:\final_project\fpga\efinity`
  - 结果：exit code 0；综合日志中确认 `video_2pix_to_1pix_cdc(FIFO_DEPTH=4096,START_LEVEL=256)`。
  - `EFX.err.log` 无新增错误；`EFX.warn.log` 仍有既有未连接端口/位宽类 warning，本轮搜索未发现 `dbg_*` LED 端口绑定相关 warning。
- 仍需要完整 Efinity P&R/bitstream 生成并烧录，上板记录 LED18-29 状态。
- 若布局布线出现新的 I/O 绑定、Bank 电平或 timing warning，必须先记录 warning 原文再判断是否继续烧录。

---

## 参考文件

- 开发路线: `分赛区决赛实施开发路线.md`
- 管线现状: `final_project/docs/architecture/dual_camera_hdmi_pipeline_current_2026-07-06.md`
- Fix 计划: `final_project/docs/architecture/dual_camera_hdmi_fix_plan_2026-07-06.md`
- 顶层 RTL: `final_project/fpga/rtl/top/top.v`
- CDC 桥: `final_project/fpga/rtl/dvi_tx/video_2pix_to_1pix_cdc.v`
- HDMI TOP: `final_project/fpga/rtl/dvi_tx/hdmi_top.v`
- Framebuffer: `final_project/fpga/rtl/framebuffer/frame_buffer.v`
- DDR 读缓冲: `final_project/fpga/rtl/framebuffer/ddr_rd_buffer.v`
- 读端时序生成: `final_project/fpga/rtl/framebuffer/data_tx.v`
- 管脚映射: `final_project/fpga/efinity/mem_test.peri.xml`
- 约束文件: `final_project/fpga/efinity/constrain.sdc`

---

## 9. 2026-07-07 board feedback and next probe set (Codex)

### Board feedback from user

- LEDs ON: LED18, LED25, LED26, LED30, LED31, LED32, LED33.
- All other LEDs OFF.
- Efinity timing WNS: -1.462 ns. Treat this as a real timing violation until the next manual Efinity build proves otherwise.

### Interpretation of previous LED map

- LED18 ON means DDR configured.
- LED25/LED26 ON is not enough to prove valid camera video reached HDMI, because the current CDC bridge writes one word every `wr_clk` cycle, including blank/invalid words.
- LED19/LED21/LED28/LED29 OFF points to CSI format/framebuffer readiness not established.
- LED30-33 were not explicitly driven in the previous debug map, so their previous ON state is not diagnostic.

### WNS finding and constraint action

The old timing report `D:\final_project\fpga\efinity\outflow\mem_test.timing.rpt` showed WNS -1.462 ns on paths from `i_sysclk_div2` to `mipi_rx_ck1_CLKOUT`, for example `soft_mipi_rx_top_inst1/reset_pixel_n` into CSI RX byte2pixel / async FIFO reset synchronizers.

Added this SDC constraint after the MIPI RX clock definitions in `final_project/fpga/efinity/constrain.sdc`:

```tcl
set_clock_groups -asynchronous -group {i_sysclk_div2} -group {mipi_rx_ck0_CLKOUT} -group {mipi_rx_ck1_CLKOUT}
```

User will run Efinity manually from this point. Manual checks:

- Confirm whether WNS -1.462 ns disappears or moves to a different real synchronous path.
- Confirm the new `set_clock_groups` line has no SDC parse warning.
- Confirm LED30-33 are bound to N7/P7/P6/R6.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 CSI VS seen |
| LED20 | F3 | `led[2]` | ch0 CSI DE seen |
| LED21 | F2 | `led[3]` | ch0 CSI datatype RAW10 seen |
| LED22 | G2 | `dbg_ddr_ok` | ch0 CSI `pixel_per_clk == 4` seen |
| LED23 | K6 | `dbg_fb0_ready` | ch0 CSI RAW10/4ppc seen |
| LED24 | J3 | `dbg_fb0_underflow` | selected framebuffer `frame_stable` |
| LED25 | L6 | `dbg_csi_fmt_ok` | selected framebuffer `wr_frame_done` |
| LED26 | K4 | `dbg_bridge_active` | selected framebuffer `rd_frame_available` |
| LED27 | K3 | `dbg_bridge_under` | selected framebuffer ready |
| LED28 | M5 | `dbg_video_ready` | selected framebuffer underflow, 1=BAD |
| LED29 | M6 | `dbg_input_stable` | HDMI `i_video_ready` |
| LED30 | N7 | `dbg_led30` | ch1 CSI VS seen |
| LED31 | P7 | `dbg_led31` | ch1 CSI DE seen |
| LED32 | P6 | `dbg_led32` | ch1 CSI RAW10/4ppc seen |
| LED33 | R6 | `dbg_led33` | selected DDR read data seen |

### Files changed in this step

- `final_project/fpga/efinity/constrain.sdc`: add async clock grouping for `i_sysclk_div2` vs MIPI RX CLKOUT clocks.
- `final_project/fpga/rtl/framebuffer/frame_buffer.v`: expose `dbg_frame_stable`, `dbg_wr_frame_done`, `dbg_rd_frame_available`.
- `final_project/fpga/rtl/top/top.v`: add CSI seen latches and remap LED18-33 to CSI/framebuffer probes.
- `final_project/fpga/efinity/mem_test.peri.xml`: bind LED30-33 top-level ports.

Synced these files to `D:\final_project` for manual Efinity build/flash.

---

## 10. 2026-07-07 second board feedback and timing constraint update (Codex)

### Board feedback from user

- LEDs ON: LED18, LED19, LED20, LED21, LED22, LED23, LED24, LED26, LED27, LED29, LED33.
- LED25 is flashing.
- All other LEDs OFF.
- Efinity timing WNS: -0.834 ns.

### Interpretation

- LED18 ON: DDR configuration reached done.
- LED19/20/21/22/23 ON: ch0 CSI is active and has seen VS, DE, RAW10 datatype, and `pixel_per_clk == 4`.
- LED24/26/27 ON and LED25 flashing: selected framebuffer is stable, write-frame-done is toggling, read-frame-available is asserted, and selected framebuffer ready is asserted.
- LED28 OFF: selected framebuffer underflow flag is not asserted.
- LED29 ON: HDMI `i_video_ready` is asserted.
- LED33 ON: selected DDR read data has been seen.
- LED30/31/32 OFF: ch1 CSI is not active in this test, but ch0 is already sufficient for the next HDMI data-correctness checks.

Conclusion: this board state proves the main ch0 CSI -> framebuffer -> DDR read -> HDMI-ready chain is alive. If horizontal stripes remain, the next video-debug target should move from "no data / no ready" to data correctness: RAW10 unpack/truncation, Bayer phase, debayer input ordering, framebuffer word ordering, and the temporary 480/960 HDMI width-reduction patch.

### Timing report finding

Read-only scan of `D:\final_project\fpga\efinity\outflow\mem_test.timing.rpt`:

- Worst setup relationship: `i_fb_clk -> hdmi_tx_slow_clk`, slack -0.834 ns, constraint 0.001 ns.
- Worst paths start at `cfg_st[1]~FF|CLK` and end at HDMI-domain async reset pins such as `sw4_sync[0]~FF|SR`, `rgb_de_r~FF|SR`, `rgb_datax1[*]~FF|SR`, `selected_frame_ready_cdc[0]~FF|SR`, and `selected_fifo_underflow_cdc[0]~FF|SR`.
- Code check: `cfg_st` is generated in the `i_fb_clk` DDR configuration FSM; `sys_rst_n = ddr_cfg_ok`; several HDMI-domain always blocks use `or negedge sys_rst_n`.
- Secondary failing relationship: `i_sysclk_div2 -> mipi_clk`, slack -0.742 ns, also reset/control paths from `soft_mipi_rx_top_inst1/reset_pixel_n` to MIPI I2C reset counters.

### Constraint action

Added these constraints to `final_project/fpga/efinity/constrain.sdc`:

```tcl
set_clock_groups -asynchronous -group {i_fb_clk} -group {hdmi_tx_slow_clk}
set_clock_groups -asynchronous -group {i_sysclk_div2} -group {mipi_clk}
```

These constraints are intended to remove false setup analysis across asynchronous reset/control crossings. They do not prove the reset implementation is ideal; they only prevent Efinity from treating unrelated clock phases as a 0.001/0.002 ns synchronous requirement.

### Manual Efinity checks for next run

- Confirm the two new `set_clock_groups` lines produce no SDC parse warning.
- Confirm WNS no longer reports `i_fb_clk -> hdmi_tx_slow_clk` through `cfg_st[1]` to HDMI reset pins.
- Confirm WNS no longer reports `i_sysclk_div2 -> mipi_clk` through `reset_pixel_n` to MIPI reset counters.
- If WNS moves to a real synchronous data path, record the new launch/capture clocks, first three path begin/end points, and slack.
- If the displayed image still has horizontal stripes with the same LED state, continue with pixel data correctness probes rather than more readiness LEDs.

### Files changed in this step

- `final_project/fpga/efinity/constrain.sdc`: add async clock grouping for `i_fb_clk` vs `hdmi_tx_slow_clk`, and `i_sysclk_div2` vs `mipi_clk`.

Synced `constrain.sdc` and this log to `D:\final_project` for the next manual Efinity build/flash.

---

## 11. 2026-07-07 third board feedback and front-end LED probe set (Codex)

### Board feedback from user

- LEDs ON: LED18 only.
- All other LEDs OFF.
- Efinity timing WNS: +1.406 ns; WHS: +0.011 ns.
- Timing summary image shows positive setup slack after the async clock constraints.
- HDMI image is broad colored horizontal bands with dense vertical/noisy pixel stripes, consistent with fallback/test-pattern style output rather than a valid camera frame.

### Interpretation

The timing issue is no longer the primary blocker in this run. Only LED18 ON means DDR configuration is complete, but the current LED map saw no ch0 CSI VS/DE/datatype/4ppc/framebuffer/HDMI-ready state. Combined with the visible fallback-like HDMI image, this run should be treated as a front-end startup failure: the design is outputting HDMI, but camera/CSI data is not reaching the previous CSI/framebuffer checkpoints.

Because the previous LED map was mostly downstream readiness, the next probe set moves LEDs earlier in the chain: reset release, camera I2C activity, MIPI lane enable/FIFO activity, and CSI packet metadata.

### RTL changes

`final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v`

- Added read-only debug outputs:
  - `dbg_reset_pixel_n`
  - `dbg_i2c_rst_n`
- These expose existing internal reset states and do not change MIPI/CSI datapath behavior.

`final_project/fpga/rtl/top/top.v`

- Connected the new ch0/ch1 debug reset outputs from `soft_mipi_rx_top`.
- Added latched front-end probes for:
  - ch0 I2C SCL/SDA edge seen
  - ch0 MIPI clock-lane HS enable seen
  - ch0 MIPI data-lane HS enable seen
  - ch0 MIPI lane FIFO ever non-empty
  - ch0 MIPI lane FIFO read enable seen
  - ch0 CSI datatype ever non-zero
  - ch0 CSI pixel-per-clock ever non-zero

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 CSI RX pixel reset released |
| LED20 | F3 | `led[2]` | video pipeline reset delay released (`pixel_data_en`) |
| LED21 | F2 | `led[3]` | ch0 camera I2C reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C SCL has toggled |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C SDA has toggled |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 MIPI clock-lane HS enable seen |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 MIPI data-lane HS enable seen |
| LED26 | K4 | `dbg_bridge_active` | ch0 MIPI lane FIFO ever non-empty |
| LED27 | K3 | `dbg_bridge_under` | ch0 MIPI lane FIFO read enable seen |
| LED28 | M5 | `dbg_video_ready` | ch0 CSI datatype ever non-zero |
| LED29 | M6 | `dbg_input_stable` | ch0 CSI pixel-per-clock ever non-zero |
| LED30 | N7 | `dbg_led30` | ch0 CSI VS seen |
| LED31 | P7 | `dbg_led31` | ch0 CSI DE seen |
| LED32 | P6 | `dbg_led32` | ch0 CSI RAW10/4ppc seen |
| LED33 | R6 | `dbg_led33` | HDMI video ready |

### Manual checks for next run

- If only LED18 is still ON: suspect ch0 CSI RX pixel reset is not releasing; check PLL/reset chain and `reset_pixel_n` source.
- If LED19 ON but LED21 OFF: CSI pixel reset releases, but camera I2C reset counter is not completing.
- If LED21 ON but LED22/23 OFF: camera I2C controller may not be toggling the bus or pins are not connected as expected.
- If LED22/23 ON but LED24/25 OFF: camera I2C activity exists, but MIPI HS receive is not entering high-speed mode.
- If LED24/25 ON but LED26/27 OFF: MIPI lanes are enabled but lane FIFO is empty or not being read.
- If LED26/27 ON but LED28/29/30/31/32 OFF: low-level MIPI data exists, but CSI packet decode/metadata is not valid.

### Verification performed by Codex

- Confirmed `mem_test.xml` includes `../rtl/mipi_csi/soft_mipi_rx_top.v`.
- Confirmed both `soft_mipi_rx_top` instances in `top.v` connect `dbg_reset_pixel_n` and `dbg_i2c_rst_n`.
- Ran `git diff --check` on `top.v` and `soft_mipi_rx_top.v`; no whitespace/error output.

Synced `top.v`, `soft_mipi_rx_top.v`, and this log to `D:\final_project` for the next manual Efinity build/flash.

---

## 12. 2026-07-07 fourth board feedback and HDMI-ready gate probe set (Codex)

### Board feedback from user

- LEDs ON: LED18, LED19, LED20, LED21, LED22, LED23, LED24, LED25, LED26, LED27, LED28, LED29, LED30, LED31, LED32.
- LED33 OFF.
- Efinity timing WNS: +1.797 ns; WHS: +0.008 ns.
- HDMI image is still the same fallback-style broad colored horizontal bands with dense vertical/noisy stripes.

### Interpretation of previous front-end probe map

The previous probe set proves ch0 front-end is now alive:

- DDR configured.
- ch0 CSI RX pixel reset released.
- `pixel_data_en` released.
- ch0 camera I2C reset released.
- ch0 I2C SCL/SDA toggled.
- ch0 MIPI clock/data lane HS enable seen.
- ch0 MIPI lane FIFO non-empty and read enable seen.
- ch0 CSI datatype and pixel-per-clock are non-zero.
- ch0 CSI VS/DE and RAW10/4ppc were seen.

Only LED33 OFF means `hdmi_video_ready` did not assert in the previous map. Therefore the next target is not camera/I2C/MIPI bring-up; it is the HDMI-ready gate:

```verilog
hdmi_video_ready_sync <= channel_sel_toggle ? 2'b00 :
    {hdmi_video_ready_sync[0],
     selected_frame_ok_hdmi & selected_bridge_active & ~selected_bridge_underflow};
```

There is one especially important suspicion: the previous probe map was ch0-specific, but HDMI output uses `selected_*`. If `channel_sel` has toggled to ch1, the selected path may be ch1 even though ch0 front-end is good. In the current debug code, ch1 framebuffer input is intentionally disabled (`.i_de = 1'b0`, `.vin = 32'd0`) from an earlier bandwidth-reduction step, so selecting ch1 can keep HDMI in fallback.

### RTL change

`final_project/fpga/rtl/top/top.v`

- Replaced the front-end probe LED map with an HDMI-ready gate probe map.
- No data-path behavior changed; only LED assignments changed.
- LED19 now directly reports `channel_sel`:
  - LED19 OFF = ch0 selected
  - LED19 ON = ch1 selected

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | `channel_sel`: OFF=ch0 selected, ON=ch1 selected |
| LED20 | F3 | `led[2]` | selected framebuffer ready synchronized to HDMI |
| LED21 | F2 | `led[3]` | selected framebuffer underflow synchronized, 1=BAD |
| LED22 | G2 | `dbg_ddr_ok` | selected `frame_ok_hdmi` |
| LED23 | K6 | `dbg_fb0_ready` | selected CDC bridge active |
| LED24 | J3 | `dbg_fb0_underflow` | selected CDC bridge underflow, 1=BAD |
| LED25 | L6 | `dbg_csi_fmt_ok` | selected CDC FIFO reached start level |
| LED26 | K4 | `dbg_bridge_active` | selected CDC FIFO level low, 1=BAD |
| LED27 | K3 | `dbg_bridge_under` | raw `hdmi_video_ready` gate into `hdmi_top` |
| LED28 | M5 | `dbg_video_ready` | selected DDR read data seen |
| LED29 | M6 | `dbg_input_stable` | selected DDR read gap seen, 1=BAD |
| LED30 | N7 | `dbg_led30` | selected framebuffer frame stable |
| LED31 | P7 | `dbg_led31` | selected framebuffer `wr_frame_done` |
| LED32 | P6 | `dbg_led32` | selected framebuffer `rd_frame_available` |
| LED33 | R6 | `dbg_led33` | `hdmi_top` accepted input timing (`hdmi_input_stable`) |

### Manual checks for next run

- If LED19 is ON, press SW4 once or reset/reflash so ch0 is selected; ch1 is not a valid video source in this current debug build.
- If LED20/22/23/25/27 are ON and LED21/24/26/29 are OFF, but LED33 is still OFF, then the blocker is inside `hdmi_top` input timing/size/stability detection.
- If LED27 is OFF, read LED20/21/22/23/24 to identify which gate term blocks `hdmi_video_ready`.
- If LED24 or LED26 is ON, CDC bridge underflow/low-water is still the blocker.
- If LED29 is ON, DDR read data has long gaps even if some data arrives.

Synced `top.v` and this log to `D:\final_project` for the next manual Efinity build/flash.

---

## 13. 2026-07-07 fifth board feedback and ch0 framebuffer probe set (Codex)

### Board feedback from user

- LEDs ON: LED18, LED23, LED25.
- All other LEDs OFF.
- HDMI image: stable broad horizontal color bands with dense vertical/noisy stripes. This is still fallback/no-valid-frame style HDMI output, not a recognizable live camera frame.

### Interpretation of previous HDMI-ready gate probe map

The previous LED map was:

- LED18 = DDR configured.
- LED19 = `channel_sel`, OFF means ch0 selected.
- LED20/22/27 = selected framebuffer/video-ready gates.
- LED23/25 = selected HDMI CDC bridge active / FIFO start level reached.
- LED28/30/31/32 = selected framebuffer DDR read / frame stable / write done / read frame available.

The reported LED18/23/25 ON only means:

- DDR configuration completed.
- ch0 is selected because LED19 is OFF.
- The HDMI CDC bridge has activity and reached its start level.
- But selected framebuffer ready, frame stable, write completion, read availability, DDR read data, and `hdmi_video_ready` are all OFF.

This moves the blocker upstream of the HDMI-ready gate and inside the ch0 framebuffer path. The previous front-end probe already proved ch0 CSI reset/I2C/MIPI/RAW10/4ppc/VS/DE are alive, so the next question is whether `frame_buffer.v` sees a stable frame and starts DDR write/read.

### RTL changes

`final_project/fpga/rtl/framebuffer/frame_buffer.v`

- Added read-only debug outputs. These are latched "seen" indicators for short pulses:
  - `dbg_frame_start_seen`
  - `dbg_wr_fifo_wren_seen`
  - `dbg_wr_start_seen`
  - `dbg_awvalid_seen`
  - `dbg_wr_frame_done_seen`
  - `dbg_rd_start_seen`
  - `dbg_arvalid_seen`
- Introduced a named internal wire:
  - `wr_start = frame_start & frame_stable`
- No data path behavior was intentionally changed; this step only exposes internal milestones.

`final_project/fpga/rtl/top/top.v`

- Connected the new `frame_buffer` debug outputs for both ch0 and ch1 instances.
- Replaced the LED map with a ch0-specific framebuffer probe set.
- LED indicators are latched in the HDMI clock domain where useful, so short one-cycle events remain visible after they happen.
- This map intentionally focuses on ch0, because ch1 framebuffer input remains disabled in this debug build and LED19 from the previous run already showed ch0 is selected.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`); should be ON |
| LED20 | F3 | `led[2]` | ch0 CSI VS seen |
| LED21 | F2 | `led[3]` | ch0 CSI DE seen |
| LED22 | G2 | `dbg_ddr_ok` | ch0 CSI RAW10/4ppc format seen |
| LED23 | K6 | `dbg_fb0_ready` | ch0 `frame_buffer` `frame_start` seen |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 `frame_buffer` `frame_stable` |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 write FIFO write-enable seen |
| LED26 | K4 | `dbg_bridge_active` | ch0 `wr_start` or AXI `AWVALID` seen |
| LED27 | K3 | `dbg_bridge_under` | ch0 `wr_frame_done` seen |
| LED28 | M5 | `dbg_video_ready` | ch0 `rd_frame_available` |
| LED29 | M6 | `dbg_input_stable` | ch0 `rd_start` seen |
| LED30 | N7 | `dbg_led30` | ch0 AXI `ARVALID` seen |
| LED31 | P7 | `dbg_led31` | ch0 DDR read data seen |
| LED32 | P6 | `dbg_led32` | ch0 framebuffer `frame_ready` |
| LED33 | R6 | `dbg_led33` | raw HDMI video-ready gate |

### Manual checks for next run

- Expected minimum if CSI is still alive: LED18, LED19, LED20, LED21, LED22 should be ON.
- If LED20/21/22 are OFF, the CSI front-end regressed and this is no longer a framebuffer-only problem.
- If LED20/21/22 are ON but LED23 is OFF, `vid_rx_align_v1` is not producing `frame_start`; suspect VS polarity/frame boundary detection.
- If LED23 is ON but LED24 is OFF, frame length/timing is not stable enough for `frame_stable`; inspect `vid_rx_align_v1` / frame size detection assumptions.
- If LED24 is ON but LED25 is OFF, stable-frame detection is present but pixel write packing into the framebuffer write FIFO is not happening.
- If LED25 is ON but LED26 is OFF, write FIFO accepts data but DDR write does not start; focus on `wr_start = frame_start & frame_stable` timing and `ddr_buffer` write start acceptance.
- If LED26 is ON but LED27 is OFF, AXI write starts but the framebuffer never completes a frame write.
- If LED27 is ON but LED28/29/30/31 are OFF, write completes but read-side start/AXI read is blocked.
- If LED31 is ON but LED32/33 are OFF, DDR read data returns, then the blocker is read FIFO/data_tx/frame_ready/HDMI-ready gating.

### Verification performed by Codex

- Ran `tools/agent_handoff_health_check.ps1`; it passed with only the unrelated `pymycobot` missing warning.
- Ran `git diff --check` on `top.v` and `frame_buffer.v`; no whitespace/error output.
- Confirmed both `frame_buffer` instances in `top.v` connect the new debug ports.

Sync target for this step: `top.v`, `frame_buffer.v`, and this log need to be copied to `D:\final_project` before the next manual Efinity build/flash.

---

## 14. 2026-07-07 sixth board feedback and hdmi_top/input-content probe set (Codex)

### Board feedback from user

- LEDs ON: LED18 through LED33 all ON.
- HDMI image still looks the same as before: broad horizontal color bands with dense vertical/noisy stripes.

### Interpretation of previous ch0 framebuffer probe map

All LEDs ON in the previous map proves the earlier "no frame" hypothesis is no longer the main blocker:

- DDR configured.
- ch0 selected.
- ch0 CSI VS/DE and RAW10/4ppc format seen.
- ch0 framebuffer `frame_start` and `frame_stable` reached.
- ch0 write FIFO accepted data.
- DDR write start / AXI write valid happened.
- `wr_frame_done` happened.
- read frame became available.
- DDR read start / AXI read valid happened.
- DDR read data was seen.
- ch0 `frame_ready` asserted.
- raw `hdmi_video_ready` gate asserted.

However, the previous LED33 was only `hdmi_video_ready`, not `hdmi_top`'s internal `use_input_video` / `o_input_stable`. Therefore the unchanged screen can still be either:

1. `hdmi_top` is still using fallback color bars because its internal `vid_info_det` stable/size/error gates do not qualify the input timing.
2. `hdmi_top` is already using input video, but the live pixel stream is wrong, for example RAW/Bayer order, pixel packing, RGB channel order, or framebuffer data ordering.

The next probe set distinguishes those two cases directly.

### RTL changes

`final_project/fpga/rtl/dvi_tx/hdmi_top.v`

- Added read-only debug outputs for the internal input-selection gates:
  - `o_video_path_ready`
  - `o_use_input_video`
  - `o_vidinfo_stable`
  - `o_timing_size_ok`
  - `o_h_active_error`
  - `o_v_active_error`
  - `o_v_total_error`
  - `o_h_total_error`
  - `o_h_sync_error`
- Added latched input-content probes:
  - `o_input_de_seen`
  - `o_input_vs_seen`
  - `o_input_hs_seen`
  - `o_input_data_nonzero_seen`
  - `o_input_data_change_seen`
- No HDMI encoder selection behavior was intentionally changed.

`final_project/fpga/rtl/top/top.v`

- Connected the new `hdmi_top` debug outputs.
- Replaced the LED map with an `hdmi_top` / input-content probe set.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`); should be ON |
| LED20 | F3 | `led[2]` | raw `hdmi_video_ready` gate |
| LED21 | F2 | `led[3]` | `hdmi_top` `video_path_ready` |
| LED22 | G2 | `dbg_ddr_ok` | `hdmi_top` input DE seen |
| LED23 | K6 | `dbg_fb0_ready` | `hdmi_top` `vid_info_det` frame stable |
| LED24 | J3 | `dbg_fb0_underflow` | `hdmi_top` timing size exactly 960x1080 |
| LED25 | L6 | `dbg_csi_fmt_ok` | `hdmi_top` `o_input_stable` |
| LED26 | K4 | `dbg_bridge_active` | `hdmi_top` input VS seen |
| LED27 | K3 | `dbg_bridge_under` | `hdmi_top` input HS seen |
| LED28 | M5 | `dbg_video_ready` | input pixel data nonzero seen |
| LED29 | M6 | `dbg_input_stable` | input pixel data changed over time |
| LED30 | N7 | `dbg_led30` | `h_active_error`, 1=BAD |
| LED31 | P7 | `dbg_led31` | `v_active_error` or `v_total_error`, 1=BAD |
| LED32 | P6 | `dbg_led32` | `h_total_error` or `h_sync_error`, 1=BAD |
| LED33 | R6 | `dbg_led33` | `hdmi_top` `use_input_video`; ON means HDMI encoder should be using live input, OFF means fallback remains active |

### Manual checks for next run

- If LED20/21 are ON but LED33 is OFF, HDMI is still fallback. Use LED23/24/30/31/32 to identify which `hdmi_top` stable gate is blocking.
- If LED23 is OFF, `vid_info_det` in `hdmi_top` does not see stable input timing.
- If LED24 is OFF while LED23 is ON, the input timing size is not exactly the `hdmi_top` expected 960x1080; compare with the current top-level `HDMI_H_VALID=480` and the 2-pixel-to-1-pixel bridge behavior.
- If any of LED30/31/32 is ON, the corresponding timing error is active and can keep fallback enabled.
- If LED33 is ON and the picture still looks like the same color bands/noise, HDMI has switched to input video; next focus is pixel data correctness: RAW10-to-RAW8 truncation, Bayer phase, `debayer_top_2to1` input packing, RGB byte ordering, or DDR frame ordering.
- If LED33 is ON but LED28/29 are OFF, HDMI uses input timing but pixel data is zero/static.

### Verification performed by Codex

- Ran `git diff --check` on `hdmi_top.v` and `top.v`; no whitespace/error output.
- Confirmed `hdmi_top` exposes the new debug outputs and the top-level instance connects them.

Sync target for this step: `top.v`, `hdmi_top.v`, and this log need to be copied to `D:\final_project` before the next manual Efinity build/flash.

---

## 15. 2026-07-07 seventh board feedback and selected ready-gate latch probe set (Codex)

### Board feedback from user

- LEDs ON: LED18 and LED19 only.
- All other LEDs OFF.
- HDMI image still looks the same as before.

### Interpretation of previous hdmi_top/input-content probe map

In the previous map:

- LED18 ON: DDR configured.
- LED19 ON: ch0 selected.
- LED20 OFF: raw `hdmi_video_ready` is not asserted.
- LED21 OFF and later LEDs OFF: `hdmi_top` never reaches its internal video path checks because `i_video_ready` is already low.

Therefore this run does not yet answer whether `hdmi_top` would accept input video. The immediate blocker is before `hdmi_top`:

```verilog
hdmi_video_ready_sync <= channel_sel_toggle ? 2'b00 :
    {hdmi_video_ready_sync[0],
     selected_frame_ok_hdmi & selected_bridge_active & ~selected_bridge_underflow};
```

The next probe must decompose those selected ready-gate terms and latch them, because some events can be short pulses.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Replaced the `hdmi_top` internal probe LED map with a selected ready-gate latch probe.
- Added HDMI-clock-domain latched indicators for:
  - selected framebuffer ready
  - selected framebuffer underflow
  - selected `frame_ok_hdmi`
  - selected bridge active
  - selected bridge underflow
  - selected bridge FIFO level ready
  - selected bridge FIFO low-water
  - raw `hdmi_video_ready`
  - selected framebuffer `frame_stable`
  - selected framebuffer `wr_frame_done`
  - selected framebuffer `rd_frame_available`
  - selected DDR read data seen
  - selected DDR read gap seen
- `hdmi_top.v` debug outputs from the previous step are kept connected, but LEDs are now assigned to the ready-gate latch map.
- No video data path behavior was intentionally changed.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`); should be ON |
| LED20 | F3 | `led[2]` | selected framebuffer ready seen in HDMI domain |
| LED21 | F2 | `led[3]` | selected framebuffer underflow seen, 1=BAD |
| LED22 | G2 | `dbg_ddr_ok` | selected `frame_ok_hdmi` seen |
| LED23 | K6 | `dbg_fb0_ready` | selected CDC bridge active seen |
| LED24 | J3 | `dbg_fb0_underflow` | selected CDC bridge underflow seen, 1=BAD |
| LED25 | L6 | `dbg_csi_fmt_ok` | selected CDC FIFO start level reached seen |
| LED26 | K4 | `dbg_bridge_active` | selected CDC FIFO low-water seen, 1=BAD |
| LED27 | K3 | `dbg_bridge_under` | raw `hdmi_video_ready` seen |
| LED28 | M5 | `dbg_video_ready` | selected framebuffer `frame_stable` seen |
| LED29 | M6 | `dbg_input_stable` | selected framebuffer `wr_frame_done` seen |
| LED30 | N7 | `dbg_led30` | selected framebuffer `rd_frame_available` seen |
| LED31 | P7 | `dbg_led31` | selected DDR read data seen |
| LED32 | P6 | `dbg_led32` | selected DDR read gap seen, 1=BAD |
| LED33 | R6 | `dbg_led33` | selected CSI RAW10/4ppc format |

### Manual checks for next run

- If LED20 is OFF but LED28/29/30/31 are ON, framebuffer internals work but `frame_ready = frame_en & out_sync & ~fifo_rd_underflow_latched` is blocked; inspect `out_sync`, `frame_en`, and read FIFO underflow latch.
- If LED20 is ON and LED22 is OFF, selected framebuffer underflow LED21 is likely blocking `frame_ok_hdmi`.
- If LED22 is ON but LED23 is OFF, the CDC bridge is not active even though framebuffer is ready.
- If LED23 is ON and LED24 is ON, bridge underflow blocks `hdmi_video_ready`.
- If LED23 is ON, LED24 is OFF, and LED27 is still OFF, check whether the gate terms are being asserted in different cycles rather than overlapping; the next fix would be to latch or restructure the ready gate, not to change CSI.
- If LED27 is ON, then the next return to `hdmi_top` internal probes is valid.

### Verification performed by Codex

- Ran `git diff --check` on `top.v`; no whitespace/error output.
- Confirmed the LED map in `top.v` now says `selected ready-gate latch probe set`.

Sync target for this step: `top.v` and this log need to be copied to `D:\final_project` before the next manual Efinity build/flash.

---

## 16. 2026-07-07 eighth board feedback and current/stable HDMI gate probe set (Codex)

### Board feedback from user

- LEDs ON: LED18, LED19, LED20, LED22, LED23, LED25, LED26, LED27, LED28, LED29, LED30, LED31, LED32.
- LED33 flashing.
- LEDs OFF: LED21 and LED24.
- HDMI image still looks the same as before.

### Interpretation of previous selected ready-gate latch map

This probe used latched "seen" indicators, so ON means the event happened at least once after reset, not necessarily that it stayed true.

Important positives:

- LED18/19: DDR configured and ch0 selected.
- LED20: selected framebuffer ready was seen.
- LED22: selected `frame_ok_hdmi` was seen.
- LED23: selected CDC bridge active was seen.
- LED25: selected CDC FIFO reached start level.
- LED27: raw `hdmi_video_ready` was seen.
- LED28/29/30/31: framebuffer `frame_stable`, `wr_frame_done`, `rd_frame_available`, and DDR read data were all seen.
- LED33 flashing: selected CSI RAW10/4ppc format is present but not visually stable under this LED wiring.

Important negatives / bad indicators:

- LED21 OFF: selected framebuffer underflow was not latched.
- LED24 OFF: selected CDC bridge underflow was not latched.
- LED26 ON: selected CDC FIFO low-water was latched, but this can happen during initial FIFO fill before the bridge starts. It is not enough by itself to prove a runtime underflow.
- LED32 ON: selected DDR read gap was latched. This is the more concerning sign: DDR read data has gaps while read is active.

The result proves `hdmi_video_ready` can become true, but it does not prove it remains true long enough for `hdmi_top` to switch away from fallback and hold valid input video. The next probe therefore changes LEDs from mostly "ever seen" to current state plus "held stable long enough" counters.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Replaced the selected ready-gate latch LED map with a current/stable HDMI gate probe set.
- Added saturating hold counters in `hdmi_tx_slow_clk` domain for:
  - selected framebuffer ready held high
  - selected CDC bridge active held high
  - raw `hdmi_video_ready` held high
  - `hdmi_top use_input_video` held high
- Kept LED32 as the latched DDR read gap bad indicator.
- No video data path behavior was intentionally changed.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`); should be ON |
| LED20 | F3 | `led[2]` | current selected framebuffer ready |
| LED21 | F2 | `led[3]` | current selected `frame_ok_hdmi` |
| LED22 | G2 | `dbg_ddr_ok` | current selected CDC bridge active |
| LED23 | K6 | `dbg_fb0_ready` | current selected CDC bridge underflow, 1=BAD |
| LED24 | J3 | `dbg_fb0_underflow` | current raw `hdmi_video_ready` |
| LED25 | L6 | `dbg_csi_fmt_ok` | `hdmi_top` `o_input_stable` |
| LED26 | K4 | `dbg_bridge_active` | `hdmi_top` `use_input_video` |
| LED27 | K3 | `dbg_bridge_under` | selected framebuffer ready held stable long enough |
| LED28 | M5 | `dbg_video_ready` | selected CDC bridge active held stable long enough |
| LED29 | M6 | `dbg_input_stable` | raw `hdmi_video_ready` held stable long enough |
| LED30 | N7 | `dbg_led30` | `hdmi_top use_input_video` held stable long enough |
| LED31 | P7 | `dbg_led31` | HDMI input pixel data changed |
| LED32 | P6 | `dbg_led32` | selected DDR read gap seen, 1=BAD |
| LED33 | R6 | `dbg_led33` | selected CSI RAW10/4ppc format |

### Manual checks for next run

- If LED20/21/22 are ON but LED24 is OFF, the three ready terms are not overlapping in the same HDMI clock cycles.
- If LED24 is ON but LED29 is OFF, `hdmi_video_ready` is only intermittent.
- If LED24/29 are ON but LED26/30 are OFF, `hdmi_top` is not accepting the input despite ready being stable; return to `hdmi_top` timing/size/error probe.
- If LED26/30/31 are ON and the picture is still unchanged, HDMI is using live input and the next target is pixel correctness rather than readiness.
- If LED32 remains ON while LED24/29 are unstable, focus on DDR read continuity / read FIFO refill rather than RAW/Bayer ordering.

### Verification performed by Codex

- Ran `git diff --check` on `top.v`; no whitespace/error output.
- Confirmed the LED map in `top.v` now says `current/stable HDMI gate probe set`.

Sync target for this step: `top.v` and this log need to be copied to `D:\final_project` before the next manual Efinity build/flash.
