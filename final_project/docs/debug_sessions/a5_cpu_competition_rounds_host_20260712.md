# A5 CPU 四任务与逐轮事务 Host 验证记录

> 日期：2026-07-12  
> 范围：仅 `final_project/cpu/` 的纯 C 逻辑和宿主测试；未修改 FPGA、SoC、约束或机械臂控制。

## 本轮交付

- `cpu/app/include/competition_tasks.h` 与 `cpu/app/src/competition_tasks.c`
  - 固化官方四任务判断规则和可解释理由码。
  - 任务一、二：指定颜色正方体；任务三：相对于 2cm/3cm 参考物边长差恰为 1cm；任务四：相对于 2cm/3cm 目标物边长差不超过 0.5cm。
- `cpu/app/include/round_controller.h` 与 `cpu/app/src/round_controller.c`
  - 单轮事务状态：等待观察、等待 ACK、完成、超时、放弃。
  - 首个最终“执行/跳过”结论锁存，后续帧不会重复产生动作语义；只有匹配 `event_seq` 的 ACK 才能结束该轮。
- `cpu/tests/test_competition_rounds.c` 与 `cpu/tests/run_competition_rounds_host.ps1`
  - 覆盖四项规则、无尺寸结果时等待、错误 ACK、超时、放弃、软复位和 20 轮 Mock 场景。

## 验证结果

| 检查 | 结果 |
|---|---|
| 新四任务/逐轮测试 | `competition_rounds: 135/135 passed` |
| 原有 `task_matcher` 回归 | `82/82 passed` |
| `git diff --check` | 通过 |

宿主命令：

```powershell
powershell -ExecutionPolicy Bypass -File final_project\cpu\tests\run_competition_rounds_host.ps1
```

测试显式使用 `APB_VISION_BASE_PLACEHOLDER`，编译日志会保留既有 `soc.h` 缺失警告。该宏只用于纯 C 测试通过 `board_io.h` 的安全门；本轮代码不访问 MMIO，不能将测试通过解释为 SoC/APB 已接入。

## 设计边界

- 未改动旧 `task_matcher`，保留其既有调用语义和回归测试。
- `round_controller` 不调用 `arm_controller`，只输出执行或跳过的事务语义；后续硬件安全门通过后才能由集成层把执行语义映射为机械臂请求。
- 未定义或读取 FPGA 目标寄存器、APB 地址、OSD 写回格式或 CDC 协议。

## NOT VERIFIED

- RISC-V 交叉构建、SoC 固件运行、APB/OSD 回写、FPGA 目标输入、真实特征快照和板级 20 轮。
- 任何机械臂 UART、夹爪或实际动作。
- 任务三/四现场口径如发生官方书面更新，必须先更新真值表和测试，不得仅改代码。
