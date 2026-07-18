# F1 最简接口确认册（本机语义基线）

> 基线：`0c7530fd9090b1e198679410832f7e6af120df09` + 2026-07-18 用户接口修改口令
> 状态：`TEAM SEMANTICS CONFIRMED / I0 UART1 ROUTE FROZEN / I1-I4 WIRE ABI NOT FROZEN`

本页只确认已能由现有契约和源码核对的**语义与安全边界**，不产生 APB 地址、位宽、PSTRB、IRQ、CDC 实现、复位或 OSD 像素 ABI。动态 Gate 与板级状态仍以 [CURRENT_STATE.md](../../CURRENT_STATE.md) 为准。

| ID | 本机固定信息 | 唯一语义来源 | 明确保留为未冻结 |
|---|---|---|---|
| I0 | SoC UART1 路由到板载 Type-C UART1，`115200 8N1`；RX=`GPIOR_96/B12`，TX=`GPIOR_100/D12`。UART0/R0 退出活动路线，只作历史证据。 | [I0 UART1 冻结页](I0_UART1_INTERFACE_FREEZE.md) / [总账](F1_INTERFACE_ALIGNMENT_DRAFT.md#3-接口清单与冻结状态) | 当前 Hard SoC 生成物仍只有 UART0；UART1 wrapper、`.peri.xml`、`soc.h`、基址、IRQ、bitstream/ELF 和板级结果必须由新原子批次产生。 |
| I1 | FPGA 向 CPU 发布完整帧快照；CPU 只消费稳定快照，对同一 `frame_id` ACK；结果锁存后的 Host seam 另有 release-only ACK，只释放单槽、不分类、不产生第二个结果。未 ACK 的新帧只能标记 overrun，不能回压 HDMI。 | [特征契约](single_camera_feature_contract.md#5-帧原子性与-ack) / [总账](F1_INTERFACE_ALIGNMENT_DRAFT.md#4-i1fpga--cpu-特征快照已对齐的语义) | 业务 ACK 与终态释放的 wire 编码、CDC 方案、寄存器布局、读序、PSTRB、IRQ、复位。 |
| I2 | CPU 配置必须遵循 `staging → commit(config_seq) → frame-boundary active`；统计只使用 active 配置。 | [特征契约](single_camera_feature_contract.md#3-cpu-配置语义) / [总账](F1_INTERFACE_ALIGNMENT_DRAFT.md#5-i2cpu--fpga-统计配置只冻结语义) | 配置寄存器、commit/active 回执、CDC 与实际 VSYNC 接入。 |
| I4 | CPU 只产生可解释、可追溯的结果语义；F1 中 `arm_enabled=0`，不得把目标命中表述为机械臂已执行。 | [总账](F1_INTERFACE_ALIGNMENT_DRAFT.md#7-i4cpu--fpga-osd-结果语义初步提案) | result/OSD ABI、提交寄存器、清屏、字体、像素时序和 OSD 实现。 |
| I5 | `OUT OF F1 / BLOCKED`：不接 J52、不初始化 UART2、不发送 myCobot 帧。 | [总账](F1_INTERFACE_ALIGNMENT_DRAFT.md#8-i5cpu--mycobot明确不属于当前-f1) | 全部电气、串口协议与动作条件。 |

I0 的方向、板级管脚和波特率已冻结；实现与板级证据未完成。I3 的真实输入来源仍未定义。

## 队友确认

| 角色 | 确认内容 | 状态 | 日期 / SHA |
|---|---|---|---|
| libaoxun | I0 UART1 原子生成；I1/I2 FPGA 语义与“不回压 HDMI”边界 | 已确认 | 2026-07-18 / `0c7530f` |
| wsc | I0 UART1 固件适配；I1 消费/ACK、I2 配置意图、I4 结果语义 | 已确认 | 2026-07-18 / `0c7530f` |
| qzs | 单摄唯一路线、I4 可解释展示、I5 阻断、Gate 和最终集成 | 已确认 | 2026-07-18 / `0c7530f` |

后续修改本页固定信息必须再次收到用户完整口令 `确认接口文件修改，已经和wsc、libaoxun、qzs沟通。`。任何 I1–I4 wire ABI 冻结必须另附同批 F1 ABI Review Packet。
