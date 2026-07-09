# 硬件调试指南：Type-C UART 视频状态控制台

> 版本：2026-07-09 Codex 审查修订版
> 适用对象：FPGA、板上 CPU 固件与上板调试人员
> 重要说明：本文讲解目标架构和正确实施顺序，不表示当前工程已经具备 SapphireSoC、Type-C UART 或 APB 视频寄存器。

## 1. 先理解这条链路

最终希望得到：

```text
PC 串口助手（115200 8N1）
        |
开发板 Type-C 物理 UART1
        |
Efinity 实际配置的 SoC UARTx
        |
QCRV32/SapphireSoC 裸机固件
        |
APB 视频调试寄存器
        |
CSI / DDR / framebuffer / HDMI 状态
```

这里有三个容易混淆的名字：

1. **Type-C UART1**：开发板连接器和原理图上的板级名称。
2. **SoC UARTx**：SapphireSoC 内部启用的某个 UART 外设实例。
3. **UART2**：项目预留给 myCobot 280 的独立外设通道。

板上写着 UART1，不代表软件宏一定叫 `SYSTEM_UART_1`。最终 UARTx 编号、基地址和时钟只能以本工程生成的 `.peri.xml` 与 `soc.h` 为准。

## 2. 为什么使用 CPU，而不是在 RTL 里解析文本

FPGA 适合：

- 捕获 frame ready、underflow、HDMI ready 等状态；
- 保存 sticky 错误；
- 提供计数器和只读寄存器；
- 完成必要的跨时钟域处理。

CPU 适合：

- 解析 `ping`、`status`、`dump video`；
- 把寄存器位翻译成人可读文本；
- 限制命令长度和访问范围；
- 控制日志频率；
- 后续与参数管理、识别决策共用软件框架。

这样不会把复杂 ASCII 状态机塞进高速视频 RTL，也符合项目“FPGA 做前端，CPU 做决策与管理”的主线。

## 3. 当前工程到底完成到哪里

截至 2026-07-09，真实状态是：

| 项目 | 当前状态 |
|---|---|
| 视频 RTL | 已在正式 Efinity 工程中，仍处于 HDMI 上游 ready/bridge 探针调试 |
| SapphireSoC | 当前正式 `.peri.xml` 的 `soc_info` 为空，尚不能视为已集成 |
| Type-C UART | 官方管脚已知，但当前工程尚未形成可验证的 SoC UART 路由 |
| 生成 `soc.h` | 正式工程中尚未找到本设计对应版本 |
| CPU 控制台 | 只有简单 TX/循环骨架，没有 RX 与 CLI |
| CPU 可见 APB 调试块 | 尚未实现 |
| 视频寄存器 v2 | 仍是架构草案，不能当成现有硬件 |

因此，原来“CPU 软件已准备好，只缺 UART 引脚”的说法不成立。需要先建立 SoC，再做控制台，最后才是视频寄存器。

## 4. 已确认的开发板管脚

官方开发板资料给出的 Type-C 物理 UART1：

| 信号 | FPGA 资源 | 封装脚 |
|---|---|---|
| `FPGA_UART1_RXD` | `GPIOR_96_CLK13` | `B12` |
| `FPGA_UART1_TXD` | `GPIOR_100` | `D12` |

预留 UART2：

| 信号 | FPGA 资源 | 封装脚 |
|---|---|---|
| `FPGA_UART2_RXD` | `GPIOR_97` | `C14` |
| `FPGA_UART2_TXD` | `GPIOR_101_PLLIN1` | `F12` |

本次只处理 B12/D12。不要为了“以后可能用机械臂”提前启用 UART2，这会增加引脚、时钟、软件和安全变量。

文档也不再假定板载桥接芯片一定是 FT2232HL；具体器件型号应从原理图确认。调试时把它称为“板载 USB-UART/JTAG 桥”即可。

## 5. 当前 LED 探针的正确读法

最近一次 `top.v` 调试构建中，部分 `dbg_*` 端口名是历史名称，**端口名已经不能代表实际语义**。必须按当前 RTL 右值解释：

