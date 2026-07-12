# myCobot 板上 CPU 调试执行方案（F2 条件升级）

> 日期：2026-07-12
> 状态：执行版；以 `CURRENT_STATE.md`、官方比赛细则和 `competition_score_maximization_execution_plan_20260712.md` 为准。
> 范围：把已验证的 PC 端机械臂健康/示教经验迁移到“FPGA 板上 CPU -> UART -> myCobot”的正式控制链；不把 PC、`pymycobot` 或外部 MCU 放入比赛闭环。

## 1. 目标、边界与冻结条件

### 1.1 本轮目标

在 **2026-07-16 12:00** 前完成“板到臂安全链”最小证据：

```text
CPU 可运行 + APB MAGIC/heartbeat 可见
    -> UART2 1 Mbps 物理收发已量测
    -> myCobot GET_ANGLES 连续只读可解析
    -> 急停/故障不再继续下发运动命令
```

该门通过后，才允许在用户确认现场安全条件后推进低速、空载、单段关节角运动。若到期未通过，立即冻结为 **F1（无机械臂）**：保留 CPU 对目标/非目标的 `GRAB` / `SKIP` 语义、OSD 与日志，不再为机械臂链路占用视频/SoC 保底资源。

### 1.2 不可突破的边界

| 边界 | 本方案的落实 |
|---|---|
| 职责划分 | FPGA 仅提供 UART/寄存器/OSD 渲染通道；分类、四任务判定、逐轮事务、点位、安全门、协议和机械臂状态机均在板上 CPU C 固件。 |
| PC 定位 | PC / myBlockly / `pymycobot` 只用于示教、串口观察、健康证明和录像；断开 PC 控制后仍必须能演示。 |
| 动作主线 | 使用已示教的关节角点位回放；不将笛卡尔 `sync_send_coords()` 或其等价 C 实现作为自动抓取主线。 |
| 安全 | 未量测 UART 电平、未核准 TX/RX/GND、未固定机械臂和工件、未明确断电/扶稳方式时，只允许无动作验证。 |
| 触发纪律 | 每个稳定 `frame_id` 只形成一笔事务；非目标只输出 `SKIP + 原因`，绝不执行抓取；动作期间拒绝新的 `GRAB`。 |

比赛依据：每轮须清晰输出识别、目标判断和执行/不执行理由；正确识别后机械臂必须有明确且唯一响应；目标物放置为相对起点旋转 180°（±10°）且最大臂展位置，夹持失败或提前释放均视为跌落。

## 2. 当前真实起点与工作拆分

| 项目 | 已有事实 | 本方案中的处理 |
|---|---|---|
| 协议层 | `mycobot_protocol.c` 已有 `FE FE LEN CMD ... FA` 组帧/解析与角度编码；它本身无 UART/MMIO/动作副作用。 | 保持协议层纯函数；补 transport 和实机收发证据，不重写协议。 |
| 控制器 | `arm_controller.c` 已有非阻塞 `tick`、`PRECHECK`、移动/夹爪、软超时后二次读回、一次重试、`FAULT/ESTOP` 状态及 Host 回归测试。 | 先以 mock transport 做回归，再接真实 UART；不得把阻塞延时塞回控制器。 |
| CPU-视频接口 | `board_io.h` 已定义 `REG_MAGIC`、心跳、全局 ARM/ERROR 回写及 OSD/结果提交，但实际 SoC/APB 尚未随视频工程闭环。 | APB 最小闭环是 UART 前置门；`board_io` 不承担 UART ISR。 |
| 硬件链路 | 开发板 UART、`soc.h`、UART2 地址/IRQ、电平、接线均未定；PC 端多轮动作稳定但旧路径约 10 cm。 | 把所有未定项列为 T0 真源表；禁止从占位宏或 PC 日志推定已上板。 |

### 分工

| 角色 | 责任与交付 | 不承担 |
|---|---|---|
| A：FPGA/SoC | 明确 CPU 是否在视频工程内运行；生成并提交 `soc.h`、linker、UART2 基址/IRQ、APB 从机和 UART 顶层连线证据；协助逻辑分析。 | 不在 RTL 实现分类、任务判定或机械臂动作序列。 |
| B：CPU（1–2 h/日） | 审核地址/中断真源、接口契约、`mycobot_uart` 设计和构建命令；每个小提交做 Codex Gate 前自测。 | 不在真源未知时猜地址或直接接真实臂动作。 |
| C：机械臂（6–8 h/日） | 固定工作台、低速重新示教 180°点位、做 PC 健康/姿态复核、负责实机安全员和录像/数据记录。 | 不把 PC 控制脚本当作正式运行程序。 |
| Codex | 审查跨 SoC/UART/控制器边界、动作前 Review Packet、风险/回滚判断。 | 不替代现场人员确认机械臂固定、急停和接线。 |

