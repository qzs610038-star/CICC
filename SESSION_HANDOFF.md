# SESSION HANDOFF — 2026-07-18 单摄接口冻结与三人分工确认

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
7. 用户与三人确认单摄为唯一正式视频/识别路线，原双摄方案取消。
8. I0 固定为 SoC UART1 → 板载 Type-C UART1，RX=`GPIOR_96/B12`、TX=`GPIOR_100/D12`、`115200 8N1`；UART0/R0 只保留为历史证据。
9. 三人文件所有权与精简 Gate 已冻结，见 `docs/agent_context/TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md`。

## 已观察测试结果

- `single_camera_f1`：`213/213 PASS`
- `single_camera_feature_adapter`：`33/33 PASS`
- `single_camera_runtime`：`648/648 PASS`
- G2 bundle validation：`PASS`
- classifier 直接 Host 入口：`FAIL`，MSVC `/W4 /WX` 下 `test_single_camera_classifier.c` 常量宏断言触发 `C4127`，可执行文件未运行。
- PowerShell R0 capture 脚本：仅语法解析 `PASS`。
- 本次接口治理后已补跑离线门禁：`offline_presubmit=PASS_WITH_WARNINGS`（exit 0），接口冻结、context budget、handoff health、G2 bundle/runtime、QEMU skeleton 和 `git diff --check` 均 PASS；freshness 为 7 WARN/0 FAIL。完整 myCobot 非动作矩阵与 PowerShell fail-closed 负例未单独重跑。

## 硬件与安全状态

- 本次三方集成未改变 FPGA/Hard SoC 八项关键输入 blob，不因合并自动重跑 Efinity，不因合并自动使现有 Hello ELF 失效。
- R0/UART0 已降级为历史批次，不再作为下一 Gate；其 bitstream/ELF、COM12/COM17 和 0-byte 记录不得继承到 UART1。
- 当前 Hard SoC 仍只有 UART0：`PERI_UART_1=0`，wrapper/`.peri.xml`/`soc.h` 均未生成 UART1。I0 UART1 为 `ROUTE FROZEN / IMPLEMENTATION PENDING`。
- UART2/J52、myCobot 接线、帧和动作全部 `NO-GO`；本次没有执行任何硬件或机械臂动作。

## 三位队友拉取后的第一步

三位队友先在各自本机检出本集成分支，阅读：

1. `CURRENT_STATE.md`
2. `competition_project_single_camera/integration/F1_INTERFACE_ALIGNMENT_DRAFT.md`
3. `competition_project_single_camera/integration/F1_INTERFACE_CONFIRMATION_REGISTER.md`
4. `competition_project_single_camera/integration/single_camera_feature_contract.md`
5. `final_project/cpu/CPU_MODULE_PLAN.txt`

三人确认已经完成。下一步按所有权表直接执行：libaoxun 生成 UART1 原子硬件批次，wsc 修复离线 FAIL并在新 `soc.h` 后构建 UART1 Hello，qzs 维护冻结/范围检查和最终证据。固定同一输入 hash 后，USER2、UART1 Hello 与 APB MAGIC 使用一次连续批准，不重复确认。

## 下一执行者必须保留的标注

- `FAIL`：classifier MSVC 严格测试入口。
- `BLOCKED`：任务二非正方体完整能力、I0 UART1 生成与板级链路。
- `NOT VERIFIED`：真实 I1/APB/CDC、CPU→OSD、正式 RISC-V 集成、板级逐轮事务、机械臂闭环。
- `NOT RUN`：完整 myCobot 非动作矩阵与 PowerShell fail-closed 负例；不影响本次接口/分工冻结 PASS。
