# FPGA 视觉预处理模块执行与协作交接方案

> 状态：方案已获用户批准，RTL 尚未开始  
> 日期：2026-07-11  
> 适用工程：`final_project/`  
> 第一阶段目标：完成可脱离摄像头独立验证的双像素流 ROI 与统计特征模块  
> 当前阶段不修改：`top.v`、MIPI/I2C、`mem_test.xml`、`.peri.xml`、`constrain.sdc`

## 1. 目的

摄像头模块当前仍在排查，因此预处理开发不能依赖实时 MIPI 输入才能推进。本方案把预处理模块设计成一个标准、可仿真的像素流消费者：先用确定性测试图完成 RTL 和自动化验证，待摄像头链路稳定后，再以旁路监听方式接入现有 Debayer/白平衡输出。

本阶段希望交付以下能力：

- 对双像素 RGB888 视频流生成准确的像素坐标。
- 按可配置矩形提取 ROI。
- 计算 ROI 内 RGB/亮度统计。
- 生成可配置的红、蓝、黄像素掩码及面积。
- 生成不依赖特定颜色的通用前景掩码。
- 根据通用前景掩码计算 `fg_area`、bbox 和中心点。
- 在帧结束时产生稳定、完整、可交给 CPU 的特征快照。
- 通过不依赖摄像头、DDR 和厂商 MIPI IP 的自检 testbench 证明结果正确。

## 2. 系统边界

### 2.1 FPGA 负责

- 接收 Debayer 或白平衡后的流式 RGB 像素。
- ROI 坐标判断。
- 像素级轻量比较、差分和掩码生成。
- RGB/Y 求和、像素计数、颜色面积、前景面积。
- bbox、中心点等原始几何特征。
- 帧级特征快照和必要的错误状态。
- 后续阶段中的 OSD 物理叠加、APB 寄存器物理通道和 CDC。

### 2.2 板上 CPU 负责

- 颜色、形状和尺寸的最终分类。
- 阈值表和背景参考值管理。
- 多帧滤波、稳定性判断和双摄结果融合。
- 任务匹配、抓取条件判断和异常处理。
- myCobot 协议、动作序列和安全互锁。

### 2.3 明确不做

首版预处理 RTL 不实现：

- HSV 转换、浮点计算或复杂除法流水线。
- 最终颜色、形状、尺寸类别输出。
- 纯 RTL 目标匹配或机械臂动作状态机。
- 多物体连通域标号、目标跟踪或复杂形态学处理。
- CPU 逐像素扫描全帧 DDR。
- DDR ROI 图块传输和 PLIC 中断。
- PC 或外部 MCU 进入正式识别闭环。

上述边界来自 `AGENTS.md`「分赛区决赛主线」和 `分赛区决赛实施开发路线.md`。任何试图把分类或机械臂控制重新放回纯 RTL 的变更都必须停止并重新评审。

## 3. 当前工程事实

### 3.1 当前可用像素流

正式工程当前的 Debayer 模块为：

```text
final_project/fpga/rtl/debayer/debayer_top_2to1.v
```

其输出 `rgb_datax2_o[47:0]` 每个像素时钟携带两个相邻 RGB888 像素，现有实现的字节排列为：

```text
{B1, G1, R1, B0, G0, R0}
```

其中 pixel 0 为偶像素，pixel 1 为其右侧相邻奇像素。预处理适配器必须在一个位置集中处理该字节序，后续模块只使用明确命名的 `r0/g0/b0` 与 `r1/g1/b1`，不得在多个模块重复切片。

### 3.2 当前时钟域

预处理本体计划运行在现有像素/Framebuffer/Debayer 时钟域：

```text
i_sysclk_div2
```

首版模块内部不新增时钟域。APB CPU 域和 HDMI 域的 CDC 放到后续集成阶段单独设计、单独审查。

### 3.3 尚未闭合的事实

以下内容在顶层集成前必须确认，不能直接沿用旧草案：

- `stream_id` 与物理摄像头接口的对应关系。
- 物理接口与 RTL `ch0/ch1` 的对应关系。
- `ch0/ch1` 与 `wb0/wb1` 的对应关系。
- `wb0/wb1` 与 CPU Camera 0/Camera 1 寄存器块的对应关系。
- 哪一路是俯视主相机，哪一路是侧视相机。
- 预处理最终取 Debayer 原始 RGB，还是白平衡后的 RGB。

必须形成并评审如下映射表后，才允许接入 `top.v`：

