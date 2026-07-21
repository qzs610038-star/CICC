# HISTORICAL / SUPERSEDED — prior Goal 4 dispatch

> This dated dispatch must not override the confirmed UART1 H0-H6 route, `COM17` identity, or H3 route in `CURRENT_STATE.md`.

# Goal 4 队友拉取提示词与 I0-SMOKE 任务分工

> 日期：2026-07-19（Asia/Shanghai）
>
> 集合分支：`codex/qzs-wsc-libaoxun-integration-20260718`
>
> 两人独立个人补丁基线：`182fd6f5c4d628379760d6f4fc74e3b342e30083`
>
> 状态：`TEMPORARY_SCOPE_REGISTERED / NO_DIRECT_WORK_OR_MERGE / HARDWARE_WINDOW_NOT_APPROVED`

> **2026-07-19 交接覆盖说明**：本文件先前包含的 `e72fb6a` 同步/执行提示仅作历史记录，
> 不得再执行。qzs 同名远端当前 HEAD 必须在派工时以 `git ls-remote` 回读；它只承载治理记录，
> 不改变两人的 `182fd6f` 个人补丁基线，也不授权 cherry-pick、合并、构建或硬件操作。
> 唯一有效的范围与回收点见
> [`GOAL4_TEMPORARY_OWNERSHIP_REGISTER_20260719.md`](GOAL4_TEMPORARY_OWNERSHIP_REGISTER_20260719.md)。

本文件把 Goal 4 拆成“共同同步 → 双人独立预检 → qzs 汇总 → 用户批准 → libaoxun 单人上板执行 → wsc 判读 → qzs 收口”。它不授权 JTAG、USER2、串口打开、APB 访问或任何机械臂操作。

`D:\第十届集创赛-雄芯院材料` 只是 qzs 主机上的来源路径，不是队友主机必须存在的路径。跨机器执行只认队友实际 Git clone 根目录和仓库相对路径。

## 1. 本轮唯一固定输入

| 项目 | 固定值 | 边界 |
|---|---|---|
| 集合分支 / SHA | `codex/qzs-wsc-libaoxun-integration-20260718@e72fb6a3ce1b7999f61cae1e0ed7b2773f1e4fda` | 两位队友必须独立核对本地、upstream、远端 SHA |
| WSC 来源 | `13419d9922f3f8e7585bd43b77491b81b4bc0681` | Host/CPU 修复来源；必须是集合 SHA 的祖先 |
| libaoxun 来源 | `72cc281bd104726d9db1e88cb2894facb1d5fd1a` | UART1 Hard SoC 与 I0-BUILD 证据来源；必须是集合 SHA 的祖先 |
| batch ID | `I0_UART1_20260719_CLEAN_LF_FINAL` | 输入、工具或制品 hash 变化即关闭窗口 |
| bitstream SHA-256 | `D05EFD4EC91FDB51F2997483BA7F4A3634C3E50FF1C65DD7239B41E19E2BC544` | 只能使用 libaoxun 原始证据主机上的匹配文件 |
| Hello ELF SHA-256 | `919B291A2B980F02507D1591EFBE5D8E395DC53F4A480AE5A8C577D74CF5F7FA` | 不得复用旧 UART0/R0 ELF |
| 同批 `soc.h` SHA-256 | `25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B` | UART1 `0xe8011000`、`115200`；不得猜基址/IRQ |
| 片上 RAM | `0xF9000000..0xF9003FFF` | 实际 PC 越界立即停止 |
| UART1 物理路由 | RX=`GPIOR_96/B12`，TX=`GPIOR_100/D12`，`115200 8N1` | UART0 不回退；UART2/J52 必须保持断开 |
| APB 只读期望 | 同批 `soc.h` 来源地址；MAGIC=`0x375A0001` | 不写业务寄存器，不用 `0`/`FFFFFFFF` 猜根因 |

权威证据入口：

- `CURRENT_STATE.md`
- `docs/agent_context/QZS_GOAL1_LIBAOXUN_UART1_I0_BUILD_FINAL_AUDIT_20260719.md`
- `docs/agent_context/QZS_GOAL2_WSC_CPU_UART1_AUDIT_20260718.md`
- `competition_project_single_camera/embedded_sw/uart1_hello_onchip/I0_UART1_BUILD_MANIFEST.json`
- `competition_project_single_camera/docs/debug_sessions/I0_SMOKE_OPERATION_CARD_DRAFT_20260718.md`

