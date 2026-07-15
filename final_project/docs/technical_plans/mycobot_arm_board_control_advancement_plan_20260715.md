# myCobot 机械臂上板控制推进方案

> 日期：2026-07-15
> 状态：**Codex 复核后有条件 GO，可开启新对话开始执行**；各 Gate 仍须按 §13 的快速闭环口径逐项关闭，不代表 G4–G11 已经 PASS。
> 上位约束：官方比赛细则、`AGENTS.md`「分赛区决赛系统架构硬边界」与安全红线、`CURRENT_STATE.md`、`competition_score_maximization_execution_plan_20260712.md`（决赛主方案 v1.3-main）。
> 同级参考：`mycobot_cpu_board_bringup_implementation_plan_20260714.md`（G0–G11 详细门禁）、`mycobot_board_bringup_operator_sop_20260712.md`（操作员 SOP）、`mycobot_uart2_j52_wiring_review_20260713.md`（接线 Review Packet）。
> 转写来源：本文件由 Claude 据用户 2026-07-15 指示起草，供 Codex 复核。

## 0. 方案目的与适用范围

本方案把“机械臂上板控制”从当前 G0–G3 软件门推进到真实动作放行，按 G4→G11 顺序分层解锁，不跳安全门。

**适用范围**：
- 正式主线 `final_project/` 的 CPU 固件、FPGA 工程、SoC/APB、UART2/J52 和机械臂动作链。
- 不覆盖单摄候选工程 `competition_project_single_camera/` 的 M0 升格门。
- 不覆盖 PC 端 `pymycobot` 实验路径（仅作开发期参考）。

**不变量**：本方案允许按 §13 启动分阶段执行，但不构成跨 Gate 的一次性烧录、接线或动作授权；每次真实硬件操作仍须满足当阶段的不可逆硬件损坏防护条件。

## 1. 当前状态盘点（事实依据）

> 实时基线（2026-07-15 核查）：`main == origin/main == 64260b6`。G0–G3 软件门的合并基线为 `39e8a92`，但 `39e8a92` 不是当前 `origin/main`；本节以实时 HEAD 为准，不再把单一历史 PNR 计数（1776 或 2288）写成当前唯一事实。

### 1.1 已关闭的软件门（G0–G3）

- 16-bit `event_seq/ACK` 安全契约已进入 `origin/main@39e8a92`（后续提交推进到 `64260b6`）。
- `arm_bringup + disabled/simulated` 四组合 RISC-V 构建均 PASS。
- QEMU `_start` 入口与有界超时已验证，零 unexpected warning。
- 所有制品标记 `NOT_FOR_FLASH`，真实 UART2 transport 被源码排除。

### 1.2 硬件接线状态

- J52 接口编号、位置、四针标签顺序：**GO（文档冻结）**。
- C14 RX / F12 TX / 3.3V VCCIO：**GO（文档冻结）**。
- 正式交叉线序：**条件 GO，待断电双人复核**。
- 无电平转换直连：**条件 GO，待机械臂端空闲电平/容限验证**。

### 1.3 关键阻塞门

- **G4**：正式 SoC/BSP/PNR/部署链未闭合。历史 PNR 报告过 1776 和 2288 两个不同的未放置 I/O 计数，二者对应不同构建批次，不能合并为单一数字；当前 PNR 仍 FAIL，无可验收 bitstream/STA。
- **T0 其余项、D2 UART2 回环**未完成。
- **真实臂只读、真实动作**：NO-GO。

### 1.4 用户确认的现场条件（2026-07-15）

1. 开发板（TJ375N529）、myCobot 280 机械臂、JTAG/Programmer、UART0 串口均在手边；**分析仪/示波器暂无**。
2. Efinity 相关均已安装可用。
3. 推进顺序：先做 B（纯软件完善），再做 A（G4 攻坚）。
4. 需要安排一次固件版本核验，看看是否能采用 Python API 代码读取。
5. 接线双人复核已由用户自行安排，本方案仅在相关阶段提及。

### 1.5 设备/证据缺口（Codex 2026-07-15 指出）

由于用户暂无逻辑分析仪/示波器，以下判据**不能在本轮标 PASS**，只能关闭软件子门：

- G5 的 F12 静默证明（§7.3 要求 ELF 符号表 + 逻辑分析仪双重证明）。
- G7/T0 的空闲电压、电平、共地和收发证据。

在获得经审核的 3.3V 兼容被动采集/测量方案前，不得以代码测试代替电气事实，也不得默认现有 CH340 可安全充当接收器。保留 7/16 中午未闭环即冻结 F1 的截止线。

## 2. 推进策略：分层解锁，不跳安全门

按 `mycobot_cpu_board_bringup_implementation_plan_20260714.md` 的 G4→G11 顺序，结合决赛主方案的双 P0 主线，分四个阶段推进：

```
阶段 0：固件版本只读核验（官方 Python API / myStudio）
阶段 B：纯软件完善（不依赖硬件，立即可做）
  B1：补齐 mycobot_protocol 命令真值表和官方长度窗口校验
  B2：完善 arm_sim_transport 故障注入覆盖
  B3：为 G7 准备 UART2 回环测试套件
阶段 A：G4 攻坚（硬件条件齐备后启动）
  A0：受控 GUI/Interface Designer 资源可支持性审计（PLL/JTAG/pad/系统时钟）
  A1：审查 FPGA/SoC 当前工程和已有隔离候选
  A2：修复 Interface Designer/periphery 与顶层 IO 导出导致的 PNR/outpad 阻塞
  A3：闭合正式 SoC/BSP/PNR/部署链
阶段后续：G5 → G6 → G7 → G8 → G9 → G10 → G11
```

## 3. 阶段 0：固件版本只读核验（官方 Python API / myStudio）

**目标**：只读记录机型、控制器、产品序列、端口 HWID 和可查询的 Atom 运行版本；Basic/Arduino 侧只记录匹配机型的官方工具、构建或烧录证据，不把无法从设备读取的文件 hash 伪装成实测结果。

**方法**：先按 VID/PID、serial/HWID、设备描述和人工确认冻结端口 allowlist，再用受审的 `pymycobot.MyCobot280`/myStudio 只读查询接口读取可获得的运行版本；不使用动作、供电、servo 或 updater API。

> Codex 2026-07-15 指出：当前机型证据指向 myCobot 280 for Arduino。官方 Python API 的 `get_system_version()` 可读取运行中的 Atom 版本；`get_basic_version()` 文档针对 M5 Basic，不能预设它可证明 Arduino/Mega 侧 transponder 版本。Python 也不能从设备读出"已刷二进制文件 hash"。因此阶段 0 只做只读报告，固件刷新必须另开需用户显式批准的 Review Packet。

**任务卡**：

- [ ] 编写只读 Python 脚本，通过人工确认的端口 allowlist、波特率 1000000 调用版本查询 API；脚本不得包含动作、供电、servo 或 updater 方法。
- [ ] 记录 Atom 运行版本；Basic/Arduino 侧仅在匹配机型的官方接口确实返回有效值时记录，否则明确写 `NOT_AVAILABLE` 并引用 myStudio/Arduino 构建或烧录证据。
- [ ] 与匹配机型、控制器和产品序列的官方资料对照，只输出“无需刷新/建议另开刷新任务/证据不足”结论。
- [ ] 本阶段禁止刷新固件；如确需刷新，另开需用户显式批准的 Review Packet，记录官方包来源、兼容性、回退方法和实际烧录制品 hash。
- [ ] 输出核验报告到 `final_project/docs/debug_sessions/mycobot_firmware_check_YYYYMMDD.md`。

