# 摄像头不可用期间的下游并行推进方案

> 日期：2026-07-12  
> 状态：待审核，尚未实施  
> 适用范围：`final_project/`  视觉链路当前未产生稳定 CSI 数据流时的下游开发与验证  
> 前提：不把测试输入、PC 脚本或 mock 结果描述为真实摄像头、板上 SoC 或机械臂正式闭环

> 实施更新（2026-07-12）：已按用户指令实现 `synthetic_2ppc_source.v`，并只接入 `u_preprocess_ch1_tap` 的输入选择。默认 `PREPROCESS_CH1_USE_SYNTHETIC_SOURCE=1`；HDMI、CSI、DDR 和机械臂路径不变。独立 map 和正式工程 map 已通过；RTL 仿真、PNR、bitstream 和板级采集仍为 `NOT VERIFIED`。

## 1. 决策与目标

当前 ch1 已完成 I2C stream-on 交互，但尚未观测到稳定的 `rx_out_de1`。故障边界仍在 MIPI LP/HS、DPHY、接收 FIFO 和 CSI 解析之间，继续修改 framebuffer、Debayer、HDMI、CPU 或机械臂下游不能解决该硬件问题。

本方案的决策是：**保留摄像头恢复支线，同时以可追溯的合成输入完成下游逻辑与接口验证。**

可提前推进：

- 四任务目标语义、关系匹配和“一轮一事务”状态机。
- 分类器输入/输出、参数表、日志与评分可解释输出。
- `feature_snapshot_t` 的回放数据集和 CPU host 回归。
- myCobot 协议、控制器、超时/重试/停机逻辑的 mock 集成。
- FPGA 预处理的独立仿真，以及不依赖 MIPI 的合成像素源验证。
- 在取得真实 SoC 生成物后，APB 寄存器、CDC 和 OSD 的输入无关实现。

本方案不提前宣称：

- 摄像头颜色、形状、尺寸识别准确率。
- CPU 与真实 FPGA/APB、OSD 的板级连通。
- 机械臂真实动作、轻取轻放、落放精度或比赛全流程通过。

## 2. 总体结构

```text
                       摄像头恢复支线（独立）
camera -> MIPI/DPHY -> CSI parser -> Debayer -> feature source
                                                |
                                                | 恢复后只替换输入
                                                v
合成像素源 -> vision_preprocess_channel -> stable feature snapshot
                                                |
                                                v
                                    APB/CDC adapter（待 SoC 生成物）
                                                |
                                                v
feature replay ----------------------------> CPU classify / task FSM
                                                |
                                                +--> OSD semantic model
                                                |
                                                +--> arm-controller mock
```

核心原则是下游只依赖**稳定特征快照契约**，而不依赖数据来自摄像头还是合成源。正式构建中不允许永久存在“摄像头失效时自动切换模拟目标并驱动机械臂”的回退逻辑。

## 3. 三层推进路线

### L1：Host 回放，先完成任务与控制逻辑

新增一个只用于 CPU host 测试的回放适配层。它从版本化数据集读取或静态提供 `feature_snapshot_t` 序列，模拟每路的 `frame_id`、有效帧、空帧、重复帧、丢帧和 ACK 顺序；它不得访问 MMIO、串口或 PC `pymycobot`。

建议目录与交付物：

```text
final_project/cpu/tests/fixtures/feature_replay/
  README.md
  schema.md
  task01_color_cube.csv
  task02_shape_color.csv
  task03_delta_10mm.csv
  task04_delta_le_5mm.csv
  fault_empty_duplicate_stale_ack.csv

final_project/cpu/tests/
  test_feature_replay.c
  test_round_transaction.c
  test_competition_tasks.c
  test_main_arm_mock_integration.c
```

每条 fixture 至少包含：`camera_id`、`frame_id`、`red_area`、`blue_area`、`yellow_area`、`fg_area`、bbox、`height_px`、预期颜色/形状/尺寸、预期任务判断与理由。白/黑条目必须通过 `fg_area`、bbox 和 RGB/Y 汇总补充，而不是伪装成红/蓝/黄面积。

L1 必须完成的语义：

