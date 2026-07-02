# Bring-up Checklist

- 摄像头 I2C 配置完成。
- MIPI 输入有稳定 frame/line 信号。
- `dvi_tx` 输出 HDMI 画面。
- FPGA bitstream 下载后，Efinity RISC-V IDE 可导入/构建 CPU makefile project。
- JTAG/OpenOCD 可 halt/resume CPU，debug 视图能看到寄存器和 memory。
- CPU terminal UART 可打印，并在启动时打印当前 `soc.h` 地址摘要。
- CPU 可读 `REG_MAGIC/REG_VERSION`，确认 AXI/APB 窗口正确。
- CPU 可写 `CPU_HEARTBEAT/RESULT_*`，并显示到 OSD。
- FPGA `FEATURE_VALID/FRAME_ID` 与 CPU `FEATURE_ACK` 握手稳定，无半帧撕裂。
- 若启用共享 DDR/ROI，完成 cache invalidate/flush 对照测试。
- myCobot 串口先完成只读或安全小动作验证。
- myCobot 控制 UART 与调试 UART 的波特率和通道分配已确认。
