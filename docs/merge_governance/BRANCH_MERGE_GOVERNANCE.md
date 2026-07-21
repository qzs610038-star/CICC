# 长期分支合并与职责治理

> 生效对象：`@qzs610038-star`、`@libaoxun688`、`@wsc6090-CPU` 及所有后续协作 Agent。
>
> 目标：在持续合并 Hard SoC/FPGA、CPU 和机械臂相关提交时，保持可复现工程、可审计证据和机械臂安全边界；本文件不授予任何上板或动作权限。

## 1. 不可覆盖的优先级

本文件只裁决“同一内容存在多个候选实现或结论时采用哪一来源”。它始终低于以下事实与规则：

1. 当前用户的明确指令；
2. 官方比赛细则和 `AGENTS.md` 的架构、安全、Git 规则；
3. `CURRENT_STATE.md`、真实源码、工程 XML、构建日志与板级现象；
4. 本文件中的成员职责和合并流程；
5. 旧分支、历史 handoff、旧 outflow、聊天摘要与来源副本。

来源优先级不能把未验证内容写成 PASS，也不能删除失败、HOLD、NOT VERIFIED 或相反证据。

## 2. 成员职责与冲突裁决

| 内容领域 | 优先来源 | 裁决规则 |
|---|---|---|
| FPGA、Hard SoC、IP、工程 XML、约束、顶层硬件连接、时钟/复位、JTAG、I0 UART1、APB、Efinity 构建与板级硬件结论 | `@libaoxun688` 的批准固定提交 | 采用其匹配的硬件配置与硬件结论；原子输入变化才重开对应制品 Gate，同一输入 hash 不重复确认。 |
| CPU 代码基础架构、CPU 分类/任务状态机、结果语义、固件构建策略、CPU→APB/OSD 软件契约 | `@wsc6090-CPU` 的批准固定提交 | 采用其 CPU 设计/代码意图，但必须适配已批准的 Hard SoC 硬件 ABI。 |
| myCobot 协议、动作序列、点位、夹爪、速度、互锁、超时/重试、异常停机、机械臂安全策略，以及仓库最终集成与核心约束 | `@qzs610038-star` 当前本地 `main` | 保留本地实现和安全策略；其他来源只能提出可审计补丁，不得隐式覆盖。 |

同一文件可能跨多个领域，必须按模块、接口或事实条目分段裁决；不得因“分支属于某成员”而整文件覆盖。

## 3. 来源固定与审查分支

“最新分支”不是动态名称。每次审查必须记录以下三项：

- 远端 ref；
- 完整 commit SHA；
- 提交人确认的纳入范围与排除范围。

`@wsc6090-CPU` 当前存在仅大小写不同的远端 ref，Windows 本地 Git 的 files refs 后端无法同时可靠维护它们。审查时必须用 `git ls-remote` 获取权威 SHA，并使用 SHA、临时审查分支或补丁审查；不得依赖含糊的本地 `origin/dev/wsc...` 名称，更不得迁移 refs 后端来绕过本项目流程。

所有合并均从当时的最新本地 `main` 新建审查分支。禁止直接在 `main` 合并陈旧个人分支，禁止用旧个人分支作为新基线。

## 4. `@libaoxun688` 的 FPGA / Hard SoC 责任

当前已批准审查起点为：

```text
origin/dev/libaoxun688-hard-soc-source-sync-20260716
14b924866f9df8a27e65f9719c285d23b3b8fa7e
```

该分支的 Hard SoC 配置必须按同批原子集合处理，至少包括：

```text
competition_project_single_camera/mem_test.xml
competition_project_single_camera/mem_test.peri.xml
competition_project_single_camera/constrain.sdc
competition_project_single_camera/src/top.v
competition_project_single_camera/src/apb_reg_magic.v
competition_project_single_camera/ip/EfxSapphireHpSoc_slb/**
competition_project_single_camera/embedded_sw/efx_hard_soc/**
```

不得从其他分支拼接旧 `settings.json`、`hard_ip_args.ini`、wrapper、生成 RTL、`.peri.xml`、`soc.h` 或链接/地址配置。CPU C 代码可由 `@wsc6090-CPU` 优先，但必须重新适配这组硬件 ABI，而不是反向替换它。

`@libaoxun688` 的已配置 Efinity/IP Manager 环境可作为 FPGA 构建、时序收敛复核和 JTAG SRAM 配置的主要执行环境。每一次实际构建或上板仍须提交：工程 SHA、Efinity 版本、bitstream/ELF SHA-256、Map/PNR/STA/CDC 结果、关键 warning、JTAG/USER2 证据及剩余 `NOT VERIFIED` 项。机器、工具路径、outflow、bitstream、ELF 和临时日志不进入 Git。

硬件输入发生改变（XML、IP、顶层、约束、BSP 硬件 ABI、构建工具版本或产物 hash）时，旧 bitstream、ELF 绑定和板级结果立即失效；重新建立批次后才可继续。历史批次只能作经验，不能证明合并后工程已经上板通过。

## 5. `@wsc6090-CPU` 的 CPU 与 M0 责任

