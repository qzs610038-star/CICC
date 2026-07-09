# HDMI 横纹调试上下�?�?Phase 2

> 日期: 2026-07-07 (更新)
> 分支: dev/libaoxun688
> 目标: 摄像头画面通过 FPGA �?HDMI �?电脑正常显示
> 上板状�? HDMI 有画面但有横纹，LED24 + LED27 持续亮（�?FIFO 下溢 + CDC 桥下溢）

---

## 1. 板上 Debug LED 定义（关键排障工具）

### 1.1 板卡 LED 物理映射�?026-07-07 用户确认�?
| LED 编号 | 信号名称 | 管脚�?| 所�?Bank |
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

Efinity GPIO 资源名以 outflow `Ti375C529_devkit.pinout.rpt` 核对：LED18=B2=`GPIOB_N_30_CDI8`，LED19=E3=`GPIOB_P_24_CDI15`，LED20=F3=`GPIOB_N_24_CDI14`，LED21=F2=`GPIOB_P_17_CLK4_P`，LED22=G2=`GPIOB_N_17_CLK4_N`，LED23=K6=`GPIOB_P_25_CDI13`，LED24=J3=`GPIOB_N_28`，LED25=L6=`GPIOB_N_25_CDI12`，LED26=K4=`GPIOB_N_29_CDI10`，LED27=K3=`GPIOB_P_29_CDI11_EXTFB`，LED28=M5=`GPIOB_P_22`，LED29=M6=`GPIOB_N_22`�?
### 1.2 Phase 2 Debug LED 映射

| LED | 管脚 | 信号 | 含义 | 1= | 0= |
|-----|------|------|------|----|----|
| LED18 | B2 | led[0] | DDR 配置完成 | OK | BAD |
| LED19 | E3 | led[1] | 当前选择通道 framebuffer ready | OK | BAD |
| **LED20** | **F3** | **led[2]** | **当前选择通道 framebuffer �?FIFO 下溢** | **BAD** | OK |
| LED21 | F2 | led[3] | 当前选择通道 CSI RAW10/4ppc 确认 | OK | BAD |
| LED22 | G2 | dbg_ddr_ok | 当前选择通道 framebuffer 读周�?active | OK | BAD |
| LED23 | K6 | dbg_fb0_ready | 当前选择通道 DDR 读数据曾到达 | OK | BAD |
| **LED24** | **J3** | **dbg_fb0_underflow** | **当前选择通道 DDR 读数�?gap 过长（锁存）** | **BAD** | OK |
| LED25 | L6 | dbg_csi_fmt_ok | 当前选择通道 CDC FIFO 达到启动水位 | OK | BAD |
| LED26 | K4 | dbg_bridge_active | 当前选择通道 CDC 2pix�?pix 桥输�?active | OK | BAD |
| **LED27** | **K3** | **dbg_bridge_under** | **当前选择通道 CDC �?FIFO 下溢（锁存）** | **BAD** | OK |
| LED28 | M5 | dbg_video_ready | i_video_ready �?hdmi_top | OK | BAD |
| LED29 | M6 | dbg_input_stable | hdmi_top 接受输入时序�?CSI 格式 OK | OK | BAD |

代码位置: `final_project/fpga/rtl/top/top.v` 端口声明�?L205-214，驱动逻辑�?L1814-L1900；内部探针来�?`frame_buffer.v` �?`video_2pix_to_1pix_cdc.v`�?
**LED 排障原则**:
- LED20 = 1 表示当前选择通道 framebuffer 内部的读 FIFO（`data_tx` 模块中的 `DC_FIFO`）在 `fifo_rd_empty & fifo_rd_en` 时发生了下溢
- LED24 = 1 表示当前选择通道 AXI/DDR 读数据出现超�?1024 �?AXI 时钟周期的无 `ddr_rd_valid` gap
- LED27 = 1 表示 CDC 桥（`video_2pix_to_1pix_cdc`）在输出端空了，即写入速度跟不上读出速度
- 两个下溢信号都是锁存的——只要发生过就会保持亮，�?SW4 切换通道时清�?- 如果�?SW4 清零后立即重新亮�?�?对应错误是持续性的（每帧都在发生）
- 如果�?SW4 清零后灭掉一段时间再亮起 �?对应错误是间歇性的

---

## 2. 已排除的假设

### 假设 A: DDR 读带宽不�?�?已排�?
**原假�?*: 两个 framebuffer 同时竞争 DDR 读写，实际可用读带宽不够 1920×1080 @60fps�?
**验证手段**:
- Patch 1: 关闭 ch1 framebuffer �?CSI 输入（`.i_de = 1'b0`, `.vin = 32'd0`），只保�?ch0 独立使用 DDR
- Patch 2: �?framebuffer 输出水平有效像素�?960 dual-pixel（对�?1920 single-pixel）降�?480 dual-pixel（对�?960 single-pixel），DDR 读带宽需求减�?- Patch 2b: `hdmi_top.v` �?`MAX_HRES` �?1920 同步改为 960

**验证结果�?026-07-07 上板�?*:
- LED22-29 **全部亮起**，与改之前完全一�?- LED24 (fb0_underflow) = 亮（持续�?- LED27 (bridge_underflow) = 亮（持续�?- HDMI 画面横纹依旧存在
- 即使 DDR 只服务一�?960×1080 输出，下溢仍然持�?
**结论**: DDR 读带�?*不是**根因。问题在别处�?
### 假设 B: ch1 竞争 DDR 写入带宽 �?已排�?
Patch 1 已把 ch1 �?CSI 写输入全部清零，ch1 不写 DDR。下溢依旧�?
---

## 3. 未被排除的假设（下一步排查方向）

### 假设 C: 时钟域不匹配 / CDC FIFO 深度不足

**现象证据**: LED27（CDC 桥下溢）持续性亮。CDC 桥是 `video_2pix_to_1pix_cdc`，写侧时�?`i_sysclk_div2`（约�?14.286ns �?70MHz），读侧时钟 `hdmi_tx_slow_clk`（约�?7.143ns �?140MHz）�?
**可疑�?*:
- 70MHz 写侧�?2 pixels/clock 产生数据（对�?140M pixels/s�?- 140MHz 读侧�?1 pixel/clock 消耗数据（对应 140M pixels/s�?- 带宽理论上匹配，但实际像素吞吐可能因为消隐期占比不同而导致写/读速率不均
- `START_LEVEL = 16` 可能太浅，无法吸收突发抖�?
**验证方法**:

**Step 1: 增大 CDC FIFO 深度和启动水�?*
- 修改 `video_2pix_to_1pix_cdc` 例化参数: `FIFO_DEPTH` �?1024 改为 4096，`START_LEVEL` �?16 改为 256
- 位置: `top.v` L1695-1698 (ch0) �?L1714-1717 (ch1)
- 编译烧录 �?观察 LED27 是否熄灭

**Step 2: �?CDC 桥内部增�?debug 探针**
- �?`fifo_wr_usedw` / `fifo_rd_usedw` 引出�?spare LED，观�?FIFO 水位是否持续走低
- 如果写侧 always 满（wr_usedw 接近 1024）→ 读不够快
- 如果读侧 always �?�?写不够快

### 假设 D: Framebuffer 输出时序参数与实际不匹配

**现象证据**: LED24（framebuffer �?FIFO 下溢）持续性亮。framebuffer 内部 `data_tx` 模块�?`par2ser_parse �?DC_FIFO �?时序生成器` 的路径中，如�?`H_VALID` 参数与实际像素数据速率不匹配，会导�?FIFO 读空�?
**可疑�?*:
- `frame_buffer.v` �?`fifo_rd_period` 信号控制 DDR 读端的使能。如果这个信号的占空比计算有误（如认为消隐期比实际长），DDR 读端会停太久，导致读 FIFO 被掏�?- `data_tx` 模块输出的时序参数（h_front_porch / h_sync / h_valid 等）是从 `frame_buffer` 的输入端口锁存后传递的，确认这些值在 dual-pixel 域下是否正确

**验证方法**:

**Step 1: 确认 `fifo_rd_period` 的实际行�?*
- `fifo_rd_period` �?`data_tx.v` L424 产生: `fifo_rd_period <= ~(v_state == S_V_SYNC && v_cnt == 0);`
- 即除�?VSYNC 的第 0 行之外都�?1，表�?DDR 读在绝大部分时间都在工作
- 这逻辑看起来正确，但需要确�?`v_state` 机是否真的在正常跳转
- **建议添加 LED**: �?`fifo_rd_period` 的反（即 VSYNC 期间短暂亮一下），如果这�?LED 常亮 �?v_state 卡在 S_V_SYNC

**Step 2: 检�?h_valid / v_valid 参数寄存器传�?*
- `frame_buffer.v` L293-303: `h_valid` 等参数在 `o_clk` 域用 `always @(posedge o_clk)` 锁存
- �?`H_VALID` 输入来自 `top.v` �?`HDMI_H_VALID` localparam，是常量
- 确认 `i_clk` �?`o_clk` 是否同源（两者都�?`i_sysclk_div2`）→ �?- 确认 `i_sysclk_div2` 实际频率是否�?70MHz �?需�?scope 确认

### 假设 E: AXI 读通道仲裁或响应延迟导�?DDR 读数据断�?
**现象证据**: 即使 DDR 带宽足够，如�?AXI 读请求的响应延迟过大，DDR 读数据到�?`ddr_rd_buffer` 的间隔会拉长，导致下�?FIFO 被掏空�?
**可疑�?*:
- `ddr_rd_buffer.v` L259: `assign rready = 1'b1` �?读通道来者不拒，但这不代�?AXI 端的 `arvalid→rvalid` 延迟�?- 如果 DDR controller 的读延迟（CAS latency + controller pipeline）导致相�?burst 之间出现 gap，下�?FIFO 可能来不及补�?
**验证方法**:

**Step 1: �?LED 探测 DDR 读有效信号间�?*
- �?`ddr_rd_buffer` 中取 `ddr_rd_valid`（即 `rvalid & rready`），驱动一个计数器
- 如果 `ddr_rd_valid` �?burst 之间有长时间�?100 cycles @ 200MHz = >500ns）的 gap �?AXI 读延迟过�?- 简化版: �?`~ddr_rd_valid` 驱动 LED，如�?LED 明显亮（不止是短暂闪烁）�?读数据断�?
**Step 2: 确认 axi0_ACLK 频率**
- 约束文件: `create_clock -period 5.000 -name axi0_ACLK` �?200MHz
- `i_fb_clk`: `create_clock -period 40.000` �?25MHz（DDR PHY 参考时钟？需确认�?- 如果 `axi0_ACLK` 实际不是 200MHz �?DDR 带宽显著下降

### 假设 F: CSI 输入端的帧率/分辨率与 framebuffer 期望不匹�?
**可疑�?*: framebuffer �?`MAX_VID_WIDTH = 1920`, `MAX_VID_HIGHT = 1080`，但 CSI 实际进来的可能是不同分辨率。如�?`frame_stable` 不满足某条件导致写端不工作，但读端仍在跑 �?读空�?
**验证方法**:
- LED23 (fb0_ready) = 亮说�?`frame_ready = frame_en & out_sync & ~fifo_rd_underflow_latched` 的第 1 �?`frame_en` �?1
- `frame_en = rd_frame_en_r[1] & rd_frame_available`，其�?`rd_frame_en_r[1]` 来自 `frame_stable`
- �?`frame_ready` 亮了不代表写端在正常工作

---

## 4. 推荐的排障执行顺�?
按由简到繁、由外到内的原则:

### �?1 �? 增加 CDC FIFO 深度（假�?C，低风险�? 次编译）

修改 `top.v` 中两�?`video_2pix_to_1pix_cdc` 的例�?
```verilog
// L1695-1698 �?L1714-1717
video_2pix_to_1pix_cdc #(
    .FIFO_DEPTH(4096),     // 原来�?1024
    .START_LEVEL(256)      // 原来�?16
) u_hdmi0_video_cdc (...)
```

**预期**: 如果 LED27 熄灭 �?CDC FIFO 深度不够是根因；否则继续�?
### �?2 �? �?LED 探查 framebuffer 内部关键信号（假�?D/E�?-2 次编译）

**2a: 暴露 `fifo_rd_period` 反信号到 LED**
- �?`top.v` 中把 `frame_buffer` 输出的某个未用信号（或新�?port）接�?LED
- `fifo_rd_period` = 0 只在 VSYNC 期间，如�?LED 常亮 �?v_state 卡死

**2b: 暴露 DDR 读有效信号到 LED**
- �?`ddr_rd_valid`（或 `~ddr_rd_valid`）直接输出到 LED
- 如果 LED 明显不是微弱闪烁而是持续�?�?DDR 读断流严�?
**2c: �?scope 实测时钟频率**
- �?`i_sysclk_div2` 实际频率（期�?70MHz�?- �?`hdmi_tx_slow_clk` 实际频率（期�?140MHz�?- �?`axi0_ACLK` 实际频率（期�?200MHz�?- 如果实际频率与约束不�?�?PLL 配置有问�?
### �?3 �? 绕过 DDR 做最小环路测试（假设全部，中风险�?-2 次编译）

临时�?`top.v` 中新增一�?color bar 发生器（已在代码中被注释掉，�?L1128-1155 �?`color_bar_rgb`），直接产生 1920×1080 的彩�?�?debayer bypass �?CDC �?�?HDMI�?
**预期**:
- 如果彩条正常无横�?�?问题�?DDR/framebuffer 读写链路
- 如果彩条也有横纹 �?问题�?CDC 桥或 HDMI TX 时钟�?
---

## 5. 修改涉及的文件清�?
| 文件 | 路径 |
|------|------|
| 顶层 | `final_project/fpga/rtl/top/top.v` |
| CDC �?| `final_project/fpga/rtl/dvi_tx/video_2pix_to_1pix_cdc.v` |
| HDMI TOP | `final_project/fpga/rtl/dvi_tx/hdmi_top.v` |
| Framebuffer | `final_project/fpga/rtl/framebuffer/frame_buffer.v` |
| DDR 读缓�?| `final_project/fpga/rtl/framebuffer/ddr_rd_buffer.v` |
| 读端时序生成 | `final_project/fpga/rtl/framebuffer/data_tx.v` |
| 约束 | `final_project/fpga/efinity/constrain.sdc` |
| 管脚映射 | `final_project/fpga/efinity/mem_test.peri.xml` |

**每次修改 C 盘后务必同步�?D �?*:
```powershell
Copy-Item "C:\...\top.v" "D:\final_project\fpga\rtl\top\top.v" -Force
```

---

## 6. 当前代码修改状态（2026-07-07�?
以下修改**已在代码中生�?*，不要回退除非确认不需�?

1. `top.v` L629: `HDMI_H_VALID = 13'd480`（已�?960 减半�?2. `top.v` L1190-1191: ch1 framebuffer `.i_de = 1'b0`, `.vin = 32'd0`
3. `hdmi_top.v` L50: `MAX_HRES = 12'd960`（已�?1920 减半�?4. `top.v` L206-216 + L1801-1868: 8 �?debug LED 驱动逻辑

**如果要做�?3 步（color bar 测试），建议先回退 Patch 1+2 �?H_VALID �?MAX_HRES 修改，恢复到 1920×1080 输出**�?
---

## 7. 时钟域速查

```
i_sysclk_div2     ~70MHz     CSI 像素�?/ framebuffer 读写共用
hdmi_tx_slow_clk  ~140MHz    HDMI TX 像素�?/ CDC 桥读�?hdmi_tx_fast_clk  ~700MHz    HDMI TX 串行�?(5x)
axi0_ACLK         ~200MHz    AXI / DDR 控制器域
i_fb_clk           ~25MHz    DDR PHY 参考时�?```

CDC 桥从 70MHz (2pix/clk = 140Mpix/s) �?140MHz (1pix/clk = 140Mpix/s)，理论带宽刚好匹配，�?**消隐期占比在两侧可能不同**（dual-pixel 域和 single-pixel 域的 H 消隐参数换算可能不一致），导致平均写入速率 < 平均读出速率�?
---

## 8. 2026-07-07 本轮执行记录（Codex�?
### 修改目标

�?Phase 2 推荐顺序执行“增�?CDC FIFO 深度”，并同步加入可上板判读�?LED 探针，避免只�?HDMI 横纹现象猜测�?
### 已修�?
1. `top.v`
   - `u_hdmi0_video_cdc` / `u_hdmi1_video_cdc`: `FIFO_DEPTH 1024 -> 4096`，`START_LEVEL 16 -> 256`�?   - LED18-29 重新作为 HDMI 横纹排障灯组，覆�?DDR ready、framebuffer ready、framebuffer underflow、CSI 格式、framebuffer 读周期、DDR 读数据到达、DDR �?gap、CDC FIFO 水位、CDC active、CDC underflow、HDMI video ready、HDMI input stable�?2. `frame_buffer.v`
   - 新增只读 debug 输出：`dbg_fifo_rd_period`、`dbg_ddr_rd_seen`、`dbg_ddr_read_gap`�?   - `dbg_ddr_read_gap` 表示读周期内连续 1024 �?AXI 时钟周期未见 `ddr_rd_valid`，用于粗略定�?AXI/DDR 读数据断流�?3. `video_2pix_to_1pix_cdc.v`
   - 新增只读 debug 输出：`o_level_ready`、`o_level_low`�?   - 本轮 LED 使用 `o_level_ready`，表�?CDC FIFO 已达�?`START_LEVEL`�?4. `mem_test.peri.xml`
   - 显式绑定 LED22-29 �?`dbg_*` 顶层端口�?BANK4C 管脚，避免依赖手�?Interface Designer 状态�?
### 上板判读重点

- �?LED18 亮、LED19 亮、LED20 亮：framebuffer �?ready 但读端仍下溢，优先看 LED22/23/24�?- �?LED22 常灭：`data_tx` 读周期没有进入正�?active，怀疑读端时序状态机�?frame_en/out_sync�?- �?LED23 灭：读周期内从未收到 DDR 读数据，优先�?AXI 读请�?DDR 响应�?- �?LED23 亮但 LED24 亮：DDR 数据能到，但存在�?gap，优先查 AXI 仲裁/读延�?burst 连续性�?- �?LED25 灭或 LED27 亮：CDC FIFO 没有维持足够水位，优先查 70MHz 写侧平均吞吐、消隐换算或 CDC 读写策略�?- �?LED28 灭：`hdmi_top` 仍未接收�?video ready，通常�?framebuffer/CDC 错误门控导致�?- �?LED29 灭但 LED28 亮：HDMI 输入时序/尺寸检测或 CSI 格式条件未满足�?
### 待验�?
- `efx_map` 前端综合已通过�?  - 命令：`D:\Efinity\2025.2\bin\efx_map.exe --project-xml mem_test.xml --root top --family Titanium --device TJ375N529 --work-dir work_syn_codex_hdmi_led_probe_v1 --output-dir work_syn_codex_hdmi_led_probe_v1`
  - 目录：`D:\final_project\fpga\efinity`
  - 结果：exit code 0；综合日志中确认 `video_2pix_to_1pix_cdc(FIFO_DEPTH=4096,START_LEVEL=256)`�?  - `EFX.err.log` 无新增错误；`EFX.warn.log` 仍有既有未连接端�?位宽�?warning，本轮搜索未发现 `dbg_*` LED 端口绑定相关 warning�?- 仍需要完�?Efinity P&R/bitstream 生成并烧录，上板记录 LED18-29 状态�?- 若布局布线出现新的 I/O 绑定、Bank 电平�?timing warning，必须先记录 warning 原文再判断是否继续烧录�?
---

## 参考文�?
- 开发路�? `分赛区决赛实施开发路�?md`
- 管线现状: `final_project/docs/architecture/dual_camera_hdmi_pipeline_current_2026-07-06.md`
- Fix 计划: `final_project/docs/architecture/dual_camera_hdmi_fix_plan_2026-07-06.md`
- 顶层 RTL: `final_project/fpga/rtl/top/top.v`
- CDC �? `final_project/fpga/rtl/dvi_tx/video_2pix_to_1pix_cdc.v`
- HDMI TOP: `final_project/fpga/rtl/dvi_tx/hdmi_top.v`
- Framebuffer: `final_project/fpga/rtl/framebuffer/frame_buffer.v`
- DDR 读缓�? `final_project/fpga/rtl/framebuffer/ddr_rd_buffer.v`
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

---

## 17. 2026-07-08 ninth board feedback and frame_ready factor probe set (Codex)

### Board feedback from user

- LEDs ON: LED18, LED19, LED22, LED28.
- LED33 flashing.
- All other LEDs OFF.
- HDMI image still looks the same as before.

### Interpretation of previous current/stable HDMI gate probe map

Previous map meanings:

- LED18 ON: DDR configured.
- LED19 ON: ch0 selected.
- LED22 ON: selected CDC bridge is currently active.
- LED28 ON: selected CDC bridge active has been held stable long enough.
- LED20 OFF: selected framebuffer `frame_ready` is currently not asserted.
- LED21 OFF: selected `frame_ok_hdmi` is currently not asserted.
- LED24 OFF: raw `hdmi_video_ready` is currently not asserted.
- LED25/26/30 OFF: `hdmi_top` is not accepting input video.

This means the HDMI-side CDC bridge can remain active, but the selected framebuffer ready condition is not currently true. The next target is the exact equation in `frame_buffer.v`:

```verilog
frame_ready = frame_en & out_sync & ~fifo_rd_underflow_latched;
```

Previous probes already showed DDR read/write events can happen, but this run says the live/current ready condition is missing. The next probe splits `frame_en`, `out_sync`, and the read FIFO underflow latch.

### RTL changes

`final_project/fpga/rtl/framebuffer/frame_buffer.v`

- Added read-only debug outputs:
  - `dbg_frame_en`
  - `dbg_tx_underflow_seen`
  - `dbg_fifo_rd_frame_end_seen`
- These expose whether the read/display side is enabled and whether the data_tx read FIFO underflow or frame-end events occurred.

`final_project/fpga/rtl/top/top.v`

- Connected the new debug outputs for both ch0 and ch1 `frame_buffer` instances.
- Added selected-channel wires for the new debug outputs.
- Replaced the current/stable HDMI gate LED map with a `frame_ready` factor probe set.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`); should be ON |
| LED20 | F3 | `led[2]` | current selected `frame_ready` |
| LED21 | F2 | `led[3]` | current selected `frame_en` |
| LED22 | G2 | `dbg_ddr_ok` | `out_sync` |
| LED23 | K6 | `dbg_fb0_ready` | current selected FIFO read underflow latch, 1=BAD |
| LED24 | J3 | `dbg_fb0_underflow` | current selected `frame_stable` |
| LED25 | L6 | `dbg_csi_fmt_ok` | current selected `rd_frame_available` |
| LED26 | K4 | `dbg_bridge_active` | current selected `fifo_rd_period` |
| LED27 | K3 | `dbg_bridge_under` | selected `tx_fifo_rd_underflow` ever seen, 1=BAD |
| LED28 | M5 | `dbg_video_ready` | selected `fifo_rd_frame_end` ever seen |
| LED29 | M6 | `dbg_input_stable` | selected DDR read gap seen, 1=BAD |
| LED30 | N7 | `dbg_led30` | selected `frame_ready` held stable long enough |
| LED31 | P7 | `dbg_led31` | selected `frame_en` held stable long enough |
| LED32 | P6 | `dbg_led32` | raw `hdmi_video_ready` |
| LED33 | R6 | `dbg_led33` | selected CSI RAW10/4ppc format |

### Manual checks for next run

- If LED22 is ON but LED21 is OFF, `out_sync` is okay and `frame_ready` is blocked because `frame_en` is false.
- If LED21 and LED22 are ON but LED20 is OFF, LED23 should explain it: FIFO read underflow latch is blocking `frame_ready`.
- If LED21/22/20 are ON but LED32 is OFF, the later selected ready gate or CDC bridge is blocking HDMI ready.
- If LED24/25 are ON but LED21 is OFF, `frame_stable` and `rd_frame_available` exist but do not combine into `frame_en`; inspect `rd_frame_en_r` crossing/holding.
- If LED27 is ON, data_tx read FIFO underflow has happened at least once and can explain intermittent `frame_ready`.
- If LED29 remains ON, DDR read gaps are still present and may starve the read/display side.

### Verification performed by Codex

- Ran `git diff --check` on `frame_buffer.v` and `top.v`; no whitespace/error output.
- Confirmed the LED map in `top.v` now says `frame_ready factor probe set`.
- Confirmed both `frame_buffer` instances connect the new debug outputs.

Sync target for this step: `frame_buffer.v`, `top.v`, and this log need to be copied to `D:\final_project` before the next manual Efinity build/flash.

---

---

---

## 18. 2026-07-08 tenth board feedback and ch0 front/write/read milestone probe set (Codex)

### Board feedback from user

- LEDs ON: LED18, LED19, LED22.
- All other LEDs OFF.
- HDMI image still looks the same as before.

### Interpretation of previous `frame_ready` factor probe map

Previous map meanings:

- LED18 ON: DDR configured.
- LED19 ON: ch0 selected.
- LED22 ON: `out_sync` is asserted.
- LED20 OFF: selected `frame_ready` is not currently asserted.
- LED21 OFF: selected `frame_en` is not currently asserted.
- LED23 OFF: selected FIFO read underflow latch is not currently blocking `frame_ready`.
- LED24 OFF: selected `frame_stable` is not currently asserted.
- LED25 OFF: selected `rd_frame_available` is not currently asserted.
- LED26 OFF: selected `fifo_rd_period` is not currently active.
- LED32 OFF: raw `hdmi_video_ready` is not asserted.
- LED33 OFF: selected CSI RAW10/4ppc format is not currently stable under this current-state LED.

This narrows the live failure to the upstream side of `frame_en`, not to `out_sync` or a currently latched read FIFO underflow. The next LED set backs up from `frame_ready` into ch0 CSI input milestones, frame detection, DDR write completion, and DDR read start/data milestones.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Replaced the `frame_ready` factor LED map with a ch0 front/write/read milestone probe set.
- No video data path behavior was intentionally changed.
- The new map uses mostly latched "seen" probes so that short VS/DE/frame/AXI pulses can be observed on board LEDs.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`); should be ON |
| LED20 | F3 | `led[2]` | ch0 CSI VS seen |
| LED21 | F2 | `led[3]` | ch0 CSI DE seen |
| LED22 | G2 | `dbg_ddr_ok` | `out_sync` |
| LED23 | K6 | `dbg_fb0_ready` | ch0 CSI RAW10/4ppc format seen |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 `frame_start` seen |
| LED25 | L6 | `dbg_csi_fmt_ok` | current selected `frame_stable` |
| LED26 | K4 | `dbg_bridge_active` | selected `frame_stable` ever seen |
| LED27 | K3 | `dbg_bridge_under` | ch0 write FIFO write enable seen |
| LED28 | M5 | `dbg_video_ready` | ch0 write start seen: `frame_start & frame_stable` |
| LED29 | M6 | `dbg_input_stable` | ch0 AXI write address valid seen |
| LED30 | N7 | `dbg_led30` | ch0 DDR write frame done seen |
| LED31 | P7 | `dbg_led31` | current ch0 `rd_frame_available` |
| LED32 | P6 | `dbg_led32` | ch0 AXI read address valid seen |
| LED33 | R6 | `dbg_led33` | ch0 DDR read data seen |

