# SESSION HANDOFF — 2026-07-19 Goal 1/2 固定 SHA 集合与 Goal 3 准备

## 恢复入口

- 分支：`codex/qzs-wsc-libaoxun-integration-20260718`
- 基线：`main@9acf4d8b2ec788ccd5777f3833a7bfb756c51cad`
- WSC 来源：`dev/wsc6090-uart1-cpu-20260719@13419d9922f3f8e7585bd43b77491b81b4bc0681`
- libaoxun 来源：`dev/libaoxun688-uart1-i0-20260719-cleanlf-final@72cc281bd104726d9db1e88cb2894facb1d5fd1a`，合并提交 `f10cbd3`
- QZS 来源：`codex/qzs-final-integration-goals-20260718@018ced2a6e7b96c8e1fef85ea6c15d4c1fa77a23`，合并提交 `03f9750`
- 固定输入顺序：libaoxun UART1 原子批次 → WSC Host 修复 → QZS 状态刷新。正式 `main` 未改；恢复后先实读 `git status --short --branch`、`git rev-parse HEAD` 和 `CURRENT_STATE.md`。

## 已完成的集成裁决

1. 排除 WSC 根目录四份重复指南，只保留 `learning_guides/接口对齐与数据链路学习/`。
2. 圆柱/锥体保持 `WAIT`；任务二完整能力标记为 `BLOCKED`，只能超时或人工放弃。
3. runtime 终态改为 release-only idle-drain ACK：不分类、不重复提交结果、不产生动作；真实 I1 wire ABI 仍未冻结。
4. G1 与 R0 分成独立批次。R0 manifest 固定 `9F6F.../CD4C...`、八项输入 blob、COM12/COM17 和 CH340 `1A86:7523`。
5. PR #12 feature contract 的 Gate 文案已改为未来条件，避免误读成当前全部通过。
6. G2 runner 已修复为传播 C 测试失败码；直接 Host 脚本增加 VS2022 fallback。
7. 用户与三人确认单摄为唯一正式视频/识别路线，原双摄方案取消。
8. I0 固定为 SoC UART1 → 板载 Type-C UART1，RX=`GPIOR_96/B12`、TX=`GPIOR_100/D12`、`115200 8N1`；UART0/R0 只保留为历史证据。
9. 三人文件所有权与精简 Gate 已冻结，见 `docs/agent_context/TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md`。
10. Goal 1 已对 libaoxun `72cc281` / batch `I0_UART1_20260719_CLEAN_LF_FINAL` 给出 `APPROVE`；Goal 2 已在 WSC `13419d9` 与该硬件批次的固定组合上复审为 `APPROVE`。
11. 两个固定分支均已合入集合分支；合并树探测无文本冲突，未拆分 Hard SoC 原子输入。
12. 用户在 2026-07-19 明确授权本轮集合提交同时纳入 `ppt_doc_outlines/**` 的 Markdown 行尾空格清理；该目录不在旧 `team_scope_check -Role qzs` 白名单，因此静态范围检查会保留 3 项预期 violation，作为治理 WARN 而不是接口/安全越界。

## 已观察测试结果

- `single_camera_f1`：`213/213 PASS`
- `single_camera_feature_adapter`：`33/33 PASS`
- `single_camera_runtime`：`648/648 PASS`
- G2 bundle validation：`PASS`
- classifier 直接 Host 入口：`54/54 PASS`；VS2022 `/W4 /WX` 编译并实际运行，旧 `C4127` 阻断已关闭。
- 以上四项均为 2026-07-19 在合并后的真实工作树 fresh 运行并 exit 0；Host PASS 不证明板级 UART1、APB、OSD 或 RISC-V 执行。
- Goal 3 `offline_presubmit=PASS_WITH_WARNINGS`（exit 0）：G2 bundle unittest、G2 Host `648/648`、myCobot 非动作 QEMU、接口冻结、context、handoff 与 `git diff --check` 均 PASS；freshness 为 `WARN=8 / FAIL=0`。
- 沙箱内首次 offline presubmit 因系统 TEMP `PermissionError` 失败；获准在真实环境用同一命令重跑后 PASS，故该失败归类为环境权限证据而非代码回归。
- `team_scope_check -Role qzs` 报 3 个 `ppt_doc_outlines/**` violation；用户已明确授权一起提交，未改旧白名单，保留为治理 WARN。

## 硬件与安全状态

- 当前集合树已改变并固定 FPGA/Hard SoC UART1 原子输入；旧 UART0/R0 制品因此保持历史状态，不得继承。
- R0/UART0 已降级为历史批次，不再作为下一 Gate；其 bitstream/ELF、COM12/COM17 和 0-byte 记录不得继承到 UART1。
- 当前 Hard SoC 源码与离线制品身份为 UART1：`PERI_UART_0=0`、`PERI_UART_1=1`、`PERI_UART_2=0`，RX/TX=`GPIOR_96/GPIOR_100`；同批 bitstream `D05E...C544`、Hello ELF `919B...F7FA`。其结论仅为 `I0-BUILD APPROVE`，USER2/UART1/APB 仍 `NOT VERIFIED`。
- UART2/J52、myCobot 接线、帧和动作全部 `NO-GO`；本次没有执行任何硬件或机械臂动作。

## 三位队友拉取后的第一步

三位队友先在各自本机检出本集成分支，阅读：

1. `CURRENT_STATE.md`
2. `competition_project_single_camera/integration/F1_INTERFACE_ALIGNMENT_DRAFT.md`
3. `competition_project_single_camera/integration/F1_INTERFACE_CONFIRMATION_REGISTER.md`
4. `competition_project_single_camera/integration/single_camera_feature_contract.md`
5. `final_project/cpu/CPU_MODULE_PLAN.txt`

三人确认已经完成。下一步先读取 Goal 1/2 审查记录和本次 Goal 3 总门结果；只有集合 SHA、bitstream/ELF hash 与用户批准窗口完全匹配，才连续执行 USER2、UART1 Hello/回显与 APB MAGIC。相同输入 hash 不重复确认，任何 hash、接线或失败现象变化都重开对应 Gate。

## 下一执行者必须保留的标注

- `BLOCKED`：任务二非正方体完整能力；I0 UART1 离线批次已生成，但板级链路仍未验证。
- `NOT VERIFIED`：USER2、Type-C UART1 Hello/回显、APB MAGIC、真实 I1/APB/CDC、CPU→OSD、正式 RISC-V 板上执行、板级逐轮事务、机械臂闭环。
- `NOT RUN`：完整 myCobot 非动作矩阵与 PowerShell fail-closed 负例；不影响本次接口/分工冻结 PASS。
