# SESSION HANDOFF — 2026-07-19 qzs 最终静态集成审查

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

Goal 4 已形成可直接转发的双人提示词与任务矩阵：`docs/agent_context/GOAL4_LIBAOXUN_WSC_PULL_AND_TASK_DISPATCH_20260719.md`。libaoxun 是唯一上板执行者；wsc 只做 CPU/Hello 预检和实时日志判读；qzs 在两份 READY 到齐后才请求用户批准并负责证据收口。当前仍为 `HARDWARE WINDOW NOT YET APPROVED`。

## qzs 最终静态集成状态

- 所有权已重新冻结：qzs 接管最终集成 manifest、Gate 与治理文档；上游 libaoxun
  build manifest 和 WSC CPU probe 仍保持各自来源归属，禁止复制后改写来源。
- 旧“7 项例外”已更正为 11 个跨所有者 ACMR 输入（libaoxun 5、WSC 2、qzs Gate/治理 4），
  并按来源 SHA 分段 `team_scope_check -BaseRef/-TargetRef` 全部 PASS。
- EOL 策略固定为 `*.ps1=CRLF`、`*.gdb=LF`、`*.cfg=LF`；docs/JSON/source 延续
  `.gitattributes` 既有规则。clean `8f0f618` checkout 已生成并独立验证最终 manifest，记录
  checkout SHA-256 和 Git blob SHA-1。
- `git diff --check`、接口冻结、WSC G2 `648/648`、classifier `54/54` 均 PASS；危险路由
  扫描未发现 Flash 或 direct-APB 写入，APB `PWDATA` 未连接。
- libaoxun 总 verifier 本机为 `NOT_RERUN_LOCAL`：集合 checkout 缺 `evidence/work/Efinity`
  原始 roots；只能引用固定 SHA 的历史 `inputs=82 artifacts=21 PASS`，不得将其写成当前复跑。
  因此当前 `VERDICT=NOT_READY_FOR_NEW_WINDOW_REQUEST`，并且远端仍为 `e72fb6a`，未回读最终 SHA。

```text
USER2=NOT_VERIFIED
PC=NOT_VERIFIED
UART1_HELLO_ECHO=NOT_VERIFIED
APB_MAGIC=NOT_VERIFIED
I3=BLOCKED_CONTRACT_NOT_FROZEN
```

2026-07-19 12:27:31 +08:00，libaoxun 正确报告 qzs 来源路径在其证据主机上不存在，并在任何 Git/硬件动作前停止。该问题归类为 `PATH_RESOLUTION_BLOCKED`，不是 I0 或 Git 失败。派工文档已改为先解析实际 `$RepoRoot`、验证 Git 顶层和 origin，再用 `git -C "$RepoRoot"` 执行全部 repo-relative 检查；不得扫描整盘或猜路径。

随后 libaoxun 回传 `$RepoRoot = 'C:\Users\20306\Desktop\赛题资料\CICC'`：目录存在，Git 顶层为同一 clone，origin 为 `https://github.com/qzs610038-star/CICC.git`，当前 `dev/libaoxun688-uart1-i0-20260719` 未报告 dirty，结论 `READY_PATH`。路径阻塞已关闭；尚未执行 fetch/switch/merge、原始 82/21 证据预检、构建或任何硬件动作。下一步只推进固定集合 SHA 同步与只读预检。

WSC 随后回传 clone `D:\CICC w` 与 origin 身份正常，但 `dev/claude-cpu-plan-status-0717` 含 tracked 修改 `final_project/cpu/CPU_MODULE_PLAN.txt`，因此在 fetch/switch 前停止；未执行任何 Git 同步或硬件动作。根因是原工作树承载需保护的 CPU 工作，不是集合 ref 失败。解阻方式固定为：保持原 dirty 原样，精确 fetch 后在不存在的新 sibling 目录执行 `git worktree add --detach <new-root> e72fb6a...`，WSC 只在该 clean 固定 SHA worktree 做 CPU/Hello 只读审查；禁止 stash/reset/clean/prune/remove。

## 下一执行者必须保留的标注

- `BLOCKED`：任务二非正方体完整能力；I0 UART1 离线批次已生成，但板级链路仍未验证。
- `NOT VERIFIED`：USER2、Type-C UART1 Hello/回显、APB MAGIC、真实 I1/APB/CDC、CPU→OSD、正式 RISC-V 板上执行、板级逐轮事务、机械臂闭环。
- `NOT RUN`：完整 myCobot 非动作矩阵与 PowerShell fail-closed 负例；不影响本次接口/分工冻结 PASS。

## Goal 4 follow-up — 2026-07-19 18:14 +08:00

