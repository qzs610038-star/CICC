# Review Packet: A7 CPU Host 端到端 20 轮流程

> 日期：2026-07-12  
> 审查结论：Host 端到端调用顺序已证明；不得等同于正式系统闭环。

## 已验证

- 配置 staging/apply 后在轮内锁存。
- `PLACE -> 观察 -> 最终结果 -> 匹配 ACK -> REMOVE` 的顺序可执行。
- 同序号事件重传幂等，错误 ACK 拒绝。
- 任务一、二在尺寸未标定条件下可输出执行或跳过及理由。
- 任务三、四不读取或发布 Mock 尺寸，固定等待后允许人工放弃。
- 20 轮 Host 流程 `164/164` 通过，并保留 A5/A6 回归。

## 不得误读

此测试没有调用 `main.c`、`board_io` MMIO、FPGA、OSD、UART 或 `arm_controller`。因此它不能证明任何板级 CPU、视频、显示或机械臂能力。

## 下一步门禁

若继续 CPU 线工作，只允许整理将来 `main.c` 接入所需的适配清单；实际接入必须等待 SoC/APB 资源问题解决，并先审核字段快照、CDC、commit/ACK 和 OSD 映射。
