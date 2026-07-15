# M2 Hard SoC 资源重规划操作包

> 日期：2026-07-15
>
> 适用范围：仅用于 `competition_project_single_camera/` 的 M2 阶段 CPU UART 启动验证。
>
> 本文档不允许手工合并 `.peri.xml`、Efinity 生成的 RTL、`soc.h` 或时序约束。

## 本次目标

在不改变已验证的 J48/ch0 到 HDMI 视频数据链路的前提下，建立最小的板上 CPU 证据：

```text
由 Efinity 生成 Hard SoC
-> 同次生成 BSP 与 soc.h
-> 运行独立 UART0 Hello 程序
-> 电脑串口终端收到 BUILD_ID
```

本 Gate 明确不包含：`REG_MAGIC`、APB、按键 GPIO 事件输入、ROI 特征、OSD、UART2、myCobot 以及任何机械臂动作。

## 进入本 Gate 前的事实

- 当前视频 bitstream 已在板上验证 J48/ch0 到 HDMI 正常。
- 当前视频工程的 `mem_test.peri.xml` 中 `<efxpt:soc_info/>` 为空，即尚未加入 Hard SoC。
- 当前视频工程已占用 `PLL_BL0`、`PLL_BL1`、`PLL_BL2`、`PLL_TR0` 和 `JTAG_USER1`。
- 官方 Hard SoC 的系统 PLL 只能使用 `PLL_BL0`、`PLL_BL1` 或 `PLL_BL2`；这三组目前均是视频链路的在用资源。
- 官方 TJ375C529 eMMC 例程包含 Hard SoC、生成的 BSP、`soc.h`、linker 以及独立 `uartEchoDemo`。它仅用于参考生成流程，不能整套复制进视频工程。

因此，禁止把 SoC 片段直接复制到视频工程的 `.peri.xml`。资源规划必须由 Interface Designer 完成并生成。

## 操作步骤

### 1. 先做工程外备份

在源工程目录之外，新建带时间戳的备份目录，复制以下四个文件：

```text
mem_test.peri.xml
mem_test.xml
constrain.sdc
src/top.v
```

不要覆盖 `competition_project_single_camera/`，也不要覆盖现有 `D:\TJ375N529_SC431HAI2LCD_Demo_V3` 的 `outflow/`。

### 2. 打开 Interface Designer

1. 使用 Efinity `2025.2.288.4.15` 打开 `D:\TJ375N529_SC431HAI2LCD_Demo_V3\mem_test.xml`。
2. 打开 Interface Designer。

### 3. 修改前留证

在任何改动之前，记录并截图以下资源当前的占用情况：

```text
PLL_BL0
PLL_BL1
PLL_BL2
PLL_TR0
JTAG_USER1
clk_25m / GPIOT_P_50
ddr_clk_ref / GPIOL_25
```

### 4. 先判断是否存在合法的资源重规划

仅通过 Interface Designer 的 GUI 判断：是否能迁移或释放一组视频正在使用的 `PLL_BL*`，同时保持下列项目不变：

- 原时钟来源和输出频率
- 复位行为
- DDR 依赖
- CSI 依赖
- HDMI 依赖

禁止手工编辑 XML 或生成 RTL 来强行挪动 PLL。

如果没有可用的、且不影响视频的系统 PLL，立即停止，不再继续本操作包。

### 5. 仅当出现合法系统 PLL 时，创建最小 Hard SoC

在 Interface Designer 中按下列边界创建：

- 系统 PLL：只选择 GUI 显示为合法、空闲或已合法重规划出的 `PLL_BL*`。
- JTAG：使用 `JTAG_USER2` 或其他不与 `JTAG_USER1` 冲突的 JTAG 用户块。
- 调试串口：仅配置 UART0，不配置 UART2。
- 自定义外设：本 Gate 一律不添加 APB slave、AXI bridge、业务按键 GPIO 或 myCobot 串口。
- 保留现有视频 GPIO 管脚：不能把 `GPIOT_P_50`、`GPIOL_25` 再定义为 SoC 的新时钟 GPIO。

### 6. 由 Interface Designer 生成

运行 Interface Designer 的生成操作。生成后的文件不得手工修改。

### 7. 生成后立刻停止，等待审查

此时不要运行 Map、PNR、bitstream，也不要烧录。请回传以下完整生成物：

```text
更新后的 mem_test.peri.xml
生成的 SoC wrapper 和 define 文件
生成的 BSP 目录
生成的 soc.h
生成的 linker 脚本
OpenOCD/debug 配置（若生成）
Interface Designer 的资源截图
生成日志和所有 warning
```

## Codex 审查通过条件

只有同时满足以下所有条件，才允许进入下一步：

- 没有重复的 GPIO 或封装管脚定义。
- SoC 系统 PLL 实际合法，且不与仍在工作的任何视频 PLL 重叠。
- SoC 的 JTAG 不与 `JTAG_USER1` 重叠。
- DDR、CSI、HDMI 原有资源仍被保留，且可与当前工程比较生成后的信号名和频率。
- `soc.h` 确实来自这一次生成，而不是官方 eMMC 例程。
- 生成设计和开发板资料中能看到 UART0 选用的引脚与电脑终端连接路径。

## 失败即停止规则

- 若无法找到不破坏视频的合法系统 PLL：停止，仅回传资源表；不再修改现有视频源码和工程文件。
- 若生成过程重写了无关的视频资源：停止，回传改动对比；不要 Map、PNR 或烧录。
- 若没有生成 `soc.h`：不得编写正式 MMIO 适配层，也不能宣称 CPU UART 已可用。
- 本 Gate 不允许连接机械臂、不允许配置 UART2，也不允许发送任何动作帧。

## 审查通过后的下一步

Codex 审核生成物通过后，才会增加一个基于该 BSP 的最小 UART0 独立程序。它仅输出：

```text
CICC M2 HELLO
BUILD_ID=<本次生成配置的标识>
UART0=<来自本次 soc.h 的地址>
```

在后续单独审查通过 `REG_MAGIC` APB Gate 之前，CPU 程序不得访问 FPGA 特征寄存器。