> **2026-07-20 执行权转移（当前优先）**：本节中 qzs 汇总 READY、请求/签发窗口、WSC 实时判读以及多次交接的执行要求均已失效。用户已将 Goal 4 硬件段交给 libaoxun 证据主机独立闭环；qzs 不再签发硬件执行、也不作为运行前置。唯一当前入口为 `docs/agent_context/GOAL4_LIBAOXUN_SINGLE_OWNER_EXECUTION_20260720.md`；qzs 只在收到完整原始 evidence bundle 后做事后归档。历史结论与 SHA 仍保留，不能被解释为当前板级 PASS。

- qzs 临时登记起点：`fd3fc0881d4e71338f1aa34f361cd498b7cd2d4c`；同名远端当前 HEAD 每次以 `ls-remote` 回读，两位个人补丁基线始终为 `182fd6f5c4d628379760d6f4fc74e3b342e30083`。
- WSC `48548f47dfa5964b13aed7edf3b3e9da6f6583a2` 已固定并通过独立复跑：四个临时文件、APB contract、success/timeout/trap/wrong-PC/wrong-reason、G2 `648/648`、classifier `54/54`。原 WSC dirty 未被覆盖。
- libaoxun `2d713b80a41185e472837abaec3a10c01383c70f` 是直接基于 `182fd6f` 的十文件独立补丁；只接受为 `BLOCKER_SNAPSHOT`，未合入集合分支。
- USER2 根因已定位为 OpenOCD 参数语义错误：`use_bscan_tunnel 6 1` 是 width/type，`set_bscan_tunnel_ir` 必须消费 USER2 `0x09`；当前候选传入 `8`。Efinity `select_user()` API 不是必须的直接调用桥。
- libaoxun 还缺统一 host orchestrator：UART capture 没有 resume-marker 生产者，APB GDB 没有真实 1000 ms timeout 主动 halt/halt-reason Gate；原始 82/21 verifier 缺六个 evidence-root 文件并 exit 1。
- qzs 旧 final manifest 在本机 LF checkout 可 PASS，但 fresh worktree 依据 `.gitattributes` 签出 CRLF 后三个 PS1 hash 失败。生成器/verifier 必须同时检查 attribute 与实际字节，并从 fresh worktree 重生成。
- 临时所有权新增唯一 `competition_project_single_camera/tools/run_i0_uart1_execution_chain.ps1`，只授权 libaoxun 离线实现和 mock fixture；不授权启动 OpenOCD/GDB/串口/JTAG/APB。

保持：`GOAL4=BLOCKED_EXECUTION_TOOLCHAIN`、`HARDWARE_ACTIONS=NONE`、USER2/PC/UART1/APB 全部 `NOT_VERIFIED`。

## P0/P1 qzs + wsc 并行开工交接 — 2026-07-19

用户已裁定“批准 P0-A 与 P1 并行推进，同时仅准备 P0-B Review Packet”，并进一步明确本轮只启动 qzs 与 wsc。libaoxun 不接收新提示词、不需要拉取或审阅，继续其既有 UART1/USER2 攻坚。

本机/队友新对话统一从 `docs/agent_context/P0_P1_PARALLEL_KICKOFF_INDEX_20260719.md` 恢复，并读取 `docs/agent_context/prompts/` 下 qzs 或 wsc 提示词。已准备 Host 契约/证据前置包与 P0-B HOLD Packet；没有修改冻结接口、CPU/RTL/Hard SoC 真源，也没有构建、上板或机械臂动作。

### WSC 首批固定 SHA 复审

wsc 已提交 `codex/wsc-p0a-p1-host-20260719@dbdbc9b84ce43e4c58a112678eadbb18ea4ef70d`。qzs 在独立 detached worktree 反向审查并 fresh 复跑，确认范围、冻结面、P0-A Host/严格构建及 P1/既有 Host 计数通过；但正式 READY 条件仍有实质缺口，结论为 `CHANGES_REQUESTED / DO NOT MERGE`。完整 findings 与最小修复清单见 `docs/review_packets/WSC_P0_A_P1_HOST_CANDIDATE_AUDIT_20260719.md`。P0-B 保持 HOLD，USER2/板级 canary Gate 未打开，libaoxun 不受本轮影响。

### WSC 整改固定 SHA 复审

wsc 后续提交 `328f115c1716023d2c10c4276c80cd2833291551`，实现固定为 `6e9475ff6678c289160b3374cd98b6fda88c8455`。qzs fresh 复跑确认首次阻断中的阶段顺序、build_id、P0 manifest/verifier、制品治理、P1 schema/reserved-bit/ACK 与生命周期均已关闭；两个独立工作树 P0 身份制品实际同 hash。仍有两个最小阻断：提交的 `reproducibility.json` 保留旧 objdump hash；P1 20 轮 runner 仍将 decision/reason/snapshot hash/commit count 预置打印，没有以 fake snapshot 驱动 runtime/参考模型。最新结论仍为 `CHANGES_REQUESTED / DO NOT MERGE / P0-A READY NO / P1-HOST READY NO`，详见 `docs/review_packets/WSC_P0_A_P1_HOST_REMEDIATION_AUDIT_20260719.md`。P0-B 与 USER2 Gate 不打开，libaoxun 不受影响。

