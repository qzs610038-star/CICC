# CURRENT_STATE — 单摄接口冻结与三人最终日状态

> 日期：2026-07-18（Asia/Shanghai）
> 正式 `main` 仍为 `9acf4d8b2ec788ccd5777f3833a7bfb756c51cad`；本文件描述独立分支 `codex/qzs-wsc-libaoxun-integration-20260718`。
> 真实源码、工程 XML、构建日志和板上现象是最终事实；稳定架构与安全红线见 [AGENTS.md](AGENTS.md)。

- 证据索引：`competition_project_single_camera/integration/interface_freeze_manifest.json`
- 证据路径：`competition_project_single_camera/integration/I0_UART1_INTERFACE_FREEZE.md`
- 证据路径：`docs/agent_context/TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md`

## CURRENT_SNAPSHOT

### 集成来源与范围

- WSC：`dev/claude-cpu-plan-status-0717@cbe6eafa395a2aa95bee0e86ff9fd3d54490a54f`，选择性合入提交 `30d3274`。
- QZS：PR #12 `b3682a4dc824e460b7018cee9d09ef4b52b09a90`，合入提交 `60afcbd`。
- libaoxun：PR #13 `5bada18a4079053b0531772b2ab645492043e912`，合入提交 `8495859`。
- WSC 根目录四份重复指南已排除；唯一规范版本位于 `learning_guides/接口对齐与数据链路学习/`。
- 当前结果只存在于独立本地集成分支；尚未合入 `main`，尚未推送。
- 2026-07-18 用户与三位协作者确认：正式视频/识别路线固定为单摄 J48/ch0，原双摄方案取消；三人文件范围已冻结。
- I0 固定为 SoC UART1 → 板载 Type-C UART1，`115200 8N1`，RX=`GPIOR_96/B12`、TX=`GPIOR_100/D12`。UART0/R0 退出活动路线，仅保留历史证据。
- I0 UART1 板级状态仍为 `NOT VERIFIED`；片上 RAM 地址继续以同批 `soc.h` 核对，预期启动区域为 `0xF9000000`，不得凭旧批次外推。

### CPU / F1 语义裁决

- 圆柱体和锥体只保留为未标定诊断标签，不产生 `SHAPE_MISMATCH/SKIP`，保持 `WAIT`。
- `TASK2_FULL_CAPABILITY=BLOCKED`：混合形状池遇到非正方体时只能由超时或人工放弃结束；在固定摄像头实拍标定和混淆矩阵完成前，不得称为任务二完成。
- runtime 已实现终态 idle-drain：结果锁存后只对成功读取的同一 `frame_id` 做释放 ACK，更新帧序水位，不再分类、不产生第二个结果、不触发动作。
- 撕裂、配置版本错误或释放 ACK 失败继续 fail-closed。该逻辑仅为 Host seam；真实 I1/APB/CDC wire ABI 仍未冻结、未实现、未上板。
- `ARM_ENABLED=0`；UART2/J52、myCobot transport、接线和动作继续禁止。
- myCobot 正式速率仍为 `1000000`，与 I0 UART1 `115200` 完全独立；本次 Gate 放宽不适用于机械臂。

### 离线测试状态

旧 `182/182`、`921/921` 均为历史计数，不得继承。2026-07-18 本次接口治理后的状态如下：

| 项目 | 当前结果 |
|---|---|
| 单摄 F1 Host | `PASS 213/213` |
| feature adapter Host | `PASS 33/33` |
| runtime/G2 C Host | `PASS 648/648` |
| G2 bundle 格式校验 | `PASS`；runner 已修复为传播 C 测试失败码 |
| classifier 直接 Host 入口 | `FAIL`：MSVC `/W4 /WX` 下测试宏常量表达式触发 `C4127`，测试可执行文件未运行 |
| classifier/F1/adapter 原 GCC 入口 | 本机无 `gcc`；已增加 VS2022 fallback，其中 classifier 仍为上述 `FAIL` |
| `tools/offline_presubmit.ps1` | `PASS_WITH_WARNINGS`；沙箱外真实运行 exit 0，接口冻结、G2 bundle/runtime、QEMU、空白与 diff 检查均 PASS |
| myCobot 非动作 QEMU skeleton | `PASS`；仅断言执行，不授权 UART2/J52、接线或动作；完整矩阵仍未重跑 |
| freshness / context budget / handoff health | `PASS`；freshness 有 7 个既有/dirty/stale/CBM WARN，无 FAIL |
| 接口冻结 / qzs 范围 / `git diff --check` | `PASS`；wsc、libaoxun 白名单已静态加载；各自分支仍须单独执行 |
| PowerShell fail-closed 负例 | 本次未单独重跑 |

