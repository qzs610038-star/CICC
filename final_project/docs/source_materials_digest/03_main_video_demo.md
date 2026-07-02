# 03 Main Video Demo

> 来源路径仅用于本机追溯；原件线下获取，仓库不收录。

## 来源入口

- `赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/`
- 工程入口：`mem_test.xml`、`mem_test.peri.xml`
- 顶层和主要源码：`src/top.v`
- 关键目录：`src/mipi_csi/`、`src/framebuffer/`、`src/debayer/`、`src/mipi_dsi/`、`src/dvi_tx/`、`src/axi_interconnect/`、`src/uvc_src/`
- IP 入口：`ip/csi_rx_controller/`、`ip/dsi_tx/`、`ip/ram/`

## 链路理解

主 demo 适合作为视频链路经验库。它覆盖摄像头输入、MIPI CSI 接收、I2C 相机配置、帧缓存、DDR 读写、RAW 到 RGB、亮度/对比度调整、MIPI DSI/显示输出和 HDMI/DVI 相关模块。

可借鉴的模块职责：

- `mipi_csi`：MIPI CSI 接收、相机 I2C 初始化、视频流入口。
- `framebuffer`：视频帧缓存、DDR 写读缓冲、位宽转换、帧信息检测。
- `debayer`：Bayer RAW 到 RGB 的基础转换、line buffer、RGB gain。
- `contract_bright`：亮度和对比度调整参考。
- `dvi_tx` / `mipi_dsi`：显示输出链路参考。
- `axi_interconnect` / `axi_mux`：总线连接和仲裁参考。

## 可吸收经验

- 调试顺序应从彩条/显示输出开始，再接 MIPI 摄像头、DDR/framebuffer、debayer/gamma、OSD 和识别统计。
- 视觉队友应优先分析 RAW 相位、曝光、白平衡、gamma、ROI 和前景统计，不应直接跳到复杂识别。
- 决赛正式工程需要把 FPGA 定位为视频前端、ROI/统计特征和显示叠加提供者；颜色、形状、尺寸、任务匹配和阈值管理放到板上 CPU。

## 禁止边界

- 不直接把官方 demo 的 `src/top.v` 当决赛顶层基线。
- 不直接迁移官方 demo 的生成 IP、旧 `outflow` 结论或本机 debug profile。
- 不从 demo 中恢复纯 FPGA 视觉识别主线。
- 不把 demo 的历史构建结果当作当前工程质量保证；正式工程必须重新综合、布局布线、检查 warning 并上板验证。
