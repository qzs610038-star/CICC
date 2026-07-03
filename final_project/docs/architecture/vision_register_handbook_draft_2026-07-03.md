# FPGA 与 CPU 视觉交互寄存器手册（草案）

**状态：架构方向通过（Codex 三次复核 2026-07-03）；RTL/SoC 执行前 Gate 保留，待 B1 `generated_soc_summary` 闭合。**
**计划来源：[vision_module_implementation_plan.md](vision_module_implementation_plan.md) v4.3**

> 本手册是 FPGA 视觉前端与板上 QCRV32 CPU 之间第一版软硬件接口契约。基地址 `IO_APB_SLAVE_x_BASE` 以当前 Efinity 生成 `soc.h` 为准，本手册只定义偏移量与语义。
> 在补齐 B1 前置（`generated_soc_summary_YYYY-MM-DD.md`）前，本手册草案的地址宏允许保持占位，但不得据此改 SoC/IP 配置或落 RTL。

---

## 0. 适用范围与硬性边界

- 第一版主通道：`wb1_*`（已核实 [top.v:1398-1401](../../fpga/rtl/top/top.v#L1398-L1401) DSI、[1468-1474](../../fpga/rtl/top/top.v#L1468-L1474) HDMI 均消费 `wb1_*`）；`wb0_*` 旁路预留，不进首版识别/显示。
- 插入点：feature_extract 与 OSD 均取 `wb1_*`，OSD 接在 `wb1_white_balance` 之后、DSI/HDMI 之前。**真正插 OSD 仍会改 `top.v` 连线，需另交小包审查 fanout/timing**；"零改顶层"仅对选 wb1 主通道成立。
- 首版不做：多物体分割、PLIC 中断、FPGA 尺寸分类。
- `CPU_HEARTBEAT` 看门狗超时仅重置视觉状态机，**禁止触发 myCobot 急停/动作**。

## 1. 时钟域与 CDC 契约

| 域 | 内容 |
|---|---|
| APB 时钟域 | APB3 Slave，QCRV32 SoC，频率待 `soc.h`（参考 `SYSTEM_CLINT_HZ`）；CPU 读写、`staging` 配置 |
| 像素时钟域 | `wb1_*` 视频流、feature_extract 统计、OSD 叠加；`active shadow` 配置 |

### 1.1 配置原子提交（staging → commit → active shadow）

- CPU 写所有 R/W `CFG_*` → APB 域 **staging**。
- CPU 写 `CFG_COMMIT[15:0] = config_seq` → 这组 staging 进入待生效状态。
- 像素域仅在 **VSYNC 边界** 整组原子切换 active shadow；中途不部分更新。
- `CFG_STATUS` 回报 `active_seq` / `pending_seq`，CPU 据此确认生效。

四个边界（v4.3 已落，必须写死）：

1. **`config_seq` 复用/回绕**：CPU 每次 commit 必须用与上一次不同的序号；16-bit 回绕允许，**不得连续两次相同**，否则 FPGA 视为无效 commit（不更新 pending）。
2. **pending 覆盖**：pending 未到 VSYNC 生效前 CPU 再次 commit，**最后一次 commit 胜出**，`pending_seq` 更新为最新值。
3. **复位默认**：复位后 `active_seq=0`、`pending_seq=0`；active shadow 使用安全默认（OSD 关闭、ROI=全画面、阈值保守值）。
4. **staging 读回**：CPU 读 R/W `CFG_*` → 读到 **staging**（最近写入，未必生效）；读 `CFG_STATUS.active_seq` 判断像素域是否已采用（`active_seq == 本次 commit_seq` 即已生效）。

### 1.2 帧结果握手（valid / ack + frame_id）

- FPGA 帧末整组锁存 `LIVE_*` + `frame_id` 到 APB 读侧快照，再置 `feature_valid`。
- `frame_id` 16-bit 单调递增，回绕可接受；天然由像素域产生，不走 Gray 编码。
- `feature_valid` 单 bit 两级同步器；**未被匹配 ack 前不覆盖快照**。
- CPU 读 `SYS_STATUS.frame_id`，写 `SYS_ACK[15:0] = 同一 frame_id` 应答。
- FPGA 仅在同步后 ack 且 `ack.frame_id == 当前快照 frame_id` 匹配时清 `valid`，然后才推进下一帧快照。
- CPU 掉帧（valid 仍置、新帧已备）时，FPGA 采用"**保持最新快照等 ack**"策略。

## 2. Register Map

*(偏移量定义；基地址取 `soc.h` 的 `IO_APB_SLAVE_x_BASE`)*

| Offset | R/W | 名称 | 描述 |
|---|---|---|---|
| `0x000` | R | `REG_MAGIC` | 固定 Magic `0x375A` + Version；CPU 上电探嗅，不匹配则挂起 |
| `0x004` | R/W | `SYS_CTRL` | [0] 视觉流水线使能; [1] OSD 使能 (写入 staging，需 CFG_COMMIT 生效) |
| `0x008` | R | `SYS_STATUS` | [0] `feature_valid`(FPGA置位); [31:16] `frame_id`(当前快照帧号) |
| `0x00C` | W | `SYS_ACK` | [15:0]= 要确认的 `frame_id`；FPGA 仅 ack.frame_id 匹配时清 valid |
| `0x010` | R/W | `CPU_HEARTBEAT` | 32-bit 递增；看门狗超时仅重置视觉状态机，不动 myCobot |
| `0x014` | R/W | `CFG_ROI_TL` | [31:16] ROI MIN_Y, [15:0] MIN_X (staging) |
| `0x018` | R/W | `CFG_ROI_BR` | [31:16] ROI MAX_Y, [15:0] MAX_X (staging) |
| `0x020` | R/W | `CFG_RED_TH_0` | 红色阈值下限 (staging) |
| `0x024` | R/W | `CFG_RED_TH_1` | 红色阈值上限 (staging) |
| `0x028` | R/W | `CFG_BLUE_TH_0` | 蓝色阈值下限 (staging) |
| `0x02C` | R/W | `CFG_BLUE_TH_1` | 蓝色阈值上限 (staging) |
| `0x030` | R/W | `CFG_YEL_TH_0` | 黄色阈值下限 (staging) |
| `0x034` | R/W | `CFG_YEL_TH_1` | 黄色阈值上限 (staging) |
| `0x03C` | R/W | `CFG_LUMA_TH` | [31:16] TH_LUMA_MAX, [15:0] TH_LUMA_MIN (R+G+B 全局门限, staging) |
| `0x040` | R/W | `CPU_OSD_CTRL` | CPU 稳态回写控制字 (最终判决颜色/形状类别, staging) |
| `0x044` | R/W | `CPU_OSD_BBOX_MIN` | CPU 回写稳态 bbox 最小坐标 (staging, 红框用) |
| `0x048` | R/W | `CPU_OSD_BBOX_MAX` | CPU 回写稳态 bbox 最大坐标 (staging, 红框用) |
| `0x04C` | W | `CFG_COMMIT` | [15:0]=`config_seq`；触发整组 staging→active 在下个 VSYNC 切换 |
| `0x050` | R | `CFG_STATUS` | [31:16] `active_seq`(已生效), [15:0] `pending_seq`(待VSYNC) |
| `0x080` | R | `LIVE_RED_AREA` | 当前帧红色 mask 像素数 (快照, 像素域写) |
| `0x084` | R | `LIVE_BLUE_AREA` | 当前帧蓝色 mask 像素数 |
| `0x088` | R | `LIVE_YEL_AREA` | 当前帧黄色 mask 像素数 (WHITE/BLACK 预留 `0x08C/0x090`, 首版不实现) |
| `0x0A0` | R | `LIVE_BBOX_MIN` | FPGA 实时最大物体外接矩形最小坐标: [31:16] Y_MIN, [15:0] X_MIN |
| `0x0A4` | R | `LIVE_BBOX_MAX` | FPGA 实时最大物体外接矩形最大坐标: [31:16] Y_MAX, [15:0] X_MAX |
| `0x0A8` | R | `LIVE_CENTER` | FPGA 实时目标中心坐标: [31:16] Y_CEN, [15:0] X_CEN |

**命名约定**：`LIVE_*` = FPGA 像素域实时统计（只读，画绿框，无 CDC）；`CPU_OSD_*` = CPU 多帧滤波后稳态回写（写 staging→commit→shadow，画红框）。

## 3. 复位默认值表（解除 RTL Gate 前需固化）

| 寄存器 | 复位默认 |
|---|---|
| `SYS_CTRL` | `0x0`（视觉流水线关、OSD 关） |
| `SYS_STATUS.feature_valid` | `0`；`frame_id=0` |
| `active_seq` / `pending_seq` | `0` / `0` |
| `CFG_ROI_TL` / `CFG_ROI_BR` | 全画面（MIN=0，MAX=画面尺寸） |
| 颜色阈值 | 保守值（具体值解除 Gate 前按现场标定填入并固化） |
| `CFG_LUMA_TH` | 宽门限（不误杀） |
| `CPU_OSD_*` | `0`（不显示红框/控制字） |

## 4. CPU 操作流程（参考实现）

```
上电:
  1. 读 REG_MAGIC == 0x375A? 否则挂起打印
  2. 编译期: #ifndef IO_APB_SLAVE_x_BASE → #error

配置一组阈值/ROI:
  3. 写 CFG_ROI_TL / CFG_ROI_BR / CFG_*_TH_0/1 / CFG_LUMA_TH  (→ staging)
  4. config_seq++ (与上次不同)
  5. 写 CFG_COMMIT = config_seq
  6. 轮询 CFG_STATUS 直到 active_seq == config_seq  (最多等下一帧 VSYNC)

每帧取结果:
  7. 读 SYS_STATUS: feature_valid==1 且 frame_id 变化
  8. 读 LIVE_*_AREA / LIVE_BBOX_* / LIVE_CENTER  (同 frame_id 快照)
  9. 多帧滤波 + 分类决策
 10. 写 SYS_ACK = 该 frame_id  (匹配才清 valid, FPGA 推进下一帧)
 11. 回写 CPU_OSD_BBOX_* / CPU_OSD_CTRL (经 CFG_COMMIT 生效) 画红框/结果

心跳:
 12. 周期写 CPU_HEARTBEAT 递增值; 超时仅重置视觉状态机
```

## 5. 解除 RTL/SoC 执行前 Gate 的最终前置

在补齐 `generated_soc_summary_YYYY-MM-DD.md` 且满足以下四项后，才最终解除 Gate 并进入 RTL：

1. `soc.h` 摘要：UART0/UART1、CLINT、PLIC、APB user window、AXI user window、DDR base/size、`SYSTEM_CLINT_HZ` 或等价时钟。
2. 选定的 `io_apbSlave_x_*` 端口空闲，记录 PADDR 宽度、PSEL/PREADY/PSLVERROR 语义。
3. CPU BSP 占位地址被 `soc.h` 真源替代，或编译期强制缺失即失败。
4. OSD 真插 `top.v` 前另交小包审查 fanout/timing。

## 6. 仍待决/预留

- 尺寸分类阈值：首版由 CPU 查表（`param_table.c`），不在本手册寄存器；若后续要 FPGA 参与再补。
- WHITE/BLACK 颜色：预留 `0x08C/0x090`，首版不实现。
- 分组 commit：`CFG_COMMIT` 位域 [ROI]/[THRESH]/[OSD]，首版不实现。
- PLIC 中断：首版轮询，后续再评估。

---

**修订记录**
- 2026-07-03 (草案 v1)：基于 plan v4.3 起草，含 Codex 三次复核确认的 B4 契约基线 + 4 个边界。状态待 B1 `generated_soc_summary` 闭合后转正式。