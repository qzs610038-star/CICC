# I0 SoC UART1 / Type-C UART1 接口冻结

> 状态：`TEAM CONFIRMED / ROUTE FROZEN / GENERATED IMPLEMENTATION PENDING / BOARD NOT VERIFIED`
> 日期：2026-07-18
> 授权口令：`确认接口文件修改，已经和wsc、libaoxun、qzs沟通。`

## 1. 固定接口

| 项目 | 冻结值 |
|---|---|
| 用途 | I0：QCRV32 生命证明、开发期日志与只读调试控制台 |
| SoC 外设 | `SYSTEM_UART_1`；必须来自新 Efinity Hard SoC 生成物 |
| 板级连接 | Type-C UART1 |
| RX | `GPIOR_96` / `B12` |
| TX | `GPIOR_100` / `D12` |
| 串口格式 | `115200 8N1`，无流控 |
| 输出最小要求 | 完整 Hello、构建/批次标识、单字符回显 |
| 安全边界 | 不初始化 UART2/J52，不发送 myCobot 帧，不触发动作 |

“板载 Type-C UART1”与“Hard SoC UART1”在本方案中明确绑定；不得继续用
`SYSTEM_UART_0` 路由到 Type-C 后仍把 I0 描述为 UART1，也不得手填 UART1
基址、IRQ 或 `soc.h` 宏。

## 2. 当前实现事实

当前仓库生成物仍为旧 UART0 批次：

- `settings.json` / `hard_ip_args.ini`：`PERI_UART_0=1`、`PERI_UART_1=0`；
- wrapper 只暴露 `system_uart_0_io_rxd/txd`；
- `.peri.xml` 只绑定 UART0 的 `GPIOR_165/GPIOR_145`；
- `soc.h` 只包含 `SYSTEM_UART_0_*`；
- `cpu_bringup/uart_hello_onchip/` 与 R0 bitstream/ELF 全部属于历史批次。

因此本页只冻结目标接口，不宣称 UART1 已生成、已构建或已上板。

## 3. 原子生成要求

libaoxun 必须在 Efinity/IP Manager 中一次性完成并提交匹配真源：

1. 启用 `PERI_UART_1`，停用活动 I0 的 UART0；
2. 将 `system_uart_1_io_rxd/txd` 绑定到 `GPIOR_96/GPIOR_100`；
3. 同批生成 IP/wrapper、`.peri.xml`、BSP 和 `soc.h`；
4. wsc 仅根据新 `soc.h` 构建 UART1 片上 RAM Hello，不猜地址；
5. 冷构建并记录 Map/Interface/PNR/STA/CDC、warning、bitstream/ELF hash；
6. qzs 将新批次写入 `CURRENT_STATE.md`，旧 R0 标为历史。

生成文件不得手工逐处替换 `uart_0` 为 `uart_1`。若 Efinity 不能生成完整一致树，
本批次保持 `BLOCKED_GENERATION`。

## 4. 精简 Gate

新输入 hash 固定后只执行一次连续非动作 Gate：

```text
I0-BUILD
  -> USER2 + on-chip RAM
  -> UART1 Hello / echo
  -> APB MAGIC read-only
  -> I0-SMOKE PASS
```

同一输入 hash 中间不重复请求批准。仅在硬件/固件输入、制品 hash、接线或失败
现象变化时重开。I0-SMOKE 不授权 I1-I4 业务寄存器、UART2/J52 或机械臂。

## 5. 变更规则

本文件、确认册、F1 总账及冻结清单中的 I0 固定值只能在以下条件同时满足时修改：

1. 用户再次发送完整接口修改口令；
2. wsc、libaoxun、qzs 给出固定 SHA 和影响范围；
3. Review Packet 说明生产者、消费者、测试、失效证据与回滚点。