| LED | 当前实际信号 | 解释 |
|---:|---|---|
| 24 | `selected_frame_ready_cdc[2]` | 选中通道 frame ready 已跨域；不是 fb0 underflow |
| 25 | `selected_fifo_underflow_cdc[1]` | 选中通道 FIFO underflow；不是 CSI format OK |
| 26 | `selected_frame_ok_hdmi` | HDMI 域看到的 frame 条件成立 |
| 27 | `selected_bridge_active` | bridge 正在活动；不是 bridge underflow |
| 28 | `selected_bridge_level_ready` | bridge level 达到 ready 阈值 |
| 29 | `selected_bridge_level_low` | bridge 水位偏低；不是 input stable |
| 30 | `hdmi_video_ready` | 最终 HDMI video ready |
| 31 | `selected_bridge_underflow` | bridge underflow |
| 32 | 输入像素变化探针 | 输入数据存在变化 |
| 33 | `use_input_video` | 当前选择真实输入视频而非 fallback |

调试纪律：

- 每次上板记录 bitstream/commit 与 LED 表，不混用旧映射。
- UART 状态以后必须与**同一版 bitstream** 的 LED 结果比较。
- 如果 LED30 不亮，先按当前 HDMI 调试日志继续定位，不要立刻引入整套 SoC 改造。

## 6. 正确的五阶段上板顺序

### 阶段 A：先收口当前 HDMI checkpoint

1. 使用当前 RTL 的 LED24～LED33 映射记录灯态。
2. 记录 HDMI 黑屏、条纹、fallback 或正常显示现象。
3. 如调试记录要求，可做一次 `i_video_ready` hard bypass 对照构建；实验后恢复。
4. 不在同一构建中顺便加入 SoC、APB 和新 CDC。

验收产物：一条可复现记录，包含 commit、bitstream、LED 表和显示现象。

### 阶段 B：建立最小 SoC 与 Type-C UART

1. 打开正式 `mem_test.xml` 和 Interface Designer。
2. 配置 SapphireSoC/QCRV32 的时钟、复位、启动存储和一个 UARTx。
3. 将该 UARTx 路由至：
   - RX：`GPIOR_96_CLK13` / B12
   - TX：`GPIOR_100` / D12
4. 暂不启用 UART2。
5. 综合并布局布线，记录 setup/hold slack 与 warning。
6. 导出同一设计生成的 `soc.h`、linker 和调试配置。
7. 把实际 UARTx 编号、基地址、时钟和管脚写入 `io_pin_map.md`。

此阶段必须做视频回归检查，因为 `.peri.xml`、时钟/复位和顶层连接变化可能影响原视频设计。

### 阶段 C：先 TX，后 RX

第一版固件只输出：

```text
video-console boot
fw=<版本>
uart=<实际 UARTx> baud=115200
```

TX 稳定后，再加入：

| 命令 | 返回 |
|---|---|
| `ping` | `pong` |
| `ver` | 固件版本 |
| `echo abc` | `abc` |
| `status cpu` | 心跳、RX/TX/溢出计数 |

实现要求：

- 行缓冲最大 64 或 128 字节。
- 同时接受 `\r`、`\n`、`\r\n`。
- 超长行丢弃至换行并增加 overflow 计数。
- 初版可轮询，但单次循环必须限制处理字节数。
- 周期日志默认关闭；发送不得无限 busy-wait。

### 阶段 D：只实现一个最小 APB 从设备

不要一开始就复制整本视觉寄存器手册。先实现：

| Offset | 名称 | 作用 |
|---:|---|---|
| `0x000` | `REG_MAGIC` | 验证 CPU 能稳定读到 FPGA 自定义逻辑 |
| `0x004` | `REG_CAPS` | 表明当前硬件实际支持哪些调试组 |

CPU 只开放白名单读取，例如：

```text
dump id
read 0x000
```

不要允许任意 32 位地址读取，否则输入错误可能访问未映射外设。

寄存器版本冲突说明：

- 当前 `board_io.h` 使用 v2 语义：`0x000=REG_MAGIC`、`0x004=SYS_CTRL`。
- 旧 `register_map.md` 使用 `0x004=REG_VERSION`。
- 新调试块必须先确定一份版本化契约，不能在 CPU 与 RTL 中各自选一版。

### 阶段 E：逐组加入视频状态

首批优先加入当前 HDMI 定位真正需要的状态：

- frame ready / frame ok；
- bridge active / level ready / level low；
- HDMI video ready；
- FIFO underflow / bridge underflow；
- frame、line 或像素活动计数。

