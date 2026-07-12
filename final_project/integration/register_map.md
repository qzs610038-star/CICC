# FPGA ↔ CPU Register Map（候选契约）

> 状态：**CPU 实现候选布局，未冻结、未接入正式 APB RTL、未分配正式 SoC 基址。**
>
> 本表在 2026-07-13 与 `cpu/app/include/board_io.h` 当前偏移对齐，目的是消除文档和 Host 代码之间的旧草案冲突；它不是已经实现的硬件事实。实际基址必须来自最终 Efinity SoC 生成的 `soc.h`。帧快照、配置提交和 CPU-to-OSD 的 CDC 规则见 [preprocess_apb_cdc_contract_draft_20260711.md](preprocess_apb_cdc_contract_draft_20260711.md)。

## 全局窗口

| CPU 符号 | 偏移 | 方向 | 当前语义 |
|---|---:|---|---|
| `OFF_REG_MAGIC` | `0x000` | FPGA → CPU | 固定魔数 `0x375A`，验证 APB 窗口。 |
| `OFF_SYS_CTRL` | `0x004` | 双向候选 | Cam0/Cam1 enable、OSD enable 与 HDMI 选择。 |
| `OFF_CPU_HEARTBEAT` | `0x010` | CPU → FPGA | 32-bit 递增心跳。 |
| `OFF_CPU_ARM_STATE` | `0x064` | CPU → FPGA | 机械臂/事务状态；随全局结果提交。 |
| `OFF_CPU_ERROR_CODE` | `0x068` | CPU → FPGA | CPU、识别或机械臂错误码。 |
| `OFF_TARGET_SEL` | `0x06C` | FPGA → CPU | 旧 5-bit 目标输入候选：valid、尺寸、红/蓝/黄；**不足以覆盖正式五色四任务，待替换或扩展。** |

`TARGET_SEL` 的当前 2-bit 颜色字段只能表达 ANY/RED/BLUE/YELLOW。Host matcher 已支持 WHITE/BLACK，但白/黑板上目标注入尚未定版；不得用本字段宣称五色硬件入口完成。

## 每通道窗口

Cam0 使用下表偏移；Cam1 使用相同布局整体加 `0x100`。`OFF_CPU_ARM_STATE`、`OFF_CPU_ERROR_CODE` 和 `OFF_TARGET_SEL` 是全局字段，不随通道复制。

