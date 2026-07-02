# 06 Open Questions And Validation

> 来源路径仅用于本机追溯；原件线下获取，仓库不收录。

## 视觉和视频链路

- 摄像头实际输出模式、RAW 位深、lane 数、Bayer 相位和分辨率需要上板确认。
- 是否先跑 720p/1080p 或官方 demo 稳定模式，需要结合 MIPI、DDR、显示输出和时序余量决定。
- ROI 位置、托盘背景、光照变化、曝光/白平衡策略需要通过实拍数据验证。
- 颜色、形状、尺寸分类阈值应放到 CPU 可调参数表，不能固定在 RTL 中。

## CPU 和接口

- 最终工程的 `soc.h`、linker、OpenOCD/debug profile、地址映射必须由当前 Efinity 工程生成或复核。
- FPGA 到 CPU 的统计特征寄存器、共享缓冲、OSD 回写和状态寄存器需要做最小闭环验证。
- 共享 DDR 如果进入方案，必须验证 cache invalidate/flush、burst、位宽转换和总线仲裁。

## 机械臂

- 本机先确认 CP210x 识别、COM 口、波特率 `1000000`、`pymycobot` 可用性和只读状态读取。
- 点位表、速度、夹爪范围、互锁、超时和急停策略必须在 PC 开发期验证后，再迁移到板上 CPU 控制。
- FPGA 侧只实现 UART/FIFO/寄存器通道，不在 RTL 中实现复杂动作协议。

## 文档和同步

- 每次更新同步包后检查是否仍然 docs-only。
- 不允许 `git status --short` 中出现 `初赛demo` 或 `赛方提供材料` 前缀。
- 不允许摘要暗示原始安装包、视频、PDF/DOCX 已经进入仓库。
- 若队友需要官方原件，走线下或网盘传递，不通过 GitHub 仓库。
