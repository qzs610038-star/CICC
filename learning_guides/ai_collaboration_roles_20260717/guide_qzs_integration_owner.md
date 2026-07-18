# 岗位学习指南：qzs——Integration Owner、机械臂与仓库维护

> 生成日期：2026-07-17
> 面向角色：qzs / `@qzs610038-star`
> 预计阅读时间：10–15 分钟
> 角色定位来源：[BRANCH_MERGE_GOVERNANCE.md](file:///D:/第十届集创赛-雄芯院材料/docs/merge_governance/BRANCH_MERGE_GOVERNANCE.md#L93-L119)

## 先理解你的核心产出

你的第一产出不是“写了多少机械臂代码”，而是每天让团队知道：系统比昨天多证明了什么、哪条路被明确禁止、下一步由谁提供什么证据。

你像机场塔台：不替飞机设计发动机，也不替乘客开车，但要确认跑道、航班、通信和起飞许可都没有混淆。当前 `CURRENT_STATE.md` 明确禁止 UART2/J52、机械臂接线、myCobot 帧和动作；任何推进都必须先满足独立 Review Gate，不能因为比赛临近而跳过。

## 1. 你必须补齐的知识

### 第一优先级：证据和 Gate 管理

你要能区分：

- 离线构建通过 ≠ 制品已正确烧录；
- CPU Hello ≠ APB 实读；
- APB 实读 ≠ 视频统计已正确进入 CPU；
- PC/`pymycobot` 调试 ≠ 板上 CPU 正式闭环；
- Host/mock 通过 ≠ 机械臂能安全动作。

每次看 Agent 报告，先问“这条证据落在哪一级？它明确没有证明什么？”当前单摄候选的关键未验证项可直接查看 [CURRENT_STATE.md](file:///D:/第十届集创赛-雄芯院材料/CURRENT_STATE.md#L16-L57)。

### 第二优先级：接口、状态机和 Git

你不必先学会写 RTL，但要会画出：

```text
输入事实 → 接口契约 → CPU/FPGA处理 → 结果语义 → 执行门 → 证据记录
```

机械臂控制要理解 IDLE、READY、SEND、WAIT_ACK、TIMEOUT、ABORT 等状态；仓库维护要理解固定 SHA、个人分支、审查范围、语义冲突和证据失效。合并规则在 [BRANCH_MERGE_GOVERNANCE.md](file:///D:/第十届集创赛-雄芯院材料/docs/merge_governance/BRANCH_MERGE_GOVERNANCE.md#L29-L119)。

### 第三优先级：UART 与机械安全

UART0 `115200` 和 myCobot `1000000` 是两条独立链路。帧格式、超时、重试、single-flight 和急停属于安全设计，不是“调通串口后再补”的装饰。协议笔记见 [mycobot_protocol_notes.md](file:///D:/第十届集创赛-雄芯院材料/final_project/integration/mycobot_protocol_notes.md#L1-L98)。

## 2. 你应该怎样使用 Agent

### 用法 A：让 Agent 做“状态审计员”

```text
只读审计当前仓库。
请分别读取 AGENTS.md、CURRENT_STATE.md、最近 handoff 和 git status。
输出：当前真实事实、已过 Gate、未过 Gate、禁止项、队友依赖、最小下一步。
禁止修改文件，禁止给出动作命令，禁止将历史证据当作当前 PASS。
```

### 用法 B：让 Agent 做“合并审查员”

```text
审查候选 SHA ……，不要直接合并。
按 qzs / wsc / libaoxun 的职责矩阵逐文件列出：纳入、排除、冲突、证据影响。
特别检查是否覆盖 CURRENT_STATE、XML、soc.h、操作卡、机械臂安全门。
输出 CHANGES_REQUESTED / APPROVE / BLOCKED 及理由。
```

### 用法 C：让 Agent 做“文档编译器”

把已确认的命令、时间、SHA、原始输出交给 Agent，让它更新状态、handoff、Review Packet 或 PPT 提纲。禁止让它用“推测结果”填空；缺证据就写 `NOT VERIFIED`、`HOLD` 或“待队友提供”。

### 用法 D：动作相关只做计划，不发指令

涉及接线、电平、CP210x、UART2、夹爪、点位、速度和真实运动时，Agent 只能帮你整理前置检查和 Review Packet。当前快照要求用户明确确认目标、速度、角度范围、安全姿态与急停/断电方式；在此之前不执行动作。

## 3. 你每天要交付的“集成五行报”

```text
今日系统事实：
今日关闭的风险：
本轮仍未验证：
对 wsc 的明确依赖：
对 libaoxun 的明确依赖：
```

例如，不写“等待 CPU”；写“需要 wsc 提供与当前 Hard SoC 批次匹配的 ELF/入口审计，以及允许读取的正式 ABI；在此之前不猜 APB 地址”。

## 4. 合并验收清单

- [ ] 分支、HEAD、工作区状态已记录。
- [ ] 候选远端 ref、完整 SHA、纳入/排除范围已固定。
- [ ] 没有整文件覆盖 `CURRENT_STATE.md` 或他人负责的接口事实。
- [ ] 硬件原子输入变化时，旧制品和旧板测证据被标为失效/不可继承。
- [ ] 代码测试、构建、制品身份、JTAG、UART、APB、机械臂分别列出。
- [ ] 失败项保留 `NOT VERIFIED` / `HOLD`，没有被“优化措辞”抹掉。
- [ ] 更新了队友可直接执行的下一步和依赖文件。

## 5. 你不应让 Agent 替你做的事

不要让 Agent：

- 为了“让工程通过”擅自改 FPGA XML、约束、地址或生成 IP；
- 把 `final_project`、候选工程和历史制品拼成一个“看起来完整”的版本；
- 未经审查生成或发送机械臂运动命令；
- 用旧操作卡包装当前新 hash；
- 用大规模重构解决一个尚未定位的板级故障。

## 6. 推荐学习顺序

优先阅读：`AGENTS.md` 的架构/安全/审查门、`CURRENT_STATE.md` 的 Gate、[operations_runbook.md](file:///D:/第十届集创赛-雄芯院材料/docs/agent_context/operations_runbook.md#L1-L18)、合并治理第 6–8 节。然后读 `mycobot_protocol_notes.md` 的帧和事务规则。

拓展基础：Git 分支与 cherry-pick、状态机、UART 帧/校验/超时、ABI 与寄存器映射、如何写最小可复现实验。

你的学习验收标准是：能否阻止一次危险合并，能否准确写出“这条证据证明了什么/没有证明什么”，能否让两个队友在没有口头补充的情况下继续工作。
