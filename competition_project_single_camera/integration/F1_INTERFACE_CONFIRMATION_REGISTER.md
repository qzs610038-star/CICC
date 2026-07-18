# F1 最简接口确认册（本机语义基线）

> 基线：`46492acde39959c96575a1e58e59f0da9d22ea42`
> 状态：`LOCAL SEMANTICS CONFIRMED / TEAM SIGN-OFF PENDING / WIRE ABI NOT FROZEN`

本页只确认已能由现有契约和源码核对的**语义与安全边界**，不产生 APB 地址、位宽、PSTRB、IRQ、CDC 实现、复位或 OSD 像素 ABI。动态 Gate 与板级状态仍以 [CURRENT_STATE.md](../../CURRENT_STATE.md) 为准。

| ID | 本机固定信息 | 唯一语义来源 | 明确保留为未冻结 |
|---|---|---|---|
| I1 | FPGA 向 CPU 发布完整帧快照；CPU 只消费稳定快照，对同一 `frame_id` ACK；未 ACK 的新帧只能标记 overrun，不能回压 HDMI。 | [特征契约](single_camera_feature_contract.md#4-帧快照载荷) / [总账](F1_INTERFACE_ALIGNMENT_DRAFT.md#4-i1fpga--cpu-特征快照已对齐的语义) | CDC 方案、寄存器布局、读序、ACK 位点、PSTRB、IRQ、复位。 |
| I2 | CPU 配置必须遵循 `staging → commit(config_seq) → frame-boundary active`；统计只使用 active 配置。 | [特征契约](single_camera_feature_contract.md#3-cpu-配置语义) / [总账](F1_INTERFACE_ALIGNMENT_DRAFT.md#5-i2cpu--fpga-统计配置只冻结语义) | 配置寄存器、commit/active 回执、CDC 与实际 VSYNC 接入。 |
| I4 | CPU 只产生可解释、可追溯的结果语义；F1 中 `arm_enabled=0`，不得把目标命中表述为机械臂已执行。 | [总账](F1_INTERFACE_ALIGNMENT_DRAFT.md#7-i4cpu--fpga-osd-结果语义初步提案) | result/OSD ABI、提交寄存器、清屏、字体、像素时序和 OSD 实现。 |
| I5 | `OUT OF F1 / BLOCKED`：不接 J52、不初始化 UART2、不发送 myCobot 帧。 | [总账](F1_INTERFACE_ALIGNMENT_DRAFT.md#8-i5cpu--mycobot明确不属于当前-f1) | 全部电气、串口协议与动作条件。 |

I0 仍仅是板级 Gate，I3 的真实输入来源仍未定义；二者不在本次本机固定范围内。

## 队友确认

| 角色 | 确认内容 | 状态 | 日期 / SHA |
|---|---|---|---|
| libaoxun | I1/I2 FPGA 语义与“不回压 HDMI”边界 | 待确认 | — |
| wsc | I1 消费/ACK、I2 配置意图、I4 结果语义 | 待确认 | — |
| qzs | I4 可解释展示边界与 I5 阻断 | 待确认 | — |

三人确认后，仅可将本页状态改为 `TEAM SEMANTICS CONFIRMED`；任何 wire ABI 冻结必须另附同批 F1 ABI Review Packet。
