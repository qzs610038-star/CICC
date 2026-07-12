# CPU 队友分支合并 Codex Review Packet（2026-07-12）

## 结论

`origin/dev/wsc6090-CPU@af82e52` 可合并，但原提交不能原样进入主线。Codex 在隔离工作树中修正两项阻塞：任务二误用了尺寸精确匹配；同一目标每帧重写会解除 `GRAB_REQUESTED`，存在重复抓取风险。

当前结论仅为 Host 层通过；RISC-V 交叉构建、SoC/APB、UART2、PLIC、OSD、bitstream 与机械臂实机均未验证。

## 合并范围

- CPU 队友原始提交：`af82e52 feat(cpu): refine task matcher workflow`
- 合并基线：`origin/main@e2b3902`
- 冲突文件：`final_project/cpu/CPU_MODULE_PLAN.txt`
- 主要代码：`task_matcher.h/.c`、`main.c`、`test_task_matcher.c`

## Codex 阻塞修正

1. 官方细则 §二.3.1 ②规定任务二的 5 个物体为相同尺寸规格，目标是指定颜色正方体。因此 `TASK_MODE_2` 改为颜色+形状匹配、尺寸通配；旧 API 的精确尺寸语义隔离为 `TASK_MODE_LEGACY_EXACT`。
2. `task_matcher_set_target_ex()` 对同一轮、完全相同的目标改为幂等。只有 `task_matcher_next_round()` 后才能用相同目标重新布防，避免 `main()` 每帧读取目标寄存器导致连续 GRAB。
3. 合并冲突保留主线关于 `soc.h`、APB/CDC/OSD、round_controller 与板到臂 UART 未闭环的边界，不把 Host 代码完成描述成上板完成。

## 验证

- MSVC 19.42，`/std:c11 /W4 /utf-8`
- `test_classifier`: 31/31 PASS
- `test_param_table`: 81/81 PASS
- `test_task_matcher`: 146/146 PASS
- 合计：258/258 PASS
- Efinity RISC-V GCC 8.3.0：`task_matcher.c`、`main.c` compile-only PASS；使用占位 APB 基址，出现预期 `#warning`，不代表正式固件可上板。
- 已知 warning：`vision_classifier.c` 的 `zero_result` 在 MSVC 下出现 C4132；非本分支新增，未作为失败处理。

## GO / NO-GO

- GO：合并到本地 `main`；继续 APB MAGIC/心跳和 UART2 无动作验证。
- NO-GO：不得据此接通机械臂动作；不得宣称 round_controller、PLIC/Ring Buffer、板到臂 1 Mbps、GET_ANGLES 或抓放闭环完成。

## 下一步最小动作

按 `mycobot_board_debug_execution_plan_20260712.md` 依次执行真源冻结、APB、UART2 无动作、GET_ANGLES、PLIC/Ring Buffer dry-run、低速空载单段、抓放闭环；任何阶段失败立即停在当前门，不越级。
