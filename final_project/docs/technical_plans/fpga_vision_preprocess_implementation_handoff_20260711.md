# FPGA 视觉预处理模块实现交接

> 日期：2026-07-11  
> 状态：第一阶段 RTL 已完成，已以 ch1 Debayer 旁路方式接入正式 C 盘工程并通过带 `mark_debug` 探针的正式 map；尚未接入 APB、OSD、CPU 或双摄像头  
> 适用分支：`dev/libaoxun688`  
> 关联方案：[fpga_vision_preprocess_execution_plan_20260711.md](fpga_vision_preprocess_execution_plan_20260711.md)

## 1. 交接结论

预处理模块已做成一个单通道、可复用的 RGB888 2 pixels-per-clock (2ppc) 流水组件。它从 Debayer 的 48-bit 输出中拆出相邻两个 RGB 像素，在可配置 ROI 内统计颜色、亮度和通用前景信息，并在帧结束时产生稳定快照。

当前模块只在像素时钟域中工作，不依赖摄像头、DDR、CPU、APB 或 HDMI。因此摄像头链路尚未稳定时，预处理 RTL 仍可继续验证和迭代。

正式边界不变：FPGA 输出 ROI 和原始统计特征；板上 CPU 负责颜色/形状/尺寸最终分类、阈值管理、任务匹配与 myCobot 控制。

## 2. 已新增的文件

| 路径 | 内容 |
|---|---|
| `fpga/rtl/roi_crop/vision_stream_adapter_2ppc.v` | 2ppc 字节序适配、帧/行/像素坐标生成。 |
| `fpga/rtl/roi_crop/roi_window_2ppc.v` | ROI 半开区间命中判断。 |
| `fpga/rtl/feature_extract/pixel_mask_2ppc.v` | 红/蓝/黄及通用前景掩码。 |
| `fpga/rtl/feature_extract/feature_accumulator_2ppc.v` | ROI 累加、面积、bbox 统计。 |
| `fpga/rtl/feature_extract/feature_snapshot.v` | 帧级稳定快照、ack 与丢帧计数。 |
| `fpga/rtl/feature_extract/vision_preprocess_channel.v` | 单通道封装顶层。 |
| `tests/fpga_sim/feature_extract/tb_vision_preprocess_channel.v` | 自检 testbench。 |
| `tests/fpga_sim/feature_extract/run_vision_preprocess_iverilog.ps1` | Icarus Verilog 执行脚本。 |

对应目录 README 已补充模块说明和仿真入口。

## 3. 输入语义

预处理模块顶层为：

```text
vision_preprocess_channel
```

输入视频接口：

| 信号 | 宽度 | 语义 |
|---|---:|---|
| `i_clk` | 1 | 像素时钟；预计接现有 `i_sysclk_div2` 域。 |
| `i_rst_n` | 1 | 低有效像素域复位。 |
| `i_vs/i_hs/i_de/i_valid` | 各 1 | 上游视频时序和数据有效指示。 |
| `i_rgb_2ppc` | 48 | 固定为 Debayer `rgb_datax2_o` 的 `{B1,G1,R1,B0,G0,R0}`。 |

像素 0 是左侧/偶像素：`{R0,G0,B0} = i_rgb_2ppc[7:0], [15:8], [23:16]`。

像素 1 是右侧/奇像素：`{R1,G1,B1} = i_rgb_2ppc[31:24], [39:32], [47:40]`。

坐标器以 `i_de && i_valid` 的上升沿作为主要行起点，同时接受 HS 上升沿作为交叉核验。接入真实视频链路前，必须使用 Debayer 实际波形确认 HS 极性、DE/valid 关系和完整行时序。

## 4. 配置与帧边界

当前像素域配置端口包含：

- ROI：`i_cfg_roi_x0/y0/x1/y1`。
- 背景参考：`i_cfg_bg_r/g/b`。
- 前景差分和亮度范围：`i_cfg_fg_diff_min`、`i_cfg_luma_min/max`。
- 红、蓝、黄的通道差阈值。
- `i_cfg_enable` 与 `i_snapshot_ack`。

