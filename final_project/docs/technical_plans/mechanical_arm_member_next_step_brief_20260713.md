# 机械臂成员下一步简要方案

> 日期：2026-07-13
> 适用对象：机械臂方向负责成员
> 复核依据：Fable 5 架构审核及其后续 Codex 复核
> 当前判定：纯 C 协议、transport、动作控制器和 Mock 已有；J52/C14/F12 与开发板 3.3V VCCIO 已完成文档冻结，但机械臂端线序、电平实测、正式工程约束、结构固定、180°新点位和主循环联动尚未闭环。真实动作当前为 **NO-GO**。
> 2026-07-14 状态覆盖：详细上板实施以 `mycobot_cpu_board_bringup_implementation_plan_20260714.md` 为准。16-bit event/ACK 已合入 `main@d433bca`；当前新阻塞是仓库尚无真实 `ARM_DISABLED` 构建模式、板上 simulated 后端和 `main -> round_controller -> arm_controller` 调用链。

## 一句话结论

**先把 `io_pin_map.md` 的 T0 表作为唯一放行表补齐，再完成 UART2 无臂回环和真实臂只读；动作请求必须只走 `round_controller → arm_controller`，不要继续堆动作代码，也不要先接真实机械臂。**

## Fable 审核后新增的四条硬约束

1. **T0 不是个人自检表，而是 F2 唯一放行表。** `final_project/integration/io_pin_map.md` 中 CPU/SoC、APB、UART2、PLIC、板端电气、机械臂线序、急停和结构八项必须全部 PASS，并由 A/B/C 分别签核。
2. **禁止旧 matcher 直通机械臂。** `main.c` 第一版必须在 `ARM_DISABLED` 下接入正式 `round_controller`；只有控制器锁存的单次 `request_arm_grab` 才能交给 `arm_controller`。
3. **事件契约已修复，但板上调用链仍是阻塞。** 当前 `main@d433bca` 已合入 16-bit `event_seq/ACK`、`ACCEPTED/REJECTED` 和半范围回绕；它仍只在 Host/Mock 层有证据。`main.c` 未调用 `round_controller`/`arm_controller`，仓库也没有实际的 disabled/simulated 板上构建模式，因此不得把契约修复外推为板上可用。
4. **首次动作试验与 F2 比赛启用是两道门。** T0、回环、只读、dry-run 通过后只允许首次低速空载试验；F2 比赛启用还必须补齐 180°点位、至少 5 轮低速带载、电气/共地、急停/断电、底座固定和全程无跌落证据。

## 你现在先做什么

### P0：今天先交付 T0-C 机械臂侧证据包

1. **固定机械结构**
   - 加固机械臂底座、相机、摆放区和目标区。
   - 设置外部基准标记，记录动作前后是否有底座相对位移；只看 `get_coords()` 不能证明底座没动。
   - 交付整体、底座连接处、工作区净空的照片或录像编号。

2. **冻结安全条件**
   - 明确现场断电/急停方式、执行人和安全观察员。
   - 写清发生异常时的动作：停止下发新命令、进入 FAULT/ESTOP、人工确认后再恢复；不得默认自动释放扭矩。

3. **补齐机械臂端接线真源**
   - 开发板侧已冻结并经用户人工图像核验：J52 位于 Type-C 与 MIPI DSI 之间；Pin1 GND，Pin2 `RXD/C14/Input`，Pin3 `TXD/F12/Output`，Pin4 VCC 悬空；C14/F12 均为 3.3V VCCIO。
   - 正式候选为 GND→GND、C14/RXD←机械臂 TX、F12/TXD→机械臂 RX；机械臂独立 12V5A，J52 VCC **严禁连接**。
   - 机械臂官方 USB-TTL 说明存在 TX→TX/RX→RX 的命名冲突；与 FPGA/SoC 成员在断电状态双人复核针脚方向，并实测机械臂 TX 空闲电平后，才能把线序/直连兼容性标为 PASS。
   - 证据入口：`final_project/docs/review_packets/mycobot_uart2_j52_wiring_review_20260713.md`。

4. **版本化记录 180°点位**
   - 使用 PC 仅做低速、单步示教，记录 `HOME`、`home_ready`、`pick_hover`、`pick`、`drop_hover`、`drop`。
   - 记录六关节角、速度、夹爪值、工件类型和碰撞余量。
   - `drop` 必须面向官方要求：相对起点旋转 180°（±10°）且位于最大臂展处。
   - 新点位另存版本，不覆盖已成功的旧基线；旧约 10 cm 路径不能冒充决赛点位。

T0-C 的通过标准：上述证据全部写入 `final_project/integration/io_pin_map.md` 并附对应 Review Packet；任一项仍为“未知”即保持 NO-GO。机械臂成员只负责 C 侧事实，但进入下一门前必须确认 A/B 侧六项也已签核，不能用口头“已完成”代替。

### P1：与 FPGA/CPU 成员配合完成 D2 无臂 UART 验证

在 A/B 成员提供真实 `soc.h`、UART2 基址/时钟/引脚后：

1. 机械臂断开，只接本地回环、USB-UART 监听器或逻辑分析仪。
2. 确认 **1,000,000 bps、8N1**、空闲电平和 TX/RX 方向。
3. 发送预期只读帧 `FE FE 02 20 FA`，验证原始字节、帧边界和超时。
4. 完成 100 帧无丢字节测试并保存波形、日志和工具配置。

