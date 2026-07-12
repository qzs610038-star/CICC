# Review Packet: A4 SoC 与视频工程资源裁定

> 日期：2026-07-12  
> 审查结论：**阻塞 A3 工程级接入；不得直接合并。**

## 已验证

| 项目 | 结论 |
|---|---|
| 视频工程 PLL | `PLL_BL0/PLL_BL1/PLL_BL2/PLL_TR0` 均已在 `.peri.xml` 分配 |
| 视频工程 JTAG | `JTAG_USER1` 已分配 |
| A2 SoC 系统 PLL | 请求 `PLL_BL0` |
| A2 SoC 外设 PLL | 请求 `PLL_TR0` |
| A2 SoC JTAG | 请求 `JTAG_USER1` |
| 官方系统 PLL 参数 | 仅允许 `PLL_BL0/PLL_BL1/PLL_BL2`，均已被占用 |
| 官方 JTAG 参数 | 可切换到 `JTAG_USER2/3/4`，但不能解决系统 PLL 冲突 |

## 阻塞原因

SoC 系统 PLL 的官方合法集合与视频工程已占用集合完全重合。任何 A4 小改候选都会继续占用视频 `PLL_BL*` 中的一颗；参数校验通过也不能代表与视频工程可共存。

## 允许的下一步

仅允许由 Efinity GUI / Interface Designer 进行受控资源再规划。必须先确认视频 `PLL_BL*` 的功能依赖、重分配后时钟源和 reset 影响，再用官方生成器重新生成 SoC，并复核实际 `.peri.xml`。

## 禁止项

- 不直接拼接 `.peri.xml`。
- 不手改生成 SoC RTL 或 Interface Designer 资源块。
- 不修改 A3 顶层、`mem_test.xml` 或约束以绕过冲突。
- 不运行 PNR、烧录、UART2 或机械臂动作。

## NOT VERIFIED

GUI 可行的重规划方案、时钟/复位、UART0/JTAG 引脚、工程级 map/PNR、固件、CPU/APB 板级闭环均未验证。
