# myCobot 板上操作员 SOP（基于团队整合 `510caca`）

> 日期：2026-07-13（基于 2026-07-12 SOP 更新基线）
> 适用对象：A（FPGA/SoC）、B（CPU）、C（机械臂现场安全员）
> 总原则：每一门只增加一个新变量；前门未 PASS，禁止进入后门。真实动作当前为 **NO-GO**。
> 2026-07-14 协议勘误：事务与命令语义以 `mycobot_cpu_board_bringup_implementation_plan_20260714.md` 和 `final_project/integration/mycobot_protocol_notes.md` 为准；官方确认有返回命令在 500 ms 内响应，而 0x22/0x66/0x67/0x29 无返回值。

## 0. 先看当前结论

| 项目 | 当前事实 | 操作判定 |
|---|---|---|
| CPU 分支 | 本地统一筛选分支 `codex/team-integration-20260713@510caca`，保留两位队友提交历史；`origin/main` 未改写 | 可继续本地筛选/开发 |
| 四任务与逐轮控制 | matcher、完整 round controller、轻量 transaction、契约与 snapshot flow 已整合；Host 合计 795/795 | 仅 Host GO；`main`/APB/OSD 未闭环 |
| myCobot 协议/transport | `mycobot_protocol`、内存 Ring Buffer、TX 队列已有；QEMU asserts PASS | 纯 C/QEMU GO |
| UART2 硬件层 | `mycobot_uart.c/.h` 不存在；UART2 基址/IRQ/分频/引脚无真源 | NO-GO |
| APB/SoC | 视频工程内 SoC、正式 `soc.h`、APB 从机/OSD 证据未闭环 | NO-GO |
| 点位 | 板上默认表仍是旧点位；180°新点位只在 PC 调试资料中 | 动作 NO-GO |
| 机械结构 | 当日记录存在底座连接微倾，尚无外部基准复核 | 动作 NO-GO |

## 1. 阶段 S0：冻结版本并跑无动作回归

在仓库根目录执行：

```powershell
git log -3 --oneline --decorate
git status --short --branch
& '.\final_project\cpu\tests\run_mycobot_arm_skeleton_host.ps1'
```

通过判据：

- 第一条应显示当前受审分支与 Review Packet 一致；本版基线为 `codex/team-integration-20260713@510caca`。若切换分支，必须重新核对 `CURRENT_STATE.md` 和整合 Review Packet，不能继续照抄本基线。
- 工作区原有 myCobot 调试改动必须仍在，不得为了“干净”而丢弃。
- 测试最后必须显示 `RESULT: PASS`。本机当前走 Efinity RISC-V QEMU shim，已实际执行 asserts。

若失败：只修 Host/QEMU，不连接开发板 UART2，更不能接机械臂。

## 2. 阶段 S1：填写 T0 真源表

先补全 `final_project/integration/io_pin_map.md`，每一格都必须带证据路径或照片编号：

| 必填项 | 必须记录的值 | 可接受证据 |
|---|---|---|
| CPU 真源 | 视频工程内 SoC IP、生成 `soc.h`、linker、ELF 下载方式 | 生成目录、Efinity 工程节点、CPU Hello 日志 |
| APB 真源 | 用户 APB 基址、`REG_MAGIC` 地址、时钟/复位 | `soc.h` + RTL/工程 XML + 读回日志 |
| UART2 真源 | 基址、输入时钟、1 Mbps 分频、TX/RX FIFO 位、IRQ | `soc.h`/生成驱动/RTL/逻辑分析截图 |
| PLIC 真源 | UART2 source ID、priority、enable、threshold、claim/complete | 生成文件或赛方示例的对应实现 |
| 物理接口 | 板端引脚、方向、空闲电平、逻辑电平、GND、是否需电平转换 | 原理图/管脚表 + 万用表/示波器记录 |
| 急停 | 输入来源、默认电平、失效安全、CPU 可见路径 | 原理图/RTL/实测日志 |

只读核对命令：

```powershell
rg --files final_project | rg '(^|[\\/])soc\.h$'
rg -n 'UART|PLIC|IO_APB_SLAVE|CLINT' <生成的soc.h或BSP目录>
```

停止条件：任何基址、IRQ、引脚或电平仍是“猜测/占位”，S1 判 FAIL。禁止把 `bsp.h` 的历史宏或 `0xF0000000` 测试占位带进 bitstream。

## 3. 阶段 S2：CPU Hello 与 APB MAGIC/心跳