| stream_id | 物理接口 | RTL 通道 | 像素流信号 | CPU 寄存器块 | 角色 |
|---|---|---|---|---|---|
| 待定 | 待定 | `ch0` 或 `ch1` | `wb0_*` 或 `wb1_*` | Cam0 或 Cam1 | 俯视或侧视 |
| 待定 | 待定 | `ch0` 或 `ch1` | `wb0_*` 或 `wb1_*` | Cam0 或 Cam1 | 俯视或侧视 |

旧视觉手册中出现过 `Camera 0 = wb1`、`Camera 1 = wb0` 的规划性命名，但当前 RTL 已经历后续双通道修改，不得把该旧命名直接当作当前事实。

## 4. 模块架构

```text
RGB888 2ppc stream
        |
        v
vision_stream_adapter_2ppc
        |  r0/g0/b0, r1/g1/b1, x0/x1/y, valid0/valid1
        v
roi_window_2ppc
        |  roi_hit0/roi_hit1
        v
pixel_mask_2ppc
        |  red/blue/yellow/foreground mask x 2
        v
feature_accumulator_2ppc
        |  running sums/areas/bbox
        v
feature_snapshot
        |  stable frame snapshot + valid/frame_id/status
        v
CPU register/CDC bridge (later phase)
```

### 4.1 `vision_stream_adapter_2ppc.v`

职责：

- 拆分 `{B1,G1,R1,B0,G0,R0}`。
- 生成每拍两个像素的坐标 `x`、`x+1` 和 `y`。
- 根据 `vs/hs/de/valid` 识别帧、行和有效像素。
- 处理帧首、行首、奇数有效宽度和复位边界。
- 向后级输出无歧义的两像素接口。

该模块是唯一允许了解上游打包字节序的模块。

### 4.2 `roi_window_2ppc.v`

ROI 使用半开区间：

```text
[x0, x1) x [y0, y1)
```

职责：

- 分别判断 pixel 0 和 pixel 1 是否位于 ROI。
- 处理一拍中只有一个像素命中 ROI 的情况。
- 检测空 ROI、反向 ROI 和超出帧边界的配置。
- 配置只允许在帧边界切换，帧内不得部分更新。

半开区间可使宽高直接等于 `x1-x0`、`y1-y0`，减少边界加一歧义。CPU、OSD 和 testbench 必须使用相同语义。

### 4.3 `pixel_mask_2ppc.v`

职责：

- 对两个像素并行生成红、蓝、黄掩码。
- 依据 RGB 通道差、亮度上下限等轻量条件生成掩码。
- 依据可配置背景参考值和 RGB 绝对差生成通用前景掩码。
- 所有阈值从配置接口输入，不把现场参数硬编码在 RTL 中。

建议的通用前景定义为：像素位于 ROI，且 RGB 至少一个通道相对背景参考值的绝对差超过阈值，同时满足可配置亮度门限。具体阈值由 CPU 标定和管理。

通用前景掩码不能简单定义为 `red | blue | yellow`，否则白色和黑色物块无法得到可靠的 `fg_area` 和 bbox。

### 4.4 `feature_accumulator_2ppc.v`

职责：

- 同一拍内正确处理 0、1 或 2 个 ROI 像素。
- 累加 `sum_r/sum_g/sum_b/sum_y`。
- 累加 `roi_pixel_count`。
- 累加 `red_area/blue_area/yellow_area/fg_area`。
- 根据通用前景掩码更新 bbox。
- 在帧结束后计算 bbox 中心点。
- 检测计数或累加溢出并输出状态位。

首版不在 FPGA 内计算 RGB/Y 均值。CPU 使用 `sum/count` 计算，避免引入不必要的除法器。

建议数据宽度：

| 数据 | 建议宽度 | 说明 |
|---|---:|---|
| 坐标 | 16 bit | 覆盖当前分辨率并兼容后续调整 |
| 帧号 | 16 bit | 允许自然回绕 |
| 面积/像素数 | 32 bit | 覆盖 2568 x 1448 |
| RGB/Y 总和 | 32 bit | 2568 x 1448 x 255 小于 2^32 |

### 4.5 `feature_snapshot.v`

职责：

- 在一帧统计完整后一次性锁存全部特征。
- 输出稳定 `frame_id`、特征值和状态。
- `snapshot_valid` 未被匹配确认前，不允许读侧看到撕裂数据。
- 明确新帧到达但旧快照未确认时的行为和丢帧计数。

正式 CPU 集成继续使用已有规划的 `frame_id + valid/ack` 协议。首版 testbench 可以直接驱动 ack，以验证保持和释放行为。

### 4.6 `vision_preprocess_channel.v`

该模块封装以上模块，作为单路预处理顶层。双摄不维护两套不同源代码，而是用同一模块实例化两次。