ROI 定义是半开区间：

```text
[x0, x1) x [y0, y1)
```

模块在 `vs` 上升沿把直接配置锁存为像素域影子配置，因此一帧内不会混用新旧参数。当前直接配置仅服务 testbench；接入 APB 后必须改为正式 `staging -> commit -> VSYNC active shadow` 协议，不能把 APB 多位总线直接接到这些输入。

## 5. 已输出的特征

`vision_preprocess_channel` 输出：

| 输出 | 语义 |
|---|---|
| `o_snapshot_valid` / `o_frame_id` | 当前稳定快照和帧号。 |
| `o_roi_pixel_count` | ROI 内参与统计的像素数。 |
| `o_sum_r/g/b/y` | ROI 内 RGB 和亮度总和；均值由 CPU 用总和/像素数计算。 |
| `o_red/blue/yellow_area` | ROI 内三种轻量颜色掩码面积。 |
| `o_fg_area` | ROI 内通用前景掩码面积。 |
| `o_bbox_min/max` | `{y[15:0], x[15:0]}`，前景 bbox 的闭区间最小/最大命中坐标。 |
| `o_center` | bbox 中点 `{y[15:0], x[15:0]}`。 |
| `o_status` | bit 0 = 空前景，bit 1 = 非法 ROI。 |
| `o_dropped_frames` | 前一快照未 ack 时到达的新完整帧计数。 |

空前景约定：`fg_area=0`、bbox 和 center 全为 0，且 `o_status[0]=1`。CPU 不得使用该帧的 bbox/center。

通用前景掩码定义为：ROI 内像素相对背景 RGB 至少一个通道的绝对差达到阈值，并位于亮度范围内。它不等于 `red | blue | yellow`，因此可以支持白色和黑色物块的填充率统计。

## 6. 快照行为

1. 当前帧开始时清空运行计数。
2. 下一帧 `vs` 上升沿到来时，将前一帧统计锁存为快照。
3. `o_snapshot_valid=1` 时，快照保持不变。
4. 收到 `i_snapshot_ack` 后，允许接收下一快照。
5. 若旧快照未确认而又完成一帧，旧快照不被覆盖，`o_dropped_frames` 加一。

当前 ack 是同一像素域的直接输入，仅用于自检。CPU/APB 接入时必须改为带匹配 `frame_id` 的 CDC 握手。

## 7. 已完成验证

已运行 Efinity 2025.2 独立映射：新增 6 个 RTL 文件作为唯一源文件，顶层为 `vision_preprocess_channel`。

结果：

```text
EFX_MAP: PASS
EFX_LUT4: 1291
EFX_FF:   1042
EFX_ADD:   775
```

无语法、模块展开、组合环、未连接端口或 CDC warning。

唯一剩余提示是 `pixel_mask_2ppc.v` 中 `abs_diff8` 函数临时信号被 Efinity 识别为冗余并删除。这是综合器优化提示，不构成时序结论；正式工程全量综合时仍须复核。

testbench 已覆盖：

- 2ppc 字节序。
- ROI 边界与跨 even/odd 像素。
- RGB/Y 总和、颜色/前景面积、bbox 和中心点。
- 快照未 ack 时的保持和 dropped frame 计数。
- ROI 无像素时的空前景快照。

本机未找到 Icarus、Verilator 或 ModelSim 命令，因此 testbench 尚未实际运行。具备 Icarus 的环境执行：

```powershell
Set-Location final_project/tests/fpga_sim/feature_extract
.\run_vision_preprocess_iverilog.ps1
```

## 8. 正式工程旁路接入记录

### 2026-07-11 ch1 tap

已在 `fpga/rtl/top/top.v` 实例化：

```text
u_preprocess_ch1_tap
```

接线为：

```text
ch1 framebuffer -> debayer_top1 -> rgb1_vs/hs/de/valid + rgb1_datax2
                                      -> u_preprocess_ch1_tap
```