### Manual checks for next run

- If LED20/21 are OFF, the failure is before framebuffer: ch0 CSI VS/DE are not being seen by `top.v`.
- If LED20/21/23 are ON but LED24 is OFF, CSI format is recognized but `frame_info_det` is not producing `frame_start`.
- If LED24 is ON but LED25/26 are OFF, frame starts happen but `frame_stable` does not assert or does not persist.
- If LED24/26 are ON but LED28 is OFF, `wr_start = frame_start & frame_stable` is not overlapping.
- If LED28 is ON but LED29/30 are OFF, focus on the framebuffer write AXI path.
- If LED30 is ON but LED31/32/33 are OFF, DDR write completes but read availability/read start/read data is failing.
- If LED30/31/32/33 are ON while LED20/21/23 stay ON, return to the downstream `frame_ready` and HDMI gate probes.

### Verification performed by Codex

- Ran `git diff --check` on `top.v` and this log; no whitespace/error output.
- Confirmed the LED map in `top.v` now says `ch0 front/write/read milestone probe set`.
- Synced `top.v` and this log to `D:\final_project`.

---

## 19. 2026-07-08 eleventh board feedback and ch0 CSI front-end probe set (Codex)

### Board feedback from user

- LEDs ON: LED18, LED19, LED22.
- All other LEDs OFF.

### Interpretation of previous ch0 front/write/read milestone probe map

Previous map meanings:

- LED18 ON: DDR configured.
- LED19 ON: ch0 selected.
- LED22 ON: `out_sync` is asserted.
- LED20 OFF: ch0 CSI VS was not seen.
- LED21 OFF: ch0 CSI DE was not seen.
- LED23 OFF: ch0 RAW10/4ppc format was not seen.
- LED24 OFF and above: no ch0 `frame_start`, no frame/write/read milestones.

This pushes the failure point before the framebuffer. The next probe checks whether ch0 camera reset/I2C, MIPI D-PHY/FIFO activity, and CSI controller outputs are alive.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Replaced the ch0 front/write/read milestone map with a ch0 CSI front-end probe set.
- No video data path behavior was intentionally changed.
- Reused existing ch0 observability registers for I2C edges, MIPI HS/FIFO activity, datatype, pixel-per-clock, VS, and DE.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`); should be ON |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 CSI pixel reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C controller reset released |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C SCL edge seen |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C SDA edge seen |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 camera reset output level |
| LED26 | K4 | `dbg_bridge_active` | ch0 MIPI clock-lane HS enable seen |
| LED27 | K3 | `dbg_bridge_under` | ch0 MIPI data-lane HS enable seen |
| LED28 | M5 | `dbg_video_ready` | ch0 MIPI lane FIFO non-empty seen |
| LED29 | M6 | `dbg_input_stable` | ch0 MIPI lane FIFO read seen |
| LED30 | N7 | `dbg_led30` | ch0 CSI datatype nonzero seen |
| LED31 | P7 | `dbg_led31` | ch0 CSI pixel_per_clk nonzero seen |
| LED32 | P6 | `dbg_led32` | ch0 CSI VS seen |
| LED33 | R6 | `dbg_led33` | ch0 CSI DE seen |

### Manual checks for next run

- If LED20/21/22 are not all ON, focus on reset release into `soft_mipi_rx_top`.
- If LED20/21/22 are ON but LED23/24 are OFF, I2C may not be toggling on the observed ch0 pins.
- If LED23/24 are ON but LED25 suggests the camera is held in the wrong reset level, inspect `o_cam_rst_p` polarity and camera reset wiring.
- If LED23/24/25 are good but LED26/27/28 remain OFF, camera initialization may not be producing MIPI HS traffic or the MIPI lane wiring/polarity is wrong.
- If LED26/27/28 are ON but LED29/30/31/32/33 are OFF, the D-PHY side has activity but the CSI controller is not decoding valid packets.
- If LED30/31 are ON but LED32/33 are OFF, packets are decoded but frame VS/DE is missing or not on VC0.
- If LED32/33 turn ON, return to `frame_info_det` / framebuffer milestone probes.

### Verification performed by Codex

- Ran `git diff --check` on `top.v` and this log; no whitespace/error output.
- Confirmed the LED map in `top.v` now says `ch0 CSI front-end probe set`.
- Synced `top.v` and this log to `D:\final_project`.

---

## 20. 2026-07-08 twelfth board feedback and camera reset polarity validation build (Codex)

### Board feedback from user

- LEDs ON: LED18, LED19, LED20, LED21, LED22, LED23, LED24.
- All other LEDs OFF.

### Interpretation of previous ch0 CSI front-end probe map

Previous map meanings:

- LED18 ON: DDR configured.
- LED19 ON: ch0 selected.
- LED20 ON: global reset `arst_n` is released.
- LED21 ON: ch0 CSI pixel reset is released.
- LED22 ON: ch0 I2C controller reset is released.
- LED23 ON: ch0 I2C SCL edge was seen.
- LED24 ON: ch0 I2C SDA edge was seen.
- LED25 OFF: ch0 camera reset output `S0_o_cam_rst_p` is low.
- LED26/27/28 OFF: no ch0 MIPI clock-lane HS, data-lane HS, or lane FIFO non-empty was seen.
- LED29/30/31/32/33 OFF: no MIPI FIFO read, CSI datatype, pixel-per-clock, VS, or DE was seen.

This proves reset release into the CSI block and I2C activity are alive, but the camera reset output is low and no MIPI traffic follows. The immediate risk is that the camera is being held in reset while I2C still toggles.

### Source comparison

- Current active module: `final_project/fpga/rtl/top/top.v` instantiates `soft_mipi_rx_top` for ch0 and ch1.
- Current active reset assignment before this step: `final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v` used `assign o_cam_rst_p = ~arst_n;`.
- The vendor demo under `..\TJ375N529_SC431HAI2LCD_Demo_V3` and `赛方提供材料\TJ375N529_SC431HAI2LCD_Demo_V3` also uses `assign o_cam_rst_p = ~arst_n;`.
- The current board evidence contradicts the assumption that low is a released camera reset level for this board/setup, because LED25 is OFF and no MIPI HS/FIFO activity follows despite I2C activity.

### RTL changes

`final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v`

- Changed the camera reset output from `~arst_n` to `arst_n`.
- This is a controlled validation build: after reset release, LED25 should turn ON. If the polarity was the blocker, LED26/27/28 and eventually LED30-33 should begin showing MIPI/CSI activity.
- Kept the existing ch0 CSI front-end LED map in `top.v`.

### LED map remains the same for next run

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`); should be ON |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 CSI pixel reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C controller reset released |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C SCL edge seen |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C SDA edge seen |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 camera reset output level |
| LED26 | K4 | `dbg_bridge_active` | ch0 MIPI clock-lane HS enable seen |
| LED27 | K3 | `dbg_bridge_under` | ch0 MIPI data-lane HS enable seen |
| LED28 | M5 | `dbg_video_ready` | ch0 MIPI lane FIFO non-empty seen |
| LED29 | M6 | `dbg_input_stable` | ch0 MIPI lane FIFO read seen |
| LED30 | N7 | `dbg_led30` | ch0 CSI datatype nonzero seen |
| LED31 | P7 | `dbg_led31` | ch0 CSI pixel_per_clk nonzero seen |
| LED32 | P6 | `dbg_led32` | ch0 CSI VS seen |
| LED33 | R6 | `dbg_led33` | ch0 CSI DE seen |

### Manual checks for next run

- If LED25 turns ON and LED26/27/28 also turn ON, the previous reset polarity was blocking MIPI traffic.
- If LED25 turns ON but LED26/27/28 remain OFF, reset polarity is no longer the blocker; focus on camera init register sequence, lane wiring/polarity, or MIPI clock lane.
- If LED25 remains OFF, the output is not reaching the LED/probe as expected; inspect top-level output mapping or synthesis optimization.
- If LED30/31 turn ON but LED32/33 remain OFF, the CSI controller is decoding packets but not seeing VC0 frame sync / pixel valid.

### Verification performed by Codex

- Ran `git diff --check` on `soft_mipi_rx_top.v` and this log; no whitespace/error output.
- Confirmed `soft_mipi_rx_top.v` now drives `o_cam_rst_p = arst_n`.
- Synced `soft_mipi_rx_top.v` and this log to `D:\final_project`.

---

## 21. 2026-07-08 thirteenth board feedback and I2C config progress probe set (Codex)

### Board feedback from user

- LEDs ON: LED18, LED19, LED20, LED21, LED22, LED23, LED24, LED25.
- All other LEDs OFF.

### Interpretation of previous camera reset polarity validation build

Previous map meanings:

- LED18 ON: DDR configured.
- LED19 ON: ch0 selected.
- LED20/21/22 ON: global reset, ch0 CSI pixel reset, and ch0 I2C controller reset are released.
- LED23/24 ON: ch0 I2C SCL/SDA have toggled.
- LED25 ON: ch0 camera reset output is now high after changing `o_cam_rst_p` to `arst_n`.
- LED26/27/28 OFF: no ch0 MIPI clock-lane HS, data-lane HS, or lane FIFO non-empty was seen.
- LED29/30/31/32/33 OFF: no MIPI FIFO read, CSI datatype, pixel-per-clock, VS, or DE was seen.

The reset polarity validation succeeded at the pin/probe level, but the sensor still does not output MIPI traffic. I2C line toggles alone do not prove that the full register table completed or that the stream-on register was issued, so the next probe exposes I2C configuration progress.

### RTL changes

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_reg_set.v`

- Added read-only debug outputs for:
  - register table complete
  - final ROM index write issued
  - stream-on register `0x0100 = 0x01` write issued

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_ctrl_top.v`

- Forwarded I2C core init, write issue, write done, config done, final-index, and stream-on debug outputs.

`final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v`

- Forwarded the I2C debug outputs to top-level observability.

`final_project/fpga/rtl/top/top.v`

- Connected ch0/ch1 I2C debug outputs from both `soft_mipi_rx_top` instances.
- Latched short ch0 I2C pulses in the observable clock domain.
- Replaced LED26-33 with I2C config progress plus coarse MIPI activity checks.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`); should be ON |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 CSI pixel reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C controller reset released |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C SCL edge seen |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C SDA edge seen |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 camera reset output level |
| LED26 | K4 | `dbg_bridge_active` | ch0 I2C core init done |
| LED27 | K3 | `dbg_bridge_under` | ch0 I2C register write issued |
| LED28 | M5 | `dbg_video_ready` | ch0 I2C register write completed |
| LED29 | M6 | `dbg_input_stable` | ch0 I2C config reached final ROM index |
| LED30 | N7 | `dbg_led30` | ch0 I2C config done |
| LED31 | P7 | `dbg_led31` | ch0 I2C stream-on `0x0100=0x01` write issued |
| LED32 | P6 | `dbg_led32` | ch0 MIPI clock/data-lane HS enable seen |
| LED33 | R6 | `dbg_led33` | ch0 MIPI FIFO non-empty or FIFO read seen |

### Manual checks for next run

- If LED26 is OFF, the I2C core init sequence is not completing after reset.
- If LED26 is ON but LED27 is OFF, the register-table sequencer is not issuing writes.
- If LED27 is ON but LED28 is OFF, writes are issued but the write engine is not reporting completion.
- If LED28 is ON but LED29/30 are OFF, the table does not reach the final ROM entry; focus on I2C transaction completion/stall.
- If LED29/30 are ON but LED31 is OFF, the table completes but the stream-on write was not observed.
- If LED31 is ON but LED32/33 remain OFF, the sensor was commanded to stream but no MIPI activity reaches the D-PHY side; focus on sensor init contents, clocking, lane wiring/polarity, or MIPI receiver lane config.
- If LED32/33 turn ON, return to CSI datatype/VS/DE probes.

### Verification performed by Codex

- Ran `git diff --check` on `top.v`, `soft_mipi_rx_top.v`, `i2c_master_ctrl_top.v`, `i2c_master_reg_set.v`, and this log; no whitespace/error output.
- Confirmed the new debug ports are connected through the hierarchy.
- Synced changed RTL files and this log to `D:\final_project`.

---

## 22. 2026-07-08 fourteenth board feedback and stream-on index probe correction (Codex)

### Board feedback from user

- LEDs ON: LED18 through LED30.
- LEDs OFF: LED31, LED32, LED33.

### Interpretation of previous I2C config progress probe

Previous map meanings:

- LED18-25 ON: DDR/ch0/reset/I2C pins/camera reset are all in the expected basic state.
- LED26 ON: I2C core init done.
- LED27 ON: I2C register writes were issued.
- LED28 ON: I2C register writes completed.
- LED29 ON: I2C reached the final ROM index.
- LED30 ON: I2C config is done.
- LED31 OFF under the previous exact-match probe: `addr == 16'h0100 && dout == 8'h01 && wr_en` was not observed.
- LED32/33 OFF: no MIPI HS/FIFO activity was seen.

The ROM has a synchronous read output. The exact-match stream-on probe can miss the write because `addr/dout` and the one-cycle `wr_en` are not guaranteed to align in the simple combinational check. Since the ROM table reaches the final entry and `0x0100=0x01` is at ROM index `8'h90`, stream-on should have been passed if the table really advanced to the end.

### RTL changes

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_reg_set.v`

- Added `dbg_stream_on_index_reached`, asserted when the register table counter reaches or passes ROM index `8'h90`.

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_ctrl_top.v`

- Forwarded `dbg_stream_on_index_reached`.

`final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v`

- Forwarded `dbg_i2c_stream_on_index_reached`.

`final_project/fpga/rtl/top/top.v`

- Rewired LED31 to `ch0_dbg_i2c_stream_on_index_reached`.
- Kept LED32/33 as coarse MIPI activity indicators.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`); should be ON |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 CSI pixel reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C controller reset released |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C SCL edge seen |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C SDA edge seen |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 camera reset output level |
| LED26 | K4 | `dbg_bridge_active` | ch0 I2C core init done |
| LED27 | K3 | `dbg_bridge_under` | ch0 I2C register write issued |
| LED28 | M5 | `dbg_video_ready` | ch0 I2C register write completed |
| LED29 | M6 | `dbg_input_stable` | ch0 I2C config reached final ROM index |
| LED30 | N7 | `dbg_led30` | ch0 I2C config done |
| LED31 | P7 | `dbg_led31` | ch0 I2C stream-on ROM index `8'h90` reached |
| LED32 | P6 | `dbg_led32` | ch0 MIPI clock/data-lane HS enable seen |
| LED33 | R6 | `dbg_led33` | ch0 MIPI FIFO non-empty or FIFO read seen |

### Manual checks for next run

- If LED31 turns ON and LED32/33 remain OFF, the I2C table has passed stream-on but no MIPI activity reaches the FPGA; focus on sensor clock/lane/reset timing/init contents/MIPI receiver configuration.
- If LED31 still stays OFF while LED29/30 are ON, there is a counter/probe inconsistency in `i2c_master_reg_set` that needs direct waveform or a simpler high-water counter LED.
- If LED32/33 turn ON, return to CSI datatype/VS/DE probes.

### Verification performed by Codex

- Ran `git diff --check` on changed RTL files and this log; no whitespace/error output.
- Confirmed `dbg_stream_on_index_reached` is connected through the hierarchy.
- Synced changed RTL files and this log to `D:\final_project`.

---

## 23. 2026-07-08 fifteenth board feedback and MIPI physical probe split (Codex)

### Board feedback from user

- LEDs ON: LED18 through LED31.
- LEDs OFF: LED32, LED33.

### Interpretation of previous I2C/MIPI coarse probe

Previous map meanings:

- LED18-25 ON: DDR, selected channel, reset release, I2C pin activity, and camera reset output are in the expected basic state.
- LED26-30 ON: I2C core initialized, writes were issued/completed, the table reached the final ROM index, and config done asserted.
- LED31 ON: the register table reached the stream-on ROM index `8'h90`.
- LED32/33 OFF: no MIPI HS-enable or D-PHY FIFO activity was observed by the coarse probe.

Conclusion: the camera configuration sequence now appears to pass stream-on. The next useful split is below the CSI decoder: byte-clock presence, LP input states, HS input bytes, D-PHY termination/enable, then FIFO activity.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Kept LED18-25 as the basic precondition signals.
- Reassigned LED26-33 from I2C progress to MIPI physical/front-end probes.
- Added a byte-clock toggle latch using `mipi_rx_ck0_CLKOUT`, synchronized back into `i_sysclk_div2`.
- Added latch probes for clock-lane LP input, data-lane0 LP input, any data-lane LP toggle, HS input byte nonzero/toggle, HS termination, HS enable, and FIFO non-empty/read.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`); should be ON |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 CSI pixel reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C controller reset released |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C SCL edge seen |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C SDA edge seen |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 camera reset output level |
| LED26 | K4 | `dbg_bridge_active` | ch0 MIPI byte clock toggled |
| LED27 | K3 | `dbg_bridge_under` | ch0 MIPI clock-lane LP input nonzero |
| LED28 | M5 | `dbg_video_ready` | ch0 MIPI data-lane0 LP input nonzero |
| LED29 | M6 | `dbg_input_stable` | ch0 MIPI any data-lane LP input toggled |
| LED30 | N7 | `dbg_led30` | ch0 MIPI HS input byte nonzero/toggled |
| LED31 | P7 | `dbg_led31` | ch0 MIPI HS termination enabled |
| LED32 | P6 | `dbg_led32` | ch0 MIPI clock/data-lane HS enable seen |
| LED33 | R6 | `dbg_led33` | ch0 MIPI FIFO non-empty or FIFO read seen |

### Manual checks for next run

- If LED18-25 are ON but LED26 is OFF, no MIPI byte clock is reaching the FPGA clock lane after stream-on; focus on sensor clock output, lane wiring/polarity, camera reset/power, or D-PHY clock-lane configuration.
- If LED26 is ON but LED27/28/29 are OFF, the byte clock exists but LP state observation is suspicious; check LP pin mapping and D-PHY port wiring.
- If LED27/28/29 are ON but LED30 is OFF, LP activity exists but no HS byte data is visible; focus on stream-on contents, lane count, lane polarity, and HS receiver enable/termination.
- If LED30 is ON but LED32 is OFF, HS bytes are changing while the CSI controller does not report HS enable; focus on D-PHY controller configuration/reset.
- If LED32 is ON but LED33 is OFF, HS is enabled but the lane FIFO is empty or not being read; focus on CSI packet decoding, lane count, datatype, and FIFO read enable.
- If LED33 turns ON, return to CSI datatype/VS/DE and framebuffer write probes.

### Verification performed by Codex

- Ran `git diff --check` on `top.v` and this log; no whitespace/error output. Git reported only the existing CRLF/LF normalization warning for this Markdown file.
- Confirmed `constrain.sdc` already has `set_clock_groups -asynchronous` for `i_sysclk_div2`, `mipi_rx_ck0_CLKOUT`, and `mipi_rx_ck1_CLKOUT`; no extra constraint was added for this probe.
- Synced the changed `top.v` and this log to `D:\final_project`.

---

## 24. 2026-07-08 sixteenth board feedback and MIPI LP current-state probe (Codex)

### Board feedback from user

- LEDs ON: LED18, LED19, LED20, LED21, LED22, LED23, LED24, LED25, LED27, LED28.
- LEDs OFF: LED26, LED29, LED30, LED31, LED32, LED33.

### Interpretation of previous MIPI physical probe

Previous map meanings:

- LED18-25 ON: DDR, selected channel, reset release, I2C pin activity, and camera reset output are in the expected basic state.
- LED26 OFF: no byte-clock toggle was observed on `mipi_rx_ck0_CLKOUT`.
- LED27 ON: clock-lane LP input was nonzero.
- LED28 ON: data-lane0 LP input was nonzero.
- LED29 OFF: no data-lane LP transition was observed.
- LED30 OFF: no HS input byte was nonzero or toggled.
- LED31 OFF: no HS termination enable was observed.
- LED32 OFF: no HS enable was observed.
- LED33 OFF: no FIFO non-empty/read was observed.

Conclusion: the MIPI pins are not all stuck at zero, but the sensor/receiver is not entering a visible HS transfer. The next useful split is the current LP state on clock lane and data lane0, plus a direct stream-on index recheck in the same build.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Kept LED18-25 as the basic precondition signals.
- Reassigned LED26 to the ch0 I2C stream-on ROM index reached probe.
- Reassigned LED27-30 to the current sampled LP P/N levels:
  - LED27 = clock lane LP_P
  - LED28 = clock lane LP_N
  - LED29 = data lane0 LP_P
  - LED30 = data lane0 LP_N
- Reassigned LED31 to any current LP level on data lanes 1/2/3.
- Reassigned LED32 to byte-clock toggled.
- Reassigned LED33 to any HS/FIFO activity.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`); should be ON |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 CSI pixel reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C controller reset released |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C SCL edge seen |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C SDA edge seen |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 camera reset output level |
| LED26 | K4 | `dbg_bridge_active` | ch0 I2C stream-on ROM index reached |
| LED27 | K3 | `dbg_bridge_under` | ch0 clock-lane LP_P current level |
| LED28 | M5 | `dbg_video_ready` | ch0 clock-lane LP_N current level |
| LED29 | M6 | `dbg_input_stable` | ch0 data-lane0 LP_P current level |
| LED30 | N7 | `dbg_led30` | ch0 data-lane0 LP_N current level |
| LED31 | P7 | `dbg_led31` | ch0 data-lane1/2/3 LP any current level |
| LED32 | P6 | `dbg_led32` | ch0 MIPI byte clock toggled |
| LED33 | R6 | `dbg_led33` | ch0 any HS/FIFO activity seen |

### Manual checks for next run

- LED26 should be ON if the I2C table still reaches stream-on.
- LED27/28 show clock-lane LP state. Both ON means LP11; only LED27 ON means LP10; only LED28 ON means LP01; both OFF means LP00 or no observed LP level.
- LED29/30 show data-lane0 LP state using the same encoding.
- If LED26 is ON and clock/data lanes remain LP11 with LED32/33 OFF, the camera was commanded to stream but remains in LP stop state; focus on sensor init table, XCLK/MCLK, reset/power sequencing, and whether the `0x0100=0x01` stream-on write is accepted.
- If LED26 is OFF in this build, the previous stream-on progression is not reproducing and the I2C table/probe must be checked again before further MIPI work.

---

## 25. 2026-07-08 seventeenth board feedback and I2C ACK/status probe (Codex)

### Board feedback from user

- LEDs ON: LED18 through LED31.
- LEDs OFF: LED32, LED33.

### Interpretation of previous MIPI LP current-state probe

Previous map meanings:

- LED18-25 ON: DDR configured, ch0 selected, global/pixel/I2C reset released, I2C pins toggled, and camera reset output is released.
- LED26 ON: ch0 I2C table reached the stream-on ROM index.
- LED27/28 ON: clock lane current LP_P and LP_N are both high, meaning clock lane LP11.
- LED29/30 ON: data lane0 current LP_P and LP_N are both high, meaning data lane0 LP11.
- LED31 ON: at least one current LP level is high on data lanes 1/2/3.
- LED32 OFF: no `mipi_rx_ck0_CLKOUT` byte-clock toggle was observed.
- LED33 OFF: no HS/FIFO activity was observed.

Conclusion: the camera sequence reaches the stream-on index, but the visible MIPI receiver state remains LP11 with no byte clock and no HS/FIFO activity. The next split must verify whether the I2C writes are actually ACKed/accepted, rather than only checking that the ROM counter advanced.

### RTL changes

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_16addr_8data.v`

- Added latched debug outputs for I2C status register observations while the controller polls `S_WR_TIP_CHK` / `S_RD_TIP_CHK`.
- Latched OpenCores I2C status bits:
  - `sr[7]` / `rxack`: 1 means NACK/no ACK, bad.
  - `sr[6]` / `i2c_busy`.
  - `sr[5]` / `al`: arbitration lost, bad.
  - `sr[1]` / `tip`.
- Added `dbg_last_status[7:0]` for future expansion if LEDs are not enough.

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_ctrl_top.v`

- Forwarded the new I2C status debug signals.

`final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v`

- Forwarded the new I2C status debug signals to the top-level camera instances.

`final_project/fpga/rtl/top/top.v`

- Kept LED18-26 as the basic precondition and stream-on index probes.
- Reassigned LED27-31 to I2C status-register observation and error bits.
- Kept LED32/33 as MIPI byte-clock and HS/FIFO activity probes.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`); should be ON |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 CSI pixel reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C controller reset released |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C SCL edge seen |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C SDA edge seen |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 camera reset output level |
| LED26 | K4 | `dbg_bridge_active` | ch0 I2C stream-on ROM index reached |
| LED27 | K3 | `dbg_bridge_under` | ch0 I2C status register sampled |
| LED28 | M5 | `dbg_video_ready` | ch0 I2C RxACK/NACK ever seen, `1=BAD` |
| LED29 | M6 | `dbg_input_stable` | ch0 I2C arbitration-lost ever seen, `1=BAD` |
| LED30 | N7 | `dbg_led30` | ch0 I2C BUSY ever seen |
| LED31 | P7 | `dbg_led31` | ch0 I2C TIP ever seen |
| LED32 | P6 | `dbg_led32` | ch0 MIPI byte clock toggled |
| LED33 | R6 | `dbg_led33` | ch0 any HS/FIFO activity seen |

### Manual checks for next run

- If LED27 is OFF, the status readback probe did not sample; check the instrumentation/state timing before interpreting ACK.
- If LED27 is ON and LED28 is ON, the I2C master saw NACK/RxACK=1 at least once; focus on device address, wiring, reset/power, or a rejected write in the init table.
- If LED27 is ON and LED28 is OFF, the bus likely ACKed; focus shifts to sensor init contents, MCLK/XCLK, stream-on acceptance, and power/reset sequencing.
- If LED29 is ON, arbitration was lost; treat this as an I2C bus-level fault.
- LED30/31 ON are expected during transfers because BUSY/TIP occur while the master is active; if they must distinguish stuck-vs-pulsed later, change them to current-state or timeout probes.
- If LED32/33 remain OFF with no I2C error bits, the sensor is probably ACKing but still not outputting MIPI; next checks should target MCLK/XCLK, reset/power timing, and the correctness of the sensor ROM table around `0x0100=0x01`.

---

## 26. 2026-07-08 eighteenth board feedback and I2C NACK phase probe (Codex)

### Board feedback from user

- LEDs ON: LED18 through LED28, LED30, LED31.
- LEDs OFF: LED29, LED32, LED33.

### Interpretation of previous I2C ACK/status probe

Previous map meanings:

- LED18-26 ON: basic board state is still good and the ch0 I2C table reaches the stream-on ROM index.
- LED27 ON: the I2C status register was sampled.
- LED28 ON: `rxack` / NACK was observed at least once. This is a bad-state indicator.
- LED29 OFF: no arbitration-lost event was observed.
- LED30/31 ON: BUSY/TIP occurred during transfer. This is expected for active I2C traffic and does not by itself mean the bus is stuck.
- LED32/33 OFF: no MIPI byte clock or HS/FIFO activity was seen in this build.

Conclusion: the current blocker is now upstream of MIPI output. The sensor register table advances to stream-on, but the I2C controller has seen at least one NACK. The next useful split is to locate which write phase produced the NACK: stale/pre-byte status, device address, register high byte, register low byte, or data byte.

### RTL changes

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_16addr_8data.v`

