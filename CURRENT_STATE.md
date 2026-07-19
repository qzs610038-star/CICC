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
| qzs 范围检查 | 工具结果 `FAIL`：仅 3 个 `ppt_doc_outlines/**` 文件不在旧白名单；用户已明确授权本轮一起提交，记录为治理例外，不授权任何冻结接口/硬件/动作扩展 |
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
5. libaoxun 已确认其证据主机实际 clone 为 `C:\Users\20306\Desktop\赛题资料\CICC`，Git 顶层、origin 与 clean status 均通过，结论为 `READY_PATH`；这只关闭路径阻塞，集合分支同步、原始制品预检和全部板级项仍未执行。
6. WSC 已确认 clone `D:\CICC w` 与 origin 身份，但原 `dev/claude-cpu-plan-status-0717` 含 tracked 修改 `final_project/cpu/CPU_MODULE_PLAN.txt`，已在 fetch/switch 前安全停止。该修改必须保留；WSC 后续改用 detached 固定 SHA 独立 worktree，不在原 dirty 工作树切分支。

## NEXT_GATE

1. libaoxun 已完成 Stage 0；下一步只在已确认的 `$RepoRoot` 上精确 fetch 集合 ref、ff-only 切换，并提交固定 SHA 与原始制品只读预检报告。WSC 已完成路径确认，但须保留原 dirty，改在新建 detached 固定 SHA worktree 中审查。
2. libaoxun 与 wsc 分别安全拉取并独立提交 READY/BLOCKED；两人 HEAD 必须为 `e72fb6a3ce1b7999f61cae1e0ed7b2773f1e4fda`。
3. qzs 比对两份 READY、固定板卡/制品/接线/停止责任与窗口，再请求用户明确批准；批准前不执行任何硬件动作。
4. 批准后由 libaoxun 作为唯一上板执行者，在 wsc CPU 判读支持下连续完成 USER2 → UART1 Hello/回显 → APB MAGIC；中间不重复确认，不回退 UART0。
5. I0-SMOKE PASS 后进入受审 I1-I4；UART2/J52、机械臂接线、myCobot 帧和动作继续独立 `NO-GO`。

## PENDING_DECISIONS

- 任务二圆柱/锥体真实标定方案与混淆矩阵验收阈值。
- idle-drain 在真实单槽 I1 中的 wire ACK/flush 编码和跨轮调度规则。
- I0-SMOKE 的实际 Type-C UART1 枚举端口、板卡/制品批准窗口和失败停止责任人。
- libaoxun 的板卡/原始制品 READY 报告与 WSC 的 CPU/Hello READY 报告尚未返回；WSC 需先完成 dirty 隔离 worktree。

## DEPRECATED_ROUTES

- WSC 根目录四份重复指南：不纳入集成结果。
- 原双摄视频/识别路线：取消，只作历史资料，不得恢复。
- G1/R0 UART0 manifest、制品、COM口、操作卡和 0-byte 结论：历史保留，不得改名或继承到 UART1。
- 纯 FPGA 分类、PC/`pymycobot` 进入正式闭环、UART2/J52 提前接入：禁止。

## HISTORY_ARCHIVE_INDEX

- 历史状态总索引：`debug_records/state_history/archive_manifest.md`。
- UART0/R0 专项索引：`competition_project_single_camera/docs/debug_sessions/UART0_R0_HISTORICAL_INDEX_20260718.md`。

## POST_MERGE_REFRESH_REQUIRED

2026-07-19 已按 `libaoxun 72cc281 → WSC 13419d9 → QZS 018ced2` 的固定输入顺序形成集合树并刷新本文件。Goal 3 离线总门为 `PASS_WITH_WARNINGS`；qzs 范围工具的 3 项 PPT violation 由用户显式授权形成治理例外。待提交后静态门与远端 SHA 核对完成才可交给下一步；本状态仍不是正式 `main` 或板级 PASS。