## 2. 共同安全拉取规则

两位队友均须先检查 dirty。若存在未提交修改、未跟踪工程文件或当前分支不明，停止并完整报告；不得自动 stash、reset、checkout 覆盖或删除。

Windows 若遇到 `dev/wsc6090-CPU` / `dev/wsc6090-cpu` 大小写 ref 冲突，只精确 fetch 本集合 ref，并以 `git ls-remote --heads origin` 为远端真源。不得迁移 refs 后端。

### 2.1 Stage 0：解析队友主机上的真实仓库根目录

禁止把 qzs 的 `D:\第十届集创赛-雄芯院材料` 当作队友路径，也不要递归扫描整块磁盘。按以下顺序处理：

1. 若 Agent 当前已经位于某个 clone 内，运行 `git rev-parse --show-toplevel`；exit 0 的输出才是候选根目录。
2. 若不在 clone 内，由主机操作人提供该机已有 CICC clone 的实际绝对路径；若尚未 clone，先报告 `REPO_NOT_CLONED`，不要自行选择磁盘或目录。
3. 把实际路径写入当前 PowerShell 会话变量，例如：`$RepoRoot = '<ACTUAL_CICC_REPO_ROOT>'`。
4. 只读验证：

```powershell
Test-Path -LiteralPath $RepoRoot -PathType Container
git -C "$RepoRoot" rev-parse --show-toplevel
git -C "$RepoRoot" remote get-url origin
git -C "$RepoRoot" status --short --branch
```

只有四项同时满足才进入同步：目录存在；Git 顶层与 `$RepoRoot` 指向同一 clone；`origin` 确认为本项目 CICC 仓库（HTTPS/SSH 形式均可）；status 已回传且无需要保护的本地工作。否则保持 `PATH_RESOLUTION_BLOCKED`，不执行 fetch/switch/merge。

### 2.2 立即发给 libaoxun 的路径解阻提示词

```text
你上一步因 D:\第十届集创赛-雄芯院材料 不存在而 BLOCKED 是正确行为。该路径只是 qzs 主机来源路径，不要求你的证据主机相同。

当前只执行 Stage 0，不执行 fetch/switch/merge、构建或任何硬件动作：
1. 如果你当前已经位于 CICC clone 内，运行 git rev-parse --show-toplevel，并回传完整输出和 exit code。
2. 如果不在 clone 内，请由本机操作人提供这台机器上 CICC clone 的实际绝对路径；不要递归扫描整盘，也不要猜目录。
3. 得到路径后设置 $RepoRoot='<实际路径>'，只运行：
   Test-Path -LiteralPath $RepoRoot -PathType Container
   git -C "$RepoRoot" rev-parse --show-toplevel
   git -C "$RepoRoot" remote get-url origin
   git -C "$RepoRoot" status --short --branch
4. 回传 RepoRoot、四项原始输出和 READY_PATH / REPO_NOT_CLONED / PATH_RESOLUTION_BLOCKED 三选一结论。

在 qzs 确认 RepoRoot 和 origin 身份前，不执行 git fetch/switch/merge，不执行 JTAG、USER2、串口、APB、接线或构建。
```

### 2.3 libaoxun 路径回传

2026-07-19，libaoxun 已回传并通过 Stage 0：

- `$RepoRoot = 'C:\Users\20306\Desktop\赛题资料\CICC'`；
- `Test-Path` 为 `True`；
- Git 顶层为 `C:/Users/20306/Desktop/赛题资料/CICC`；
- `origin = https://github.com/qzs610038-star/CICC.git`；
- 当前分支为 `dev/libaoxun688-uart1-i0-20260719`，status 未报告 dirty；
- 结论：`READY_PATH`。

这只关闭路径阻塞，不等于集合分支已同步、I0 预检已完成或硬件窗口已批准。

### 2.4 WSC dirty 回传与隔离策略

2026-07-19，WSC 已确认其 clone 为 `D:\CICC w`、origin 为 `https://github.com/qzs610038-star/CICC.git`，但当前 `dev/claude-cpu-plan-status-0717` 含 1 个 tracked 修改：`final_project/cpu/CPU_MODULE_PLAN.txt`。WSC 在 fetch/switch 前停止，结论正确。

