# A4 SoC 与视频工程资源冲突核查

> 日期：2026-07-12  
> 状态：工程级集成被物理资源冲突阻塞；本轮只读核查，未修改 RTL、`.peri.xml`、`mem_test.xml`、约束、C/D 工程或 D 盘基线。

## 结论

A2 最小硬核 SoC 不能直接引入当前视频工程。视频工程已经占用 `PLL_BL0`、`PLL_BL1`、`PLL_BL2`、`PLL_TR0` 与 `JTAG_USER1`；A2 则固定请求系统 PLL `PLL_BL0`、外设 PLL `PLL_TR0` 和 `JTAG_USER1`。

核查官方硬 SoC IP 参数后确认，`PLL_SOC_SYS_RESOURCE` 仅允许 `PLL_BL0/PLL_BL1/PLL_BL2`，三者均已被视频工程占用。虽然 JTAG 可从 `JTAG_USER1` 迁至 `JTAG_USER2`，外设 PLL 也可选择更多资源，但系统 PLL 无未占用的合法候选，故不能通过小改动关闭冲突。

SoC 自动创建的时钟输入 GPIO 也与视频工程重叠：`GPIOT_P_50`（视频 `clk_25m`）和 `GPIOL_25`（视频 `ddr_clk_ref`）。同一 pad 不能在 `.peri.xml` 中以两个 GPIO 实例重复声明。

## 证据

- 视频 Interface Designer 数据库：`C:\fpga_soc_isolated\tj375_video_soc_a3_20260712\fpga\efinity\mem_test.peri.xml`
- A2 SoC 生成数据库：`C:\fpga_soc_isolated\tj375_soc_a2_20260712\a2_soc_generated.peri.xml`
- 官方 IP 枚举：`D:\Efinity\2025.2\ipm\ip\efx_hard_soc\ipm\ip_component.xml`
- 官方 JTAG 映射实现：`D:\Efinity\2025.2\ipm\ip\efx_hard_soc\generator\hard_ip_generation.py`
- 隔离完整记录：`C:\fpga_soc_isolated\tj375_video_soc_a3_20260712\A4_SOC_VIDEO_RESOURCE_AUDIT_20260712.md`

## 后续门禁

必须先在 Efinity GUI / Interface Designer 中，以视频工程为基础决定是否允许重新规划任一视频 `PLL_BL*` 及其 DDR/MIPI/视频时钟依赖。该架构决定通过后，才可使用官方 IP Manager 重生成 SoC（JTAG 应改为 `JTAG_USER2`），再对实际生成物做资源交集检查。

禁止直接合并 XML、手改生成 RTL 或用重复 GPIO/pad 声明规避冲突。

## NOT VERIFIED

SoC 合并、工程级 map/PNR、bitstream、RISC-V 固件、CPU Hello、APB 实读、UART0/JTAG 引脚及视频时序均未验证。
