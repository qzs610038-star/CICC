# Review Packet: ch1 Preprocess Tap

> 日期：2026-07-11  
> 范围：将已完成的单通道预处理模块以旁路方式接入当前 `top.v` 及正式 Efinity 源文件清单  
> 不涉及：MIPI/I2C、Framebuffer/AXI、HDMI 数据路径、OSD、APB/CPU、约束、机械臂

## 目标

确认正式工程可以从 ch1 Debayer 输出编译预处理模块，同时保证当前摄像头与 HDMI bring-up 行为不改变。

## 输入链路与依据

当前 `top.v` 中：

```text
ch1 framebuffer -> debayer_top1 -> rgb1_vs/hs/de/valid + rgb1_datax2
```

- `debayer_top1` 的时钟为 `i_sysclk_div2`，复位为 `pixel_data_en`。
- `rgb1_datax2` 为 `{B1,G1,R1,B0,G0,R0}` 双像素 Debayer 原始输出。
- HDMI ch1 在白平衡旁路模式下也直接使用 `rgb1_*` / `rgb1_datax2`。
- 当前板级状态中 ch1 是默认 HDMI 通道，但尚未观测到稳定 parsed CSI DE；因此本次只做静态旁路接线和综合验证，不把它当成有效图像特征验证。

## 修改内容

新增 Efinity design file：

```text
rtl/roi_crop/vision_stream_adapter_2ppc.v
rtl/roi_crop/roi_window_2ppc.v
rtl/feature_extract/pixel_mask_2ppc.v
rtl/feature_extract/feature_accumulator_2ppc.v
rtl/feature_extract/feature_snapshot.v
rtl/feature_extract/vision_preprocess_channel.v
```

新增顶层实例：

```text
rgb1_* / rgb1_datax2 -> u_preprocess_ch1_tap
```

本次配置固定为完整像素坐标范围（`[0, 16'hffff)`），使用保守的通道差阈值、`i_cfg_enable=1` 和 `i_snapshot_ack=1`，因此每帧都可生成新的内部快照。快照导出到标记 `(* mark_debug = "true" *)` 的顶层导线：帧号、状态、ROI 像素数、颜色面积、前景面积、bbox 和中心点。

这些导线不连接 HDMI、CPU、LED 或任何控制信号；它们使用 Efinity `mark_debug` 属性作为下阶段 Debugger probe 或受审查寄存器窗口的稳定像素域源。当前 map 不启用 Debugger 自动包装。

## 时钟与复位

| 项 | 结论 |
|---|---|
| 模块时钟 | `i_sysclk_div2` |
| 模块复位 | `pixel_data_en`，沿用 Debayer 与白平衡链路 |
| 新 CDC | 无；模块全部在同一像素域 |
| HDMI/DDR/AXI 时钟 | 不修改 |

## 风险控制

- 旁路实例不回写上游，也不替换 `rgb1_*` 或 `hdmi1_*`。
- 固定配置只影响内部统计，不改变任何视频或外设控制路径。
- `mark_debug` 和 `keep_hierarchy` 已由正式 map 核验：生成的 `debug_profile.mark_debug.json` 列出 11 组 `i_sysclk_div2` 域探针。不得把 map 成功误报为完整 Debugger build 成功。
- 下一阶段需先定义受控 Debugger probe 或寄存器出口，不能把未验证特征直接用于 CPU 或 OSD。
- 该改动不解除 APB/SoC、CDC、OSD 或双通道 Gate。

## 验证

1. 已运行正式 `mem_test.xml` 的 map，结果 `map : PASS`。
2. 正式 map 资源：`EFX_ADD=2081`、`EFX_LUT4=11945`、`EFX_FF=10484`。
3. 工程总计 586 条既有/IP warning；未发现本次模块的语法、缺模块、组合环、未连接端口或 CDC warning。`abs_diff8` 的冗余临时信号提示保留后续复核。
4. `debug_profile.mark_debug.json` 包含 `preprocess_ch1_snapshot_valid`、`frame_id`、ROI 像素数、三类颜色面积、前景面积、bbox、中心点和状态，共 11 组数据/触发探针。
5. 未同步 `D:\final_project`，未烧录，不执行机械臂动作。

## 后续批准条件

- 真实 ch1 Debayer 波形确认 `vs/hs/de/valid` 语义。
- Icarus/ModelSim testbench 得到 PASS 日志。
- 先由 Efinity GUI Debug Wizard 生成含 ch1 探针的 `.dbg.vdb`。项目自动实例化开启时，命令行 PNR 会要求该专用 VDB；普通 map VDB 不能替代它。
- 独立闭合工程 I/O/约束问题：2026-07-11 PNR 已越过打包和 SDC 解析，但因 2,288 个未约束 I/O 触发 Efinity `outpad` 内部断言，未生成可用时序报告。
- CPU/APB 接口仍需另行审查。
