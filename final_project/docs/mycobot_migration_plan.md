# myCobot 280 机械臂控制离线迁移方案

> 从 PC 端 Python / `pymycobot` 迁移到板上 SoC C 固件  
> 补充版：2026-07-07，Codex 审查与 Gemini 查表核查后修订  
> 当前定性：方向可行，但必须先完成真源确认；本文是实施蓝图，不是已验证完成清单。

## 0. 结论与边界

推荐路线保持不变：正式闭环采用 `FPGA 视频前端 / ROI / 统计特征 / OSD + 板上 CPU 识别决策 / 参数管理 / myCobot 串口控制`。

必须遵守以下硬边界：

- PC、`pymycobot`、myBlockly 只用于开发期调试、标定和日志观察，不进入正式识别 / 控制闭环。
- myCobot 协议封包、点位表、动作序列、互锁、超时和异常处理放在板上 CPU C 代码中。
- FPGA RTL 只提供 UART / FIFO / 寄存器等硬件通道，不实现复杂机械臂动作状态机。
- 未确认电平、接线、管脚约束、UART IP、`soc.h` 地址和急停/断电方式前，不允许把 FPGA 输出直接接入机械臂控制线。
- 涉及实机动作、夹爪、快速移动、FPGA-to-机械臂接线、`constrain.sdc`、`mem_test.xml`、`.peri.xml` 或 PLIC/CDC/AXI 的修改，必须进入 Codex Gate。

本文引用的主线边界来自 `AGENTS.md`「分赛区决赛主线」和 `分赛区决赛实施开发路线.md`；若后续 `CURRENT_STATE.md` 给出更高优先级增量，以新状态为准，但不得覆盖机械臂安全红线。

## 1. 当前工程事实

当前仓库中已有 CPU 软件骨架，但机械臂离线控制尚未真正落地：

- `final_project/cpu/app/src/mycobot_protocol.c`：仅有最小帧头拼装骨架，还没有完整命令 payload、返回帧解析、校验和超时策略。
- `final_project/cpu/app/src/arm_controller.c`：仅有 `arm_id/state` 初始化骨架，还没有动作状态机、点位表和安全门。
- `final_project/cpu/app/include/board_io.h`：已定义 FPGA/CPU APB3 视觉寄存器接口、feature snapshot、result writeback、heartbeat 等 API。
- `final_project/cpu/app/src/board_io.c`：当前仍是 stub，尚未实现 `board_io.h` 声明的 APB3 API。
- `final_project/cpu/app/include/bsp.h`：地址、CLINT/PLIC、UART、DDR 等仍是占位值，文件自身要求最终以 Efinity 生成的 `soc.h` 为真源。
- `final_project/integration/io_pin_map.md`：目前只有说明文字，尚未记录 F12/C14/U19/V19 的正式证据表。
- `final_project/integration/fpga_cpu_interface.md`：已明确第一版先验证 `REG_MAGIC -> CPU read -> CPU writeback -> OSD visible`，后续再接 PLIC 中断。

因此，后续执行顺序必须先补真源，再写动作逻辑。

## 2. 实施前真源确认清单

以下项目必须在进入实机动作前完成，并把证据路径写回本方案或 `CURRENT_STATE.md` / 调试日志。

| 编号 | 项目 | 必须确认的真源 | 验收口径 |
|---|---|---|---|
| T0-1 | SoC 地址 | 最终 Efinity 生成的 `soc.h`、linker、OpenOCD/debug profile | UART0、UART2、GPIO、CLINT、PLIC、APB user window 地址不再使用占位值 |
| T0-2 | UART 通道 | Efinity SoC/IP 配置、`mem_test.xml`、`.peri.xml`、顶层端口 | 调试 UART 与 myCobot UART 分开；UART0 默认调试 115200，myCobot UART 固定 1000000 |
| T0-3 | 管脚与电平 | 开发板原理图/管脚表、`constrain.sdc`、`mem_test.peri.xml`、实物线序 | F12/C14/U19/V19 是否可用、Bank 电压、方向、端口名和约束均可追溯 |
| T0-4 | 共地与接线 | 实物接线照片/记录、万用表或板卡 GND 标识 | FPGA GND 与机械臂底座 GND 直连共地；TX/RX 方向确认 |
| T0-5 | APB 视觉窗口 | `board_io.h/.c`、FPGA register map、OSD 显示 | CPU 可读 `REG_MAGIC/REG_VERSION`，可写 heartbeat/result 并在 OSD 或日志可见 |
| T0-6 | 机械臂协议 | `mycobot_pc_tests/` 已验证脚本、串口抓包、`mycobot_protocol_notes.md` | 命令码、长度、校验、返回帧、异常返回不靠猜测 |
| T0-7 | 安全点位 | PC 端已验证点位与日志、`home_ready_points.md`（若使用） | 工作点、home_ready、抓取/放置路径各自安全门分开记录 |