**安全约束**：

- 仅读取固件版本，不发送任何动作命令。
- 若脚本意外触发动作，立即断电。
- Python API 仅用于开发期核验，不进入正式闭环。

## 4. 阶段 B：纯软件完善（不依赖硬件，立即可做）

### 4.1 B1：补齐 `mycobot_protocol` 命令真值表和官方长度窗口校验

**目标**：让协议层能精确校验每条命令的 expected command、精确 payload 长度和取值域，为 G7 UART2 回环测试准备官方示例帧/边界向量。

**依据**：官方协议页 + `final_project/integration/mycobot_protocol_notes.md`。

> Codex 2026-07-15 指出：实时 `mycobot_protocol.h/.c` 已有 `0x20/0x22/0x23/0x65/0x66/0x67` 及 GET_ANGLES 精确 12-byte payload 和 signed big-endian 解码；内部 `deg_x10` 使用 raw/10，等价于协议角度 raw/100°。`expected command`、750 ms deadline、迟到/重复响应和 single-flight 不属于无状态通用帧解析器，应放到新的 UART 事务层（详细计划所述 `mycobot_uart` 或等价模块），不要污染通用 parser。

**已有能力—真实缺口—修改文件—测试入口矩阵**：

| 项目 | 已有能力 | 真实缺口 | 修改文件 | 测试入口 |
|---|---|---|---|---|
| 命令定义 | `0x20/0x22/0x23/0x65/0x66/0x67` 已存在 | 缺 `STOP 0x29`、`IS_MOVING 0x2B`、`IS_GRIPPER_MOVING 0x69` 的命令常量、payload 长度和返回值结构 | `mycobot_protocol.c/.h` | 扩展现有 `test_mycobot_arm_skeleton.c` 与 `run_mycobot_arm_skeleton_host.ps1` |
| GET_ANGLES 解码 | `mycobot_decode_get_angles_response()` 要求 12 字节并按 signed int16 解码 | 内部 `deg_x10` 会舍去百分之一度精度，需在测试中明确；负角边界覆盖不足 | `mycobot_protocol.c` | 同上 |
| 官方 LEN 窗口校验 | 通用 parser 的 `MYCOBOT_MAX_PAYLOAD=64` 宽于官方 `LEN<=0x10` | 缺命令级精确长度校验；窗口外帧只能计数丢弃，不能推进状态机 | 新增 UART 事务层（`mycobot_uart` 或等价模块） | 新事务层 Host 单测 |
| expected-command 校验 | 无 | 响应必须同时匹配 expected command 和精确 payload 长度，否则计为 bad_command 并重同步 | 新增 UART 事务层 | 同上 |
| 逐关节官方绝对限位 | `arm_controller_plan_validate()` 校验相邻点位变化和若干结构规则 | 未覆盖 J1±168/J2±135/J3±150/J4±145/J5±165/J6±180 官方绝对范围；REAL 编码前必须逐关节检查 | `arm_controller.c` 或新增限位 helper | 扩展 `test_arm_runtime.c` |

**任务卡**：

- [ ] 在 `mycobot_protocol.c/.h` 中补齐 0x29（STOP）、0x2B（IS_MOVING）、0x69（IS_GRIPPER_MOVING）的命令定义、payload 长度、返回值结构。
- [ ] 不重复实现已存在的 GET_ANGLES 解码；补充官方响应帧、负角、精确 12-byte payload 和 `deg_x10` 精度口径的回归测试，只有测试证明实现错误时才修改 helper。
- [ ] 在新增 UART 事务层中实现官方有效 `LEN=0x02..0x10` 窗口校验：窗口外帧只能计数丢弃，不能推进状态机。
- [ ] 在新增 UART 事务层中实现 expected-command 校验：响应必须同时匹配 expected command 和精确 payload 长度，否则计为 bad_command 并重同步。
- [ ] 在进入 REAL 编码前增加逐关节绝对限位 helper；任何越界角度必须明确拒绝并记录错误，不得静默 clamp 后发送。
- [ ] 扩展现有 `test_mycobot_arm_skeleton.c` 与 `run_mycobot_arm_skeleton_host.ps1`：官方示例帧、signed 边界向量、LEN 2/16/越界、错误命令/长度/域被拒绝。
- [ ] 运行回归，确保 classifier/params/matcher/round/contract/flow/A13/skeleton 基线不回退。

**PASS 判据**：

- 新增协议向量测试全部通过。
- 官方示例帧与官方文档一致。
- 错误命令/长度/域被拒绝，不推进状态机。
- 既有回归不回退。

### 4.2 B2：完善 `arm_sim_transport` 故障注入覆盖

**目标**：让模拟后端能覆盖真实动作路径的全部故障场景，使 G5 板上模拟真正有诊断价值。

> Codex 2026-07-15 指出：`arm_sim_transport` 已有 happy、read failure、soft pass、retry success、retry failure；`test_arm_runtime.c` 已覆盖这些场景、busy 拒绝和固定 20 轮。B2 应先做差分审计，只补真实缺口（如 cancel、fault 后 re-init、DONE 后下一请求、可配置 N-poll），不要仅为凑枚举重复塞进 transport。busy/fault 属于 runtime/controller 断言，不应混入 transport 故障注入。

**已有能力—真实缺口—修改文件—测试入口矩阵**：

| 项目 | 已有能力 | 真实缺口 | 修改文件 | 测试入口 |
|---|---|---|---|---|
| 故障场景枚举 | HAPPY/READ_FAILURE/SOFT_PASS/RETRY_SUCCESS/RETRY_FAILURE | 缺 PERMANENT_FAIL、CANCEL 后 re-init、DONE 后下一请求的可配置触发 | `arm_sim_transport.h/.c` | 扩展 `test_arm_runtime.c` 与 `run_arm_runtime_host.ps1` |
| 可配置 N-poll 收敛 | `set_settle_reads()` 提供固定收敛 | 缺"N 次轮询后收敛"的显式可配置场景 | 同上 | 同上 |
| busy/fault 断言 | `test_arm_runtime.c` 已覆盖 busy 拒绝和 twenty rounds | 无需在 transport 重复 | — | — |

**任务卡**：

- [ ] 先冻结现有 HAPPY/READ_FAILURE/SOFT_PASS/RETRY_SUCCESS/RETRY_FAILURE 行为，只在差分测试证明缺口后补 permanent fail 或可配置 N-poll；不得为凑枚举重复实现已有场景。
- [ ] cancel、fault 后 re-init、DONE 后下一请求在 runtime/controller 测试中补断言；busy/fault 不作为 transport 故障枚举。
- [ ] 快速最小闭环中 B2 可后置到 G4/G7 之后；进入 G10 前至少保留 happy、read failure、retry failure、cancel/fault fail-closed 和固定 20 轮证据。
- [ ] 编写/扩展 Host 单测覆盖上述全部场景。
- [ ] 运行回归，确保 disabled/simulated 四组合基线不回退。

**PASS 判据**：

- 全部故障场景在 Host 下可复现且行为符合预期。
- 失败停线、故障证据保留。
- 既有回归不回退。