1. 此阶段机械臂控制线保持断开。
2. 用包含 QCRV32 SoC 的视频工程生成 bitstream；CPU 固件必须使用同一次生成的 `soc.h`。
3. 先只烧录 CPU Hello 固件，确认调试 UART 能区分“CPU 未启动”和“串口显示错误”。
4. 再运行：`board_io_validate()` 读 `REG_MAGIC` → 周期写 `CPU_HEARTBEAT` → 写 `ARM_STATE_BOOT/READY`、`ERROR_CODE=0` → `board_io_commit_global()`。
5. 同时保存 UART 日志、APB 逻辑分析/ILA 或 OSD 录像；记录 bitstream、ELF、commit 和 Efinity 版本。

GO：连续复位 3 次均能看到 Hello、MAGIC 正确、心跳递增且状态提交可见。

NO-GO：任一失败只排查 SoC/APB/CDC，不开始 UART2。

## 4. 阶段 S3：实现 UART2 硬件适配，但不接机械臂

当前代码缺少 `mycobot_uart.c/.h`。B 的最小实现必须复用现有 `mycobot_transport_t`：

- `init(baud=1000000, 8N1)`：只使用 S1 真源分频和 MMIO 位；
- `poll_tx(deadline)`：从 `mycobot_transport_tx_pop_byte()` 取字节，有界等待 TX FIFO；
- `poll_rx(deadline)`：读 RX FIFO后调用 `mycobot_transport_rx_push_byte()`；
- `next_frame()`：在主循环调用 `mycobot_transport_next_frame()`，ISR 内不得解析协议或打印；
- 超时、RX overflow、bad footer、noise 和 PLIC 错误必须有独立计数/错误码。

此阶段先用轮询，不开 PLIC。构建后只接逻辑分析仪或已确认 3.3V 兼容的 USB-UART 监听器；不要接真实机械臂。

## 5. 阶段 S4：UART2 1 Mbps 无臂验证

1. 在已签核电平下做 TX→RX 本地回环；GND 共地，机械臂仍断开。
2. UART 参数固定为 **1,000,000 bps、8N1**。
3. 发送只读请求帧的原始字节：

```text
FE FE 02 20 FA
```

4. 连续 100 帧，逐帧记录：发送序号、5 字节原文、逻辑分析仪实测速率、回环字节、超时、overflow/noise/bad-frame 计数。
5. 人为做一次断开 RX、一次坏尾 `00`、一次分段到达，确认超时可返回、坏帧能重同步、下一帧仍可解析。

GO：100/100 正确，实测速率落在设备容许范围，所有等待有超时且错误计数符合注入。

NO-GO：只查时钟/分频/引脚/方向/电平；不得拿真实机械臂“试一下”。

## 6. 阶段 S5：真实机械臂 GET_ANGLES 只读

当前不能执行。只有以下项目全部签字才可进入：

- 底座连接已加固，外部基准复核无可见相对位移；
- S1–S4 全 PASS；机械臂固定、周边净空、断电方式明确；
- 线序按已签核 `io_pin_map.md` 连接，不能凭“TX/RX 应该交叉/同名”猜测；
- 已确认并记录 Basic `transponder` 与最新版 Atom `atomMain` 的来源、版本或文件哈希；
- 固件白名单只允许 `0x20 GET_ANGLES`，禁止 `0x22`、`0x66`、`0x67` 和扭矩/使能类命令。

操作：

1. 先断电接线并复核 GND/逻辑电平，再上电；机械臂保持静止。
2. 首测请求周期不快于 1 s，一次只允许一个未完成请求；响应 deadline 初始为 750 ms，收到匹配帧或超时并完成重同步前不得发送下一请求，首测不自动重发。
3. 预期响应为 `FE FE 0E 20 <12-byte angles> FA`；必须匹配返回命令 0x20、精确 12 字节 payload 和尾字节，再用 `mycobot_decode_get_angles_response()` 解为 6 个 `deg_x10` 关节角并检查逐关节官方范围。
4. 连续 30 次记录 TX/RX 十六进制、响应延迟、expected command/length、解析状态、角度和迟到/重复/未知帧计数；任何错配或超时立即停止发送并断开控制线。

GO：30/30 正确，零超时、零迟到/重复/未知帧、无机械臂动作、无结构位移。

NO-GO：回退 S4；不自动重发动作命令，不释放扭矩。

## 7. 阶段 S6：PLIC + Ring Buffer dry-run

1. 保留 S5 稳定轮询固件作为回退版本。
2. ISR 只执行：claim → 读取全部可用 RX 字节 → `mycobot_transport_rx_push_byte()` → overflow 计数 → complete。
3. `mycobot_transport_next_frame()`、解码、日志、状态机全部留在主循环。
4. 使用同一批 30 次 GET_ANGLES 比较轮询与中断结果，再做 100 帧监听器压力测试。
5. 打开 `ARM_READONLY/dry-run`：`arm_controller_tick()` 只能打印“将发送”，硬件发送白名单仍只含 GET_ANGLES。

