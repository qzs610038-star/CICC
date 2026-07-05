# FPGA 与 CPU 视觉交互寄存器手册（草案 v2）

**状态：架构方向通过（Codex 三次复核 2026-07-03）；双路扩展 2026-07-05；RTL/SoC 执行前 Gate 保留，待 B1 `generated_soc_summary` 闭合。**
**计划来源：[vision_module_implementation_plan.md](vision_module_implementation_plan.md) v4.3 → 双路扩展**

> 本手册是 FPGA 视觉前端与板上 QCRV32 CPU 之间的软硬件接口契约。基地址 `IO_APB_SLAVE_x_BASE` 以当前 Efinity 生成 `soc.h` 为准，本手册只定义偏移量与语义。
> 在补齐 B1 前置（`generated_soc_summary_YYYY-MM-DD.md`）前，本手册草案的地址宏允许保持占位，但不得据此改 SoC/IP 配置或落 RTL。

---

## 0. 适用范围与硬性边界

### 0.1 双路摄像头

| 摄像头 | 链路 | 角色 | 寄存器块 | 特征输出 |
|--------|------|------|----------|----------|
| **Camera 0**（俯视） | `wb1_*`（原主显示链路，已接 DSI/HDMI） | 形状 + 颜色 | `0x000-0x0FF` | `LIVE_*_AREA`, `LIVE_BBOX_*`, `LIVE_CENTER` |
| **Camera 1**（侧面） | `wb0_*`（需 FPGA 队友改接；原旁路，首版不接显示） | 形状 + 颜色 + **尺寸** | `0x100-0x1FF` | 同 Cam0 + `LIVE_HEIGHT_PX`（侧面高度像素数） |

- HDMI 显示通过 FPGA 按键在 Cam0 和 Cam1 之间切换；OSD 跟随当前选中摄像头。
- `wb0_*` 需 FPGA 队友从旁路改为接入 feature_extract；改动量需评估。
- 首版不做：多物体分割、PLIC 中断、FPGA 尺寸分类（尺寸仍由 CPU 查表判定）。
- `CPU_HEARTBEAT` 看门狗超时仅重置视觉状态机，**禁止触发 myCobot 急停/动作**。

### 0.2 寄存器布局总览

```
0x000 ┌──────────────────────┐
      │  全局寄存器          │  MAGIC, SYS_CTRL, HEARTBEAT,
      │  (共用，不重复)      │  ARM_STATE, ERROR_CODE
      ├──────────────────────┤
      │  Camera 0 寄存器     │  STATUS0/ACK0, CFG_*0, OSD_*0,
0x0FF │  (俯视: 形状+颜色)   │  RESULT_*0, MATCH_ACTION0,
      ├──────────────────────┤  LIVE_*0
0x100 │  Camera 1 寄存器     │  STATUS1/ACK1, CFG_*1, OSD_*1,
      │  (侧面: 形状+颜色    │  RESULT_*1, MATCH_ACTION1,
0x1FF │          +尺寸)      │  LIVE_*1, LIVE_HEIGHT_PX
      └──────────────────────┘
```

---

## 1. 时钟域与 CDC 契约

| 域 | Cam0 | Cam1 | 内容 |
|----|------|------|------|
| APB 时钟域 | — | — | APB3 Slave，QCRV32 SoC，频率待 `soc.h`；CPU 读写、`staging` 配置 |
| 像素时钟域 | `wb1_*` 像素时钟 | `wb0_*` 像素时钟 | feature_extract 统计、OSD 叠加；`active shadow` 配置 |

> **注意**：两路摄像头像素时钟可能独立（各自的 PLL/MIPI 通道）。CDC 协议对每路独立生效。

### 1.1 配置原子提交（staging → commit → active shadow）

**每路独立**。Cam0 和 Cam1 各有自己的 `CFG_COMMIT` 和 `CFG_STATUS`，各自的 staging/active shadow 在各自的 VSYNC 边界切换，互不阻塞。

- CPU 写 CamN 所有 R/W `CFG_N_*` → APB 域 **staging (per camera)**。
- CPU 写 `CFG_COMMIT_N[15:0] = config_seq` → 该路 staging 进入待生效。
- 该路像素域仅在**自己的 VSYNC 边界**整组原子切换 active shadow。
- `CFG_STATUS_N` 回报 `active_seq` / `pending_seq`，CPU 据此确认该路生效。

四个边界（每路独立，必须写死）：