每个信号都要先回答六个问题：

1. 来源模块是什么？
2. 原始时钟域是什么？
3. 它是 level、pulse、sticky 还是 counter？
4. 如何跨到 APB 时钟域？
5. 如何复位和清除？
6. 通道 0/1 是否需要对称实现？

## 7. CDC 不能只写一句“两级同步”

不同数据类型需要不同方法：

| 数据类型 | 推荐方法 |
|---|---|
| 稳定单比特电平 | 两级同步器 |
| 短脉冲事件 | toggle、展宽+握手，或源域 sticky |
| 错误历史 | 源域 sticky，CPU clear 用 toggle/握手返回 |
| 多位活动计数 | Gray counter 或冻结快照握手 |
| 多位配置 | staging + commit + 目标域边界生效 |

常见错误：

- 窄脉冲直接进两级同步器，CPU 可能永远看不到。
- 多位二进制计数器逐位同步，CPU 可能读到混合值。
- APB 域直接清除另一个时钟域的 sticky 位，可能产生亚稳态或丢清除。

`staging -> commit -> active` 很适合未来的多寄存器视频配置，但它不是当前 UART 调试链路已经实现的功能，讲解时要区分“架构目标”和“代码事实”。

## 8. 构建命令应如何理解

当前 CPU Makefile 默认依赖 Efinity/RISC-V 工具链和真实 `soc.h`。

`STANDALONE_TEST=1` 的作用仅是绕过部分硬件生成依赖，适合做占位环境下的语法或主机侧检查；它不能证明：

- UART 基地址正确；
- RISC-V 固件可链接、下载和启动；
- Type-C 管脚已经连通；
- APB 寄存器可访问。

此外，当前普通 Windows PowerShell 中未发现可直接使用的 `make`。应在下列一种已配置环境运行并记录：

- Efinity 提供的 shell；
- 团队配置好的 MSYS/MinGW；
- WSL 中已配置的 RISC-V 工具链。

真实上板验收必须使用本次 Efinity 工程导出的 `soc.h` 和对应 linker。

## 9. 串口调试示例

Phase C：

```text
video-console boot
fw=uart-phase1-20260709
uart=SYSTEM_UART_x baud=115200

> ping
pong

> status cpu
cpu=alive hb=1234 rx=18 tx=96 overflow=0
```

Phase D：

```text
> dump id
magic=0x375A0001 caps=0x00000001
```

Phase E：

```text
> status video
frame_ready=1 frame_ok=1 bridge_active=1
level_ready=0 level_low=1 hdmi_ready=0
fifo_underflow_sticky=1 bridge_underflow_count=3
```

这段输出只能作为格式示例。字段存在与否、位定义和数值必须来自实际 RTL 版本，不能预先伪造。

## 10. 如何安全制造和复现异常

推荐：

- 使用已有 reset/test pattern/fallback 选择做单变量实验；
- 使用一次构建的受控 bypass，并立即恢复；
- 对比同一 bitstream 的 LED、UART 原始寄存器和屏幕现象；
- 断电后检查或更换 MIPI 排线。

不推荐：

- 带电拔插摄像头排线；
- 随意提高 HDMI 刷新率；
- 同时修改时钟、复位、FIFO 阈值和 UART；
- 用 heartbeat 自动复位视频链路来掩盖根因。

## 11. 与 myCobot 的安全边界

- Type-C UART 控制台不得发送机械臂动作。
- 本次不启用 UART2，不写 myCobot 协议。
- PC 串口断开、CPU 崩溃或 heartbeat 超时都不得自动触发机械臂动作。
- 未来若加入 UART2，必须单独审查电平、接线、`1000000` 波特率、互锁、超时和急停策略。

## 12. 最终验收清单

### SoC/UART

- [ ] `.peri.xml` 中有真实 SoC 配置。
- [ ] UARTx 明确路由至 B12/D12。
- [ ] UART2 未被改动。
- [ ] 使用本次生成的 `soc.h`、linker 和时钟值。
- [ ] Efinity setup/hold 与 warning 已记录。

### CPU 控制台

- [ ] boot banner 稳定。
- [ ] `ping`、`echo`、超长输入处理通过。
- [ ] 日志限流，不阻塞主循环。
- [ ] UART 开启前后视频表现无回归。

### APB/视频状态

