# 仓库稳定治理入口

> 本文件只承载稳定架构边界、安全红线、审查门、文档优先级和协作规则。当前 SHA、构建批次、hash、测试/资源/warning 数与下一步状态只见 [CURRENT_STATE.md](CURRENT_STATE.md)。操作命令见 [Agent runbook](docs/agent_context/operations_runbook.md)。

## 项目与权威入口

- 正式视频/识别主线固定为 `competition_project_single_camera/` 的单摄方案；原双摄视频方案取消，只保留为历史调试资料。`final_project/` 中仍可复用 CPU、myCobot、比赛文档与经审查模块，但不得恢复双摄闭环或以旧双摄寄存器覆盖单摄接口。
- 官方任务、评分、时间和现场流程：[0710 比赛细则](final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md)。
- 当前状态、阻塞、下一 Gate、待定决策：[CURRENT_STATE.md](CURRENT_STATE.md)。
- 执行顺序与回退策略：[决赛主方案](final_project/docs/technical_plans/competition_score_maximization_execution_plan_20260712.md)。
- 会话恢复入口：[SESSION_HANDOFF.md](SESSION_HANDOFF.md)；历史状态：[state_history](debug_records/state_history/archive_manifest.md)。
- 代码图谱只用于定位；准确基线读 `.codebase-memory/artifact.json`，安全/完成度结论必须回到真实源码、XML、日志和板上现象。

## 分赛区决赛系统架构硬边界

- FPGA：MIPI/RAW/Debayer/Gamma 视频前端、ROI、基础统计特征、OSD 像素渲染，以及 AXI/UART/FIFO 等硬件通道。
- 板上 CPU：五色、三形状、三尺寸分类，四任务目标配置与关系判断，逐轮“识别—判断—执行”状态机、结果语义、参数管理和 myCobot 协议/点位/动作/互锁/超时/有限重试/异常停机。
- OSD：CPU 产生识别、目标/非目标、执行/不执行及理由；FPGA 负责渲染。不得只输出不可解释寄存器值。
- PC、外部 MCU、`pymycobot`、myBlockly 仅用于开发期调试、标定、健康检查、日志和录像，不进入正式识别/控制闭环。
- 禁止把分类、四任务关系、阈值管理、逐轮事务或机械臂动作状态机放回纯 RTL。
- 架构目标不等于已完成；任何接口、bitstream、地址或板级能力必须以 `CURRENT_STATE.md` 与当前证据为准。

## 官方比赛不可弱化约束

- 四项任务，每项五轮：指定颜色正方体；指定颜色正方体（混合形状池）；与 2cm/3cm 参考正方体边长差等于 1cm；与 2cm/3cm 目标正方体边长差不超过 0.5cm。
- 覆盖白/黑/红/蓝/黄五色，正方体/圆柱体/锥体三形状，2/2.5/3cm 三尺寸。
- 四任务实操总时限不超过十分钟；每轮必须输出识别、判断、执行或不执行及理由。
- 单轮按识别 25%、判断 25%、执行 50% 串行计分；识别错误后续不得分，正确识别后机械臂响应必须明确且唯一。
- 目标物必须轻取轻放至相对起点旋转 180°（±10°）且最大臂展处；提前释放、未夹稳并碰撞台面均按跌落。
- 随机顺序确定后不得调换；演示录像、现场签字和疑义处理以官方细则为准。

## FPGA、SoC 与证据安全红线

- 单摄 Hard SoC 的 XML、peri.xml、SDC、IP/wrapper、顶层、BSP/Hello 和 APB 必须作为原子批次审查；任一构建输入变化都会使旧 Map/PNR/STA/CDC、slack、warning、bitstream、ELF 和板级证据失效。
- 旧 bitstream/ELF 不得跨批次继承；map PASS 不等于 PNR、STA、bitstream 或板级 PASS；不同 warning 集合必须分别报告。
- 当前 Gate 与禁止项只从 `CURRENT_STATE.md` 读取。任何 USER TAP、Flash、DDR、UART 或机械臂范围扩大必须有匹配证据和独立 Review Packet。
- 不直接修改生成 IP、`ipm/`、赛方补丁、原始压缩包、波形、outflow 或历史制品。不得提交许可证、本机工具路径、临时数据库或私密板卡配置。
- 初赛 demo 仅作经验库；不得直接迁移识别 RTL、`DEMO_MODE`、临时脚本、硬编码路径或旧 outflow 结论。

## UART 与机械臂安全