- Added write-phase-specific NACK latch outputs using `wr_cnt` while sampling status in `S_WR_TIP_CHK`:
  - `wr_cnt == 0`: pre-byte/status-precheck NACK.
  - `wr_cnt == 1`: NACK after device-address byte.
  - `wr_cnt == 2`: NACK after register-high byte.
  - `wr_cnt == 3`: NACK after register-low byte.
  - `wr_cnt == 4`: NACK after data byte.
- Kept the existing aggregate `dbg_status_rxack_seen` output.
- This remains read-only observability; it does not change I2C command sequencing.

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_ctrl_top.v`

- Forwarded the write-phase-specific NACK debug outputs.

`final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v`

- Forwarded the write-phase-specific NACK debug outputs through both camera instances.

`final_project/fpga/rtl/top/top.v`

- Kept LED18-28 as preconditions plus aggregate status/NACK.
- Reassigned LED29-33 to the write-phase-specific NACK latches.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`); should be ON |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 CSI pixel reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C controller reset released |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C SCL edge seen |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C SDA edge seen |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 camera reset output level |
| LED26 | K4 | `dbg_bridge_active` | ch0 I2C stream-on ROM index reached |
| LED27 | K3 | `dbg_bridge_under` | ch0 I2C status register sampled |
| LED28 | M5 | `dbg_video_ready` | ch0 I2C RxACK/NACK ever seen, `1=BAD` |
| LED29 | M6 | `dbg_input_stable` | ch0 I2C NACK before first write byte/status-precheck, `1=BAD` |
| LED30 | N7 | `dbg_led30` | ch0 I2C NACK after device-address byte, `1=BAD` |
| LED31 | P7 | `dbg_led31` | ch0 I2C NACK after register-high byte, `1=BAD` |
| LED32 | P6 | `dbg_led32` | ch0 I2C NACK after register-low byte, `1=BAD` |
| LED33 | R6 | `dbg_led33` | ch0 I2C NACK after data byte, `1=BAD` |

### Manual checks for next run

- LED27 should stay ON; otherwise this probe did not sample status.
- LED28 ON with LED30 ON means the camera did not ACK the device address byte. Focus first on I2C device address, reset release, power, pullups, and whether the selected sensor is physically on that bus.
- LED28 ON with LED30 OFF but LED31/32/33 ON means the device address ACKed, but some register address/data byte was rejected or the controller is sampling stale `rxack` later in the transaction.
- LED29 ON alone may indicate stale `rxack` from a previous transaction before the next byte is sent; use it as a caution flag, not as the primary failure location.
- If LED28 turns OFF but LED32/33 from the previous MIPI activity probes would still be OFF, the next step should return to MCLK/XCLK, reset/power sequencing, and sensor ROM table contents around stream-on.

---

## 27. 2026-07-08 nineteenth board feedback and first-NACK attribution probe (Codex)

### Board feedback from user

- LEDs ON: LED18 through LED33.
- Timing screenshot:
  - WNS: `+1.529 ns`
  - WHS: `+0.026 ns`
  - Notable reported clocks: `axi0_ACLK 288.101 MHz`, `i_fb_clk 263.089 MHz`, `mipi_clk 217.675 MHz`, `i_sysclk_div2 186.776 MHz`, `hdmi_tx_slow_clk 256.213 MHz`, `mipi_rx_ck0_CLKOUT 194.062 MHz`, `mipi_rx_ck1_CLKOUT 391.236 MHz`.

### Interpretation of previous I2C NACK phase probe

Previous map meanings:

- LED18-28 ON: basic state, stream-on index, status sampling, and aggregate NACK are all present.
- LED29-33 ON: all write-phase NACK latches turned on.
- WNS/WHS are positive in this build, so this observation is treated as a functional debug result, not a timing-failure symptom.

Conclusion: the all-on phase result likely means the OpenCores `rxack` status bit remains high after the first unacknowledged byte, so later status polls also see `rxack=1` and latch their phase bits. It does not prove that every byte independently NACKed. The next useful split is therefore the first NACK phase only.

### RTL changes

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_16addr_8data.v`

- Changed phase-specific NACK latches to only record the first `rxack` event:
  - The module now checks `!dbg_status_rxack_seen` before setting one of the phase bits.
  - After the first phase is captured, aggregate `dbg_status_rxack_seen` is set and later NACK observations no longer set additional phase LEDs.
- This still does not change I2C behavior; it only changes debug attribution.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`); should be ON |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 CSI pixel reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C controller reset released |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C SCL edge seen |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C SDA edge seen |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 camera reset output level |
| LED26 | K4 | `dbg_bridge_active` | ch0 I2C stream-on ROM index reached |
| LED27 | K3 | `dbg_bridge_under` | ch0 I2C status register sampled |
| LED28 | M5 | `dbg_video_ready` | ch0 I2C RxACK/NACK ever seen, `1=BAD` |
| LED29 | M6 | `dbg_input_stable` | first NACK was before first write byte/status-precheck, `1=BAD` |
| LED30 | N7 | `dbg_led30` | first NACK was after device-address byte, `1=BAD` |
| LED31 | P7 | `dbg_led31` | first NACK was after register-high byte, `1=BAD` |
| LED32 | P6 | `dbg_led32` | first NACK was after register-low byte, `1=BAD` |
| LED33 | R6 | `dbg_led33` | first NACK was after data byte, `1=BAD` |

### Manual checks for next run

- Exactly one of LED29-33 should normally be ON when LED28 is ON.
- If LED30 is the only first-NACK phase LED, the camera is not ACKing the device address. Use this to check I2C address selection (`8'h60` in `i2c_master_ctrl_top`), reset/power, pullups, and whether ch0 is connected to the selected sensor bus.
- If LED31/32/33 is the first-NACK phase LED, the device address was ACKed and the failure moved into register/data transfer; then focus on I2C controller command sequencing or whether the sensor rejects a specific register-table entry.
- If LED29 is the first-NACK phase LED, it may be a stale status bit before the first byte. Then the next probe should suppress pre-byte attribution and sample only immediately after byte command completion.

---

## 28. 2026-07-08 twentieth board feedback and ch0/ch1 I2C address compare probe (Codex)

### Board feedback from user

- LEDs ON: LED18 through LED28, LED30.
- LEDs OFF: LED29, LED31, LED32, LED33.
- Timing screenshot:
  - WNS: `+1.458 ns`
  - WHS: `+0.026 ns`
  - Notable reported clocks: `axi0_ACLK 282.326 MHz`, `i_fb_clk 280.978 MHz`, `mipi_clk 222.767 MHz`, `i_sysclk_div2 158.253 MHz`, `hdmi_tx_slow_clk 288.850 MHz`, `mipi_rx_ck0_CLKOUT 207.857 MHz`, `mipi_rx_ck1_CLKOUT 416.146 MHz`.

### Interpretation of previous first-NACK attribution probe

Previous map meanings:

- LED18-28 ON: basic state, stream-on index, status sampling, and aggregate NACK are present.
- LED30 ON and LED29/31/32/33 OFF: the first captured NACK happened after the device-address byte.
- WNS/WHS are positive, so this remains a functional I2C/camera bring-up issue.

Conclusion: the current ch0 I2C master sends the configured device address but receives no ACK for the address byte. The current RTL and vendor demo both use `I2C_DEVICE_ADDR = 8'h60` at `i2c_master_ctrl_top`; the `8'h20` value in `i2c_master_reg_set` is only the submodule default and is overridden. Next probe compares ch0 and ch1 I2C behavior to determine whether the active camera/bus is on the other sensor channel, or whether both buses fail at address ACK.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Added ch1 I2C SCL/SDA input edge latches, mirroring the existing ch0 edge latches.
- Reassigned LED26-33 to compare ch0 and ch1 I2C address ACK behavior:
  - ch0 aggregate NACK and first address-byte NACK.
  - ch1 status sampling, aggregate NACK, first address-byte NACK, stream-on index reached, SCL edge, and SDA edge.
- No I2C behavior, address value, ROM table, reset polarity, or timing constraint was changed in this step.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`) |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 CSI pixel reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C controller reset released |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C SCL edge seen |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C SDA edge seen |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 camera reset output level |
| LED26 | K4 | `dbg_bridge_active` | ch0 I2C aggregate NACK seen, `1=BAD` |
| LED27 | K3 | `dbg_bridge_under` | ch0 first NACK was after device-address byte, `1=BAD` |
| LED28 | M5 | `dbg_video_ready` | ch1 I2C status register sampled |
| LED29 | M6 | `dbg_input_stable` | ch1 I2C aggregate NACK seen, `1=BAD` |
| LED30 | N7 | `dbg_led30` | ch1 first NACK was after device-address byte, `1=BAD` |
| LED31 | P7 | `dbg_led31` | ch1 I2C stream-on ROM index reached |
| LED32 | P6 | `dbg_led32` | ch1 I2C SCL edge seen |
| LED33 | R6 | `dbg_led33` | ch1 I2C SDA edge seen |

### Manual checks for next run

- If LED26/27 are ON and LED29/30 are OFF while LED31/32/33 are ON, ch1 may be the working camera I2C bus while ch0 is not ACKing.
- If LED26/27 and LED29/30 are all ON, both camera I2C buses fail at address ACK; focus on address `8'h60`, camera reset/power, pullups, and I/O OE polarity.
- If LED28/31/32/33 are OFF, ch1 I2C did not run or its bus activity is not visible; then stay focused on ch0 physical bus/address.
- If LED29 is OFF and LED31 is ON, ch1 reached stream-on without aggregate NACK; then the next test should temporarily switch the selected display/debug path or compare MIPI activity on ch1.

---

## 29. 2026-07-08 twenty-first board feedback and vendor reset-polarity validation (Codex)

### Board feedback from user

- LEDs ON: LED18 through LED33.
- Timing screenshot:
  - WNS: `+1.724 ns`
  - WHS: `+0.026 ns`
  - Notable reported clocks: `axi0_ACLK 305.250 MHz`, `i_fb_clk 239.120 MHz`, `mipi_clk 229.463 MHz`, `i_sysclk_div2 168.265 MHz`, `hdmi_tx_slow_clk 263.505 MHz`, `mipi_rx_ck0_CLKOUT 153.069 MHz`, `mipi_rx_ck1_CLKOUT 407.664 MHz`.

### Interpretation of previous ch0/ch1 I2C address compare probe

Previous map meanings:

- LED26/27 ON: ch0 saw aggregate NACK and the first NACK was after the device-address byte.
- LED28 ON: ch1 status register was sampled.
- LED29/30 ON: ch1 also saw aggregate NACK and first NACK after the device-address byte.
- LED31/32/33 ON: ch1 I2C sequencer reached stream-on index and ch1 SCL/SDA inputs had visible edge activity.
- WNS/WHS are positive, so this remains a functional board/sensor/I2C issue.

Conclusion: both camera I2C masters are active, both buses show SCL/SDA activity, and both fail at the address ACK phase. This points away from a single selected-channel mismatch and toward address, reset/power, pullups, or OE polarity. The current project had temporarily changed camera reset output to `arst_n`; the vendor demo uses `assign o_cam_rst_p = ~arst_n`. Because both cameras fail to ACK their address, the next lowest-risk functional test is restoring the vendor reset polarity.

### RTL changes

`final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v`

- Restored vendor-demo camera reset polarity:
  - From `assign o_cam_rst_p = arst_n;`
  - To `assign o_cam_rst_p = ~arst_n;`
- No I2C address, ROM table, I2C command sequencing, or timing constraint changed.

`final_project/fpga/rtl/top/top.v`

- Kept the ch0/ch1 I2C address ACK compare LED map.
- Changed LED25 to report reset release under vendor polarity:
  - LED25 = `~S0_o_cam_rst_p`.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`) |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 CSI pixel reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C controller reset released |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C SCL edge seen |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C SDA edge seen |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 camera reset released under vendor polarity |
| LED26 | K4 | `dbg_bridge_active` | ch0 I2C aggregate NACK seen, `1=BAD` |
| LED27 | K3 | `dbg_bridge_under` | ch0 first NACK was after device-address byte, `1=BAD` |
| LED28 | M5 | `dbg_video_ready` | ch1 I2C status register sampled |
| LED29 | M6 | `dbg_input_stable` | ch1 I2C aggregate NACK seen, `1=BAD` |
| LED30 | N7 | `dbg_led30` | ch1 first NACK was after device-address byte, `1=BAD` |
| LED31 | P7 | `dbg_led31` | ch1 I2C stream-on ROM index reached |
| LED32 | P6 | `dbg_led32` | ch1 I2C SCL edge seen |
| LED33 | R6 | `dbg_led33` | ch1 I2C SDA edge seen |

### Manual checks for next run

- If LED26/27 and LED29/30 turn OFF while LED31/32/33 remain ON, reset polarity was the ACK blocker and MIPI probes should be restored next.
- If LED26/27 and LED29/30 remain ON, both buses still fail address ACK even under vendor reset polarity; next focus should be I2C pullups/OE polarity or trying a controlled address-candidate build.
- If LED25 is OFF, the reset-release indicator is not in the expected vendor-released state and the reset path must be checked before further I2C conclusions.

---

## 30. 2026-07-08 twenty-second board feedback and current diagnosis summary (Codex)

### Board feedback from user

- LEDs ON: LED18 through LED33.
- Timing screenshot:
  - WNS: `+1.773 ns`
  - WHS: `+0.016 ns`
  - Notable reported clocks: `axi0_ACLK 309.885 MHz`, `i_fb_clk 250.000 MHz`, `mipi_clk 214.777 MHz`, `i_sysclk_div2 154.440 MHz`, `hdmi_tx_slow_clk 276.625 MHz`, `mipi_rx_ck0_CLKOUT 212.630 MHz`, `mipi_rx_ck1_CLKOUT 381.534 MHz`.

### Interpretation of previous vendor reset-polarity validation

Current map meanings:

- LED18-25 ON: DDR, selected channel, resets, ch0 SCL/SDA activity, and vendor-polarity reset-release indicator are all present.
- LED26/27 ON: ch0 still sees aggregate NACK and first NACK after the device-address byte.
- LED28 ON: ch1 I2C status register is sampled.
- LED29/30 ON: ch1 still sees aggregate NACK and first NACK after the device-address byte.
- LED31/32/33 ON: ch1 table advances to stream-on index and ch1 SCL/SDA activity is visible.
- WNS/WHS are positive, so this is not presently a timing-closure blocker.

### Current diagnosis

The visible HDMI/PC symptom is downstream, but the currently proven failing point is upstream:

- Both camera I2C controllers run.
- Both camera I2C buses show SCL/SDA input activity.
- Both controllers advance through the register table to the stream-on index.
- Both controllers see NACK at the device-address phase.
- Reset polarity restoration to the vendor-demo value did not clear the address NACK.

Therefore the camera sensor is not currently acknowledging the I2C slave address. Until the sensor ACKs and accepts the init table, it cannot be considered configured or streaming. In this state the FPGA should not be expected to receive valid MIPI frames, and HDMI/PC cannot display a restored real camera image. The HDMI output can still show fallback/color/noise patterns because the HDMI timing generator can run without a valid camera frame.

### Most likely remaining fault classes

- I2C address mismatch: current RTL and vendor demo use `I2C_DEVICE_ADDR = 8'h60` as the 8-bit write address.
- I/O OE polarity or open-drain behavior mismatch on camera SCL/SDA.
- Camera power/reset sequencing still not matching the module requirement.
- Camera I2C pullups or board-side electrical issue.
- Camera module not present/responding on the expected S0/S1 buses.

### Recommended next probe

Use a controlled I2C address/OE probe rather than more video-path probes:

- Keep LED18-25 as preconditions.
- Add an address-candidate test build or LED-visible candidate selector for `8'h60` versus one or two likely alternatives.
- In parallel, add SCL/SDA current-level LEDs or low-level-seen latches to prove the bus is idling high and that the target can pull SDA low for ACK.

Do not spend the next step on debayer, framebuffer, DDR, HDMI color ordering, or CSI packet decoding until the I2C address ACK problem is resolved.

---

## 31. 2026-07-08 twenty-third iteration: I2C physical-level/OE probe plus S0 weak-pullup test (Codex)

### User instruction

Continue the planned investigation, try small repair steps while using LEDs to verify each checkpoint.

### Starting diagnosis

The previous board result had LED18-33 all ON with positive timing. Under the previous map, that meant both camera I2C controllers ran, both SCL/SDA inputs showed activity, both tables reached the stream-on region, and both channels still observed NACK after the device-address byte. The immediate target is therefore the I2C physical/OE/pullup layer before returning to any framebuffer, debayer, DDR, or HDMI pixel-path changes.

### Low-risk repair/probe changes

`final_project/fpga/efinity/mem_test.peri.xml`

- Changed S0 camera I2C input pullups to match S1:
  - `S0_io_cam_scl_IN`: `pull_option="none"` -> `pull_option="weak pullup"`.
  - `S0_io_cam_sda_IN`: `pull_option="none"` -> `pull_option="weak pullup"`.
- S1 was already using `weak pullup`.

`final_project/fpga/rtl/top/top.v`

- Added I2C low-level-seen and SDA-OE-seen latches for S0 and S1.
- Reassigned LED18-33 to report current bus idle levels, low-level activity, and SDA output-enable activity.
- No I2C address, ROM table, reset polarity, clock, framebuffer, DDR, debayer, or HDMI logic was changed in this step.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`) |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 CSI pixel reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C controller reset released |
| LED23 | K6 | `dbg_fb0_ready` | S0 I2C SCL current high |
| LED24 | J3 | `dbg_fb0_underflow` | S0 I2C SDA current high |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 camera reset released under vendor polarity |
| LED26 | K4 | `dbg_bridge_active` | S0 I2C SCL low was seen |
| LED27 | K3 | `dbg_bridge_under` | S0 I2C SDA low was seen |
| LED28 | M5 | `dbg_video_ready` | S0 I2C SDA output-enable active was seen |
| LED29 | M6 | `dbg_input_stable` | S1 I2C SCL current high |
| LED30 | N7 | `dbg_led30` | S1 I2C SDA current high |
| LED31 | P7 | `dbg_led31` | S1 I2C SCL low was seen |
| LED32 | P6 | `dbg_led32` | S1 I2C SDA low was seen |
| LED33 | R6 | `dbg_led33` | S1 I2C SDA output-enable active was seen |

### How to read the next board result

- Normal I2C idle after the sequence should have LED23/24 and LED29/30 ON. If either pair is OFF after boot, that bus is not idling high and the pullup/OE/electrical path is suspect.
- LED26/27 and LED31/32 should turn ON at least once if the master is really pulling SCL/SDA low. If they stay OFF while the controller reset is released, the output-enable/open-drain path is suspect.
- LED28 and LED33 should turn ON if the master actively drove SDA during transactions. If SDA low is seen but SDA OE is not, the low level likely came from the board/target side, not from the master drive request.
- If both buses idle high and the master drive-low events are present, but address NACK persists in a later ACK map, then the focus should move to sensor address, sensor power/reset sequencing, or camera-module presence.

---

## 32. 2026-07-08 twenty-fourth board feedback and LCD-power/I2C ACK retest (Codex)

### Board feedback from user

- LEDs ON: LED18 through LED33.
- Timing screenshot:
  - WNS: `+1.742 ns`
  - WHS: `+0.027 ns`
  - Notable reported clocks: `axi0_ACLK 306.937 MHz`, `i_fb_clk 270.709 MHz`, `mipi_clk 227.015 MHz`, `i_sysclk_div2 191.241 MHz`, `hdmi_tx_slow_clk 275.938 MHz`, `mipi_rx_ck0_CLKOUT 168.237 MHz`, `mipi_rx_ck1_CLKOUT 394.789 MHz`.

### Interpretation of previous I2C physical/OE probe

Previous map meanings:

- LED23/24 ON: S0 SCL/SDA currently idle high.
- LED26/27 ON: S0 SCL/SDA were pulled low at least once.
- LED28 ON: S0 SDA output-enable became active at least once.
- LED29/30 ON: S1 SCL/SDA currently idle high.
- LED31/32 ON: S1 SCL/SDA were pulled low at least once.
- LED33 ON: S1 SDA output-enable became active at least once.
- WNS/WHS are positive.

Conclusion: after enabling weak pullups on S0, the I2C physical-level/OE path looks plausible on both buses: the buses can idle high, the master can drive low, and SDA OE activity is visible. The next suspect is not the basic open-drain/OE path; it is sensor address, sensor power/reset sequencing, or camera-module presence.

### Reference check before next change

The current HDMI-only project had tied off LCD power/reset pins:

- `P0_lcd_power_en = 1'b0`
- `P0_lcd_rstp = 1'b0`
- `P1_lcd_power_en = 1'b0`
- `P1_o_lcd_rstn = 1'b0`

The vendor `dsi_tx_top.v` drives `LCD_POWER = 1'b1` and `LCD_RST_P = ~rst_n`. In the vendor top, these signals are connected to the P0/P1 LCD pins through the DSI instances. Because the current failure is camera address NACK and the hardware may share module power rails or connector-side enables, the next lowest-risk functional test is to keep DSI TX lanes tied off but set both LCD power enables high.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Changed the HDMI-only DSI tie-off block:
  - `P0_lcd_power_en`: `1'b0` -> `1'b1`
  - `P1_lcd_power_en`: `1'b0` -> `1'b1`
  - Kept `P0_lcd_rstp = 1'b0`
  - Kept `P1_o_lcd_rstn = 1'b0`
  - Kept all DSI TX data/clock lane OEs disabled/Hi-Z as before.
- Reassigned LED18-33 to combine I2C ACK status with bus-idle and power-enable checks.
- No I2C address, ROM table, reset polarity, I2C core, framebuffer, DDR, debayer, or HDMI data-path logic was changed.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`) |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 CSI pixel reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C controller reset released |
| LED23 | K6 | `dbg_fb0_ready` | S0 I2C SCL/SDA currently both high |
| LED24 | J3 | `dbg_fb0_underflow` | S1 I2C SCL/SDA currently both high |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 camera reset released under vendor polarity |
| LED26 | K4 | `dbg_bridge_active` | ch0 I2C aggregate NACK seen, `1=BAD` |
| LED27 | K3 | `dbg_bridge_under` | ch0 first NACK was after device-address byte, `1=BAD` |
| LED28 | M5 | `dbg_video_ready` | P0/P1 LCD power enables are high |
| LED29 | M6 | `dbg_input_stable` | ch1 I2C status register sampled |
| LED30 | N7 | `dbg_led30` | ch1 I2C aggregate NACK seen, `1=BAD` |
| LED31 | P7 | `dbg_led31` | ch1 first NACK was after device-address byte, `1=BAD` |
| LED32 | P6 | `dbg_led32` | ch1 I2C stream-on ROM index reached |
| LED33 | R6 | `dbg_led33` | ch1 SDA output-enable active was seen |

### How to read the next board result

- If LED26/27 and LED30/31 turn OFF while LED23/24 stay ON, the LCD power-enable tie-off was likely keeping the camera side from acknowledging.
- If LED26/27 and LED30/31 remain ON, both channels still NACK at the device-address byte even with the power enables high; next step should be a controlled address-candidate test or a camera reset/power sequencing test.
- If LED23 or LED24 turns OFF, re-check bus idle level because the power-enable change affected the I2C electrical state.
- LED28 should be ON in this build; OFF would mean the new power-enable test was not actually in the programmed image.

---

## 33. 2026-07-08 twenty-fifth board feedback: LCD-power ACK retest still NACKs (Codex)

### Board feedback from user

- LEDs ON: LED18 through LED33.
- Timing screenshot:
  - WNS: `+1.646 ns`
  - WHS: `+0.026 ns`
  - Notable reported clocks: `axi0_ACLK 298.151 MHz`, `i_fb_clk 264.061 MHz`, `mipi_clk 245.339 MHz`, `i_sysclk_div2 157.505 MHz`, `hdmi_tx_slow_clk 315.358 MHz`, `mipi_rx_ck0_CLKOUT 214.869 MHz`, `mipi_rx_ck1_CLKOUT 400.802 MHz`.

### Interpretation

Under the previous LCD-power/I2C ACK retest map:

- LED28 ON proves the build with `P0_lcd_power_en = 1'b1` and `P1_lcd_power_en = 1'b1` was present in the programmed image.
- LED23/24 ON means both S0 and S1 I2C buses still idled high.
- LED26/27 ON means ch0 still saw aggregate NACK and first NACK after the device-address byte.
- LED30/31 ON means ch1 still saw aggregate NACK and first NACK after the device-address byte.
- Timing slack was positive, so this result is not explained by a current WNS violation.

Conclusion: enabling the LCD power pins while keeping DSI TX lanes tied off did not make either sensor acknowledge the current `8'h60` write address. The direct blocker remains upstream camera I2C ACK, not DDR, debayer, framebuffer, or HDMI restoration.

### Next action

Run a controlled address-candidate comparison in one bitstream:

- ch0 remains baseline `I2C_DEVICE_ADDR = 8'h60`.
- ch1 uses candidate `I2C_DEVICE_ADDR = 8'h6c`.
- LCD power enables remain high.
- LEDs compare NACK status for baseline and candidate side by side.

---

## 34. 2026-07-08 twenty-sixth iteration: ch0 8'h60 vs ch1 8'h6c I2C address comparison (Codex)

### RTL changes

`final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v`

- Added top-level parameter `I2C_DEVICE_ADDR`, defaulting to `8'h60`.
- Passed that parameter into `i2c_master_ctrl_top`.

`final_project/fpga/rtl/top/top.v`

- Added:
  - `CH0_I2C_DEVICE_ADDR = 8'h60`
  - `CH1_I2C_DEVICE_ADDR = 8'h6c`