### WSC R1/R2 快速复审

wsc `a74b21d19e9a81d315456e106e9d23cc5402243a` 的 R1 经 qzs 两个 clean `2564f146...` 工作树 fresh 构建、verifier、12 字段自动比对与篡改负例通过，现签发 `P0-A-READY`；它不等于板上 CPU 执行。R2 runner 已真实驱动 snapshot/runtime/fake transport/packer，20 轮与 tamper 通过；但 clean checkout 的三份 P1 bundle 文本 hash 与 manifest 不一致，命令卡还含两个脚本不接受的参数。因此 P1 仅剩 evidence-packaging 修复，`P1-HOST-READY=NO / MERGE=NO`。下一轮不再重跑 P0 双构建。详见 `docs/review_packets/WSC_P0_A_P1_HOST_R1_R2_FINAL_AUDIT_20260719.md`。USER2 板级 Gate 仅可另行排期，本轮未运行；P0-B HOLD，libaoxun 不受影响。

### WSC P0-A / P1 Host 最终验收

wsc evidence-only SHA `aaf2058e7e05b8cda905b8096db309c65e0da5ef` 经 qzs clean-checkout manifest verifier、单文件 tamper、fresh P1 bundle 与 committed-vs-fresh 四 hash 比较全部通过；相对已接受实现没有 C/H 或硬件输入差分。现签发 `P0-A-READY=YES / P1-HOST-READY=YES / SOURCE_READY_FOR_INTEGRATION_REVIEW=YES`。尚未执行合并、USER2、上板或机械臂动作；BOARD 继续 NOT VERIFIED，P0-B HOLD。完整证据见 `docs/review_packets/WSC_P0_A_P1_HOST_FINAL_ACCEPTANCE_20260720.md`。

### WSC / QZS 跨成员集成审查

qzs `fce2cf461ea9bc48f786a70f3276c7e4a84658e7` 与 wsc `aaf2058e7e05b8cda905b8096db309c65e0da5ef` 已完成固定 SHA no-commit 集成审查。两侧直接基于 kickoff，分别 21/41 个文件且零交集；merge-tree 和独立工作树合成树均为 `e689cd5395bcd8b65b0fa1b59f3b491c52c8d08a`。组合 P0-A `10/10`、P1 `37/39/54/213`、runtime `648/648`、20 轮 replay、manifest/tamper、QEMU、接口冻结和 offline presubmit 全部通过。结论 `APPROVE_FOR_MERGE / MERGE_NOT_PERFORMED`，见 `docs/review_packets/WSC_QZS_CROSS_MEMBER_INTEGRATION_REVIEW_20260720.md`。libaoxun 的分支、工作区和 UART1/USER2 实验未被触碰或通知。

### qzs P0-A/P1 治理 checkpoint — kickoff `0e5ab490` 后

- qzs 个人分支：`codex/qzs-p0a-p1-host-governance-20260719`，固定起点 `0e5ab490559c58642734b0095753c6cf8787c709`。
- 新增 P0-A fail-closed artifact contract：`competition_project_single_camera/tools/p0_a_evidence_verifier.py` 与 `competition_project_single_camera/docs/evidence_manifests/p0_a_evidence_manifest.template.json`。它不猜工具链、BSP、RAM base 或 wsc 文件路径；未来 bundle 必须显式提供并哈希绑定 ELF/map/readelf/objdump/build log/TX-never-ready witness。通过前仍为 `BOARD NOT VERIFIED`。
- 新增 P1 三层矩阵、JSONL schema/样例、采集 CSV、OSD/输入状态表和 20 轮 bundle manifest 模板，入口为 `competition_project_single_camera/docs/evidence_manifests/P1_HOST_THREE_LAYER_EVIDENCE_MATRIX_20260719.md`。全部为 `TEMPLATE/NOT RUN`，不冻结 APB/PSTRB/IRQ/CDC/OSD wire ABI。
- qzs 已审查本机可见 wsc refs：最新 `origin/dev/wsc6090-goal4-contract-after-qzs-20260719@48548f47...` 早于 kickoff 且无 P0-A/P1 实现 diff；本轮 verdict=`P0-A=BLOCKED / NO_SUBMISSION_VISIBLE`。未运行任何 wsc Host runner 或 P0-A 工具链，旧 Host PASS 不继承。
- P0-B review draft 已同步此事实并保持 `HOLD`；没有向 libaoxun 发送任务/同步/审阅请求。`ARM_ENABLED=0`，USER2/PC/UART1/APB/OSD/board 均保持 `NOT VERIFIED`。
