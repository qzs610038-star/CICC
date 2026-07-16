# M2 Hard SoC APB0 REG_MAGIC 离线 Review Packet

> 日期：2026-07-16
> 分支：`codex/hard-soc-apb-magic-20260716`
> 基线：`db94aa4a38a02c2ea3a738cf9d209f193b4b7526`
> 工具：Efinity `2025.2.288.4.15`
> 当前裁定：APB0 `REG_MAGIC` 离线实现及 Map/Interface/PNR/STA/CDC/bitstream generation 已通过；新 bitstream 尚未上板，APB 实读为 `NOT VERIFIED`。

## 1. 任务与安全边界

本批次只在仓库外 ASCII 隔离 worktree 中为现有单摄 Hard SoC 启用 APB0，并新增最小只读 `REG_MAGIC`。未修改共享主工作区，未连接开发板，未执行 JTAG/program，未写 Flash，未使用 USER1，未初始化外部 DDR，未连接 UART2/J52 或机械臂。

禁止从旧 A3 隔离工程或其他副本拼接 wrapper、BSP、`.peri.xml` 或 RTL。本批次的 Hard SoC 生成物全部来自当前工程中的同一个 `settings.json` 和 Efinity 2025.2 IP Manager。

## 2. Hard SoC 生成配置

用户通过 IP Manager 对现有 `EfxSapphireHpSoc_slb` 进行受控修改：

- APB3 interface 0：启用
- APB0 size：4096 bytes
- APB1-4：关闭
- UART0：保持启用
- FPGA JTAG：保持 USER2，IR=9
- DDR AXI1：保持关闭
- PLL、DDR、AXI、UART0 引脚和 SoC reset：保持原配置

Generate 后，Interface Designer 中 `Periphery Controller Clock Pin Name` 被生成器重置，已恢复为既有适配 `axi0_ACLK`。Check Design 为 `0 error / 4 warning`。

生成物一致给出：

```text
PERI_APB_0=1
PERI_APB_0_SIZE=4096
IO_APB_SLAVE_0_INPUT=0xE8100000
IO_APB_SLAVE_0_INPUT_SIZE=4096
```

## 3. RTL 契约

`src/apb_reg_magic.v` 为纯组合、无状态 APB 从机：

| 访问 | `PRDATA` | `PREADY` | `PSLVERROR` |
|---|---:|---:|---:|
| 偏移 `0x000` 读 | `0x375A0001` | 1 | 0 |
| 偏移 `0x000` 写 | 0 | 1 | access phase 为 1 |
| 其他偏移读/写 | 0 | 1 | access phase 为 1 |
| 非 access phase | 按地址/方向组合 | 1 | 0 |

从机不接收 `PWDATA`，无寄存器状态、无写副作用。`src/top.v` 将 SoC APB0 的地址低 12 位接入从机；该 12 位正好覆盖 4 KiB 窗口内偏移。

## 4. Warning 审查

### 4.1 自定义 `PWDATA` warning

首轮 Map 中，只读从机曾声明但不使用 `PWDATA[31:0]`，产生 32 条冗余信号 warning。移除自定义从机的 `PWDATA` 端口，并把 SoC 的 `PWDATA` 输出在顶层显式留空后，重跑 Map 不再报告任何 `apb_reg_magic/pwdata` warning，访问语义不变。

### 4.2 官方 `PADDR` 32->12 warning

官方生成文件外层模块导出 `io_apbSlave_0_PADDR[31:0]`，其内部 APB0 模块端口为 `[11:0]`，因此 Map 报告 1 条实参 32 位、形参 12 位 warning。以下证据一致：

- APB0 窗口由生成器固定为 4096 bytes
- BSP 基址为 `0xE8100000`，窗口内偏移宽度为 12 位
- 内部 `Axi4PeripheralTop` 导出 `[11:0] PADDR`
- 顶层从机显式使用 `apb0_paddr[11:0]`
- Map/PNR/STA/CDC 均通过

裁定：该 warning 是官方生成层级间对 APB0 窗口内地址的确定性低位截断，可接受但必须记录；不得手改生成 wrapper 消除。此裁定只覆盖该 1 条 warning，不代表继承 Demo/IP warning 可忽略。

