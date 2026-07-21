# wsc 直接开工提示词

```text
你现在负责 CICC 的 P0-A 诊断固件与 P1 CPU/Host 实现。先保护原工作树的 final_project/cpu/CPU_MODULE_PLAN.txt dirty：不得 stash/reset/clean/覆盖；在安全个人分支或新的 sibling worktree 中，仅精确 fetch 一次 qzs 提供的 codex/qzs-wsc-libaoxun-integration-20260718 kickoff SHA。实读 AGENTS.md、CURRENT_STATE.md、P0/P1 方案、P1 Host 前置包、三人所有权和 kickoff 索引，记录 branch/HEAD/dirty/remote SHA。

已批准：P0-A + P1 Host。未批准：P0-B 实现/构建/上板、冻结接口修改、RTL/XML/SDC/IP、纯 FPGA 分类迁移、UART2/J52、机械臂。ARM_ENABLED=0。Host PASS 不能写成 RISC-V/MMIO/APB/OSD/board PASS。

本轮只与 qzs 协作。不要向 libaoxun 发送新任务、同步要求、审阅请求或追问；其现有 UART1/USER2 攻坚保持不受影响。需要其 USER2/硬件输入的工作一律标为未来依赖，P0-A 先完成离线 READY 候选包。

只写你的范围：competition_project_single_camera/cpu/src/**、cpu/tests/**、cpu/README.md，以及 cpu_bringup/uart1_hello_onchip/**。冻结 cpu/include/** 和 integration/** 保持只读；如确需改语义，停止并提交差异，不要改文件。

优先级一（6h Gate，先完成）：
1. 在 cpu_bringup/uart1_hello_onchip/** 新建独立 P0-A 诊断程序，不覆盖 libaoxun embedded_sw 真源。实现 volatile canary：magic/schema/build_id/write_count/stage/last_pc_tag/uart_status/error/heartbeat/checksum。
2. 阶段至少 C001/C002/C003/C004/C005/E101/C0FF；所有 UART 轮询有界，TX 永不 ready 时写 E101 后继续无外设 heartbeat。
3. 从当前同批 linker/map 得到 canary 实址，不硬编码旧地址，不写未知 APB，不触发复位。
4. 产出严格编译日志、ELF/map/readelf/objdump/disassembly、输入 hash；证明阶段写在 UART 等待前，entry/LOAD/canary 位于当前 16 KiB RAM 且不与栈/段冲突。
5. 增加 Host/mock 负例，证明 TX 永不 ready 不会死循环。向 qzs 提交固定 SHA 与 P0-A-READY 候选包；不自行宣称板上 CPU 已执行。

优先级二（与 P0-A 后半段并行）：
1. 按 P1_HOST_CONTRACT_AND_EVIDENCE_PREWORK 的编号扩展黄金模型、fake transport、snapshot/ACK、回绕、复位、idle-drain、错误 ACK 等正负例。
2. 实现抽象 32-bit staging/commit 结果 packer 与 round_id 去旧结果测试，但不猜 APB 偏移。
3. 实现 fake input 的 APPLY/PLACE/REMOVE/ABANDON/RESET + event_seq/ACK/重复乱序抖动负例。
4. 生成可供 RTL TB 复用的 JSONL 向量；完成标定分析与四任务20轮 ARM=0 回放 bundle。尺寸/非正方体证据不足时保持 SIZE_UNAVAILABLE/BLOCKED/PROVISIONAL。

每个 checkpoint 运行相应 Host runner、warning-as-error、git diff --check 和 git status。提交并推送 wsc 个人分支，回传完整 SHA、文件清单、命令/exit code、测试计数、原始日志索引、未验证项与希望 qzs 裁定的问题。不得直接推集合分支或 main。
```
