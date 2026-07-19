# P1 Host 契约与证据前置包

> 状态：`PREPARED FOR IMPLEMENTATION / HOST ONLY / WIRE ABI NOT FROZEN / BOARD NOT VERIFIED`
>
> 依据：`P0_P1_NO_UART_CPU_AND_F1_PARALLEL_IMPLEMENTATION_PLAN_20260719.md`、冻结 I1/I2/I4 语义与当前 `CURRENT_STATE.md`

## 1. 本文件解决什么

本文件让 wsc 与 qzs 在不等待 UART1、真实 MMIO 或 FPGA 接线的情况下直接开发，并为未来硬件交接准备成批输入。它只固定 Host 测试不变量、数据字段和证据分层，不定义 APB 地址、PSTRB、IRQ、CDC 电路、按键管脚或 OSD wire ABI。

任何 C struct 都不得被当作寄存器布局。冻结接口文件保持只读；若实现发现语义必须变化，停止并提交差异，不得自行修改。

## 2. 不变量编号

| ID | Host 必须证明 | RTL TB 后续复用 | 板级仍待证明 |
|---|---|---|---|
| `P1-I1-01` | 快照读前后 `frame_id` 相同才可消费 | 单槽原子锁存 | 真实 APB/CDC 无撕裂 |
| `P1-I1-02` | 只 ACK 已成功消费的同一帧 | 错 frame ACK 拒绝 | ACK wire 编码与读回 |
| `P1-I1-03` | torn/config mismatch/diag/overflow/overrun/非 ch0 全部 fail-closed | flag 与 overrun 负例 | 同批真实快照 |
| `P1-I1-04` | 结果锁存后 idle-drain 只 release，不再分类或提交第二结果 | 终态释放场景 | 业务 ACK/flush wire 方案 |
| `P1-I1-05` | 16-bit `frame_id/config_seq` 回绕不复用旧结果 | 回绕向量 | 板级长时间运行 |
| `P1-I2-01` | staging 配置只有 commit 后才成为候选 active | frame-boundary active | 实际 VSYNC/CDC |
| `P1-I3-01` | `event_seq` 重复、乱序、抖动和长按不重复开轮 | 输入事件 TB | 物理管脚、极性、消抖周期 |
| `P1-I3-02` | `APPLY/PLACE/REMOVE/ABANDON/RESET` 的 ACK 与复位边界明确 | 事件锁存 TB | 真实输入来源 |
| `P1-I4-01` | 结果 staging 完整后，以新 `round_id` 原子提交 | result latch TB | result/OSD ABI |
| `P1-I4-02` | OSD 语义可解释且 `arm_enabled=0` | 固定结果显示表 | 像素渲染与 HDMI 回归 |
| `P1-SAFE-01` | 目标命中只能输出 `EXECUTE_ARM_DISABLED` | 安全负例 | 不证明 UART2/myCobot |

## 3. 黄金向量交换格式

建议在 wsc 所有权内创建 `competition_project_single_camera/cpu/tests/vectors/p1/`。每条 JSONL 记录至少包含：

```json
{
  "schema": "p1-feature-vector-v1",
  "case_id": "two_pixel_rgb_order_001",
  "category": "pixel_order|roi|frame_boundary|flag|overflow|snapshot",
  "input": {
    "frame_id": 1,
    "config_seq": 1,
    "rgb48_hex": "FF00000000FF",
    "vs": 0,
    "hs": 0,
    "de": 1,
    "roi": {"x0": 0, "y0": 0, "x1": 1, "y1": 0},
    "source_flags": 71
  },
  "expected": {
    "accept": true,
    "ack": true,
    "red_area": 1,
    "blue_area": 1,
    "yellow_area": 0,
    "foreground_area": 2,
    "roi_pixel_count": 2,
    "sum_luma": 510,
    "bbox_width": 2,
    "bbox_height": 1,
    "decision": "WAIT",
    "reason": "NONE"
  },
  "notes": "语义向量；不是 APB 寄存器镜像"
}
```

数值必须满足冻结最小位宽边界；`source_flags` bit7 保持 0。向量集至少覆盖计划 P1-1 与 P1-2 的全部正负例。新增字段须先更新本文件并说明是否改变冻结语义。

## 4. 采集与标定 CSV

qzs 后续在 `competition_project_single_camera/docs/calibration/` 建模板，首行固定为：

```text
sample_id,object_id,color,shape,nominal_size_cm_x10,lighting_id,position_id,frame_id,config_seq,red_area,blue_area,yellow_area,foreground_area,roi_pixel_count,sum_luma,bbox_width,bbox_height,source_flags,expected_label,observed_label,decision,reason,artifact_hash,evidence_level
```

`evidence_level` 只能取 `HOST_CALIBRATION_PROVISIONAL` 或 `SAME_BATCH_FPGA_SNAPSHOT`。无同批 FPGA 原始快照时不得升级。目标样本为 135 个稳定样本并另含空场、偏位、遮挡与不稳定负例；不足时如实标记覆盖率，任务二/尺寸结论保持 BLOCKED 或 PROVISIONAL。

## 5. OSD 与输入语义固定表

OSD 最小字段顺序固定为：`ROUND/FRAME/CONFIG`、`COLOR/SHAPE/SIZE`、`TARGET/NON_TARGET`、`DECISION`、`REASON`、`INPUT_FLAGS`、`ARM=0`。

| 状态 | 显示要求 | 保持/清除 |
|---|---|---|
| `WAIT` | 黄/白提示，显示等待原因 | 新有效结果前保持 |
| `EXECUTE_ARM_DISABLED` | 明确写目标命中但机械臂禁用 | 保持到 REMOVE/下一轮确认 |
| `SKIP` | 显示非目标与具体 reason | 保持到 REMOVE/下一轮确认 |
| 输入拒绝 | 显示 flags 与拒绝原因，不伪装识别结果 | 等待新稳定快照 |
| `RESET` | 清除旧 round/result，保留 `ARM=0` | 新一轮开始 |

输入语义先固定为持久配置 `task/target_color/reference_size_cm_x10` 与瞬时事件 `APPLY/PLACE/REMOVE/ABANDON/RESET`；不固定物理按键号、极性、周期或 APB 地址。

## 6. 20 轮回放证据包

每轮记录：`round_id`、输入事件与 `event_seq`、snapshot hash、ACK 序列、分类结果、decision/reason、结果提交次数、耗时、终态释放、`arm_enabled`。证据包必须包含 manifest、原始 runner 日志、编译器版本、输入 Git SHA、各文件 SHA-256、失败计数与未验证项。

只有 Host 严格编译、协议负例、20 轮回放及标定状态说明全部完成后，才允许写 `P1-HOST-READY`。该状态不等于 RISC-V、MMIO、APB、CDC、OSD 或板级 PASS。

## 7. 三人交接点

- wsc 交给 qzs：固定 SHA、变更文件、向量、runner、原始 Host 日志、20 轮 bundle、明确未验证项。
- qzs 先在本轮内部准备未来 H1 交接所需的一次性 Review Packet、向量 hash、OSD/输入表和三层证据矩阵；未到 `P1-HOST-READY` 不对外派 H1。
- libaoxun 不参与本轮、不需要同步或审阅；继续其既有 UART1/USER2 攻坚。