该路径已由当前源码核实：

- `debayer_top1` 的时钟为 `i_sysclk_div2`，复位为 `pixel_data_en`。
- `rgb1_datax2` 仍是 `{B1,G1,R1,B0,G0,R0}`。
- ch1 是当前默认 HDMI 通道；HDMI 白平衡旁路时同样使用 `rgb1_*` / `rgb1_datax2`。
- 预处理实例不回写或替换 Debayer、Framebuffer、HDMI、LED、CPU 或任何摄像头控制信号。

为保持 bring-up 行为不变，本次实例的配置固定为全坐标范围和保守阈值；`i_cfg_enable=1`、`i_snapshot_ack=1`，只影响旁路内部统计。`mem_test.xml` 已加入 6 个预处理 RTL 文件。

帧号、ROI 像素数、红/蓝/黄/前景面积、bbox、中心点、状态和快照有效位已导出为顶层 `(* mark_debug = "true" *)` 导线。这些导线不接 HDMI、CPU、LED 或控制信号；Efinity 已生成 `debug_profile.mark_debug.json`，其中 11 组探针均属于 `i_sysclk_div2` 域。

正式工程 map 从 ASCII 路径 `D:\cicc_cbm_link\final_project\fpga\efinity\mem_test.xml` 运行，结果：

```text
map: PASS
EFX_ADD : 2081
EFX_LUT4: 11945
EFX_FF  : 10484
```

该 map 报告了 586 条既有工程/IP warning；未发现本次新增模块的语法、缺模块、组合环、未连接端口或 CDC warning。`pixel_mask_2ppc.v` 的 `abs_diff8` 仍有独立映射时已知的冗余临时信号优化提示。

重要限制：`mark_debug` profile 证明 map 已识别这些探针，但尚未完成 Efinity Debugger 自动封装、PNR 或 bitstream 验证。map PASS 只证明源文件、顶层实例、当前时钟/复位接线及探针识别可综合；它不证明任何真实摄像头特征。

### D 盘状态

`D:\final_project` 仍是手工 build/flash 树，但本次核验中其 `top.v` 和 `mem_test.xml` 的 SHA-256 均与 C 盘仓库版本不同。因此：

- 未把本次预处理改动复制到 D 盘。
- 未运行 D 盘构建、PNR、bitstream 或 JTAG 烧录。
- 未执行机械臂动作。

在 D 盘同步前，必须先保留其现有改动并再次比较差异；不得使用盲目覆盖。

## 9. 明确未做

- 未在 D 盘 build/flash 树中同步、构建或烧录本次改动。
- 未完成可上板采集的 Debugger flow；现有 `mark_debug` profile 仅作为后续探针配置输入。
- 未用真实帧确认 Debayer 时序或统计结果。
- 未实现 APB 寄存器、CPU CDC、真实 `frame_id` ack 匹配或中断。
- 未实现 OSD、DDR ROI 缓冲、双路实例化。
- 未做颜色、形状、尺寸最终分类，也未做机械臂控制逻辑。

## 10. 接手者的第一步

在改 `top.v` 前，先完成最小事实核查：

1. 从实际 Debayer 输出波形确认 48-bit 字节序、`vs/hs/de/valid` 极性和时序。
2. 用 Icarus/ModelSim 运行现有 testbench，保存 PASS 日志。
3. 在已生成的 `debug_profile.mark_debug.json` 基础上，确认可工作的 Efinity Debugger capture 流程；不要触发此前已知的缺失 `debug_top.v` 自动包装失败。
4. 填写并评审 `物理接口 -> ch0/ch1 -> wb0/wb1 -> CPU Cam0/Cam1` 映射表。
5. 在同步 D 盘前先比较并合并其当前 `top.v`/`mem_test.xml` 差异；随后再做 D 盘 map/PNR/bitstream 审查。

顶层接入不得改变 HDMI 原数据路径。先只分叉像素流到预处理，验证内部特征；单通道通过后再对称实例化第二通道。

## 11. 必须保持的约束

