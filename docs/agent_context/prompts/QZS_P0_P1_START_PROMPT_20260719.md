# qzs 直接开工提示词

```text
你现在在 CICC 集合分支的 qzs 工作区推进 P0-A + P1 Host 的治理/证据准备。先实读 AGENTS.md、CURRENT_STATE.md、SESSION_HANDOFF.md、docs/agent_context/P0_P1_NO_UART_CPU_AND_F1_PARALLEL_IMPLEMENTATION_PLAN_20260719.md、P1_HOST_CONTRACT_AND_EVIDENCE_PREWORK_20260719.md、P0_P1_PARALLEL_KICKOFF_INDEX_20260719.md，并运行 tools/agent_handoff_health_check.ps1、git status --short --branch、git rev-parse HEAD。以实际 kickoff SHA 为固定基线，保护所有无关 dirty。

已批准边界：立即推进 P0-A 与 P1 Host；只准备 P0-B Review Packet。未授权 P0-B 构建/上板、冻结接口修改、纯 FPGA 分类迁移、UART2/J52 或机械臂。ARM_ENABLED=0。不得把 Host/设计状态写成 RISC-V、APB、OSD 或板级 PASS。

本轮只与 wsc 协作。不要给 libaoxun 发送新任务、同步要求、审阅请求或追问；其现有 UART1/USER2 攻坚保持不受影响。需要其硬件输入的事项全部登记为未来依赖，不在本轮等待。

你的写入范围仅为 qzs 所有权：docs/**、final_project/docs/**、competition_project_single_camera/docs/**、competition_project_single_camera/tools/**、根 tools/** 及状态/交接治理文件；不得修改 CPU/RTL/Hard SoC 真源或冻结 integration/header 文件。

本轮按顺序完成：
1. 为 P0-A 定义可机器检查的证据 manifest/verifier：固定固件 Git SHA、输入 hash、ELF/map/readelf/objdump、entry/LOAD 段、canary 符号地址与 16 KiB RAM/栈/段不重叠、TX 永不 ready 负例、停止条件；先审查 wsc 实际 diff 后再实现 verifier，不猜工具链或路径。
2. 把 P1 不变量编号落实为 Host→RTL TB→board 三层证据矩阵；维护 JSONL 向量格式、采集 CSV、OSD/输入状态表、20轮 bundle manifest。禁止定义 APB 地址/PSTRB/IRQ/CDC wire ABI。
3. 维护 p0_b_bram_preload_apb_heartbeat_review_draft_20260719.md 为 HOLD；只有 P0-A READY、USER2 两次有新证据修正仍失败且用户另批后才允许升级。
4. 审核 wsc 首批提交的范围、严格编译、负例与 hash；形成 P0-A-READY / CHANGES_REQUESTED / BLOCKED 三选一结论。
5. 每个可验证 checkpoint 后更新 CURRENT_STATE.md、SESSION_HANDOFF.md 和对应 Review Packet；保留 NOT RUN/NOT VERIFIED。

验证至少运行受影响的 Host runner、tools/offline_presubmit.ps1、接口冻结检查、team scope、git diff --check、git status。完成时提交并推送 qzs 个人分支，回传完整 SHA、远端 ref、测试 exit code、warning、未验证项和下一 Gate；不要直接修改 main 或集合分支，除非用户再次明确要求。
```
