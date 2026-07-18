# SESSION HANDOFF — 2026-07-18 三人本地集成候选

## 恢复入口

- 分支：`codex/qzs-wsc-libaoxun-integration-20260718`
- 基线：`main@9acf4d8b2ec788ccd5777f3833a7bfb756c51cad`
- WSC 来源：`cbe6eafa395a2aa95bee0e86ff9fd3d54490a54f`，本地合并提交 `30d3274`
- QZS 来源：PR #12 `b3682a4dc824e460b7018cee9d09ef4b52b09a90`，本地合并提交 `60afcbd`
- libaoxun 来源：PR #13 `5bada18a4079053b0531772b2ab645492043e912`，本地合并提交 `8495859`
- `main` 未改，远端未推送。恢复后先实读 `git status --short --branch` 和 `CURRENT_STATE.md`。

## 已完成的集成裁决

1. 排除 WSC 根目录四份重复指南，只保留 `learning_guides/接口对齐与数据链路学习/`。
2. 圆柱/锥体保持 `WAIT`；任务二完整能力标记为 `BLOCKED`，只能超时或人工放弃。
3. runtime 终态改为 release-only idle-drain ACK：不分类、不重复提交结果、不产生动作；真实 I1 wire ABI 仍未冻结。
4. G1 与 R0 分成独立批次。R0 manifest 固定 `9F6F.../CD4C...`、八项输入 blob、COM12/COM17 和 CH340 `1A86:7523`。
5. PR #12 feature contract 的 Gate 文案已改为未来条件，避免误读成当前全部通过。
6. G2 runner 已修复为传播 C 测试失败码；直接 Host 脚本增加 VS2022 fallback。

## 已观察测试结果

- `single_camera_f1`：`213/213 PASS`
- `single_camera_feature_adapter`：`33/33 PASS`
- `single_camera_runtime`：`648/648 PASS`
- G2 bundle validation：`PASS`
- classifier 直接 Host 入口：`FAIL`，MSVC `/W4 /WX` 下 `test_single_camera_classifier.c` 常量宏断言触发 `C4127`，可执行文件未运行。
- PowerShell R0 capture 脚本：仅语法解析 `PASS`。
- 用户随后要求停止检测；presubmit、myCobot 完整非动作矩阵、freshness、context budget、最终 handoff health、脚本负例和 `git diff --check` 均为 `NOT RUN`。

## 硬件与安全状态

- 本次三方集成未改变 FPGA/Hard SoC 八项关键输入 blob，不因合并自动重跑 Efinity，不因合并自动使现有 Hello ELF 失效。
- R0 是唯一下一 Gate 活动批次。PR #13 文档报告 USER2、四 hart/PC 和 APB MAGIC；本轮没有重新取得原始外部日志，标记为 `REPORTED / NOT REVERIFIED`。
- UART0 在 CH340 COM12/COM17 的历史只读窗口均为 0 RX bytes，R0 仍阻塞。
- UART2/J52、myCobot 接线、帧和动作全部 `NO-GO`；本次没有执行任何硬件或机械臂动作。

## 三位队友拉取后的第一步

三位队友先在各自本机检出本集成分支，阅读：

1. `CURRENT_STATE.md`
2. `competition_project_single_camera/integration/F1_INTERFACE_ALIGNMENT_DRAFT.md`
3. `competition_project_single_camera/integration/F1_INTERFACE_CONFIRMATION_REGISTER.md`
4. `competition_project_single_camera/integration/single_camera_feature_contract.md`
5. `final_project/cpu/CPU_MODULE_PLAN.txt`

随后只决定接口文件、文件所有权、下一步任务分工和 Review Packet 方案。尚未完成三人确认前，不实现真实 MMIO、不接入 OSD、不执行 UART/机械臂硬件动作。

## 下一执行者必须保留的标注

- `FAIL`：classifier MSVC 严格测试入口。
- `BLOCKED`：任务二非正方体完整能力、R0 UART0 横幅。
- `NOT VERIFIED`：真实 I1/APB/CDC、CPU→OSD、正式 RISC-V 集成、板级逐轮事务、机械臂闭环。
- `NOT RUN`：用户叫停后的完整离线门禁与脚本负例。