GO：轮询/中断解析一致、0 overflow、claim/complete 数一致、dry-run 不发运动帧。

NO-GO：回退轮询只读；不得进入动作。

## 8. 阶段 S7：迁移 180°点位并做低速空载单段

当前仍为 NO-GO。放行前必须：

1. 从 `teach_points_20260712_180deg.json` 生成新的板上 C 点位文件，不覆盖旧 `arm_positions.c` 基线；新旧计划用不同符号/版本号。
2. 对 `HOME`、`home_ready`、`pick_hover`、`pick`、`drop_hover`、`drop` 逐点做单位、角度、速度、最大关节差、半径和夹爪值核对，并通过 `arm_controller_plan_validate()`。
3. 先只允许 `HOME -> home_ready` 一段；不带工件、不闭合夹爪，现场 C 手持断电/急停。
4. 速度采用新点位中经 PC 单步确认的低速值；不得直接套旧默认表的 12/16/20/30，也不得超过 30。
5. 0x22 无返回值，不得等待或伪造动作 ACK；到位后只读角度，严格失败走 `POST_READBACK -> SOFT_PASS / RETRY_ONCE -> FAULT`，最多一次重试。
6. G10 前补充软件 stop：0x29 有界发送后用 0x2B + 0x20 核验；0x29 无返回且不是安全额定急停，现场断电/急停不可撤销。

任何结构位移、姿态异常、碰撞、超时、解析错误、急停或无法立即断电：立即停止新命令，记录 `FAULT`，回退 S5/F1。

## 9. 阶段 S8：抓放闭环

只在 S7 可重复 PASS 后执行：

```text
home_ready -> pick_hover -> pick -> close_gripper
-> pick_hover -> drop_hover -> drop -> open_gripper -> home_ready
```

先空载路径，再单个已验证正方体，最后才接 matcher。`SKIP` 必须零动作并输出原因；`GRAB` 在同一轮只能锁存一次。完整 `round_controller` 和轻量 transaction 已在 Host 层存在，但 `main.c`、OSD 理由码、round-controller `arm_done/event ACK` 与正式 UART 尚未闭环；这里的 ACK 是板内逐轮事务，不是 0x22/0x66/0x67 的协议返回。完成这些板级连接前不得接自动识别触发。

0x66/0x67 无返回值。闭爪/开爪后必须 single-flight 轮询 0x69 至停止，再用 0x65 检查稳定位置窗口；位置读回不能证明夹持力或物体未滑落，仍需低速带载抬升观察和失败测试。

## 10. 每次实机记录模板

每次创建 `final_project/docs/debug_sessions/mycobot_board_run_YYYYMMDD_NN.md`，至少写：

- bitstream/ELF/commit/Efinity 版本、生成 `soc.h` 路径；
- UART2 基址/分频/IRQ/引脚/电平证据；
- Basic `transponder`、Atom `atomMain` 的来源、版本/哈希与核验时间；
- 现场固定、净空、断电/急停、安全员；
- TX/RX 原始字节、expected command/length、最大延迟、超时/迟到/重复/未知帧、overflow/noise/bad-frame；
- 是否发送动作帧；若发送，点位版本、段、速度、空载/工件、录像名；
- PASS/FAIL/WARN、回退版本和下一门是否放行。

## 11. 最短执行清单

```text
[ ] S0 QEMU Host PASS
[ ] S1 soc.h/APB/UART2/PLIC/引脚/电平/急停真源齐全
[ ] S2 Hello + MAGIC + heartbeat + state/error commit，连续复位3次 PASS
[ ] S3 mycobot_uart 轮询适配完成，所有等待有超时
[ ] S4 无臂回环 100/100 + 故障注入 PASS
[ ] 结构加固与外部基准复核 PASS
[ ] S5 GET_ANGLES 30/30，只读且零动作
[ ] S6 PLIC/Ring Buffer 100帧、0 overflow、dry-run零动作
[ ] 180°新点位独立迁移并通过 plan_validate
[ ] S7 HOME->home_ready 低速空载单段 PASS
[ ] S8 空载全路径 -> 单物体 -> matcher 单轮锁存
```

截至 2026-07-16 12:00 仍未通过 S5/S6 安全链时，停止 F2 投入并冻结 F1；不得为了赶时间跳过结构、电平、只读或 dry-run 门。