模块参数只允许描述结构性差异，例如最大分辨率或复位默认 ROI；颜色分类阈值不得固化为 Verilog 参数。

## 5. 像素流接口草案

以下是模块级接口语义，具体端口名可在编码前做一次小范围整理。

### 5.1 输入视频接口

| 信号 | 方向 | 宽度 | 时钟域 | 语义 |
|---|---|---:|---|---|
| `pixel_clk` | input | 1 | pixel | 像素时钟 |
| `reset_n` | input | 1 | pixel | 低有效复位 |
| `in_vs` | input | 1 | pixel | 帧同步，极性沿用上游当前定义 |
| `in_hs` | input | 1 | pixel | 行同步，极性沿用上游当前定义 |
| `in_de` | input | 1 | pixel | 有效像素区 |
| `in_valid` | input | 1 | pixel | 当前双像素数据有效 |
| `in_rgb_2ppc` | input | 48 | pixel | `{B1,G1,R1,B0,G0,R0}` |

`in_de` 和 `in_valid` 的实际关系必须通过当前 Debayer 波形或最小 testbench 确认。禁止在没有证据时假设二者恒等。

### 5.2 配置接口

纯像素域首版先使用直接配置端口：

| 配置组 | 内容 |
|---|---|
| ROI | `roi_x0/roi_y0/roi_x1/roi_y1` |
| 背景参考 | `bg_r/bg_g/bg_b` |
| 前景阈值 | RGB 差分阈值、亮度上下限 |
| 颜色阈值 | 红/蓝/黄通道差及亮度条件 |
| 控制 | enable、snapshot ack、debug enable |

这些输入在 testbench 中直接驱动。接 APB 后，配置必须经过 staging -> commit -> VSYNC active shadow，不允许 APB 多位总线直接跨入像素域。

### 5.3 特征快照输出

| 信号 | 宽度 | 语义 |
|---|---:|---|
| `snapshot_valid` | 1 | 当前快照稳定有效 |
| `frame_id` | 16 | 特征对应帧号 |
| `roi_pixel_count` | 32 | ROI 内参与统计的像素数 |
| `sum_r/g/b/y` | 各 32 | ROI 内通道与亮度总和 |
| `red/blue/yellow_area` | 各 32 | ROI 内对应掩码像素数 |
| `fg_area` | 32 | ROI 内通用前景像素数 |
| `bbox_min` | 32 | `{y_min[15:0],x_min[15:0]}` |
| `bbox_max` | 32 | `{y_max[15:0],x_max[15:0]}` |
| `center` | 32 | `{y_center[15:0],x_center[15:0]}` |
| `status` | 待定 | 空前景、非法 ROI、溢出、快照丢帧等 |

空前景时建议统一定义：

```text
fg_area   = 0
bbox_min  = 0
bbox_max  = 0
center    = 0
status.empty_foreground = 1
```

CPU 不得在 `empty_foreground=1` 时使用 bbox 或 center。

## 6. CPU/APB 交接约束

CPU/APB 集成不属于第一阶段，但接口必须提前保持一致。

### 6.1 执行前阻塞门

进入 APB/SoC RTL 前必须生成并审查：

```text
generated_soc_summary_YYYY-MM-DD.md
```

至少确认：

- 当前 Efinity 生成 `soc.h` 中真实 APB user window 基地址。
- 选用的 `io_apbSlave_x_*` 当前确实空闲。
- `PADDR` 宽度足够覆盖双路寄存器窗口。
- APB 时钟、复位、`PREADY` 和 `PSLVERROR` 语义。
- UART、CLINT、PLIC、AXI 和 DDR 地址没有冲突。

在该摘要闭合前：

- 不修改 SoC IP 设置。
- 不修改 `.peri.xml`。
- 不把草案中的 APB 地址当成真实地址。
- 不解除 CPU 侧 `REG_MAGIC` 运行期探测。

### 6.2 配置 CDC

APB 域必须维护 staging 配置。CPU 写完一整组配置后写 `CFG_COMMIT(config_seq)`；像素域只在帧边界把完整 pending 配置锁存为 active shadow。

必须保留以下语义：

- 未 commit 的配置不生效。
- 同一帧内配置不变化。
- 连续相同 `config_seq` 不算新提交。
- 多次 pending 采用最后一次完整提交胜出。
- CPU 读 `CFG_STATUS.active_seq` 判断是否生效。

### 6.3 特征 CDC