- `task_mode`：任务一颜色方块、任务二形状加颜色、任务三差值等于 10 mm、任务四差值不大于 5 mm。
- `target_color`：白、黑、红、蓝、黄及无颜色约束。
- `reference_size_mm_x10`：统一使用 20/25/30 的 0.1 cm 编码；关系判定使用整数差值，禁止浮点比较。
- `apply/lock`、`round_reset` 与目标回读；一轮期间目标不得被动态覆盖。
- 状态机：`WAIT_OBJECT -> STABLE_RECOGNITION -> RESULT_LATCHED -> DECISION_LATCHED -> EXECUTE_OR_SKIP -> ROUND_DONE -> WAIT_REMOVE`。
- 每轮产生可解释语义：识别结果、`TARGET/NON_TARGET/UNSURE`、`GRAB/SKIP/FAULT` 与原因码。
- 同一物体的连续帧只能触发一次 `arm_controller_request_grab()`；`SKIP` 必须有不执行理由。

L1 通过门：四任务、五色、三形状、三尺寸、UNKNOWN、空前景、重复帧、陈旧 ACK、目标切换和放弃路径均有确定性单测；20 轮随机序列可在 host 上完成且无重复 GRAB。

### L2：FPGA 合成像素流，验证“像素到特征”而非摄像头

复用 `vision_preprocess_channel`，不重写统计模块。新增一个只在仿真或专用验证顶层中使用的 `synthetic_2ppc_source`：按同一 `vs/hs/de/valid` 与 `{B1,G1,R1,B0,G0,R0}` 接口输出可参数化色块、方形、圆形和三角形。

建议分两步：

1. 仿真优先：扩展现有 `tb_vision_preprocess_channel.v` 或新增 `tb_synthetic_preprocess_pipeline.v`，产生 1920x1080 等比例缩小或参数化测试帧。验证 ROI、边界、快照、颜色面积、前景面积、bbox、中心点、ACK 和 dropped frame。
2. 板上专用验证顶层可选：只有在 PNR/I/O 约束问题关闭、且 GUI Debugger 或 HDMI 观察通道可用时才做。合成源只能由**构建时显式专用顶层或锁定的验证常量**启用，禁止通过现场拨码悄悄切入正式比赛路径。

L2 不需要 MIPI、DDR 或真实 Debayer；其结论只能写为“合成 RGB 2ppc -> 统计快照正确”。真实 CSI 数据恢复后，仍必须确认 Debayer 的 `vs/hs/de/valid` 语义和 48-bit 字节序。

L2 通过门：仿真产生可存档 PASS 日志和波形；所有快照字段与软件参考模型一致；不会修改 HDMI/MIPI/DDR 主路径。板上验证若执行，必须单独保存 bitstream 身份、构建模式和探针证据。

### L3：接口实现，待 SoC 生成物而非待摄像头

摄像头不可用并不阻止设计 APB/CDC/OSD，但实际 RTL 实施仍以生成 `soc.h`、真实 APB 端口/时钟/复位为前置门。拿到这些生成物后：

- 实现特征快照的 `valid + frame_id` 匹配 ACK CDC。
- 实现配置 `staging -> commit -> VSYNC active` CDC。
- 以同一寄存器文件接收来自合成源或未来摄像头源的快照。
- 实现 CPU 到 OSD 的结果语义 shadow；先显示文本/状态/bbox，再接真实视觉。
- 建立 APB slave 的 bus-level testbench，用合成快照验证 CPU read/ack、config commit 和 OSD staging。

L3 的最小可接受演示是“合成快照 -> APB -> CPU 任务状态机 -> OSD 语义 -> arm-controller mock”。它不使用真实机械臂，且不能称为板上全闭环，除非完成 RISC-V 构建、PNR、bitstream、烧录与硬件读回。

## 4. 构建隔离和防误用

| 模式 | 输入 | 允许输出 | 明确禁止 |
|---|---|---|---|
| `host_replay` | fixture 快照 | 测试报告、日志、mock 控制器事件 | MMIO、UART、真实机械臂 |
| `rtl_sim` | 合成 RGB 2ppc | 波形、特征快照断言 | MIPI/DDR 正确性的结论 |
| `fpga_synth_verify` | 合成源专用顶层 | Debugger/HDMI/寄存器链路观察 | 比赛 bitstream、机械臂执行 |
| `competition_live` | 仅 CSI/Debayer 实时流 | CPU/OSD/板上 UART 正式闭环 | 自动回退到合成源 |