`@wsc6090-CPU` 的 CPU 架构优先级仅覆盖 CPU 软件领域。例如 `c1651444d5a84be2eaa7dacb6a66d43dccfdf121` 的 CPU→APB/OSD 结果打包包是设计依据，不是 APB/OSD/CDC 或板上 CPU 已实现的证据。

M0 分支必须单独处理：

```text
handoff: 73f96cde952fe14e5e7e7206902ed2219171978b
actual candidate: 82892d3ba1251d6a1eeefbe195e60c08f6688f3d
```

`73f96c` 是交接说明，不含功能实现；`82892d3` 是唯一允许审查的 M0 补丁，禁止整支合并。其唯一 RTL 语义改动是 `dsi_tx_top.v` 的 DSI 初始化 `.mem` 路径相对化，因此它属于 FPGA 工程路径问题，最终采纳需服从 `@libaoxun688` 的硬件配置优先级。

M0 的固定审查步骤：

1. 从最新 `main` 建审查分支；
2. 以 `git cherry-pick -n 82892d3` 或等价补丁方式检查；
3. 先报告 P0/P1/P2 Findings，再给出 `APPROVE`、`CHANGES_REQUESTED` 或 `BLOCKED`；
4. `CURRENT_STATE.md`、`WORK_LOG.md`、CSV 和 Review Packet 只做时间线语义合并，禁止整文件覆盖；
5. 若原始 Map outflow/失败日志未随审查包提供，Map PASS 与 warning 计数只能保留为未独立复核的历史记录；若要写成当前项目事实，则 `BLOCKED` 并索取原始证据；
6. 不手改 `mem_test.xml`、`.peri.xml`、`constrain.sdc` 或 IP settings；不把 Map PASS 写成 M0、PNR、STA、CDC、bitstream 或板级 PASS；
7. 对最终补丁执行 `git diff --check`，并在 libaoxun 的实际 Efinity 工程语义下重测路径修复。

`dev/wsc6090-cpu-g4a-20260716` 不在上述 M0 范围内。它改变的是已退役 UART0 Hello 源码/构建，只能作为历史候选，不得进入新的 I0 UART1 批次。

## 6. `@qzs610038-star` 的机械臂与仓库治理责任

`@qzs610038-star` 的本地 `main` 是 myCobot 控制实现、机械臂安全策略、最终仓库集成和核心约束的优先来源。其范围包括协议帧、波特率、点位、动作序列、夹爪、速度限制、互锁、超时、重试、异常停机和防重复触发。

任何来源都不得借 CPU Hello、FPGA 构建或 Host/Mock 测试推断 UART2/J52/myCobot 可用。I0 UART1 的 115200 验证与 myCobot UART2 的 1000000 baud 控制是独立 Gate；未确认电平、接线、安全姿态、急停/断电方式和用户明确授权前，禁止输出机械臂动作命令。

涉及实际动作、夹爪、FPGA-to-机械臂接线/电平、CP210x 驱动或 `pymycobot` 控制脚本的补丁，必须先形成 Codex Review Packet。PC 工具仅用于调试、标定、健康检查、日志和录像，不能进入正式识别/控制闭环。

## 7. 通用合并与验收流程

1. 运行 `tools/agent_handoff_health_check.ps1`，核对 branch、HEAD、工作区和交接证据；
2. 运行 `git status --short --branch`，保留无关本地修改；
3. `git fetch --prune origin`。若 WSC 大小写 ref 冲突，记录 WARN，改用 `git ls-remote` 与精确 SHA；
4. 固定候选 ref/SHA/范围，计算 `git rev-list --left-right --count main...<sha>`，再用 `git merge-tree --write-tree` 探测文本冲突；
5. 用文件/接口/事实条目执行本文件第 2 节的领域裁决，区分文本冲突、语义冲突和证据冲突；
6. 对硬件原子配置、CPU ABI 或机械臂安全边界发生变化的候选，先写明失效的证据批次和必须重跑的 Gate；
7. 解决后运行受影响的构建、测试或静态检查。未经真实运行的项必须写 `NOT VERIFIED`、`HOLD` 或 `ASSERTS_NOT_EXECUTED`；
8. 仅提交源码、配置、脱敏文档和可复查脚本。禁止提交本机路径、许可证、临时数据库、outflow、work、bitstream、ELF、原始截图或未脱敏原始日志；
9. 在 `CURRENT_STATE.md` 和对应 Review Packet 记录采用来源、SHA、舍弃内容、证据批次、验证结果及下一 Gate。

## 8. 状态与文档规则

`CURRENT_STATE.md` 是当前状态索引，不是任一成员分支可整文件覆盖的对象。它必须保留不同制品 hash、不同构建批次、历史失败和当前门禁之间的关系。

每次合并必须明确区分：离线构建通过、制品身份通过、JTAG 传输通过、CPU 取指通过、UART 通过、APB 通过和机械臂实机通过。任何一项都不能替代另一项。

本文件由 `@qzs610038-star` 维护。修改本文件须有明确用户授权，并同时审查它是否与官方细则、`AGENTS.md`、`CURRENT_STATE.md` 和实际证据一致。