- FPGA 只输出高速前端、ROI 和统计特征；分类/参数管理在 CPU。
- 不恢复纯 FPGA 视觉识别或纯 RTL myCobot 控制。
- 修改 `top.v`、时钟/复位、CDC、`mem_test.xml`、`.peri.xml`、IP 设置或 `constrain.sdc` 前必须走 Codex Review Packet。
- 双路扩展使用同一个 `vision_preprocess_channel` 模块，不能复制后分叉维护。
- APB/SoC 接入前必须先生成并审查 `generated_soc_summary_YYYY-MM-DD.md`。
- 真实源码、工程 XML、构建日志和上板现象优先于本文件中的规划性说明。

## 12. 续作健康检查

2026-07-11 续作前已运行 `tools/agent_handoff_health_check.ps1`（进程级 ExecutionPolicy Bypass）。检查确认仓库根、分支 `dev/libaoxun688`、`HEAD=b0b9c21`、关键 FPGA 文件和交接目录均可用；工作树为预期 dirty。唯一警告为本机未安装 `pymycobot`，与本次 FPGA 预处理无关，未执行任何机械臂动作。

## 13. PNR 与 Debugger 路径检查

本机仅提供 GUI `dbg_wizard.exe`，没有 `efx_debugger.exe` 命令行采集工具。现有 `debug_profile.wizard.json` 是旧的 USER2 多时钟配置，未包含本次 ch1 预处理探针；map 自动生成的 `debug_profile.mark_debug.json` 只证明 11 组 `mark_debug` 网表可被向导选择。

2026-07-11 在 `D:\cicc_cbm_link\final_project\fpga\efinity` 做了只读构建路径检查：

1. 正式 map 再次 `PASS`。
2. `efx_run.bat ... -f pnr` 因项目 `debugger.auto_instantiation=on` 自动携带 `--enable_dbg`，此模式要求 `mem_test.dbg.vdb`；当前 map 只产生 `mem_test.vdb`，不能直接进入 Debugger PNR。
3. 普通 VDB 的 PNR 已完成打包与 SDC 读取，但工程有 2,288 个未约束 I/O，Efinity 在 I/O 放置时触发内部 `Assertion: '!available_io_sites.empty()' failed: outpad`。因此没有有效的 Setup/Hold Slack，不能作为时序签核。

结论：本次失败不是预处理模块语法、时钟域或 `mark_debug` 探针识别失败。下一步必须由熟悉 Efinity GUI 的成员在受控项目副本中运行 Debug Wizard，生成包含 ch1 预处理探针的 `.dbg.vdb`；同时独立修复或核实正式工程的接口/约束完整性。不得手工伪造或复制 VDB，不得在未合并的 `D:\final_project` 树上覆盖构建。

## 14. 暂缓与系统接口准备

用户已明确批准暂缓以下四项：testbench 实跑、真实 Debayer 波形语义确认、PNR/时序/bitstream 闭环、板级特征采集。在恢复前，它们必须保持 `NOT VERIFIED`，且特征不得驱动 CPU 分类、OSD 或机械臂动作。

暂缓期间已完成仅文档化的系统准备：

- `docs/architecture/generated_soc_summary_2026-07-11.md`：确认当前树缺少 Efinity 生成的 `soc.h`、生成 SoC 摘要、APB slave 和 `results_cdc` RTL；因此 APB 基址、端口、时钟、复位、地址宽度都未确认。
- `integration/preprocess_apb_cdc_contract_draft_20260711.md`：冻结 frame-stable snapshot、`frame_id` ack、staging -> commit -> VSYNC active 配置，以及 CPU-to-OSD shadow 规则。

该准备不授权修改 `top.v`、SoC/IP 设置、`mem_test.peri.xml`、`constrain.sdc`、`board_io.h` 偏移或任何 CPU/APB/OSD/ch0 RTL。恢复集成的首个输入必须是匹配实际构建配置的生成 `soc.h` 和 APB 端口/时钟信息，并附新的 Review Packet。