- 像素域先锁存完整快照，再通知 APB 域。
- CPU 读取前后检查同一个 `frame_id`。
- CPU 写 ack 时必须携带当前 `frame_id`。
- 不匹配的 ack 不得清除 `snapshot_valid`。
- CDC 报告中的多位总线、握手和复位问题不得直接忽略。

### 6.4 寄存器文档一致性

目前存在两份不同成熟度的接口材料：

- `final_project/integration/register_map.md`
- `final_project/docs/architecture/vision_register_handbook_draft_2026-07-03.md`

CPU 代码还预留了：

```text
LIVE_FG_AREA0 = 0x0B0
LIVE_FG_AREA1 = 0x1B0
```

接 APB 前必须合并为一份受控的正式寄存器契约，并同步 `board_io.h`。在正式契约完成前，不得分别修改 FPGA 和 CPU 偏移量后口头约定。

## 7. OSD 交接约束

OSD 不是第一阶段的阻塞项，计划在特征与 CPU 接口稳定后接入。

- FPGA 实时 bbox 在像素域产生，可直接作为绿色调试框来源。
- CPU 回写的稳定 bbox 必须经过 VSYNC shadow 后作为另一颜色框来源。
- 双像素输出必须分别比较 `x` 和 `x+1`，不能一拍只画一个像素。
- OSD 插入 `top.v` 前需要检查 fanout、延迟和 HDMI 显示链路时序。
- OSD 只显示结果，不参与识别或动作判定。

OSD 顶层接入属于 Codex 审查门，需另交集成 Review Packet。

## 8. 实施阶段与交付物

### 阶段 A：接口冻结

交付物：

- 通道映射表。
- 2ppc 像素字节序和 `de/valid/vs/hs` 语义说明。
- ROI 半开区间定义。
- 通用前景掩码定义。
- 特征快照字段和空前景语义。

通过条件：FPGA、CPU 和顶层协作者对上述内容无冲突。

### 阶段 B：独立 RTL

计划新增：

```text
final_project/fpga/rtl/roi_crop/vision_stream_adapter_2ppc.v
final_project/fpga/rtl/roi_crop/roi_window_2ppc.v
final_project/fpga/rtl/feature_extract/pixel_mask_2ppc.v
final_project/fpga/rtl/feature_extract/feature_accumulator_2ppc.v
final_project/fpga/rtl/feature_extract/feature_snapshot.v
final_project/fpga/rtl/feature_extract/vision_preprocess_channel.v
```

本阶段不修改现有顶层和工程 XML。

### 阶段 C：自动化 testbench

建议新增独立仿真目录，并覆盖：

- 空背景帧。
- ROI 内固定色块。
- 目标贴 ROI 四条边界。
- 目标跨 even/odd 像素。
- 一拍仅一个像素落入 ROI。
- 奇数 ROI 宽度。
- 白、黑、红、蓝、黄目标。
- 空 ROI、反向 ROI 和越界 ROI。
- 帧中修改配置但下一帧才生效。
- 快照未 ack 时继续到来新帧。
- 复位发生在帧内。
- 计数器和状态位边界。

testbench 必须自行计算期望的 sum、area、bbox 和 center，自动输出 PASS/FAIL，不能只靠人工看波形。

### 阶段 D：单通道旁路集成

- 先选择映射已确认的一条通道。
- 从 Debayer/白平衡输出分叉给预处理。
- 不改变 HDMI 原数据路径。
- 先只暴露内部 probe 或临时稳定快照，不立即接机械臂逻辑。
- 通过单通道验证后再对称实例化第二通道。

### 阶段 E：APB/CPU 集成

- 闭合 `generated_soc_summary`。
- 实现正式寄存器文件、配置 commit 和快照握手。
- 同步 CPU `board_io.h`、寄存器手册和特征结构体。
- 将 `FG_AREA_AVAILABLE` 从 0 改为 1 前，必须实测寄存器有效。

### 阶段 F：OSD、双路与工程签核

- 接实时/CPU bbox OSD。
- 对称接入第二通道。
- 更新 `mem_test.xml` 前提交文件清单和依赖检查。
- 运行 Efinity map、PNR、bitstream。
- 记录 Setup/Hold Slack、CDC 和所有新增 warning。

## 9. 验收标准

### 9.1 模块级

- testbench 所有用例自动 PASS。
- even/odd 两像素无漏计、重计或横向错位。
- RTL 结果与软件参考模型逐项一致。
- 帧内 ROI 和阈值保持不变。
- 空前景和非法 ROI 有明确状态，不产生伪 bbox。
- 不引入不必要的乘除法长组合路径。
- 复位后所有 valid、计数和状态处于确定值。

### 9.2 集成级

