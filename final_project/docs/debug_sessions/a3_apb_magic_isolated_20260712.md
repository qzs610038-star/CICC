# A3 隔离 APB REG_MAGIC 记录

> 日期：2026-07-12  
> 状态：地址语义已裁定，隔离 RTL map 与 CPU 宏预处理通过；未进入视频工程或板级验证。

## 本轮结论

已基于 `D:\final_project\fpga` 创建 A3 隔离视频副本：`C:\fpga_soc_isolated\tj375_video_soc_a3_20260712\fpga`。副本创建时 `top.v` 和 `mem_test.xml` 与 D 盘 HDMI 合成源基线 SHA-256 一致，复制过程排除了 `outflow` 和各历史 `work_*` 目录。

APB0 正式候选语义已裁定：生成 BSP 的 `IO_APB_SLAVE_0_INPUT=0xe8100000` 是 4 KiB 窗口基址，RTL 从机只能使用 `PADDR[11:0]` 窗口内偏移。`REG_MAGIC` 固定为偏移 `0x000`、读值 `0x375A0001`；该值与既有 `board_io_validate()` 的高 16 位检查兼容。

## 已验证

| 项目 | 结果 | 证据 |
|---|---|---|
| 生成 SoC -> APB0 -> 从机 RTL | Efinity map PASS，退出码 0 | `C:\fpga_soc_isolated\tj375_video_soc_a3_20260712\map_check\efx_map.log` |
| 映射网表 | 已写出 | `C:\fpga_soc_isolated\tj375_video_soc_a3_20260712\map_check\out\a3_soc_apb_proof.v` |
| CPU 生成 `soc.h` 兼容 | `board_io.c` 预处理 PASS | `C:\fpga_soc_isolated\tj375_video_soc_a3_20260712\cpu_app\app\build_board_io_preprocessed.c` |

新增逻辑仅在隔离副本：`apb_magic_slave.v` 对合法只读访问零等待返回魔数；所有写/其他偏移访问没有副作用且报告 `PSLVERROR=1`。没有新增分类、任务判断、OSD 或机械臂逻辑。

## NOT VERIFIED / 阻塞

- 未合并 A3 SoC `.peri.xml` 到视频工程 Interface Designer 数据库，故硬核 CPU、PLL、JTAG、UART0 引脚、时钟复位和 PNR 均未验证。
- `apb_magic_slave` 尚未接入视频 `top.v` 或 `mem_test.xml`，没有 bitstream、烧录、CPU Hello 或真实 `board_io_validate()` 读回。
- 本机没有可用 Verilog 仿真器和 QCRV32 交叉工具链；行为仿真与固件 ELF/HEX 构建未运行。
- 禁止同步 A3 内容到 C/D 主工程；禁止推进 `LIVE_FG_AREA`、CDC、OSD、UART2 或机械臂。

## 下一步

在 A3 内审查 SoC `.peri.xml` 与视频工程 Interface Designer 合并方案，确认 SoC 时钟/复位/JTAG/UART0 端口后，再将 `REG_MAGIC` 接入视频顶层并执行受控 map。只有工程级 map/PNR 通过后，才考虑板上 CPU Hello 和 APB 读回。