未完成 T0 清单时，只允许做文档、只读代码、离线编译和无机械臂接入的串口回环测试。

### 2.1 Gemini 依据官方手册查询确认后的部分结果

根据官方手册（《TJ375N529开发板IO定义列表》、《TJ375N529_PINOUT_CONFIGURATION》等）的交叉核查，目前已部分确认以下硬件信息：

- **系统核心时钟 (SYSTEM_CLINT_HZ)**：官方手册默认约束通常为 100 MHz。**[⚠️ 待实地确认]**：仍需进入 Efinity Interface Designer -> PLL 确认实际馈入 RISC-V 核心的时钟分频输出，以防团队修改为 50 MHz 或其他频率。
- **UART2 基地址 (SYSTEM_UART_2_IO_CTRL)**：挂载在系统 I/O 总线上，基地址理论上偏移为 `0xF8012000`（中断 ID 为 3）。**[⚠️ 待实地确认]**：Sapphire SoC 默认配置通常仅开启 UART0/1，必须实地确认 Efinity IP Manager -> Sapphire SoC -> UART 选项卡中 `UART 2` 已被勾选开启。
- **UART2 控制模式**：属于 RISC-V 软核自带的 UART IP 直接通过 MMIO 控制（即使用 `#include "driver/uart.h"` 等标准 API），非 APB 封装。
- **视觉 APB 基地址**：确认默认物理映射地址为 `0xF810_0000`，与 C 代码中预留的 `0xF8100000u` 一致。
- **引脚电平安全**：C14 (Bank TR1) 与 F12 (Bank TR2) 均为 3.3V 电压域，完全契合 myCobot 3.3V TTL 电平。在约束中应配置为 `3.3 V LVCMOS`。
- **物理按键纠错 (U18 冲突避免)**：
  - **U19** 为 `Push_Button_1`（按键1），按键按下为低电平（0），带 Schmitt Trigger。
  - **U18 在引脚表中明确为 GND（地）引脚，切勿配置为按键！**
  - **V19** 才是真实的 `Push_Button_2`（按键2），亦为按下触发低电平。
  - **[⚠️ 待实地确认]**：需在 Interface Designer 中将 U19 和 V19 的 `Pull Option` 设置为 `weak pull-up`（内部上拉），防止因引脚悬空导致误触发急停或录入。

---

## 3. 物理接口规划

### 3.1 建议引脚表

下表是当前建议，不代表已完成约束或已上板验证。正式表必须同步写入 `final_project/integration/io_pin_map.md`。

| 信号 | 建议管脚 | 方向 | 电平/用途 | 当前状态 |
|---|---:|---|---|---|
| `FPGA_UART2_TXD` | F12 | FPGA -> myCobot RX | 3.3V TTL 串口发送 | 待原理图、约束和上板确认 |
| `FPGA_UART2_RXD` | C14 | myCobot TX -> FPGA | 3.3V TTL 串口接收 | 待原理图、约束和上板确认 |
| `GND` | 板卡 GND | 双向参考 | 与机械臂底座 GND 直连共地 | 实机接线前必须确认 |
| `BTN_TEACH_RECORD` | U19 / PB1 | FPGA/CPU 输入 | 示教记录键 | 待确认消抖、GPIO 映射和中断/轮询方式 |
| `BTN_TORQUE_RELEASE` | V19 / PB2 | FPGA/CPU 输入 | 急停/释放扭矩触发 | 需注意防止引脚悬空，避免 U18 GND 混淆 |

