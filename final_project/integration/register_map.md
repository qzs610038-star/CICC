# Register Map

待填写 FPGA 输出特征字段、CPU 回写字段和 OSD 显示字段。

第一版至少覆盖：

- FPGA -> CPU：ROI 坐标、面积、bbox、颜色/亮度统计、frame counter、valid bit。
- CPU -> FPGA：color、shape、size、match、action、arm_state、error_code。
- Debug：链路心跳、丢帧计数、CDC 状态、UART 状态。
