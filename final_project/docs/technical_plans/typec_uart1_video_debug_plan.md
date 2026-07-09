# Type-C UART1 视频调试控制台技术方案

> 日期：2026-07-09
> 状态：**Codex 审查修订版，方向通过，实施需按本文阶段门推进**
> 修订来源：Codex 基于当前源码、Efinity 工程 XML、CPU 工程、最新 HDMI 调试记录及官方开发板资料独立复核。
> 范围：本文件是实施方案，不表示 SoC、UART、APB 调试寄存器或 CPU CLI 已经集成完成。

## 1. 审查结论

方案目标正确，但原版把当前工程成熟度高估了一级。

- **战略匹配：高。** Type-C 调试控制台符合“FPGA 提供视频状态，板上 CPU 负责解析与人机交互”的决赛主线。
- **当前可实施度：低至中。** 当前正式 Efinity 工程尚未形成可供 CPU 使用的 SapphireSoC、终端 UART 和 APB 用户寄存器窗口，不能直接从“编写 CLI”开始。
- **与即时调试进度：部分匹配。** 当前最近的上板工作仍是 HDMI `i_video_ready` 上游条件与 LED 探针定位；在该 checkpoint 未收口前，把 SoC/APB/UART 一次性加入当前视频 bitstream 会扩大变量面。
- **审批结论：有条件通过。** 按本文修订后的 Phase 0～5 小步实施；不批准原版“Phase 1 不修改 FPGA/Efinity，仅写 CPU UART1”的执行范围。

一句话概括：**方向对、时机稍早、基础设施判断错了一级。**

## 2. 目标与边界

目标链路：

```text
PC 串口终端
  <-> 开发板 Type-C 物理 UART1，计划 115200 8N1
  <-> Efinity 中实际配置并引出的某个 SapphireSoC UARTx
  <-> 板上 QCRV32/SapphireSoC 裸机固件
  <-> CPU 可见的 APB 调试寄存器块
  <-> 经 CDC 处理的视频链路状态
```

边界：

- Type-C UART 仅用于开发期日志、命令和可观测性，不进入正式识别/控制闭环。
- myCobot 280 控制不属于本方案；未来独立 UART2 路径使用 `1000000` 波特率。
- UART1 命令不得触发机械臂动作、复位 MIPI/DDR/HDMI、修改 PLL 或改变视频时序。
- FPGA 只提供紧凑寄存器和必要 CDC；ASCII 解析、格式化、限流和命令调度由 CPU 完成。
- 所有 UART、APB、时钟和地址宏必须来自本次生成的 `.peri.xml`、`soc.h` 与 linker 文件，禁止使用示例工程或当前占位 `bsp.h` 的地址猜测。

## 3. 当前工程事实基线

截至 2026-07-09：

1. 正式工程 `final_project/fpga/efinity/mem_test.xml` 当前仅包含视频 RTL，最近记录为综合通过。
2. `final_project/fpga/efinity/mem_test.peri.xml` 中 `soc_info` 为空，不能据此声称 SapphireSoC 已经配置。
3. 正式工程目录中没有本设计生成的 `soc.h`；CPU 工程 `bsp.h` 中仍有占位地址。
4. CPU `main.c` 目前只有启动文本/循环打印骨架，没有 RX、行缓冲、命令解析或视频寄存器访问。
5. CPU `board_io.h` 采用较新的 v2 草案语义：`0x000=REG_MAGIC`、`0x004=SYS_CTRL`；旧 `register_map.md` 的 `0x004=REG_VERSION` 与之冲突。
6. 当前 RTL 尚未实现 CPU 可访问的 APB 视频调试寄存器块，因此 Phase 2 不能只靠 CPU 软件完成。
7. 当前 HDMI 调试的即时 checkpoint 是核对最新 LED 探针与 `i_video_ready` 上游条件，必要时仅做一次构建的受控 bypass 实验。

以上事实意味着：**目前不是“UART 引脚没配好”，而是 SoC、UART 外设、地址导出和 APB 用户从设备都尚待建立。**

## 4. 物理接口与术语

官方开发板资料确认 Type-C 物理 UART1 管脚：

| 板级信号 | FPGA 管脚资源 | 封装脚 |
|---|---|---|
| `FPGA_UART1_RXD` | `GPIOR_96_CLK13` | `B12` |
| `FPGA_UART1_TXD` | `GPIOR_100` | `D12` |

预留 UART2：

| 板级信号 | FPGA 管脚资源 | 封装脚 |
|---|---|---|
| `FPGA_UART2_RXD` | `GPIOR_97` | `C14` |
| `FPGA_UART2_TXD` | `GPIOR_101_PLLIN1` | `F12` |

注意：

