# 单摄 Hard SoC 板级配置索引

> 用途：集中保存单摄 Hard SoC 的 FPGA、JTAG、CPU、片上 RAM、UART0 和 APB0 联调配置。
> 当前批次：2026-07-16 APB0 `REG_MAGIC` Gate。
> 适用工程：`competition_project_single_camera/`。
> 状态真源仍为仓库根目录 `CURRENT_STATE.md`；本文件是操作配置索引，不替代构建日志和 Review Packet。

## 使用规则

1. 每次上板前先核对“当前批次产物”，不得把旧 bitstream、旧 ELF 或来源副本证据混入当前批次。
2. 每次重新生成 bitstream 或重新构建 ELF 后，必须更新文件大小、SHA-256 和验证状态。
3. COM 号会随 USB 插拔变化，每次联调都要重新枚举；本文件中的 COM10 只表示 2026-07-16 当前枚举。
4. 工具安装目录只通过本机环境或安装发现获取，不把绝对安装路径写入仓库。
5. 任何与真实 `settings.json`、生成 BSP、工程 XML 或板级现象不一致的值，立即标记为失效并回到真实文件核查。

## 固定工程契约

| 项目 | 配置 |
|---|---|
| FPGA | TJ375，JTAG IDCODE `0x006A0EF3` |
| Efinity | `2025.2.288.4.15` |
| CPU | QCRV32，4 hart，RV32IMAC |
| CPU JTAG | FPGA `USER2`，IR=`9` |
| 视频 JTAG | FPGA `USER1`，本 Gate 禁止使用 |
| 片上 RAM | `0xF9000000`，16 KiB，末地址 `0xF9003FFF` |
| UART0 MMIO | `0xE8010000` |
| UART0 物理管脚 | RX=`GPIOR_165`，TX=`GPIOR_145` |
| UART0 参数 | 115200 baud，8 data bits，no parity，1 stop bit，no flow control |
| SoC reset | `GPIOL_79` |
| APB0 基址 | `0xE8100000` |
| APB0 窗口 | 4096 bytes |
| `REG_MAGIC` | 偏移 `0x000`，只读期望值 `0x375A0001` |
| DDR AXI | AXI1 关闭；本 Gate 不初始化或访问外部 DDR |
| SoC 时钟 | `soc_system_clk=594 MHz`，`soc_memory_clk=237.6 MHz` |

固定地址的直接证据位于：

- `embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/include/soc.h`
- `embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/linker/default_i.ld`
- `ip/EfxSapphireHpSoc_slb/settings.json`
- `mem_test.xml`
- `mem_test.peri.xml`
- `constrain.sdc`

## 当前批次产物

| 产物 | 仓库相对位置 | SHA-256 | Git 状态 |
|---|---|---|---|
| FPGA bitstream | `outflow/mem_test.bit` | `138F435C7F2B6CA2EDA2605CABCBA1C44D3C0BD6E3A7AE1D54D244DA12277D15` | 本地生成，不提交 |
| UART0 Hello ELF | `cpu_bringup/uart_hello_onchip/build/uart_hello_onchip.elf` | `67A1AE3CFB7D8388CD3C8D14906FBF6F4128EF2722AEC0A8BF145F27C6050B26` | 本地生成，不提交 |

本批次 Hello ELF 契约：

- 大小：2608 B / 16 KiB。
- 入口：`0xF9000000`。
- 唯一 LOAD 段：`0xF9000000..0xF9000A30`。
- 无未解析符号。
- `ELF_LOAD_AUDIT=PASS`。

## FTDI 与 JTAG

2026-07-16 当前链路枚举：

| 项目 | 当前值 |
|---|---|
| FTDI VID:PID | `0403:6011` |
| JTAG URL | `ftdi://0x0403:0x6011:1:8/2` |
| OpenOCD FTDI channel | `1` |
| FTDI layout | `ftdi layout_init 0x08 0x0b` |
| FPGA SRAM 加载频率 | 3 MHz |
| 检测设备数 | 1 |
| 检测 IDCODE | `0x006A0EF3` |

bitstream 仅允许通过官方 JTAG programmer 临时加载到 FPGA SRAM。不得选择 Flash erase/program。

## OpenOCD USER2 配置

OpenOCD 使用 Efinity 2025.2 随附版本。配置必须保留以下语义：

```tcl
riscv use_bscan_tunnel 6 1
riscv set_bscan_tunnel_ir 9
```