1. **`config_seq` 复用/回绕**：CPU 每次 commit 必须用与上一次不同的序号；16-bit 回绕允许，**不得连续两次相同**，否则 FPGA 视为无效 commit（不更新 pending）。
2. **pending 覆盖**：pending 未到 VSYNC 生效前 CPU 再次 commit，**最后一次 commit 胜出**，`pending_seq` 更新为最新值。
3. **复位默认**：复位后 `active_seq=0`、`pending_seq=0`；active shadow 使用安全默认。
4. **staging 读回**：CPU 读 R/W `CFG_N_*` → 读到 **staging**；读 `CFG_STATUS_N.active_seq` 判断是否生效。

### 1.2 帧结果握手（valid / ack + frame_id）— 独立握手

**每路独立的 valid/ack/frame_id**。两路帧率可能不同（各自 MIPI 时序），互不阻塞。

> **CPU 编译开关 `HANDSHAKE_MODE`**：预留 FPGA 后续可能要求合并握手。代码中通过此宏支持两种模式：
> - `HANDSHAKE_INDEPENDENT`（默认）：每路独立 valid/ack/frame_id。
> - `HANDSHAKE_MERGED`（备选）：全局 STATUS 同时汇报两路 valid，CPU 一次 ack 确认两路。

- FPGA 帧末整组锁存 `LIVE_N_*` + `frame_id` 到 APB 读侧快照，再置 `feature_valid`。
- `feature_valid` 单 bit 两级同步器；**未被匹配 ack 前不覆盖快照**。
- CPU 读 `SYS_STATUS_N.frame_id`，写 `SYS_ACK_N[15:0] = 同一 frame_id` 应答。
- FPGA 仅在同步后 ack 且 `ack.frame_id == 当前快照 frame_id` 匹配时清 `valid`。
- CPU 掉帧时 FPGA 采用"**保持最新快照等 ack**"策略。

---

## 2. Register Map

*(偏移量定义；基地址取 `soc.h` 的 `IO_APB_SLAVE_x_BASE`)*

### 2.1 全局寄存器

| Offset | R/W | 名称 | 描述 |
|--------|-----|------|------|
| `0x000` | R | `REG_MAGIC` | 固定 Magic `0x375A` + Version (高 16 位)；CPU 上电探嗅，不匹配则挂起 |
| `0x004` | R/W | `SYS_CTRL` | [0] Cam0 视觉使能; [1] Cam0 OSD 使能; [2] Cam1 视觉使能; [3] Cam1 OSD 使能; [4] HDMI 选择 (0=Cam0, 1=Cam1, 由 FPGA 按键控制，CPU 只读) |
| `0x010` | R/W | `CPU_HEARTBEAT` | 32-bit 递增计数；看门狗超时仅重置视觉状态机，不动 myCobot |
| `0x064` | R/W | `CPU_ARM_STATE` | myCobot 状态机当前阶段: 0=idle,1=approach,2=grasp,3=lift,4=place,5=retreat,6=error (staging, Cam0 commit 触发) |
| `0x068` | R/W | `CPU_ERROR_CODE` | [15:0] 错误码: 0=无错误,1=通信超时,2=分类置信度低,3=机械臂异常,4=参数越界 (staging, Cam0 commit 触发) |

> **设计说明**：`CPU_ARM_STATE` 和 `CPU_ERROR_CODE` 是全局的（只有一个机械臂），跟随 Cam0 的 commit 通道生效。Cam0 的 commit 同时触发全局寄存器 + Cam0 配置 + Cam0 结果生效。

### 2.2 Camera 0 寄存器（俯视：形状 + 颜色）

