# CPU_MODULE_PLAN.txt 审查反馈 - Round 1

来源：Claude 审查 CPU 源码后与 plan 对照
日期：2026-07-12
状态：已交给 Codex 更新 plan

## P0 必须修正

### P0-1：main.c 的 error_code 清零 bug

`commit_global` 成功后执行 `err_code = 0`，会把同一迭代中 `commit_results` 失败设置的错误码吞掉。这是上板后最难排查的静默故障。

要求 plan 明确：
- `commit_results fail + commit_global success` 时，ERROR_CODE 仍必须非 0。
- 修复方案可以是本轮局部错误码 `loop_err`，也可以是 latch 型错误码。
- 不能再把当前 commit 失败处理标记为完全完成。

### P0-2：四任务判定逻辑缺少函数签名

plan 只说要加 `task_mode`，但没有给出清晰的判定函数签名和伪代码。任务三比较对象是 `reference_size`，任务四比较对象是 `target_size`，两者不同。

要求 plan 明确：
- `task_target_t` 需要包含 `task_mode`、`target_size_cm_x10`、`reference_size_cm_x10`、`round_state`。
- `task_matcher_evaluate()` 内部必须按 task_mode 分派。
- 任务三：比较 `obs.size_cm_x10` 与 `reference_size_cm_x10`。
- 任务四：比较 `obs.size_cm_x10` 与 `target_size_cm_x10`。
- 差值规则和容差必须集中成常量，不允许散落魔法数字。

### P0-3：五色目标 vs 2-bit color_sel 的矛盾

比赛要求白/黑/红/蓝/黄五色，但旧 `TARGET_SEL.color_sel[1:0]` 只有 4 个编码，且已经用掉：

- `00` = 通配
- `01` = 红
- `10` = 蓝
- `11` = 黄

这个旧编码无法表示白/黑。plan 必须明确扩展方案，不能只写“target_color 支持五色”。

建议方向：
- 正式硬件契约改为 3-bit `target_color_sel`。
- 未定版前，Host 层通过显式 API/mock/UART 调试命令注入五色目标。
- 不得打开 `TARGET_SEL_AVAILABLE=1` 去读取旧 2-bit 字段作为五色目标来源。

## P1 应该修正

### P1-1：测试统计必须重跑

开始四任务修改前，必须重跑全部已有 host 测试，并更新实际通过数。不能继续只写“维护时未重跑”。

历史归档基线为：
- classifier 31
- param_table 81
- task_matcher 82
- 合计 194

### P1-2：arm_controller 归属和集成阻塞要写清楚

`arm_controller.c`、`mycobot_protocol.c`、`mycobot_transport.c` 已在 CPU 目录中存在，但 `main.c` 尚未调用 `arm_controller_tick()`。

这应标为“机械臂控制代码骨架已存在，但主循环集成未完成”，阻塞条件包括：
- 板上 UART 驱动未定版
- FPGA-to-myCobot UART 接线和电平未确认
- 四任务一轮一事务状态机未定版

### P1-3：形状融合组合表需显式列出

`fuse_results()` 中非法组合落到 `SHAPE_UNKNOWN` 的策略是合理的，但 plan 应列出组合表，方便 Codex 审查。

## P2 建议改进

- `startup_qcrv32.S` trap/interrupt handler 仍是 halt，占位限制需写入 plan。
- param_table 有 API，但缺现场标定流程。
- `conveyor_control.c` 是空桩，若不用应标为废弃或未接入。