| Cam0 CPU 符号 | Cam0 偏移 | Cam1 偏移 | 方向 | 当前语义 |
|---|---:|---:|---|---|
| `OFF_SYS_STATUS0/1` | `0x008` | `0x108` | FPGA → CPU | bit0 valid，bits31:16 frame_id。 |
| `OFF_SYS_ACK0/1` | `0x00C` | `0x10C` | CPU → FPGA | 已处理 frame_id。 |
| `OFF_CFG_CAM0/1_ROI_TL` | `0x014` | `0x114` | CPU → FPGA | ROI `{y_min,x_min}` staging。 |
| `OFF_CFG_CAM0/1_ROI_BR` | `0x018` | `0x118` | CPU → FPGA | ROI `{y_max,x_max}` staging。 |
| `OFF_CFG_CAM0/1_RED_TH_0` | `0x020` | `0x120` | CPU → FPGA | 红色阈值下限 staging。 |
| `OFF_CFG_CAM0/1_RED_TH_1` | `0x024` | `0x124` | CPU → FPGA | 红色阈值上限 staging。 |
| `OFF_CFG_CAM0/1_BLUE_TH_0` | `0x028` | `0x128` | CPU → FPGA | 蓝色阈值下限 staging。 |
| `OFF_CFG_CAM0/1_BLUE_TH_1` | `0x02C` | `0x12C` | CPU → FPGA | 蓝色阈值上限 staging。 |
| `OFF_CFG_CAM0/1_YEL_TH_0` | `0x030` | `0x130` | CPU → FPGA | 黄色阈值下限 staging。 |
| `OFF_CFG_CAM0/1_YEL_TH_1` | `0x034` | `0x134` | CPU → FPGA | 黄色阈值上限 staging。 |
| `OFF_CFG_CAM0/1_LUMA_TH` | `0x03C` | `0x13C` | CPU → FPGA | `{luma_max,luma_min}` staging。 |
| `OFF_CPU_OSD_CTRL0/1` | `0x040` | `0x140` | CPU → FPGA | OSD 控制字 staging。 |
| `OFF_CPU_OSD_BBOX_MIN0/1` | `0x044` | `0x144` | CPU → FPGA | OSD bbox `{y_min,x_min}` staging。 |
| `OFF_CPU_OSD_BBOX_MAX0/1` | `0x048` | `0x148` | CPU → FPGA | OSD bbox `{y_max,x_max}` staging。 |
| `OFF_CFG_COMMIT0/1` | `0x04C` | `0x14C` | CPU → FPGA | 写入 config_seq，申请帧边界提交。 |
| `OFF_CFG_STATUS0/1` | `0x050` | `0x150` | FPGA → CPU | `{active_seq,pending_seq}`。 |
| `OFF_CPU_RESULT_COLOR0/1` | `0x054` | `0x154` | CPU → FPGA | `0 unknown, 1 white, 2 black, 3 red, 4 blue, 5 yellow`。 |
| `OFF_CPU_RESULT_SHAPE0/1` | `0x058` | `0x158` | CPU → FPGA | `0 unknown, 1 cube, 2 cylinder, 3 cone`。 |
| `OFF_CPU_RESULT_SIZE0/1` | `0x05C` | `0x15C` | CPU → FPGA | 0.1 cm 单位：20/25/30。 |
| `OFF_CPU_MATCH_ACTION0/1` | `0x060` | `0x160` | CPU → FPGA | 两路写入同一融合动作语义，供当前显示通道读取。 |
| `OFF_LIVE_RED_AREA0/1` | `0x080` | `0x180` | FPGA → CPU | 红色像素面积。 |
| `OFF_LIVE_BLUE_AREA0/1` | `0x084` | `0x184` | FPGA → CPU | 蓝色像素面积。 |
| `OFF_LIVE_YEL_AREA0/1` | `0x088` | `0x188` | FPGA → CPU | 黄色像素面积。 |
| `OFF_LIVE_BBOX_MIN0/1` | `0x0A0` | `0x1A0` | FPGA → CPU | `{y_min,x_min}`。 |
| `OFF_LIVE_BBOX_MAX0/1` | `0x0A4` | `0x1A4` | FPGA → CPU | `{y_max,x_max}`。 |
| `OFF_LIVE_CENTER0/1` | `0x0A8` | `0x1A8` | FPGA → CPU | `{y_center,x_center}`。 |
| `OFF_LIVE_HEIGHT_PX` | — | `0x1AC` | FPGA → CPU | Cam1 侧视像素高度。 |
| `OFF_LIVE_FG_AREA0/1` | `0x0B0` | `0x1B0` | FPGA → CPU | bbox 内前景像素面积；仅在真源与 `FG_AREA_AVAILABLE` 门关闭后有效。 |

## 暂未分配的帧统计字段

合并后的 `feature_snapshot_t` 和 Host replay 已包含 `roi_pixel_count`、`sum_r`、`sum_g`、`sum_b`、`sum_y`，但正式 APB 偏移仍未定义。它们必须在 SoC/APB Review Packet 中分配，不得复用上表已有槽位或沿用旧版 `REG_SUM_*` 偏移。

## 握手与提交规则

1. FPGA 在像素域帧边界锁存完整快照，再通过 request/toggle 向 APB 域发布稳定数据。
2. CPU 先读 status/frame_id，再读全部特征，最后复读 status/frame_id；两次不一致则丢弃且不 ACK。
3. CPU 写 `SYS_ACK=frame_id`；陈旧或不匹配 ACK 不得清除新快照。
4. CPU 配置和 OSD 字段先写 staging，最后写 `CFG_COMMIT(config_seq)`；像素域只在 VSYNC 边界切换 active 组。
5. CPU 只在 `CFG_STATUS.active_seq==config_seq` 后认为配置生效。
6. 分类结果、理由/动作和全局状态必须按同一轮锁存；OSD 不得显示半更新字段。

## 冻结条件

- 生成正式 `soc.h` 并确认 APB slave、窗口基址、时钟和复位。
- FPGA APB/CDC RTL 与 CPU `board_io` 通过同一份 Review Packet。
- 为帧统计字段、任务模式、五色目标输入、理由码和全局结果提交分配不冲突的 32-bit 槽位。
- Host 契约测试、RISC-V 构建、RTL 仿真以及板上 MAGIC/heartbeat/单帧 ACK 逐级通过。

冻结前，本表只能用于接口收口和测试，不能直接驱动机械臂动作，也不能作为 APB/OSD 已实现的证据。