Host PASS 不等于 RISC-V ELF、真实 MMIO/APB、UART、OSD 或板级闭环。

### FPGA / Hard SoC 制品批次

当前生成 Hard SoC 仍是 UART0 配置：`PERI_UART_0=1`、`PERI_UART_1=0`，wrapper、`.peri.xml` 和 `soc.h` 只包含 UART0。因此 I0 UART1 是已冻结目标接口，不是已实现接口。启用 UART1 将改变 IP/wrapper、`.peri.xml`、BSP/`soc.h`、bitstream 和 Hello ELF，必须建立全新原子批次。

| 批次 | 制品 | 当前结论 |
|---|---|---|
| G1 历史离线批次 | bitstream `A897...1ACD` / ELF `E5BC...1928A` | `HISTORICAL`；不得继承到 UART1 |
| R0 UART0 历史批次 | bitstream `9F6F...8F320` / ELF `CD4C...9411B` | `HISTORICAL / SUPERSEDED`；原始结论与 0-byte UART0 记录保留，不再排障 |
| I0-UART1 新批次 | 尚未生成 | `NOT STARTED`；等待 libaoxun Efinity 原子生成与 wsc UART1 Hello |

- R0/UART0 历史索引：[`UART0_R0_HISTORICAL_INDEX_20260718.md`](competition_project_single_camera/docs/debug_sessions/UART0_R0_HISTORICAL_INDEX_20260718.md)。
- I0 UART1 冻结页：[`I0_UART1_INTERFACE_FREEZE.md`](competition_project_single_camera/integration/I0_UART1_INTERFACE_FREEZE.md)。
- 禁止手改生成 wrapper/`soc.h` 或复用 R0 COM口、批准 JSON、bitstream、ELF 和采集结论。

## CURRENT_BLOCKERS

1. classifier 严格 Host 入口仍为 `FAIL(C4127)`。
2. 任务二非正方体识别未实拍标定，完整能力为 `BLOCKED`。
3. idle-drain 仅 Host 验证；真实 I1 单槽、ACK、overrun、CDC 和 APB ABI 未冻结。
4. I0 UART1 的 Hard SoC/IP/`.peri.xml`/BSP/`soc.h`/Hello 尚未生成；当前无可用于 UART1 的匹配 bitstream/ELF。
5. CPU→OSD、正式目标输入、板级逐轮事务和 myCobot UART2/J52 均未形成当前批次证据。

## NEXT_GATE

1. wsc 修复 classifier 的 MSVC `C4127` 严格入口并重跑 classifier/F1/adapter/runtime；qzs 已完成本基线离线、freshness、handoff、冻结/范围检查，并在两位队友固定 SHA 合并后再执行一次最终刷新。
2. libaoxun 在 Efinity 中启用 SoC UART1、路由 Type-C `GPIOR_96/GPIOR_100`，原子生成 IP/wrapper/`.peri.xml`/BSP/`soc.h`，再冷构建新 bitstream。
3. wsc 仅依据新 `SYSTEM_UART_1_*` 宏构建片上 RAM UART1 Hello ELF；禁止猜基址。
4. 固定新输入与制品 hash 后，在一次批准窗口内连续完成 USER2 → UART1 Hello/回显 → APB MAGIC；中间不重复确认。
5. I0-SMOKE PASS 后直接进入受审 I1-I4；UART2/J52、机械臂接线、myCobot 帧和动作继续独立 `NO-GO`。

## PENDING_DECISIONS

- 任务二圆柱/锥体真实标定方案与混淆矩阵验收阈值。
- idle-drain 在真实单槽 I1 中的 wire ACK/flush 编码和跨轮调度规则。
- 新 UART1 原子批次生成后的实际 `soc.h` 基址/IRQ、匹配制品 hash 和 Type-C 枚举端口。

## DEPRECATED_ROUTES

- WSC 根目录四份重复指南：不纳入集成结果。
- 原双摄视频/识别路线：取消，只作历史资料，不得恢复。
- G1/R0 UART0 manifest、制品、COM口、操作卡和 0-byte 结论：历史保留，不得改名或继承到 UART1。
- 纯 FPGA 分类、PC/`pymycobot` 进入正式闭环、UART2/J52 提前接入：禁止。

## HISTORY_ARCHIVE_INDEX

- 历史状态总索引：`debug_records/state_history/archive_manifest.md`。
- UART0/R0 专项索引：`competition_project_single_camera/docs/debug_sessions/UART0_R0_HISTORICAL_INDEX_20260718.md`。

## POST_MERGE_REFRESH_REQUIRED

当前裁决仍只在独立本地集成分支。合并 libaoxun、wsc 固定 SHA 后，qzs 必须最后刷新本文件、接口 manifest、证据索引和 freshness；合并前状态不得写成正式 `main` 结论。