### 4.3 B3：为 G7 准备 UART2 回环测试套件

**目标**：在 G7 硬件就绪前，先把官方示例帧/边界向量的测试数据准备好，使 G7 能直接跑。

> Codex 2026-07-15 指出：B3 使用统一 ±180°/±165° 不符合官方逐关节限制；GET_ANGLES 命令长度语义也混淆。官方协议页给出的 myCobot 280 关节范围是 J1 ±168、J2 ±135、J3 ±150、J4 ±145、J5 ±165、J6 ±180；不同固件/API 版本还可能有差异。GET_ANGLES 请求 `LEN=0x02`，响应为 `LEN=0x0E`；`LEN=0x10` 只可作为通用帧窗口测试，不能当作合法 GET_ANGLES 响应。命令级测试必须使用该命令的精确长度；通用 parser 才测 `0x02..0x10`。

**已有能力—真实缺口—修改文件—测试入口矩阵**：

| 项目 | 已有能力 | 真实缺口 | 修改文件 | 测试入口 |
|---|---|---|---|---|
| 官方示例帧 | `test_mycobot_arm_skeleton.c` 已有 frame build/parse | 缺 GET_ANGLES 请求帧 + 响应帧的官方示例对照 | 新增测试 fixture 或扩展 `test_mycobot_arm_skeleton.c` | `run_mycobot_arm_skeleton_host.ps1` |
| 逐关节边界向量 | 无 | 需按 J1±168/J2±135/J3±150/J4±145/J5±165/J6±180 构造每轴 valid-min/max 和越界拒绝向量 | 同上 | 同上 |
| LEN 边界向量 | 通用 parser 的 `MYCOBOT_MAX_PAYLOAD=64` | 需 `LEN=0x02`（最小有效）、`LEN=0x10`（最大有效）、`LEN=0x01`（过小）、`LEN=0x11`（过大）作为通用帧窗口测试 | 同上 | 同上 |
| 错误帧向量 | `test_transport_rx_bad_footer_resync` 等已覆盖部分 | 需补错误命令/长度/尾字节/迟到/重复/未知帧向量 | 同上 | 同上 |

**任务卡**：

- [ ] 从官方协议页提取 GET_ANGLES 的官方示例帧（请求帧 + 响应帧）。
- [ ] 按 J1±168/J2±135/J3±150/J4±145/J5±165/J6±180 构造每轴 valid-min/max 和越界拒绝向量。
- [ ] 构造 LEN 边界向量：`LEN=0x02`（最小有效）、`LEN=0x10`（最大有效）、`LEN=0x01`（过小）、`LEN=0x11`（过大），作为通用帧窗口测试。
- [ ] 构造错误命令/长度/尾字节/迟到/重复/未知帧向量。
- [ ] 把上述向量组织成 C 数组或 Host 测试 fixture，供 G7 直接调用。
- [ ] 运行回归，确保既有测试不回退。

**PASS 判据**：

- 官方示例帧与官方文档完全一致。
- 边界向量覆盖 J1..J6 全关节、LEN 全窗口。
- 错误帧向量覆盖全部异常类型。
- 既有回归不回退。

## 5. 阶段 A：G4 攻坚（硬件条件齐备后启动）

### 5.1 A0：受控 GUI/Interface Designer 资源可支持性审计

> Codex 2026-07-15 指出：G4 漏了真正的首个资源决策门。`CURRENT_STATE.md` 与 `a14_soc_pll_replanning_decision_gate_20260712.md` 已记录：视频工程占用 `PLL_BL0/1/2`，候选硬 SoC system PLL、JTAG/BSCAN 和 pads 是否还能合法落位尚未由 Interface Designer/GUI 证实。现方案直接跳到"修 PNR/outpad"，会在架构可支持性未判定时盲改 XML/top/SDC。

**任务卡**：

- [ ] 在受控副本中打开 Efinity Interface Designer，逐项记录 PLL bank、system clock、JTAG/BSCAN、UART0/pad 的合法组合。
- [ ] 确认硬 SoC system PLL 是否有未占用的合法候选（官方硬 SoC IP 的 `PLL_SOC_SYS_RESOURCE` 仅允许 `PLL_BL0/PLL_BL1/PLL_BL2`，且三者均已占用）。
- [ ] 确认 JTAG 可改为 `JTAG_USER2`、外设 PLL 可改至其它合法资源。
- [ ] 只有 A0 给出可复现 GO，才允许做 periphery/top 对齐和联合 PNR。
- [ ] 禁止猜 pin、fabric 分频代替 system clock、手改生成 XML。

**PASS 判据**：

- Interface Designer/GUI 给出 PLL/JTAG/pad/系统时钟的合法组合证据。
- 硬 SoC system PLL 有未占用合法候选，或书面确认"不支持"并转入降级路径。

### 5.2 A1：审查 FPGA/SoC 当前工程和已有隔离候选

**任务卡**：

- [ ] 审查 `final_project/fpga/efinity/mem_test.xml`、`.peri.xml`、`constrain.sdc`、顶层 `top.v`。
- [ ] 审查已有隔离候选 `competition_project_single_camera/`（仅作只读参考，不能替代 `final_project/` 的 G4 真源）。
- [ ] 不得把候选 `soc.h` 直接冒充 final_project 真源。
- [ ] 设计或审查最小 QCRV32 + UART0 + CLINT + JTAG/BSCAN 配置。
- [ ] 输出审查报告，明确 G4 的 SoC/PNR/JTAG/UART0 条件是否齐备。

### 5.3 A2：复现并分类 PNR/outpad 阻塞，再按证据修复

> Codex 2026-07-15 指出：A2 预设了根因，且"D 盘 build/flash 树"没有唯一来源。当前证据只支持 Interface Designer/periphery/top/VDB 不一致是待验证假设，不能先宣布它就是 outpad 根因；不明来源构建树更不能成为烧录真源。

**任务卡**：

- [ ] 在受控副本中记录 Efinity 版本、项目 XML、顶层、`.peri.xml`、SDC 和实际 top 端口数量。
- [ ] 复现当前 map，保存命令和时间戳。
- [ ] 区分两条构建路径：生产非调试 PNR；Debug Wizard 生成 `.dbg.vdb` 后的调试 PNR。
- [ ] 检查未放置 I/O 的来源：真实顶层端口、展开数组、错误包装器还是不匹配 VDB。
- [ ] 只有在对照树的绝对路径、commit/hash、Efinity 版本、生成批次和只读/可部署属性已登记时，才比较其 `top.v`、`mem_test.xml`、`.peri.xml` 和约束；来源不明的 D 盘树跳过或仅作 forensic reference。
- [ ] 形成 Review Packet 后，再决定是否修改 `mem_test.xml`、`.peri.xml` 或 `constrain.sdc`。
- [ ] 涉及 XML、SDC、顶层或时钟复位时，单独形成 Review Packet。

**禁止的快捷修复**：

- 不给"未约束 I/O"直接批量分配管脚。
- 不手工伪造或复制 `.dbg.vdb`。
- 不在未比较的 D 盘烧录树上覆盖 `top.v` 或 `mem_test.xml`。
- 不把 map PASS 描述为 PNR、时序、bitstream 或真实视频 PASS。

### 5.4 A3：闭合正式 SoC/BSP/PNR/部署链

**任务卡**：