### 3.2 电气和接线底线

- myCobot UART 波特率固定 `1000000`，数据格式为 8N1。
- UART divider 必须按最终 UART IP 文档 and `soc.h` 时钟参数计算。当前 `bsp.h` 中 `CLINT_HZ / (baud * dataLen) - 1` 是现有骨架注释，不可与简化公式混用。
- 若 UART2 由 CPU 直接 MMIO 控制，则在 C 端实现 UART2 driver；若 UART2 由 FPGA RTL/FIFO 包装，则 CPU 通过 `arm_uart_fifo` 或 APB/FIFO 窗口写入，RTL 只做字节搬运。
- 机械臂底座 TX/RX 与 FPGA TX/RX 的方向必须用串口工具或逻辑分析仪确认，不凭线色判断。
- V19 只能作为“释放扭矩/软件急停触发”或“进入保护态”输入；它不能替代机械臂电源急停或人工断电手段。

---

## 4. C 语言模块分层

建议保持 `final_project/cpu/app/` 为正式固件主目录，按职责拆分，不把 UART 中断塞进视觉 APB 层。

| 模块 | 建议文件 | 职责 | 第一版策略 |
|---|---|---|---|
| BSP / MMIO | `bsp.h`、后续 `bsp_vendor/` | 最终 `soc.h` 地址、UART/GPIO/CLINT/PLIC 原语 | 先替换占位地址，保留调试 UART 打印 |
| 视觉 APB | `board_io.h/.c` | `REG_MAGIC`、feature snapshot、result writeback、heartbeat、ARM_STATE 回写 | 先实现轮询 APB，不承担 myCobot UART ISR |
| myCobot UART | `mycobot_uart.h/.c` | UART2 init、polling read/write、RX ring buffer、后续 PLIC ISR | 先轮询，链路稳定后再开 RX interrupt |
| 协议封装 | `mycobot_protocol.h/.c` | 帧拼装、命令码、payload、checksum/长度、返回帧解析 | 先实现只读/释放/关节角命令最小集 |
| 按键输入 | `gpio_keys.h/.c` | U19/V19 消抖、边沿检测、事件上报 | 第一版轮询；确认 IRQ 后再进 PLIC |
| 安全门 | `safety_guard.h/.c` | 坐标半径、关节差、home_ready、速度/超时、故障降级 | 从 PC 端已验证常量迁移，但保留注释来源 |
| 状态机 | `arm_controller.h/.c` | connect/idle/teach/replay/release/error 非阻塞状态机 | 不用死延时；每轮 tick 可回写 `CPU_ARM_STATE` |
| 点位/参数 | `params/arm_points.*` 或 `param_table.c` | HOME、home_ready、pick/drop hover/down、速度、超时 | 点位来自 PC 标定后固化，加载后仍跑安全门 |

命名约束：

- `board_io` 只表示 FPGA/CPU APB3 寄存器读写。
- `mycobot_uart` 只表示机械臂 UART 物理收发。
- `mycobot_protocol` 只表示协议帧和解析，不直接做状态机决策。
- `arm_controller` 只做业务状态机和安全互锁，不直接硬编码 MMIO 地址。

---

## 5. 协议迁移范围

第一版不追求完整复刻 `pymycobot`，只迁移比赛闭环必需命令。

### 5.1 最小命令集

| 优先级 | 功能 | 用途 | 动作风险 |
|---|---|---|---|
| P0 | `GET_ANGLES` / 读取关节角 | 只读链路验证、失败后诊断 | 低 |
| P0 | `RELEASE_ALL_SERVOS` / 释放全部舵机 | V19 触发、异常保护、人工扶正 | 中，需扶稳防下坠 |
| P1 | `POWER_ON` 或等效上电/抱紧 | 示教后恢复控制 | 中 |
| P1 | `SEND_ANGLES` / 发送关节角 | 主运动路径 | 高，必须过安全门 |
| P1 | 夹爪开合 | 抓取/释放物块 | 中 |
| P2 | 状态/到位查询 | 软到位、超时诊断 | 低 |

