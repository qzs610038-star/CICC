# 单摄接口冻结与三人最终日文件所有权

> 状态：`TEAM CONFIRMED / ACTIVE`
> 基线：`codex/qzs-wsc-libaoxun-integration-20260718@0c7530f`
> 决策日期：2026-07-18；执行日：2026-07-19

## 1. 已确认决策

1. 正式视频/识别路线固定为 `competition_project_single_camera/` 单摄 J48/ch0；原双摄方案取消，只保留历史资料。
2. I0 固定使用 SoC UART1 → 板载 Type-C UART1，`115200 8N1`，RX=`GPIOR_96/B12`、TX=`GPIOR_100/D12`。
3. UART0/R0 退出活动路线，只作历史证据；不得改写历史日志或继承旧 PASS。
4. 三人已确认接口语义、文件范围和合并顺序。
5. 同一输入 hash 的非动作 Gate 合并为一次连续链；机械臂 Gate 不放宽。

## 2. 冻结接口文件

以下文件为冻结真源：

- `competition_project_single_camera/integration/I0_UART1_INTERFACE_FREEZE.md`
- `competition_project_single_camera/integration/F1_INTERFACE_ALIGNMENT_DRAFT.md`
- `competition_project_single_camera/integration/F1_INTERFACE_CONFIRMATION_REGISTER.md`
- `competition_project_single_camera/integration/single_camera_feature_contract.md`
- `competition_project_single_camera/cpu/include/single_camera_classifier.h`
- `competition_project_single_camera/cpu/include/single_camera_feature_adapter.h`
- `competition_project_single_camera/cpu/include/single_camera_f1.h`
- `competition_project_single_camera/cpu/include/single_camera_runtime.h`

`src/feature_stats/feature_stats_tap.v` 的模块参数、端口、字段宽度和 flag 编码也属于
冻结接口面。内部 RTL 只能由 libaoxun 在不改变该接口面的前提下修改。

冻结接口修改必须有用户完整口令：

```text
确认接口文件修改，已经和wsc、libaoxun、qzs沟通。
```

## 3. 唯一写入范围

| 负责人 | 允许写入 | 当前任务 | 禁止写入 |
|---|---|---|---|
| wsc | `competition_project_single_camera/cpu/src/**`、`cpu/tests/**`、`cpu/README.md`、新 `cpu_bringup/uart1_hello_onchip/**` | 修复 classifier `C4127`；重跑 Host；在新 `soc.h` 到位后构建 UART1 Hello 与 CPU MMIO | 冻结头文件、RTL/XML/SDC/IP、状态/治理文档、myCobot |
| libaoxun | `competition_project_single_camera/src/**`、`tests/rtl/**`、`mem_test.xml`、`mem_test.peri.xml`、`constrain.sdc`、Hard SoC IP/BSP 原子树 | 用 Efinity 正式生成 UART1；完成 I0-BUILD；之后实现受审 I1/I2/I4 硬件侧 | CPU 业务代码、qzs 工具/状态文档、冻结接口语义、机械臂 |
| qzs | `.gitattributes`、`.gitignore`、`AGENTS.md`、`CLAUDE.md`、`CURRENT_STATE.md`、`SESSION_HANDOFF.md`、`.agents/skills/**`、`docs/**`、`final_project/docs/**`、`competition_project_single_camera/{README.md,docs/**,integration/**,tools/**}`、根 `tools/**`、`learning_guides/**`、`ppt_doc_outlines/**` | 维护冻结清单、Gate、证据、范围检查、最终集成 manifest 和技术报告/PPT索引 | CPU/RTL/Hard SoC 实现；无口令修改冻结接口；机械臂动作 |

任何文件不在对应允许范围内即视为越界。需要跨范围时，先由 qzs 在 Review Packet
登记临时所有权、固定 SHA 和回收时间，不能直接“帮队友顺手改”。

`cpu_bringup/uart1_hello_onchip/README.md` 是本次接口搭建的一次性空骨架例外；从本基线起，该目录全部归 wsc。冻结 `integration/**` 虽在 qzs 治理范围内，也只能在用户给出完整口令后更新 manifest。

### 3.1 最终集成归属与来源保真

qzs 接管的是最终集成层的 manifest、静态 Gate 转录和治理文档；其新
`competition_project_single_camera/integration/FINAL_STATIC_INTEGRATION_MANIFEST_20260719.json`
只读取上游固定 SHA 的文件，不复制或重提交上游源码。它不改变以下唯一来源归属：