- 预处理旁路监听不改变 HDMI 原链路行为。
- 双通道使用同一模块，配置和状态独立。
- CPU 读取快照无撕裂，ack 不会误清其他帧。
- APB 配置最多在下一帧边界完整生效。
- Efinity 全流程通过。
- Setup/Hold Slack 有记录且满足要求。
- CDC warning 全部有证据闭环；任何“可忽略”判定必须经过 Codex Gate。

## 10. 协作与工作树约束

当前工作树已经存在摄像头 bring-up 相关未提交修改，包括：

```text
final_project/fpga/rtl/top/top.v
final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v
final_project/fpga/rtl/mipi_csi/cam_i2c_ctrl/i2c/*.v
final_project/docs/debug_sessions/hdmi_stripe_debug_20260707.md
```

预处理开发期间必须遵守：

- 不回退、覆盖或顺手整理这些现有修改。
- 阶段 B/C 只新增预处理模块和仿真文件。
- 顶层集成前先确认摄像头调试分支已形成可识别 checkpoint。
- 若必须同时修改 `top.v`，先保存 Review Packet，列出双方修改区域和信号所有权。
- 不直接改赛方 demo、生成 IP、`outflow/` 或补丁目录。
- 若实际构建仍使用 `D:\final_project`，同步前必须先检查 C/D 两棵树差异，不允许盲目覆盖。

## 11. 队友交接清单

### 摄像头/顶层队友需要提供

- 最终稳定通道的 `vs/hs/de/valid/data` 信号名。
- 48-bit 数据的实际字节序确认。
- 有效分辨率、总时序和像素时钟。
- `de` 与 `valid` 的关系。
- `ch0/ch1`、`wb0/wb1` 与物理接口映射。
- 哪个阶段允许在 `top.v` 分叉像素流。

### CPU 队友需要确认

- 正式需要的最小特征字段。
- `LIVE_FG_AREA` 的语义接受为“ROI 内通用前景像素数”。
- bbox 采用半开还是闭区间；本方案推荐半开 ROI、bbox 输出实际命中像素的闭区间 min/max。
- 背景参考值和阈值由 CPU 如何标定、保存和 commit。
- `frame_id + valid/ack` 处理和掉帧策略。
- 正式寄存器偏移合并方案。

### FPGA 预处理实现者需要交付

- 模块端口说明和时序图。
- testbench、测试向量和 PASS 日志。
- 资源与关键路径摘要。
- 未验证项和已知限制。
- 顶层集成所需的最小接线清单。
- 对 `mem_test.xml`、SDC、SoC/IP 是否需要变更的明确声明。

## 12. Review Packet 要求

以下节点必须形成独立 Review Packet：

1. 首次修改 `top.v` 接入预处理。
2. 首次修改 `mem_test.xml` 加入新源文件。
3. APB/QCRV32/CDC 接入。
4. 修改 `.peri.xml`、SoC IP 或 `constrain.sdc`。
5. OSD 接入 HDMI 主链路。
6. 将单路扩展为双路并改变资源或时序结构。

Review Packet 至少包含：

- 目标和当前结论。
- 修改文件列表及关键 diff。
- 模块、信号、时钟域、复位和双通道映射。
- 已运行命令、日志路径和结果。
- Setup/Hold、CDC 和 warning 摘要。
- 未验证项、风险假设和请求审查的问题。

## 13. 参考文件优先级

实施时按以下顺序判断事实：

1. 用户当前明确指令。
2. `AGENTS.md`「分赛区决赛主线」和安全红线。
3. `CURRENT_STATE.md`。
4. 通过健康检查的最新 handoff。
5. 真实 RTL、工程 XML、构建日志和上板现象。
6. `final_project/integration/video_pipeline.md`。
7. `final_project/integration/fpga_cpu_interface.md`。
8. `final_project/integration/register_map.md`。
9. `final_project/docs/architecture/vision_module_implementation_plan.md`。
10. `final_project/docs/architecture/vision_register_handbook_draft_2026-07-03.md`。

规划文件和手册草案不能覆盖真实源码、当前工程生成物或上板证据。

## 14. 第一项立即动作

在开始 RTL 前，先完成阶段 A 的接口冻结，尤其是：

1. 用一个最小仿真或已知像素向量确认 48-bit 字节序。
2. 明确 `de` 与 `valid` 的实际语义。
3. 固化 ROI 和 bbox 边界定义。
4. 与 CPU 队友确认通用前景掩码和 `LIVE_FG_AREA` 语义。
5. 建立并签认通道映射表。

上述内容确认后，直接进入独立 RTL 和自检 testbench，不等待摄像头恢复。
