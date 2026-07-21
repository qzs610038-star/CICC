# HISTORICAL / SUPERSEDED — UART0 Hello guide

> This guide is not executable. Current CPU Hello governance is UART1 Type-C, with H0-H6 evidence receipt controlled by `../review_packets/CPU_HELLO_UART1_H0_H6_RECEIPT_20260721.md`.

# M2 UART0 片上 RAM Hello 上板操作说明

## 当前前提

- 联合 bitstream 已烧录并通过 J48/ch0 HDMI视频回归。
- CPU JTAG 固定使用 FPGA `JTAG_USER2`。
- `JTAG_USER1` 属于原视频工程，禁止改占。
- Hello ELF只加载片上 RAM，不写Flash。

## 文件

- ELF：`D:\TJ375N529_SC431HAI2LCD_Demo_V3_cpu_video_joint_20260715_161500\cpu_bringup\uart_hello_onchip\build\uart_hello_onchip.elf`
- 源码和中文说明：`D:\TJ375N529_SC431HAI2LCD_Demo_V3_cpu_video_joint_20260715_161500\cpu_bringup\uart_hello_onchip\README.md`
- 匹配 bitstream：`D:\TJ375N529_SC431HAI2LCD_Demo_V3_cpu_video_joint_20260715_161500\outflow\mem_test.bit`

## UART终端

- 参数：115200 baud、8 data bits、no parity、1 stop bit、no flow control。
- 当前枚举候选：COM9、COM10、COM13，均为FTDI `VID:PID=0403:6011` 的不同通道。
- Windows 历史注册表只读记录显示：FTDI 子接口 A=`COM9`、C=`COM10`、D=`COM13`；B 未留下 COM 端口记录。该信息只用于重连后的差分辨认，尚不能证明哪一个 COM 是 E9/E10 对应的 UART0。
- 在没有锁定 E9/E10 对应通道前，只打开端口监听，不向端口发送字符；机械臂串口不得连接本轮测试。
- 运行固件后应看到：

```text
TJ375 CPU+VIDEO UART0 HELLO
ONCHIP_RAM=0xF9000000 UART0=115200 8N1
Type characters to verify echo.
```

## Efinity RISC-V IDE

1. 启动 `D:\Efinity\efinity-riscv-ide-2025.2\Efinity-RISCV-IDE\efinity-riscv-ide.exe`。
2. 打开 `Run -> Debug Configurations...`。
3. 新建 Efinix/QCRV32 的硬件调试配置；如果列表中存在多个 RISC-V/OpenOCD类型，先截图，不要凭名称猜选。
4. C/C++ Application或ELF字段选择上述 `uart_hello_onchip.elf`。
5. Target必须是TJ375/QCRV32硬核，JTAG必须是FPGA USER TAP `USER2`。看到USER1时停止；USER1是视频工程资源。
6. 选择下载程序到RAM并在 `_start=0xF9000000` 启动。禁止勾选Flash erase/program、外部DDR初始化或写入启动Flash。
7. 第一次Debug连接成功后，确认PC停在 `_start` 或 `main`，再点击Resume。
8. 观察候选串口的启动横幅。确定唯一正确COM后，输入单个ASCII字符测试逐字节回显。

## 官方 softTap 配置审计

- 联合 BSP 已生成：
  - `embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\openocd\external.cfg`
  - `embedded_sw\efx_hard_soc\bsp\efinix\EfxSapphireSoc\openocd\debug_softTap.cfg`
- `external.cfg` 使用板载 FTDI `VID:PID=0403:6011` 的 channel 0 作为 OpenOCD/JTAG 通道；它不是 UART COM 号映射。
- `debug_softTap.cfg` 的 QCRV32 work area 为 `0xF9000000`，与片上 RAM 一致；但其中仍保留生成模板的 `instr_addr=0x00001000`。本次不得使用该旧地址覆盖 ELF 入口或 PC。
- 官方 TJ375 `uartEchoDemo_softTap.launch` 的可复用语义是：`Debug in RAM`、加载 ELF、加载 symbols、连接 OpenOCD、停在 `main`；配置中没有 Flash program/erase 命令。
- 官方 `.launch` 绑定其原 Eclipse 项目名和旧绝对路径，禁止原样复制。本轮在 GUI 中按上述语义配置，并以当前联合 BSP路径和当前 Hello ELF 为准。
- 当前开发板不在场，尚未创建或执行联合工程 `.launch`。只有上板连通后确认 Target、USER2、入口地址和下载日志，才允许固化启动配置。

## 停止条件

出现以下任一情况立即停止并回传截图或Console尾部日志：

- Target不是TJ375/QCRV32。
- JTAG显示USER1或要求新建USER TAP。
- IDE要求擦除/写入Flash。
- ELF加载地址不是 `0xF9000000`。
- OpenOCD/GDB 试图把 PC 设置为模板旧地址 `0x00001000`。
- 无法连接QCRV32、下载失败、CPU停在异常地址或UART无输出。

## 当前验收边界

看到完整横幅并能回显一个字符，才可记为 `CPU EXECUTION + UART0 PASS`。仅能连接JTAG、仅能下载ELF或仅有HDMI画面都不算CPU Hello通过。
