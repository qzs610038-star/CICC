# wsc F1 Preboard 新对话启动提示词

> 使用时把 `{{PUBLISHED_PLAN_SHA}}` 替换为本次 qzs 交接消息给出的完整 SHA。

```text
你现在负责第十届集创赛单摄项目的 wsc F1 Preboard CPU/标定流。请在一次精确拉取后直接执行，不要只给方案。

固定来源：
- remote branch: codex/qzs-wsc-p0a-p1-integration-20260720
- exact plan SHA: {{PUBLISHED_PLAN_SHA}}
- personal branch: codex/wsc-f1-preboard-runtime-calibration-20260720
- recommended worktree: D:\CICC-wsc-f1-preboard-runtime-calibration-20260720

先保护当前工作：运行 git status --short --branch 与 git worktree list。不要全量 fetch --prune；用：
git ls-remote --heads origin refs/heads/codex/qzs-wsc-p0a-p1-integration-20260720
git fetch origin refs/heads/codex/qzs-wsc-p0a-p1-integration-20260720:refs/remotes/origin/codex/qzs-wsc-p0a-p1-integration-20260720
核对远端 HEAD 必须等于 exact plan SHA。若个人分支/工作区不存在，从该 SHA 新建 sibling worktree；若存在则核验其基线后继续。禁止 reset/clean/stash 覆盖现有修改。

完整阅读 AGENTS.md、CURRENT_STATE.md、以下三份文件：
1) docs/agent_context/QZS_WSC_F1_PREBOARD_KICKOFF_INDEX_20260720.md
2) docs/agent_context/WSC_F1_PREBOARD_PERSONAL_EXECUTION_PLAN_20260720.md
3) docs/agent_context/QZS_WSC_F1_PREBOARD_INTERFACE_CONTRACT_20260720.md
并读 cpu_mycobot skill、冻结/所有权文件。

随后按 W0->W2 立即实施；W3/W4 只消费 qzs 发布且 hash 固定的 QW-CALIBRATION-SAMPLE-v1 batch，没有 batch 时明确等待数据但继续 W1/W2/W5。实现平台无关 QW-F1-BOARD-SELFTEST-v1、Host 正负例、RISC-V freestanding profile/16KiB budget 与 UART1 PASS 后的小适配模板。不要重做已有合成证据，修改后仅把现有 runner 作为回归门。

硬边界：只写 wsc 允许范围；本轮不改 cpu/include、RTL/top/XML/SDC/IP/BSP/soc.h/embedded_sw/冻结 integration；single_camera_mmio_transport 保持 fail-closed；不猜 APB；ARM_ENABLED=0；不碰 UART2/J52/myCobot。不要进入、切换、merge 或修改 libaoxun 的活动 UART1/USER2 分支和工作区，也不要向他索取中间 SHA。只有其未来主动交付全链路 PASS 固定包后，才按 B0 流程绑定真实 BSP/linker。

每个 checkpoint 运行 team_scope(wsc)、interface freeze、diff-check、fresh 正负例。完成后在个人分支提交并推送，回报完整 SHA、qzs batch/hash、ELF profile 身份、map/readelf/objdump/预算、命令/exit code、未验证项和 ARM=0。除非用户另行要求，不合并 main，也不直接推双人种子分支。
```
