# A7 CPU Host 端到端 20 轮流程记录

> 日期：2026-07-12  
> 范围：纯 Host 适配与 Mock；未修改板上 `main.c`、FPGA、SoC、APB、OSD 或机械臂。

## 本轮目标

将 A5/A6 已冻结的任务规则、逐轮控制与输入/事件/结果契约串成可执行的 Host 流程，验证调用顺序，而不把未定硬件接口写入正式固件。

## 新增内容

- `cpu/app/include/competition_host_adapter.h`
- `cpu/app/src/competition_host_adapter.c`
- `cpu/tests/test_competition_host_flow.c`
- `cpu/tests/run_competition_host_flow.ps1`

适配层只提供：配置 apply、操作事件、观察结果、ACK 和时间推进。它不包含 MMIO 访问、寄存器地址、相机读取、OSD 回写、UART 或机械臂调用。

## 20 轮 Mock 覆盖

| 任务 | 轮数 | 验证行为 |
|---|---:|---|
| 任务一 | 5 | 1 轮红色正方体 `EXECUTE`，4 轮颜色不匹配 `SKIP` |
| 任务二 | 5 | 1 轮蓝色正方体 `EXECUTE`，4 轮蓝色锥体 `SKIP` |
| 任务三 | 5 | 每轮 `WAIT + SIZE_UNAVAILABLE`，再 `ABANDON` |
| 任务四 | 5 | 每轮 `WAIT + SIZE_UNAVAILABLE`，再 `ABANDON` |

每轮同时验证：同序号 `PLACE` 重传幂等、不生成第二轮；错误 ACK 拒绝；正确 ACK 完成最终结论；`REMOVE` 后才能进入下一轮。

## 验证结果

| 检查 | 结果 |
|---|---|
| 端到端 20 轮 Host 流程 | `competition_host_flow: 164/164 passed` |
| 契约回归 | `35/35 passed` |
| 四任务/逐轮回归 | `135/135 passed` |
| 原 matcher 回归 | `82/82 passed` |
| `git diff --check` | 通过 |

运行命令：

```powershell
powershell -ExecutionPolicy Bypass -File final_project\cpu\tests\run_competition_host_flow.ps1
```

测试使用既有 APB 占位宏以通过 `board_io.h` 的编译安全门；适配层本身不访问 MMIO。编译日志中的 `soc.h` 缺失警告是预期的测试边界，不是硬件已配置的证据。

## NOT VERIFIED

- 正式 `main.c` 对新契约的集成。
- FPGA 配置/事件输入、去抖、快照/CDC、APB、OSD 回写和 RISC-V 固件。
- 摄像头真实识别、尺寸标定、任务三/四真实执行、板级 20 轮和机械臂动作。