实际命令字节、长度和校验规则必须来自串口抓包、`pymycobot` 源码或已验证 PC 端日志，并同步写入 `final_project/integration/mycobot_protocol_notes.md`。`CMD_RELEASE_ALL` 等名字在本文中只是语义占位，不代表最终命令码。

### 5.2 不迁移为主线的能力

- 不把 PC 端 `sync_send_coords()` 等坐标驱动作为 C 端主运动路线。PC 端测试已把坐标驱动隔离为 IK 风险，正式自动动作优先用关节角回放。
- 坐标值可以用于只读验证、日志诊断和工作区半径检查，但不作为第一版自动运动命令输入。
- 不迁移 PC 端交互式提示文本和临时调试分支到正式主循环；调试信息通过 UART0 日志、OSD 状态码和错误码表达。

---

## 6. 安全门迁移规则

安全门要按“工作点”“短距离动作”“回零/安全中间点”分开，不混用。

### 6.1 工作区点位

适用于 pick/drop 及其 hover/down 点：

- 目标坐标必须为有限数值，拒绝 NaN/Inf。
- 工作半径建议硬限 `R <= 280 mm`，超出丢弃并要求重新示教。
- 低 Z、高 Z、离底座过近等业务边界按 PC 端最新已验证脚本迁移，但必须注明来源。
- 点位加载自预设文件时仍必须重跑安全门，不能因“已保存”而跳过。

### 6.2 短距离 hover/down 点对

适用于 pick_hover <-> pick、drop_hover <-> drop：

- 1-5 轴单轴最大角差建议 `<= 30 deg`。
- 第 6 轴腕部最大角差建议 `<= 45 deg`。
- 短距离动作保持低速，失败后读取实际角度/坐标做诊断，再决定重试或进入保护态。

### 6.3 home_ready 与回零

`home_ready` 是单独的安全中间姿态，不是普通抓放工作点：

- 不套用工作区 `Z_MAX=280` 等抓放边界；直立附近 Z 可能显著高于抓放工作区。
- 自动回零前只看 1-5 轴大臂相对 HOME 的 `arm_max_diff`，第 6 轴偏差单独记录，不阻断大臂安全回零。
- `home_ready` 示教目标推荐 `arm_max_diff <= 40 deg`；`40-45 deg` 只能作为强警告临界区；`>45 deg` 禁止自动回零。
- 自动回零失败时，不在半空直接释放；应提示扶稳，再下发释放扭矩或进入人工扶正流程。

### 6.4 到位与失败判定

PC 端经验说明，`sync_send_angles()` 返回失败不一定等于机械臂没有移动。C 端迁移时应保留“失败后读实际角度/坐标”的诊断路径：

```text
send target joint angles
wait or poll until timeout
if strict arrival fails:
    read actual angles
    read actual coords if available
    if residual is inside soft tolerance:
        mark soft-arrived and continue
    else:
        enter retry / release / manual-safe branch
```

第一版建议保留以下状态码，方便 OSD 和 UART 日志定位：

| 状态码 | 含义 |
|---|---|
| `ARM_IDLE` | 已连接，空闲 |
| `ARM_TEACH` | 拖拽示教/记录点位 |
| `ARM_READY` | 已有可执行点位，等待识别结果 |
| `ARM_MOVING` | 正在执行关节角动作 |
| `ARM_GRIPPER` | 正在执行夹爪动作 |
| `ARM_SOFT_ARRIVED` | 严格到位失败但实际误差可接受 |
| `ARM_RELEASED` | 已释放扭矩，等待人工处理 |
| `ARM_ERROR` | 协议、超时、安全门或通信错误 |

---

## 7. 阶段化迁移路线

### 阶段 0：真源冻结

目标：把 T0 清单变成可追溯事实。

任务：

- 从最终 Efinity 工程导出或复制 `soc.h`、linker、OpenOCD/debug profile。
- 在 `io_pin_map.md` 补 F12/C14/U19/V19 的证据表。
- 明确 UART2 是 CPU 直接控制还是 FPGA UART/FIFO 桥接。
- 明确 V19/U19 是 CPU GPIO、FPGA GPIO、还是 APB/寄存器事件。
- 更新 `mycobot_protocol_notes.md`，记录最小命令集的命令字、长度、校验和返回帧。