- “板上 Type-C UART1”是连接器/板级命名，不等于 SoC 内部一定要使用 `SYSTEM_UART_1`。
- 最终应写成“将一个已启用的 SoC UARTx 路由到 Type-C UART1 的 B12/D12”，其中 `x` 由本工程生成的 `.peri.xml` 和 `soc.h` 确认。
- 本方案第一轮只启用 Type-C 调试口，不提前启用或修改 UART2。
- 官方资料已确认引脚，但 USB-UART/JTAG 桥芯片具体型号仍应以原理图为准，文档不再假定为 FT2232HL。

## 5. 推荐架构

采用 CPU-first 控制台，但先补齐 SoC 基础设施：

```text
视频时钟域
  ├─ live level -------- 2FF/专用 CDC --------┐
  ├─ rare event -- sticky(set until clear) ---┤
  └─ activity counter -- Gray/snapshot -------┤
                                               v
APB 时钟域 -> versioned debug register block -> CPU polling
                                               |
                                               v
                                        bounded UART console
```

原则：

- 先证明 UART 单向 TX，再证明 RX/命令闭环。
- 先实现一个只读 `REG_MAGIC`，再扩展视频状态。
- 先建立版本化寄存器契约，再在 CPU 中写解码表。
- 日志发送必须有预算，不允许阻塞式 `putchar` 长时间占用未来的识别与控制主循环。
- UART 调试功能关闭或断线时，不得影响视频链路。

## 6. 分阶段实施与验收门

### Phase 0：收口当前 HDMI 探针 checkpoint

目的：避免 SoC 集成掩盖当前视频故障。

工作：

1. 根据最新 `top.v` 和 `hdmi_stripe_debug_20260707.md` 核对 LED24～LED33 的实际语义。
2. 记录本轮上板灯态和 HDMI 现象。
3. 如 LED30 仍未亮，仅按当前调试记录进行一次构建的 `i_video_ready` hard bypass 对照，随后恢复。

验收：

- 当前 HDMI 现象、bitstream 版本和 LED 映射均有记录。
- 明确下一轮是继续视频定位还是转入 SoC 控制台集成。

### Phase 1：最小 SapphireSoC 与 Type-C UART 集成

目的：建立真实可生成的 CPU 子系统，不接视频寄存器。

工作：

1. 在 Efinity Interface Designer 中加入/确认 SapphireSoC/QCRV32 实例。
2. 只启用一个终端 UARTx，并路由到 Type-C B12/D12。
3. 明确 CPU 时钟、复位、片上存储/启动方式及调试接口。
4. 运行综合、布局布线，记录 setup/hold slack 和所有 CDC/时钟 warning。
5. 导出本次设计对应的 `soc.h`、linker 和 OpenOCD/debug 配置。
6. 更新 `final_project/integration/io_pin_map.md`。

验收：

- `.peri.xml` 的 `soc_info` 非空并包含实际 SoC/UART 配置。
- 生成地址与引脚可从工程产物追溯。
- 视频输出相较 Phase 0 无新增回归。
- UART2 未被启用或改动。

### Phase 2：UART 最小存活证明

目的：先验证 PC -> Type-C -> CPU，不访问 FPGA 视频状态。

实施顺序：

1. 固件仅输出一次 boot banner、版本和实际 UARTx 名称。
2. 验证稳定 TX 后，再增加有界 RX。
3. 支持 `ping`、`ver`、`echo`、`status cpu`。
4. 行缓冲限制为 64 或 128 字节；溢出后丢弃至换行并计数。
5. 初版可轮询，但每次主循环限制 RX/TX 工作量；周期日志默认关闭。

验收：

- `ping -> pong`，`echo abc -> abc`。
- 错误输入和超长输入不越界、不锁死。
- UART 空闲、持续输入和周期输出均不改变 HDMI 表现。

### Phase 3：最小 APB 用户寄存器

目的：独立证明 CPU 能读取 FPGA 自定义逻辑。

工作：

1. 选择并记录空闲 APB 用户窗口。
2. 实现最小只读寄存器：

| Offset | 名称 | 语义 |
|---:|---|---|
| `0x000` | `REG_MAGIC` | 固定魔数与接口主版本 |
| `0x004` | `REG_CAPS` | 当前实现能力位，不复用为旧版 `REG_VERSION` |

3. CPU 增加 `read <offset>` 与 `dump id`。
4. 对非法/未对齐地址返回错误，不允许任意 MMIO 越界访问。

验收：

- CPU 连续读取固定魔数稳定一致。
- 地址定义同时落入 RTL、CPU 头文件和寄存器文档。
- 不把尚未实现的整套 v2 视觉寄存器草案描述为已存在。

### Phase 4：版本化视频调试寄存器

目的：逐组增加真实、可解释的视频状态。

建议首批只覆盖当前调试真正需要的信号：

| 类别 | 示例 | 推荐表示 |
|---|---|---|
| HDMI 上游 | frame ready、frame ok、bridge active、level ready、HDMI ready | live level |
| 错误事件 | FIFO underflow、bridge underflow | sticky + counter |
| 活动证明 | frame/line/change event | counter 或 toggle |
| DDR/CSI | config done、frame seen | level + sticky |

