# myCobot 实验分支同步与流程调整 Review Packet

日期：2026-07-14

审查者：Codex

## 1. 结论

本地 `main` 已与 `origin/main@39e8a92` 对齐，并从该提交创建 `codex/mycobot-experiment-main-sync-20260714`。原工作区中的本机配置和 `.agents` 交接文件已原样恢复，不进入本分支提交。

机械臂实验流程已拆分为 PC 端 E0–E7 与板上 G4–G11 两条独立证据链。当前只放行离线/只读检查；真机示教、probe、夹爪、接线、烧录和板上动作继续 NO-GO。

## 2. 主线变化对实验的影响

- G0–G3 Host/QEMU/RISC-V/ELF 软件门已进入主线，不再是未提交草稿；四组合制品仍全部 `NOT_FOR_FLASH`。
- CPU 的 16-bit 事件契约、`arm_busy` 安全门和失败即安全的 runtime 已进入主线，但尚无正式 SoC、真实硬件时基、UART2 transport 或板级闭环。
- `competition_project_single_camera/` 只是隔离候选工程；M0 新构建、匹配 bitstream、烧录、冷启动和 10 分钟画面复现未完成，不能给机械臂实验提供真实识别触发。
- PC 端成功脚本归档保持不变；旧 180°候选点位因外部基准与机械固定证据不足继续失效，必须重新示教并生成新版本，而不是修改或覆盖旧文件。

## 3. 修改文件

- `CURRENT_STATE.md`：登记同步基线、实验分支和 PC/板上双路线门禁。
- `final_project/docs/technical_plans/mycobot_pc_experiment_continuation_plan_20260714.md`：新增 E0–E7 续跑计划。
- `final_project/docs/technical_plans/mycobot_g3_to_g5_next_steps_quick_guide_20260714.md`：修正已合入主线的状态，并明确双路线。
- `final_project/docs/technical_plans/mycobot_cpu_board_bringup_implementation_plan_20260714.md`：移除过时的 PARTIAL/未提交草稿描述，更新下一会话入口。
- 本文件：记录本轮审查结论和验证边界。

未修改 `mycobot_pc_tests/teach_replay_pick.py`、成功基线归档、180° v1 脚本、任何预设、FPGA RTL/SDC/XML 或 CPU 源码。

## 4. 本轮验证结果

已执行且 PASS：

- 分支/提交/dirty 状态核对：新分支基于 `39e8a92`，本机文件恢复后无冲突；
- 三个脚本 SHA-256 核对：原成功脚本与归档同为 `A7859EC1...C945913`，180° v1 为 `62304E52...72B75CC`；
- 180° v1 `py_compile` 和 `--help`：退出码均为 0；
- 缺少 `--port` 的 probe：参数层退出码 2，在导入/连接 `pymycobot` 前 fail closed；
- 旧 `20260712_180deg` 预设离线校验：退出码 1，理由为 `external_reference.complete` 未为 true，符合预期 FAIL；
- `pymycobot` 可导入；只读枚举发现 COM4/COM5 均为蓝牙串口候选，未自动选择端口、未打开串口、未发命令。
- `run_arm_runtime_host.ps1`：disabled、simulated 均 PASS；只出现已登记的 `soc.h`/APB 占位提示。
- `run_arm_runtime_qemu.ps1`：显式使用本机 Efinity 2025.2 toolchain/QEMU 后，disabled、simulated 和 1 秒超时注入均 PASS。
- RISC-V `competition/arm_bringup × disabled/simulated` 四组合从 `39e8a92` 重建均 BUILD/ELF PASS，全部 `NOT_FOR_FLASH` 且 `uart2=excluded`；`-BoardBuild` 在创建输出目录前 fail closed。

禁止并未执行：打开机械臂串口、释放舵机、关节/夹爪动作、J52/UART2 接线、FPGA 构建/烧录和机械臂动作测试。

## 5. 下一检查点

用户若要继续真机实验，需先提交 E3 的基座刚性固定、外部角度/距离基准、净空、急停/断电和准确 COM 口证据，并明确本轮允许的单一动作段。未满足时继续停在 E2；不得仅凭 `--confirm-action-gate` 参数文字自行放行动作。