验收：

- `bsp.h` 不再作为地址真源；正式构建必须包含最终 `soc.h`。
- `io_pin_map.md`、`constrain.sdc`、`.peri.xml` 与顶层端口一致。
- 任何人都能从文档追到真实工程文件。

### 阶段 1：CPU 与 FPGA APB 最小闭环

目标：先证明 CPU 能可靠读写 FPGA 侧寄存器，不接机械臂。

任务：

- 实现 `board_io.c` 中 `board_io_validate()`、heartbeat、result writeback 的最小路径。
- CPU 启动后通过 UART0 打印 `soc.h` 地址摘要。
- CPU 读 `REG_MAGIC/REG_VERSION`，写 `CPU_HEARTBEAT`、`CPU_ARM_STATE` 和 `CPU_ERROR_CODE`。
- FPGA/OSD 或 UART 日志能看到 CPU 写回结果。

验收：

- `REG_MAGIC -> CPU read -> CPU writeback -> OSD/log visible` 成功。
- 失败时能区分地址错误、APB 未接通、OSD 未显示和 CPU 未运行。

### 阶段 2：UART2 无动作链路验证

目标：证明 UART2 物理收发和 1 Mbps 分频正确。

推荐顺序：

1. UART2 TX/RX 本地回环或串口工具监听，确认 1 Mbps 8N1。
2. 不接机械臂动作线，只用串口工具或逻辑分析仪看帧格式。
3. 接 myCobot 后只发送 `GET_ANGLES` 等只读命令。
4. 返回帧通过 UART0 打印，同时写 `ARM_STATE=READONLY_OK`。

验收：

- 能连续读取关节角，且返回帧长度、命令码、校验均可解析。
- 不发送夹爪、上电、移动或释放扭矩命令。
- 若读取失败，日志能显示 TX 字节、RX 字节、超时点和错误码。

### 阶段 2.5：UART RX 中断与 Ring Buffer

目标：在轮询链路稳定后，再引入 PLIC 中断。

任务：

- 在 `mycobot_uart.c` 内实现 RX ring buffer。
- 确认 UART2 RX Not Empty 的 IRQ 号、PLIC enable、priority、claim/complete 流程。
- ISR 只搬字节，不做协议解析、不执行动作、不下发释放命令。
- 主循环从 ring buffer 中解析完整帧。

验收：

- 轮询版和中断版解析结果一致。
- ISR 不阻塞、不调用复杂状态机、不写长日志。
- V19 急停事件优先级高于普通 RX 解析。

### 阶段 3：协议封装和只读/保护命令

目标：让 C 端掌握最小协议能力。

任务：

- `mycobot_protocol.c` 实现 frame build、checksum、parser、timeout error。
- 完成 `GET_ANGLES`、状态查询和 `RELEASE_ALL_SERVOS`。
- V19 触发后进入 `ARM_RELEASED`，通过 UART0/OSD 提示“扶稳后处理”。
- 所有命令均有返回值 and 错误码，不允许静默失败。

验收：

- 只读读取稳定。
- 释放扭矩命令只能在用户确认已扶稳、或 V19 明确触发保护态时发送。
- 错误码能回写 `CPU_ERROR_CODE`。

### 阶段 4：关节角主运动路径

目标：迁移 PC 端已验证的关节角回放路线。

任务：

- 固化 HOME、home_ready、pick_hover、pick、drop_hover、drop 点位。
- 所有点位加载后重跑安全门。
- 实现非阻塞动作状态机：下发命令、等待/轮询、读取实际姿态、软到位判断、失败降级。
- 夹爪开合独立成状态，不与大臂移动混在一个 blocking 函数里。

验收：

- 空载、低速、单步执行各段动作。
- 每段动作前后都打印目标角、实际角、最大误差、状态码。
- 不使用坐标驱动作为自动运动主线。

### 阶段 5：与识别决策联动

目标：把机械臂动作和 CPU 识别/任务匹配接起来。

触发条件：

- CPU 已能从 FPGA 读取稳定 feature snapshot。
- `task_matcher` 已能输出 `match/grab/skip`。
- OSD 或 UART 能显示识别结果和动作状态。

任务：

