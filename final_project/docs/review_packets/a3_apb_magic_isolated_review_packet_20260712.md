# Review Packet: A3 隔离 APB REG_MAGIC

> 日期：2026-07-12  
> 审查目标：决定是否允许进行 SoC `.peri.xml` 与视频工程的隔离合并。

## 已完成

- 从 `D:\final_project\fpga` 创建独立 A3 视频副本，未覆盖 C/D 主工程。
- 固化 APB0 语义：`0xe8100000` 基址 + `PADDR[11:0]` 偏移，窗口 4 KiB。
- 新增零等待、只读 `REG_MAGIC` 从机，偏移 `0x000` 返回 `0x375A0001`。
- 生成 SoC、APB 从机和隔离顶层的 Efinity map 通过。
- 生成 `soc.h` 下的 CPU `board_io.c` 预处理通过，已解决 `IO_APB_SLAVE_0_INPUT` 与旧 `IO_APB_SLAVE_0_BASE` 命名不匹配。

## 未授权事项

本包不授权修改视频顶层、`mem_test.xml`、`mem_test.peri.xml`、`constrain.sdc` 或 D 盘工程；也不授权 PNR、bitstream、烧录、UART2 或机械臂。

## 必须审查的问题

1. A2 SoC `.peri.xml` 与现有视频工程 `.peri.xml` 的 PLL、JTAG、硬核 SoC 资源是否可共存，及应以哪个 Interface Designer 数据库为基准。
2. 生成 SoC 所需 `io_peripheralClk/io_peripheralReset` 是否能由视频工程中已存在的资源安全提供；不得根据 RTL 名称猜测。
3. 硬核 UART0 是否有与本板 Type-C/JTAG-IF 匹配的安全引脚和电平证据；没有证据时保持不约束、不烧录。
4. `REG_MAGIC` 接入视频顶层后，原 HDMI 合成源基线 map/PNR 的新增 warning、时序与 I/O 约束影响。

## 通过后的最小动作

只创建并修改 A3 的 Efinity 工程元数据和顶层连接，保持 APB 从机只读；执行 map，必要时再进行受控 PNR。未完成 CPU Hello 与硬件 APB 读回前，不增加 `LIVE_FG_AREA`、CDC 或 OSD。