要求：

- 编译产物名称必须带模式，例如 `feature_replay_host`、`synthetic_preprocess_verify`；不得覆盖 `mem_test.bit` 或模糊命名为“正式”。
- fixture 文件必须写明“合成、非实拍、非标定数据”。
- `ARM_ENABLE` 默认硬锁为 0，只有独立安全确认后的受控硬件阶段可解除；L1/L2/L3 均只注入 mock transport。
- OSD 固定显示来源标记：`SIM`、`REPLAY` 或 `LIVE`，录像/串口日志同样记录。
- 不修改 `top.v`、`mem_test.xml`、`.peri.xml`、`constrain.sdc`、MIPI/IP 设置，直到相应阶段另附 Review Packet。

## 5. 并行工作包和依赖

| 工作包 | 内容 | 输入依赖 | 不依赖摄像头 | 验收 |
|---|---|---|---|---|
| WP-1 | 四任务契约和 matcher 重构 | 官方细则、现有 classifier IDs | 是 | 四任务规则单测 |
| WP-2 | 逐轮事务状态机与理由码 | WP-1、arm controller API | 是 | 20 轮 host 回放 |
| WP-3 | feature replay fixture/runner | 现有 `feature_snapshot_t` | 是 | 可复现 fixture 结果 |
| WP-4 | 主循环到 mock arm 集成 | WP-2/WP-3 | 是 | GRAB/SKIP/FAULT 唯一响应 |
| WP-5 | 合成像素源与 RTL 仿真 | 现有预处理接口 | 是 | 统计快照 PASS/波形 |
| WP-6 | APB/CDC/OSD | 生成 SoC 产物 | 是，但依赖 SoC | bus-level/RTL 测试 |
| WP-7 | 摄像头恢复 | 板级测量与 LED 证据 | 否 | `rx_out_de1` 与真实帧 |

推荐顺序：先 WP-1、WP-2、WP-3；随后 WP-4 与 WP-5；SoC 生成物到位后执行 WP-6；WP-7 始终独立推进。这样不必等待摄像头，但也不会因缺少 SoC 而虚假声称接口已实现。

## 6. 分阶段验收结论模板

每次测试报告必须使用如下表述之一：

- `HOST VERIFIED`：仅 host fixture / mock 已通过。
- `RTL SIM VERIFIED`：仅合成像素流仿真已通过。
- `FPGA VERIFY-BUILD VERIFIED`：专用验证 bitstream 的合成源链路已观察；非正式比赛路径。
- `LIVE CAMERA VERIFIED`：仅在稳定 CSI -> Debayer -> 特征证据、PNR/bitstream 和板级日志齐全时使用。
- `NOT VERIFIED`：任一缺少的层级，不以相邻层结果替代。

## 7. 风险与退出条件

- 合成图形过于理想会掩盖真实光照、白/黑反射、背景和遮挡问题。因此 fixture 必须含边界、噪声、UNKNOWN 和冲突序列，但不替代真实标定集。
- 摄像头恢复后可能暴露 `de/valid`、字节序、尺度标定和双摄同步差异；L2/L3 不能关闭这些风险。
- PNR/I/O 未约束问题仍会阻塞任何新 FPGA 板上 bitstream，合成源不是绕过时序签核的方法。
- 正式机械臂动作必须独立通过 UART、电平、接线、急停和姿态安全门；模拟视觉输入不提供动作授权。

本路线的退出条件是稳定真实 CSI 数据恢复并完成实时特征与回放基线对比。届时只替换 `feature source`，保留 task FSM、理由码、APB/CDC 契约和控制器接口；对比失败则回到契约层定位，不让摄像头调试改写已通过的下游业务规则。

## 8. 审核请求

请审核以下三项后再实施：

1. 是否同意以 `feature_snapshot_t` 作为摄像头恢复前唯一的下游输入契约。
2. 是否同意先实施 WP-1 至 WP-5，而 APB/CDC/OSD 保持受生成 SoC 产物门禁约束。
3. 是否同意所有 L1/L2/L3 均强制 mock 机械臂和来源标记，不产生真实动作。