- Instantiated ch0 `soft_mipi_rx_top` with `8'h60`.
- Instantiated ch1 `soft_mipi_rx_top` with `8'h6c`.
- Kept `P0_lcd_power_en = 1'b1` and `P1_lcd_power_en = 1'b1`.
- Kept DSI TX lanes tied off / Hi-Z.
- Reassigned LED28 as the candidate-build marker instead of the LCD-power marker.

No ROM table, I2C core state machine, reset polarity, CSI receiver, framebuffer, debayer, DDR, or HDMI data-path logic was changed in this step.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`) |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 CSI pixel reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C controller reset released |
| LED23 | K6 | `dbg_fb0_ready` | S0 I2C SCL/SDA currently both high |
| LED24 | J3 | `dbg_fb0_underflow` | S1 I2C SCL/SDA currently both high |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 camera reset released under vendor polarity |
| LED26 | K4 | `dbg_bridge_active` | ch0 baseline `8'h60` aggregate NACK seen, `1=BAD` |
| LED27 | K3 | `dbg_bridge_under` | ch0 baseline `8'h60` first NACK after device-address byte, `1=BAD` |
| LED28 | M5 | `dbg_video_ready` | candidate marker: ch1 is using `8'h6c` in this build |
| LED29 | M6 | `dbg_input_stable` | ch1 I2C status register sampled |
| LED30 | N7 | `dbg_led30` | ch1 candidate `8'h6c` aggregate NACK seen, `1=BAD` |
| LED31 | P7 | `dbg_led31` | ch1 candidate `8'h6c` first NACK after device-address byte, `1=BAD` |
| LED32 | P6 | `dbg_led32` | ch1 I2C stream-on ROM index reached |
| LED33 | R6 | `dbg_led33` | ch1 SDA output-enable active was seen |

### How to read the next board result

- If LED26/27 stay ON but LED30/31 turn OFF, `8'h6c` is the better candidate address for the responding module on ch1; next step is to apply the candidate to both channels or test the paired read/write address convention.
- If LED26/27 turn OFF but LED30/31 stay ON, the original `8'h60` path can ACK while the candidate fails; return to `8'h60` and continue with reset/power sequencing or MIPI stream checks.
- If LED26/27 and LED30/31 all stay ON, neither `8'h60` nor `8'h6c` is being acknowledged. The next branch should be sensor reset/power sequencing or physical camera-module presence/address confirmation.
- If LED23 or LED24 turns OFF, re-check I2C idle level because the bus is no longer electrically idle-high.
- LED28 should be ON in this build; OFF means the candidate-address image is not the one currently programmed.

---

## 35. 2026-07-08 twenty-seventh board feedback: 8'h60 vs 8'h6c both still NACK (Codex)

### Board feedback from user

- LEDs ON: LED18 through LED33.
- Timing screenshot:
  - WNS: `+1.847 ns`
  - WHS: `+0.026 ns`
  - Notable reported clocks: `axi0_ACLK 317.158 MHz`, `i_fb_clk 240.038 MHz`, `mipi_clk 217.155 MHz`, `i_sysclk_div2 134.571 MHz`, `hdmi_tx_slow_clk 263.992 MHz`, `mipi_rx_ck0_CLKOUT 223.264 MHz`, `mipi_rx_ck1_CLKOUT 389.105 MHz`.

### Interpretation

Under the address-candidate comparison map:

- LED28 ON proves the bitstream is the candidate-address comparison build.
- LED26/27 ON means ch0 baseline `8'h60` still sees aggregate NACK and first NACK after the device-address byte.
- LED30/31 ON means ch1 candidate `8'h6c` also sees aggregate NACK and first NACK after the device-address byte.
- LED23/24 ON means both I2C buses are still idle-high at the sampled point.
- LED29/32/33 ON means the ch1 I2C status path ran, the stream-on index was reached, and SDA OE activity was visible.
- Timing slack remains positive.

Conclusion: neither the vendor/default `8'h60` write address nor the `8'h6c` candidate is acknowledged in this build. The next variable should not be another downstream video probe. The next controlled probe is reset/power sequencing: delay I2C start longer after camera reset release, then re-check address ACK.

### Reference check

Vendor reference files:

- `赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/src/mipi_csi/soft_mipi_rx_top.v` uses `i2c_rst_cnt[12]` under `mipi_clk`, same short-delay style as the current migrated `soft_mipi_rx_top`.
- `赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/src/mipi_csi/mipi_csi_top.sv` uses `CLK_5M` and `i2c_rst_cnt[15]`, which is roughly a millisecond-scale delay.

At the reported `mipi_clk = 217.155 MHz`, the old `2^12` delay is only about `18.9 us`. That may be too short for the camera module to finish reset/power stabilization before the I2C master sends the first address byte.

---

## 36. 2026-07-08 twenty-eighth iteration: delayed I2C-start ACK retest (Codex)

### RTL changes

`final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v`

- Added parameter `I2C_RST_DELAY_BIT`, default `22`.
- Changed `i2c_rst_cnt` from fixed `[12:0]` to `[I2C_RST_DELAY_BIT:0]`.
- Changed `i2c_rst_n` from `i2c_rst_cnt[12]` to `i2c_rst_cnt[I2C_RST_DELAY_BIT]`.

`final_project/fpga/rtl/top/top.v`

- Set both channels back to the vendor/default write address:
  - `CH0_I2C_DEVICE_ADDR = 8'h60`
  - `CH1_I2C_DEVICE_ADDR = 8'h60`
- Added `CAM_I2C_RST_DELAY_BIT = 22`.
- Passed `.I2C_RST_DELAY_BIT(CAM_I2C_RST_DELAY_BIT)` into both `soft_mipi_rx_top` instances.
- Reassigned LED28 from the `8'h6c` candidate marker to ch1 delayed I2C reset release.

At the last reported `mipi_clk = 217.155 MHz`, `2^22` cycles is about `19.3 ms`; at lower/higher observed mipi clocks it remains millisecond-scale. This is intentionally a sequencing probe, not a throughput/video-path change.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`) |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 CSI pixel reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C controller reset released after delayed start |
| LED23 | K6 | `dbg_fb0_ready` | S0 I2C SCL/SDA currently both high |
| LED24 | J3 | `dbg_fb0_underflow` | S1 I2C SCL/SDA currently both high |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 camera reset released under vendor polarity |
| LED26 | K4 | `dbg_bridge_active` | ch0 delayed-start `8'h60` aggregate NACK seen, `1=BAD` |
| LED27 | K3 | `dbg_bridge_under` | ch0 delayed-start `8'h60` first NACK after device-address byte, `1=BAD` |
| LED28 | M5 | `dbg_video_ready` | ch1 I2C controller reset released after delayed start |
| LED29 | M6 | `dbg_input_stable` | ch1 I2C status register sampled |
| LED30 | N7 | `dbg_led30` | ch1 delayed-start `8'h60` aggregate NACK seen, `1=BAD` |
| LED31 | P7 | `dbg_led31` | ch1 delayed-start `8'h60` first NACK after device-address byte, `1=BAD` |
| LED32 | P6 | `dbg_led32` | ch1 I2C stream-on ROM index reached |
| LED33 | R6 | `dbg_led33` | ch1 SDA output-enable active was seen |

### How to read the next board result

- Immediately after programming, LED22 and LED28 may stay OFF briefly during the longer I2C reset delay. After about 20 ms they should turn ON.
- If LED22/28 are OFF after a normal wait, the delayed-start counter or reset path is not releasing I2C.
- If LED26/27 and LED30/31 turn OFF while LED22/28/29/32/33 are ON, the short reset-to-I2C delay was the ACK blocker.
- If LED26/27 and LED30/31 remain ON, delayed I2C start did not solve address ACK; the next branch should test camera reset hold/release timing itself or perform a physical/module presence check.
- If LED23 or LED24 turns OFF, re-check bus idle-high/pullup state before interpreting ACK.

---

## 37. 2026-07-08 twenty-ninth board feedback: delayed I2C start fixes ch0 address ACK (Codex)

### Board feedback from user

- LEDs OFF: LED26 and LED27 only.
- All other LEDs in LED18-33 are ON.
- Timing screenshot:
  - WNS: `+1.889 ns`
  - WHS: `+0.026 ns`
  - Notable reported clocks: `axi0_ACLK 321.440 MHz`, `i_fb_clk 289.017 MHz`, `mipi_clk 240.616 MHz`, `i_sysclk_div2 177.336 MHz`, `hdmi_tx_slow_clk 285.959 MHz`, `mipi_rx_ck0_CLKOUT 218.436 MHz`, `mipi_rx_ck1_CLKOUT 382.702 MHz`.

### Interpretation

Under the delayed I2C-start ACK retest map:

- LED22 ON: ch0 delayed I2C controller reset released.
- LED26 OFF: ch0 no longer sees aggregate NACK.
- LED27 OFF: ch0 no longer sees first NACK after the device-address byte.
- LED28 ON: ch1 delayed I2C controller reset released.
- LED30/31 ON: ch1 still sees aggregate NACK and first device-address NACK.
- LED32/33 ON: ch1 still reaches stream-on index and shows SDA OE activity despite the NACK.
- Timing slack is positive.

Conclusion: the short reset-to-I2C delay was a real blocker for ch0. Extending the I2C start delay to `I2C_RST_DELAY_BIT = 22` lets ch0 acknowledge the vendor/default `8'h60` address. The next debug target should move from I2C address ACK to the post-ACK ch0 video receive path: MIPI D-PHY activity, CSI packet decode, RAW10/4ppc detection, and then framebuffer/HDMI readiness.

### Important branch decision

Do not spend the next iteration on ch1. ch1 still NACKs, but ch0 is now the working path and is enough to continue toward the stated goal: restore a real camera picture on HDMI/PC. Keep the longer I2C delay.

---

## 38. 2026-07-08 thirtieth iteration: ch0 post-ACK MIPI/CSI probe (Codex)

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Kept the longer I2C delay and both addresses at `8'h60`.
- Reassigned LED18-33 from I2C ACK comparison to ch0 post-ACK video receive checkpoints.
- No camera ROM table, I2C core state machine, reset polarity, framebuffer, DDR, debayer, or HDMI data-path logic was changed in this step.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`) |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 delayed I2C controller reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C status register sampled |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C stream-on ROM index reached |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 I2C stream-on write seen |
| LED26 | K4 | `dbg_bridge_active` | ch0 MIPI byte clock toggled |
| LED27 | K3 | `dbg_bridge_under` | ch0 MIPI clock-lane LP activity seen |
| LED28 | M5 | `dbg_video_ready` | ch0 MIPI data-lane LP activity or toggle seen |
| LED29 | M6 | `dbg_input_stable` | ch0 MIPI HS termination seen |
| LED30 | N7 | `dbg_led30` | ch0 MIPI HS enable seen on clock and data lanes |
| LED31 | P7 | `dbg_led31` | ch0 MIPI lane FIFO became non-empty and was read |
| LED32 | P6 | `dbg_led32` | ch0 CSI RAW10/4ppc format seen |
| LED33 | R6 | `dbg_led33` | ch0 CSI VS and DE seen |

### How to read the next board result

- If LED23 is ON, ch0 I2C ACK remains fixed under the longer delay.
- If LED24/25 are ON, the ch0 init table reached the stream-on region.
- If LED26 is OFF, the ch0 MIPI byte clock is not visible after init; check sensor stream start, reset/power, or lane clock routing.
- If LED26 is ON but LED29/30 are OFF, low-power activity or byte clock exists but high-speed lane receive is not entering HS.
- If LED29/30 are ON but LED31 is OFF, the D-PHY/CSI receiver enables HS but lane FIFO is not producing readable data.
- If LED31 is ON but LED32/33 are OFF, raw lane data exists but CSI packet decode is not yielding RAW10/4ppc frames or valid VS/DE.
- If LED32/33 are ON and the picture is still wrong, the next target becomes framebuffer/debayer/pixel ordering/HDMI selection rather than camera ACK.

---

## 39. 2026-07-08 thirty-first board feedback: ch0 CSI present, only stream-on write latch missing (Codex)

### Board feedback from user

- LEDs OFF: LED25 only.
- LEDs ON: LED18-24 and LED26-33.
- Timing screenshot:
  - WNS: `+1.693 ns`
  - WHS: `+0.017 ns`
  - Notable reported clocks: `axi0_ACLK 302.389 MHz`, `i_fb_clk 294.551 MHz`, `mipi_clk 246.792 MHz`, `i_sysclk_div2 170.619 MHz`, `hdmi_tx_slow_clk 261.575 MHz`, `mipi_rx_ck0_CLKOUT 191.388 MHz`, `mipi_rx_ck1_CLKOUT 380.518 MHz`.
- Captured artifacts:
  - Picture: `assets/20260708_iter31_picture_led25_off.png`
  - Timing: `assets/20260708_iter31_timing_led25_off.png`

### Interpretation

Under the ch0 post-ACK MIPI/CSI probe map:

- LED23 ON confirms the longer I2C-start delay still lets ch0 ACK `8'h60`.
- LED24 ON means the ch0 I2C ROM reached the stream-on region.
- LED25 OFF means the `stream-on write seen` latch did not trigger. Because the downstream MIPI/CSI indicators are ON, this is treated as a probe-condition weakness rather than proof that the sensor never streamed.
- LED26-31 ON prove ch0 MIPI byte clock, LP/HS activity, lane FIFO non-empty, and lane FIFO read all happened.
- LED32 ON proves ch0 CSI RAW10/4ppc format was observed.
- LED33 ON proves ch0 CSI VS and DE were observed.
- Timing slack is positive, so the current visible fault should not be explained as a setup/hold failure first.

Conclusion: ch0 camera init/ACK and CSI receive are now far enough along that the remaining visible problem is downstream pixel/frame restoration. The screen is a stable horizontal color-noise band pattern, consistent with a line-width/stride/pixel-path mismatch or framebuffer/HDMI selection issue, not with absent camera data.

### Next action

The temporary debug value `HDMI_H_VALID = 13'd480` is now the highest-probability functional mismatch. It was introduced earlier to reduce DDR read bandwidth, but the migrated/vendor path expects `HACT/2 = 960` in this 2-pixel domain and `hdmi_top` already checks for `MAX_HRES = 960`.

---

## 40. 2026-07-08 thirty-second iteration: restore HDMI width and probe framebuffer-to-HDMI path (Codex)

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Restored `HDMI_H_VALID` from `13'd480` to `HACT >> 1` (`960` pixels in the 2-pixel framebuffer domain).
- Reassigned LED24-33 from MIPI/CSI receive probes to framebuffer and HDMI input-selection probes.
- Kept LED18-23 as the baseline DDR/ch0/I2C ACK sanity set.
- Kept the longer `CAM_I2C_RST_DELAY_BIT = 22` and ch0/ch1 I2C address `8'h60`.

No I2C ROM table, reset polarity, RAW10 truncation function, debayer wiring, or HDMI TMDS output module logic was changed in this step.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`) |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 delayed I2C controller reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C status register sampled |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 CSI RAW10/4ppc format seen |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 CSI VS and DE seen |
| LED26 | K4 | `dbg_bridge_active` | ch0 framebuffer frame_start seen |
| LED27 | K3 | `dbg_bridge_under` | ch0 framebuffer write input or AXI write start seen |
| LED28 | M5 | `dbg_video_ready` | ch0 framebuffer write-frame-done seen |
| LED29 | M6 | `dbg_input_stable` | selected read-frame-available seen |
| LED30 | N7 | `dbg_led30` | selected frame_ready held stable |
| LED31 | P7 | `dbg_led31` | selected CDC bridge active held stable |
| LED32 | P6 | `dbg_led32` | `hdmi_top` input active size matches 960x1080 |
| LED33 | R6 | `dbg_led33` | `hdmi_top` held real input video selected |

### How to read the next board result

- If LED24/25 are ON, ch0 CSI RAW10 and VS/DE remain present after restoring width.
- If LED26/27 are OFF, CSI is present but framebuffer write-side frame detection or AXI write entry is not starting.
- If LED26/27 are ON but LED28 is OFF, write starts but a full write frame is not completing.
- If LED28 is ON but LED29 is OFF, write frames complete but the read side is not seeing an available frame.
- If LED29 is ON but LED30 is OFF, read availability is intermittent or blocked by underflow/reset gating.
- If LED30 is ON but LED31 is OFF, framebuffer is ready but the 2-pixel-to-1-pixel HDMI CDC bridge is not continuously active.
- If LED31 is ON but LED32 is OFF, HDMI receives activity but active-size/timing does not match the expected 960x1080 input.
- If LED32 is ON but LED33 is OFF, HDMI timing is valid but `hdmi_top` has not held the real input path selected.
- If LED24-33 are all ON and the screen still shows color-noise bands, the next target is pixel ordering / RAW10-to-RAW8 truncation / Bayer phase / debayer input ordering rather than camera ACK, CSI, or HDMI fallback selection.

---

## 41. 2026-07-08 thirty-third board feedback: width restored but CSI/framebuffer probe drops upstream indicators (Codex)

### Board feedback from user

- LEDs ON: LED18-23 and LED31.
- LEDs OFF: LED24-30 and LED32-33.
- Timing screenshot:
  - WNS: `+1.422 ns`
  - WHS: `+0.026 ns`
  - Notable reported clocks: `axi0_ACLK 279.486 MHz`, `i_fb_clk 285.796 MHz`, `mipi_clk 243.843 MHz`, `i_sysclk_div2 165.590 MHz`, `hdmi_tx_slow_clk 258.866 MHz`, `mipi_rx_ck0_CLKOUT 169.405 MHz`, `mipi_rx_ck1_CLKOUT 394.633 MHz`.
- Captured artifacts:
  - Picture: `assets/20260708_iter33_picture_led18_23_31.png`
  - Timing: `assets/20260708_iter33_timing_led18_23_31.png`

### Interpretation

Under the framebuffer/HDMI restore probe map:

- LED18-23 ON means DDR configured, ch0 selected, global reset released, delayed I2C reset released, I2C status sampled, and ch0 still ACKs with no aggregate NACK.
- LED24/25 OFF means the coarse ch0 CSI RAW10/VS/DE indicators did not light in this build.
- LED26-30 OFF means no framebuffer frame-start/write/read-ready/stable-ready milestones were visible to this probe set.
- LED31 ON alone means the selected 2-pixel-to-1-pixel CDC bridge reported a held active condition. Without LED24-30, this is not enough to conclude valid camera frames are feeding HDMI.
- LED32/33 OFF means `hdmi_top` did not see matching 960x1080 input size and did not hold real input video selected.
- Timing slack remains positive; this is still a functional video-chain problem, not first-order setup/hold failure.

Important observation: restoring `HDMI_H_VALID = HACT >> 1` should not directly change `soft_mipi_rx_top` CSI outputs. The LED24/25 drop therefore needs a finer probe before reverting width. The previous coarse CSI indicators may be too narrow, too dependent on reset/cross-domain sampling, or affected by whether the expected datatype/ppc combination appears at the exact sampled point.

### Next action

Keep the restored 960-pixel width. Replace LED24-33 with a CSI split probe that separately exposes:

- MIPI byte clock and lane FIFO read visibility.
- ch0 CSI datatype nonzero.
- ch0 RAW10 datatype.
- ch0 pixel-per-clock nonzero.
- ch0 4 pixels/clock.
- ch0 VS and DE independently.
- ch0 RAW10+4ppc combined.
- ch0 framebuffer frame_start.

This distinguishes "CSI genuinely disappeared" from "RAW10/4ppc condition is not matching" from "CSI exists but framebuffer does not accept it".

---

## 42. 2026-07-08 thirty-fourth iteration: CSI split probe after width restore (Codex)

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Kept `HDMI_H_VALID = HACT >> 1`.
- Reassigned LED24-33 from framebuffer/HDMI restore probe to a finer ch0 CSI split probe.
- Left LED18-23 unchanged as DDR/ch0/reset/I2C ACK sanity indicators.

No reset sequencing, I2C table, CSI receiver, framebuffer write/read logic, debayer path, or HDMI output logic was changed.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`) |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 delayed I2C controller reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C status register sampled |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 MIPI byte clock toggled |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 MIPI lane FIFO non-empty and read seen |
| LED26 | K4 | `dbg_bridge_active` | ch0 CSI datatype nonzero seen |
| LED27 | K3 | `dbg_bridge_under` | ch0 CSI RAW10 datatype seen |
| LED28 | M5 | `dbg_video_ready` | ch0 CSI pixel-per-clock nonzero seen |
| LED29 | M6 | `dbg_input_stable` | ch0 CSI 4 pixels/clock seen |
| LED30 | N7 | `dbg_led30` | ch0 CSI VS seen |
| LED31 | P7 | `dbg_led31` | ch0 CSI DE seen |
| LED32 | P6 | `dbg_led32` | ch0 CSI RAW10 and 4ppc seen together |
| LED33 | R6 | `dbg_led33` | ch0 framebuffer frame_start seen |

### How to read the next board result

- If LED24/25 are OFF, the problem is before or inside MIPI lane FIFO readout.
- If LED24/25 are ON but LED26/28 are OFF, raw lane movement exists but CSI packet decode is not producing datatype/ppc metadata.
- If LED26 is ON but LED27 is OFF, CSI metadata exists but datatype is not RAW10 (`6'h2B`).
- If LED28 is ON but LED29 is OFF, CSI metadata has a pixel-per-clock value but not the expected 4ppc.
- If LED27 and LED29 are ON but LED32 is OFF, RAW10 and 4ppc occurred separately but not together in the same sampled condition.
- If LED30 or LED31 is OFF while LED24/25 are ON, CSI data movement exists but VS/DE framing is missing.
- If LED24-32 are ON but LED33 is OFF, CSI is alive and correctly typed, but framebuffer input timing/frame detection is the next blocker.

---

## 43. 2026-07-08 thirty-fifth board feedback: CSI split probe shows no ch0 MIPI/CSI activity (Codex)

### Board feedback from user

- LEDs ON: LED18-23.
- LEDs OFF: LED24-33.
- Timing screenshot:
  - WNS: `+1.659 ns`
  - WHS: `+0.013 ns`
  - Notable reported clocks: `axi0_ACLK 299.312 MHz`, `i_fb_clk 258.398 MHz`, `mipi_clk 229.358 MHz`, `i_sysclk_div2 162.973 MHz`, `hdmi_tx_slow_clk 294.724 MHz`, `mipi_rx_ck0_CLKOUT 210.128 MHz`, `mipi_rx_ck1_CLKOUT 384.615 MHz`.
- Captured artifacts:
  - Picture: `assets/20260708_iter35_picture_led18_23_only.png`
  - Timing: `assets/20260708_iter35_timing_led18_23_only.png`

### Interpretation

Under the CSI split probe map:

- LED18-23 ON confirms DDR, ch0 selection, global reset, delayed I2C reset, I2C status sampling, and no aggregate ch0 I2C NACK.
- LED24 OFF means even the ch0 MIPI byte-clock toggle latch did not fire.
- LED25 OFF means lane FIFO non-empty/read was not observed.
- LED26-32 OFF means no ch0 CSI datatype, RAW10, pixel-per-clock, VS, or DE was observed.
- LED33 OFF means framebuffer frame_start did not occur.
- Timing slack remains positive.

Conclusion: the current on-screen color-noise bands should be treated as `hdmi_top` fallback/test output, not restored camera video. The live camera path is not producing observable ch0 MIPI/CSI data in this build, even though I2C ACK is good.

The next question is no longer pixel ordering. It is whether the sensor was actually commanded into streaming and whether the D-PHY input entered LP/HS/byte-clock activity after that command.

### Next action

Keep `HDMI_H_VALID = HACT >> 1`. Move the LED probe one step upstream:

- Prove the I2C config sequence reaches `init_done`, completes the config table, and reaches the last ROM entry.
- Prove the stream-on region and `0x0100 = 0x01` write are actually observed.
- Confirm the stream-on data byte is ACKed.
- In parallel, expose MIPI LP, HS enable/termination, and byte-clock activity.

This separates "I2C ACKs but stream-on is not written" from "stream-on is written but sensor/D-PHY does not produce MIPI".

---

## 44. 2026-07-08 thirty-sixth iteration: stream-on and D-PHY entry probe (Codex)

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Kept `HDMI_H_VALID = HACT >> 1`.
- Added held latches for ch0 I2C `init_done`, `cfg_done`, and stream-on-index-reached.
- Reassigned LED24-33 to ch0 I2C stream-on and D-PHY entry checkpoints.
- Left LED18-23 unchanged as DDR/ch0/reset/I2C ACK sanity indicators.