通过标准：100/100 帧正确，波特率实测正确，超时可退出；失败时只排查时钟、引脚、电平和方向，不接真实臂试错。

### P2：只读与 `ARM_DISABLED` 联调

1. T0、APB 最小闭环和 D2 全部 PASS 后，真实臂只允许低频 `GET_ANGLES`。
2. 连续 30 次返回均可解析，记录 TX/RX 十六进制、延迟、超时和错误码。
3. CPU 联调必须验证 `main.c → round_controller → arm_controller`，不得从 `task_matcher_evaluate()` 或日志坐标直接触发机械臂。
4. 第一版强制 `ARM_DISABLED` / dry-run：20 轮中目标轮只记录一次“将请求动作”，非目标轮 `SKIP` 必须零动作；busy/fault、重复/过期事件和非法状态均不得产生新请求。
5. 联调前确认 16-bit `event_seq/ACK` 修复已进入当前协作主线并有可复现测试；仅存在于探索分支不算通过。

### P3：分两级放行动作

**D5 首次受控动作试验门：** T0 全 PASS、D2 100/100、D3 30/30、`ARM_DISABLED` dry-run 和动作前 Codex Review Packet 全部通过后，现场人员才可明确放行低速、空载、单段 `HOME -> home_ready`。任何异常立即停止新命令并退回只读模式。

**F2 比赛启用门：** 在首次试验通过后，逐段验证固定关节角路径，再使用单一已验证正方体完成至少 5 轮低速带载。每轮必须满足：

- 同一轮只有一次动作请求，机械臂 busy 时不接受新物体；
- 夹取、抬升、180°±10°放置和回 `home_ready` 全程可追溯；
- 底座外部基准无相对位移，急停/断电可用；
- 无提前释放、未夹稳、碰撞或跌落；
- 保留动作前后角度、TX/RX、耗时、错误码和连续录像。

上述证据经 Gate D / Codex Review 后，才能在比赛构建中把机械臂从 `ARM_DISABLED` 升级为 F2。PC、`pymycobot` 或临时脚本不得进入正式比赛闭环。

若 **2026-07-16 12:00** 前板到臂安全链未通过，冻结 F1（机械臂禁用），不要在最后一天临时接动作。

## 你需要补的知识（按优先级）

| 优先级 | 知识 | 学到什么程度算够用 |
|---|---|---|
| 1 | TTL UART 与测量 | 会解释 TX/RX/GND、逻辑电平、共地、8N1、1 Mbps；会用逻辑分析仪判断波特率、方向、丢字节和空闲电平。 |
| 2 | myCobot 帧协议 | 看懂 `FE FE LEN CMD PAYLOAD FA`、big-endian int16、角度 ×100；能人工核对 `GET_ANGLES` 请求和 12 字节角度响应。 |
| 3 | 非阻塞状态机与事务握手 | 理解 `tick`、超时、有限重试、读回确认、busy/fault/ESTOP；能解释 16-bit `event_seq/ACK`、接受/拒绝、回绕与“单轮唯一请求”，知道为什么连续视频帧不能重复触发。 |
| 4 | 关节空间示教与机械安全 | 会设计 hover 点、低速单段验证、工作区净空、夹持余量、底座外部基准和异常停机流程；不依赖笛卡尔实时伺服。 |
| 5 | 比赛动作与证据记录 | 熟悉 180°±10°、最大臂展、轻取轻放、跌落判定和唯一响应；能整理照片/录像、点位表、原始串口、耗时与 PASS/FAIL/WARN。 |

暂时不用深挖：MIPI/视频 RTL、纯 FPGA 分类、重写已有协议/transport、复杂 PLIC 实现细节，以及把 `pymycobot` 直接翻译成正式闭环。你只需能与 A/B 成员核对接口真源和验收证据。

## 最小交付清单

- [ ] T0-C：底座/场地固定与外部基准记录
- [ ] T0-C：断电/急停/安全员说明
- [ ] T0-C：机械臂端 TX/RX/GND/电平/连接器证据
- [ ] T0-C：新版本 180°六点位表，不覆盖旧基线
- [ ] T0：`io_pin_map.md` 八项全部 PASS，A/B/C 签核并附 Review Packet
- [ ] D2：UART2 1 Mbps 8N1 无臂波形与 100/100 帧日志
- [ ] D3：30/30 次 `GET_ANGLES` 只读日志（前门全 PASS 后）
- [ ] CPU：16-bit event/ACK 修复已进入当前主线，`main → round_controller` 在 `ARM_DISABLED` 下通过 20 轮 dry-run
- [ ] D5：动作前 Review Packet、现场明确放行和低速空载单段记录
- [ ] Gate D：至少 5 轮低速带载、180°±10°、底座稳定且无跌落
- [ ] F2：Codex 复核通过后才允许比赛构建启用机械臂

## 依据与入口

- 当前状态：`CURRENT_STATE.md`
- Fable 架构审核：`final_project/docs/review_packets/architecture_audit_fable5_20260713.md`
- 决赛主方案：`final_project/docs/technical_plans/competition_score_maximization_execution_plan_20260712.md`
- 详细板上调试方案：`final_project/docs/technical_plans/mycobot_board_debug_execution_plan_20260712.md`
- 操作员 SOP：`final_project/docs/technical_plans/mycobot_board_bringup_operator_sop_20260712.md`
- T0 真源表：`final_project/integration/io_pin_map.md`
- 协议真源：`final_project/integration/mycobot_protocol_notes.md`