| Offset | R/W | 名称 | 描述 |
|--------|-----|------|------|
| `0x008` | R | `SYS_STATUS0` | [0] `feature_valid`; [31:16] `frame_id` |
| `0x00C` | W | `SYS_ACK0` | [15:0] = 要确认的 `frame_id` |
| `0x014` | R/W | `CFG_CAM0_ROI_TL` | [31:16] ROI MIN_Y, [15:0] MIN_X (staging) |
| `0x018` | R/W | `CFG_CAM0_ROI_BR` | [31:16] ROI MAX_Y, [15:0] MAX_X (staging) |
| `0x020` | R/W | `CFG_CAM0_RED_TH_0` | 红色阈值下限 (staging) |
| `0x024` | R/W | `CFG_CAM0_RED_TH_1` | 红色阈值上限 (staging) |
| `0x028` | R/W | `CFG_CAM0_BLUE_TH_0` | 蓝色阈值下限 (staging) |
| `0x02C` | R/W | `CFG_CAM0_BLUE_TH_1` | 蓝色阈值上限 (staging) |
| `0x030` | R/W | `CFG_CAM0_YEL_TH_0` | 黄色阈值下限 (staging) |
| `0x034` | R/W | `CFG_CAM0_YEL_TH_1` | 黄色阈值上限 (staging) |
| `0x03C` | R/W | `CFG_CAM0_LUMA_TH` | [31:16] TH_LUMA_MAX, [15:0] TH_LUMA_MIN (staging) |
| `0x040` | R/W | `CPU_OSD_CTRL0` | Cam0 OSD 控制字 (staging) |
| `0x044` | R/W | `CPU_OSD_BBOX_MIN0` | Cam0 稳态 bbox 最小坐标 (staging, 红框) |
| `0x048` | R/W | `CPU_OSD_BBOX_MAX0` | Cam0 稳态 bbox 最大坐标 (staging, 红框) |
| `0x04C` | W | `CFG_COMMIT0` | [15:0]=`config_seq`；触发 Cam0 整组 staging→active + 全局 staging→active |
| `0x050` | R | `CFG_STATUS0` | [31:16] `active_seq`, [15:0] `pending_seq` |
| `0x054` | R/W | `CPU_RESULT_COLOR0` | Cam0 颜色结果 (staging) |
| `0x058` | R/W | `CPU_RESULT_SHAPE0` | Cam0 形状结果 (staging) |
| `0x05C` | R/W | `CPU_RESULT_SIZE0` | Cam0 尺寸结果，0.1cm 单位 (staging; 俯视尺寸可能不准，可用于辅助) |
| `0x060` | R/W | `CPU_MATCH_ACTION0` | Cam0 匹配/动作: [0] match, [1] grab, [2] skip, [3] error, [31:16] 目标编号 (staging) |
| `0x080` | R | `LIVE_RED_AREA0` | Cam0 红色 mask 像素数 (快照) |
| `0x084` | R | `LIVE_BLUE_AREA0` | Cam0 蓝色 mask 像素数 |
| `0x088` | R | `LIVE_YEL_AREA0` | Cam0 黄色 mask 像素数 |
| `0x0A0` | R | `LIVE_BBOX_MIN0` | Cam0 bbox 最小: [31:16] Y_MIN, [15:0] X_MIN |
| `0x0A4` | R | `LIVE_BBOX_MAX0` | Cam0 bbox 最大: [31:16] Y_MAX, [15:0] X_MAX |
| `0x0A8` | R | `LIVE_CENTER0` | Cam0 中心: [31:16] Y_CEN, [15:0] X_CEN |

### 2.3 Camera 1 寄存器（侧面：形状 + 颜色 + 尺寸）

Camera 1 偏移量 = Camera 0 对应偏移量 + `0x100`。

| Offset | R/W | 名称 | 描述 |
|--------|-----|------|------|
| `0x108` | R | `SYS_STATUS1` | [0] `feature_valid`; [31:16] `frame_id` |
| `0x10C` | W | `SYS_ACK1` | [15:0] = 要确认的 `frame_id` |
| `0x114` | R/W | `CFG_CAM1_ROI_TL` | [31:16] ROI MIN_Y, [15:0] MIN_X (staging) |
| `0x118` | R/W | `CFG_CAM1_ROI_BR` | [31:16] ROI MAX_Y, [15:0] MAX_X (staging) |
| `0x120` | R/W | `CFG_CAM1_RED_TH_0` | 红色阈值下限 (staging) |
| `0x124` | R/W | `CFG_CAM1_RED_TH_1` | 红色阈值上限 (staging) |
| `0x128` | R/W | `CFG_CAM1_BLUE_TH_0` | 蓝色阈值下限 (staging) |
| `0x12C` | R/W | `CFG_CAM1_BLUE_TH_1` | 蓝色阈值上限 (staging) |
| `0x130` | R/W | `CFG_CAM1_YEL_TH_0` | 黄色阈值下限 (staging) |
| `0x134` | R/W | `CFG_CAM1_YEL_TH_1` | 黄色阈值上限 (staging) |
| `0x13C` | R/W | `CFG_CAM1_LUMA_TH` | [31:16] TH_LUMA_MAX, [15:0] TH_LUMA_MIN (staging) |
| `0x140` | R/W | `CPU_OSD_CTRL1` | Cam1 OSD 控制字 (staging) |
| `0x144` | R/W | `CPU_OSD_BBOX_MIN1` | Cam1 稳态 bbox 最小坐标 (staging, 红框) |
| `0x148` | R/W | `CPU_OSD_BBOX_MAX1` | Cam1 稳态 bbox 最大坐标 (staging, 红框) |
| `0x14C` | W | `CFG_COMMIT1` | [15:0]=`config_seq`；触发 Cam1 整组 staging→active（不含全局寄存器） |
| `0x150` | R | `CFG_STATUS1` | [31:16] `active_seq`, [15:0] `pending_seq` |
| `0x154` | R/W | `CPU_RESULT_COLOR1` | Cam1 颜色结果 (staging) |
| `0x158` | R/W | `CPU_RESULT_SHAPE1` | Cam1 形状结果 (staging) |
| `0x15C` | R/W | `CPU_RESULT_SIZE1` | Cam1 尺寸结果，0.1cm 单位 (staging; **侧面主尺寸来源**) |
| `0x160` | R/W | `CPU_MATCH_ACTION1` | Cam1 匹配/动作 (staging) |
| `0x180` | R | `LIVE_RED_AREA1` | Cam1 红色 mask 像素数 (快照) |
| `0x184` | R | `LIVE_BLUE_AREA1` | Cam1 蓝色 mask 像素数 |
| `0x188` | R | `LIVE_YEL_AREA1` | Cam1 黄色 mask 像素数 |
| `0x1A0` | R | `LIVE_BBOX_MIN1` | Cam1 bbox 最小坐标 |
| `0x1A4` | R | `LIVE_BBOX_MAX1` | Cam1 bbox 最大坐标 |
| `0x1A8` | R | `LIVE_CENTER1` | Cam1 中心坐标 |
| `0x1AC` | R | `LIVE_HEIGHT_PX` | Cam1 **侧面物体高度（像素数）**，用于辅助尺寸判定 (快照) |