No I2C ROM contents, reset polarity, CSI receiver logic, framebuffer logic, or HDMI output logic was changed.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`) |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 delayed I2C controller reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C status register sampled |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C `init_done` seen |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 I2C config table completed |
| LED26 | K4 | `dbg_bridge_active` | ch0 I2C last ROM index write seen |
| LED27 | K3 | `dbg_bridge_under` | ch0 I2C stream-on index region reached |
| LED28 | M5 | `dbg_video_ready` | ch0 I2C `0x0100=0x01` stream-on write seen |
| LED29 | M6 | `dbg_input_stable` | ch0 I2C data-byte ACK success |
| LED30 | N7 | `dbg_led30` | ch0 MIPI clock-lane LP activity seen |
| LED31 | P7 | `dbg_led31` | ch0 MIPI data-lane LP activity/toggle seen |
| LED32 | P6 | `dbg_led32` | ch0 MIPI HS enable/termination seen |
| LED33 | R6 | `dbg_led33` | ch0 MIPI byte clock toggled |

### How to read the next board result

- If LED24/25 are OFF, the I2C init/config sequencer did not finish even though ACK looked good.
- If LED25 is ON but LED27/28 are OFF, the table finishes or advances but the stream-on marker condition is not being observed.
- If LED27 is ON but LED28 is OFF, the stream-on region is reached but the exact `0x0100=0x01` write condition is not matching the ROM/probe.
- If LED28 is ON but LED29 is OFF, stream-on write was attempted but the data byte NACKed.
- If LED28/29 are ON and LED30-33 are OFF, the sensor accepted stream-on but the D-PHY/MIPI output did not start or is not connected to ch0.
- If LED30/31 are ON but LED32/33 are OFF, LP activity exists but HS/byte-clock entry is missing.
- If LED30-33 are ON, return downstream to CSI packet decode/framebuffer.

---

## 45. 2026-07-08 thirty-seventh board feedback: D-PHY is active, exact stream-on probe is too narrow (Codex)

### Board feedback from user

- LEDs OFF: LED28 only.
- All other LEDs in LED18-33 are ON.
- Timing screenshot:
  - WNS: `+1.776 ns`
  - WHS: `+0.017 ns`
  - Notable reported clocks: `axi0_ACLK 310.174 MHz`, `i_fb_clk 252.781 MHz`, `mipi_clk 233.263 MHz`, `i_sysclk_div2 173.280 MHz`, `hdmi_tx_slow_clk 277.393 MHz`, `mipi_rx_ck0_CLKOUT 156.912 MHz`, `mipi_rx_ck1_CLKOUT 393.391 MHz`.
- Captured artifacts:
  - Picture: `assets/20260708_iter37_picture_led28_off.png`
  - Timing: `assets/20260708_iter37_timing_led28_off.png`

### Interpretation

Under the stream-on and D-PHY entry probe map:

- LED24/25/26/27 ON prove the ch0 I2C sequence reached `init_done`, completed the config table, hit the last ROM/index indicators, and reached the stream-on region.
- LED28 OFF means the exact probe condition `(addr == 16'h0100) && (dout == 8'h01) && wr_en` did not trigger. Because downstream D-PHY LEDs are ON, this is now treated as a narrow/inexact stream-on probe condition, not proof that streaming failed.
- LED29 ON means the sampled data-byte ACK condition is good.
- LED30/31/32/33 ON prove ch0 MIPI LP activity, HS enable/termination activity, and byte-clock toggling are present.
- Timing slack remains positive.

Conclusion: the current blocker is no longer I2C ACK, nor total absence of MIPI. The sensor/D-PHY side is active. The next target is the boundary between D-PHY lane FIFO readout and CSI packet decode, then framebuffer frame_start.

### Next action

Keep the restored 960-pixel width and move the LED probe downstream again:

- Keep I2C config completion as a sanity bit.
- Expose MIPI byte clock and HS.
- Split MIPI lane FIFO non-empty from lane FIFO read.
- Expose CSI datatype nonzero, RAW10 datatype, 4ppc, and VS/DE.
- Expose framebuffer frame_start.

This should distinguish "D-PHY has HS/byteclk but FIFO is not being read" from "FIFO read exists but CSI packet metadata is wrong" from "CSI is valid but framebuffer does not start".

---

## 46. 2026-07-08 thirty-eighth iteration: D-PHY to CSI decode probe (Codex)

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Kept `HDMI_H_VALID = HACT >> 1`.
- Reassigned LED24-33 to D-PHY FIFO, CSI metadata, VS/DE, and framebuffer frame_start checkpoints.
- Left LED18-23 unchanged as DDR/ch0/reset/I2C ACK sanity indicators.

No I2C sequence, reset polarity, CSI receiver logic, framebuffer logic, or HDMI output logic was changed.

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`) |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 delayed I2C controller reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C status register sampled |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C config table completed |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 MIPI byte clock toggled |
| LED26 | K4 | `dbg_bridge_active` | ch0 MIPI HS enable/termination seen |
| LED27 | K3 | `dbg_bridge_under` | ch0 MIPI lane FIFO non-empty seen |
| LED28 | M5 | `dbg_video_ready` | ch0 MIPI lane FIFO read seen |
| LED29 | M6 | `dbg_input_stable` | ch0 CSI datatype nonzero seen |
| LED30 | N7 | `dbg_led30` | ch0 CSI RAW10 datatype seen |
| LED31 | P7 | `dbg_led31` | ch0 CSI 4 pixels/clock seen |
| LED32 | P6 | `dbg_led32` | ch0 CSI VS and DE seen |
| LED33 | R6 | `dbg_led33` | ch0 framebuffer frame_start seen |

### How to read the next board result

- If LED25/26 are ON but LED27/28 are OFF, D-PHY enters HS/byteclk but lane FIFO is not producing readable data.
- If LED27 is ON but LED28 is OFF, FIFO sees data but the CSI controller is not reading it.
- If LED28 is ON but LED29 is OFF, lane FIFO is read but CSI packet decode does not produce datatype metadata.
- If LED29 is ON but LED30 is OFF, packets decode but datatype is not RAW10.
- If LED30 is ON but LED31 is OFF, RAW10 exists but ppc is not the expected 4.
- If LED30/31 are ON but LED32 is OFF, RAW10/4ppc metadata exists but VS/DE frame signals are not generated.
- If LED32 is ON but LED33 is OFF, CSI frame exists but framebuffer input frame detection is the next blocker.

---

## 47. 2026-07-08 thirty-ninth board feedback: D-PHY activity is unstable across builds/runs (Codex)

### Board feedback from user

- LEDs ON: LED18-24.
- LEDs OFF: LED25-33.
- Timing screenshot:
  - WNS: `+1.852 ns`
  - WHS: `+0.022 ns`
  - Notable reported clocks: `axi0_ACLK 317.662 MHz`, `i_fb_clk 248.077 MHz`, `mipi_clk 222.469 MHz`, `i_sysclk_div2 149.053 MHz`, `hdmi_tx_slow_clk 282.087 MHz`, `mipi_rx_ck0_CLKOUT 175.070 MHz`, `mipi_rx_ck1_CLKOUT 382.702 MHz`.
- Captured artifacts:
  - Picture: `assets/20260708_iter39_picture_led18_24_only.png`
  - Timing: `assets/20260708_iter39_timing_led18_24_only.png`

### Interpretation

Under the D-PHY to CSI decode probe map:

- LED18-24 ON means DDR/ch0/reset/I2C ACK are still good and the ch0 I2C config table completed.
- LED25-33 OFF means this run did not observe ch0 MIPI byte clock, HS, lane FIFO, CSI metadata, VS/DE, or framebuffer frame_start.
- This conflicts with the immediately previous run where D-PHY LP/HS/byteclk indicators were ON. The more likely current issue is unstable sensor stream start / D-PHY entry, not a fixed downstream CSI/framebuffer-only fault.
- Timing slack remains positive.

### Reference check

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_reg_rom.v` matches the vendor demo ordering around stream start:

- `ROM[8'h90] = {16'h0100,8'h01,1'b0};`
- `ROM[8'h91]` through `ROM[8'ha0]` continue writing `320x` window/crop registers.

Because the vendor demo has the same order, this is not proven to be a migration bug. But with the current intermittent D-PHY evidence, a minimally invasive retry is useful: repeat `0x0100=0x01` once after the final window/crop register to test whether stream start becomes stable.

---

## 48. 2026-07-08 fortieth iteration: tail stream-on retry probe (Codex)

### RTL changes

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_ctrl_top.v`

- Changed default `DATA_LENGTH` from `161` to `162`.

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_reg_rom.v`

- Added `ROM[8'ha1] = {16'h0100,8'h01,1'b0};` as a second stream-on write after the final `320x` window/crop register.
- Existing vendor-order register writes were not removed or reordered.

`final_project/fpga/rtl/top/top.v`

- Reassigned LED24-33 to verify the tail stream-on retry and MIPI/CSI entry:
  - config table completion
  - stream-on write seen
  - last ROM index write seen
  - data-byte ACK
  - byte clock / HS / FIFO / CSI metadata / VS+DE

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`) |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 delayed I2C controller reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C status register sampled |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C config table completed |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 I2C `0x0100=0x01` stream-on write seen |
| LED26 | K4 | `dbg_bridge_active` | ch0 I2C last ROM index write seen |
| LED27 | K3 | `dbg_bridge_under` | ch0 I2C data-byte ACK success |
| LED28 | M5 | `dbg_video_ready` | ch0 MIPI byte clock toggled |
| LED29 | M6 | `dbg_input_stable` | ch0 MIPI HS enable/termination seen |
| LED30 | N7 | `dbg_led30` | ch0 MIPI lane FIFO non-empty seen |
| LED31 | P7 | `dbg_led31` | ch0 MIPI lane FIFO read seen |
| LED32 | P6 | `dbg_led32` | ch0 CSI datatype nonzero seen |
| LED33 | R6 | `dbg_led33` | ch0 CSI VS and DE seen |

### How to read the next board result

- If LED25/26 are ON, the second stream-on write was reached/executed at the table tail.
- If LED25 is OFF but LED26 is ON, the exact stream-on detector still does not match; inspect the detector rather than the whole I2C sequence.
- If LED25-27 are ON and LED28-33 are OFF, the sensor accepts the tail stream-on sequence but D-PHY still does not start.
- If LED28/29 become stable ON across repeated builds, the tail stream-on retry improved D-PHY start stability.
- If LED28-31 are ON but LED32/33 are OFF, move back to CSI packet decode.

### Handoff status before manual Efinity build

- `git diff --check` was run after the tail stream-on retry patch; it passed with only Git CRLF/LF conversion warnings.
- `i2c_master_reg_rom.v` trailing whitespace at the final `endmodule` line was removed. This was formatting only and did not change the register sequence.
- Next manual build/download should be made from `D:\final_project` after syncing these files:
  - `fpga/rtl/top/top.v`
  - `fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_ctrl_top.v`
  - `fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_reg_rom.v`
  - `docs/debug_sessions/hdmi_stripe_debug_20260707.md`
- The requested board result for this next image is the full LED18-33 state plus WNS/WHS and whether the HDMI picture changes.

---

## 49. 2026-07-08 forty-first board feedback: CSI reaches VS/DE but HDMI still shows fallback-like stripes (Codex)

### Board feedback from user

- LEDs ON: LED18-25 and LED27-33.
- LEDs OFF: LED26 only.
- Timing screenshot:
  - WNS: `+1.54 ns`
  - WHS: `+0.017 ns`
  - Notable reported clocks: `axi0_ACLK 289.017 MHz`, `i_fb_clk 245.821 MHz`, `mipi_clk 220.313 MHz`, `i_sysclk_div2 171.292 MHz`, `hdmi_tx_slow_clk 267.451 MHz`, `mipi_rx_ck0_CLKOUT 221.435 MHz`, `mipi_rx_ck1_CLKOUT 391.236 MHz`.
- Captured artifacts:
  - Picture: `assets/20260708_iter41_picture_led26_off.png`
  - Timing: `assets/20260708_iter41_timing_led26_off.png`

### Interpretation

Under the tail stream-on retry probe map:

- LED25 ON proves the `0x0100=0x01` stream-on write was observed.
- LED28-33 ON proves ch0 MIPI byte clock, HS/termination, lane FIFO activity, CSI datatype nonzero, and CSI VS+DE were all observed.
- LED26 OFF is now treated as a probe-quality issue, not a chain failure: it was driven by a single-cycle `cnt == LAST_INDEX && wr_en` condition that could be missed when sampled into the HDMI/debug clock domain. Because LED24/25/27 and the MIPI/CSI LEDs were ON, the table did not fail at the tail.
- The picture remains the same horizontal noisy/color-bar-like screen. The next blocker is likely after CSI: framebuffer write, DDR frame completion/readback, HDMI input stability gate, or the fallback-vs-input selection in `hdmi_top`.

---

## 50. 2026-07-08 forty-second iteration: post-CSI framebuffer/HDMI LED probe (Codex)

