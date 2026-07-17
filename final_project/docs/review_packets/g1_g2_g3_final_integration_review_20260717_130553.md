# G1/G2/G3 最终整合 Review Packet

## 结论与隔离边界

- 工程证据输入基线：`489ab5b0c1773bfcb776cedd9f39b3f088fb4a0f`。
- 已验证 G1/G2/G3 内容提交：`683814edb46f0185fe61df5e6829ce2862fccca4`。G1/G2 源分支没有独立新增 commit，其成果通过核验后随该 G3 提交纳入。
- 组合工作树：`D:\CICC-g3-context-slim`，分支 `codex/g3-context-slim`。
- 合并到本地 `main` 后，以 Git 实读 HEAD 和本 Packet 的最终验证记录为准；merge commit 不得被误写为新的硬件证据批次。
- 未 fetch、未 push、未删除任何 worktree，也未执行下载、烧录、串口写入或机械臂动作。

## G1 探测与纳入

- 源 worktree：`D:\CICC-g1-efinity`；分支 `codex/g1-efinity-evidence`；dirty 仅为 G1 Review Packet。
- 纳入文件：`competition_project_single_camera/docs/review_packets/G1_EFINITY_COLD_BUILD_REVIEW_PACKET_20260717_0307.md`。
- 有效离线批次：`D:\CICC-runs\g1-efinity-evidence\20260717_025725`。
- Efinity 2025.2 的 Map、Interface、PNR、STA、CDC 和 bitstream 生成记录为 PASS；warning 未被抹除，仍按原 Packet 分类。
- bitstream 实读复核：11,847,132 bytes，SHA-256 `A897E33514A1079BB1B46C02C464B0BD679AF551CEFCB67C4A0EBD5B8FCD1ACD`。
- UART0 Hello ELF 实读复核：31,116 bytes，SHA-256 `E5BC80A2F18A7E2951D53DA539BE2FC61AAECFA90C5CDADB29E65FFC6141928A`；静态 LOAD/入口位于 `0xF9000000` 片上 RAM 范围。
- 以上仅证明固定 SHA 的离线冷构建。USER2、CPU 取指、UART0 115200、APB 实读与视频/机械臂均仍为 `NOT VERIFIED`。

## G2 探测、缺陷修复与纳入

- 源 worktree：`D:\CICC-g2-cpu-observability`；分支 `codex/g2-cpu-observability`；成果为未提交补丁。
- 纳入 20 个文件：单摄 CPU Host/fake runtime seam、fail-closed MMIO transport、C 测试、离线 bundle 工具/负例 fixture、README 与 Review Packet。
- 首次独立复测发现 Windows PowerShell 将含 `Program Files` 的嵌套长命令传给 Python 时拆参，C 测试虽为 `182/182`，bundle 创建却因 argparse 失败。
- 最小修复：runner 以 UTF-8 Base64 传递 `test_command`；Python 工具解码后写 manifest，并用 `getattr` 保持直接单测 Args 兼容。没有改变事件协议、MMIO 安全或机械臂禁用语义。
- 最终组合复测：`D:\CICC-g3-combined-g2-20260717_130416`，C Host `182/182`、bundle `VALIDATION_PASS`；Python unittest `3/3` PASS。
- G2 仍只证明 OFFLINE OBSERVABILITY L0；真实 RISC-V、MMIO/APB、板上 UART、CPU→OSD 和机械臂均未验证。

## G3 最后刷新

- `CURRENT_STATE.md` 已改为描述“固定 SHA + G1/G2 经验证未提交补丁”，明确不能称为已合入 main。
- NEXT_GATE 已从“再次冷构建”推进为：操作边界先复核固定 SHA 与匹配 bitstream/ELF hash，然后仅允许 `USER2` + `0xF9000000` + UART0 115200 Hello。
- 禁止项保持：USER1、Flash、DDR、UART2/J52、机械臂连接和动作。G2 的真实 MMIO/APB transport 必须在基础板级 Gate 后另审，禁止猜测地址。
- `maintenance_manifest.json` 增加 G1/G2 evidence packet 管理和源码变化触发；`last_verified_commit` 仍保留固定基线，未预写未来 commit。
- G1/G2/G3 的路径集合没有同路径覆盖；语义整合点仅为 CURRENT_STATE、manifest 与本 Packet。