- I0 CPU 生命证明固定使用 **SoC UART1 → 板载 Type-C UART1**，`115200 8N1`；物理管脚固定为 RX=`GPIOR_96/B12`、TX=`GPIOR_100/D12`。当前生成 Hard SoC 尚未启用 UART1，必须由 libaoxun 通过 Efinity 原子再生成，禁止手改 wrapper、`soc.h` 或猜测基址。
- UART0 已被烧录器链路占用，不再作为活动 I0；旧 G1/R0 UART0 bitstream、ELF、manifest、操作卡和日志只作历史证据，不得改写或继承为 UART1 PASS。
- UART1 I0 为 `115200`；myCobot UART2 为 `1000000`。两者是独立链路和独立 Gate，禁止混用或相互外推。
- 正式 myCobot 控制必须由板上 CPU 实现；FPGA 仅提供硬件通道。
- 未确认电平、线序、共地、协议和急停/断电前，禁止 FPGA 管脚接入机械臂控制线。
- 联调顺序：只读环境/端口核验 → 用户确认固定、安全姿态与急停 → 只读或极小幅测试 → 独立动作 Review Packet。
- 任何导致运动的命令都必须由用户明确确认目标、速度、角度范围与急停/断电方式；动作期间不得用盲复位伪装恢复。

## 文档优先级

1. 用户当前明确指令。
2. 官方 0710 细则与本文件安全/架构硬边界；疑似冲突立即停止扩展并请求确认。
3. `CURRENT_STATE.md` 当前事实与下一 Gate。
4. 决赛主方案的顺序和回退策略。
5. 经健康检查的 `SESSION_HANDOFF.md` / handoff 记录。
6. 历史计划、state_history、初赛资料。

真实源码、工程 XML、日志与板上现象始终是最终事实来源。

## Handoff 与矛盾报告

- 发现 handoff 后，修改前运行 `tools/agent_handoff_health_check.ps1`。默认接受有证据的 Verified Facts，但不做重型重复验证。
- 以下任一情况进入 `[Contradiction Report]`，只核查冲突事实：分支/HEAD/dirty 不符；证据缺路径/命令/时间/SHA；关键文件缺失；健康检查失败；用户指令冲突；或涉及机械臂、接线/电平、bitstream、XML/SDC、时钟复位、AXI/CDC、CPU→OSD 等安全关键边界。
- 完成首个可验证 checkpoint 且新事实已写入状态/日志后，才可按仓库规则归档 handoff；本规则不授权修改用户指定保持不变的 handoff。

## Codex 审查门

扩大修改范围前必须由 Codex 复核：跨两个以上子系统或顶层连线；时钟/复位/视频时序/AXI/framebuffer/位宽/双通道；XML/peri.xml/SDC/IP settings；QCRV32/JTAG/APB/CDC/OSD；机械臂动作/接线/驱动/控制脚本；warning 取舍；恢复纯 FPGA 决策；连续两轮同错失败；或判断高层方案是否合理可行。

Review Packet 至少包含目标、文件/diff、模块与信号、时钟/复位/CDC/双通道、命令/日志/结果/warning、未验证项、风险假设、安全状态和希望裁定的问题。模板见 [review_packet_template.md](docs/agent_context/review_packet_template.md)。

### 快速 Gate 规则

- 同一固定输入 hash 的非动作开发只批准一次连续链：离线构建/检查 → USER2 → UART1 Hello/回显 → APB MAGIC；中间不重复索取相同确认。
- 只有硬件原子输入、固件输入、制品 hash、接线或失败现象变化时才重开对应 Gate；文档排版、Host 纯软件内部实现和不改变接口的测试补充不重开硬件 Gate。
- UART2/J52、机械臂接线、查询或动作始终使用独立安全 Gate，不能由本节的快速规则放宽。

## 接口冻结与三人文件所有权

- 冻结接口、三人写入范围与最终日执行顺序见 [TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md](docs/agent_context/TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md)。
- 修改冻结接口前，用户必须明确发送完整口令：`确认接口文件修改，已经和wsc、libaoxun、qzs沟通。`。口令只授权本次 Review Packet 列明的接口差分，不授权硬件动作或机械臂动作。
- wsc 只写 CPU 实现/测试范围；libaoxun 只写单摄 FPGA/SoC 原子硬件范围；qzs 只写治理、状态、证据、工具和最终集成范围。任何跨范围修改必须先在 Review Packet 中重新分配所有权。

## Git 与合并治理

- 默认个人分支开发、PR、至少一人审查；保留无关 dirty 修改，不覆盖他人工作。
- 跨成员合并前必须阅读 [BRANCH_MERGE_GOVERNANCE.md](docs/merge_governance/BRANCH_MERGE_GOVERNANCE.md) 与 [MERGE_REGISTER.md](docs/merge_governance/MERGE_REGISTER.md)，固定 SHA 审查并保持硬件配置原子性。
- G1/G2/G3 并行时，G3 必须最后基于最终 HEAD 刷新状态与 freshness；不得从其他 dirty worktree采信事实。

## Skill 路由

- FPGA/RTL/XML/SDC/Efinity：`.agents/skills/fpga_vision/SKILL.md`。
- CPU/UART/myCobot：`.agents/skills/cpu_mycobot/SKILL.md`。
- Skill 只承载模块稳定规则，不能覆盖官方细则、本文件或 `CURRENT_STATE.md`。
