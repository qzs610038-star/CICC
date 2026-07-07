# HDMI 横纹调试上下文 — Phase 2

> 日期: 2026-07-07 (更新)
> 分支: dev/libaoxun688
> 目标: 摄像头画面通过 FPGA → HDMI → 电脑正常显示
> 上板状态: HDMI 有画面但有横纹，LED24 + LED27 持续亮（读 FIFO 下溢 + CDC 桥下溢）

---

## 1. 板上 Debug LED 定义（关键排障工具）

| LED | 管脚 | 信号 | 含义 | 1= | 0= |
|-----|------|------|------|----|----|
| LED22 | G2 | dbg_ddr_ok | DDR 配置完成 | OK | BAD |
| LED23 | K6 | dbg_fb0_ready | ch0 framebuffer 锁定稳定帧 | OK | BAD |
| **LED24** | **J3** | **dbg_fb0_underflow** | **ch0 读 FIFO 下溢（锁存）** | **BAD** | OK |
| LED25 | L6 | dbg_csi_fmt_ok | CSI RAW10/4ppc 确认 | OK | BAD |
| LED26 | K4 | dbg_bridge_active | CDC 2pix→1pix 桥运行中 | OK | BAD |
| **LED27** | **K3** | **dbg_bridge_under** | **CDC 桥 FIFO 下溢（锁存）** | **BAD** | OK |
| LED28 | M5 | dbg_video_ready | i_video_ready → hdmi_top | OK | BAD |
| LED29 | M6 | dbg_input_stable | hdmi_top 接受输入时序 | OK | BAD |

代码位置: `final_project/fpga/rtl/top/top.v` 端口声明 L206-216，驱动逻辑 L1801-1868。

**LED 排障原则**:
- LED24 = 1 表示 ch0 framebuffer 内部的读 FIFO（`data_tx` 模块中的 `DC_FIFO`）在 `fifo_rd_empty & fifo_rd_en` 时发生了下溢
- LED27 = 1 表示 CDC 桥（`video_2pix_to_1pix_cdc`）在输出端空了，即写入速度跟不上读出速度
- 两个下溢信号都是锁存的——只要发生过就会保持亮，按 SW4 切换通道时清零
- 如果按 SW4 清零后立即重新亮起 → 下溢是持续性的（每帧都在发生）
- 如果按 SW4 清零后灭掉一段时间再亮起 → 下溢是间歇性的

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