该 dirty 属于需保护的既有 CPU 工作，不要求 WSC stash、reset、clean、提交或放弃。后续同步改用同一 clone 的独立 detached worktree：原工作树只作为 Git object/ref 来源，精确 fetch 集合 ref 后，在一个经确认不存在的新目录上执行 `git worktree add --detach <new-root> e72fb6a...`。不得删除、移动或修复原 dirty 文件，也不得复用已有目录或执行 `git worktree prune/remove`。

独立 worktree 建立后，WSC 的 Goal 4 审查根目录改为新 worktree；原 `D:\CICC w` 只用于证明 dirty 在隔离前后保持不变。

## 3. 可直接发给 libaoxun 的提示词

```text
你现在在自己的证据主机上承担 Goal 4 的 libaoxun 上板执行者职责。你回传的 `$RepoRoot = 'C:\Users\20306\Desktop\赛题资料\CICC'`、Git 顶层、origin 和 clean status 已被 qzs 接受为 `READY_PATH`。当前只执行集合分支同步和 I0 只读预检；在 qzs 转发用户的单次硬件批准记录前，严禁 JTAG、USER2、打开串口、APB 访问或任何接线变化。

先在当前 PowerShell 会话固定：
`$RepoRoot = 'C:\Users\20306\Desktop\赛题资料\CICC'`

【固定目标】
只从集合分支 codex/qzs-wsc-libaoxun-integration-20260718 拉取固定 SHA e72fb6a3ce1b7999f61cae1e0ed7b2773f1e4fda，并准备在批准后按操作卡连续执行：同批预检 → USER2 + 片上 RAM PC → Type-C UART1 Hello/单字符回显 → 同批 soc.h 来源的只读 APB MAGIC。任何单点失败立即停止。

【第一步：安全同步，不覆盖本地工作】
1. 只在已验证的 `$RepoRoot` 上运行 `git -C "$RepoRoot" status --short --branch`、`git -C "$RepoRoot" branch --show-current`、`git -C "$RepoRoot" rev-parse HEAD`，原样回传。
2. 若 tracked dirty、未跟踪 Efinity 工程输入或本地工作未保存，立即停止；不要 stash、reset、clean 或强制切分支。
3. 工作区干净时，精确执行：
   git -C "$RepoRoot" fetch origin refs/heads/codex/qzs-wsc-libaoxun-integration-20260718:refs/remotes/origin/codex/qzs-wsc-libaoxun-integration-20260718
4. 运行：
   git -C "$RepoRoot" ls-remote --heads origin refs/heads/codex/qzs-wsc-libaoxun-integration-20260718
   git -C "$RepoRoot" rev-parse refs/remotes/origin/codex/qzs-wsc-libaoxun-integration-20260718
   两者必须都是 e72fb6a3ce1b7999f61cae1e0ed7b2773f1e4fda。
5. 若本地已有同名分支，只允许执行：
   git -C "$RepoRoot" switch codex/qzs-wsc-libaoxun-integration-20260718
   git -C "$RepoRoot" merge --ff-only refs/remotes/origin/codex/qzs-wsc-libaoxun-integration-20260718
   若没有本地同名分支，只允许执行：
   git -C "$RepoRoot" switch --track -c codex/qzs-wsc-libaoxun-integration-20260718 refs/remotes/origin/codex/qzs-wsc-libaoxun-integration-20260718
   最后回传实际 RepoRoot、`git -C "$RepoRoot" rev-parse HEAD` 和 `git -C "$RepoRoot" status --short --branch`。

若 fetch、远端 SHA、switch 或 ff-only 任一步失败，立即输出 `SYNC_BLOCKED`、失败命令、exit code 和原始输出；不得改用普通 merge、rebase、force、reset、stash 或 clean。

【第二步：只读 I0 预检】
1. 实读 AGENTS.md、CURRENT_STATE.md、SESSION_HANDOFF.md、Goal 1 最终审计、I0 build manifest 和 I0-SMOKE 操作卡。
2. 证明 72cc281bd104726d9db1e88cb2894facb1d5fd1a 是当前 HEAD 的祖先；不得用“分支最新”代替完整 SHA。
3. 在你的原始 Efinity 证据主机上核对 batch=I0_UART1_20260719_CLEAN_LF_FINAL：
   - bitstream SHA-256=D05EFD4EC91FDB51F2997483BA7F4A3634C3E50FF1C65DD7239B41E19E2BC544
   - Hello ELF SHA-256=919B291A2B980F02507D1591EFBE5D8E395DC53F4A480AE5A8C577D74CF5F7FA
   - soc.h SHA-256=25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B
   - Efinity=2025.2.288.4.15、Titanium TJ375N529、timing model I3
   - 82 项输入、21 项制品、Map/Interface/PNR/STA/CDC/warning 身份与 manifest 完全一致。
   集合工作区 `$RepoRoot` 只用于同步和审阅，不能直接作为 verifier 的 `DesignRoot`。`verify_i0_uart1_build_evidence.ps1` 要求 `DesignRoot` 为原始 clean-LF 设计 checkout，且 HEAD 严格等于 manifest `design_sha=6effdc3685d696cb4d33f3fbb1c449729ed72e33`；同时必须使用原始 `EvidenceRoot`、`OutflowRoot`、`WorkRoot` 和已绑定工具路径。若这些原始根目录任一不确定，输出 `EVIDENCE_ROOT_BLOCKED`，不要猜路径、重建或把集合 HEAD 临时 reset 到设计 SHA。
4. 只确认板卡身份、当前接线、Type-C UART1 路由、UART2/J52 物理断开、急停/断电责任人和失败停止方法；不要执行硬件动作。
5. 任一 hash、工具身份、板卡、接线或证据根目录不匹配，输出 BLOCKED 和完整差异，不重建、不替换制品、不继续。

【向 qzs 提交 READY 报告】
必须逐项回传：branch、HEAD、dirty；远端 SHA；板卡 ID；执行人；Efinity/器件/timing identity；bitstream/ELF/soc.h 完整 SHA-256；82/21 verifier 结果；UART2/J52 断开确认；急停/断电责任人；拟使用的原始日志目录；READY 或 BLOCKED。

【收到批准后才执行】
批准记录必须同时固定：集合 SHA、batch、bitstream/ELF/soc.h hash、板卡 ID、执行人、Type-C UART1 接线、UART2/J52 断开、停止策略和窗口时间。缺一项就不开始。

只按仓库 I0-SMOKE 操作卡和已审查工具执行，不临时拼命令，不猜 USER、COM、PC、基址或 APB 地址。顺序固定为：
0. same-batch preflight；
1. USER2 + 片上 RAM PC，PC 必须在 0xF9000000..0xF9003FFF；
2. Type-C UART1 115200 8N1 Hello/单字符回显；
3. 同批 soc.h 来源的只读 APB MAGIC，期望 0x375A0001；
4. 形成唯一 I0-SMOKE 结论。

USER2_FAIL、PC_RANGE_FAIL、UART1_FAIL、APB_MAGIC_FAIL 任一出现立即停止并保留原始证据。禁止回退 UART0、USER1、Flash、DDR、业务 I1-I4、UART2/J52、myCobot 查询或动作。

【交付格式】
回传每步时间、命令/工具、exit code、原始输出或截图索引、发送/接收字节数、PC、APB 地址来源、读值、文件 SHA-256和停止点。原始机器日志留在证据主机；Git 中只允许脱敏摘要、hash 和可复查索引。不要自行修改 CURRENT_STATE、冻结接口或推送集合分支。
```