注意：官方 Ti375C529 示例可能写成 IR=`8`，本工程生成配置和工程契约是 `USER2` IR=`9`，不得照抄示例值。

4 个 hart 必须建立为独立 target：

| Target | coreid | dbgbase | work area |
|---|---:|---:|---:|
| `fpga_spinal.cpu0` | 0 | `0x10B80000` | `0xF9000000` |
| `fpga_spinal.cpu1` | 1 | `0x10B81000` | 不设置 |
| `fpga_spinal.cpu2` | 2 | `0x10B82000` | 不设置 |
| `fpga_spinal.cpu3` | 3 | `0x10B83000` | 不设置 |

禁止使用 SMP 聚合 `RUN_SMP_APP`。历史实测中聚合 resume 会导致 `pc=0`、`mcause=1`；本 Gate 只运行 CPU0，CPU1..3 保持 halt。

## CPU 启动顺序

USER2 连接并识别 4 个 hart 后，先执行 NDMRESET：

```tcl
riscv dmi_write 0x10 0x00000003
sleep 100
riscv dmi_write 0x10 0x00000001
sleep 200
```

随后严格按以下顺序：

1. halt CPU0..CPU3。
2. 将 Hello ELF 下载并校验到 `0xF9000000` 片上 RAM。
3. 明确保持 CPU1..CPU3 halt。
4. 设置 CPU0 PC=`0xF9000000`。
5. 只 resume CPU0。
6. 从 UART0 核对完整横幅，再发送单个 ASCII `K` 核对回显。

## UART0 验收

2026-07-16 当前 UART0 枚举为 COM10。打开参数：115200 8N1，无流控。

预期完整横幅：

```text
TJ375 CPU+VIDEO UART0 HELLO
ONCHIP_RAM=0xF9000000 UART0=115200 8N1
Type characters to verify echo.
```

完整横幅和单字符 `K` 回显同时成立，才可记录 `CPU EXECUTION + UART0 PASS`。仅能连接 JTAG、仅下载 ELF、仅看到 HDMI 或仅看到部分串口文本均不算通过。

## APB0 REG_MAGIC 验收

只有 UART0 Hello/回显通过后，才允许进行一次只读：

```text
mdw 0xE8100000 1
```

唯一期望值：

```text
0x375A0001
```

本 Gate 不测试非法写、不测试非法偏移，也不扩展到 feature snapshot 或 OSD。

## 当前板级状态

| Gate | 状态 | 证据摘要 |
|---|---|---|
| bitstream SHA-256 | PASS | 当前本地产物为 `138F435C...` |
| JTAG 链检测 | PASS | 1 个 TJ375，IDCODE `0x006A0EF3` |
| FPGA JTAG SRAM load | PASS | 官方 programmer 报告完成，未写 Flash |
| HDMI 回归 | PASS | 用户确认画面正常 |
| CDONE | PASS | 用户确认点亮 |
| USER2 识别 4 hart | NOT VERIFIED | 待当前 bitstream 实测 |
| NDMRESET + CPU0 取指 | NOT VERIFIED | 待当前 bitstream 实测 |
| UART0 完整横幅 | NOT VERIFIED | 待当前 bitstream 实测 |
| UART0 单字符回显 | NOT VERIFIED | 待当前 bitstream 实测 |
| APB0 `REG_MAGIC` 实读 | NOT VERIFIED | Hello 通过后仅执行一次只读 |

## 当前禁止项

- 禁止写 Flash 或执行 Flash erase/program。
- 禁止使用 `USER1`。
- 禁止初始化或访问外部 DDR。
- 禁止 UART2/J52、机械臂接线、机械臂协议或任何机械臂动作。
- 禁止从其他工程副本拼接 OpenOCD、BSP、RTL 或工程配置。
- 禁止在 `REG_MAGIC` Gate 通过前扩展 feature snapshot、OSD、分类算法或任务状态机。

## 证据入口

- 离线 Review Packet：`docs/review_packets/m2_hard_soc_apb_magic_offline_review_20260716.md`
- Hello 说明：`cpu_bringup/uart_hello_onchip/README.md`
- 历史操作说明：`docs/debug_sessions/m2_uart0_onchip_hello_operator_guide_20260715.md`
- 工程日志：`WORK_LOG.md`
- 最新项目状态：仓库根目录 `CURRENT_STATE.md`