## 3. T0 真源冻结：先完成这张表，未完成即 NO-GO

| 项目 | 需给出的证据 | 负责人 | 通过判据 |
|---|---|---|---|
| CPU 工程真源 | 最终 Efinity 工程中的 `soc.h`、linker、ELF 装载/调试配置及 CPU Hello 日志 | A | 可从视频工程追到生成源，不使用 `bsp.h` 或 `board_io.h` 占位基址。 |
| UART2 真源 | UART2 基址、时钟、1 Mbps 分频、TX/RX FIFO 标志、IRQ 号及 PLIC claim/complete 说明 | A+B | 以生成文件/RTL/日志交叉核对，禁止猜测。 |
| 物理接口 | 开发板 UART2 的针脚、方向、逻辑电平、GND 共地、myCobot 串口线序/供电隔离、示波器或万用表记录 | A+C | 证实相容后才允许接入；不相容时采用批准的电平转换/隔离方案。 |
| 安全输入 | V19/其他急停输入的来源、默认电平、去抖、CPU 可见路径及失效安全语义 | A+B | 触发后停发新动作、进入 `ESTOP`；“释放扭矩”仅在已扶稳或明确保护流程下允许。 |
| 点位 | `HOME`、`home_ready`、`pick_hover`、`pick`、`drop_hover`、`drop` 的关节角、速度、夹爪值、碰撞余量和适用工件 | C | 每个点位单独 PC 复核；`home_ready` 使用独立差值门，不能混入普通抓取工作区。 |

T0 的唯一产物为 `final_project/integration/io_pin_map.md` 的证据表和一份 Review Packet。任一条为“未知”时，允许继续纯 C / 文档工作，但禁止连接真实机械臂控制线或下发动作帧。

## 4. 日级执行路径与 GO / NO-GO

### D0（7 月 12 日）：脱离 PC 的前置条件

1. A 提供 CPU/SoC/UART2 真源；B 对 `soc.h` 宏、APB 地址、UART2 地址和 PLIC 编号做只读核对；C 固定机械臂、相机、取放区和录像机位。
2. C 重新示教/验证 180°放置方向的六类点位，只可用 PC 低速单步；记录实际角度与最大关节差，不改动已成功基线脚本。
3. B 新建 `mycobot_uart.h/.c` 的接口草案：`init`、有界 `tx`、有界 `rx`、`poll`、时间基准和错误码。此日不接 PLIC、不接 `main.c`、不改 RTL。

**GO：** T0 中 CPU、UART2、电平、急停、点位五项均有可追溯证据。
**NO-GO：** 任一缺失则只产出缺口清单，转入 F1 SoC/APB 保底。

### D1（最晚 7 月 14 日晚）：CPU ↔ FPGA/APB 最小闭环

执行顺序：CPU Hello（UART0） -> `board_io_validate()` 读 `REG_MAGIC` -> 递增 `CPU_HEARTBEAT` -> 写 `CPU_ARM_STATE=BOOT/READY`、`CPU_ERROR_CODE=0` -> `board_io_commit_global()` -> OSD 或 UART 日志可见。

验收记录必须能区分：CPU 未启动、`soc.h`/地址错误、APB 从机未连、staging 未 commit、OSD 未显示。到 7 月 14 日晚仍无 CPU Hello + APB MAGIC，机械臂工作停止消耗共享资源，团队集中救 SoC/APB。

### D2：UART2 无机械臂验证（仅线路/帧）

1. 用本地回环、USB-UART 监听器或逻辑分析仪确认 UART2 为 **1,000,000 bps、8N1**，并保存波形/原始字节。
2. 运行有界 TX/RX：每字节发送等待和每帧接收等待都必须有超时；不允许沿用 `bsp.h` 当前无超时的 `uart_write()` 作为机械臂 transport。
3. 把预期 `GET_ANGLES` 请求帧发至监听器，验证 `mycobot_parse_frame()` 能处理分段到达、错误头、错误尾和超长帧。

**GO：** 100 帧无丢字节，速率、帧边界、超时日志均正确。
**NO-GO：** 只排查时钟/引脚/电平/方向，不将真实臂作为示波器。

### D3：真实臂只读链路

