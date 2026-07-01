# FPGA / CPU Interface

本文件记录 FPGA 与板上 CPU 之间的 AXI、寄存器或 FIFO 接口。

必须明确：

- 地址基址和偏移。
- 读写方向。
- 时钟域和 CDC 处理。
- valid/ready 或 frame-stable 语义。
- CPU 回写结果与 OSD 显示之间的同步关系。
