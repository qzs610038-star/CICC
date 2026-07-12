# A2 隔离硬核 SoC 生成与 Map 记录

> 日期：2026-07-12  
> 状态：隔离生成与最小 `map` 已通过；视频工程集成仍未开始  
> 适用范围：A 队员 SoC/APB 前置验证，不适用于 D 盘 HDMI 烧录基线

## 本轮结论

已在 `C:\fpga_soc_isolated\tj375_soc_a2_20260712` 生成一个 `TJ375N529` 最小硬核 SoC，并使用 Efinity `2025.2.288.4.15` 对生成的 wrapper/外围 RTL 完成最小 `map`。隔离 map 退出码为 `0`，日志和映射网表均已保留。

生成配置保留硬核 JTAG、UART0、APB0、8 路用户中断和所需 AXI slave/PLL；关闭 DMA、SPI、I2C、GPIO、WDT、SDHC、TSE、UART1、UART2、AXI master 和 custom instruction。APB0 生成窗口为 4 KiB。生成的 BSP 给出 UART0 地址 `0xe8010000`、APB0 地址 `0xe8100000`、频率 `200 MHz`。

这次结果替代了“完全没有生成 `soc.h`/SoC 产物”的前置缺口，但只针对隔离配置。它不替代仓库内 `generated_soc_summary_2026-07-11.md` 对正式视频工程的结论：正式工程仍未集成这些产物。

## 已验证

| 检查项 | 结果 | 证据 |
|---|---|---|
| IP Manager 参数校验 | PASS | `C:\fpga_soc_isolated\tj375_soc_a2_20260712\ipm_validation.log` |
| 外围 RTL 生成 | PASS | `C:\fpga_soc_isolated\tj375_soc_a2_20260712\ip\A2MinSoc\source\Axi4PeripheralTop.v` |
| 官方后处理生成 | PASS | 后处理后的 RTL 时间戳，以及最终 `C:\fpga_soc_isolated\tj375_soc_a2_20260712\map_check\efx_map.log` 的完整展开记录 |
| 生成 BSP | PASS | `C:\fpga_soc_isolated\tj375_soc_a2_20260712\embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\include\soc.h` |
| 最小 `efx_map` | PASS，退出码 0 | `C:\fpga_soc_isolated\tj375_soc_a2_20260712\map_check\efx_map.log` |

最小 map 的资源是 `EFX_ADD=7`、`EFX_LUT4=9`、`EFX_FF=13`。这不是完整比赛工程的资源估算；夹具只保留生成外围 wrapper 的可观测锥，硬核资源通过 `.peri.xml` 物理配置引入。

## 发现并纠正的问题

首次 wrapper 生成命令漏传 `PERI_GEN`，官方脚本报 `KeyError: 'PERI_GEN'`，造成初版内部 wrapper 端口缺失。该错误未被忽略，也没有手改生成 RTL；已按官方 fileset 顺序补跑 `post_script.py`、`wrapper_script.py`，随后 map 完成语法、层级展开和逻辑映射。

## 未完成和风险

- `NOT VERIFIED`：隔离 map 未带实际 `.peri.xml`、PLL/硬核实体/板级复位/UART 管脚和约束，不能证明 CPU 启动或 Type-C UART 可见。
- `NOT VERIFIED`：无视频工程隔离副本，无 `top.v`、`mem_test.xml`、`constrain.sdc` 或 `.peri.xml` 集成变更。
- `NOT VERIFIED`：没有 APB 从机、`REG_MAGIC`、`LIVE_FG_AREA`、结果 CDC、CPU 到 OSD、RISC-V 应用构建、PNR、bitstream、烧录或板级测试。
- `BLOCKER`：生成 wrapper 报 APB0 `PADDR` 的 `32 -> 12 -> 32` 位宽 warning；接入前必须定版地址传递方式。
- `BLOCKER`：当前 wrapper 仅模板默认 `PREADY=1`、`PRDATA=0`，且 `PSLVERROR` 未驱动，不是可用视觉寄存器从机。
- 不涉及 UART2、机械臂电平/接线或实际动作。

## 后续门禁

下一步只能在新的 CPU/APB Review Packet 获审后，在视频工程的隔离副本中完成 SoC 端口接入和最小 APB `REG_MAGIC` 读回。未通过该门禁，禁止将本目录生成物同步到 `final_project` 或 `D:\final_project`。
