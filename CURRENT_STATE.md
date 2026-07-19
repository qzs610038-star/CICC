# CURRENT_STATE — 单摄接口冻结与三人最终日状态

> 日期：2026-07-19（Asia/Shanghai）
> 正式 `main` 仍为 `9acf4d8b2ec788ccd5777f3833a7bfb756c51cad`；本文件描述独立分支 `codex/qzs-wsc-libaoxun-integration-20260718`。
> 真实源码、工程 XML、构建日志和板上现象是最终事实；稳定架构与安全红线见 [AGENTS.md](AGENTS.md)。

- 证据索引：`competition_project_single_camera/integration/interface_freeze_manifest.json`
- 证据路径：`competition_project_single_camera/integration/I0_UART1_INTERFACE_FREEZE.md`
- 证据路径：`docs/agent_context/TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md`

## CURRENT_SNAPSHOT

### 集成来源与范围

- WSC：`dev/wsc6090-uart1-cpu-20260719@13419d9922f3f8e7585bd43b77491b81b4bc0681`；集合分支包含其相对 `f47af29` 的 6 个 Host 修复提交。
- libaoxun：`dev/libaoxun688-uart1-i0-20260719-cleanlf-final@72cc281bd104726d9db1e88cb2894facb1d5fd1a`；以 merge commit `f10cbd3` 原子合入 UART1 Hard SoC、Hello 与构建证据。
- QZS：`codex/qzs-final-integration-goals-20260718@018ced2a6e7b96c8e1fef85ea6c15d4c1fa77a23`；以 merge commit `03f9750` 合入 Goal 控制面、审查记录和报告草稿。
- Goal 4 WSC 合同：`dev/wsc6090-goal4-contract-after-qzs-20260719@48548f47dfa5964b13aed7edf3b3e9da6f6583a2`；直接基于 `182fd6f`，四个临时授权文件，fail-closed Host 合同与 G2/classifier 静态复核 PASS。
- Goal 4 libaoxun 快照：`dev/libaoxun688-goal4-static-20260719@2d713b80a41185e472837abaec3a10c01383c70f`；直接基于 `182fd6f`，仅接受为 `BLOCKER_SNAPSHOT`，未合入集合分支。
- WSC 根目录四份重复指南已排除；唯一规范版本位于 `learning_guides/接口对齐与数据链路学习/`。
- 当前结果只属于独立集合分支；该分支可推送到同名远端供协作，但尚未合入正式 `main`。
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

旧 `182/182`、`921/921` 均为历史计数，不得继承。2026-07-19 在合并后的真实工作树重新执行如下：

| 项目 | 当前结果 |
|---|---|
| 单摄 F1 Host | `PASS 213/213` |
| feature adapter Host | `PASS 33/33` |
| runtime/G2 C Host | `PASS 648/648` |
| G2 bundle 格式校验 | `PASS`；runner 已修复为传播 C 测试失败码 |
| classifier 直接 Host 入口 | `PASS 54/54`；VS2022 `/W4 /WX` 编译并实际运行，原 `C4127` 阻断已关闭 |
| classifier/F1/adapter/runtime 编译入口 | 本机实际使用 VS2022；四项 runner 均 exit 0，未用降低 warning 或跳过可执行文件伪装 PASS |
| `tools/offline_presubmit.ps1` | `PASS_WITH_WARNINGS`，exit 0；沙箱内首次运行因系统 TEMP 写权限失败，获准在真实环境用同一命令重跑后全部功能检查 PASS |
| myCobot 非动作 QEMU skeleton | `PASS`；仅断言执行，不授权 UART2/J52、接线或动作；完整矩阵仍未重跑 |
| freshness / context budget / handoff health | `PASS`；freshness `WARN=8 / FAIL=0`，context budget 与 handoff health PASS |
| 接口冻结 / `git diff --check` | `PASS`；接口 route=`UART1_TYPEC`，当前工作区无空白错误 |
| qzs 范围检查 | `PASS`：工具现支持 `-BaseRef/-TargetRef`，按 libaoxun、wsc 与 qzs 固定来源分别审查；`.gitattributes`、`.gitignore` 与 `ppt_doc_outlines/**` 已重冻结为 qzs 治理范围，不再以“例外”掩盖 |
| PowerShell fail-closed 负例 | 本次未单独重跑 |

Host PASS 不等于 RISC-V ELF、真实 MMIO/APB、UART、OSD 或板级闭环。

### FPGA / Hard SoC 制品批次

当前集合树已经合入 UART1 原子批次：`PERI_UART_0=0`、`PERI_UART_1=1`、`PERI_UART_2=0`，`.peri.xml` 将 RX/TX 分别绑定 `GPIOR_96/GPIOR_100`，同批 `soc.h` 定义 UART1 `0xe8011000` 与 `115200`。这只证明固定源码与离线构建身份，尚未证明板上 CPU 取指、UART1 Hello/回显或 APB MAGIC。

| 批次 | 制品 | 当前结论 |
|---|---|---|
| G1 历史离线批次 | bitstream `A897...1ACD` / ELF `E5BC...1928A` | `HISTORICAL`；不得继承到 UART1 |
| R0 UART0 历史批次 | bitstream `9F6F...8F320` / ELF `CD4C...9411B` | `HISTORICAL / SUPERSEDED`；原始结论与 0-byte UART0 记录保留，不再排障 |
| I0-UART1 `I0_UART1_20260719_CLEAN_LF_FINAL` | bitstream `D05E...C544` / Hello ELF `919B...F7FA` | `I0-BUILD APPROVE`；82 项输入、21 项制品由 manifest 绑定；USER2/UART1/APB 仍 `NOT VERIFIED` |

