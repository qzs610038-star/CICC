# P0-A WSC 首批提交审查记录

> 原结论：`BLOCKED / NO_SUBMISSION_VISIBLE`
>
> 后续状态：已收到 `dbdbc9b84ce43e4c58a112678eadbb18ea4ef70d`，本记录的“未见提交”结论已被固定 SHA 复审取代；新裁定见 [WSC P0-A / P1 Host 固定 SHA 候选审查](WSC_P0_A_P1_HOST_CANDIDATE_AUDIT_20260719.md)。
>
> 审查者：qzs；基线：`codex/qzs-wsc-libaoxun-integration-20260718@0e5ab490559c58642734b0095753c6cf8787c709`

## 审查范围与实际来源

- 2026-07-19 kickoff 明确要求先审查 wsc 实际 diff，再实现 P0-A verifier，且不猜测工具链或路径。
- 使用 `git for-each-ref --sort=-committerdate` 与 `git log --all` 审查本机可见 refs。可见的最新 wsc refs 是：
  - `origin/dev/wsc6090-goal4-contract-after-qzs-20260719@48548f47dfa5964b13aed7edf3b3e9da6f6583a2`（Goal 4 合同，早于 kickoff）；
  - `origin/dev/wsc6090-uart1-cpu-20260719@13419d9922f3f8e7585bd43b77491b81b4bc0681`（已集成的历史 Host 修复）。
- 未见 kickoff 后可供审查的 wsc P0-A canary/有界 TX probe/P1 Host 实现 commit、diff 或固定 artifact bundle。本记录不推断远端尚未同步的工作不存在，只记录当前 qzs 工作区可审计的事实。

## 必需证据与当前结果

| P0-A 审查项 | 当前事实 | 结论 |
|---|---|---|
| 固定 wsc 固件 Git SHA 与输入 hash | 无新提交/manifest | `NOT RUN` |
| 严格构建、ELF/map/readelf/objdump | 无 artifact bundle | `NOT RUN` |
| entry/PT_LOAD、canary 地址、16 KiB RAM 与 stack/section 无重叠 | 无实际 ELF/linker/map | `NOT RUN` |
| stage write 在 UART wait 前 | 无实际 disassembly | `NOT RUN` |
| TX 永不 ready → 有界 `E101` 且 heartbeat 继续 | 无 Host/mock 负例 | `NOT RUN` |
| 停止条件：两次新证据修正后再 Review | 尚无 USER2/P0-A 执行轮次 | `NOT RUN` |

## qzs 已准备的可执行审查输入

- `competition_project_single_camera/tools/p0_a_evidence_verifier.py`：只验证 wsc 将来明示的 bundle 内 artifacts；解析 ELF entry、PT_LOAD 与符号表，检查 16 KiB RAM、canary/section/stack 范围、artifact hashes、objdump order witness、TX never-ready witness 和 fail-closed stop/safety fields。
- `competition_project_single_camera/docs/evidence_manifests/p0_a_evidence_manifest.template.json`：全部值为 `REPLACE`/`TEMPLATE_NOT_RUN`，不得作为证据或直接运行。
- 该准备不修改 CPU、RTL、Hard SoC、BSP 或冻结接口，也不选择任何工具链/路径。

## 三选一裁定

`P0-A-READY` 不成立：证据包尚未提交。`CHANGES_REQUESTED` 不适用：没有实现 diff 可要求修改。固定结论为 **`BLOCKED / NO_SUBMISSION_VISIBLE`**。

下一 Gate 是 wsc 在个人分支交付固定 SHA、允许范围 diff、严格编译与真实 bundle；qzs 再对该 SHA 运行 verifier，并单独复跑受影响 Host runner。即使该 Gate 完成，也不授权 USER2、P0-B 构建/上板、冻结接口变更、UART2/J52 或机械臂。