## 4. 可直接发给 wsc 的提示词

```text
你现在在自己的主机上承担 Goal 4 的 WSC CPU/固件审查与板级日志判读职责。你的 `D:\CICC w` 路径和 origin 已通过，但原工作树含需保护的 `final_project/cpu/CPU_MODULE_PLAN.txt` tracked 修改。不得在原工作树 switch，也不得 stash/reset/clean/提交该修改；按本文 2.4 建立 detached 固定 SHA 独立 worktree 后再继续。libaoxun 是唯一上板执行者；你只能同步、独立核对 CPU/Hello 输入并提交 READY/BLOCKED，不能执行 JTAG、串口或硬件操作。

【固定目标】
只从集合分支 codex/qzs-wsc-libaoxun-integration-20260718 拉取固定 SHA e72fb6a3ce1b7999f61cae1e0ed7b2773f1e4fda，证明 WSC 13419d9922f3f8e7585bd43b77491b81b4bc0681 与 libaoxun 72cc281bd104726d9db1e88cb2894facb1d5fd1a 都是其祖先；核对 UART1 Hello、同批 soc.h、RAM/PC 与 APB MAGIC 语义，随后在 libaoxun 执行窗口中只负责判读。

【第一步：隔离原 dirty 并安全同步】
1. 固定 `$SourceRepo='D:\CICC w'`，先回传原工作树 status 和 `git worktree list --porcelain`；确认 dirty 仍只有 `final_project/cpu/CPU_MODULE_PLAN.txt`。
2. 选择一个不存在的新 sibling 目录作为 `$ReviewRoot`；若目录已存在，停止，不删除、不覆盖。
3. 原工作树不得 switch。只允许在 `$SourceRepo` 精确 fetch 集合 ref，并确认远端与 remote-tracking SHA 都为 `e72fb6a3ce1b7999f61cae1e0ed7b2773f1e4fda`。
4. 只允许用固定 SHA 建 detached worktree：`git -C "$SourceRepo" worktree add --detach "$ReviewRoot" e72fb6a3ce1b7999f61cae1e0ed7b2773f1e4fda`。
5. 在 `$ReviewRoot` 核对 HEAD 严格等于固定 SHA、status clean；再次核对 `$SourceRepo` 的 dirty 原样保留。任一步失败即 `WORKTREE_SYNC_BLOCKED`，不得 prune/remove/reset/stash/clean。

【第二步：只读 CPU/Hello 预检】
1. 实读 AGENTS.md、CURRENT_STATE.md、SESSION_HANDOFF.md、Goal 2 复审、I0 build manifest、同批 soc.h、uart1_hello_onchip/src/main.c 和 I0-SMOKE 操作卡。
2. 核对并报告：
   - soc.h SHA-256=25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B；
   - UART1 base=0xe8011000、115200 8N1；
   - RAM=0xF9000000..0xF9003FFF；
   - Hello 只消费 SYSTEM_UART_1_*，没有旧 SYSTEM_UART_0_* 硬编码、猜 IRQ 或旧 ELF；
   - manifest 的 Hello ELF SHA-256=919B291A2B980F02507D1591EFBE5D8E395DC53F4A480AE5A8C577D74CF5F7FA；
   - APB MAGIC 只读期望=0x375A0001，地址必须来自同批 soc.h/已审查映射，不能猜。
3. 运行现有只读/Host 检查时记录完整命令和 exit code；不得因 Host PASS 推断板上 CPU、UART1 或 APB PASS。若你没有原始 ELF/bitstream，只能标记“manifest identity reviewed / artifact bytes not independently rehashed”。
4. 确认 SC_RUNTIME_ARM_ENABLED=0；UART2/J52、myCobot transport、查询和动作不属于本 Goal。

【向 qzs 提交 READY 报告】
必须逐项回传：branch、HEAD、dirty；两位来源 SHA 的祖先检查；soc.h hash与 UART1/RAM 宏；Hello 源码使用的宏；ELF manifest identity；APB MAGIC 的语义与地址来源；Host 检查结果；未独立验证项；READY 或 BLOCKED。

【libaoxun 执行窗口中的职责】
你不操作板卡，只根据原始证据实时判读：
- PC 是否在 0xF9000000..0xF9003FFF；
- Hello/echo 是否与当前源码一致，是否出现 reset/乱码/错误端口迹象；
- APB 地址是否有同批来源，读值是否严格等于 0x375A0001；
- 当前失败应归类为 USER2_FAIL、PC_RANGE_FAIL、UART1_FAIL 或 APB_MAGIC_FAIL 中哪一个。

第一处失败后只解释该层，不建议回退 UART0、换 hash/线缆继续拼证据、写业务寄存器或试探 I1-I4。若确认是 CPU/Hello 源码问题，只在新的 WSC 个人修复分支提出补丁和 Review Packet；不要改 RTL/XML/SDC/IP、冻结头文件、CURRENT_STATE 或集合分支。

【交付格式】
提交一份 CPU 判读摘要：固定 SHA、核对命令/exit code、宏与地址来源、Hello 预期、实际日志逐行解释、失败分类、仍未验证项和建议的单一下一步。禁止 UART2/J52 或 myCobot 查询/动作。
```

