# 预处理 APB 与 CDC 接口契约草案

> 日期：2026-07-11
> 状态：仅草案。当前不存在匹配的 APB 从机 RTL、生成 SoC 基址或 CDC 实现。
> 目的：在不分配地址、不修改 RTL 的前提下，冻结后续实现边界。

## 适用范围

本契约用于把像素域中的 `vision_preprocess_channel` 经后续 APB 寄存器从机连接到板上 CPU。FPGA 提供帧稳定的 ROI/统计特征；CPU 负责阈值选择、分类、结果回写和 OSD 控制值。不得把分类或 myCobot 控制迁回 RTL。

## 通道规则

契约按摄像头通道分别适用。当前只有 ch1 调试旁路；ch0 尚未实例化，在 ch1 协议和真实视频验证通过前不得增加。

## 特征快照：像素域到 APB

像素域只能在帧边界发布新快照。置位 `snapshot_valid` 前，必须锁存以下全部字段。

| 字段 | 宽度 | 当前来源 | 后续 APB 行为 |
|---|---:|---|---|
| `frame_id` | 16 | `o_frame_id` | 与 valid 一同读取，用于匹配 ack |
| `snapshot_valid` | 1 | `o_snapshot_valid` | 只读状态位 |
| `roi_pixel_count` | 32 | `o_roi_pixel_count` | 只读快照字段 |
| `sum_r/g/b/y` | 各 32 | 顶层输出已存在 | 只读快照字段；当前未导出为调试探针 |
| `red/blue/yellow_area` | 各 32 | 对应输出 | 只读快照字段 |
| `fg_area` | 32 | `o_fg_area` | 只读快照字段 |
| `bbox_min/max/center` | 各 32 | 对应输出 | `{y[15:0],x[15:0]}`，只读 |
| `status` | 32 | `o_status` | 只读；bit 0 为空前景，bit 1 为非法 ROI |
| `dropped_frames` | 32 | `o_dropped_frames` | 只读诊断字段 |

### 快照 CDC 要求

1. `valid` 置位期间，像素域必须保持整组快照不变。
2. 仅在快照寄存器组稳定后，像素域才可向 APB 域传递单比特 request/toggle。
3. APB 域同步该请求，只对 CPU 暴露稳定的快照寄存器组。
4. CPU 先读 `status/frame_id`，再读所有字段，最后复读 `status/frame_id`；不一致则丢弃且不发 ack。
5. CPU 写入 `ACK(frame_id)`，APB 域以 request/toggle 方式传回像素域。
6. 仅当返回帧号与锁存快照帧号匹配时，像素域才清除 valid。
7. 不匹配或陈旧 ack 不得改变快照；若实现诊断计数器，应计入该事件。

禁止将持续变化的多位像素总线直接同步到 APB 域。

## 配置：APB 到像素域

每路 staging 字段包括 ROI、背景 RGB、前景差分、亮度范围和颜色掩码阈值。CPU 只能写 staging 寄存器。

1. CPU 写完整组 staging 字段。
2. CPU 写 `CFG_COMMIT(config_seq)`。
3. APB 域把一次 commit request/toggle 及稳定 staging 寄存器组传入像素域。
4. 像素域只能在 VSYNC 边界将完整 staging 组采样为 active 配置。
5. 像素域回传 `active_seq=config_seq` 给 APB 域。
6. 仅当 `CFG_STATUS.active_seq` 匹配时，CPU 才可认为配置已经生效。

重复的 `config_seq` 不代表新提交。若 VSYNC 前到达多次提交，以最后一次完整 staging 数据为准。APB 不得直接驱动 `vision_preprocess_channel` 配置输入。

## CPU 到 OSD

CPU 可以暂存分类结果和显示 bbox，但 OSD 必须使用在 VSYNC 采样的像素域 active 寄存器组。第一版 OSD 只显示边框/状态，不得影响分类或特征生成。

| 字段 | 方向 | 规则 |
|---|---|---|
| OSD 使能/样式 | CPU -> 像素域 | staging 后在 VSYNC 生效 |
| OSD bbox min/max | CPU -> 像素域 | `{y,x}` 坐标格式；在 VSYNC 生效 |
| CPU 结果/状态 | CPU -> 像素域 | 仅可选显示元数据 |

OSD 插入必须另行提交顶层时序/fanout Review Packet，不能仅依据本文档实施。

## 寄存器布局状态

`integration/register_map.md` 已在 2026-07-13 与 `cpu/app/include/board_io.h` 的当前候选偏移对齐，但二者仍不是硬件事实。必须在生成 `soc.h` 给出真实 APB 窗口，并完成 APB/CDC Review Packet 后，才能为本文字段冻结槽位。本文刻意不定义基址；`roi_pixel_count`、`sum_r/g/b/y`、任务模式、五色目标和理由码仍未分配正式偏移。

## 明确暂缓项

- 本契约不新增 APB、CDC、OSD、CPU 或 ch0 RTL。
- 不授权修改 `board_io.h` 偏移或 `FG_AREA_AVAILABLE`。
- 仿真、真实摄像头时序验证、Debugger 采集、PNR、bitstream 和板级测试继续暂缓。
- 在这些门禁关闭前，特征数据不得用于分类或 myCobot 控制。