每个字段在实现前必须记录：

- 来源模块和原始时钟域；
- level、pulse、sticky 或 counter 类型；
- 跨域方法；
- 复位域及复位值；
- clear 方法；
- 双通道 0/1 是否对称。

CDC 规则：

- 稳定单比特 level：两级同步器。
- 窄脉冲：不得直接两级同步；使用 toggle、脉冲握手或源域 sticky。
- sticky 位：在源域置位；CPU 清除必须通过 toggle/握手返回源域，不能异步直接清。
- 多位计数器：使用 Gray 编码、冻结快照或请求/应答握手，不能逐位独立两级同步。

验收：

- UART 原始值、解码值与同一 bitstream 的 LED/ILA/上板现象一致。
- 跨域实现经代码审查和 CDC warning 审查。
- `watch video` 有最小周期和输出预算，不阻塞 CPU 主循环。

### Phase 5：受控维护命令

稳定后才增加：

- `clear err`：W1C/握手清 sticky 错误。
- `log on|off`、`set loglevel <n>`。
- `watch video <ms>`，设置安全最小间隔和可中止机制。

本阶段仍禁止：

- 通过 UART 改 MIPI/DDR/HDMI 时钟或复位；
- 通过 UART2 发送 myCobot 包；
- 将 PC 串口在线作为正常视频/识别流程的必要条件。

## 7. CPU 软件结构建议

可能新增：

- `uart_console.h/.c`：有界 RX、非阻塞 TX、解析和统计。
- `video_debug_cli.h/.c`：寄存器白名单、读取与格式化。
- `video_debug_regs.h`：由已批准的接口契约维护偏移和位定义。

修改：

- `main.c`：以合作式 tick 调用控制台，不使用无限阻塞打印。
- `bsp.h`：只引用或封装生成的 `soc.h`，不得固化未经确认的 UART1 地址。
- `Makefile`：区分 host/standalone 语法检查与真实 RISC-V 固件构建。

说明：

- `STANDALONE_TEST=1` 只能用于占位环境下的主机侧/语法级检查，不证明固件可在板上运行。
- 当前 Windows PowerShell 环境未提供可直接调用的 `make`；实际命令应在 Efinity shell、已配置的 MSYS/WSL 或团队规定的 RISC-V 构建环境中执行并记录。

## 8. FPGA 文件影响与审查门

预计涉及：

- `final_project/fpga/efinity/mem_test.xml`
- `final_project/fpga/efinity/mem_test.peri.xml`
- `final_project/fpga/efinity/constrain.sdc`
- `final_project/fpga/rtl/top/top.v`
- 新增 APB 调试寄存器 RTL

这些修改跨越 SoC、引脚、总线、CDC 和视频顶层，必须提交 Review Packet，至少包含：

- 修改文件与关键 diff；
- SoC UARTx 到 B12/D12 的连接；
- APB 地址窗口；
- 每个调试信号的时钟域/复位/CDC；
- 双通道对应关系；
- Efinity setup/hold slack、关键 warning；
- 上板 bitstream/固件版本和无视频回归证据。

## 9. 安全与上板操作

- 不带电插拔 MIPI 摄像头排线；需要改变连接时先按硬件流程断电。
- 不通过提高 HDMI 刷新率“制造 underflow”；应使用可恢复、单变量的调试激励。
- Type-C 控制台不得控制或隐式触发 myCobot。
- CPU heartbeat 目前不是已实现的视频复位机制，禁止在说明中将其写成现有事实。
- 任何未来 heartbeat 联动都必须定义失效安全行为，且不得向 UART2 发动作包。

## 10. 当前推荐的下一步

1. 先完成 Phase 0 的 HDMI LED/现象记录。
2. 单独制作 Phase 1 的 SoC 最小集成 Review Packet。
3. 只引出 Type-C 调试 UART，不碰 UART2。
4. 首个固件只做 boot banner；稳定后再加 RX。
5. 首个 APB 从设备只做 `REG_MAGIC`；验证后再添加视频状态。

## 11. 参考事实源

- `AGENTS.md`「分赛区决赛主线」与「Codex 审查门」
- `CURRENT_STATE.md`
- `final_project/fpga/efinity/mem_test.xml`
- `final_project/fpga/efinity/mem_test.peri.xml`
- `final_project/fpga/rtl/top/top.v`
- `final_project/cpu/app/include/bsp.h`
- `final_project/cpu/app/include/board_io.h`
- `final_project/cpu/app/src/main.c`
- `final_project/docs/debug_sessions/hdmi_stripe_debug_20260707.md`
- `final_project/docs/architecture/vision_register_handbook_draft_2026-07-03.md`
- 官方《开发板使用说明》与《TJ375N529 开发板 IO 定义列表》

---

**Codex 审查标记：** 本版由 Codex 于 2026-07-09 根据仓库当前事实修订；后续若 `.peri.xml`、生成 `soc.h`、APB 地址或 LED 映射变化，应重新审查本方案。