- libaoxun 保有 `embedded_sw/uart1_hello_onchip/**` 的构建 manifest、输入清单、重建与
  verifier；qzs 只把它们作为只读、可哈希的集成输入。
- wsc 保有 `cpu/src/**`、`cpu/tests/**` 及其 CPU probe；WSC probe 源码与其 Review Packet
  必须标注 `dev/wsc6090-uart1-cpu-20260719@13419d9922f3f8e7585bd43b77491b81b4bc0681`，
  不得由 libaoxun 复制同名文件后重新声明来源。
- qzs 保有 I0 operation card、接口冻结检查、team-scope Gate、最终 manifest/Packet 与
  `CURRENT_STATE.md`/`SESSION_HANDOFF.md`。这些是治理产物，不是 CPU 或 Hard SoC 实现。

### 3.2 Goal 4 硬件执行单主机例外（2026-07-20）

用户已将 Goal 4 的 live preflight、volatile JTAG、USER2/PC、UART1 与 APB 只读链，以及原始 evidence bundle 的唯一结论，完整交给 libaoxun 证据主机。qzs 的 operation card/状态文档所有权不等于硬件签发权：qzs 不再要求 READY、签发窗口、实时判读或作为执行前置；若收到完整原始证据，只能事后归档。该例外不修改冻结接口、CPU/RTL/Hard SoC 文件归属，也不放开 UART2/J52、myCobot 或持久化烧录。

最终集成审查不得把全量三方差分交给单一角色判定。`team_scope_check.ps1` 必须用
`-BaseRef` 和 `-TargetRef` 按固定来源分别运行：libaoxun
`f47af29..72cc281`、wsc `f47af29..13419d9`，以及 qzs 的独立治理片段
`0c7530f..018ced2`、`f10cbd3..bb34856` 与最终 qzs 补丁。若新增跨范围文件，先更新
本表和 Packet；不得以“治理例外”掩盖。

## 4. 当前完成/未完成/需重测分配

### wsc

- 已完成：F1/adapter/runtime Host 语义、终态 release-only ACK、`ARM_ENABLED=0`。
- 需修复：`test_single_camera_classifier.c` 的 MSVC `/W4 /WX` `C4127`。
- 需重测：classifier、F1、adapter、runtime/G2 bundle。
- 待硬件输入：新 UART1 `soc.h` 后新增片上 RAM Hello；不得沿用 UART0 基址。

### libaoxun

- 已完成：旧 R0 离线冷构建与原子树可追溯；feature tap 源码/testbench 存在但未连接。
- 当前首要：Efinity 启用 SoC UART1并路由 Type-C UART1，产生全新批次。
- 需重测：Map/Interface/PNR/STA/CDC、HDMI ch0 回归、bitstream/ELF身份。
- 禁止：手改生成 wrapper/`soc.h`，或混入旧 R0 制品。

### qzs

- 已完成：三方集成裁决、接口口令和文件范围确认。
- 已重测：`offline_presubmit=PASS_WITH_WARNINGS`、freshness 0 FAIL、handoff、`git diff --check`、qzs 范围和接口冻结均 PASS；合并两位队友固定 SHA 后再跑一次最终检查。
- 当前首要：新批次证据索引、一次性 I0-SMOKE 操作卡、最终状态和报告/PPT材料索引。
- 禁止：把 Host/旧 UART0/R0 写成 UART1 板级 PASS。

## 5. 最终日顺序与停止条件

1. wsc 修复离线 FAIL；libaoxun 并行生成 UART1 批次；qzs 建证据/检查骨架。
2. 固定新 bitstream/ELF/输入 hash 后，由一次批准连续完成 USER2、UART1 Hello/echo、APB MAGIC。
3. I0-SMOKE PASS 后直接推进受审 I1-I4；不重复确认已固定事实。
4. 合并顺序：libaoxun 固定 SHA → wsc 固定 SHA → qzs 最终状态/证据刷新。
5. 若 UART1 生成或 I0-SMOKE 失败，只处理该故障；不得回退 UART0、恢复双摄、猜 MMIO 或接 UART2。

## 6. 报告/PPT接口

qzs 只从以下证据生成报告结论：当前 `CURRENT_STATE.md`、匹配 Review Packet、同批
构建摘要、原始 UART1/APB/视频日志和三人固定 SHA。任何 `NOT VERIFIED` 项必须在
报告/PPT中保持同一状态，不用设计目标代替结果。