### 2.4 预留 / 空白区

| 区域 | 用途 |
|------|------|
| `0x06C-0x07F` | 将来 FPGA 尺寸分类阈值 / 全局控制扩展 |
| `0x08C-0x09F` | Cam0 WHITE/BLACK 面积寄存器 |
| `0x0B0-0x0FF` | Cam0 扩展 / 下采样 ROI 缓冲 |
| `0x16C-0x17F` | Cam1 扩展 |
| `0x18C-0x19F` | Cam1 WHITE/BLACK 面积寄存器 |
| `0x1B0-0x1FF` | Cam1 扩展 / 下采样 ROI 缓冲 |

---

## 3. 复位默认值表

### 3.1 全局

| 寄存器 | 复位默认 |
|--------|----------|
| `SYS_CTRL` | `0x0`（两路视觉关、OSD 关） |
| `CPU_HEARTBEAT` | `0x0` |
| `CPU_ARM_STATE` | `0`（idle） |
| `CPU_ERROR_CODE` | `0`（无错误） |

### 3.2 Camera 0 / Camera 1（各自独立）

| 寄存器 | 复位默认 |
|--------|----------|
| `SYS_STATUS_N.feature_valid` | `0`；`frame_id=0` |
| `active_seq` / `pending_seq` | `0` / `0` |
| `CFG_CAMn_ROI_TL/BR` | 全画面 |
| 颜色阈值 | 保守值（现场标定后固化） |
| `CFG_CAMn_LUMA_TH` | 宽门限 |
| `CPU_OSD_*N` | `0` |
| `CPU_RESULT_*N` | `0` |
| `CPU_MATCH_ACTIONN` | `0` |
| `LIVE_HEIGHT_PX` | `0` |

---

## 4. CPU 操作流程

```
上电:
  1. 读 REG_MAGIC == 0x375A? 否则挂起打印
  2. 编译期: #ifndef IO_APB_SLAVE_x_BASE → #error

配置 Cam0 俯视（形状+颜色）:
  3. 写 CFG_CAM0_ROI_TL/BR, CFG_CAM0_*_TH_*, CFG_CAM0_LUMA_TH
  4. config_seq0++ (与上次不同)
  5. 写 CFG_COMMIT0 = config_seq0
  6. 轮询 CFG_STATUS0 直到 active_seq == config_seq0

配置 Cam1 侧面（形状+颜色+尺寸）:
  7. 写 CFG_CAM1_ROI_TL/BR, CFG_CAM1_*_TH_*, CFG_CAM1_LUMA_TH
  8. config_seq1++ (与上次不同)
  9. 写 CFG_COMMIT1 = config_seq1
 10. 轮询 CFG_STATUS1 直到 active_seq == config_seq1

主循环（两路独立轮询，互不阻塞）:
 11. IF SYS_STATUS0.feature_valid == 1 AND frame_id0 变化:
       → 读 LIVE_*0 → Cam0 分类(颜色+形状) → 写 SYS_ACK0 = frame_id0
 12. IF SYS_STATUS1.feature_valid == 1 AND frame_id1 变化:
       → 读 LIVE_*1 + LIVE_HEIGHT_PX → Cam1 分类(颜色+形状+尺寸) → 写 SYS_ACK1 = frame_id1
 13. 融合两路结果: Cam0 颜色/形状 + Cam1 尺寸 → 最终决策
 14. 回写 CPU_RESULT_*0/1, CPU_MATCH_ACTION0/1, CPU_ARM_STATE
 15. 回写 CPU_OSD_*0/1 (BBox 坐标)
 16. 写 CFG_COMMIT0 = config_seq0 (全局结果寄存器跟 Cam0 commit)
 17. 写 CFG_COMMIT1 = config_seq1 (Cam1 结果寄存器跟 Cam1 commit)

心跳:
 18. 周期写 CPU_HEARTBEAT 递增值
```

