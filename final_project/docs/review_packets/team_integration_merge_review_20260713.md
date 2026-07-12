# 两位队友分支与个人工作区整合 Review Packet（2026-07-13）

## 1. 目标与提交拓扑

- 基线：本地 `main@af6d32d`；远端 `origin/main@e2b3902`。
- 个人备份：`dev/qzs610038-star-workspace-backup-20260713@c835594`，已推送远端。
- 队友 A：`origin/dev/libaoxun688@7a51cba`。
- 队友 B：`origin/dev/wsc6090-CPU@dcd9584`。
- 本地整合分支：`codex/team-integration-20260713`。
- merge commit：`0072cf4`（FPGA/CPU 框架）、`7a7e7a8`（CPU round controller）；个人 WIP 回放 commit 为 `9824f6a`。

两位队友分支均使用 `--no-ff` 合并，未 squash、未 rebase、未改写其公开历史。

## 2. 冲突与裁定

实测最佳顺序为 `libaoxun688 -> wsc6090-CPU`。第二步产生四个冲突：

1. `CURRENT_STATE.md`：保留 FPGA A11/A12/A13 与 SoC 隔离证据，并新增 CPU 双层控制器整合事实；回放个人 WIP 时继续保留 180°机械臂诊断条目。
2. `CPU_MODULE_PLAN.txt`：保留本地任务二语义、幂等锁、错误锁存与 146/146 matcher 事实，再补入正式逐轮控制器状态。
3. `round_controller.h/.c`：保留 WSC 的完整操作员/逐轮/机械臂等待状态机为正式 `round_controller`。
4. LIB 的同名单轮观察控制器：改名为 `competition_round_transaction.h/.c`，同步 `competition_contract`、测试和 Host 脚本，保留目标评估、event_seq、ACK、超时与放弃能力。

## 3. 验证证据

| 验证 | 结果 |
|---|---|
| task matcher Host | 146/146 PASS |
| vision classifier Host | 31/31 PASS |
| competition round transaction Host | 135/135 PASS |
| competition contract Host | 35/35 PASS |
| competition host flow | 164/164 PASS（20 rounds） |
| A13 FPGA snapshot replay | 169/169 PASS（20 rounds；MSVC 仅关闭特有 `C4132`） |
| formal round controller Host | 115/115 PASS（含 20-round no-deadlock） |
| Host 合计 | 795/795 PASS |
| Efinity RISC-V GCC compile-only | 8 个关键源文件 PASS，`-Wall -Wextra -Werror` |
| 个人机械臂 Python | `py_compile` PASS |
| 180° preset JSON | 解析 PASS，五点齐全 |
| Git whitespace | 个人备份提交与 CPU 冲突整合均 PASS |

本轮未发送机械臂动作命令。

## 4. Codex 结论

### 本地 integration：PASS（可用于统一筛选）

- 三方提交与原始日志均已保留。
- 两套同名控制器已按职责拆层，现有测试实际运行通过。
- 个人学习指南、180°预设、J4 探针、审计记录及 13 份原始日志已纳入 Git。

### 合入远端 main：WARN / 暂不放行

1. `final_project/fpga/rtl/top/top.v` 当前 `PREPROCESS_CH1_USE_SYNTHETIC_SOURCE=1'b1` 且 `HDMI_USE_SYNTHETIC_VERIFY=1'b1`，会选择合成验证链；不能直接作为真实摄像头比赛构建。
2. `top.v` 与 `mem_test.xml` 属于 Codex 系统级审查门。本轮未对合并后的正式 `final_project` 运行完整 Efinity map/PNR/bitstream/上板回归。
3. CPU 双层控制器尚未接入 `main.c`、正式 `soc.h`、APB/CDC/OSD、UART2 或机械臂动作链。
4. myCobot 真实动作继续 NO-GO；底座连接位移、T0 真源、电平/线序/急停与 D0-D4 均未闭环。

## 5. 下一步最小动作

1. 将合成源/HDMI 验证选择改成明确的构建配置，生产默认必须回到真实输入；形成差异可审查的 debug/production 两套入口。
2. 对正式工程执行 Efinity project parse、map、PNR、时序与视频回归；保留工具版本、warning 和产物路径。
3. 在 `ARM_DISABLED` 下把正式 `round_controller` 接到板上事件/结果寄存器，先跑 20 轮无动作流程。
4. T0 表全部 PASS 前，不连接真实机械臂控制线，不发送动作帧。