## 5. 下一阶段任务分工

| 阶段 | 用户 | qzs | libaoxun | wsc | 退出条件 |
|---|---|---|---|---|---|
| A. 同步 | 不操作 | 固定远端集合 SHA | 安全 ff-only 拉取并回传 clean/HEAD | 安全 ff-only 拉取并回传 clean/HEAD | 两人均为 `e72fb6a...`，否则 BLOCKED |
| B. 双人预检 | 不操作 | 比对两份 READY 报告 | 核对原始制品、Efinity、板卡、接线、停止计划 | 核对 soc.h、Hello、RAM、APB语义 | hash/身份/职责一致，否则 BLOCKED |
| C. 批准 | 明确批准或拒绝一次硬件窗口 | 形成完整批准记录并转发 | 等待批准，不提前执行 | 等待批准，保持只读 | 所有字段齐全才进入 D |
| D. 连续 I0-SMOKE | 保持可联系 | 记录 Gate 与停止点 | 唯一上板执行者：preflight→USER2/PC→UART1→APB | 实时判读 CPU/Hello/APB，不碰板卡 | 第一失败即停；全 PASS 才进入 E |
| E. 证据交接 | 确认窗口结束 | 收取并核对证据 | 交原始日志索引、截图/hash、唯一结论 | 交 CPU 判读摘要与失败分类 | qzs 能逐条回溯 0–3 checkpoint |
| F. 状态收口 | 决定是否推进下一 Gate | 更新状态候选、Goal 4 Review Packet/五行报 | 只审阅硬件事实 | 只审阅 CPU 事实 | I0-SMOKE PASS 或唯一单点 FAIL 被固定 |

