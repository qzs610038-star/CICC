# CURRENT_STATE — 三人本地集成候选状态

> 日期：2026-07-18（Asia/Shanghai）
> 正式 `main` 仍为 `9acf4d8b2ec788ccd5777f3833a7bfb756c51cad`；本文件描述独立分支 `codex/qzs-wsc-libaoxun-integration-20260718`。
> 真实源码、工程 XML、构建日志和板上现象是最终事实；稳定架构与安全红线见 [AGENTS.md](AGENTS.md)。

## CURRENT_SNAPSHOT

### 集成来源与范围

- WSC：`dev/claude-cpu-plan-status-0717@cbe6eafa395a2aa95bee0e86ff9fd3d54490a54f`，选择性合入提交 `30d3274`。
- QZS：PR #12 `b3682a4dc824e460b7018cee9d09ef4b52b09a90`，合入提交 `60afcbd`。
- libaoxun：PR #13 `5bada18a4079053b0531772b2ab645492043e912`，合入提交 `8495859`。
- WSC 根目录四份重复指南已排除；唯一规范版本位于 `learning_guides/接口对齐与数据链路学习/`。
- 当前结果只存在于独立本地集成分支；尚未合入 `main`，尚未推送。

### CPU / F1 语义裁决

- 圆柱体和锥体只保留为未标定诊断标签，不产生 `SHAPE_MISMATCH/SKIP`，保持 `WAIT`。
- `TASK2_FULL_CAPABILITY=BLOCKED`：混合形状池遇到非正方体时只能由超时或人工放弃结束；在固定摄像头实拍标定和混淆矩阵完成前，不得称为任务二完成。
- runtime 已实现终态 idle-drain：结果锁存后只对成功读取的同一 `frame_id` 做释放 ACK，更新帧序水位，不再分类、不产生第二个结果、不触发动作。
- 撕裂、配置版本错误或释放 ACK 失败继续 fail-closed。该逻辑仅为 Host seam；真实 I1/APB/CDC wire ABI 仍未冻结、未实现、未上板。
- `ARM_ENABLED=0`；UART2/J52、myCobot transport、接线和动作继续禁止。

### 离线测试状态

旧 `182/182`、`921/921` 均为历史计数，不得继承。用户已要求停止后续检测，最终状态如下：

| 项目 | 当前结果 |
|---|---|
| 单摄 F1 Host | `PASS 213/213` |
| feature adapter Host | `PASS 33/33` |
| runtime/G2 C Host | `PASS 648/648` |
| G2 bundle 格式校验 | `PASS`；runner 已修复为传播 C 测试失败码 |
| classifier 直接 Host 入口 | `FAIL`：MSVC `/W4 /WX` 下测试宏常量表达式触发 `C4127`，测试可执行文件未运行 |
| classifier/F1/adapter 原 GCC 入口 | 本机无 `gcc`；已增加 VS2022 fallback，其中 classifier 仍为上述 `FAIL` |
| `tools/offline_presubmit.ps1` | `NOT RUN`（用户要求停止检测） |
| myCobot 非动作 QEMU/Host 完整矩阵 | `NOT RUN`（用户要求停止检测） |
| freshness / context budget / 最终 handoff health | `NOT RUN`（用户要求停止检测） |
| PowerShell fail-closed 负例 / `git diff --check` | `NOT RUN`（用户要求停止检测） |

Host PASS 不等于 RISC-V ELF、真实 MMIO/APB、UART、OSD 或板级闭环。

### FPGA / Hard SoC 制品批次

本次集成没有修改 `mem_test.xml`、`mem_test.peri.xml`、`constrain.sdc`、`src/top.v`、`src/apb_reg_magic.v`、Hard SoC `settings.json`、UART0 Hello `build.ps1` 或 `src/main.c` 的 Git blob。因此本次合并本身不强制重跑 Efinity，也不自动使 Hello ELF 失效。

| 批次 | 制品 | 当前结论 |
|---|---|---|
| G1 历史离线批次 | bitstream `A897...1ACD` / ELF `E5BC...1928A` | 冷构建和离线操作包可追溯；不得与 R0 混用，不是当前下一 Gate 活动批次 |
| R0 唯一活动批次 | bitstream `9F6F...8F320` / ELF `CD4C...9411B` | 独立 manifest 已绑定八项输入 blob；PR #13 文档报告 USER2、四 hart/PC、APB MAGIC，但原始外部日志本轮未独立复核；UART0 COM12/COM17 均为 0 RX bytes，仍 `BLOCKED` |

- R0 manifest：[`r0_current_batch_manifest_20260718.json`](competition_project_single_camera/docs/debug_sessions/r0_current_batch_manifest_20260718.json)。
- 串口采集脚本只允许 `COM12`/`COM17`，并要求枚举身份为 CH340 `VID:PID 1A86:7523`、匹配 R0 manifest 和独立批准 JSON。
- 采集脚本仅完成 PowerShell 语法解析 `PASS`；实际 dry-run、错误 manifest/hash/端口/批准 JSON 负例均为 `NOT RUN`。

## CURRENT_BLOCKERS

1. classifier 严格 Host 入口仍为 `FAIL(C4127)`。
2. 任务二非正方体识别未实拍标定，完整能力为 `BLOCKED`。
3. idle-drain 仅 Host 验证；真实 I1 单槽、ACK、overrun、CDC 和 APB ABI 未冻结。
4. R0 UART0 横幅仍为 0 字节阻塞；PR #13 的 USER2/PC/APB 结论未由本轮直接读取原始外部日志复核。
5. CPU→OSD、正式目标输入、板级逐轮事务和 myCobot UART2/J52 均未形成当前批次证据。

## NEXT_GATE

1. 三位队友先在各自本机基于本集成分支决定 I1/I2/I3/I4 接口文件、下一步任务分工与实施方案；本步骤只做本地设计确认，不授权硬件动作。
2. 修复 classifier 的 MSVC `C4127` 严格测试入口，再重跑完整离线矩阵、presubmit、freshness、handoff 和脚本负例。
3. 接口语义确认后，由独立 F1 ABI Review Packet 冻结 I1 snapshot/ACK/flush、I2 配置和 I4 结果语义；禁止提前手填 APB 地址。
4. 若继续 R0，只能使用 R0 manifest 的匹配制品，并在用户确认的 CH340 COM12 或 COM17 上同步 CPU/SoC reset 做只读采集；本文件不授权执行该动作。
5. UART2/J52、机械臂接线、myCobot 帧和动作继续 `NO-GO`。

## PENDING_DECISIONS

- 三人对 I1/I2/I3/I4 的文件所有权、Review Packet 负责人和下一轮提交边界。
- 任务二圆柱/锥体真实标定方案与混淆矩阵验收阈值。
- idle-drain 在真实单槽 I1 中的 wire ACK/flush 编码和跨轮调度规则。
- R0 UART0 下一次只读排障的唯一端口、接线核验和 reset 时间同步方案。

## DEPRECATED_ROUTES

- WSC 根目录四份重复指南：不纳入集成结果。
- G1 与 R0 manifest/制品混选：禁止。
- 纯 FPGA 分类、PC/`pymycobot` 进入正式闭环、UART2/J52 提前接入：禁止。
