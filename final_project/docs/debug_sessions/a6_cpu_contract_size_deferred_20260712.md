# A6 CPU 接口契约与尺寸暂缓记录

> 日期：2026-07-12  
> 范围：CPU 纯 C 语义契约与 Host 测试。未修改 FPGA、SoC、APB 地址、OSD、`main.c`、约束或机械臂控制。

## 本轮决策落实

用户决定暂缓尺寸标定。实现将尺寸能力显式建模为 `SIZE_STATE_UNAVAILABLE`：

| 任务 | 尺寸未标定时的行为 |
|---|---|
| 任务一 | 颜色 + 正方体继续判断 |
| 任务二 | 形状（正方体）+ 颜色继续判断 |
| 任务三 | `WAIT + SIZE_UNAVAILABLE`，不执行、不跳过、不伪造尺寸 |
| 任务四 | `WAIT + SIZE_UNAVAILABLE`，不执行、不跳过、不伪造尺寸 |

## 新增契约

新增 `cpu/app/include/competition_contract.h` 和 `cpu/app/src/competition_contract.c`。这是寄存器无关 API，不含 APB 地址或 bitfield：

| 结构 | 语义 |
|---|---|
| `target_config_t` | `valid + task_mode + target_color + reference_size`；先 staging，再 apply 后锁存为 active |
| `operator_event_t` | `PLACE`、`REMOVE`、`ABANDON`、`RESET` 加单调 `event_seq` |
| `result_status_t` | 识别颜色/形状、可用时的尺寸、`SIZE_STATE`、决策、理由码、当前轮状态和事件序号 |

事件规则：

- 同序号重复事件视为消抖后的重传，幂等返回，不创建第二轮事务。
- 旧序号拒绝。
- `PLACE` 只能在已 apply 的有效目标下开启一轮。
- 目标在轮内锁存；staging 的新配置不会修改当前轮。
- 只有相同 `event_seq` 的 ACK 才能结束已锁存的执行或跳过结论。
- `ABANDON` 输出 `SKIP + OPERATOR_ABANDONED`；超时输出 `WAIT + ROUND_TIMEOUT`。

## 验证

| 检查 | 结果 |
|---|---|
| 新接口契约 Host 测试 | `competition_contract: 35/35 passed` |
| 四任务与 20 轮 Mock 回归 | `competition_rounds: 135/135 passed` |
| 原 `task_matcher` 回归 | `82/82 passed` |
| `git diff --check` | 通过 |

运行新契约测试：

```powershell
powershell -ExecutionPolicy Bypass -File final_project\cpu\tests\run_competition_contract_host.ps1
```

Host 测试仍会显示既有 `soc.h` 缺失占位告警；这只是测试穿过 `board_io.h` 的安全门，并不表示 APB、SoC 或硬件地址已就绪。

## NOT VERIFIED

- FPGA 如何把物理按键/拨码转换为上述事件和配置；地址、位宽、CDC、去抖均未定版。
- CPU 到 OSD 的结果寄存器映射、RISC-V 构建、SoC/APB、真实特征、板级 20 轮。
- 尺寸标定、任务三/四真实识别与机械臂动作。