- [ ] 把生成 BSP、linker、startup、debug profile 和来源说明放入受控目录或提供稳定引用。
- [ ] 将 deploy 占位替换为经过验证的 dry-run/实际命令分离流程。
- [ ] 不把"已有 `soc.h`"当成 BSP 完成：逐项核对 UART0、CLINT、PLIC、内存起点/长度、linker 中 `_sidata` 与 `.data/.bss`、栈顶、startup 的 `mtvec`/trap 行为和中断路径。
- [ ] 当前 provisional startup 的异常/中断路径不能直接作为正式可恢复运行证据。

**用户操作**：

- [ ] 执行需要 GUI 的 SoC 生成、map/PNR/STA、Programmer/JTAG 操作并保存日志。
- [ ] 每次操作前确认机械臂和 J52 信号线断开。

**PASS 判据**：

- production/bring-up 对应的 map、PNR、STA 均有可验收日志。
- bitstream、`soc.h`、linker、ELF 来自同一可追溯配置。
- 连续 3 次复位都有 CPU Hello。
- Programmer/JTAG 操作可重复，不靠临时 GUI 状态。

**STOP 条件**：PNR/STA 失败、地址冲突、CPU Hello 不稳定或生成批次混用时，不烧 arm simulated ELF。

## 6. 阶段后续：G5 → G6 → G7 → G8 → G9 → G10 → G11

按 `mycobot_cpu_board_bringup_implementation_plan_20260714.md` 的 §8 顺序逐门推进，每门 PASS 后才进入下一门。关键节点：

- **G5**（机械臂断开的板上模拟）：J52 断开，烧 simulated ELF，3 次复位 + 20 轮 + 10 分钟 + F12 静默。
- **G6**（competition + simulated 整链干跑）：四任务 20 轮、≤10 分钟、识别/判断/理由锁存正确，非目标零动作，目标由模拟后端各完成一次，OSD/UART/记录一致。
- **G7**（UART2 无机械臂回环）：用 B3 准备的测试套件，100/100 帧。
- **G8**（真实机械臂只读）：30/30 次 GET_ANGLES 响应正确。
- **G9**（PLIC/Ring Buffer 与 readonly dry-run）：ISR 只 claim/read/push/count/complete；主循环解析；比较轮询/中断结果。
- **G10**（首次低速空载单段）：HOME → home_ready，低速、空载、不闭夹爪。
- **G11**（F2 抓放与比赛启用）：补齐夹爪确认，逐级推进到整场演练。

> Codex 2026-07-15 指出：§2 和 §6 原写 `G5 → G7 → G8 → G10 → G11`，遗漏了 G6 和 G9。G6 是比赛语义 + simulated 全链 dry-run；G9 是 PLIC/ring-buffer 只读 dry-run。即使最后选择有界 polling 降级，也必须在 G9 明确评审和记录，不能静默删门。G10 必须以 G0–G9 全部满足或书面批准的降级门为前提。

## 7. 安全不变量（不可违反）

1. 默认构建必须是机械臂禁用；任何未明确选择后端的构建均不得包含真实 UART2 动作路径。
2. `STANDALONE_TEST=1`、`0xF0000000`、当前 `bsp.h` 硬编码地址或临时 linker 生成物禁止烧录。
3. `DISABLED` 和 `SIMULATED` 模式不得初始化 UART2，不得向 F12 写任何字节。正式完整验收仍以 ELF 符号表和逻辑分析仪双重证明为目标；快速最小闭环可仅按 §13 的 G5 降级门放行，但机械臂及 J52 四线必须物理全断开。
4. 动作请求的唯一合法路径是 `main -> round_controller -> arm_runtime -> arm_controller -> backend`。禁止 `task_matcher`、调试命令或 OSD 逻辑直通 `arm_controller_request_grab()`。
5. 同一轮最多产生一个 `request_arm_grab` 脉冲；busy、fault、重复/过期事件、非法状态和非目标轮必须为零动作请求。
6. UART0/Type-C 仅作 CPU 启动与日志控制台；UART2/J52 才是未来机械臂链，二者不得复用。
7. 真实 UART 适配的所有 TX/RX 等待均有界；ISR 只搬运字节和计数，不解析协议、不打印、不推进动作状态机。
8. 通信故障、到位失败或急停后默认停止新命令并保持故障证据；不得默认自动释放扭矩。
9. J52 Pin4 VCC 永久悬空；机械臂独立 12V 5A 供电。没有电平、共地和线序双签时不得连接三根信号线。
10. PC、myBlockly 和 `pymycobot` 只用于学习、示教、健康检查和证据采集，不进入比赛闭环。
11. build backend 在固件生命周期内不可变；`real` 即使被编进 ELF，上电后也必须保持 `real_armed=0`，只有明确操作员事件和运行时安全检查通过后才能置 1，复位、fault、ESTOP 或通信异常立即清零。
12. `readonly` 模式必须向 `round_controller` 报告 `arm_enabled=0`，真实 UART 只由独立只读服务发起 GET_ANGLES；不得因为 UART 可收发就让逐轮控制器产生动作请求。
13. 真实臂只读前必须确认具体机型/控制器、Atom `atomMain` 运行版本，以及 Arduino/Basic 侧处于匹配的通信模式；只对实际下载/烧录的制品记录 hash，设备无法回读的 hash 写 `NOT_AVAILABLE`。证据不足时不得把无响应直接归因于板侧 UART。
14. 协议没有事务序号，所有带返回值命令必须 single-flight；响应同时匹配 expected command、精确 payload 长度和取值域。首版响应期限为 750 ms，未完成或未重同步前不得发送下一请求。
15. `SEND_ANGLES`、夹爪设置和 `STOP` 均无协议 ACK；TX 完成绝不等于动作完成、夹持成功或停止成功。真实状态必须由独立查询和物理观测确认，人工断电/急停仍是最终保护。

## 8. 回退链与截止线

**最优回退链**：

```
real -> readonly -> simulated -> disabled/F1
```

每一级都必须有独立、可识别的 manifest 和制品，禁止通过现场改宏但不重建/不记录来切换模式。

**截止线**（来自决赛主方案）：

- 7/16 中午：板到臂 UART 电气、回环、无运动/只读验证通过 → 否则冻结 F1 无机械臂。
- 7/16 晚：F1 连续 20 轮无死锁、无重复动作且 ≤10 分钟。
- 7/17：基础保底冻结，不再扩大架构。

## 9. 接线双人复核（用户已安排）

J52 正式交叉线序和无电平转换直连均为“条件 GO”。断电双人复核只能关闭针序、方向、VCC 悬空和共地相关行，不能自动关闭 SoC、APB、UART2 MMIO/PLIC、ESTOP、机械臂电平等八项。结果必须在 `final_project/integration/io_pin_map.md` 中逐行引用独立证据；快速闭环的仪器降级按 §13 执行。

## 10. 希望 Codex 判断的问题

1. 阶段 B 的三项纯软件任务（B1 协议真值表、B2 模拟故障注入、B3 回环测试套件）是否与现有 G0–G3 软件基线兼容？是否存在重复实现或与 `mycobot_protocol_notes.md` 冲突的口径？
2. 阶段 A 的 G4 攻坚顺序（A1 审查 → A2 修 PNR → A3 闭合部署链）是否合理？是否有遗漏的前置门？
3. 本方案与 `mycobot_cpu_board_bringup_implementation_plan_20260714.md` 是否存在冲突？若有，以哪份为准？
4. 固件版本核验（阶段 0）用 Python API 读取是否足够？是否还需要物理 JTAG 读取 Atom 固件版本？