## 上下文预算

| 文件 | G3 前 bytes / lines / o200k tokens | 最终 bytes / lines / o200k tokens |
|---|---:|---:|
| AGENTS.md | 22,585 / 197 / 6,511 | 7,153 / 81 / 2,056 |
| CLAUDE.md | 10,977 / 212 / 3,115 | 2,928 / 48 / 799 |
| CURRENT_STATE.md | 129,522 / 566 / 40,044 | 6,728 / 74 / 2,027 |
| 三入口合计 | 163,084 / 975 / 49,670 | 16,809 / 203 / 4,882 |
| 五文件强制阅读链 | — / — / 65,241 | — / — / 20,453 |

五文件链降低 `68.65%`；各单文件、三入口和五文件预算均 PASS。tokenizer 为 `tiktoken / o200k_base`。

## 历史归档

- 逐字归档：`debug_records/state_history/CURRENT_STATE_20260717_pre_g3.md`。
- 原文/归档 SHA-256：`FC022DBC9839C11C0A47F6FAD3AE21361B1FE10A17318E21C9FF5D41642C2D5B`。
- 129,522 bytes / 566 lines；原 1–566 行到归档 1–566 行一一映射。
- freshness 已通过归档文件、hash、行数和映射检查。

## 七问等价性复核（不读取历史正文）

1. FPGA 负责视频前端、ROI、统计特征、OSD 渲染和硬件通道；CPU 负责分类、四任务判断、逐轮状态机、参数及 myCobot：PASS。
2. 下一 Gate 是复核匹配 hash 后仅用 USER2 将匹配 ELF 下载到 `0xF9000000`，验证 UART0 115200 Hello：PASS。
3. USER1、Flash、DDR、UART2/J52、机械臂连接/动作仍禁止：PASS。
4. 四项任务、每项五轮、总计十分钟，识别/判断/执行理由与评分约束可从精简入口定位：PASS。
5. 当前 main 没有板级 PASS；G1 只有离线构建 PASS：PASS。
6. UART0 为 115200，myCobot 为 1000000，禁止混用：PASS。
7. 旧 bitstream 不可继承；只允许 G1 固定 SHA 的匹配 hash 进入下一受限 Gate：PASS。

## 最终验证

- `tools/agent_handoff_health_check.ps1`：exit 0，PASS。
- `tools/project_freshness_check.ps1`：exit 0，FAIL=0，WARN=3。
- Windows PowerShell 5.1 `tools/agent_context_budget.ps1`：exit 0，PASS。
- PowerShell 7 `tools/agent_context_budget.ps1`：exit 0，PASS。
- G2 Host runner：`182/182`，bundle `VALIDATION_PASS`，exit 0。
- G2 Python unittest：`3/3`，exit 0。
- G1 bitstream/ELF 文件存在且 SHA-256 与 Packet 一致：PASS。
- managed Markdown 链接、required markers、CURRENT_STATE evidence paths、历史归档 hash/映射：PASS。
- `git diff --check`：exit 0。
- 变更范围检查：没有 RTL、XML、peri.xml、SDC、IP settings、bitstream/ELF 或原始硬件证据变更；没有 SESSION_HANDOFF、官方细则、决赛主方案或 merge governance 变更。
- freshness 的 3 个 WARN：组合 worktree 按要求未提交；两份既有维护方案被 checker/manifest 变化触发但不在 G3 允许修改范围。没有 FAIL。

## POST_MERGE_REFRESH_REQUIRED

本组合必须在 G1/G2 之后最后审核和处理。审核者形成最终 commit/合并后：

1. 以结果 HEAD 更新 `maintenance_manifest.json` 的 `last_verified_commit`，不得继续使用当前未提交描述。
2. 重新核对 G1 bitstream/ELF hash 和 G2 Host bundle，重跑 handoff、freshness、context budget、Markdown/marker/archive/diff/scope 门。
3. 若结果 HEAD 或原子工程输入变化，G1 离线证据立即失效，退回冷构建；不得继承本 Packet 的 PASS。
4. 若任何板级观察与本快照冲突，进入 Contradiction Report，不得扩大到 Flash、DDR、UART2/J52 或机械臂。
