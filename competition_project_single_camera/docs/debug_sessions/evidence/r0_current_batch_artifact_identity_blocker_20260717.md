# R0 阻塞报告：当前批次匹配制品不可用

> 时间：2026-07-17（Asia/Shanghai）
> Gate：R0 `USER2 + UART0 + APB MAGIC` 基础验证前置核验
> 裁定：`BLOCKED / NO USER2 OR UART ACTION PERFORMED`

## 目标与边界

本次只核对当前 `main` 是否拥有可用于 R0 的、与已冻结冷构建证据匹配的 FPGA bitstream 和 UART0 Hello ELF。未启动 Programmer、OpenOCD、GDB 或串口，未选择 USER1/USER2，未触及 Flash、外部 DDR、UART2/J52 或机械臂。

## 输入身份核验

- 实读仓库：`main@9acf4d8b2ec788ccd5777f3833a7bfb756c51cad`。
- G1 Review Packet 的有效冷构建基线：`489ab5b0c1773bfcb776cedd9f39b3f088fb4a0f`。
- `489ab5b..HEAD` 的单摄差异仅包含 G2 Host/runtime 文件和 G1 Packet；`mem_test.xml`、`mem_test.peri.xml`、`constrain.sdc`、`src/top.v`、`src/apb_reg_magic.v`、Hard SoC `settings.json`、Hello `build.ps1` 与 `main.c` 均未改变。
- 因而 G1 输入批次仍可对应当前 HEAD；其固定制品身份为：
  - bitstream `A897E33514A1079BB1B46C02C464B0BD679AF551CEFCB67C4A0EBD5B8FCD1ACD`
  - Hello ELF `E5BC80A2F18A7E2951D53DA539BE2FC61AAECFA90C5CDADB29E65FFC6141928A`

## 本机可得制品

| 文件 | 实读 SHA-256 | 与 G1 固定批次 |
|---|---|---|
| `outflow/mem_test.bit` | `3B1182511175CAC98BF95BCDC1119A5BE75102E82A5711632FF8F53915714D93` | 不匹配 |
| `cpu_bringup/uart_hello_onchip/build/uart_hello_onchip.elf` | `2A71E0000F6BEE03C321D3B871F522AF0BA7C602A9E9E8E77B23BA15FD80C448` | 不匹配 |
| `<external-g1-evidence>/output/mem_test.bit` | 缺失 | 不可用 |
| `<external-g1-worktree>/cpu_bringup/uart_hello_onchip/build/uart_hello_onchip.elf` | 缺失 | 不可用 |

历史 M2 操作卡所固定的 `2EA4.../C99...` 亦是旧批次，不得替代当前 G1 的 `A897.../E5BC...`。

## 阻塞结论

当前没有可证明匹配的 bitstream/ELF 对，因此不得开始 FPGA 易失性配置、`JTAG_USER2` RAM 下载、CPU 取指、UART0 监听或 APB 实读。此结论是制品身份阻塞，不是 CPU、APB、UART 或板卡功能失败。

## 最小恢复动作

1. 由已配置的 Efinity 2025.2 环境，在当前 `main@9acf4d8...` 对单摄工程执行受控冷构建并重新构建 Hello ELF。
2. 将 bitstream、ELF、输入 hash、Map/Interface/PNR/STA/CDC、warning 分类写入新的当前批次 R0 构建证据；确认 WNS/WHS 非负且无未处理 CDC warning。
3. 仅当新 bitstream/ELF hash 与该证据一致时，重新进入 R0 的 USER2/SRAM 操作卡审查。不得从旧 `outflow`、历史 ASCII 暂存目录或历史板测记录继承 PASS。

## 保持 NOT VERIFIED

`USER2`、CPU 取指、UART0 115200 横幅/回显、APB `0xE8100000` 实读、视频与 R5 识别闭环均保持 `NOT VERIFIED`。
