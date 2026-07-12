# Review Packet: A5 CPU 四任务与逐轮事务

> 日期：2026-07-12  
> 审查范围：纯 C 任务判定和 Host Mock 事务控制。

## 已完成

| 模块 | 结论 |
|---|---|
| `competition_tasks` | 五色、三形状输入下的四任务判断与理由码已实现 |
| 任务三 | 仅 2cm/3cm 参考有效，目标尺寸差必须为 `10`（0.1cm 单位） |
| 任务四 | 仅 2cm/3cm 目标有效，尺寸差必须 `<=5`（0.1cm 单位） |
| `round_controller` | 首个最终结论锁存，要求匹配序号 ACK，支持超时、放弃、软复位 |
| Host 测试 | 新测试 `135/135`、原 matcher 回归 `82/82` 通过 |

## 审查结论

可以把新模块作为后续 `main.c` 集成的纯软件前置，但**不可**据此宣称已经具备板上 CPU 闭环或机械臂控制能力。

## 仍然禁止

- 不把 `round_controller` 直接接到 `arm_controller`。
- 不在未定版前写死 `TARGET_CFG`、`OPERATOR_EVENT`、`RESULT_STATUS` 或 APB 地址。
- 不以 Host Mock 替代 SoC、OSD、真实相机或机械臂验收。

## 下一步

在 SoC 资源问题仍冻结时，继续设计并评审以下纯软件契约：

1. `target_config` 的四任务输入结构和 apply/lock 行为。
2. `operator_event` 的 `PLACE/REMOVE/ABANDON/RESET` 与 `event_seq/ACK` 对应关系。
3. `result_status` 的识别结果、目标判断、执行或不执行理由编码。

确认这些语义后，才可在不改变任务算法的前提下，由 FPGA/SoC 集成层映射到实际寄存器。

## NOT VERIFIED

RISC-V 构建、SoC/APB、CDC/OSD、真实视频、板级计时和机械臂均未验证。