- [ ] `REG_MAGIC` 先独立通过。
- [ ] RTL、CPU 头文件、寄存器文档版本一致。
- [ ] 每个调试字段记录来源域和 CDC 方法。
- [ ] 多位计数器未逐位两级同步。
- [ ] sticky clear 有跨域握手。
- [ ] UART、LED 与屏幕现象使用同一 bitstream 对照。

## 13. Codex 文档更新审核与方案疏漏分析

针对 Codex 刚刚更新的《Type-C UART1 视频调试控制台技术方案》(plan) 和本《讲解指南》(guide)，我已经完成了全面的分析与评估。

**审核结论：Codex 的更新极为关键且完全合理。**

### 13.1 当前（原）方案的疏漏与 Codex 更新理由
在 Codex 介入审查之前，原技术方案存在一个致命的“基础设施高估”错误。原方案认为：“CPU 软件已基本准备好，仅仅是因为没配置 UART1 的物理引脚导致无法通信”。
Codex 经过对底层 Efinity 工程 (`mem_test.peri.xml`) 和源码的严格核查，指出了以下**严重疏漏**，这也是其重写方案的核心理由：
1. **SoC 根本不存在**：目前的 `.peri.xml` 中 `soc_info` 是空的，意味着连 SapphireSoC 软核都还没有在硬件工程里实例化，更谈不上里面的 UART 外设了。
2. **基地址全是假的**：由于 SoC 没生成，真正的 `soc.h` 并不存在。当前 C 代码里 `bsp.h` 用的全是从示例抄来的占位地址，如果按原方案硬连，必然会导致总线访问异常甚至程序跑飞。
3. **寄存器处于“图纸阶段”**：虽然架构草案里写了各种视频状态寄存器，但在真实 RTL 代码里，CPU 可以通过 APB 总线访问的“视频状态寄存器块”根本没写。
4. **时机不对**：当前 HDMI 显示本身还在定位，如果盲目把一整个 SoC 加进去，排错时将无法分辨是“原有的视频流坏了”还是“新加的 SoC 破坏了时序”。

### 13.2 方案的具体实现步骤总结
为了弥补上述疏漏，Codex 在讲解指南中为你梳理了全新的、从零到一的“五阶段（Phase A-E）”实现步骤。我在此为你做核心总结，方便全面理解整个链路的搭建：

*   **步骤 1 (阶段A - 冻结现状)**：不要立刻加 SoC。先用 FPGA 上现成的 LED 灯确认现在的 HDMI 视频到底坏在哪、走到哪了，并记录下来，作为之后对比的“基线”。
*   **步骤 2 (阶段B - 铺设基础设施)**：打开 Efinity 软件，真正实例化 SapphireSoC，把时钟、复位连好，引出一个 UART 外设到 Type-C 的 B12/D12 管脚上。综合后生成真实的 `soc.h`。
*   **步骤 3 (阶段C - 打通串口)**：写裸机 C 代码，先让 CPU 仅仅通过 UART 往电脑屏幕上打印一行字 (TX)，成功后再写接收电脑命令 (RX) 的逻辑，实现 `ping -> pong`。
*   **步骤 4 (阶段D - 联通软硬)**：在 FPGA 里写一段最简单的 APB 寄存器，固定返回一个魔数（如 `0x375A0001`）。CPU 通过真正的地址去读，如果读对了，证明“CPU 终于能和 FPGA 自定义逻辑说话了”。
*   **步骤 5 (阶段E - 接入真实状态)**：最后一步，才是把真正的帧率、FIFO 溢出、HDMI 准备好等信号，经过安全的跨时钟域处理（CDC）后，挂在 APB 寄存器上。CPU 通过串口把这些状态转换成人类可读的字符串打印出来。

**总结**：Codex 的更新强行踩了刹车，避免了您在没有“地基”（SoC与寄存器）的情况下直接去建“二楼”（改C代码收发）。按照现有的 Phase A-E 步骤，您可以步步为营，安全打通这条视频调试链路。

---

**Codex 修订说明：** 本指南由 Codex 于 2026-07-09 根据当前正式工程事实重写，修正了“仅缺 UART 引脚”、SoC UART1 地址猜测、错误 LED 语义、PowerShell 构建命令、CDC 过度简化和不安全异常制造方法。后续实施应以生成工程产物和上板记录更新本文。