前置安全确认：机械臂已固定且周边净空、人员明确断电方式、线序和共地已签核；本阶段禁止 `SEND_ANGLES`、夹爪、使能/释放扭矩和任何运动命令。

按 `GET_ANGLES` 单命令、低频连续读取、读取状态/坐标（如协议真源支持）的顺序执行。每次记录 TX/RX 十六进制、命令码、解析结果、延迟、超时与错误码，并回写 `READONLY_OK` / 错误状态至 OSD。连续 30 次读取正确且不出现异常帧才可进入 D4。

### D4：中断和控制器接入（不动作）

1. 保留稳定轮询版作为对照；再在 `mycobot_uart.c` 实现 RX ring buffer 与 PLIC。
2. ISR 只做收字节、溢出计数、claim/complete；协议解析、日志、状态转移均在主循环 `tick`。
3. 将 transport 适配给 `arm_controller`，但在 `ARM_READONLY` / dry-run 模式只产生“将要发送”的日志；执行既有 Host 回归，尤其软超时后读回、一次重试和非连续确认测试。

**GO：** 轮询与中断解析结果一致，ring buffer 无溢出，dry-run 状态序列与单测一致。
**NO-GO：** ISR 不稳定、异常帧或错误地址时回退轮询只读版，不上动作。

### D5（截止 7 月 16 日 12:00）：单段低速空载动作

仅当 D0–D4 全部通过且 F1 关键路径未受阻时，在 C 现场确认后执行：

1. `PRECHECK`：点位有效、急停未触发、上次无未恢复故障、`home_ready` 独立安全门通过。
2. `HOME -> home_ready` 的单段低速关节角动作；绝不直接跨越到 `pick` 或夹爪。
3. 每段完成后读实际角度；严格到位失败时进入已有的 `POST_READBACK -> SOFT_PASS / RETRY_ONCE -> FAULT` 路径，禁止无限重试。
4. 故障、超时、RX 解析错误或急停发生时：停止新命令、回写错误码、提示人工扶稳；不得自动连续补发运动/释放扭矩。

**F2 安全链通过：** 完成一次可重复的低速单段动作并保留完整日志，或至少完成稳定只读+干运行并由风险评审明确“未行动作”。
**F2 冻结：** 12:00 前无法达到上述门，则正式演示保持 F1，机械臂只作为展台静态/后续增强项。

### D6：抓放闭环（仅 F2 已通过后）

逐段推进：`home_ready -> pick_hover -> pick -> close_gripper -> pick_hover -> drop_hover -> drop -> open_gripper -> home_ready`。先空载，再单一已验证正方体，最后接入 `match_action`。

接入条件：CPU 已获得稳定 feature snapshot；`task_matcher` 已能输出四任务的 `GRAB/SKIP`；OSD 能显示颜色、形状、尺寸、目标判断、执行理由和机械臂状态。`SKIP` 必须不动作且给出理由；`GRAB` 只为已锁存的轮次触发一次。每轮结束显式回到 `IDLE` 后才接受下一物体。

## 5. 固件切分与最小改动面

| 模块 | 允许职责 | 首个工单验收 |
|---|---|---|
| `mycobot_protocol.c/.h` | 帧组装/解析、负载编码/解码；无 UART/MMIO/动作。 | 既有协议单测 + 分段帧/坏帧测试通过。 |
| **新增** `mycobot_uart.c/.h` | UART2 有界轮询、RX ring buffer、PLIC ISR、时间戳、transport 错误。 | 本地回环 100 帧与超时/溢出测试。 |
| `arm_controller.c/.h` | 非阻塞状态机、点位序列、到位读回、有限重试、故障/急停收敛。 | Host mock 覆盖正常、软到位、读回失败、一次重试失败、ESTOP。 |
| `board_io.c/.h` | APB 验证、心跳、可解释状态/错误/结果回写；不实现 UART ISR。 | `MAGIC -> heartbeat -> state/error commit -> 可见`。 |
| `main.c` / `round_controller`（后续） | 仅在 F1 任务匹配与稳定帧锁存完成后，连接事务和 arm request。 | 同一 `frame_id` 不重复发请求；`SKIP` 可解释且零动作。 |

禁止在本轮修改 `top.v`、视频 RTL、SDC、Efinity XML 来“顺便”解决机械臂问题；涉及这些文件须单独 Review Packet 和 Codex Gate。

## 6. 实机运行记录与回滚规则

每次上板/接臂生成一份 `final_project/docs/debug_sessions/mycobot_board_run_YYYYMMDD_NN.md`，至少包含：