### 两路结果融合逻辑（CPU 端）

```
颜色: 以俯视 Cam0 为主，Cam1 作为校验
形状: 以俯视 Cam0 为主（顶视图更适合形状判读）
尺寸: 以侧面 Cam1 为主要来源（侧面看高度/深度更准）
      如果 Cam1 未就绪，降级用 Cam0 的面积估算
```

---

## 5. 兼容性：FPGA 变更时的 CPU 适配策略

CPU 代码通过以下编译开关适配 FPGA 侧的决策变化：

| 宏 | 默认值 | FPGA 变更时 |
|----|--------|-------------|
| `APB_BASE` | `IO_APB_SLAVE_0_INPUT` (占位) | 改为 `soc.h` 真值 |
| `CAM_ENABLED(n)` | `(1)` (两路都启用) | 可设 CAM_ENABLED(1)=0 临时关侧面 |
| `HANDSHAKE_MODE` | `INDEPENDENT` | FPGA 要求合并时改为 `MERGED` |
| `CAM_COMMIT_GLOBAL` | `CAM0` | 全局寄存器跟哪个摄像头 commit |

---

## 6. 解除 RTL/SoC 执行前 Gate 的最终前置

在补齐 `generated_soc_summary_YYYY-MM-DD.md` 且满足以下后，才最终解除 Gate 并进入 RTL：

1. `soc.h` 摘要：UART0/UART1、CLINT、PLIC、APB user window、AXI user window、DDR base/size、`SYSTEM_CLINT_HZ`。
2. 选定的 `io_apbSlave_x_*` 端口空闲，PADDR 宽度 ≥ 9 bit（覆盖 `0x000-0x1FF`）。
3. `wb0_*` 是否已接入 feature_extract（Cam1 侧面可用性前提）。
4. CPU BSP 占位地址被 `soc.h` 真源替代。
5. OSD 真插 `top.v` 前另交小包审查 fanout/timing。

## 7. 仍待决/预留

- 尺寸分类阈值：首版由 CPU 查表（`param_table.c`），不在本手册寄存器。
- WHITE/BLACK 颜色：预留 `0x08C/0x090`（Cam0）和 `0x18C/0x190`（Cam1），首版 CPU 用排除法。
- 分组 commit：`CFG_COMMIT` 位域 [ROI]/[THRESH]/[OSD]，首版不实现。
- PLIC 中断：首版轮询。
- Cam1 侧面高度估计（`LIVE_HEIGHT_PX`）：仅当 FPGA 能从侧面图计算物体像素高度时才有效；若 FPGA 无法提供，CPU 降级用 Cam0 面积估算。

---

**修订记录**
- 2026-07-05 (草案 v2)：**双路扩展**。全局寄存器独立出 `0x000-0x017` 区域；Cam0 寄存器不动 (`0x000-0x0FF`)，Cam1 新增 `0x100-0x1FF` 完整镜像；每路独立 `SYS_STATUS/ACK`、`CFG_*/COMMIT/STATUS`、`CPU_OSD_*/RESULT_*/MATCH_ACTION`、`LIVE_*`；Cam1 新增 `LIVE_HEIGHT_PX`(`0x1AC`)。新增编译开关 `HANDSHAKE_MODE`、`CAM_ENABLED`、`CAM_COMMIT_GLOBAL`。新增 §5 兼容性策略。新增两路结果融合逻辑。
- 2026-07-05 (草案 v1.1)：新增 CPU→FPGA 结果回写寄存器。
- 2026-07-03 (草案 v1)：基于 plan v4.3 起草，含 Codex 三次复核确认的 B4 契约基线。
