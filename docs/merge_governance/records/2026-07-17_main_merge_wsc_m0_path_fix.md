# main 合并记录：@wsc6090-CPU M0 DSI 路径修复

## 身份与范围

- 合并日期：2026-07-17（Asia/Shanghai）
- 合并前 `main`：`f2ba66e`
- CPU 设计来源：`dev/wsc6090-CPU@c1651444d5a84be2eaa7dacb6a66d43dccfdf121`
- M0 handoff：`dev/wsc6090-cpu@73f96cde952fe14e5e7e7206902ed2219171978b`
- 实际候选：`82892d3ba1251d6a1eeefbe195e60c08f6688f3d`
- 裁决范围：CPU 设计语义由 `@wsc6090-CPU` 优先；DSI 路径属于 FPGA 工程语义，最终服从已合并的 `@libaoxun688` Hard SoC 工程配置。

## Findings 与结论

- P0：无文本冲突。`git merge-tree --write-tree` 与 `git cherry-pick -n 82892d3` 均未产生冲突。
- P1：候选未提交原始 Map outflow、失败日志或可复查 Efinity 输出。因此 Map/PNR/STA/CDC/bitstream/板级结论全部 `BLOCKED`，不得从候选文档写入当前状态。
- P2：本机 `EFINITY_RISCV_IDE` 未配置，无法独立运行 Efinity Map；仅完成源路径存在性与 diff 静态审查。

判定：对单行源码路径修复为 `APPROVE`；对任何构建或板级 PASS 叙述为 `CHANGES_REQUESTED`，需由 `@libaoxun688` 的 Efinity 环境提供当前 `main` 的原始输出。

## 实际纳入

仅修改 `competition_project_single_camera/src/mipi_dsi/dsi_tx_top.v`：

```text
/src/mipi_dsi/Panel_1080p_reg.mem
→ src/mipi_dsi/Panel_1080p_reg.mem
```

当前工程内 `competition_project_single_camera/src/mipi_dsi/Panel_1080p_reg.mem` 已静态确认存在。

## 未产生重复变更的 CPU 设计包

对 `c165144` 执行 `git cherry-pick -n` 未产生可提交差异：CPU→APB/OSD Review Packet 已等价存在于当前 `main`；`CPU_MODULE_PLAN.txt` 的当前版本不同，不能以旧分支整文件覆盖。其逻辑字段、CDC、复位和 wire ABI 继续仅为 `PROPOSED/TBD` 设计约束，不是 APB/OSD/SoC/板级实现事实。

## 明确舍弃

从 `82892d3` 舍弃 9 份文件：`CURRENT_STATE.md`、`WORK_LOG.md`、M0 delta CSV、3 份 debug session、M0 baseline/review 文档、PNR 计划和 board execution packet。原因是它们包含需要原始 Map/outflow 支撑的状态或证据叙述；原始证据未随提交提供，且不得整文件覆盖当前状态时间线。

同时继续排除 `dev/wsc6090-cpu-g4a-20260716@c3a6c755d5c0306e80f90ea066371114c37d085a`：它不属于本 M0 范围，且会改变 UART0 Hello 源码/构建身份。

## 验证与下一 Gate

- `git diff --cached --check`：通过。
- 当前与候选的 `git merge-tree --write-tree`：无文本冲突。
- `Panel_1080p_reg.mem`：工程内存在。
- Efinity Map/PNR/STA/CDC：本机未运行，等待已配置环境。

下一 Gate：由 `@libaoxun688` 从包含本路径修复和 Hard SoC 原子配置的当前 `main` 运行 Efinity，并提交脱敏原始 Map/PNR/STA/CDC 输出；在此之前，所有硬件与板级状态保持 `NOT VERIFIED`，机械臂范围不变。