- bitstream/ELF 的 commit、构建命令和工具版本；`soc.h`、UART2/PLIC 真源路径；
- 线序、电平测量、断电方式、固定/净空检查和现场安全员；
- 目标状态、实际状态、TX/RX 原始字节、帧解析、耗时、最大角差和 `CPU_ERROR_CODE`；
- 是否发送过动作帧；若发送，动作段、速度、工件/空载、录像文件名和结果；
- PASS / FAIL / WARN、下一步，以及恢复 F1 的明确条件。

出现以下任一项立即停止本轮动作并退回 D3 或 F1：未知协议帧、连续 RX 溢出、实际姿态异常、严格失败且读回超出软阈值、急停、夹持/放置不稳、任何碰撞或人员无法立即断电。

## 7. 交付审查门

每个工单交给 Codex 前必须提供 Review Packet：目标、修改文件和关键 diff、模块/信号/时钟域、运行命令和完整日志位置、未验证项、风险假设、回滚方式。下面的事项必须先经 Codex Gate：UART2/PLIC 真源、CPU 到 OSD 状态回写、FPGA 接线/电平、首次真实动作、夹爪控制、动作超时/急停语义。

## 8. 当前结论

### 8.1 2026-07-12 现场结构风险覆盖项

当日 180° PC 调试发现：机械臂本体与乐高底座的连接存在运动中的微小倾斜。该现象会改变夹爪相对工作台/工件的真实位置，却不会被机械臂内部 `get_coords()` 的底座坐标系直接测量。J4 正偏置探针虽将内部残差降至约 `3.7–3.8 mm`，两次严格同步仍返回 `0`；偏置不能替代机械结构稳定性或严格动作门。

因此，在结构加固后取得外部基准的位移复核前：

- 允许 D1（CPU/APB）和 D2（UART2 本地回环/监听）继续；两者不得连接真实机械臂控制线。
- 进入 D3 前，禁止向真实机械臂发送任何帧；D3 也必须先满足 T0 的电平、线序、共地、急停和 CPU/UART 真源，并完成连接位移复核，且仅允许既定的只读帧。
- 禁止 D5/D6 的真实运动、夹爪和抓放。乐高多点/三角支撑仅是风险缓解；只要连接处仍有可见相对位移，即不满足“机械臂已固定”的动作前置条件。

完整 PC 端证据见 `mycobot_pc_tests/audit_logs/20260712_180deg_j4_mount_debug_record.md`。

- **架构：GO。** 板上 CPU 接管协议与状态机，符合比赛与工程职责边界。
- **纯 C / mock / APB / UART 监听：GO。** 可立即并行，但不应阻塞 F1 的视频与四任务闭环。
- **真实臂只读：条件 GO。** 前提是 T0 电平、线序、急停与 CPU/UART 真源齐全。
- **真实动作：当前 NO-GO。** 需先完成 D0–D4，并由现场安全确认和 Codex Review Packet 放行。
- **7 月 16 日 12:00 后：硬冻结。** 未达 F2 安全链即切回 F1，不用机械臂链路影响 20 轮、10 分钟的保底演示。

### 8.2 2026-07-12 CPU 分支合并后的执行增量（Codex）

- 本地 `main` 已合并 `origin/dev/wsc6090-CPU@af82e52`，合并提交为 `af6d32d`；未推送 `origin/main`。
- 四任务 matcher、同轮防重复锁与错误锁存已完成 Host 层修正，当前三组测试为 258/258；这不改变 APB/UART/机械臂板级未验证结论。
- `mycobot_transport.c/.h` 的纯内存 RX Ring Buffer、坏帧重同步和 TX 队列已经存在，不应另起一套 transport；D2/D4 只新增硬件适配层 `mycobot_uart.c/.h`，负责 UART2 MMIO、超时、轮询/ISR 和 PLIC claim/complete。
- `task_matcher_set_target_ex()` 已保证同一轮反复写入相同目标不会重新解除 `GRAB_REQUESTED`；下一轮必须显式调用 `task_matcher_next_round()`。
- `final_project/cpu/params/arm_positions.c` 仍是旧 PC 点位迁移表，文件头明确“未接 main/UART/真实 transport”，且不包含当日新示教的 180°放置点。D5/D6 禁止使用该默认表冒充新点位。
- 当前操作版见 `mycobot_board_bringup_operator_sop_20260712.md`。在 `io_pin_map.md` 真源表、结构加固复核、UART2 硬件适配和 180°板上点位迁移完成前，只允许 Host/QEMU、APB 和无臂 UART 监听。