## 11. Codex 独立核查结论与强制修正项（2026-07-15）

> 核查身份：Codex；依据为 2026-07-15 实时 `main`、`CURRENT_STATE.md`、G0–G11 详细计划、真实 CPU/FPGA 文件、现有测试以及大象机器人官方协议/API 文档。
>
> **结论：BLOCK。** 本方案可保留为推进方向草案，但在下列 P0 项修正并重新提交 Review Packet 前，不得据此执行 SoC/PNR 修复、固件刷新、J52 接线、G7/G8 或任何真实动作。本节只是审查意见，不构成烧录、接线、固件刷新或动作授权。

### 11.1 阻塞问题与修正要求

| 优先级 | 问题与证据 | Claude 必须如何修正 |
|---|---|---|
| P0 | **基线和 PNR 数量已过期。** 实时仓库为 `main@64260b6`，`39e8a92` 只是 G0–G3 合并基线，不是当前 `origin/main`。`CURRENT_STATE.md` 的历史快照分别出现过 1776 和 2288 个未放置 I/O，不能把 1776 写成当前唯一事实。 | 把 §1 改成“当前 HEAD + G0–G3 基线”双字段；以待审 commit 的同批次 map/PNR 日志重新统计端口和错误，不再硬编码 1776。保留“map PASS 不等于 PNR/STA/bitstream PASS”。 |
| P0 | **G4 漏了真正的首个资源决策门。** `CURRENT_STATE.md` 与 `a14_soc_pll_replanning_decision_gate_20260712.md` 已记录：视频工程占用 `PLL_BL0/1/2`，候选硬 SoC system PLL、JTAG/BSCAN 和 pads 是否还能合法落位尚未由 Interface Designer/GUI 证实。现方案直接跳到“修 PNR/outpad”，会在架构可支持性未判定时盲改 XML/top/SDC。 | 在 A1 前增加 **A0：受控 GUI/Interface Designer 资源可支持性审计**，逐项记录 PLL bank、system clock、JTAG/BSCAN、UART0/pad 的合法组合。只有 A0 给出可复现 GO，才允许做 periphery/top 对齐和联合 PNR。禁止猜 pin、fabric 分频代替 system clock、手改生成 XML。 |
| P0 | **A1 依赖了不存在且不受控的本机候选。** 实时检查 `C:\fpga_soc_isolated\tj375_video_soc_gui_a8\` 不存在；同时 §0 声明不覆盖单摄候选，A1 又未说明仅作参考。 | 不把缺失的 C 盘目录写成执行依赖。若确需候选，先从可追溯来源新建受控副本并记录 hash/来源；`competition_project_single_camera/` 只能作只读参考，不能替代 `final_project/` 的 G4 真源。 |
| P0 | **固件“读取、判定、刷新”混成了一张任务卡，且能力表述过度。** 当前机型证据指向 myCobot 280 for Arduino。官方 Python API 的 `get_system_version()`可读取运行中的 Atom 版本；`get_basic_version()`文档针对 M5 Basic，不能预设它可证明 Arduino/Mega 侧 transponder 版本。Python 也不能从设备读出“已刷二进制文件 hash”。 | 阶段 0 只做只读报告：先冻结具体机型、控制器、产品序列、端口 HWID，再用受审脚本调用版本查询。Atom 版本只需官方 API/myStudio，不需要物理 JTAG。Basic/Arduino 侧版本证据应来自匹配机型的 myStudio/Arduino 构建或官方烧录记录；hash 只能计算“实际下载并烧录的官方文件”。任何刷新固件必须拆成新的、需用户显式批准的任务，含官方包来源、兼容性、备份/回退、断开开发板/J52和独立 Review Packet。 |
| P0 | **B1 重复实现并把事务状态放错层。** 实时 `mycobot_protocol.h/.c` 已有 `0x20/0x22/0x23/0x65/0x66/0x67`，已有 GET_ANGLES 精确 12-byte payload、signed big-endian 解码；内部 `deg_x10` 使用 raw/10，等价于协议角度 raw/100°。`expected command`、750 ms deadline、迟到/重复响应和 single-flight 不属于无状态通用帧解析器。 | 先列“已有/缺失”差分，只补 `STOP 0x29`、`IS_MOVING 0x2B`、`IS_GRIPPER_MOVING 0x69`、对应解码和真正缺口。把 expected-command/deadline/single-flight/重同步计数放到新的 UART 事务层（详细计划所述 `mycobot_uart` 或等价模块），不要污染通用 parser。 |
| P0 | **B3 的关节和 LEN 边界会把非法动作值当作合法值。** 官方协议页给出的 myCobot 280 关节范围不是每轴统一 ±180°/±165°，而是 J1 ±168、J2 ±135、J3 ±150、J4 ±145、J5 ±165、J6 ±180；不同固件/API 版本还可能有差异。GET_ANGLES 请求 `LEN=0x02`，响应为 `LEN=0x0E`；`LEN=0x10` 只可作为通用帧窗口测试，不能当作合法 GET_ANGLES 响应。 | 把“字节级 signed-int16 边界测试”和“关节物理域测试”分开。先按已确认机型/固件冻结保守的逐关节表，再生成每轴 valid-min/max、越界拒绝向量。命令级测试必须使用该命令的精确长度；通用 parser 才测 `0x02..0x10`。当前 `SEND_ANGLES` 不能静默 clamp 越界值；进入 real backend 前必须改为明确拒绝并留证。 |
| P0 | **阶段序列跳过 G6 和 G9，与权威详细计划冲突。** §2 和 §6 写成 `G5 → G7 → G8 → G10 → G11`，但权威顺序是 `G5 → G6 → G7 → G8 → G9 → G10 → G11`。 | 恢复完整顺序。G6 是比赛语义 + simulated 全链 dry-run；G9 是 PLIC/ring-buffer 只读 dry-run。即使最后选择有界 polling 降级，也必须在 G9 明确评审和记录，不能静默删门。G10 必须以 G0–G9 全部满足或书面批准的降级门为前提。 |
| P0 | **当前现场证据能力不能满足方案自己的 G5/G7 判据。** 用户明确暂无逻辑分析仪/示波器，而 §7.3 要求 ELF 符号表 + 逻辑分析仪双重证明 F12 静默；G7/T0 还需要空闲电压、电平、共地和收发证据。 | 在计划中显式列“设备/证据缺口”，不得以代码测试代替电气事实。获得经审核的 3.3 V 兼容被动采集/测量方案前，只能关闭软件子门，不能把 F12 静默、T0 或 G7 标 PASS；不得默认现有 CH340 可安全充当接收器。保留 7/16 中午未闭环即冻结 F1 的截止线。 |
| P1 | **B2 大量重复现有实现，并混淆 transport 故障和 runtime 状态。** `arm_sim_transport` 已有 happy、read failure、soft pass、retry success、retry failure；`test_arm_runtime.c` 已覆盖这些场景、busy 拒绝和固定 20 轮。 | 先生成场景 gap matrix，再只补真实缺口，例如 cancel、fault 后 re-init、DONE 后下一请求、可配置 N-poll（若现测确实缺失）。busy/fault 属于 runtime/controller 断言，不要仅为凑枚举重复塞进 transport。B 阶段应 time-box，避免重复软件工作拖延 G4。 |
| P1 | **新增测试文件没有执行入口。** 方案提出 `test_mycobot_protocol_vectors.c`，却没有说明由哪个 Host runner、RISC-V compile-only/manifest 或总回归调用，可能形成“文件存在但从未运行”。 | 优先扩展现有 `final_project/cpu/tests/test_mycobot_arm_skeleton.c` 与 `run_mycobot_arm_skeleton_host.ps1`；若新建文件，必须同时增加可复现 runner、纳入回归清单，并记录命令与退出码。 |
| P1 | **T0 与串口身份仍可能被误判。** 接线双人复核只能关闭线序相关行，不能自动关闭 SoC、APB、UART2 MMIO/PLIC、ESTOP、机械臂物理等八项。旧记录曾用 COM9，当前只读枚举看到 CH340 为 COM10，端口号会漂移。 | `io_pin_map.md` 八行逐行引用独立证据，不得用一次双签整体 PASS。阶段 0 先记录 VID/PID、serial/HWID、描述和人工确认的 allowlist；禁止仅凭“当前 COM 号”自动连接。版本读取脚本只允许经审计的查询 API，不含 updater、servo/power/motion 方法。 |
| P1 | **A2 预设了根因，且“D 盘 build/flash 树”没有唯一来源。** 当前证据只支持 Interface Designer/periphery/top/VDB 不一致是待验证假设，不能先宣布它就是 outpad 根因；不明来源构建树更不能成为烧录真源。 | A2 先复现并分类当前 PNR 报告、Missing Interface Pins/AXI、top/periphery 端口数量和生成批次，再依据证据决定修复点。列出每棵对照树的绝对路径、commit/hash、生成版本和只读/可部署属性；来源不明的一律仅作 forensic reference。 |

### 11.2 对 §10 四个问题的明确答复

1. **B 阶段是否兼容：部分兼容，但必须改写。** B1/B3 可作为 G7 前的软件准备，不过已有常量和 GET_ANGLES 解码不能重复实现，事务校验应独立成 UART 事务层，边界向量必须按逐关节/逐命令语义重做。B2 现有覆盖已较完整，应先做差分审计，仅补缺口。
2. **A 阶段顺序是否合理：不完整。** 正确顺序应为：冻结实时 baseline 与证据批次 → A0 GUI/Interface Designer 资源可支持性门（PLL/JTAG/pads/UART0）→ 复现并分类当前 map/PNR → 对齐 periphery/top/约束 → 冻结 APB/UART 契约 → 同批次生成 SoC/BSP/bitstream/ELF → PNR/STA/三次 CPU Hello。A0 未 GO 时，不进入 A2 修改。
3. **与 20260714 详细计划冲突时谁优先：** `mycobot_cpu_board_bringup_implementation_plan_20260714.md` 继续作为 G0–G11 具体门禁真源；本文件只能作为其推进补充。若两者与 `CURRENT_STATE.md` 或实时源码/日志冲突，按 `AGENTS.md` 优先级，以官方细则/安全红线、实时状态和真实证据为准，并先修文档再执行。
4. **版本核验是否需要 JTAG：** Atom 运行版本的只读核验使用官方 `get_system_version()`/myStudio 即可，不需要为了“读版本”做物理 JTAG。`get_basic_version()`能否用于本机 Arduino 控制器不能预设；应按匹配机型的官方工具/烧录记录取证。设备无法回读已刷文件 hash，hash 只对实际下载/烧录的制品计算。固件刷新不是本次核验的默认后续动作。

官方交叉证据：

- [myCobot 280 串口协议](https://docs.elephantrobotics.com/docs/mycobot_280_ar_cn/3-FunctionsAndApplications/6.developmentGuide/CommunicationProtocolPackage/18-communication.html)：1,000,000 8N1、帧长度窗口、GET_ANGLES 示例与逐关节范围。
- [myCobot 280 Python API](https://docs.elephantrobotics.com/docs/mycobot_280_ar_cn/3-FunctionsAndApplications/6.developmentGuide/python/7.2_API.html)：`get_system_version()` 与 `get_basic_version()` 的适用说明。

### 11.3 Claude 修正后回传清单

- [ ] 更新实时 HEAD/G0–G3 基线/PNR 证据三者的口径，不再沿用单一 1776 数字。
- [ ] 加入 A0 PLL/JTAG/pad/系统时钟资源支持性门，并删除缺失本机目录的硬依赖。
- [ ] 提交 B1/B2/B3 “已有能力—真实缺口—修改文件—测试入口”矩阵；明确逐关节限制、命令精确长度和越界拒绝策略。
- [ ] 恢复 G5→G6→G7→G8→G9→G10→G11 全门序列。
- [ ] 列出暂无分析仪/示波器导致的证据缺口、补齐方式和 F1 截止回退，不降低 T0/G5/G7 判据。
- [ ] 把阶段 0 改成只读版本报告；固件刷新另开需用户显式批准的 Review Packet。
- [ ] 对每项 P0 给出 `已修正/不同意（附真实证据）/仍阻塞`，不得只回复“已知悉”。

在以上清单完成并经 Codex 复核前，维持 **G4/G7/G8/G10/G11 NO-GO，固件刷新 NO-GO，J52 信号线保持断开**。

## 12. Claude 对 Codex 审查意见的逐项回应（2026-07-15）

> 本节由 Claude 据 2026-07-15 实时仓库核查结果起草，供 Codex 复核。所有"已修正"均对应前文相应章节的实际改动。

### 12.1 P0 阻塞项回应

| # | Codex 意见 | Claude 核查 | 处置 |
|---|---|---|---|
| P0-1 | 基线 39e8a92 过期，1776 不是当前唯一事实 | ✅ 属实。`git rev-parse HEAD == origin/main == 64260b6`；CURRENT_STATE 历史 PNR 计数有 1776 和 2288 两个快照 | **已修正**。§1 开头改为"实时基线（2026-07-15 核查）：main == origin/main == 64260b6"；§1.3 改为"历史 PNR 报告过 1776 和 2288 两个不同的未放置 I/O 计数，二者对应不同构建批次，不能合并为单一数字"。 |
| P0-2 | G4 漏了首个资源决策门（PLL/JTAG/pad） | ✅ 合理。视频工程占用 PLL_BL0/1/2，硬 SoC system PLL 是否能合法落位未经 Interface Designer/GUI 证实 | **已修正**。§5 新增 §5.1 A0：受控 GUI/Interface Designer 资源可支持性审计；§2 推进策略加入 A0。A0 未 GO 时不进入 A2 修改。 |
| P0-3 | A1 依赖不存在的 C 盘候选目录 | ✅ 属实。`ls -d C:/fpga_soc_isolated/tj375_video_soc_gui_a8` 返回不存在；`C:/fpga_soc_isolated/` 整个路径不存在 | **已修正**。§5.2 A1 删除对 `C:\fpga_soc_isolated\tj375_video_soc_gui_a8\` 的硬依赖；改为"`competition_project_single_camera/` 仅作只读参考，不能替代 `final_project/` 的 G4 真源"。 |
| P0-4 | 固件"读取、判定、刷新"混成一张卡，能力表述过度 | ✅ 合理。Python 不能从设备读出"已刷二进制文件 hash"；`get_basic_version()` 文档针对 M5 Basic，不能预设它可证明 Arduino/Mega 侧 transponder 版本 | **已修正**。§3 标题改为"固件版本只读核验"；任务卡拆分为"先冻结具体机型、控制器、产品序列、端口 HWID，再用受审脚本调用版本查询"；固件刷新明确"必须拆成新的、需用户显式批准的任务"。 |
| P0-5 | B1 重复现有协议实现，事务状态放错层 | ✅ 属实。`mycobot_protocol.h` 已有 0x20/0x22/0x23/0x65/0x66/0x67 及 GET_ANGLES 精确 12-byte payload、signed big-endian 解码；内部 deg_x10 使用 raw/10，等价于协议角度 raw/100° | **已修正**。§4.1 改为"已有能力—真实缺口—修改文件—测试入口"矩阵；expected-command/deadline/single-flight 明确放到新增 UART 事务层，不污染通用 parser。 |
| P0-6 | B3 用统一 ±180°/±165° 不符合官方逐关节限制；GET_ANGLES 命令长度语义混淆 | ✅ 属实。`mycobot_protocol_notes.md` 已记录 J1±168/J2±135/J3±150/J4±145/J5±165/J6±180；GET_ANGLES 请求 LEN=0x02、响应 LEN=0x0E | **已修正**。§4.3 改为按 J1±168/J2±135/J3±150/J4±145/J5±165/J6±180 构造每轴 valid-min/max 和越界拒绝向量；LEN 边界向量作为通用帧窗口测试，命令级测试使用该命令的精确长度。 |
| P0-7 | 阶段顺序跳过 G6 和 G9 | ✅ 属实。§2 和 §6 原写 G5→G7→G8→G10→G11，权威顺序是 G5→G6→G7→G8→G9→G10→G11 | **已修正**。§2 推进策略恢复完整 G 门序列；§6 标题改为"G5 → G6 → G7 → G8 → G9 → G10 → G11"，并补全 G6（比赛语义 + simulated 全链 dry-run）和 G9（PLIC/ring-buffer 只读 dry-run）节点。 |
| P0-8 | 当前现场证据能力不能满足方案自己的 G5/G7 判据 | ✅ 属实。用户明确暂无逻辑分析仪/示波器 | **已修正**。§1.5 新增"设备/证据缺口"小节，明确"在获得经审核的 3.3V 兼容被动采集/测量方案前，只能关闭软件子门，不能把 F12 静默、T0 或 G7 标 PASS；不得默认现有 CH340 可安全充当接收器"；保留 7/16 中午未闭环即冻结 F1 的截止线。 |

### 12.2 P1 改进项回应

| # | Codex 意见 | Claude 核查 | 处置 |
|---|---|---|---|
| P1-1 | B2 大量重复现有实现，并混淆 transport 故障和 runtime 状态 | ✅ 属实。`arm_sim_transport.h` 已有 HAPPY/READ_FAILURE/SOFT_PASS/RETRY_SUCCESS/RETRY_FAILURE；`test_arm_runtime.c` 已覆盖这些场景、busy 拒绝和固定 20 轮 | **已修正**。§4.2 改为"已有能力—真实缺口—修改文件—测试入口"矩阵；明确 busy/fault 属于 runtime/controller 断言，不应混入 transport 故障注入；B2 应 time-box，避免重复软件工作拖延 G4。 |
| P1-2 | 新增测试文件没有执行入口 | ✅ 合理。方案原提出 `test_mycobot_protocol_vectors.c`，却没有说明由哪个 Host runner、RISC-V compile-only/manifest 或总回归调用 | **已修正**。§4.1、§4.2、§4.3 的测试入口列改为"优先扩展现有 `final_project/cpu/tests/test_mycobot_arm_skeleton.c` 与 `run_mycobot_arm_skeleton_host.ps1`；若新建文件，必须同时增加可复现 runner、纳入回归清单，并记录命令与退出码"。 |
| P1-3 | T0 与串口身份仍可能被误判 | ✅ 合理。接线双人复核只能关闭线序相关行，不能自动关闭 SoC、APB、UART2 MMIO/PLIC、ESTOP、机械臂物理等八项；旧记录曾用 COM9，当前只读枚举看到 CH340 为 COM10，端口号会漂移 | **已修正**。§9 接线双人复核改为"`io_pin_map.md` 八行逐行引用独立证据，不得用一次双签整体 PASS"；阶段 0 先记录 VID/PID、serial/HWID、描述和人工确认的 allowlist；禁止仅凭"当前 COM 号"自动连接；版本读取脚本只允许经审计的查询 API，不含 updater、servo/power/motion 方法。 |
| P1-4 | A2 预设了根因，且"D 盘 build/flash 树"没有唯一来源 | ✅ 合理。当前证据只支持 Interface Designer/periphery/top/VDB 不一致是待验证假设，不能先宣布它就是 outpad 根因；不明来源构建树更不能成为烧录真源 | **已修正**。§5.3 A2 改为"A2 先复现并分类当前 PNR 报告、Missing Interface Pins/AXI、top/periphery 端口数量和生成批次，再依据证据决定修复点"；列出每棵对照树的绝对路径、commit/hash、生成版本和只读/可部署属性；来源不明的一律仅作 forensic reference。 |

### 12.3 关于"哪些可以暂缓跳过，直接进行实验"的评判

Codex 的审查总体合理且严格。鉴于时间有限（7/16 中午板到臂 UART 电气/回环/无运动/只读验证截止，7/17 基础保底冻结），Claude 对 Codex 安全审查中各门的处置建议如下：

**可以并行推进、不必等 G4 闭合的门（低风险纯软件）**：

- **阶段 0 固件版本只读核验**：用官方 Python API/myStudio 读取 Atom 运行版本，不发送动作命令，风险极低。可立即执行。
- **B1 协议真值表差分补齐**：只补 STOP 0x29、IS_MOVING 0x2B、IS_GRIPPER_MOVING 0x69 的命令常量和解码，扩展现有 skeleton 测试。纯 Host 软件，不碰硬件，可立即执行。
- **B3 UART2 回环测试套件准备**：按逐关节官方限制构造边界向量，组织成 C 数组或 Host fixture。纯数据准备，不碰硬件，可立即执行。

**可以暂缓但需记录降级理由的门（中等风险）**：

- **G5 F12 静默证明**：Codex 要求 ELF 符号表 + 逻辑分析仪双重证明。当前暂无逻辑分析仪/示波器。**建议暂缓"逻辑分析仪双重证明"这一子判据**，先关闭"ELF 符号表证明 simulated 不初始化 UART2"这一软件子门；逻辑分析仪证明留待仪器到位后补。**风险**：无法用电气证据证明 F12 在 simulated 测试中确实静默，只能靠 ELF 符号表和 nm/map 静态推断。若静态推断有遗漏，可能在 simulated 模式下意外驱动 F12，但由于机械臂断开、J52 Pin4 悬空，不会造成机械伤害。
- **G7/T0 空闲电压、电平、共地和收发证据**：Codex 要求空闲电压、电平、共地和收发证据。当前暂无逻辑分析仪/示波器。**建议暂缓"电平/波形测量"这一子判据**，先用 3.3V 兼容被动监听器（如经审核的 USB-TTL）做回环监听，记录 VID/PID/serial/HWID；电平/波形测量留待仪器到位后补。**风险**：无法用电气证据证明 J52 TX/RX 空闲电平和 1Mbps 8N1 波形，只能靠被动监听器抓帧推断。若 J52 电平不兼容，可能损坏开发板 IO 或机械臂驱动，因此第一次连接仍只允许 GET_ANGLES，且必须能立即断电。

**不可暂缓、必须严格执行的门（高风险硬件/动作）**：

- **A0 资源可支持性门**：必须在 Interface Designer/GUI 中确认 PLL/JTAG/pad 合法组合后才能改 XML/top/SDC。**不可跳过**，否则会在架构可支持性未判定时盲改工程文件。
- **A2/A3 PNR/STA/烧录链**：必须复现并分类当前 PNR 报告、生成同批次 SoC/BSP/bitstream/ELF、连续 3 次复位有 CPU Hello。**不可跳过**，否则会烧入来源不明或配置不一致的 bitstream/ELF。
- **G8 真实机械臂只读**：必须确认 Basic 烧录 transponder、Atom 烧录最新版 atomMain，并记录版本/文件哈希；30/30 次 GET_ANGLES 响应正确。**不可跳过**，否则会把无响应直接归因于板侧 UART，误导后续调试。
- **G10/G11 首次低速空载单段 → F2 抓放**：必须通过动作前 Review Packet、机械结构、安全员、点位和急停门。**不可跳过**，否则会造成机械伤害或物体跌落。

**结论**：Codex 的 BLOCK 结论合理。在 G4 闭合前，可以并行推进阶段 0（固件版本只读核验）、B1（协议真值表差分补齐）、B3（UART2 回环测试套件准备）三项低风险纯软件工作；G5/G7 中涉及逻辑分析仪/示波器的子判据可暂缓，但必须记录降级理由和补齐计划，且不得降低 T0/G5/G7 的核心安全判据。任何真实硬件操作仍按各自 Gate 和 Codex Review Packet 执行。

## 13. Codex 最终复核与快速闭环执行标记（2026-07-15）

> **最终裁定：CONDITIONAL GO / 可以开始执行。** 本节基于用户最新风险口径，取代 §11 的初始 `BLOCK` 和 §12.3 的旧结论作为本文件最终执行状态。它允许开启新对话从阶段 0/B/A0 开始推进，但不把尚未执行的 G4–G11 误标为 PASS。

### 13.1 风险接受边界

本轮允许接受：Host/固件失败、超时、通信失败、需重编译/重烧、需重启、实验返工、F1 降级和可恢复的软件状态错误。不得接受：可能烧毁 TJ375N529 I/O、机械臂控制器或供电路径，造成持续过流/过压/反灌，或因失控碰撞导致机械臂必须购买备件或返厂的步骤。

### 13.2 允许放宽的门

1. **阶段 0、B1、B3 与 A0 可立即并行启动。** B2 可后置到 G4/G7 之后；进入 G10 前补足 fail-closed 的最小故障证据即可。
2. **G5 可豁免逻辑分析仪。** 前提是机械臂和 J52 Pin1–Pin4 全部物理断开，只连接开发板供电、JTAG/Programmer 和 UART0；以构建 manifest、ELF/nm/map、UART0 日志、3 次复位、20 轮和 10 分钟结果关闭快速 G5。此降级不等于完整 F12 电气静默验收。
3. **G7 可豁免示波器/波形截图，但只能做板内回环。** 机械臂、USB-TTL 的 TX/VCC 和所有外部电源均不得接入 J52；经双人确认 Pin3/F12 为 3.3V TX、Pin2/C14 为 3.3V RX 后，用 `1 kΩ–2.2 kΩ` 串联限流电阻连接 Pin3→Pin2，Pin4 VCC 悬空。100/100 帧、断开 RX、坏尾和超时重同步通过后，可关闭快速 G7。没有限流或针序不确定时不得上电。
4. **G9 允许有界轮询降级。** 若 PLIC/ring-buffer 在时限内未闭合，可保留有界 polling、overflow/timeout/error 计数和 single-flight，记录为 `POLLING_FALLBACK` 后进入只读链；不得用 ISR 中解析协议或无界等待代替。

### 13.3 不可放宽的不可逆硬件损坏防护门

1. **A0 不可跳过。** 未取得 Interface Designer 对 PLL/system clock/JTAG/pad 合法组合的 GUI 证据前，不改生成 XML、top、SDC，不联合 PNR。
2. **G8 前必须排除机械臂 TX 对 C14 的过压风险。** 大象机器人官方资料证明 myCobot 280 Arduino 支持 MEGA2560/UNO 以 1,000,000 bps UART 连接，但未在该页面给出机械臂 TX 的最大输出电压。因此必须满足以下至少一项：
   - 用万用表在板臂未连接时测量机械臂 TX 对机械臂 GND 的空闲电压，并确认处于 TJ375N529 3.3V 输入允许范围；同时确认两端地之间无异常电位差；或
   - 使用明确额定、方向正确且经确认支持 1 Mbps UART 的电平转换器/数字隔离器，使 FPGA 侧信号不超过 3.3V。

   官方参考：[myCobot 280 Arduino 首次安装与 UART 接线](https://docs.elephantrobotics.com/docs/mycobot_280_ar_en/2-BasicSettings/4.FirstTimeInstallation/4-FirstTimeInstallation.html)。仅凭“支持 Arduino/USB-TTL”不能推断机械臂 TX 对 FPGA 输入绝对安全。
3. **G8 首次连接只允许精确白名单帧。** 只读固件的 UART2 TX 必须只允许 `FE FE 02 20 FA`（GET_ANGLES），发送频率不快于 1 Hz；`round_controller` 必须看到 `arm_enabled=0`，所有动作/夹爪命令不可达。J52 Pin4 永久悬空，机械臂独立 12V 供电，断电接线，30/30 后再进入下一门。
4. **G10/G11 保留动作硬门。** 机械臂固定、运动包络清空、人工断电可立即触达、点位通过代码级逐关节绝对限位且越界会被明确拒绝；先低速空载单段且不闭夹爪，再逐级增加动作。出现异常方向、重复请求、持续堵转、过热、异味、供电异常或结构碰撞趋势立即断电，不允许用“再试一次”跨过故障证据。

### 13.4 快速执行顺序

```text
阶段 0 + B1 + B3（B2 后置）
    -> A0
    -> A1/A2/A3：同批次 SoC/BSP/bitstream/ELF、PNR/STA、3 次 CPU Hello
    -> G5：断臂板上 simulated（允许无分析仪降级）
    -> G6：20 轮 competition + simulated
    -> G7：带串联限流的板内 3.3V 回环
    -> G8：电压兼容或隔离后，GET_ANGLES-only 30/30
    -> G9：中断实现或 POLLING_FALLBACK
    -> G10：低速空载单段
    -> G11：夹爪/抓放最小闭环
```

执行中若遇到软件/构建/通信失败，可回退、修复后重试；若出现未知管脚、未知电平、Pin4 VCC 被连接、制品批次不一致、只读白名单失效、异常动作、持续过流/过热/异味或碰撞趋势，立即 STOP。除这些不可逆硬件损坏风险外，不再因为缺少逻辑分析仪、示波器、PLIC 完整实现或扩展故障注入而阻塞快速最小闭环。
