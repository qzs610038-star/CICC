# RTL 分层

- `top/`：顶层集成和双通道连接。
- `video_in/`：camera_init/I2C 与 MIPI CSI 接收。
- `raw_unpack/`：RAW lane/bit 解包。
- `debayer/`：RAW to RGB 或 Bayer 统计辅助。
- `wb_gamma/`：白平衡、亮度、gamma、颜色归一化。
- `roi_crop/`：固定 ROI 裁剪和 ROI 坐标寄存器。
- `feature_extract/`：面积、bbox、均值、直方图、亮度、前景统计。
- `framebuffer/`：DDR/framebuffer 相关逻辑。
- `osd/`：识别结果、ROI、bbox、状态和计时叠加。
- `dvi_tx/`：HDMI/TMDS 输出。
- `cpu_if/`：AXI/寄存器/FIFO、CPU 读写与结果回写。
- `uart_bridge/`：UART/FIFO/寄存器硬件通道，预留双 UART。
- `debug/`：debug_regs、probe aliases 和调试保留信号。