## 6. qzs 接收清单与批准模板

qzs 在两位队友 READY 前不得请求用户批准。两份 READY 到齐后，必须比对：

1. 集合 SHA、batch、bitstream/ELF/`soc.h` 完整 hash；
2. 板卡 ID、Efinity/器件/timing identity、UART1 物理路由；
3. PC 合法范围、Hello 宏、APB MAGIC 地址来源；
4. UART2/J52 断开、急停/断电责任人、失败停止策略；
5. 原始证据保存位置、执行人和窗口时间。

建议用户批准记录采用以下字段，不得省略：

```text
批准 Goal 4 I0-SMOKE 单次窗口：
integration_sha=e72fb6a3ce1b7999f61cae1e0ed7b2773f1e4fda
batch=I0_UART1_20260719_CLEAN_LF_FINAL
bitstream_sha256=D05EFD4EC91FDB51F2997483BA7F4A3634C3E50FF1C65DD7239B41E19E2BC544
elf_sha256=919B291A2B980F02507D1591EFBE5D8E395DC53F4A480AE5A8C577D74CF5F7FA
soc_h_sha256=25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B
board_id=<BOARD_ID>
operator=libaoxun
reviewer=wsc
uart2_j52_disconnected=<YES_WITH_EVIDENCE>
poweroff_owner=<NAME>
stop_policy=<FIRST_FAILURE_STOP_AND_PRESERVE_EVIDENCE>
window=<START-END Asia/Shanghai>
```

这份批准只覆盖 I0-SMOKE 的固定连续链，不覆盖重新构建、换制品/接线、I1-I4、USER1、Flash、DDR、UART2/J52、机械臂查询或动作。

## 7. Goal 4 完成边界

- 全链通过：只能写 `I0-SMOKE=PASS`；I1-I4、OSD、UART2/J52、myCobot 继续 `NOT VERIFIED/NO-GO`。
- 任一失败：只写 `USER2_FAIL`、`PC_RANGE_FAIL`、`UART1_FAIL` 或 `APB_MAGIC_FAIL`，并关闭本窗口。
- hash、工具、板卡、接线或现象变化：旧窗口关闭，重新预检并获得新批准。
- 本派工文档不修改冻结接口、不授权任何机械臂动作，也不把 libaoxun 主机上的 BUILD 证据冒充 qzs/wsc 独立重建证据。