## 5. 离线验证结果

### 5.1 Efinity

| Gate | 结果 |
|---|---|
| Map | PASS |
| Interface | PASS，0 error / 4 个既有距离 warning |
| PNR | PASS |
| Worst Setup | `+1.321ns` |
| Worst Hold | `+0.026ns` |
| CDC | `No Synchronizer warnings to report.` |
| PGM/bitstream generation | PASS，仅生成文件 |

Interface 4 个既有 warning：

- `mipi_rx_dp01 / mipi_ln_rule_rx_distance`
- `mipi_tx_ck0 / mipi_ln_rule_tx_distance`
- `mipi_tx_dp12 / mipi_ln_rule_tx_distance`
- `tmds_data2 / lvds_rule_tx_distance`

post-synthesis netlist 仍报告 118 个 warning，与上述 Interface 4 项是不同集合。APB 改造新增的编译期 warning 仅为已裁定的官方 `PADDR` 32->12 项；自定义 `pwdata` warning 已消除。

### 5.2 资源增量

对照基线是已板测 `6B6728...` 对应的精确 main 离线构建，使用相同 Efinity 版本：

| 资源 | `6B6728...` 基线 | APB0/REG_MAGIC | 增量 |
|---|---:|---:|---:|
| EFX_ADD | 2065 | 2065 | 0 |
| EFX_LUT4 | 11204 | 11385 | +181 |
| EFX_FF | 9957 | 10104 | +147 |
| EFX_RAM10 | 165 | 165 | 0 |
| EFX_DPRAM10 | 4 | 4 | 0 |

### 5.3 新 bitstream

```text
file: mem_test.bit
size: 11847132 bytes
SHA-256: 138F435C7F2B6CA2EDA2605CABCBA1C44D3C0BD6E3A7AE1D54D244DA12277D15
```

该文件只存在于本地忽略的 `outflow/`，不提交 Git。没有执行 JTAG SRAM 下载或 Flash program。

### 5.4 UART0 Hello 回归

新生成最小 BSP 下重新构建原片上 RAM Hello：

- 2608 B / 16 KiB
- ELF entry：`0xF9000000`
- 唯一 LOAD：`0xF9000000..0xF9000A30`
- 未解析符号：无
- `ELF_LOAD_AUDIT=PASS`
- 本地 ELF SHA-256：`67A1AE3CFB7D8388CD3C8D14906FBF6F4128EF2722AEC0A8BF145F27C6050B26`

ELF 为本地构建产物，不提交。

### 5.5 行为 testbench

`tests/apb_reg_magic/tb_apb_reg_magic.v` 覆盖空闲、SETUP、合法读、非法写和非法偏移。当前机器未安装 Icarus、Verilator、ModelSim/Questa，因此 testbench 状态为 `NOT RUN`，不得表述为仿真 PASS。Map 已确认 `apb_reg_magic` 与官方 AXI/BMB-to-APB bridge 实际展开，但 Map 不替代协议行为仿真或 CPU 板级实读。

## 6. 未验证项与下一门

以下全部为 `NOT VERIFIED`：

- 新 `138F435C...` bitstream 的 JTAG SRAM 加载
- 冷启动后的真实摄像头 HDMI
- USER2、NDMRESET、4 hart 识别和 CPU 实际取指
- 片上 RAM UART0 Hello 横幅与单字符回显
- CPU 读取 `0xE8100000` 得到 `0x375A0001`
- 非法 APB 写/偏移的 CPU 侧异常表现
- feature snapshot、CPU 分类、OSD、按键、UART2/J52 和机械臂

下一板级门必须再次审核。批准后只允许按以下顺序执行：匹配 bitstream 冷启动 JTAG SRAM 临时加载 -> HDMI 回归 -> USER2 + NDMRESET + 片上 RAM UART0 Hello/回显 -> 单次只读 `0xE8100000`。任一步失败立即停止，不扩大到 feature/OSD/UART2/J52/myCobot；继续禁止 Flash、USER1 和外部 DDR 初始化。
