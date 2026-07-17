# SESSION HANDOFF — 2026-07-17 G1 当前批次板级 Gate 恢复入口

## 恢复前必读

1. `AGENTS.md`
2. `CURRENT_STATE.md`
3. `docs/merge_governance/BRANCH_MERGE_GOVERNANCE.md`
4. `docs/merge_governance/MERGE_REGISTER.md`
5. 本文件

开始任何修改前运行：

```powershell
powershell -ExecutionPolicy Bypass -File tools\agent_handoff_health_check.ps1
git status --short --branch
```

## 当前 Git 与已合入内容

- 2026-07-17 实读：本地 `main` 与 `origin/main` 均为 `9acf4d8b2ec788ccd5777f3833a7bfb756c51cad`。
- 当前审查分支为 `codex/no-board-debug-plan-20260717@1dc6bcea69d39f694ad06cc84c0fe2c0664e90bb`，相对 `main` 仅领先一个无板 Host/治理提交；真实分支、HEAD 和 dirty 状态仍须每次恢复时重读。
- `f2ba66e` 已纳入 `@libaoxun688@14b9248` 的 Hard SoC 原子真源；`77c88d2` 仅纳入 `@wsc6090-CPU@82892d3` 的 DSI 初始化 `.mem` 相对路径修复。
- G1/G2 经核验补丁随 `683814edb46f0185fe61df5e6829ce2862fccca4` 内容提交纳入；源 worktree 的其他 dirty 内容不得再次整树合并。

## 当前事实与禁止项

- G1 已针对固定原子输入完成 Efinity 2025.2 冷构建：Map/Interface/PNR/STA/CDC/bitstream 与 Hello ELF 离线证据通过。匹配 bitstream 为 `A897E335...FCD1ACD`，匹配 ELF 为 `E5BC80A2...41928A`；无需因本文件旧结论重复冷构建。
- 上述只证明离线批次。`USER2`、CPU 取指、UART0 115200、APB 实读、视频和 OSD 仍为 `NOT VERIFIED`。
- 现有 2026-07-16 M2 操作卡/脚本绑定旧 `2EA4.../C99...` 制品，不能用于当前 G1。下一位应先完成当前批次专用 manifest、preflight、PC 范围 checkpoint 与 UART0 只读采集材料。
- 禁止 `USER1`、Flash、外部 DDR、UART2/J52、机械臂接线、myCobot 帧和任何动作。myCobot 的 1000000 baud 与 UART0 Hello 的 115200 是独立 Gate。

## 图谱与同步

- 共享图谱 `D-cicc_cbm-main` 已在 `a473a52` 后持久化为 7560 nodes / 16960 edges；artifact 为 `.codebase-memory/graph.db.zst`，准确基线以 artifact metadata 为准。
- 当前图谱能定位 `competition_project_single_camera/src/apb_reg_magic.v`、`cpu_bringup/uart_hello_onchip/` 和单摄 `dsi_tx_top.v`。图谱仅用于定位，不替代 XML、RTL、日志和板测证据。

## 下一立即动作

1. qzs/本机 Codex：按 `final_project/docs/technical_plans/board_dependent_execution_plan_20260717.md` 实现当前 G1 批次专用操作包；只做无板 dry-run，不触碰原子硬件输入。
2. wsc：可并行准备独立 `apb_magic_onchip/`，只读取同批 `soc.h` 的 APB0 offset 0、期望 `0x375A0001`；不得修改现有 Hello 或 FPGA。
3. libaoxun：在新操作包获批前不得使用旧 M2 卡上板。获批后先完成 `USER2 + RAM + PC` checkpoint，回传原始证据并等待 qzs/Codex 二次批准，再 Resume 和只读监听 UART0。
4. 未收到板级原始证据前，不得把 `CURRENT_STATE.md` 中任何板级项改为 PASS。