- R0/UART0 历史索引：[`UART0_R0_HISTORICAL_INDEX_20260718.md`](competition_project_single_camera/docs/debug_sessions/UART0_R0_HISTORICAL_INDEX_20260718.md)。
- I0 UART1 冻结页：[`I0_UART1_INTERFACE_FREEZE.md`](competition_project_single_camera/integration/I0_UART1_INTERFACE_FREEZE.md)。
- 禁止手改生成 wrapper/`soc.h` 或复用 R0 COM口、批准 JSON、bitstream、ELF 和采集结论。

## CURRENT_BLOCKERS

1. 任务二非正方体识别未实拍标定，完整能力为 `BLOCKED`。
2. idle-drain 仅 Host 验证；真实 I1 单槽、ACK、overrun、CDC 和 APB ABI 未冻结。
3. I0 UART1 只有同批离线构建与制品身份；USER2、板上 CPU 取指、Type-C UART1 Hello/回显和 APB MAGIC 尚未执行。
4. CPU→OSD、正式目标输入、板级逐轮事务和 myCobot UART2/J52 均未形成当前批次证据。
5. libaoxun `2d713b80` 的 OpenOCD 参数语义错误：`use_bscan_tunnel 6 1` 中 `6` 是 DM TAP IR width、`1` 是 tunnel type；`set_bscan_tunnel_ir` 应消费 Titanium USER2 外层 IR `0x09`，当前却传入 `8`。因此 `USER2_SELECTION_CHAIN=BLOCKED`。
6. UART capture 只消费 `RESUME_ONCE` marker，没有受控生产者；APB GDB 只打印 `1000 ms` marker，没有 host timer、timeout 主动 halt 或 halt-reason Gate。唯一 host orchestrator 尚未实现。
7. libaoxun 原始 build verifier 缺 `efinity_console.log`、四份 Hello/ELF 文本证据及 pre/postflight 共六个 evidence-root 文件，实际 exit 1；`82/21` 仅为 manifest 计数，不能写成 PASS。
8. qzs 旧 final manifest 将三个 PS1 的 LF 工作树字节写入 checkout SHA，但 `.gitattributes` 要求 CRLF；fresh worktree 会正确签出 CRLF并导致 hash mismatch。生成器/verifier 正在升级为实际字节 EOL 检查。

## NEXT_GATE

1. qzs 完成 actual-byte EOL verifier、fresh-worktree manifest 重生成，并补充授权唯一 `run_i0_uart1_execution_chain.ps1`；该授权仍只覆盖离线实现/mock。
2. libaoxun 在 `2d713b80` 后追加最小补丁：修正 `width=6/type=1/outer_ir=0x09`，实现 `CAPTURE_READY → resume marker` 与 APB timeout/halt/reason/PC host runner，将 runner/verifier 纳入 manifest。
3. 在原始证据主机恢复六个固定 hash evidence 文件并复跑总 verifier；若无法恢复，关闭当前 batch 并建立新批次，不得伪造旧日志。
4. WSC 只读复核 host runner 对 `48548f47` 合同的消费；qzs 再按两个独立 SHA 做最终集成、范围/EOL/manifest 审查。
5. 仅当三角色静态目标全 PASS，qzs 才可给出 `VERDICT=READY_FOR_NEW_WINDOW_REQUEST`；该结论不等于硬件授权。
6. 用户之后仍须明确批准，才可由 libaoxun 连续执行 USER2 → UART1 Hello/回显 → APB MAGIC；UART2/J52、机械臂继续独立 `NO-GO`。

## PENDING_DECISIONS

- 任务二圆柱/锥体真实标定方案与混淆矩阵验收阈值。
- idle-drain 在真实单槽 I1 中的 wire ACK/flush 编码和跨轮调度规则。
- I0-SMOKE 的实际 Type-C UART1 枚举端口、板卡/制品批准窗口和失败停止责任人。
- WSC `48548f47` 已返回并通过静态合同复核；libaoxun `2d713b80` 仅为 blocker snapshot，等待 USER2/host runner/82-21 evidence 三项关闭。

## DEPRECATED_ROUTES

- WSC 根目录四份重复指南：不纳入集成结果。
- 原双摄视频/识别路线：取消，只作历史资料，不得恢复。
- G1/R0 UART0 manifest、制品、COM口、操作卡和 0-byte 结论：历史保留，不得改名或继承到 UART1。
- 纯 FPGA 分类、PC/`pymycobot` 进入正式闭环、UART2/J52 提前接入：禁止。

## HISTORY_ARCHIVE_INDEX

- 历史状态总索引：`debug_records/state_history/archive_manifest.md`。
- UART0/R0 专项索引：`competition_project_single_camera/docs/debug_sessions/UART0_R0_HISTORICAL_INDEX_20260718.md`。

## POST_MERGE_REFRESH_REQUIRED

2026-07-19 Goal 4 follow-up 已固定 WSC `48548f47` 与 libaoxun `2d713b80`。WSC 合同通过；libaoxun 快照主动保持 execution verifier exit 2。Codex 反向审查进一步确认 USER2 OpenOCD 参数误解、缺 host orchestrator、六个原始 evidence 文件缺失，以及 qzs 旧 final manifest 的 LF/CRLF 可复现性问题。qzs 只推进治理/EOL 修复，不合入该 blocker snapshot，不申请硬件窗口。

## FINAL_GATE_STATUS

```text
GOAL4=BLOCKED_EXECUTION_TOOLCHAIN
USER2=NOT_VERIFIED
PC=NOT_VERIFIED
UART1_HELLO_ECHO=NOT_VERIFIED
APB_MAGIC=NOT_VERIFIED
I3=BLOCKED_CONTRACT_NOT_FROZEN
```