### RTL changes

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_reg_set.v`

- Converted I2C debug outputs for last-index and stream-on detection from single-cycle combinational pulses into sticky source-clock latches:
  - `dbg_last_index_seen`
  - `dbg_stream_on_index_reached`
  - `dbg_stream_on_seen`
- This is diagnostic only. The I2C state machine and register write order are unchanged.

`final_project/fpga/rtl/top/top.v`

- Kept LED18-26 as front-end/I2C confirmation.
- Reassigned LED27-33 to locate the post-CSI blocker:
  - CSI format
  - framebuffer input frame start
  - framebuffer write FIFO acceptance
  - DDR write frame completion
  - DDR read data
  - HDMI input data activity
  - HDMI input-video selection

### Next LED18-33 map

| LED | Pin | RTL signal | Meaning |
|-----|-----|------------|---------|
| LED18 | B2 | `led[0]` | DDR configured |
| LED19 | E3 | `led[1]` | ch0 selected (`~channel_sel`) |
| LED20 | F3 | `led[2]` | global reset released: `arst_n` |
| LED21 | F2 | `led[3]` | ch0 delayed I2C controller reset released |
| LED22 | G2 | `dbg_ddr_ok` | ch0 I2C status register sampled |
| LED23 | K6 | `dbg_fb0_ready` | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | `dbg_fb0_underflow` | ch0 I2C config table completed |
| LED25 | L6 | `dbg_csi_fmt_ok` | ch0 I2C `0x0100=0x01` stream-on write seen |
| LED26 | K4 | `dbg_bridge_active` | ch0 I2C last ROM index write seen, now sticky |
| LED27 | K3 | `dbg_bridge_under` | ch0 CSI RAW10/4ppc format seen |
| LED28 | M5 | `dbg_video_ready` | ch0 framebuffer input frame_start seen |
| LED29 | M6 | `dbg_input_stable` | ch0 framebuffer write FIFO accepts pixels |
| LED30 | N7 | `dbg_led30` | ch0 DDR write frame done seen |
| LED31 | P7 | `dbg_led31` | ch0 DDR read path produced data |
| LED32 | P6 | `dbg_led32` | `hdmi_top` input pixel data changes under DE |
| LED33 | R6 | `dbg_led33` | `hdmi_top` is using the input video path, held stable |

### How to read the next board result

- If LED27 is OFF, CSI metadata is not the expected RAW10/4ppc even though generic CSI activity may exist.
- If LED27 is ON but LED28 is OFF, CSI outputs do not produce a framebuffer frame start.
- If LED28 is ON but LED29 is OFF, framebuffer frame detection occurs but write FIFO is not accepting pixels.
- If LED29 is ON but LED30 is OFF, DDR write-frame completion is failing.
- If LED30 is ON but LED31 is OFF, DDR readback path is failing.
- If LED31/32 are ON but LED33 is OFF, the HDMI output is still using its fallback pattern because the input-stability gate is not satisfied.
- If LED33 is ON and the picture is still wrong, the HDMI path is using input video and the remaining issue is pixel packing/color interpretation or upstream image data content rather than fallback selection.

---

## 51. 2026-07-08 forty-third board feedback: framebuffer and HDMI input data are alive, fallback gate is blocking (Codex)

### Board feedback from user

- LEDs ON: LED18-32.
- LEDs OFF: LED33 only.
- Timing screenshot:
  - WNS: `+1.624 ns`
  - WHS: `+0.026 ns`
  - Notable reported clocks: `axi0_ACLK 296.209 MHz`, `i_fb_clk 254.907 MHz`, `mipi_clk 233.973 MHz`, `i_sysclk_div2 156.838 MHz`, `hdmi_tx_slow_clk 239.693 MHz`, `mipi_rx_ck0_CLKOUT 178.348 MHz`, `mipi_rx_ck1_CLKOUT 381.534 MHz`.
- Captured artifacts:
  - Picture: `assets/20260708_iter43_picture_led33_off.png`
  - Timing: `assets/20260708_iter43_timing_led33_off.png`

### Interpretation

Under the post-CSI framebuffer/HDMI probe map:

- LED27 ON: ch0 CSI RAW10/4ppc format was seen.
- LED28 ON: framebuffer input frame_start was seen.
- LED29 ON: framebuffer write FIFO accepted pixels.
- LED30 ON: DDR write-frame completion was seen.
- LED31 ON: DDR read path produced data.
- LED32 ON: `hdmi_top` saw input pixel data changing under DE.
- LED33 OFF: `hdmi_top` did not switch from fallback pattern to input video.

This narrows the current blocker to the HDMI fallback/input-stability gate. The camera-to-DDR-to-HDMI-input data path is alive enough to deliver changing pixels to `hdmi_top`, but `USE_INPUT_STABLE_GATE` prevents the encoder from using them because one or more timing qualification terms did not pass.

---

## 52. 2026-07-08 forty-fourth iteration: force HDMI input and expose gate errors (Codex)

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Changed the `hdmi_top` instance parameter:
  - `USE_INPUT_STABLE_GATE` from `1'b1` to `1'b0`.
- This is a controlled diagnostic bypass. It should force the HDMI encoder to use the selected input video whenever `i_video_ready` is true, instead of continuing to show fallback until the input-stability gate passes.
- Added sticky latches for `hdmi_top` timing error outputs:
  - `h_active_error`
  - `v_active_error`
  - `v_total_error`
  - `h_total_error`
  - `h_sync_error`
- Reassigned LED24-33 to expose the HDMI gate terms while the input path is forced through.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 delayed I2C controller reset released |
| LED22 | G2 | ch0 I2C status register sampled |
| LED23 | K6 | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | `hdmi_top` video_path_ready |
| LED25 | L6 | `hdmi_top` vid_info_det frame_stable |
| LED26 | K4 | `hdmi_top` active size is 960x1080 |
| LED27 | K3 | BAD: HDMI h_active_error seen |
| LED28 | M5 | BAD: HDMI v_active_error seen |
| LED29 | M6 | BAD: HDMI v_total_error seen |
| LED30 | N7 | BAD: HDMI h_total_error seen |
| LED31 | P7 | BAD: HDMI h_sync_error seen |
| LED32 | P6 | `hdmi_top` input pixel data changes under DE |
| LED33 | R6 | `hdmi_top` is using input video, with stability gate bypassed |

### How to read the next board result

- If LED33 turns ON and the screen changes away from the fallback color bars, the previous visible image was definitely fallback, and the next fix is to correct the stability/timing qualification.
- If LED33 turns ON but the screen is still noisy or wrongly colored, the pipeline is now showing real input and the next work is pixel packing / Bayer-to-RGB / frame format debugging.
- If LED33 remains OFF, then `i_video_ready` itself is not staying true at `hdmi_top` despite LED31/32 evidence, so inspect `selected_frame_ok_hdmi`, bridge underflow, and the ready synchronizer.
- LEDs LED27-31 are error LEDs: ON means that timing error was observed.

---

## 53. 2026-07-09 forty-fourth board feedback: forced-input build still lacks `video_path_ready` (Codex)

### Board feedback from user

- LEDs ON: LED18-23.
- LEDs OFF: LED24-33.
- Timing: not provided in this message.
- Picture: not provided in this message.

### Interpretation

Under the forced-input HDMI gate probe:

- LED18-23 ON means DDR/ch0/global reset/ch0 I2C status/no aggregate NACK are still good.
- LED24 OFF means `hdmi_top video_path_ready` is not true.
- In `hdmi_top`, `video_path_ready = sys_rst_n & i_video_ready`. Because HDMI reset should be local and stable after boot, the next likely blocker is upstream `i_video_ready`.
- In `top.v`, `i_video_ready` is driven by `hdmi_video_ready`, which is synchronized from:
  - `selected_frame_ok_hdmi`
  - `selected_bridge_active`
  - `~selected_bridge_underflow`

This run therefore requires a more direct split of the HDMI-ready upstream terms. The previous run already proved pixels can reach `hdmi_top`; the current forced-input build did not keep the ready qualification true.

---

## 54. 2026-07-09 forty-fifth iteration: split HDMI `i_video_ready` upstream terms (Codex)

### Planned RTL intent

Reassign LED24-33 to expose the terms that generate `hdmi_video_ready` before they enter `hdmi_top`:

- selected framebuffer ready
- selected framebuffer underflow
- selected frame_ok
- selected CDC bridge active
- selected CDC bridge underflow
- bridge FIFO level ready / low
- synchronized `hdmi_video_ready`
- selected bridge input data activity
- `hdmi_top` use-input output

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Kept `USE_INPUT_STABLE_GATE(1'b0)` so the HDMI input-stability gate remains bypassed for this diagnostic step.
- Reassigned LED24-33 to the exact upstream terms that generate `hdmi_video_ready`.
- Added a sticky latch for selected CDC bridge output data changes under DE. This lets LED32 show whether `video_2pix_to_1pix_cdc` is still producing changing pixels even if `hdmi_top` itself is not ready.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 delayed I2C controller reset released |
| LED22 | G2 | ch0 I2C status register sampled |
| LED23 | K6 | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | selected framebuffer ready, synchronized |
| LED25 | L6 | BAD: selected framebuffer underflow |
| LED26 | K4 | selected frame_ok_hdmi |
| LED27 | K3 | selected CDC bridge active |
| LED28 | M5 | selected CDC bridge level_ready |
| LED29 | M6 | BAD: selected CDC bridge level_low |
| LED30 | N7 | `hdmi_video_ready` synchronized into `hdmi_top` |
| LED31 | P7 | BAD: selected CDC bridge underflow |
| LED32 | P6 | selected CDC bridge output data changes under DE |
| LED33 | R6 | `hdmi_top` is using input video |

### How to read the next board result

- LED24 OFF: framebuffer ready is not stable.
- LED25 ON: framebuffer underflow is blocking `frame_ok`.
- LED26 OFF with LED24 ON and LED25 OFF: ready synchronization or frame_ok expression needs inspection.
- LED27 OFF: the 2-pixel to 1-pixel CDC bridge is not active.
- LED28 OFF or LED29 ON: bridge FIFO level is not sufficient.
- LED30 OFF: the combined `hdmi_video_ready` expression is not reaching `hdmi_top`.
- LED32 ON with LED30 OFF: pixels exist at the bridge output, but the ready qualification is blocking them.
- LED33 ON: `hdmi_top` is using input video.

## 55. 2026-07-09 forty-fifth board feedback: CDC bridge ready but framebuffer ready is false (Codex)

### Board feedback from user

- LEDs ON: LED18-23, LED27, LED28.
- LEDs OFF: LED24-26, LED29-33.
- Timing screenshot:
  - WNS: 1.521 ns
  - WHS: 0.026 ns
  - axi0_ACLK: 287.439 MHz
  - i_fb_clk: 226.296 MHz
  - mipi_clk: 233.536 MHz
  - i_sysclk_div2: 152.952 MHz
  - hdmi_tx_slow_clk: 270.124 MHz
  - mipi_rx_ck0_CLKOUT: 213.083 MHz
  - mipi_rx_ck1_CLKOUT: 409.500 MHz
- Picture: unchanged noisy colored block/stripe pattern captured in user screenshot.

### Interpretation

Using the LED map from iteration 54:

- LED18-23 ON confirms the basic DDR/ch0/reset/I2C path remained good.
- LED27 ON means the selected `video_2pix_to_1pix_cdc` bridge is active.
- LED28 ON means the selected CDC bridge has reached `level_ready`.
- LED29 OFF and LED31 OFF mean no current CDC low-level/underflow blocker was observed by this probe.
- LED24 OFF means `selected_frame_ready_cdc[2]` is false.
- LED25 OFF means selected framebuffer underflow is not the blocker.
- LED26 OFF follows from LED24 OFF because `selected_frame_ok_hdmi = selected_frame_ready_cdc[2] & ~selected_fifo_underflow_cdc[1]`.
- LED30 OFF and LED33 OFF follow because `hdmi_video_ready` cannot assert while `selected_frame_ok_hdmi` is false.

This narrows the current blocker to the framebuffer ready expression, not HDMI input selection and not the CDC bridge. In `frame_buffer.v`, the relevant expression is:

```verilog
assign frame_ready = frame_en & out_sync & ~fifo_rd_underflow_latched;
assign frame_en = rd_frame_en_r[1] & rd_frame_available;
```

The next probe must split `frame_stable`, write-frame completion, read-frame availability, `frame_en`, `out_sync`, and framebuffer underflow.

---

## 56. 2026-07-09 forty-sixth iteration: decompose selected framebuffer ready (Codex)

### RTL intent

Keep the HDMI input-stability gate bypassed, but move LED24-33 one level upstream from `hdmi_video_ready` to the selected `frame_buffer.frame_ready` terms.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Reassigned LED24-33 to expose the selected framebuffer readiness terms:
  - `selected_dbg_frame_stable`
  - sticky selected `wr_frame_done`
  - selected `rd_frame_available`
  - selected `frame_en`
  - `out_sync`
  - selected framebuffer underflow
  - synchronized selected `frame_ready`
  - selected FIFO read period
  - sticky selected DDR read gap
  - synchronized `hdmi_video_ready`
- No synthesis, P&R, or programming was run by Codex.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 delayed I2C controller reset released |
| LED22 | G2 | ch0 I2C status register sampled |
| LED23 | K6 | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | selected `frame_stable` |
| LED25 | L6 | selected `wr_frame_done` seen |
| LED26 | K4 | selected `rd_frame_available` |
| LED27 | K3 | selected `frame_en` |
| LED28 | M5 | `out_sync` released |
| LED29 | M6 | BAD: selected framebuffer underflow |
| LED30 | N7 | selected framebuffer ready, synchronized |
| LED31 | P7 | selected FIFO read period |
| LED32 | P6 | BAD: selected DDR read gap seen |
| LED33 | R6 | `hdmi_video_ready` synchronized into `hdmi_top` |

### How to read the next board result

- LED24 OFF: frame-size stability detector is still not happy; inspect camera/frame dimensions and `frame_info_det`.
- LED25 OFF: write frame completion is missing; inspect write-side AXI/framebuffer path.
- LED26 OFF with LED25 ON: write completion happened but read-frame availability did not cross into output clock domain.
- LED27 OFF with LED24/26 ON: `frame_en` generation is not staying true.
- LED28 OFF: startup `out_sync` delay never released, likely reset or `pixel_data_en` issue.
- LED29 ON: framebuffer underflow is blocking ready.
- LED30 OFF while LED24/27/28 ON and LED29 OFF: inspect `frame_ready` CDC/synchronization.
- LED31 ON: framebuffer read period is active.
- LED32 ON: DDR read gap was observed and may explain noisy/striped output.
- LED33 ON: HDMI ready is now true; then the next issue is image content/pixel packing.

---

## 57. 2026-07-09 forty-sixth board feedback: out_sync released but frame_stable/write path absent (Codex)

### Board feedback from user

- LEDs ON: LED18-23, LED28.
- LEDs OFF: LED24-27, LED29-33.
- Timing screenshot:
  - WNS: 1.797 ns
  - WHS: 0.026 ns
  - axi0_ACLK: 312.207 MHz
  - i_fb_clk: 269.687 MHz
  - mipi_clk: 224.669 MHz
  - i_sysclk_div2: 144.760 MHz
  - hdmi_tx_slow_clk: 270.783 MHz
  - mipi_rx_ck0_CLKOUT: 173.732 MHz
  - mipi_rx_ck1_CLKOUT: 407.664 MHz
- Picture: still noisy colored block/stripe pattern; monitor OSD shows USB3.0 UHD overlay.

### Interpretation

Using the LED map from iteration 56:

- LED18-23 ON: DDR/ch0/reset/I2C still pass the basic checks.
- LED28 ON: `out_sync` released, so the startup delay is not blocking `frame_ready`.
- LED24 OFF: selected `frame_stable` is false.
- LED25 OFF: selected write-frame completion was not seen.
- LED26 OFF: selected read-frame availability is false.
- LED27 OFF: selected `frame_en` is false.
- LED29 OFF: selected framebuffer underflow is not currently asserted.
- LED30/33 OFF follow from missing framebuffer readiness.

This points upstream of `frame_ready` into the `frame_buffer` input/write side. `frame_buffer.v` shows:

```verilog
assign wr_start = frame_start & frame_stable;
assign frame_ready = frame_en & out_sync & ~fifo_rd_underflow_latched;
assign frame_en = rd_frame_en_r[1] & rd_frame_available;
```

Because `frame_stable` is false, `wr_start` cannot occur even if CSI VS/DE exists. The next probe must split ch0 CSI/framebuffer-entry milestones: VS, DE, RAW10/4ppc, `frame_start`, write FIFO data, `wr_start`, AXI AWVALID, write-frame done, and `frame_stable`.

---

## 58. 2026-07-09 forty-seventh iteration: expose ch0 framebuffer input/write milestones (Codex)

### RTL intent

Move LED24-33 from final framebuffer-ready terms to ch0 input/write-side milestones before `frame_stable` and `wr_start`.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Reassigned LED24-33 to ch0-specific latched milestones that already exist in `top.v`/`frame_buffer.v`.
- Added two HDMI-clock-domain sticky latches for existing ch0 write-side debug outputs:
  - `dbg_ch0_wr_start_seen_hdmi`
  - `dbg_ch0_awvalid_seen_hdmi`
- Kept `USE_INPUT_STABLE_GATE(1'b0)` for diagnostic continuity.
- No synthesis, P&R, or programming was run by Codex.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 delayed I2C controller reset released |
| LED22 | G2 | ch0 I2C status register sampled |
| LED23 | K6 | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | ch0 CSI VS seen |
| LED25 | L6 | ch0 CSI DE seen |
| LED26 | K4 | ch0 CSI RAW10 and 4ppc format seen |
| LED27 | K3 | ch0 `frame_start` seen inside framebuffer |
| LED28 | M5 | ch0 framebuffer input FIFO write seen |
| LED29 | M6 | ch0 `wr_start` seen |
| LED30 | N7 | ch0 AXI AWVALID seen |
| LED31 | P7 | ch0 write-frame done seen |
| LED32 | P6 | ch0 `frame_stable` |
| LED33 | R6 | `out_sync` released |

### How to read the next board result

- LED24 OFF: CSI VS is not reaching ch0 framebuffer input.
- LED25 OFF with LED24 ON: CSI has frame sync but no active data enable.
- LED26 OFF with LED24/25 ON: datatype or pixel-per-clock no longer matches RAW10/4ppc at this probe point.
- LED27 OFF with LED24 ON: `frame_info_det` is not detecting frame_start from the VS polarity/edge.
- LED28 OFF with LED25 ON: `frame_info_det` is not producing write FIFO data from DE.
- LED29 OFF with LED27 ON: `frame_start` exists but `frame_stable` is false, so `wr_start = frame_start & frame_stable` never fires.
- LED30 OFF with LED29 ON: write start occurred but AXI AWVALID did not issue.
- LED31 OFF with LED30 ON: AXI write command started but a full frame write did not complete.
- LED32 OFF with LED24/25/27 ON: frame-length stability detector is rejecting the frame; inspect VS polarity and `frame_info_det` length history.
- LED33 OFF: startup output sync has regressed.

---

## 59. 2026-07-09 forty-seventh board feedback: no ch0 CSI/framebuffer-entry activity in this build (Codex)

### Board feedback from user

- LEDs ON: LED18-23, LED33.
- LEDs OFF: LED24-32.
- Timing screenshot:
  - WNS: 1.955 ns
  - WHS: 0.022 ns
  - axi0_ACLK: 328.407 MHz
  - i_fb_clk: 244.918 MHz
  - mipi_clk: 234.797 MHz
  - i_sysclk_div2: 161.421 MHz
  - hdmi_tx_slow_clk: 261.165 MHz
  - mipi_rx_ck0_CLKOUT: 155.521 MHz
  - mipi_rx_ck1_CLKOUT: 378.501 MHz
- Picture: unchanged noisy colored block/stripe pattern.

### Interpretation

Using the LED map from iteration 58:

- LED18-23 ON: DDR/ch0/reset/I2C status sampling and no aggregate ACK fault still pass.
- LED33 ON: `out_sync` released.
- LED24 OFF: no ch0 `rx_out_vs` was latched in this build.
- LED25 OFF: no ch0 `rx_out_de` was latched.
- LED26 OFF: no ch0 RAW10/4ppc format was latched.
- LED27/28 OFF: no framebuffer `frame_start` or input FIFO write was seen, expected if VS/DE are absent.

This conflicts with earlier runs where downstream CDC/bridge activity appeared. For this build, the immediate blocker is before framebuffer write: ch0 soft CSI output is not producing visible VS/DE/datatype milestones. The next probe must step one level earlier into MIPI/CSI receiver status: byte clock, LP/HS activity, lane FIFO nonempty/read, datatype nonzero, RAW10, 4ppc, and VS/DE.

---

## 60. 2026-07-09 forty-eighth iteration: expose ch0 MIPI/CSI receiver milestones (Codex)

### RTL intent

Move LED24-33 upstream from framebuffer input to the ch0 MIPI/CSI receiver boundary, using already-existing top-level sticky signals.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Reassigned LED24-33 to ch0 MIPI/CSI receiver milestones:
  - byte-clock toggle seen
  - LP activity seen
  - HS termination/enable seen
  - HS data changed/nonzero
  - MIPI lane FIFO nonempty
  - MIPI lane FIFO read
  - CSI datatype nonzero
  - CSI RAW10 datatype
  - CSI 4 pixels/clock
  - CSI VS or DE output seen
- Kept `USE_INPUT_STABLE_GATE(1'b0)` for diagnostic continuity.
- No synthesis, P&R, or programming was run by Codex.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 delayed I2C controller reset released |
| LED22 | G2 | ch0 I2C status register sampled |
| LED23 | K6 | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | ch0 MIPI byte clock toggled |
| LED25 | L6 | ch0 MIPI LP activity seen |
| LED26 | K4 | ch0 MIPI HS termination/enable seen |
| LED27 | K3 | ch0 MIPI HS data changed or nonzero |
| LED28 | M5 | ch0 MIPI lane FIFO nonempty seen |
| LED29 | M6 | ch0 MIPI lane FIFO read seen |
| LED30 | N7 | ch0 CSI datatype nonzero seen |
| LED31 | P7 | ch0 CSI RAW10 datatype seen |
| LED32 | P6 | ch0 CSI 4 pixels/clock seen |
| LED33 | R6 | ch0 CSI VS or DE seen |

### How to read the next board result

- LED24 OFF: ch0 MIPI byte clock is not toggling; inspect DPHY clock/reset/physical camera stream.
- LED24 ON but LED25/26 OFF: byte clock exists but LP/HS receiver activity is not visible.
- LED26 ON but LED27 OFF: HS receiver enables/terminates, but lane data is not changing.
- LED27 ON but LED28 OFF: HS data exists, but lane FIFO is never nonempty.
- LED28 ON but LED29 OFF: FIFO has data but CSI controller is not reading it.
- LED29 ON but LED30 OFF: CSI controller reads lanes but does not decode any nonzero datatype.
- LED30 ON but LED31 OFF: packets decode, but datatype is not RAW10.
- LED31 ON but LED32 OFF: RAW10 appears but pixel-per-clock is not 4.
- LED31/32 ON but LED33 OFF: valid RAW10/4ppc metadata exists, but no VS/DE reaches ch0 output.
- LED33 ON: CSI output exists again; return to framebuffer `frame_start/frame_stable` probe.

---

## 61. 2026-07-09 forty-eighth board feedback: MIPI LP only, no byte clock or HS/CSI decode (Codex)

### Board feedback from user

- LEDs ON: LED18-23, LED25.
- LEDs OFF: LED24, LED26-33.
- Timing screenshot:
  - WNS: 1.869 ns
  - WHS: 0.018 ns
  - axi0_ACLK: 319.387 MHz
  - i_fb_clk: 250.313 MHz
  - mipi_clk: 235.294 MHz
  - i_sysclk_div2: 173.551 MHz
  - hdmi_tx_slow_clk: 285.225 MHz
  - mipi_rx_ck0_CLKOUT: 185.460 MHz
  - mipi_rx_ck1_CLKOUT: 411.184 MHz
- Picture: unchanged noisy colored block/stripe pattern.

### Interpretation

Using the LED map from iteration 60:

- LED18-23 ON: DDR/ch0/reset/I2C status sampling and no aggregate ACK fault still pass.
- LED25 ON: ch0 MIPI LP activity is visible at the physical pins.
- LED24 OFF: ch0 MIPI byte clock toggle was not observed.
- LED26 OFF: no HS termination/enable activity was latched.
- LED27 OFF: no HS data change/nonzero was latched.
- LED28/29 OFF: no MIPI lane FIFO nonempty/read activity was observed.
- LED30-33 OFF: no CSI datatype/RAW10/4ppc/VS-DE output was observed.

This points before CSI packet decode. The receiver sees LP/idle-level activity, but the camera does not appear to enter MIPI HS streaming on ch0. The next hypothesis is that I2C initialization may not be reaching the stream-on register write (`0x0100 = 0x01`) or may be stopping before the final register table entries. The next probe must split ch0 I2C configuration completion, stream-on index, stream-on write, last index, and ACK errors.

---

## 62. 2026-07-09 forty-ninth iteration: expose ch0 I2C stream-on completion (Codex)

### RTL intent

Move LED24-33 from MIPI receiver activity to ch0 I2C/reset/stream-on completion so the next board run can distinguish camera-not-streaming from DPHY/CSI receiver failure.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Reassigned LED24-33 to ch0 I2C/reset milestones:
  - reset_pixel_n released
  - I2C init_done
  - I2C write enable seen
  - I2C write done seen
  - I2C config done
  - stream-on index reached
  - stream-on register write seen
  - last register index seen
  - ACK error seen
  - MIPI LP activity retained as a physical-pin reference
- Kept `USE_INPUT_STABLE_GATE(1'b0)` for diagnostic continuity.
- No synthesis, P&R, or programming was run by Codex.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 delayed I2C controller reset released |
| LED22 | G2 | ch0 I2C status register sampled |
| LED23 | K6 | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | ch0 `reset_pixel_n` released |
| LED25 | L6 | ch0 I2C `init_done` |
| LED26 | K4 | ch0 I2C write enable seen |
| LED27 | K3 | ch0 I2C write done seen |
| LED28 | M5 | ch0 I2C config done |
| LED29 | M6 | ch0 stream-on index reached |
| LED30 | N7 | ch0 stream-on register write seen (`0x0100 = 0x01`) |
| LED31 | P7 | ch0 last register index seen |
| LED32 | P6 | BAD: ch0 I2C ACK error seen |
| LED33 | R6 | ch0 MIPI LP activity seen |

### How to read the next board result

- LED24 OFF: ch0 soft CSI pixel-side reset is not released.
- LED25 OFF: I2C low-level init did not finish.
- LED26 OFF: register writer did not issue writes after init.
- LED27 OFF with LED26 ON: writes were requested but not completed.
- LED28 OFF with LED27 ON: table did not complete.
- LED29 OFF: register table never reached the stream-on region.
- LED30 OFF with LED29 ON: table reached stream-on region but never wrote `0x0100 = 0x01`.
- LED31 OFF with LED30 ON: stream-on was written but table did not reach the final index.
- LED32 ON: ACK error occurred; split ACK phase next.
- LED33 OFF: physical LP activity disappeared, suggesting camera/pin/reset regression.
- LED28/30/31 ON and LED32 OFF with no MIPI byte clock in the prior probe would shift focus to camera reset polarity, sensor mode table, or physical MIPI clock lane.

---

## 63. 2026-07-09 forty-ninth board feedback: I2C stream-on completes, picture changes to dynamic bars (Codex)

### Board feedback from user

- LEDs: only LED32 is OFF; all other LEDs in LED18-33 are ON.
- Timing screenshot:
  - WNS: 1.866 ns
  - WHS: 0.022 ns
  - axi0_ACLK: 319.081 MHz
  - i_fb_clk: 237.079 MHz
  - mipi_clk: 240.038 MHz
  - i_sysclk_div2: 154.631 MHz
  - hdmi_tx_slow_clk: 286.533 MHz
  - mipi_rx_ck0_CLKOUT: 210.172 MHz
  - mipi_rx_ck1_CLKOUT: 384.320 MHz
- Picture: changed from previous noisy block pattern. User reports the camera image is changing/jumping, with colored vertical bars and black bars changing continuously. Screenshot shows a stable upper color-bar-like region and dark/lined lower region.

### Interpretation

Using the LED map from iteration 62:

- LED24 ON: ch0 soft CSI pixel-side reset is released.
- LED25 ON: I2C low-level init completed.
- LED26 ON: register writer issued writes.
- LED27 ON: I2C write completion occurred.
- LED28 ON: register-table configuration completed.
- LED29 ON: stream-on index region was reached.
- LED30 ON: stream-on write `0x0100 = 0x01` was observed.
- LED31 ON: last register index was seen.
- LED32 OFF: no aggregate I2C ACK error was observed.
- LED33 ON: ch0 MIPI LP activity remains visible.

This clears the main I2C/stream-on hypothesis. The camera is configured through stream-on with no observed ACK error, and the visible HDMI picture changed materially. The next focus returns to the post-stream-on MIPI/CSI/video path: byte clock, HS activity, lane FIFO, RAW10/4ppc, VS/DE, and whether `hdmi_top` is showing input video.

---

## 64. 2026-07-09 fiftieth iteration: post stream-on MIPI/CSI and HDMI input probe (Codex)

### RTL intent

After confirming I2C stream-on, re-expose ch0 MIPI/CSI receiver milestones while keeping one LED for `hdmi_top` input selection.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Reassigned LED24-33 to:
  - ch0 stream-on write seen as the retained I2C proof
  - MIPI byte clock
  - MIPI HS term/enable
  - MIPI HS data
  - MIPI lane FIFO nonempty/read
  - CSI RAW10 and 4ppc
  - CSI VS/DE
  - `hdmi_top` using input video
- Kept `USE_INPUT_STABLE_GATE(1'b0)` for diagnostic continuity.
- No synthesis, P&R, or programming was run by Codex.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 delayed I2C controller reset released |
| LED22 | G2 | ch0 I2C status register sampled |
| LED23 | K6 | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | ch0 stream-on register write seen (`0x0100 = 0x01`) |
| LED25 | L6 | ch0 MIPI byte clock toggled |
| LED26 | K4 | ch0 MIPI HS termination/enable seen |
| LED27 | K3 | ch0 MIPI HS data changed or nonzero |
| LED28 | M5 | ch0 MIPI lane FIFO nonempty seen |
| LED29 | M6 | ch0 MIPI lane FIFO read seen |
| LED30 | N7 | ch0 CSI RAW10 datatype seen |
| LED31 | P7 | ch0 CSI 4 pixels/clock seen |
| LED32 | P6 | ch0 CSI VS or DE seen |
| LED33 | R6 | `hdmi_top` is using input video |

### How to read the next board result

- LED24 OFF: I2C stream-on did not recur in the new build; revisit I2C.
- LED25 OFF with LED24 ON: stream-on was issued but no MIPI byte clock is seen.
- LED26 OFF with LED25 ON: byte clock exists but HS receiver enable/termination is not observed.
- LED27 OFF with LED26 ON: receiver entered HS handling but lane data does not change.
- LED28 OFF with LED27 ON: HS data exists but lane FIFO is not becoming nonempty.
- LED29 OFF with LED28 ON: lane FIFO has data but CSI controller is not reading it.
- LED30 OFF with LED29 ON: CSI reads lane data but RAW10 datatype is absent.
- LED31 OFF with LED30 ON: RAW10 exists but pixel-per-clock is not 4.
- LED32 OFF with LED30/31 ON: RAW10/4ppc metadata exists but no VS/DE reaches ch0 output.
- LED33 ON: HDMI is using input video; then debug the content/timing/packing of the dynamic bars.

---

## 65. 2026-07-09 fiftieth board feedback: full stream-on/MIPI/CSI/HDMI-input chain is alive (Codex)

### Board feedback from user

- LEDs ON: LED18-33 all ON.
- Timing screenshot:
  - WNS: 1.688 ns
  - WHS: 0.026 ns
  - axi0_ACLK: 301.932 MHz
  - i_fb_clk: 204.457 MHz
  - mipi_clk: 216.169 MHz
  - i_sysclk_div2: 155.328 MHz
  - hdmi_tx_slow_clk: 270.270 MHz
  - mipi_rx_ck0_CLKOUT: 193.050 MHz
  - mipi_rx_ck1_CLKOUT: 390.930 MHz
- Picture: changes continuously. User reports the first observed image was a large color-bar/black-block pattern; later it became a noisy multi-color block pattern. Both screenshots show input-like changing content, not the prior static fallback-only state.

### Interpretation

Using the LED map from iteration 64:

- LED24 ON: ch0 stream-on register write was seen again.
- LED25 ON: ch0 MIPI byte clock toggled.
- LED26 ON: ch0 MIPI HS termination/enable was seen.
- LED27 ON: ch0 MIPI HS data changed or was nonzero.
- LED28 ON: ch0 MIPI lane FIFO became nonempty.
- LED29 ON: ch0 MIPI lane FIFO was read.
- LED30 ON: ch0 CSI RAW10 datatype was seen.
- LED31 ON: ch0 CSI 4 pixels/clock was seen.
- LED32 ON: ch0 CSI VS or DE was seen.
- LED33 ON: `hdmi_top` is using input video.

This is the first board result that proves the full intended live path is active: I2C stream-on, MIPI HS, CSI RAW10/4ppc, CSI VS/DE, and HDMI input selection. The remaining issue is no longer bring-up reachability; it is input video content/timing/packing stability. The visible symptoms are consistent with at least one of:

- framebuffer read/write instability or DDR read gaps,
- CDC bridge underflow/low-water behavior,
- HDMI input timing/size qualification mismatch,
- pixel packing/Bayer/RGB ordering still wrong after valid input reaches HDMI.

The next probe should preserve LED33 as input-video proof and expose bad-state blockers around framebuffer, DDR read, CDC, and HDMI timing.

---

## 66. 2026-07-09 fifty-first iteration: input-video content/timing fault probe (Codex)

### RTL intent

Now that the whole live chain is proven active, move LED24-33 to diagnose why the input image is unstable/wrong.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Reassigned LED24-33 to selected framebuffer, DDR read, CDC bridge, HDMI timing, and HDMI input-selection signals:
  - selected framebuffer ready
  - selected frame stable
  - selected write-frame done
  - selected read-frame available
  - framebuffer underflow
  - DDR read gap
  - CDC bridge underflow
  - CDC bridge low-water
  - HDMI timing size/error
  - `hdmi_top` using input video
- Kept `USE_INPUT_STABLE_GATE(1'b0)` for diagnostic continuity.
- No synthesis, P&R, or programming was run by Codex.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 delayed I2C controller reset released |
| LED22 | G2 | ch0 I2C status register sampled |
| LED23 | K6 | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | selected framebuffer ready |
| LED25 | L6 | selected frame stable seen |
| LED26 | K4 | selected write-frame done seen |
| LED27 | K3 | selected read-frame available |
| LED28 | M5 | BAD: selected framebuffer underflow |
| LED29 | M6 | BAD: DDR read gap seen |
| LED30 | N7 | BAD: CDC bridge underflow |
| LED31 | P7 | BAD: CDC bridge level low |
| LED32 | P6 | BAD: HDMI timing size/error seen |
| LED33 | R6 | `hdmi_top` is using input video |

### How to read the next board result

- LED24 OFF: framebuffer ready is not staying true even though live CSI exists.
- LED25 OFF: frame-size stability is still not reliable.
- LED26 OFF: write-frame completion is missing or intermittent.
- LED27 OFF: read-frame availability is missing after writes.
- LED28 ON: framebuffer underflow is corrupting the displayed content.
- LED29 ON: DDR read gap was observed and may explain black/noisy bands.
- LED30 ON: CDC bridge underflow is corrupting HDMI input.
- LED31 ON: CDC bridge FIFO level is too low.
- LED32 ON: HDMI input timing/size detector sees mismatch/error.
- LED33 OFF: HDMI fell back away from input video; return to ready-gate debugging.
- If LED24-27 and LED33 are ON while LED28-32 are OFF, the next likely focus is pixel packing/Bayer/RGB ordering rather than reachability/timing.

---

## 67. 2026-07-09 fifty-first board feedback: HDMI input path regressed, timing error observed (Codex)

### Board feedback from user

- LEDs ON: LED18-23, LED32.
- LEDs OFF: LED24-31, LED33.
- Timing screenshot:
  - WNS: 1.574 ns
  - WHS: 0.013 ns
  - axi0_ACLK: 291.886 MHz
  - i_fb_clk: 272.405 MHz
  - mipi_clk: 219.974 MHz
  - i_sysclk_div2: 156.740 MHz
  - hdmi_tx_slow_clk: 294.118 MHz
  - mipi_rx_ck0_CLKOUT: 217.108 MHz
  - mipi_rx_ck1_CLKOUT: 384.320 MHz
- Picture: user reports it returned to the previous noisy/blocky pattern.

### Interpretation

Using the LED map from iteration 66:

- LED24 OFF: selected framebuffer ready is not true in this run.
- LED25 OFF: selected frame stability was not seen by the current held probe.
- LED26 OFF: selected write-frame completion was not seen by the current held probe.
- LED27 OFF: selected read-frame availability is false.
- LED28-31 OFF: no framebuffer underflow, DDR read gap, CDC underflow, or CDC low-water was exposed by this map.
- LED32 ON: HDMI timing size/error path observed a mismatch/error.
- LED33 OFF: `hdmi_top` is not using input video in this run.

This is a regression from the prior all-LEDs-ON run. The immediate issue is not pixel packing; the design fell back from the proven input-video state. The next probe should combine the previously successful stream-on/MIPI/CSI proof chain with the two most important downstream state bits: selected framebuffer ready and HDMI input/timing status.

---

## 68. 2026-07-09 fifty-second iteration: chain regression plus ready/timing probe (Codex)

### RTL intent

Expose whether the proven chain regressed before or after CSI output, while retaining selected framebuffer ready and HDMI timing/input indicators.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Reassigned LED24-33 to:
  - ch0 stream-on write seen
  - ch0 MIPI byte clock
  - ch0 MIPI HS term/enable
  - ch0 MIPI FIFO nonempty/read
  - ch0 CSI RAW10 and 4ppc
  - ch0 CSI VS or DE
  - selected framebuffer ready
  - `hdmi_top` using input video
  - HDMI timing size/error bad state
  - framebuffer/DDR/CDC bad state aggregate
- Kept `USE_INPUT_STABLE_GATE(1'b0)` for diagnostic continuity.
- No synthesis, P&R, or programming was run by Codex.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 delayed I2C controller reset released |
| LED22 | G2 | ch0 I2C status register sampled |
| LED23 | K6 | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | ch0 stream-on register write seen (`0x0100 = 0x01`) |
| LED25 | L6 | ch0 MIPI byte clock toggled |
| LED26 | K4 | ch0 MIPI HS termination/enable seen |
| LED27 | K3 | ch0 MIPI lane FIFO nonempty/read seen |
| LED28 | M5 | ch0 CSI RAW10 and 4ppc seen |
| LED29 | M6 | ch0 CSI VS or DE seen |
| LED30 | N7 | selected framebuffer ready |
| LED31 | P7 | `hdmi_top` is using input video |
| LED32 | P6 | BAD: HDMI timing size/error seen |
| LED33 | R6 | BAD: framebuffer underflow / DDR read gap / CDC underflow / CDC low-water |

### How to read the next board result

- LED24 OFF: stream-on did not recur; return to I2C probe.
- LED25/26/27 OFF after LED24 ON: MIPI/DPHY receiver activity regressed after stream-on.
- LED28 OFF after LED25-27 ON: CSI datatype/ppc decode regressed.
- LED29 OFF after LED28 ON: CSI metadata exists but no VS/DE reaches output.
- LED30 OFF after LED29 ON: CSI output exists but framebuffer ready is still false.
- LED31 OFF after LED30 ON: framebuffer ready exists but HDMI is not using input; inspect ready gate/timing.
- LED32 ON: HDMI input timing/size error is active or latched.
- LED33 ON: downstream bad state from framebuffer/DDR/CDC is active or latched.

---

## 69. 2026-07-09 fifty-second board feedback: stream-on seen, MIPI activity absent (Codex)

### Board feedback from user

- LEDs ON: LED18-24, LED32.
- LEDs OFF: LED25-31, LED33.
- Timing screenshot:
  - WNS: 1.289 ns
  - WHS: 0.024 ns
  - axi0_ACLK: 269.469 MHz
  - i_fb_clk: 283.688 MHz
  - mipi_clk: 238.949 MHz
  - i_sysclk_div2: 163.988 MHz
  - hdmi_tx_slow_clk: 244.021 MHz
  - mipi_rx_ck0_CLKOUT: 199.880 MHz
  - mipi_rx_ck1_CLKOUT: 382.702 MHz
- Picture: noisy/blocky pattern remains.

### Interpretation

Using the LED map from iteration 68:

- LED24 ON: ch0 stream-on write (`0x0100 = 0x01`) was seen.
- LED25 OFF: no ch0 MIPI byte-clock toggle was captured.
- LED26 OFF: no ch0 MIPI HS termination/enable was seen.
- LED27 OFF: no ch0 MIPI lane FIFO nonempty/read was seen.
- LED28 OFF: CSI RAW10/4ppc was not decoded in this run.
- LED29 OFF: CSI VS/DE was not seen.
- LED30 OFF: selected framebuffer ready is not true.
- LED31 OFF: `hdmi_top` is not using input video.
- LED32 ON: HDMI timing size/error path latched a bad state.
- LED33 OFF: no downstream framebuffer/DDR/CDC bad-state aggregate was exposed by this map.

The failure is now before CSI/framebuffer: I2C can reach stream-on, but this run does not show the MIPI byte-clock/HS/FIFO activity that was present in the earlier all-LEDs-on run. The next probe should split the post-stream-on physical/MIPI side into reset output, LP activity, byte clock, HS enable/data, and FIFO activity.

---

## 70. 2026-07-09 fifty-third iteration: post-stream-on MIPI bring-up probe (Codex)

### RTL intent

Keep the proven basic/I2C checkpoints, then split LED24-33 across the earliest camera/MIPI receive states after stream-on. This distinguishes camera reset/LP-idle behavior from DPHY byte-clock/HS receive failure.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Reassigned LED24-33 to:
  - ch0 pixel-domain reset released,
  - ch0 camera reset output high,
  - ch0 stream-on write seen,
  - ch0 MIPI LP clock activity,
  - ch0 MIPI LP lane0 activity,
  - ch0 MIPI LP data toggle,
  - ch0 MIPI byte-clock toggle,
  - ch0 MIPI HS term/enable,
  - HDMI timing size/error bad state,
  - ch0 MIPI HS data or FIFO activity.
- Kept `USE_INPUT_STABLE_GATE(1'b0)` for diagnostic continuity.
- No synthesis, P&R, or programming was run by Codex.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 delayed I2C controller reset released |
| LED22 | G2 | ch0 I2C status register sampled |
| LED23 | K6 | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | ch0 pixel-domain reset released (`ch0_dbg_reset_pixel_n`) |
| LED25 | L6 | ch0 camera reset output high (`S0_o_cam_rst_p`) |
| LED26 | K4 | ch0 stream-on register write seen (`0x0100 = 0x01`) |
| LED27 | K3 | ch0 MIPI LP clock activity seen |
| LED28 | M5 | ch0 MIPI LP lane0 activity seen |
| LED29 | M6 | ch0 MIPI LP data toggle seen |
| LED30 | N7 | ch0 MIPI byte clock toggled |
| LED31 | P7 | ch0 MIPI HS termination/enable seen |
| LED32 | P6 | BAD: HDMI timing size/error seen |
| LED33 | R6 | ch0 MIPI HS data or FIFO activity seen |

### How to read the next board result

- LED24 OFF: ch0 pixel-domain reset never released; inspect `reset_pixel_n` generation.
- LED25 OFF with LED24 ON: camera reset output is held low; inspect camera reset polarity/path before changing CSI/framebuffer.
- LED26 OFF: stream-on write did not recur; return to I2C register sequencing.
- LED27/28 OFF after LED26 ON: camera/receiver LP state is not visible after stream-on; suspect camera reset/power/LP input path.
- LED29 OFF while LED27/28 ON: LP lines are present but not changing enough for the current toggle probe.
- LED30 OFF after LED26-29 ON: no byte clock starts; focus on camera stream start or DPHY clock lane.
- LED31 OFF with LED30 ON: byte clock toggles but HS enable/termination is not asserted.
- LED33 OFF with LED31 ON: HS control exists but no HS data/FIFO activity reaches the lane receivers.
- LED32 ON remains a downstream HDMI timing symptom; for this probe, prioritize LED24-31/33 to locate why live MIPI does not reappear.

---

## 71. 2026-07-09 fifty-third board feedback: LP idle visible, no MIPI stream (Codex)

### Board feedback from user

- LEDs OFF: LED25, LED29, LED30, LED31, LED33.
- LEDs ON: all other LEDs in LED18-33.
- Picture: no visible change.
- WNS/WHS: not reported in this message.

### Interpretation

Using the LED map from iteration 70:

- LED24 ON: ch0 pixel-domain reset is released.
- LED25 OFF: `S0_o_cam_rst_p` is low. In the current RTL, `soft_mipi_rx_top` drives `o_cam_rst_p = ~arst_n`, so this may be the expected released state rather than a fault.
- LED26 ON: at least one ch0 stream-on register write (`0x0100 = 0x01`) was seen.
- LED27 ON and LED28 ON: MIPI LP clock/lane0 activity is visible at the receiver pins.
- LED29 OFF: no LP data-line toggle was captured.
- LED30 OFF: no MIPI byte-clock toggle was captured.
- LED31 OFF: no MIPI HS termination/enable was seen.
- LED33 OFF: no HS data or MIPI FIFO activity was seen.

This narrows the failure to after I2C stream-on but before MIPI HS streaming. Because the current SC431HAI register table has two stream-on writes (`ROM[8'h90]` and final `ROM[8'ha1]`), the current LED26 only proves at least one stream-on write; it does not prove the table reached the final window registers and final stream-on. The next probe should confirm table completion/final index and I2C bad status while retaining LP/MIPI-start reference bits.

---

## 72. 2026-07-09 fifty-fourth iteration: I2C table completion vs MIPI-start probe (Codex)

### RTL intent

Distinguish "stream-on happened early but the table did not finish cleanly" from "full I2C table finished but the camera still never starts MIPI byte-clock/HS".

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Reassigned LED24-33 to:
  - ch0 pixel-domain reset released,
  - ch0 camera reset output low,
  - ch0 I2C config table completed,
  - ch0 final ROM entry write seen,
  - ch0 stream-on index reached,
  - ch0 stream-on write seen,
  - I2C ACK error,
  - I2C arbitration lost,
  - MIPI LP clock/lane activity,
  - MIPI byte-clock/HS/FIFO aggregate activity.
- Did not change the I2C ROM, camera reset logic, MIPI/CSI logic, constraints, or Efinity project files.
- Kept `USE_INPUT_STABLE_GATE(1'b0)` for diagnostic continuity.
- No synthesis, P&R, or programming was run by Codex.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 delayed I2C controller reset released |
| LED22 | G2 | ch0 I2C status register sampled |
| LED23 | K6 | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | ch0 pixel-domain reset released (`ch0_dbg_reset_pixel_n`) |
| LED25 | L6 | ch0 camera reset output low (`~S0_o_cam_rst_p`) |
| LED26 | K4 | ch0 I2C config table completed |
| LED27 | K3 | ch0 final ROM entry write seen |
| LED28 | M5 | ch0 stream-on index reached |
| LED29 | M6 | ch0 stream-on register write seen (`0x0100 = 0x01`) |
| LED30 | N7 | BAD: ch0 I2C ACK error seen |
| LED31 | P7 | BAD: ch0 I2C arbitration-lost seen |
| LED32 | P6 | ch0 MIPI LP clock or lane0 activity seen |
| LED33 | R6 | ch0 MIPI byte-clock / HS / FIFO activity seen |

### How to read the next board result

- LED24 OFF: ch0 pixel reset regressed.
- LED25 OFF with LED24 ON: camera reset output is high; verify whether this is intended active reset for the sensor.
- LED26 OFF: the I2C register table did not complete.
- LED27 OFF with LED26 ON: table completion logic reached `DATA_LENGTH`, but the final ROM entry was not observed as a write.
- LED28 OFF: the register sequencer never reached the stream-on index region.
- LED29 OFF with LED28 ON: the sequencer reached the stream-on region but did not write `0x0100 = 0x01`.
- LED30 ON: ACK error occurred; split ACK phase next if it recurs.
- LED31 ON: I2C arbitration-lost occurred.
- LED32 OFF: MIPI LP activity disappeared; focus on reset/power/physical pins.
- LED33 OFF while LED26/27/29 are ON and LED30/31 are OFF: full I2C table likely completed cleanly, but the camera still did not enter MIPI byte-clock/HS streaming; next focus becomes reset polarity, sensor mode table, or physical DPHY clock lane.

---

## 73. 2026-07-09 fifty-fourth board feedback: full I2C table and MIPI activity, color-bar/black-band image (Codex)

### Board feedback from user

- LEDs ON: LED18-29, LED31-33.
- LEDs OFF: LED30 only.
- Timing screenshot:
  - WNS: 1.46 ns
  - WHS: 0.016 ns
  - axi0_ACLK: 282.486 MHz
  - i_fb_clk: 271.223 MHz
  - mipi_clk: 205.846 MHz
  - i_sysclk_div2: 166.472 MHz
  - hdmi_tx_slow_clk: 289.101 MHz
  - mipi_rx_ck0_CLKOUT: 206.271 MHz
  - mipi_rx_ck1_CLKOUT: 409.668 MHz
- Picture: large stable color bars with black band/black lower or middle region. This is a substantial visual change from the prior noisy/blocky fallback-like output.

### Interpretation

Using the LED map from iteration 72:

- LED26 ON: ch0 I2C config table completed.
- LED27 ON: ch0 final ROM entry write was seen.
- LED28 ON and LED29 ON: the stream-on index was reached and a `0x0100 = 0x01` write was seen.
- LED30 OFF: no I2C ACK error was observed. This is good because LED30 is a BAD-state probe in this map.
- LED31 ON: I2C arbitration-lost was observed by the current latch. This needs to be retained as a bad-state reference, but it did not prevent the table/MIPI milestones from being reached in this run.
- LED32 ON: MIPI LP activity was seen.
- LED33 ON: MIPI byte-clock/HS/FIFO aggregate activity was seen.

This result proves the prior regression was not a persistent I2C-table-completion failure. The design reached full table completion and MIPI activity, and the displayed image changed to a stable color-bar/black-band pattern. The next probe should move downstream again: MIPI activity -> CSI RAW10/4ppc -> CSI VS/DE -> framebuffer frame_start/input FIFO/write done -> `hdmi_video_ready` -> `hdmi_top` using input video. Keep I2C arbitration-lost as a bad-state reference.

---

## 74. 2026-07-09 fifty-fifth iteration: post-MIPI CSI/HDMI input probe (Codex)

### RTL intent

Determine whether the stable color-bar/black-band image is still HDMI fallback/test behavior or whether decoded CSI/framebuffer data is now feeding `hdmi_top`.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Reassigned LED24-33 to:
  - I2C arbitration-lost bad state,
  - ch0 MIPI byte-clock/HS/FIFO activity,
  - ch0 CSI RAW10 and 4ppc,
  - ch0 CSI VS or DE,
  - ch0 framebuffer frame_start,
  - ch0 framebuffer input FIFO write,
  - ch0 framebuffer write-frame done,
  - `hdmi_video_ready`,
  - `hdmi_top` using input video,
  - aggregate HDMI timing/framebuffer/DDR/CDC bad state.
- Did not change the I2C ROM, camera reset logic, MIPI/CSI logic, constraints, or Efinity project files.
- Kept `USE_INPUT_STABLE_GATE(1'b0)` for diagnostic continuity.
- No synthesis, P&R, or programming was run by Codex.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 delayed I2C controller reset released |
| LED22 | G2 | ch0 I2C status register sampled |
| LED23 | K6 | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | BAD: ch0 I2C arbitration-lost seen |
| LED25 | L6 | ch0 MIPI byte-clock / HS / FIFO activity seen |
| LED26 | K4 | ch0 CSI RAW10 and 4ppc seen |
| LED27 | K3 | ch0 CSI VS or DE seen |
| LED28 | M5 | ch0 framebuffer `frame_start` seen |
| LED29 | M6 | ch0 framebuffer input FIFO write seen |
| LED30 | N7 | ch0 framebuffer write-frame done seen |
| LED31 | P7 | `hdmi_video_ready` asserted |
| LED32 | P6 | `hdmi_top` is using input video |
| LED33 | R6 | BAD: HDMI timing / framebuffer / DDR / CDC bad state seen |

### How to read the next board result

- LED24 ON: I2C arbitration-lost still occurred; keep watching whether it correlates with unstable MIPI/CSI.
- LED25 OFF: MIPI activity regressed again.
- LED26 OFF with LED25 ON: MIPI activity exists but CSI RAW10/4ppc decode is missing.
- LED27 OFF with LED26 ON: CSI metadata exists but no VS/DE reaches ch0 output.
- LED28 OFF with LED27 ON: CSI VS/DE exists but framebuffer `frame_start` is not detected.
- LED29 OFF with LED27 ON: active CSI data is not writing into the framebuffer input FIFO.
- LED30 OFF with LED28/29 ON: framebuffer input starts but a full frame write does not complete.
- LED31 OFF after LED30 ON: written frames exist but the HDMI ready qualification is still false.
- LED32 OFF after LED31 ON: ready is true but `hdmi_top` is not selecting input video.
- LED33 ON: downstream bad state from HDMI timing, framebuffer, DDR read, or CDC occurred and may explain the black bands.
- LED25-32 ON with LED33 OFF: input path is active and stable; remaining issue shifts toward pixel packing/Bayer/test-pattern sensor configuration.

---

## 75. 2026-07-09 fifty-fifth board feedback: MIPI/CSI/HDMI input regressed again, AL bad-state seen (Codex)

### Board feedback from user

- LEDs OFF: LED25-32.
- LEDs ON: LED18-24, LED33.
- Timing screenshot:
  - WNS: 1.919 ns
  - WHS: 0.026 ns
  - axi0_ACLK: 324.570 MHz
  - i_fb_clk: 251.762 MHz
  - mipi_clk: 229.568 MHz
  - i_sysclk_div2: 167.280 MHz
  - hdmi_tx_slow_clk: 295.421 MHz
  - mipi_rx_ck0_CLKOUT: 171.556 MHz
  - mipi_rx_ck1_CLKOUT: 384.911 MHz
- Picture: noisy multicolor tiled/stripe pattern.

### Interpretation

Using the LED map from iteration 74:

- LED24 ON: ch0 I2C arbitration-lost status was latched by the current debug signal.
- LED25 OFF: ch0 MIPI byte-clock/HS/FIFO aggregate activity was not seen in this run.
- LED26 OFF: CSI RAW10/4ppc was not seen.
- LED27 OFF: CSI VS/DE was not seen.
- LED28-30 OFF: framebuffer input/write milestones did not occur.
- LED31 OFF: `hdmi_video_ready` is false.
- LED32 OFF: `hdmi_top` is not using input video.
- LED33 ON: downstream bad-state aggregate is set. With LED25-32 OFF, this is likely a symptom of fallback/timing/bad-state latches rather than a live camera-frame corruption diagnosis.

This run regressed before MIPI activity again, and it is the first run where the current downstream probe clearly shows I2C arbitration-lost alongside missing MIPI/CSI. The AL signal is a sticky "ever seen" latch in `i2c_16addr_8data.v`, sampled while polling the I2C controller status register; it may be transient. The next probe should split ever-AL vs last-status-AL, ACK error, table completion/final entry/stream-on, and LP/byteclock/HS/FIFO activity.

---

## 76. 2026-07-09 fifty-sixth iteration: I2C AL vs MIPI-start correlation probe (Codex)

### RTL intent

Determine whether the unstable MIPI-start behavior correlates with a persistent I2C arbitration-lost status, a transient AL latch, or an otherwise completed I2C table followed by missing MIPI byteclock/HS.

### RTL changes

`final_project/fpga/rtl/top/top.v`

- Reassigned LED24-33 to:
  - I2C arbitration-lost ever seen,
  - I2C last sampled status AL bit,
  - I2C config table completion,
  - final ROM entry write,
  - stream-on write,
  - I2C ACK error ever seen,
  - MIPI LP activity,
  - MIPI byte-clock,
  - MIPI HS term/enable or HS data,
  - MIPI FIFO nonempty/read.
- Did not change the I2C ROM, camera reset logic, MIPI/CSI logic, constraints, or Efinity project files.
- Kept `USE_INPUT_STABLE_GATE(1'b0)` for diagnostic continuity.
- No synthesis, P&R, or programming was run by Codex.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 delayed I2C controller reset released |
| LED22 | G2 | ch0 I2C status register sampled |
| LED23 | K6 | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | BAD: ch0 I2C arbitration-lost ever seen |
| LED25 | L6 | BAD: ch0 I2C last sampled status has AL set |
| LED26 | K4 | ch0 I2C config table completed |
| LED27 | K3 | ch0 final ROM entry write seen |
| LED28 | M5 | ch0 stream-on register write seen (`0x0100 = 0x01`) |
| LED29 | M6 | BAD: ch0 I2C ACK error ever seen |
| LED30 | N7 | ch0 MIPI LP clock or lane0 activity seen |
| LED31 | P7 | ch0 MIPI byte clock toggled |
| LED32 | P6 | ch0 MIPI HS term/enable or HS data seen |
| LED33 | R6 | ch0 MIPI FIFO nonempty/read seen |

### How to read the next board result

- LED24 ON and LED25 ON: AL is still present in the last sampled I2C status; treat I2C controller/bus state as the immediate blocker.
- LED24 ON and LED25 OFF: AL occurred transiently earlier, but the last sampled status cleared it; correlate with LED26-33 before changing I2C logic.
- LED26/27/28 ON and LED29 OFF with LED31-33 OFF: I2C table and stream-on completed without ACK error, but MIPI did not start; focus returns to camera reset/mode table/physical DPHY start.
- LED29 ON: ACK error occurred; split ACK phase next.
- LED30 OFF: no LP activity; suspect reset/power/pin state.
- LED31 OFF with LED30 ON: LP is visible but byte clock did not start.
- LED32 OFF with LED31 ON: byte clock exists but HS control/data did not appear.
- LED33 OFF with LED32 ON: HS is visible but lane FIFO did not receive/read data.
- LED31-33 ON: MIPI is active again; return to CSI/HDMI input probe.

---

## 77. 2026-07-09 strategy change: combine small fixes with LED probes (Codex)

### User direction

The user explicitly requested a more ordered process: avoid ineffective repeated LED-only probing, and allow each round to include both a focused fix attempt and a corresponding LED probe.

### Updated debugging rule

From this point, each board iteration should include:

1. One concrete hypothesis.
2. One minimal, reversible RTL fix or configuration adjustment when there is a plausible low-risk candidate.
3. A LED map that directly proves whether the fix hit the hypothesis.
4. No broad unrelated RTL refactor.

### Current narrowed fault range

The primary unstable region is after I2C/register programming and before stable MIPI byte-clock/HS/FIFO startup:

- Reset, DDR, channel selection, and basic I2C status sampling have repeatedly passed.
- Full I2C table completion and MIPI activity have occurred at least once.
- The full path has also reached CSI/HDMI input at least once.
- Several later runs regressed before MIPI byte-clock/HS/FIFO.
- I2C arbitration-lost (`AL`) has been observed by a sticky status latch during at least one regression run.

The immediate working hypothesis is that I2C timing/controller status robustness contributes to intermittent camera stream startup. The current `i2c_master_ctrl_top` uses `mipi_clk` as the I2C controller clock, and `CLK_DIV=16'd199`; with build-to-build `mipi_clk` variation this is a few-hundred-kHz I2C bus. This may be valid in ideal conditions, but is a reasonable first low-risk knob for the observed intermittent AL/startup behavior.

---

## 78. 2026-07-09 fifty-seventh iteration: conservative I2C prescaler fix plus AL/MIPI probe (Codex)

### Hypothesis

The camera sometimes fails to enter stable MIPI streaming because the I2C controller/register writes are too close to the board/sensor timing margin. Slowing the I2C controller clock should reduce I2C status anomalies and improve repeatability of MIPI byte-clock/HS startup.

### RTL changes

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_ctrl_top.v`

- Changed `CLK_DIV` default from `16'd199` to `16'd499`.
- This keeps the same I2C sequencer and register table, but makes SCL substantially slower when driven by `mipi_clk`.
- This is intentionally conservative and reversible.

`final_project/fpga/rtl/top/top.v`

- Kept the current I2C AL vs MIPI-start LED map from iteration 76:
  - ever-AL,
  - last-status AL,
  - table completion,
  - final table entry,
  - stream-on,
  - ACK error,
  - LP,
  - byteclock,
  - HS,
  - FIFO.

### Expected result

If I2C timing margin was contributing to the intermittent startup:

- LED24 should preferably turn OFF or at least LED25 should turn OFF, showing AL is not persistent in the last sampled status.
- LED26/27/28 should remain ON, proving the slower I2C still completes the table and stream-on.
- LED31/32/33 should become more consistently ON across boots, proving MIPI byte-clock/HS/FIFO startup is more repeatable.

If LED26/27/28 turn OFF, the slower I2C change broke or stalled the register sequence and should be reverted or retuned.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 delayed I2C controller reset released |
| LED22 | G2 | ch0 I2C status register sampled |
| LED23 | K6 | ch0 I2C sampled with no aggregate NACK |
| LED24 | J3 | BAD: ch0 I2C arbitration-lost ever seen |
| LED25 | L6 | BAD: ch0 I2C last sampled status has AL set |
| LED26 | K4 | ch0 I2C config table completed |
| LED27 | K3 | ch0 final ROM entry write seen |
| LED28 | M5 | ch0 stream-on register write seen (`0x0100 = 0x01`) |
| LED29 | M6 | BAD: ch0 I2C ACK error ever seen |
| LED30 | N7 | ch0 MIPI LP clock or lane0 activity seen |
| LED31 | P7 | ch0 MIPI byte clock toggled |
| LED32 | P6 | ch0 MIPI HS term/enable or HS data seen |
| LED33 | R6 | ch0 MIPI FIFO nonempty/read seen |

### How to read the next board result

- LED26/27/28 ON and LED29 OFF: slower I2C still programs the sensor and writes stream-on.
- LED24 OFF and LED25 OFF: AL disappeared.
- LED24 ON but LED25 OFF: AL may be transient/history only; compare with MIPI LEDs.
- LED24/25 ON: AL remains active in the last sampled status; I2C/controller state is still suspect.
- LED31/32/33 ON: MIPI is active; next step should return to CSI/HDMI input stability.
- LED31/32/33 OFF with LED26/27/28 ON and LED29 OFF: I2C table completed cleanly but MIPI still did not start; next fix should target sensor reset/start sequencing or final stream-on timing, not more LED-only probing.

---

## 79. 2026-07-09 fifty-eighth iteration result: slower I2C passed, issue moved downstream (Codex)

### Board result reported by user

With the iteration-78 I2C AL/MIPI-start probe:

- LED24 OFF
- LED25 OFF
- LED29 OFF
- All other LED18-33 ON
- Picture/video still not a clean camera image.

Timing report from the user screenshot:

| Item | Value |
|------|-------|
| WNS | 1.593 ns |
| WHS | 0.026 ns |
| axi0_ACLK | 293.513 MHz |
| i_fb_clk | 245.278 MHz |
| mipi_clk | 226.501 MHz |
| i_sysclk_div2 | 171.233 MHz |
| hdmi_tx_slow_clk | 286.779 MHz |
| mipi_rx_ck0_CLKOUT | 206.654 MHz |
| mipi_rx_ck1_CLKOUT | 409.500 MHz |

### Video observation

User provided `D:\20306\Pictures\Camera Roll\WIN_20260709_19_50_21_Pro.mp4`.

Frames extracted with local `ffmpeg.exe` show:

- frame 1: full HDMI fallback color bars;
- frame 2: fallback color bars mixed with a dark/striped input region;
- frame 3: mostly dark input with fine horizontal stripe/noise.

This is not a pure fixed color-bar output for the whole clip, and it is not yet a clean camera image.

### Interpretation

The slower I2C prescaler change is tentatively effective:

- I2C table completion, final entry, and stream-on were observed.
- Sticky AL, last-status AL, and ACK-error LEDs were all OFF in this run.
- MIPI LP/byteclock/HS/FIFO LEDs were ON.

The immediate fault range is no longer I2C register programming or MIPI startup. The current focus is downstream:

- CSI RAW10/VS/DE validity,
- framebuffer frame start/write completion/read availability,
- CDC bridge level/underflow,
- HDMI stable input gate/use-input transition,
- then RAW10 unpack/Bayer/pixel order if the path is proven continuous.

### RTL changes prepared for next build

`final_project/fpga/rtl/top/top.v`

- Restored `hdmi_top.USE_INPUT_STABLE_GATE(1'b1)` so HDMI does not switch to input until its own timing-stability checker passes.
- Replaced the I2C/MIPI startup LED map with a downstream video-path LED map.

The I2C slowdown fix remains in place:

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_ctrl_top.v`

- `CLK_DIV = 16'd499`

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 stream-on plus MIPI byteclock/FIFO seen |
| LED22 | G2 | ch0 CSI RAW10/4ppc seen |
| LED23 | K6 | ch0 CSI VS and DE seen |
| LED24 | J3 | ch0 framebuffer frame_start seen |
| LED25 | L6 | BAD: framebuffer output underflow seen |
| LED26 | K4 | ch0 framebuffer write FIFO and write-frame done |
| LED27 | K3 | selected DDR read data and read-frame available |
| LED28 | M5 | CDC bridge active and level-ready seen |
| LED29 | M6 | BAD: CDC bridge underflow or level-low seen |
| LED30 | N7 | hdmi_video_ready seen |
| LED31 | P7 | hdmi_top input-stable gate passed |
| LED32 | P6 | hdmi_top input data changed under DE |
| LED33 | R6 | hdmi_top is using input video |

### How to read the next board result

- LED21 OFF: startup regressed before or at MIPI FIFO; return to I2C/MIPI start stability.
- LED22 OFF with LED21 ON: MIPI FIFO is active but CSI does not report RAW10/4ppc.
- LED23 OFF with LED22 ON: CSI format is right, but VS/DE is not reaching the downstream path.
- LED24 OFF with LED23 ON: framebuffer is not seeing frame_start.
- LED26 OFF with LED24 ON: framebuffer sees frames but does not complete write-frame path.
- LED27 OFF with LED26 ON: write completes, but DDR read/read-frame availability is failing.
- LED25 ON: framebuffer output underflow is corrupting or blocking the displayed input.
- LED28 OFF or LED29 ON: CDC bridge fill/underflow is the immediate issue.
- LED30 OFF with LED27/28 ON: `hdmi_video_ready` qualification is blocking the HDMI path.
- LED31 OFF with LED30 ON: HDMI input timing-stability gate is rejecting the selected input timing.
- LED33 OFF with LED31 ON: HDMI gate passed but `hdmi_top` did not switch to input video.
- LED33 ON with LED32 ON and LED25/29 OFF: the whole display path is active; then debug image content/pixel packing/Bayer order.

---

## 80. 2026-07-09 fifty-ninth iteration result: downstream probe exposed CDC false-start risk (Codex)

### Board result reported by user

With the iteration-79 downstream video-path probe:

- LED18 ON
- LED19 ON
- LED20 ON
- LED28 ON
- LED29 ON
- All other LED21-27 and LED30-33 OFF

Timing report from the user screenshot:

| Item | Value |
|------|-------|
| WNS | 1.152 ns |
| WHS | 0.026 ns |
| axi0_ACLK | 259.875 MHz |
| i_fb_clk | 237.023 MHz |
| mipi_clk | 220.167 MHz |
| i_sysclk_div2 | 189.000 MHz |
| hdmi_tx_slow_clk | 276.472 MHz |
| mipi_rx_ck0_CLKOUT | 220.410 MHz |
| mipi_rx_ck1_CLKOUT | 408.998 MHz |

User screenshot shows full-screen random colored noise with broad horizontal color bands.

### Interpretation

Under the iteration-79 LED map:

- LED18/19/20 ON confirms DDR configured, ch0 selected, and global reset released.
- LED21 OFF means the aggregated `stream-on + MIPI byteclock + MIPI FIFO` condition did not pass.
- LED22/23/24 OFF means CSI RAW10/VS/DE/framebuffer frame_start were not proven in that run.
- LED28 and LED29 ON are suspicious because the CDC bridge appeared active/level-ready and also reported underflow/level-low while upstream video checkpoints were not proven.

This points to a false-start/false-fill risk in `video_2pix_to_1pix_cdc`: it was writing one FIFO word every write-clock cycle regardless of whether real input video (`i_vs` or `i_de`) had ever appeared. That can fill the CDC FIFO with blank/stale timing words, assert bridge level/active flags, and then produce misleading HDMI/CDC LEDs and noisy output even when CSI/framebuffer input is not valid.

### RTL changes prepared for next build

`final_project/fpga/rtl/dvi_tx/video_2pix_to_1pix_cdc.v`

- Added an `input_started` latch in the write-clock domain.
- The CDC FIFO now writes only after input video has started (`i_vs` or `i_de` seen), instead of writing unconditionally every write-clock cycle.
- `o_level_low` is now reported only when the bridge is already active, reducing false bad-state reports before real video begins.

`final_project/fpga/rtl/top/top.v`

- Replaced the aggregated LED21 condition with split upstream checkpoints:
  - stream-on,
  - MIPI byteclock,
  - MIPI FIFO,
  - CSI RAW10/4ppc,
  - CSI VS,
  - CSI DE,
  - framebuffer frame_start.
- Kept CDC/HDMI readiness LEDs after those split upstream checkpoints.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 stream-on register write seen |
| LED22 | G2 | ch0 MIPI byte clock toggled |
| LED23 | K6 | ch0 MIPI FIFO nonempty/read seen |
| LED24 | J3 | ch0 CSI RAW10/4ppc seen |
| LED25 | L6 | ch0 CSI VS seen |
| LED26 | K4 | ch0 CSI DE seen |
| LED27 | K3 | ch0 framebuffer frame_start seen |
| LED28 | M5 | CDC bridge active and level-ready seen |
| LED29 | M6 | BAD: CDC bridge underflow or level-low seen |
| LED30 | N7 | hdmi_video_ready seen |
| LED31 | P7 | hdmi_top input-stable gate passed |
| LED32 | P6 | hdmi_top input data changed under DE |
| LED33 | R6 | hdmi_top is using input video |

### How to read the next board result

- LED21 OFF: sensor stream-on write did not complete; return to I2C/start sequencing.
- LED21 ON and LED22 OFF: sensor was commanded on, but MIPI byte clock did not appear.
- LED22 ON and LED23 OFF: byte clock exists but FIFO data/read did not appear.
- LED23 ON and LED24 OFF: MIPI FIFO is active, but CSI parser is not reporting RAW10/4ppc.
- LED24 ON and LED25/26 OFF: CSI format is correct, but frame timing (`VS`/`DE`) is missing.
- LED25/26 ON and LED27 OFF: framebuffer is not seeing frame_start.
- LED27 ON and LED28 OFF: upstream video entered framebuffer, but CDC bridge did not become ready.
- LED29 ON: CDC bridge still underflowed or dropped below level after real video started.
- LED30/31/33 decide whether HDMI readiness/stability/use-input is blocking after CDC is clean.

---

## 81. 2026-07-09 sixtieth iteration result: final stream-on seen but MIPI byte clock absent (Codex)

### Board result reported by user

With the iteration-80 MIPI/CSI split probe:

- LED18 ON
- LED19 ON
- LED20 ON
- LED21 ON
- All other LED22-33 OFF

Timing report from the user screenshot:

| Item | Value |
|------|-------|
| WNS | 1.663 ns |
| WHS | 0.013 ns |
| axi0_ACLK | 299.670 MHz |
| i_fb_clk | 231.857 MHz |
| mipi_clk | 227.118 MHz |
| i_sysclk_div2 | 180.995 MHz |
| hdmi_tx_slow_clk | 261.575 MHz |
| mipi_rx_ck0_CLKOUT | 204.248 MHz |
| mipi_rx_ck1_CLKOUT | 383.436 MHz |

User screenshot still shows full-screen random colored noise with broad horizontal color bands.

### Interpretation

The new split probe makes the current blocker specific:

- LED21 ON means the camera register table wrote stream-on (`0x0100 = 0x01`).
- LED22 OFF means ch0 MIPI byte clock was not observed by the FPGA after stream-on.
- LED23-27 OFF are downstream consequences: without byte clock/FIFO, CSI RAW10/VS/DE/framebuffer cannot be expected.
- LED28/29 are now OFF after the CDC false-start fix, confirming the previous CDC active/level-low LEDs were at least partly false activity from unconditional CDC FIFO writes.

The immediate focus is therefore sensor stream-on sequencing and MIPI receiver start, not framebuffer, CDC, HDMI, RAW10 unpack, or Bayer order.

### RTL changes prepared for next build

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_reg_rom.v`

- Changed the earlier mid-table `ROM[8'h90] = {16'h0100, 8'h01, 1'b0}` to standby:
  - `ROM[8'h90] = {16'h0100, 8'h00, 1'b0}`
- Kept the final `ROM[8'ha1] = {16'h0100, 8'h01, 1'b0}` as the only actual stream-on after final window registers are written.
- Rationale: the table previously commanded stream-on at `0x90`, then continued changing window registers `0x3200..0x3213`, then repeated stream-on at `0xa1`. The current symptom is exactly "stream-on write seen, but byte clock absent"; making stream-on happen only after the final window registers is a small, reversible sequencing fix.

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_reg_set.v`

- Moved `STREAM_ON_INDEX_VALUE` from `8'h90` to `8'ha1` so the debug "stream-on index reached" marker matches the final stream-on entry.

### Next LED18-33 map

The LED map remains the iteration-80 split probe:

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | final ch0 stream-on register write seen |
| LED22 | G2 | ch0 MIPI byte clock toggled |
| LED23 | K6 | ch0 MIPI FIFO nonempty/read seen |
| LED24 | J3 | ch0 CSI RAW10/4ppc seen |
| LED25 | L6 | ch0 CSI VS seen |
| LED26 | K4 | ch0 CSI DE seen |
| LED27 | K3 | ch0 framebuffer frame_start seen |
| LED28 | M5 | CDC bridge active and level-ready seen |
| LED29 | M6 | BAD: CDC bridge underflow or level-low seen |
| LED30 | N7 | hdmi_video_ready seen |
| LED31 | P7 | hdmi_top input-stable gate passed |
| LED32 | P6 | hdmi_top input data changed under DE |
| LED33 | R6 | hdmi_top is using input video |

### How to read the next board result

- LED21 OFF: final stream-on no longer completed; inspect I2C sequence/ACK after the ROM change.
- LED21 ON and LED22 OFF again: final stream-on completed but the camera still does not start MIPI byte clock; next target should be sensor reset/start delay, MIPI receiver enable/termination, or camera power/LP/HS status.
- LED22 ON and LED23 OFF: byte clock exists but lane FIFO does not receive/read.
- LED23 ON and LED24 OFF: MIPI FIFO active but CSI parser does not report RAW10/4ppc.
- LED24-27 ON: upstream video is back; proceed again to CDC/HDMI or pixel-content debugging.

---

## 82. 2026-07-09 sixty-first iteration result: final stream-on sequencing fix did not restore byte clock (Codex)

### Board result reported by user

With the iteration-81 final-stream-on-only change:

- LED18 ON
- LED19 ON
- LED20 ON
- LED21 ON
- All other LED22-33 OFF

Timing report from the user screenshot:

| Item | Value |
|------|-------|
| WNS | 1.7 ns |
| WHS | 0.026 ns |
| axi0_ACLK | 303.030 MHz |
| i_fb_clk | 254.130 MHz |
| mipi_clk | 244.618 MHz |
| i_sysclk_div2 | 170.882 MHz |
| hdmi_tx_slow_clk | 285.878 MHz |
| mipi_rx_ck0_CLKOUT | 203.832 MHz |
| mipi_rx_ck1_CLKOUT | 386.399 MHz |

User screenshot still shows full-screen random colored noise with broad horizontal color bands.

### Interpretation

The iteration-81 sequencing fix did not restore ch0 MIPI byte clock:

- LED21 ON: the final stream-on write still completed after moving the true stream-on entry to `0xa1`.
- LED22 OFF: ch0 MIPI byte clock still was not observed.
- LED23-33 OFF: no MIPI FIFO, CSI, framebuffer, CDC, or HDMI input milestones followed.

The current blocker remains between final camera stream-on and MIPI byte-clock/HS entry. The next probe should no longer spend LEDs on framebuffer/CDC/HDMI. It should split the MIPI physical boundary:

- pixel-side reset release,
- clock/data LP activity,
- LP data toggling,
- CSI receiver HS enable/termination outputs,
- HS data visibility,
- byte-clock/FIFO,
- I2C ACK/AL bad states.

### RTL changes prepared for next build

`final_project/fpga/rtl/top/top.v`

- Reassigned LED22-33 to the ch0 MIPI LP/HS boundary and I2C bad-state checks.
- No sensor register-table change in this iteration.
- Kept previous fixes in place:
  - slower I2C prescaler,
  - CDC false-start guard,
  - final-only stream-on.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | final ch0 stream-on register write seen |
| LED22 | G2 | ch0 soft CSI pixel reset released |
| LED23 | K6 | ch0 MIPI clock-lane LP activity seen |
| LED24 | J3 | ch0 MIPI data-lane0 LP activity seen |
| LED25 | L6 | ch0 MIPI data-lane LP toggled |
| LED26 | K4 | ch0 MIPI clock-lane HS enable seen |
| LED27 | K3 | ch0 MIPI data-lane HS enable seen |
| LED28 | M5 | ch0 MIPI HS termination seen |
| LED29 | M6 | ch0 MIPI HS data changed/nonzero |
| LED30 | N7 | ch0 MIPI byte clock toggled |
| LED31 | P7 | ch0 MIPI FIFO nonempty/read seen |
| LED32 | P6 | BAD: ch0 I2C ACK error ever seen |
| LED33 | R6 | BAD: ch0 I2C arbitration-lost ever seen |

### How to read the next board result

- LED22 OFF: reset/pixel-side release is broken before MIPI can start.
- LED23/24 OFF with LED21 ON: camera was commanded on but LP clock/data pins do not show expected activity; suspect camera reset/power/physical lane state.
- LED23/24 ON and LED25 OFF: lanes are present but not toggling; camera may stay in LP idle.
- LED25 ON and LED26/27/28 OFF: LP toggles but receiver does not enter HS enable/termination.
- LED26/27/28 ON and LED29 OFF: receiver tries HS, but HS data is not visible.
- LED29 ON and LED30 OFF: HS data exists but byte clock/reset crossing is not being observed.
- LED30 ON and LED31 OFF: byte clock exists but lane FIFO remains empty/not read.
- LED32 or LED33 ON: I2C bad state returned; split ACK/AL phase again before more MIPI changes.

---

## 83. 2026-07-09 sixty-second iteration result: stream-on was launched but I2C AL makes it untrusted (Codex)

### Board result reported by user

With the iteration-82 MIPI LP/HS boundary probe:

- LED18 ON
- LED19 ON
- LED20 ON
- LED21 ON
- LED22 ON
- LED23 ON
- LED24 ON
- LED33 ON
- LED25-32 OFF

Timing report from the user screenshots / handoff context:

| Item | Value |
|------|-------|
| WNS | 1.555 ns |
| WHS | 0.008 ns |
| axi0_ACLK | 290.276 MHz |
| i_fb_clk | 246.548 MHz |
| mipi_clk | 231.965 MHz |
| i_sysclk_div2 | 174.978 MHz |
| hdmi_tx_slow_clk | 261.917 MHz |
| mipi_rx_ck0_CLKOUT | 182.749 MHz |
| mipi_rx_ck1_CLKOUT | 389.105 MHz |

The screen still shows random colored noise / bands.

### Interpretation

The result narrows the failure to the final camera start / MIPI entry boundary, with I2C reliability now the first suspect:

- LED21 ON means the final `0100=01` stream-on write was launched.
- LED22 ON means the soft MIPI pixel-side reset released.
- LED23/24 ON mean ch0 MIPI clock/data LP pins are not dead.
- LED25-31 OFF mean no LP toggling, no HS enable/termination/data, no byte clock, and no FIFO activity was observed.
- LED33 ON is bad: the I2C controller observed arbitration lost (`AL`) at least once.

Because the previous LED21 was driven by the `wr_en` launch condition, not by a clean I2C completion, it did not prove the sensor accepted the final stream-on write. The next iteration therefore splits final stream-on into launched / completed / clean / error before making another MIPI-side change.

### RTL changes prepared for next build

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_16addr_8data.v`

- Added per-write debug pulses:
  - `dbg_wr_done_clean`: write transaction reached `wr_done` without RXACK or AL being sampled during that transaction.
  - `dbg_wr_done_error`: write transaction reached `wr_done` after RXACK or AL was sampled during that transaction.
- This is diagnostic only; it does not change the write FSM sequencing yet.

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_reg_set.v`

- Added final stream-on completion latches:
  - `dbg_stream_on_done`
  - `dbg_stream_on_clean`
  - `dbg_stream_on_error`
- Kept final stream-on index at `8'ha1`.

`final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_ctrl_top.v`
and `final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v`

- Routed the new debug signals to `top.v`.

`final_project/fpga/rtl/top/top.v`

- Reassigned LED21-33 to final-stream-on quality plus MIPI boundary signals.
- Kept `hdmi_top.USE_INPUT_STABLE_GATE(1'b1)`.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | final ch0 stream-on write launched |
| LED22 | G2 | final ch0 stream-on write completed |
| LED23 | K6 | final ch0 stream-on write completed cleanly, without RXACK/AL |
| LED24 | J3 | BAD: final ch0 stream-on write completed with RXACK/AL |
| LED25 | L6 | ch0 soft CSI pixel reset released |
| LED26 | K4 | ch0 MIPI clock-lane LP activity seen |
| LED27 | K3 | ch0 MIPI data-lane0 LP activity seen |
| LED28 | M5 | ch0 MIPI data-lane LP toggled |
| LED29 | M6 | ch0 MIPI HS enable/termination seen |
| LED30 | N7 | ch0 MIPI HS data changed/nonzero |
| LED31 | P7 | ch0 MIPI byte clock or FIFO activity seen |
| LED32 | P6 | BAD: ch0 I2C ACK error ever seen |
| LED33 | R6 | BAD: ch0 I2C arbitration-lost ever seen |

### How to read the next board result

- LED21 OFF: final stream-on write was not even launched.
- LED21 ON and LED22 OFF: the final stream-on transaction started but did not complete; inspect I2C FSM/core stall.
- LED22 ON, LED23 ON, LED24 OFF, LED32/33 OFF: final stream-on was clean; if LED28-31 still stay OFF, shift back to MIPI receiver / lane entry.
- LED24 ON or LED32/33 ON: final stream-on or earlier I2C sequence is dirty; next fix should add an I2C retry/reset path or remove the condition causing AL before trusting MIPI symptoms.
- LED23 ON plus LED29/30/31 ON: sensor likely entered HS; continue downstream CSI/DDR/HDMI content debugging.

---

## 84. 2026-07-09 sixty-third iteration result: I2C clean and MIPI HS/FIFO active; move to CSI/framebuffer/content (Codex)

### Board result reported by user

With the iteration-83 final stream-on quality + MIPI boundary probe:

- LED24 OFF
- LED32 OFF
- LED33 OFF
- All other LED18-23 and LED25-31 ON

Timing report from the user screenshot:

| Item | Value |
|------|-------|
| WNS | 1.798 ns |
| WHS | 0.008 ns |
| axi0_ACLK | 312.305 MHz |
| i_fb_clk | 246.002 MHz |
| mipi_clk | 225.479 MHz |
| i_sysclk_div2 | 150.761 MHz |
| hdmi_tx_slow_clk | 288.101 MHz |
| mipi_rx_ck0_CLKOUT | 208.986 MHz |
| mipi_rx_ck1_CLKOUT | 386.698 MHz |

The image is still full-screen random colored noise / horizontal bands.

### Interpretation

This result clears the previous I2C suspicion:

- LED21/22/23 ON and LED24 OFF mean the final `0100=01` stream-on was launched, completed, and completed cleanly.
- LED32/33 OFF mean no aggregate RXACK or arbitration-lost was observed.
- LED29/30/31 ON mean the receiver saw HS enable/termination, HS data, and byte clock/FIFO activity.

The active fault is now downstream of MIPI physical entry:

- CSI packet decode / RAW10 + 4ppc qualification,
- RAW10-to-RAW8 packing,
- framebuffer write/read,
- debayer/Bayer phase and RGB ordering,
- HDMI input selection / content path.

Because the screen is colored noise while the raw camera stream is Bayer/RAW, the next build intentionally bypasses debayer/white-balance for HDMI and displays a grayscale copy of the framebuffer raw samples. This is both a minimal content fix and a diagnostic split:

- If the image becomes a coherent grayscale camera view, CSI/DDR/HDMI are alive and the next fix is Bayer phase/debayer.
- If the image remains random noise, focus on RAW10 unpacking / CSI payload alignment before debayer.

### RTL changes prepared for next build

`final_project/fpga/rtl/top/top.v`

- Added `HDMI_RAW_GRAY_BYPASS = 1'b1`.
- HDMI path now uses ch0/ch1 framebuffer raw two-pixel output as grayscale RGB:
  - `{gray0, gray0, gray0, gray1, gray1, gray1}`
  - timing comes directly from `ch*_hs/vs/de`.
- Debayer and white-balance remain instantiated but are bypassed for HDMI output in this probe.
- Reassigned LED21-33 to CSI/framebuffer/HDMI-content checkpoints.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | final ch0 stream-on completed cleanly |
| LED22 | G2 | ch0 MIPI byte clock or FIFO activity seen |
| LED23 | K6 | ch0 CSI datatype RAW10 seen |
| LED24 | J3 | ch0 CSI pixel-per-clock 4 seen |
| LED25 | L6 | ch0 CSI RAW10/4ppc seen |
| LED26 | K4 | ch0 framebuffer input VS seen |
| LED27 | K3 | ch0 framebuffer input DE seen |
| LED28 | M5 | ch0 framebuffer write FIFO accepted pixels |
| LED29 | M6 | ch0 DDR write frame done |
| LED30 | N7 | ch0 DDR read started and returned data |
| LED31 | P7 | selected HDMI CDC active, level-ready, and data changing |
| LED32 | P6 | hdmi_top is using input video |
| LED33 | R6 | BAD: framebuffer/CDC underflow or `hdmi_video_ready` low |

### How to read the next board result

- LED23/24/25 OFF while LED22 ON: MIPI data exists but CSI parser format/pixel-per-clock is not the expected RAW10/4ppc.
- LED26/27 OFF with LED25 ON: CSI parser reports format, but frame timing is not reaching framebuffer.
- LED28 OFF with LED27 ON: framebuffer input timing exists but write FIFO is not accepting pixels.
- LED29/30 OFF with LED28 ON: DDR write/read path is the blocker.
- LED31 ON, LED32 ON, LED33 OFF: HDMI is consuming the selected raw-gray input; judge image content.
- If the picture becomes coherent grayscale: next step is debayer/Bayer phase/RGB order.
- If it remains random noise with LED23-32 ON and LED33 OFF: next step is RAW10 byte/pixel unpacking and lane alignment.

---

## 85. 2026-07-09 sixty-fourth board result: RAW-gray probe regressed ready/MIPI-visible LEDs (Codex)

### Board result reported by user

With the iteration-84 RAW-gray HDMI bypass + CSI/framebuffer/HDMI probe:

- LED18 ON
- LED19 ON
- LED20 ON
- LED33 ON
- LED21-32 OFF

Timing report from the user screenshot:

| Item | Value |
|------|-------|
| WNS | 1.714 ns |
| WHS | 0.026 ns |
| axi0_ACLK | 304.321 MHz |
| i_fb_clk | 246.063 MHz |
| mipi_clk | 232.775 MHz |
| i_sysclk_div2 | 158.680 MHz |
| hdmi_tx_slow_clk | 259.538 MHz |
| mipi_rx_ck0_CLKOUT | 181.127 MHz |
| mipi_rx_ck1_CLKOUT | 374.813 MHz |

The image still shows the same full-screen colored noise / bands.

### Interpretation

This result conflicts with the immediately previous build, where final stream-on was clean and MIPI HS/FIFO activity was visible. The iteration-84 RTL only changed HDMI content selection and the LED map; it should not have intentionally disabled I2C or MIPI.

The most likely explanation is that the probe itself became too fragile:

- LED21 used an I2C clean pulse observed after crossing into the `i_sysclk_div2` latch path; a narrow pulse can be missed.
- The RAW-gray bypass moved HDMI timing/data directly to framebuffer raw outputs, making the HDMI-ready BAD LED33 dominate when framebuffer readiness is not stable.
- Therefore this result should not be interpreted as proof that camera stream-on or MIPI HS permanently failed.

### RTL changes prepared for next build

`final_project/fpga/rtl/top/top.v`

- Disabled the RAW-gray HDMI bypass:
  - `HDMI_RAW_GRAY_BYPASS = 1'b0`
- Restored the normal RGB/debayer HDMI path for this confirmation build.
- Added mipi-clock-domain sticky latches for final stream-on clean/error:
  - `ch0_i2c_stream_on_clean_mipi_latch`
  - `ch0_i2c_stream_on_error_mipi_latch`
- Synchronized those sticky latches into `i_sysclk_div2` before driving LEDs.
- LED21 now uses the stable synchronized stream-on-clean latch instead of the earlier pulse-derived latch.
- LED33 remains BAD, now including stream-on error plus framebuffer/CDC/ready failures.

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | final ch0 stream-on completed cleanly, mipi_clk sticky |
| LED22 | G2 | ch0 MIPI byte clock or FIFO activity seen |
| LED23 | K6 | ch0 CSI datatype RAW10 seen |
| LED24 | J3 | ch0 CSI pixel-per-clock 4 seen |
| LED25 | L6 | ch0 CSI RAW10/4ppc seen |
| LED26 | K4 | ch0 framebuffer input VS seen |
| LED27 | K3 | ch0 framebuffer input DE seen |
| LED28 | M5 | ch0 framebuffer write FIFO accepted pixels |
| LED29 | M6 | ch0 DDR write frame done |
| LED30 | N7 | ch0 DDR read started and returned data |
| LED31 | P7 | selected HDMI CDC active, level-ready, and data changing |
| LED32 | P6 | hdmi_top is using input video |
| LED33 | R6 | BAD: final stream-on error, framebuffer/CDC underflow, or `hdmi_video_ready` low |

### How to read the next board result

- If LED21 returns ON and LED22-31 also return ON, the iteration-84 regression was caused by the RAW-gray probe / fragile LED crossing; continue content debugging without that bypass.
- If LED21 remains OFF, final stream-on clean really is not being captured even by the mipi-clock sticky latch; return to I2C stream-on/AL/RXACK.
- If LED21 ON but LED22 OFF, stream-on is clean but MIPI byte/FIFO activity is absent in this build; re-run the MIPI LP/HS boundary split.
- If LED31/32 ON and LED33 OFF with the normal RGB path restored, HDMI is consuming input again; continue with debayer/Bayer phase or RAW10 unpacking based on picture content.

---

## 86. 2026-07-09 sixty-fifth board result: stable stream-on latch still absent, return to direct I2C split (Codex)

### Board result reported by user

With the iteration-85 normal RGB path restored and mipi-clock sticky stream-on latch:

- LED18 ON
- LED19 ON
- LED20 ON
- LED33 ON
- LED21-32 OFF

Timing report from the user screenshot:

| Item | Value |
|------|-------|
| WNS | 1.243 ns |
| WHS | 0.006 ns |
| axi0_ACLK | 266.170 MHz |
| i_fb_clk | 220.070 MHz |
| mipi_clk | 235.073 MHz |
| i_sysclk_div2 | 160.102 MHz |
| hdmi_tx_slow_clk | 288.268 MHz |
| mipi_rx_ck0_CLKOUT | 206.441 MHz |
| mipi_rx_ck1_CLKOUT | 408.998 MHz |

The screenshot still shows full-screen colored noise / bands.

### Interpretation

This is the second consecutive build where only the base reset/DDR/channel LEDs plus BAD LED33 are ON. Because iteration 85 already disabled the RAW-gray bypass and made stream-on clean/error sticky in the `mipi_clk` domain, the next useful step is no longer downstream CSI/DDR/HDMI probing.

Current focused hypothesis:

- The selected ch0 reset and I2C release path may not be reaching the final stream-on sequence in this build.
- Or the ROM/index reaches the stream-on entry, but the `0100=01` write launch/done/clean detection is missing.
- Or the final stream-on write is attempted but completes with RXACK/AL/error.

### RTL changes prepared for next build

`final_project/fpga/rtl/top/top.v`

- Kept the normal RGB/debayer HDMI path selected.
- Reassigned LED18-33 to a direct ch0 reset/I2C/stream-on split.
- Added `mipi_clk` sticky latches for:
  - I2C init done
  - any I2C write enable
  - any I2C write done
  - I2C config done
  - stream-on index reached
  - stream-on `0100=01` write launched
  - stream-on write done
  - stream-on clean/error
  - aggregate RXACK / arbitration-lost status

### Next LED18-33 map

| LED | Pin | Meaning |
|-----|-----|---------|
| LED18 | B2 | DDR configured |
| LED19 | E3 | ch0 selected (`~channel_sel`) |
| LED20 | F3 | global reset released |
| LED21 | F2 | ch0 `reset_pixel_n` released |
| LED22 | G2 | ch0 `i2c_rst_n` released |
| LED23 | K6 | ch0 I2C `init_done` seen |
| LED24 | J3 | ch0 any I2C `wr_en` seen |
| LED25 | L6 | ch0 any I2C `wr_done` seen |
| LED26 | K4 | ch0 I2C `cfg_done` reached |
| LED27 | K3 | ch0 stream-on index reached |
| LED28 | M5 | ch0 stream-on write launched (`0100=01`) |
| LED29 | M6 | ch0 stream-on write done |
| LED30 | N7 | ch0 stream-on completed cleanly |
| LED31 | P7 | BAD: ch0 stream-on write completed with error |
| LED32 | P6 | BAD: ch0 aggregate I2C RXACK seen |
| LED33 | R6 | BAD: ch0 aggregate I2C arbitration lost seen |

### How to read the next board result

- LED21 OFF: ch0 pixel-domain reset is not released; stop at reset/clocking.
- LED22 OFF with LED21 ON: delayed I2C reset is not releasing; inspect `i2c_rst_cnt` / `I2C_RST_DELAY_BIT`.
- LED23 OFF with LED22 ON: I2C master initialization did not complete.
- LED24 OFF with LED23 ON: register sequencer did not launch writes.
- LED25 OFF with LED24 ON: write transaction did not complete.
- LED26 OFF with LED25 ON: writes happen but full ROM config does not finish.
- LED27 OFF with LED26 possibly OFF/ON: sequencer did not reach the stream-on ROM index.
- LED28 OFF with LED27 ON: stream-on index was reached but the `0100=01` launch detector missed or ROM data is not as expected.
- LED29 OFF with LED28 ON: final stream-on write launched but did not complete.
- LED30 ON and LED31/32/33 OFF: final stream-on path is clean; return to MIPI/CSI boundary.
- LED31/32/33 ON: final stream-on or earlier I2C transaction has an explicit error condition; inspect ACK/arbitration and I2C status sampling.
