# M2 UART0 CPU Hello — 匹配 bitstream 易失性配置验收

> 日期：2026-07-16
> 适用范围：仅 `competition_project_single_camera/` 隔离 CPU UART0 Hello。
> 裁定：`VOLATILE_BITSTREAM_CONFIG_PASS / USER2_RAM_LOAD_PENDING`。

## 1. 操作员证据

操作员提供的 Efinity Programmer 截图（未复制入仓库；文件 SHA-256：`50F819E0B92288CE2B2EA419CAAB9A4F03AF567AAB282136F750A21FA60F109B`）显示：

- USB 目标为 FTDI `0403:6011`，序列号 `FTBI7G42`；
- bitstream 位于操作员指定 ASCII 暂存根目录下的 `m2_cpuhello_20260716_1730/mem_test.bit`；
- 器件为 `TJ375N529`，模式为 JTAG，读取器件 ID 为 `0x006A0EF3`，时钟为 6 MHz；
- 控制台在 15:55:08 开始对该专属路径编程，在 15:55:28 完成，15:55:32 显示 `Device is in user mode!`。

截图中没有 Flash/SPI/PROM 持久化操作、USER1/USER2 选择、ELF 下载、UART 打开或机械臂连接现象。

## 2. 制品身份的新鲜复核

Codex 在验收截图后对下列两个现存文件重新执行 `Get-FileHash -Algorithm SHA256`：

| 文件 | 大小 | SHA-256 | 结果 |
|---|---:|---|---|
| 仓库批准制品 `outflow_m2_cpuhello_20260716_1730/mem_test.bit` | 11,843,718 B | `2EA4AD287CCC6BFBF113E718B59EF6F5807222AFE1ECE3D55BD1E24D37FB8347` | PASS |
| 实际编程的专属 ASCII 暂存镜像 `m2_cpuhello_20260716_1730/mem_test.bit` | 11,843,718 B | `2EA4AD287CCC6BFBF113E718B59EF6F5807222AFE1ECE3D55BD1E24D37FB8347` | PASS |

两项完整 hash 与批准批次一致，且彼此相等。此前 `outflow/mem_test.bit` 的 `9515...A169` 误选记录仍保留在 `m2_uart0_programmer_attempt_review_20260716.md`，但不再代表当前 FPGA 的最后一次易失性配置。

## 3. 此门关闭的范围

本证据仅关闭“匹配 M2 bitstream 已通过 JTAG 易失性配置”的门。它不证明：

- QCRV32 `JTAG_USER2` 已连接；
- CPU 已从 `0xF9000000` 片上 RAM 取指；
- UART0 横幅或回显存在；
- 视频、`final_project` A0、UART2/J52、myCobot 或任何运动门可进入。

## 4. 下一张操作卡

仅可执行 `m2_uart0_user2_ram_download_operator_card_20260716.md`：使用 `JTAG_USER2`，把固定 Hello ELF 下载到 `0xF9000000` 片上 RAM，停在 `_start` 或 `main` 后保存证据。首次 USER2 下载完成前，继续禁止 UART 字节发送、Flash、USER1、外部 DDR、UART2/J52 与机械臂。
