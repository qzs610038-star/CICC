# Review Packet: A2 隔离硬核 SoC 生成

> 日期：2026-07-12  
> 审查状态：待审核后才允许创建视频工程隔离集成副本

## 目标

为板上 CPU/APB 最小闭环生成可追溯的 `TJ375N529` 硬核 SoC 候选配置，同时严格隔离现有 HDMI 合成源基线和仓库开发树。

## 本次修改范围

仅修改/新增隔离目录 `C:\fpga_soc_isolated\tj375_soc_a2_20260712` 内的生成物、日志和 `map_check/a2_soc_map_top.v` 验证夹具。仓库的 `top.v`、`mem_test.xml`、`.peri.xml`、`constrain.sdc`、CPU 源码和 D 盘构建树均未被本操作改动。

## 候选配置与接口事实

| 项目 | 值 |
|---|---|
| 硬件 | `Titanium / TJ375N529` |
| UART0 BSP 基址 | `0xe8010000` |
| APB0 BSP 基址 | `0xe8100000` |
| APB0 窗口 | 4096 bytes |
| `SYSTEM_CLINT_HZ` | 200000000 |
| 生成 UART | 仅 UART0，初始 115200 baud |
| 用户中断 | 8 路；UART0 关联 user interrupt A，B-H 默认 0 |
| 自动管脚 | 关闭；本轮未指定本板 UART 引脚 |

CPU/FPGA 职责边界保持不变：FPGA 只提供 APB/CDC/OSD 等硬件通道，CPU 继续负责颜色/形状/尺寸、四任务关系、逐轮状态机和 myCobot 协议。

## 验证结论

Efinity `efx_map.exe` 对隔离顶层、`A2MinSoc.v` 和 `EfxSapphireHpSoc_wrapper.v` 返回 0，完整日志位于 `C:\fpga_soc_isolated\tj375_soc_a2_20260712\map_check\efx_map.log`。语法分析、层级展开、外围 UART/APB 逻辑映射和网表写出均通过；输出网表为 `map_check/out/a2_soc_map_check.v`。

早期一次 map 失败由验证夹具将输出端口接常量导致；随后一次失败由漏传 `PERI_GEN` 使官方 wrapper 后处理缺失导致。两者均已复现、定位并以重新运行官方脚本/修正夹具解决，没有手工修补生成 RTL。

## 阻塞问题

1. APB0 位宽不一致：生成 `A2MinSoc.v` 的端口为 32 位，但外围 APB 输出为 12 位，wrapper map 报 `32 <-> 12` warning。集成前必须明确是把地址低 12 位作为窗口内偏移，还是修订官方 wrapper 生成模板/连接策略；不得静默截断。
2. APB0 没有用户寄存器：生成 wrapper 当前固定 `PREADY=1`、`PRDATA=0`，`PSLVERROR` 未驱动；任何 `REG_MAGIC`、视觉特征、配置或 ACK 都尚未实现。
3. map 是纯 RTL 隔离检查，未把 `.peri.xml` 放入真实 Efinity 工程，未验证硬核实例、PLL 资源、JTAG、CPU 取指、UART 引脚、时钟/复位或 APB 时序。

## 审核后的最小允许动作

若审核同意，创建视频工程的单独隔离副本，且只能依次完成：

1. 将生成 SoC/IP/`soc.h` 纳入副本，并将 `.peri.xml`、时钟和复位端口与真实板级工程核对。
2. 处理 APB 地址宽度 warning，明确 4 KiB 窗口的地址语义。
3. 实现只读 `REG_MAGIC` APB 从机和确定性的 `PREADY/PSLVERROR`，先做 CPU 读回。
4. 仅在 Hello + `REG_MAGIC` 板级证据通过后，再讨论 `LIVE_FG_AREA`、结果 CDC 和 OSD。

禁止在该审查通过前把生成物同步到 C/D 主工程；禁止扩展到 UART2、机械臂、OSD 或完整视觉寄存器。
