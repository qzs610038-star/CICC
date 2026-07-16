# SESSION HANDOFF — 2026-07-17 main 合并后恢复入口

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

## 当前 main 的已合入内容

- `f2ba66e`：合并 `@libaoxun688@14b9248` 的 Hard SoC 原子真源，包括 IP/XML/wrapper/BSP/顶层、APB0 `REG_MAGIC` 与 testbench；BSP 地址为 `0xE8100000`。
- `77c88d2`：仅合并 `@wsc6090-CPU@82892d3` 的 DSI 初始化 `.mem` 相对路径修复。
- 候选分支中的状态、Map/PNR/板级文档均未整体合入；原因和逐项舍弃内容见 `docs/merge_governance/records/`。

## 当前事实与禁止项

- Hard SoC 源码已在仓库，但这些硬件输入合并后尚未在当前 SHA 重新跑 Efinity；Map/PNR/STA/CDC、bitstream/ELF、USER2、CPU 取指、UART0 与 APB 实读全部为 `NOT VERIFIED`。
- 下一位只允许先由 `@libaoxun688` 的已配置 Efinity 环境重建并提交脱敏证据；随后才可做 `USER2` + 片上 RAM `0xF9000000` + UART0 115200 的隔离验证。
- 禁止 `USER1`、Flash、外部 DDR、UART2/J52、机械臂接线、myCobot 帧和任何动作。myCobot 的 1000000 baud 与 UART0 Hello 的 115200 是独立 Gate。

## 图谱与同步

- 共享图谱 `D-cicc_cbm-main` 已在 `a473a52` 后持久化为 7560 nodes / 16960 edges；artifact 为 `.codebase-memory/graph.db.zst`，准确基线以 artifact metadata 为准。
- 当前图谱能定位 `competition_project_single_camera/src/apb_reg_magic.v`、`cpu_bringup/uart_hello_onchip/` 和单摄 `dsi_tx_top.v`。图谱仅用于定位，不替代 XML、RTL、日志和板测证据。

## 下一立即动作

`@libaoxun688`：在当前 `main` 重新运行 Efinity，并回传 Efinity 版本、Map/PNR/STA/CDC、关键 warning、bitstream/ELF SHA-256 和未验证边界。未完成该动作前，其他人不得更新硬件 PASS 结论。
