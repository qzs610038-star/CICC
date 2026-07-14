# myCobot 机械臂代码上板详细实施方案

> 日期：2026-07-14
> 状态：G1–G3 初始草稿检查点已复核；尚未进入 G4，未修改 FPGA 工程、接线或烧录状态。
> 审计基线：分支 `codex/mycobot-g0-g3-bringup-20260714`，基线提交 `d433bca`；草稿仍为未提交工作树内容。
> 当前结论：Host 回归通过；QEMU 断言虽运行到 PASS，但测试入口/超时门不完整；RISC-V 四组合为 3/4 PASS。G1、G2、G3 均为 PARTIAL，板上无臂模拟、正式烧录和真实机械臂动作均未放行。
> 工作区边界：现有 `Makefile/main.c/deploy.bat`、`arm_build_profile.h`、`arm_runtime.*`、`arm_sim_transport.*`、profile/build/test 脚本已经完成只读审查；其来源仍是当前未提交工作树，不得描述为 `d433bca` 已具备。测试临时目录已清理，原有文档和本机配置改动继续保留。
> 并发记录（已恢复）：最终 QA 时共享工作区曾被外部会话切到无关的 `codex/explore-log-return-20260714`；按用户明确指令已切回 `codex/mycobot-g0-g3-bringup-20260714`。未提交文件完整，未执行清理或提交；后续仍应在写入前核对分支。
> 上位约束：官方比赛细则、`AGENTS.md`、`CURRENT_STATE.md` 和 `competition_score_maximization_execution_plan_20260712.md`。
> 状态覆盖：更新 2026-07-12/13 机械臂方案中的实现状态；旧方案的物理安全门、接线门和 F1/F2 回退规则继续有效。
> 协议复核：已按 Elephant Robotics 官方[串口通信协议](https://docs.elephantrobotics.com/docs/mycobot_280_ar_cn/3-FunctionsAndApplications/6.developmentGuide/CommunicationProtocolPackage/18-communication.html)于 2026-07-14 校验；本方案 §3、§4.5、§4.6、G7–G11 已据此收紧。

## 1. 本方案要解决什么

本方案把“机械臂代码上板”拆成三个不可混写的完成层级：

1. **板上无臂模拟完成**：机械臂和 J52 均断开，QCRV32 在开发板上真实运行 `round_controller`、`arm_controller` 和模拟反馈后端；状态轨迹、超时、故障和 20 轮事务可通过 UART0 日志观察，UART2 不初始化、不发送。
2. **板上只读链路完成**：正式 SoC/UART2 真源、电平、线序和回环全部通过后，只允许向真实机械臂发送 `GET_ANGLES`，30/30 次解析正确且零动作。
3. **真实动作完成**：通过动作前 Review Packet、机械结构、安全员、点位和急停门后，才从低速空载单段逐级升级到 180° 抓放。

下一次会话的首要目标是第 1 层，不是直接做第 2 或第 3 层。

## 2. 2026-07-14 当前代码事实

| 项目 | 当前证据 | 判定 |
|---|---|---|
| CPU 安全契约 | 16-bit `event_seq/ACK`、显式 ACCEPTED/REJECTED、半范围回绕和 `arm_busy` 门已合入 `main@d433bca` | Host 层 GO |
| Host/Mock | 基线 skeleton QEMU、`round_controller` 4979/4979 以及新增 disabled/simulated runtime Host 回归均通过 | Host 层 GO |
| 正式入口 | 工作树 `main.c` 已建立 `main -> round_controller -> arm_runtime` 结构桥，且没有合成未验证事件；尚无真实 `event_valid/observation_valid` 源，时基也只是循环计数 | G2 PARTIAL，不是逐轮闭环 |
| RISC-V 链接 | `competition+disabled`、`competition+simulated`、`arm_bringup+simulated` 通过；`arm_bringup+disabled` 因 `round_controller_tick` 被回收而被 ELF gate 拒绝 | 3/4 PASS，G3 未完成 |
| 安全模式 | 工作树已有 disabled/simulated profile、runtime 和模拟 transport；当前 ELF 均标为 `NOT_FOR_FLASH`，真实 UART backend 未加入 | 方向正确，仍须 G1.5–G3.5 加固 |
| QEMU 门 | disabled/simulated 均运行到断言 PASS，但 linker `ENTRY(_startup)` 与启动文件 `_start` 不一致，`TimeoutSeconds` 未生效 | 条件 PASS，严格门 FAIL |
| BSP/SoC | `bsp.h` 和 linker 仍含占位信息；正式构建要求同一 SoC 工程生成的 `soc.h`、linker、startup/debug profile | 正式构建 NO-GO |
| FPGA 构建 | production/debug map 有记录，但 PNR 仍因大量 IO/outpad 问题失败，无可验收 bitstream/STA | 烧录 NO-GO |
| 部署 | `cpu/build_tools/deploy.bat` 仍为说明占位，没有经过验证的 Programmer/OpenOCD 命令 | 烧录 NO-GO |
| UART2/J52 | 板端 J52、C14/F12 和 3.3V VCCIO 已冻结；机械臂端线序、电平、正式约束仍未 PASS | 真实接线 NO-GO |
| 协议基础 | 当前 C builder 的 `FE FE LEN CMD ... FA`、`LEN=payload+2`、0x20/0x22/0x65/0x66/0x67 和角度 big-endian 缩放与官方示例对齐 | 基础编码 GO，真实事务仍 NO-GO |
| 协议事务 | 官方规定有返回值命令在 500 ms 内响应，0x22/0x66/0x67/0x29 无返回；当前代码没有 single-flight、expected-command/deadline、STOP/夹爪回读和官方逐关节硬限位 | G7–G11 阻塞 |

因此，当前最合理的推进顺序是：先关闭 G1.5–G3.5 的构建/证据问题，使 G3 真正达到 4/4 严格通过；再闭合 SoC/bitstream/烧录平台，最后才触碰 UART2 和真实机械臂。详细检查点证据见 `../review_packets/mycobot_g1_g3_protocol_checkpoint_review_20260714.md`。

## 3. 不可违反的安全不变量

1. 默认构建必须是机械臂禁用；任何未明确选择后端的构建均不得包含真实 UART2 动作路径。
2. `STANDALONE_TEST=1`、`0xF0000000`、当前 `bsp.h` 硬编码地址或临时 linker 生成物禁止烧录。
3. `DISABLED` 和 `SIMULATED` 模式不得初始化 UART2，不得向 F12 写任何字节；验收时还要用 ELF 符号表和逻辑分析仪双重证明。
4. 动作请求的唯一合法路径是 `main -> round_controller -> arm_runtime -> arm_controller -> backend`。禁止 `task_matcher`、调试命令或 OSD 逻辑直通 `arm_controller_request_grab()`。
5. 同一轮最多产生一个 `request_arm_grab` 脉冲；busy、fault、重复/过期事件、非法状态和非目标轮必须为零动作请求。
6. UART0/Type-C 仅作 CPU 启动与日志控制台；UART2/J52 才是未来机械臂链，二者不得复用。
7. 真实 UART 适配的所有 TX/RX 等待均有界；ISR 只搬运字节和计数，不解析协议、不打印、不推进动作状态机。
8. 通信故障、到位失败或急停后默认停止新命令并保持故障证据；不得默认自动释放扭矩。
9. J52 Pin4 VCC 永久悬空；机械臂独立 12V 5A 供电。没有电平、共地和线序双签时不得连接三根信号线。
10. PC、myBlockly 和 `pymycobot` 只用于学习、示教、健康检查和证据采集，不进入比赛闭环。
11. build backend 在固件生命周期内不可变；`real` 即使被编进 ELF，上电后也必须保持 `real_armed=0`，只有明确操作员事件和运行时安全检查通过后才能置 1，复位、fault、ESTOP 或通信异常立即清零。
12. `readonly` 模式必须向 `round_controller` 报告 `arm_enabled=0`，真实 UART 只由独立只读服务发起 GET_ANGLES；不得因为 UART 可收发就让逐轮控制器产生动作请求。
13. 真实臂只读前必须确认 Basic 已烧录 `transponder`、Atom 已烧录最新版 `atomMain`，并记录版本/文件哈希；无法确认时保持 G8 NO-GO，不得把无响应直接归因于板侧 UART。
14. 协议没有事务序号，所有带返回值命令必须 single-flight；响应同时匹配 expected command、精确 payload 长度和取值域。首版响应期限为 750 ms，未完成或未重同步前不得发送下一请求。
15. `SEND_ANGLES`、夹爪设置和 `STOP` 均无协议 ACK；TX 完成绝不等于动作完成、夹持成功或停止成功。真实状态必须由独立查询和物理观测确认，人工断电/急停仍是最终保护。

## 4. 目标固件架构

### 4.1 两个正交构建维度

不能只增加一个含义模糊的 `ARM_DISABLED` 宏。应把“运行哪个主程序”和“机械臂使用哪个后端”分开。

应用入口：

| 入口 | 计划名 | 用途 |
|---|---|---|
| 独立上板自检 | `APP_PROFILE=arm_bringup` | 只运行 CPU Hello、时间基准、逐轮控制器、动作控制器和选定后端；不依赖相机、正式 APB 特征或 OSD，用于最快证明机械臂代码确实在 QCRV32 上执行。 |
| 正式比赛主循环 | `APP_PROFILE=competition` | 运行识别、任务匹配、逐轮事务、OSD/APB 状态和机械臂运行时；用于后续 Gate C/F1/F2。 |

机械臂后端：

| 后端 | 计划名 | 行为 | 当前状态 |
|---|---|---|---|
| 禁用 | `ARM_BACKEND=disabled` | `arm_enabled=0`；目标轮输出 `ARM_NOT_READY`，不创建动作控制器 transport，不初始化 UART2 | 待实现，必须为默认值 |
| 板上模拟 | `ARM_BACKEND=simulated` | `arm_enabled=1`；使用内存 Mock 模拟发送、角度收敛、夹爪、超时和故障；不包含 UART2 MMIO | 待实现，是首个上板目标 |
| 只读 | `ARM_BACKEND=readonly` | 仅包含 UART2 接收/发送和 `GET_ANGLES` 白名单；向逐轮控制器固定报告 `arm_enabled=0`；禁止 `SEND_ANGLES`、夹爪、扭矩/使能类命令 | 待 D2 后实现 |
| 真实动作 | `ARM_BACKEND=real` | 使用真实 UART2 transport 和版本化 180°点位；仍需上电默认关闭的运行时 `real_armed` 二次门 | 待全部安全门后实现，默认永不启用 |

### 4.2 允许的组合

| APP_PROFILE | disabled | simulated | readonly | real |
|---|---:|---:|---:|---:|
| `arm_bringup` | 允许，用于启动安全检查 | 允许，首个板上目标 | 允许，但必须在 D2 后 | 禁止作为第一步；仅动作专项 Review 后临时允许 |
| `competition` | 允许且默认 | 允许，用于 Gate C/20轮无臂整场 | 仅诊断构建 | 仅 Gate D/F2 放行后允许 |

无效组合应在预处理或构建脚本阶段直接失败，不能运行时“尽量安全”。

### 4.3 计划调用链

```text
APP_PROFILE entry
    +-> disabled/competition
    |     -> round_controller_tick() -> arm_runtime reports arm_enabled=0
    +-> simulated/real
    |     -> round_controller_tick()
    |         -> one-shot request_arm_grab
    |             -> arm_runtime_accept_request()
    |                 -> arm_controller_request_grab()
    |                     -> arm_controller_tick()
    |                         -> selected arm_controller_ops_t backend
    |                             simulated : memory-only fake feedback
    |                             real      : mycobot UART transport
    +-> readonly diagnostic service
          -> mycobot_readonly_service_tick() -> GET_ANGLES whitelist only
          -> round_controller always sees arm_enabled=0
```

`arm_runtime` 负责把动作控制器状态映射回逐轮输入：

- `arm_busy=1`：除 `IDLE/DONE/FAULT/ESTOP` 外的运动期；
- `arm_done=1`：`ARM_STATE_DONE`，保持到 `round_controller` 离开 `WAIT_ARM_DONE`；
- `arm_fault=1`：`FAULT/ESTOP`，必须保留错误码；
- 收到单次请求脉冲后只调用一次 `arm_controller_request_grab()`；失败也不得在同轮自动重复发起。

状态回写不能直接复用裸枚举数值。当前 `main.c` 自定义 `ARM_STATE_IDLE=1`，而 `arm_controller.h` 的 `ARM_STATE_IDLE=0`；接入前必须建立显式的“内部动作状态 -> APB/OSD 状态”映射并使用不同命名前缀，禁止靠强制类型转换或相同宏名碰运气。

### 4.4 模拟后端必须证明什么

板上模拟不是打印几行假日志，而是让真实 `arm_controller.c` 运行：

- `send_angles`：保存目标角度、速度和调用次数，不访问 UART；
- `read_angles`：按配置在 N 次轮询后收敛到目标，也能注入超时、单次读失败和永久失败；
- `set_gripper`：记录开合值与次数，不访问硬件；
- 使用独立 `g_arm_sim_plan`，不得把模拟通过冒充 180°实物点位通过；
- 每次状态变化输出结构化 UART0 日志，包含 round、state、error、request/send/read/gripper 计数；
- 正常、软超时后 soft-pass、一次重试成功、重试失败、busy、fault、20轮混合事务均可选择运行。

### 4.5 模拟通过后仍必须整改的真实动作缺口

以下是当前 `arm_controller` 的真实能力边界，不影响 G0–G6 的无臂模拟，但会阻塞 G10/G11：

1. 当前 `PRECHECK` 主要校验静态点位表，尚未纳入 `real_armed`、急停输入、当前实际姿态、transport 健康和点位版本等运行时条件。
2. `arm_controller_cancel()` 只改变软件状态。官方提供 `STOP=0x29`，但该命令无返回值，也不是安全额定急停。G10 前应新增 backend stop：有界发送一次 0x29，随后 single-flight 查询 `IS_MOVING=0x2B` 并用 `GET_ANGLES=0x20` 检查角度稳定；任一环节不确定即 FAULT，并继续以人工断电/急停作为最终保护。
3. 当前夹爪命令返回后会立即推进抬升，但官方明确 0x66/0x67 无返回值。G11 前应在设置后轮询 `IS_GRIPPER_MOVING=0x69`，停止后读取 `GET_GRIPPER_VALUE=0x65` 并验证稳定窗口；这只能证明位置/运动状态，不能单独证明物体夹牢，仍需带载抬升和滑落失败测试。
4. `arm_controller_request_grab()` 启动的是完整抓放序列，不能直接承担首次 `HOME -> home_ready` 单段服务动作。G10 前需增加独立、受限的 service-move 接口或专项控制器，且该接口不得被正式 matcher 调用。
5. 现有点位参数文件不在当前 Makefile 的 `src/*.c` wildcard 中；构建重构时必须显式纳入选定点位源，不能链接成功后才发现计划表未进入 ELF。
6. 当前点位校验未覆盖官方绝对关节范围：J1 `[-168,168]`、J2 `[-135,135]`、J3 `[-150,150]`、J4 `[-145,145]`、J5 `[-165,165]`、J6 `[-180,180]` 度。REAL 编码前必须逐点、逐关节检查，并使用点位 Review Packet 批准的保守裕量；不得仅依赖 int16 饱和。
7. 当前通用 parser 允许 64 字节 payload，而官方页写明有效 `LEN=0x02..0x10`。G7 前必须增加命令级精确长度和 expected-command 校验；未知、迟到、重复和命令不匹配帧只能计数丢弃，不能推进状态机。

### 4.6 官方协议冻结基线

| 项目 | 官方协议事实 | 对方案的约束 |
|---|---|---|
| 固件前置 | Basic=`transponder`，Atom=最新版 `atomMain` | 新增 G8 前置记录；Codex 不自动替用户刷机械臂控制器固件 |
| 物理接口 | 官方页说明 USB Type-C、`1000000 8N1` | 只证明应用层与串口参数，不替代 J52 电气/线序/端口协议门 |
| 帧 | `FE FE LEN CMD PAYLOAD FA`，`LEN=payload+2`，官方有效窗口 `0x02..0x10` | 当前 builder 对齐；readonly/real 必须用白名单和精确长度收紧 parser |
| 响应 | 有返回值命令在 500 ms 内返回；无事务序号 | 初始 deadline 750 ms、单请求在途、响应命令/长度/取值域全匹配 |
| 角度 | GET 0x20 返回 6 个 big-endian signed int16 `/100`；SEND 0x22 无返回 | 当前 helper 基本对齐；到位只用后续 0x20 连续读回，不等 0x22 ACK |
| 停止 | STOP 0x29 无返回；IS_MOVING 0x2B 返回 bool | 软件 stop 仅为附加防线；需 0x2B + 0x20 核验，物理断电不撤销 |
| 夹爪 | GET value 0x65；SET 0x66/0x67 无返回；IS_MOVING 0x69 | 新增停止/位置读回；位置不等于夹持力或物体安全 |
| 文档歧义 | 页面含字段下标、`33000` 负数阈值和命令范围等内部不一致 | 只实现白名单；官方示例 + 标准二补码 + 本地库 + 真实抓包交叉确认 |

具体命令真值表、实现差距和歧义处理统一维护在 `final_project/integration/mycobot_protocol_notes.md`；任何后续代码不得只凭 `pymycobot has_reply=True` 推断线协议存在 ACK。

## 5. 计划修改的文件

下一会话应小步提交，下面是目标改动面，不表示本轮已经创建这些源码。

| 文件 | 计划动作 | Codex 验收点 |
|---|---|---|
| `cpu/app/Makefile` | 从无条件 `src/*.c` 改为显式 common sources + profile entry；显式纳入选定 params 点位源；接受 `APP_PROFILE`、`ARM_BACKEND`、正式 linker/startup/soc 路径 | 默认后端 disabled；无效组合构建失败；两个入口不会同时链接；点位符号可在 ELF 证明 |
| `cpu/app/include/arm_build_profile.h` | 新增构建枚举、默认值、组合静态检查 | 未定义时为 disabled；real 需要额外显式门 |
| `cpu/app/include/arm_runtime.h`、`src/arm_runtime.c` | 新增逐轮控制器到动作控制器的唯一适配层和内部状态到 APB/OSD 状态的显式映射 | 请求单次、done/fault/busy 映射、无重复触发；不复用冲突的 ARM_STATE 数值 |
| `cpu/app/include/arm_sim_transport.h`、`src/arm_sim_transport.c` | 把测试 Mock 抽成无 MMIO 的目标可编译后端 | 支持确定性收敛与故障注入；无 UART 依赖 |
| `cpu/app/profiles/arm_bringup_main.c` | 新增独立 CPU 上板自检入口 | UART0 输出 build manifest、测试用例和 PASS/FAIL；不访问 APB/UART2 |
| `cpu/app/src/main.c` | 在 disabled 下最小接入正式 `round_controller` 和 `arm_runtime`，后续再启用 simulated | 不从 matcher 直通 arm；现有识别主循环行为可回归 |
| `cpu/app/include/bsp.h` | 将正式地址映射到生成 `soc.h`；硬编码地址只允许测试配置 | 正式构建缺真源直接 `#error` |
| `cpu/app/linker/`、`cpu/bsp_vendor/` | 接入同一 SoC 生成批次的 linker/startup/trap/debug profile，并记录来源与哈希 | 不混用示例工程地址；核对 DDR/RAM origin、`_sidata`、`.data/.bss`、栈、`mtvec` 和异常路径 |
| `cpu/build_tools/build_arm_profile.ps1` | 新增统一构建包装器和 manifest；按 profile/backend 生成不同文件名 | 记录 commit/profile/backend/soc/linker/hash/flags；禁止 placeholder board build；测试产物不能与可烧录产物同名 |
| `cpu/build_tools/verify_arm_elf.ps1` | 新增符号与禁用路径检查 | simulated 必须包含 arm/round/sim，且不得包含 UART2 backend；disabled 不得含真实动作路径 |
| `cpu/app/include/mycobot_protocol.h`、`src/mycobot_protocol.c` | G7–G11 补 0x29/0x2B/0x69、0x65/布尔解码、官方长度窗口和关节限位 helper | 官方示例/边界向量通过；未知命令或错误长度不能到达控制器 |
| `cpu/app/include/mycobot_uart.h`、`src/mycobot_uart.c` | D2 阶段新增 single-flight 有界事务，之后可加 PLIC | 复用现有 `mycobot_transport`；expected command/length、750 ms deadline、迟到/重复/未知计数齐全；不重复造 ring buffer |
| `cpu/app/include/arm_controller.h`、`src/arm_controller.c` | G10/G11 增加 stop、夹爪确认和 REAL 逐关节绝对限位 | 0x22/0x66/0x67/0x29 均不得等待 ACK；失败停线且保留证据 |
| `cpu/params/arm_positions_180deg_v1.c/.h` | 动作门后新增版本化点位，不覆盖旧表 | 来源、日期、底板版本、单位、速度和验证证据齐全 |
| `cpu/tests/test_arm_runtime.c` 及 runner | 新增后端组合和 20 轮端到端测试 | disabled 零请求；simulated 单轮一次；故障可恢复/停线 |
| `cpu/build_tools/deploy.*` | 只有正式 Programmer/OpenOCD 命令验证后才从占位升级；在此之前调用必须非零退出 | 读取 manifest，显示目标板/bitstream/ELF，默认只 dry-run；禁止现有占位脚本以退出码 0 冒充部署成功 |

## 6. Codex、用户和共同任务边界

### 6.1 Codex 可以独立完成

- 代码审计、分支和 diff 管理，且保留用户现有 dirty 文件。
- 构建模式、`arm_runtime`、模拟后端、自检入口、Host/QEMU 测试和 RISC-V 编译/链接实现。
- ELF/map/nm/objdump 静态检查，证明机械臂代码是否被链接和 UART2 路径是否被排除。
- 对生成的 `soc.h`、linker、startup、`.peri.xml`、APB/UART 宏和构建日志进行一致性审查。
- 编写构建、日志解析、证据模板和安全检查脚本。
- 在用户提供现场日志、波形、照片和 GUI 生成物后定位问题、给出最小补丁并复核。
- 每次跨 SoC/UART/真实动作变更形成 Review Packet，并判断是否达到下一门。

### 6.2 用户必须学习并亲自完成

- 确认开发板、JTAG/USB、供电、机械臂和仪器的真实连接状态；Codex 无法远程看见物理接线。
- 在 Efinity/RISC-V IDE 中完成需要 GUI 的 SoC 生成、Interface Designer 检查、bitstream 选择和 Programmer/JTAG 操作，或把完整日志交给 Codex分析。
- 在每次烧录前确认当前连接的确是 TJ375N529、机械臂/J52 是否断开、选择的 bitstream/ELF 是否与 manifest 一致。
- 使用万用表/示波器/逻辑分析仪测量 GND、空闲电平、1 Mbps 8N1 波形和 F12 是否有意外活动。
- 完成机械臂底座加固、净空、急停/断电演练、现场安全员和 180°点位示教；这些不能由代码替代。
- 第一次真实接线、GET_ANGLES 和任何动作必须由用户在现场执行、观察并能立即断电。
- 保存串口日志、截图、照片、录像编号和 PASS/FAIL 结论。

### 6.3 必须协作完成

| 任务 | Codex 负责 | 用户负责 | 谁能最终放行 |
|---|---|---|---|
| 正式 SoC BSP | 核对生成物、修构建和地址映射 | 在 GUI 生成并提供路径/日志 | Codex 代码审查 + 用户确认生成来源 |
| PNR/bitstream | 分析日志、修改受控工程文件、做 diff 审查 | 操作 Efinity、确认板型/许可证/GUI 结果 | Codex Gate；PNR/STA 日志必须 PASS |
| 烧录 CPU 自检 | 生成并核验 ELF/manifest、准备命令 | 确认板卡和 J52 断开，执行/授权 Programmer | 用户现场确认 + Codex核对日志 |
| UART2 回环 | 写驱动与测试、解析波形/日志 | 连接回环或分析仪、测量电平 | 双方按 D2 判据 |
| 真实臂只读 | 生成只读白名单固件、解析响应 | 断电接线、现场观察、保存 30 次记录 | T0/D2 全 PASS 后共同放行 |
| 首次动作 | 审查状态机、点位、限速、回退和 Review Packet | 固定机械臂、清场、掌握断电并实际操作 | 用户明确现场确认 + Codex Gate，缺一不可 |

## 7. 用户最小学习路线

用户不需要先学会重写 C 状态机、MIPI RTL 或手工编辑 `.peri.xml`。只需达到能够安全提供现场真源和操作工具的程度。

| 顺序 | 学习主题 | 建议练习 | “学会”的判据 | 对应阶段 |
|---|---|---|---|---|
| U0 | 四种固件模式与安全门 | 能口头解释 disabled、simulated、readonly、real 的区别 | 知道 simulated 仍未验证 UART/实物；看到 real 构建会主动核对 Review Packet | G0–G3 |
| U1 | Efinity SoC 与 CPU 烧录基础 | 在不接机械臂时找到 SoC 生成物、bitstream、Programmer/JTAG 和 UART0 控制台 | 能指出同一生成批次的 `soc.h`、linker、bitstream、ELF，并保存完整构建/烧录日志 | G4–G5 |
| U2 | TTL UART 与逻辑分析仪 | 用已知 USB-UART 或测试信号练习识别 TX/RX/GND、空闲高电平、8N1 和 1 Mbps | 能从波形读出位宽约 1 us，判断方向、帧字节和是否丢字节 | G5–G7 |
| U3 | 串口日志与证据 | 学会选择正确 COM、115200 UART0，保存原始文本和时间戳 | 能区分 boot banner、CPU 未启动、波特率错误、APB 错误和测试 FAIL | G5–G6 |
| U4 | myCobot 只读协议 | 人工核对 Basic/Atom 固件前置、`FE FE 02 20 FA`、角度响应命令/长度/尾字节和 500 ms 应答规则 | 能说明协议无事务序号、首测 single-flight/不快于 1 Hz、0x22/0x66/0x67/0x29 无 ACK；看到其他主动命令会立即停止 | G7–G8 |
| U5 | 机械安全与点位 | 演练断电、净空、底座外部基准、低速单段和录像 | 能在异常 1–2 秒内停止供电；能说明 HOME/home_ready/hover/drop 的物理意义 | G9–G10 |
| U6 | 现场证据与回退 | 按模板完成一次无臂烧录记录 | 任何 FAIL 都能指出回退固件和不得继续的下一门 | 全程 |

优先学习资料：

- `learning_guides/mechanical_arm_cpu_interaction_guide.md`
- `final_project/docs/technical_plans/mycobot_board_bringup_operator_sop_20260712.md`
- `final_project/docs/review_packets/mycobot_uart2_j52_wiring_review_20260713.md`
- `final_project/integration/io_pin_map.md`
- Efinity/RISC-V 赛方例程只用于学习生成和烧录流程，不能直接复制地址到正式工程。

## 8. 分阶段实施与验收

### G0：冻结基线与创建工作分支

Codex：

1. 检查 `main`、`origin/main`、dirty/untracked 文件和最新 `CURRENT_STATE.md`。
2. 在不清理用户改动的前提下创建独立工作分支，例如 `codex/arm-board-bringup-20260714`。
3. 运行当前 myCobot skeleton、round controller 和必要的 CPU Host 基线。
4. 将构建产物放入隔离临时目录；结束后清理，保证 `git status` 只剩进入会话前的改动。

用户：只需确认本阶段机械臂/J52 不参与，无需接板。

PASS：基线结果可复现，工作区没有被测试产物污染。

STOP：任何基线回归失败，先修纯软件，不开始构建模式重构。

### G1：建立构建隔离和失败即安全的默认值

Codex：

1. 重构 Makefile 的入口和源文件选择，新增 build profile/backend。
2. 新增 `arm_build_profile.h` 和 PowerShell 构建包装器。
3. 正式板上构建必须同时给出生成 `soc.h`、linker、startup/debug profile；缺任何一项失败。
4. build manifest 至少记录：Git commit、dirty 状态、APP_PROFILE、ARM_BACKEND、工具链、`-march/-mabi`、SoC/linker/bitstream 哈希和时间。
5. `real` 构建包装器必须要求一个已存在且明确 GO 的 Review Packet 路径；这只是软件门，不能替代现场确认。
6. 输出文件名必须编码 profile/backend，例如 `arm_bringup_simulated_<build-id>.elf`；`STANDALONE_TEST` 产物放入独立目录并带 `NOT_FOR_FLASH`，不能继续叫与正式固件相同的 `final_decision_app.hex`。
7. 在正式部署命令尚未验证前，`deploy` 脚本必须报错并以非零退出，不能打印提示后返回成功。
8. G4 前 `-BoardBuild` 必须在创建任何正式命名 ELF/HEX/BIN 之前失败；G4 后为 board artifact 使用独立 verifier，不能复用“只允许 `NOT_FOR_FLASH`”的 G0–G3 verifier。
9. PowerShell 和 Makefile 不得各自维护会漂移的 source/profile truth；选一个规范入口，另一个做薄包装或用 parity test 强制一致。
10. manifest 增加实际源文件清单、完整编译/链接 flags，以及 `soc.h`、linker、startup、bitstream 和最终制品的 SHA-256；仅检查路径存在不等于证明来自同一 SoC 批次。
11. warning policy 默认零 unexpected warning；仅允许 manifest 明确登记的 `NOT_FOR_FLASH` placeholder warning。

用户：学习 U0；不需要操作硬件。

PASS：

- 默认构建显示 `competition + disabled`；
- invalid combination 和缺真源的 board build 均失败；
- `STANDALONE_TEST` 产物被标记 `NOT_FOR_FLASH`；
- board build 在生成正式命名制品前 fail closed；manifest 可追溯且双入口 source list 一致；
- 原 CPU Host 回归保持通过。

### G2：在正式主循环中接入 disabled 路径

Codex：

1. 新增 `arm_runtime`，把 `round_controller` 作为唯一请求源。
2. 第一版 `main.c` 默认只使用 `ARM_BACKEND=disabled`；在没有受审事件/APB 源时不得合成 `event_valid` 或 `observation_valid`，此阶段明确称为 structural bridge。
3. 把逐轮输出映射到可解释日志/状态；目标轮为 `ARM_NOT_READY`，非目标轮保持 SKIP 理由。legacy matcher 的 `GRAB` 不得在 simulated competition 中冒充已接受的逐轮机械臂事务。
4. 增加 20 轮 Host 测试：7 个目标轮、13 个非目标轮；disabled 模式总动作请求和 backend 调用均为 0。
5. 保留 16-bit event/ACK 回绕、重复、过期、busy/fault 测试。
6. Host/QEMU 使用显式 test clock；正式超时在 G4 接入受审的单调硬件时基前，不得用主循环迭代次数冒充毫秒。

用户：学习逐轮唯一请求的概念，无需写代码或接硬件。

PASS：`main -> round_controller` 调用链存在；matcher 不直通 arm；无真事件源时 round 保持安全空闲；20轮无死锁、零动作；日志/OSD 不把 legacy matcher 输出写成 round transaction 结果。

STOP：识别主循环回归、事件 ACK 或错误锁存出现回退，停止扩展 simulated。

### G3：实现目标侧 simulated 后端和独立 bring-up 入口

Codex：

1. 从现有测试 Mock 抽取目标可编译、无 MMIO 的模拟 transport。
2. 新建 `arm_bringup_main.c`，通过 UART0 输出确定性用例结果。
3. 运行 Host、QEMU 和 RISC-V strict build/link。QEMU linker entry 必须与 startup 导出符号精确一致，`TimeoutSeconds` 必须真实生效，shim/startup 各自检查退出码，任何 unexpected warning 均失败。
4. 用 ELF/map/nm 验证：
   - 有 `round_controller`、`arm_runtime`、`arm_controller`、`arm_sim_transport`；
   - 无 `mycobot_uart`、UART2 MMIO 和真实动作 backend；
   - manifest 明确为 `NOT_FOR_FLASH` 并记录 provisional truth；不得伪装成 board manifest；
   - 不再出现“机械臂源码编译了但最终 ELF 被全部回收”。
5. 覆盖 happy path、读失败、软超时、一次重试、永久失败、busy/fault、cancel/re-init/done 后下一请求和固定种子交错 20 轮。
6. disabled bring-up 同样运行 20 轮零请求自检，使 `competition/arm_bringup × disabled/simulated` 四组合使用一致且合理的 ELF 门。

用户：无需硬件；可学习如何阅读结构化 PASS/FAIL 日志。

PASS：四种 RISC-V 组合 4/4 通过；形成第一份“含目标代码但仍禁止烧录”的 `arm_bringup + simulated` ELF、map 和 manifest；QEMU 真实入口/超时通过，且零 unexpected warning。

重要边界：这仍不能烧，直到 G4 的正式 SoC/bitstream/部署链通过。

### G1.5–G3.5：已关闭的纯软件检查点（2026-07-14）

已按 `../review_packets/mycobot_g1_g3_protocol_checkpoint_review_20260714.md` 关闭，证据仅适用于 `NOT_FOR_FLASH`：

1. QEMU 链接入口统一为 `_start`；每次 shim/startup 编译与链接分别检查退出码；有界子进程等待已实现，并以无限循环固件的 1 秒超时注入验证；仅 `board_io.h` standalone placeholder warning 可被 manifest 登记，其余 warning 失败。
2. `arm_bringup + disabled` 现运行固定交错的 20 轮零请求自检，保留 `round_controller_tick` 的 required-symbol 门；`competition/arm_bringup × disabled/simulated` RISC-V 矩阵已为 4/4。
3. G4 前 `-BoardBuild` 在创建任何输出目录/ELF/HEX/BIN 前 fail closed；G4 的生成 SoC 输入与 board-policy verifier 仍未实现，不能借此生成或部署制品。
4. `build_arm_profile.ps1` 是唯一规范构建入口，Makefile 只作薄包装；manifest v2 记录完整输入/flags、warning policy 及输入/制品 SHA-256，ELF 验证使用精确符号、源码/flags、map 和反汇编的 UART2/real-transport 排除证据。
5. `main.c` 保持无受审事件源时不合成事件/观察；不再把循环计数标为毫秒，legacy matcher 结果只作旧识别/判断证据而不写成逐轮机械臂事务。
6. 完整 Host、严格 QEMU、RISC-V/ELF 和 BoardBuild fail-closed 矩阵已重跑；本节完成后必须停止，等待用户决定是否准备 G4。

### G4：闭合正式 SoC、BSP、PNR 和部署链

Codex：

1. 审查 FPGA/SoC 当前工程和已有隔离候选；不得把候选 `soc.h` 直接冒充 final_project 真源。
2. 设计或审查最小 QCRV32 + UART0 + CLINT + JTAG/BSCAN；独立自检不要求先接完整视觉/APB，但后续 competition 构建必须使用同一受审 SoC 架构。
3. 修复 Interface Designer/periphery 与顶层 IO 导出导致的 PNR/outpad 阻塞；涉及 XML、SDC、顶层或时钟复位时单独 Review Packet。
4. 把生成 BSP、linker、startup、debug profile 和来源说明放入受控目录或提供稳定引用。
5. 将 deploy 占位替换为经过验证的 dry-run/实际命令分离流程。
6. 不把“已有 `soc.h`”当成 BSP 完成：逐项核对 UART0、CLINT、PLIC、内存起点/长度、linker 中 `_sidata` 与 `.data/.bss`、栈顶、startup 的 `mtvec`/trap 行为和中断路径。当前 provisional startup 的异常/中断路径不能直接作为正式可恢复运行证据。

用户：

1. 完成 U1，准备 Efinity 2025.2、许可证、开发板 USB/JTAG 和可辨认的 UART0 端口。
2. 执行需要 GUI 的生成、map/PNR/STA、Programmer/JTAG 操作并保存日志。
3. 每次操作前确认机械臂和 J52 信号线断开。

PASS：

- production/bring-up 对应的 map、PNR、STA 均有可验收日志；
- bitstream、`soc.h`、linker、ELF 来自同一可追溯配置；
- 连续 3 次复位都有 CPU Hello；
- Programmer/JTAG 操作可重复，不靠临时 GUI 状态。

STOP：PNR/STA 失败、地址冲突、CPU Hello 不稳定或生成批次混用时，不烧 arm simulated ELF。

### G5：机械臂断开的第一次板上模拟

现场前置：J52 三根信号线和 Pin4 全部断开；机械臂可不在现场。只连接开发板供电、JTAG/Programmer、UART0 控制台；如有逻辑分析仪，探测 F12 但不与机械臂相连。

Codex：

1. 核对 manifest、bitstream、ELF、commit 和 backend=`simulated`。
2. 提供明确的烧录命令和预期启动日志；先 dry-run，再等用户确认板卡状态。
3. 分析 UART0 日志、状态轨迹和 ELF/现场观测的一致性。

用户：

1. 确认 J52 断开并拍照/记录；确认当前目标板和供电。
2. 执行或明确授权烧录，保存完整 Programmer/JTAG 输出。
3. 采集 UART0 原始日志；连续复位 3 次。
4. 如有逻辑分析仪，确认 F12 在整个 simulated 测试中没有 UART 帧活动。

PASS：

- 3/3 次启动 banner 显示正确 profile/backend/build id；
- happy/fault/retry/20轮所有用例 PASS；
- 7 个目标轮各一次请求，13 个非目标轮零请求；
- UART0 日志与状态计数一致，无卡死/看门狗式重启；
- F12 无 UART 活动；
- 运行至少 10 分钟无异常。

达到本门，才可以说“机械臂代码已烧到 CPU，并在不连接实物臂时完成板上模拟检测”。

### G6：competition + simulated 的整链干跑

Codex：把 G2 主循环切到 simulated 后端，接入实际事件源/APB/OSD，保持 UART2 排除。

用户：按评委流程操作任务配置、摆放确认、移除确认并录像。

PASS：四任务 20 轮、≤10分钟、识别/判断/理由锁存正确，非目标零动作，目标由模拟后端各完成一次，OSD/UART/记录一致。

STOP：任何重复请求、事件卡死或 OSD 证据不一致，退回 G2/G3；不得用真实臂帮助定位。

### G7：UART2 无机械臂回环

本门开始实现 `mycobot_uart`，但机械臂仍断开。

Codex：使用正式 `soc.h` 实现 1 Mbps 8N1 有界轮询；复用现有 `mycobot_transport`；生成 readonly 构建。先加入官方示例帧/边界向量，再实现单请求在途事务：匹配 expected command 和精确 payload 长度，初始 deadline 750 ms，超时后先重同步再允许下一请求；官方 `LEN=0x02..0x10` 外或白名单外帧不得推进状态机。

用户：完成 U2，连接本地回环/3.3V兼容监听器/逻辑分析仪，测量 F12/C14、GND 和空闲电平。

PASS：100/100 帧、速率/位宽正确；官方 GET_ANGLES 示例和 signed 边界向量通过；超时可退出；错误命令/长度、迟到/重复、断线/坏尾/噪声注入后可计数并重同步；全程最多一个请求在途，VCC 悬空。

STOP：任何方向、电平、分频或约束不确定，继续保持 J52 与机械臂断开。

### G8：真实机械臂只读

前置：`io_pin_map.md` 八项全 PASS；结构、线序、双端电平、共地、断电方式和安全员均签核；确认并记录 Basic `transponder` 与最新版 Atom `atomMain` 的版本/文件哈希。官方页只证明 USB Type-C 协议，不得用它替代 J52 端口的独立电气和抓包证据。

Codex：构建 `readonly` 白名单固件；向逐轮控制器固定报告 `arm_enabled=0`；静态检查不得含 `SEND_ANGLES`、夹爪、STOP 和扭矩命令可达路径；只允许 0x20，single-flight、初始 deadline 750 ms、首测不自动重发；解析 expected command/length、迟到/重复/未知帧计数。

用户：断电接线，J52 Pin4 悬空；现场以不快于 1 Hz 执行 GET_ANGLES，能立即断电。

PASS：30/30 次响应，命令/长度/尾字节/角度解析和逐关节范围正确；零超时、零迟到/重复/未知帧、零动作、无结构位移；原始 TX/RX 与固件版本证据完整。

STOP：任何未知帧、动作、超时或电气异常立即断线，回退 G7。

### G9：PLIC/Ring Buffer 与 readonly dry-run

在稳定轮询固件保留为回退版本后再做。

Codex：ISR 只 claim/read/push/count/complete；主循环解析；比较轮询/中断结果。

用户：重复只读和 100 帧压力记录，不做动作。

PASS：轮询/中断一致、0 overflow、claim/complete 一致、故障可回退。

STOP：中断不稳定时比赛前允许保留经证明的有界轮询，不为“架构漂亮”冒动作风险。

### G10：首次低速空载单段

只有 D0–D4、G0–G9、动作 Review Packet、180°点位版本和用户现场明确确认全部通过才进入。

G10 前先完成 §4.5：新增不能被 matcher 调用的受限 service-move 接口；加入运行时 `real_armed`、急停/transport/实际姿态 precheck、逐关节官方绝对限位和点位安全裕量。backend stop 必须能有界发送一次 0x29，随后用 0x2B + 0x20 核验；因为 0x29 无 ACK，该能力只能作为附加防线。首次只允许 `HOME -> home_ready`，低速、空载、不闭夹爪；0x22 发出后不等待 ACK，到位依靠 GET_ANGLES 连续读回，最多一次受控重试。Codex 负责代码与日志审查，用户负责现场固定、净空、安全员和断电。任何异常立即进入 FAULT 并退回 readonly/F1。

### G11：F2 抓放与比赛启用

先补齐夹爪确认：0x66/0x67 发出后不得等待 ACK；single-flight 轮询 0x69 至停止，再读取 0x65 至稳定窗口，并覆盖超时、响应错配、接触未到目标值和滑落失败。该读回不等于夹持力证明，仍需带载抬升观察。随后按空载全路径、单一 2cm 正方体、2/2.5/3cm 覆盖、至少 5 轮低速带载、连续目标动作和整场演练逐级推进。最终 Gate D 仍以主方案为准：180°±10°、最大臂展、底座无位移、轻取轻放、无碰撞/跌落、单轮唯一请求和完整证据。未过门保持 F1。

## 9. 自动化测试与静态检查矩阵

| 层 | 必测内容 | 最低通过标准 |
|---|---|---|
| 既有回归 | classifier、params、matcher、round、contract、flow、A13、myCobot skeleton | 当前基线不回退；具体计数以运行日志为准 |
| runtime Host | disabled/simulated 请求、busy/done/fault 映射、20轮 | 7目标各一次、13非目标零次、无死锁 |
| 模拟故障 | read fail、soft timeout、soft pass、retry once、retry failed | 与 `arm_controller` 既有策略一致，失败停线 |
| 官方协议向量 | 0x20/0x22/0x29/0x2B/0x65/0x66/0x67/0x69、负角、LEN 2/16/越界 | 精确帧与官方示例一致；错误命令/长度/域被拒绝 |
| UART 事务 | single-flight、500 ms 官方窗口/750 ms deadline、迟到/重复/未知帧、超时重同步 | 最多一个请求在途；错帧不推进状态机；计数可审计 |
| REAL 安全校验 | J1..J6 官方绝对范围、点位裕量、0x22/0x29 无 ACK | 越界构建/运行均拒绝；完成/停止只靠独立回读与现场保护 |
| 夹爪确认 | 0x69 停止 + 0x65 稳定窗口、超时/接触/滑落 | 不以 TX 或 API 返回当完成；失败进入 FAULT |
| 构建组合 | 允许/禁止组合、默认值、缺真源 | 默认 disabled；非法组合和缺真源失败 |
| ELF disabled | 不可达真实动作/UART2 backend | `nm/map` 无真实 UART2 路径 |
| ELF simulated | arm/round/sim 符号存在，真实 UART2 路径不存在 | 不再被 `--gc-sections` 回收；无 MMIO backend |
| ELF readonly | UART2 与 GET_ANGLES 可达，动作/夹爪命令不可达 | 白名单静态检查 PASS |
| ELF real | 完整路径、正确点位版本、Review Packet 标识 | 仅 Gate 后生成，不作为默认产物 |
| 板上 simulated | boot、3复位、10分钟、20轮、F12静默 | 全部 PASS |
| UART2/只读/动作 | 100/100、30/30、分级动作 | 分别按 G7/G8/G10/G11 |

## 10. 每次会话和上板的证据模板

每个硬件会话创建：

```text
final_project/docs/debug_sessions/mycobot_cpu_board_run_YYYYMMDD_NN.md
```

至少记录：

- Git branch、commit、进入会话前后的 dirty 状态；
- Efinity/RISC-V 工具版本；
- APP_PROFILE、ARM_BACKEND、完整编译命令；
- bitstream、ELF、`soc.h`、linker、startup 和 manifest 的路径/SHA-256；
- 开发板、J52、机械臂、JTAG、UART0、UART2 和仪器连接状态；
- 用户现场确认：供电、GND、VCC NC、断电方式、安全员；
- Basic `transponder`、Atom `atomMain` 的版本/文件哈希、来源与核验时间；
- UART0/UART2 原始日志、波形/截图/录像编号；
- 每类命令的 TX/RX 数量、expected command/length、最大响应时延、超时/迟到/重复/未知帧计数；
- 每个用例的预期、实际、PASS/FAIL/WARN；
- 是否发过动作帧；若为否写明静态和物理证据；
- 回退 bitstream/ELF 和下一门是否放行。

大型 bitstream/录像不必直接提交仓库，但必须记录稳定路径、文件名、时间和哈希。禁止只写“烧录成功”而没有使用的两个制品和日志。

## 11. 新对话的推荐执行边界

### 11.1 第一轮先关闭 G1.5–G3.5

当前 G1–G3 已有初始草稿，下一轮由 Codex 在现有分支上做加固，不重复从零实现：修复构建/测试门，收紧 disabled 主循环语义，补齐 simulated 自检，并重跑 Host/QEMU/RISC-V/ELF 矩阵。此轮不打开 Efinity GUI、不修改 FPGA 工程、不烧录、不连接 J52。

第一轮结束必须交付：

- 修改文件与关键 diff，并逐项关闭 G1–G3 审查包问题；
- 全部回归命令和日志；
- 四种 profile/backend 的 4/4 结果及 `arm_bringup + simulated` ELF/map/manifest 生成方法；
- `nm/map` 证明 arm 代码存在、UART2 backend 不存在；
- G4 仍缺的 SoC/PNR/烧录真源清单。

### 11.2 第二轮在用户准备好硬件后做 G4–G5

用户开场需明确提供：

1. 开发板是否在手、是否能正常供电；
2. J52 和机械臂是否完全断开；
3. Efinity 2025.2、许可证、JTAG/Programmer 是否可用；
4. UART0 对应端口是否已识别；
5. 是否有逻辑分析仪/示波器；
6. 是否同意本轮最多做到 simulated，禁止 UART2 和真实臂。

### 11.3 可直接复制给下一对话的开场指令

```text
请按 final_project/docs/technical_plans/
mycobot_cpu_board_bringup_implementation_plan_20260714.md 执行。

当前分支 codex/mycobot-g0-g3-bringup-20260714 已有 G1-G3 初始草稿。
先阅读 final_project/docs/review_packets/
mycobot_g1_g3_protocol_checkpoint_review_20260714.md，只关闭 G1.5-G3.5：
修复 QEMU entry/真实超时/退出码与 warning 门，修复 arm_bringup+disabled，
补 BoardBuild fail-closed、manifest 哈希/source/flags 和构建入口一致性，
明确 main 的 structural bridge/时基/legacy matcher 语义，再重跑完整矩阵。

协议真值以 final_project/integration/mycobot_protocol_notes.md 为准；
0x22/0x66/0x67/0x29 均无 ACK。本轮 G0-G3 不实现真实 UART 命令，
也不得把模拟 backend 的函数返回解释成真实机械臂完成证据。

本轮不修改 FPGA RTL/SDC/XML，不烧录，不连接 J52 或机械臂，不产生真实 UART2 帧。
保留当前 dirty 文件；每个 checkpoint 给出修改、测试、未验证项和回退方式。
只有 Host、严格 QEMU、四种 RISC-V 组合 4/4、ELF/manifest 门全部通过，
才把 G3 标为 PASS。随后停止，等待我确认是否具备 G4-G5 的板卡/Efinity/JTAG 条件。
```

## 12. 完成定义

### “可以烧录做无臂模拟”

必须同时满足：G0–G4 PASS、正式制品链可追溯、simulated ELF 包含真实 arm/round 代码且排除 UART2 backend。仅 Host/QEMU 或 placeholder ELF 不算。

### “已完成板上无臂模拟”

必须满足 G5 全部判据，包括真实 QCRV32 执行、3次复位、20轮、10分钟、UART0 证据和 F12 静默。

### “可以接真实机械臂只读”

必须满足 G7、T0 八项、接线 Review Packet、Basic/Atom 固件前置和 official-vector/single-flight 事务测试；板上 simulated 通过不能替代电气门。

### “可以启用比赛动作”

必须满足 G8–G11、Gate D 和用户现场安全确认。任何较低层完成都不得外推为动作放行。

## 13. 截止线与回退

决赛主方案的既定规则继续有效：若 2026-07-16 12:00 前板到臂 UART 电气、回环和无运动/只读验证未通过，冻结 F1，无机械臂动作；7月17日基础保底冻结后不扩大架构。无论日期压力多大，都不得跳过 simulated、T0、回环、只读或动作 Review Packet。

最优回退链：

```text
real -> readonly -> simulated -> disabled/F1
```

每一级都必须有独立、可识别的 manifest 和制品，禁止通过现场改宏但不重建/不记录来切换模式。

## 14. 本轮方案落地结论

- 最新 16-bit CPU 安全契约已经进入主线；G1.5–G3.5 工作树通过 Host、严格 QEMU 和 RISC-V/ELF 复验，G0–G3 的纯软件门已 PASS。
- 当前 RISC-V 矩阵为 4/4；`arm_bringup + disabled` 通过真实的 20 轮零请求自检保留 `round_controller_tick`，QEMU `_start` 入口与有界超时已验证，零 unexpected warning。
- 官方协议复核确认现有帧头/长度、0x20/0x22/0x65/0x66/0x67 和角度缩放方向基本正确，但新增了 Basic/Atom 固件、逐关节限位、single-flight/750 ms deadline、无 ACK 动作回读、0x29/0x2B 停止辅证及 0x69/0x65 夹爪确认门。
- G1.5–G3.5 已由 Codex 关闭；G4 起需要用户提供 Efinity/开发板现场操作与生成物；G5 起必须由用户确认并执行物理操作。
- 下一会话不得从 UART2 或真实臂开始；应先由用户决定是否具备 G4 的 Efinity/SoC/PNR/JTAG/UART0 条件。现有 simulated ELF 只含 target code，仍排除 UART2，且不可烧录。
- 当前真实动作继续 NO-GO；本方案本身不构成烧录或动作授权。