- `match_action.grab == 1` 时进入抓取状态机。
- `skip` 时只显示结果，不动作。
- 防重复触发：同一 `frame_id` / 同一物块只触发一次动作。
- 动作期间锁住新的抓取请求，直到回到 `ARM_IDLE`。

验收：

- 符合条件物体触发一次抓取。
- 不符合条件物体不动作。
- 识别错误、通信错误、V19、超时都能进入安全降级。

### 阶段 6：离线全业务闭环

目标：脱离 PC 控制完成比赛形态演示。

任务：

- 烧录正式 bitstream 与 ELF。
- PC 只作为 HDMI 显示/录制和 UART0 日志观察工具。
- 纯离线完成：识别结果 -> match/skip -> 机械臂动作 -> 回安全位。
- 按三类评分项做 5 次一组测试。

验收：

- 断电重启后可按操作手册恢复。
- 每次动作都有日志：目标条件、识别结果、动作状态、错误码。
- 失败时优先安全停机/释放/人工扶正，不追求继续动作。

---

## 8. 建议给 Claude 的执行工单

后续可以按以下粒度分配给 Claude，每个工单完成后交 Codex 复核。

1. `docs/io-pin-truth`  
   补 `io_pin_map.md` 中 F12/C14/U19/V19 的证据表；只读核查 `constrain.sdc`、`mem_test.xml`、`.peri.xml` 和顶层端口，不直接改工程。

2. `cpu/board-io-minimal`  
   实现 `board_io.c` 的 APB 最小闭环：validate、heartbeat、write global state；不碰 myCobot UART。

3. `cpu/mycobot-uart-polling`  
   新增 `mycobot_uart.h/.c`，先做 UART2 polling TX/RX 和只读 `GET_ANGLES` 测试入口；不接 PLIC。

4. `cpu/mycobot-protocol-minset`  
   在 `mycobot_protocol.h/.c` 中实现最小命令集和 parser，并更新 `mycobot_protocol_notes.md`。

5. `cpu/arm-safety-guard`  
   新增或补充 `safety_guard`，迁移 R_MAX、短距离关节差、home_ready、软到位和失败诊断逻辑。

6. `cpu/arm-controller-state-machine`  
   扩展 `arm_controller` 为非阻塞 tick 状态机；动作前后回写 `CPU_ARM_STATE` 和 `CPU_ERROR_CODE`。

7. `integration/offline-loop`  
   接入 `task_matcher` 的 `match_action`，实现 grab/skip 联动和防重复触发。

每个工单必须附带 Review Packet：修改文件、关键 diff、已运行命令、未验证项、风险假设和下一步最小修复建议。

---

## 9. 暂不做的事项

- 不在本阶段恢复纯 FPGA 机械臂控制主线。
- 不把 PC 或外部 MCU 放入正式控制闭环。
- 不直接把 `pymycobot` 的高层阻塞调用逐行翻译成 C。
- 不先写 PLIC 中断再验证轮询链路。
- 不把坐标驱动作为自动动作主线。
- 不在未确认真实 `soc.h` 和 UART2 通道前烧录实机动作固件。

---

## 10. 当前 GO / NO-GO 判定

| 项目 | 判定 | 说明 |
|---|---|---|
| 架构方向 | GO | 符合 `AGENTS.md`「分赛区决赛主线」：板上 CPU 控制 myCobot，RTL 只给硬件通道 |
| 作为执行蓝图 | GO with conditions | 需先完成 T0 真源确认，并按阶段推进 |
| 直接实机动作 | NO-GO | 管脚、UART2、`soc.h`、协议命令和安全点位尚未完成工程级闭环 |
| 直接改 `board_io.c` 做 UART ISR | NO-GO | `board_io` 已承担 FPGA/CPU APB3 视觉寄存器接口，应新建 `mycobot_uart` |
| PLIC/Ring Buffer | LATER | 轮询只读链路稳定后再接 |

下一步优先级：先做 `docs/io-pin-truth` 和 `cpu/board-io-minimal`，再做 `cpu/mycobot-uart-polling`。这条顺序能最大限度避免把硬件地址、APB 窗口、UART2 和机械臂动作风险混在一起。
