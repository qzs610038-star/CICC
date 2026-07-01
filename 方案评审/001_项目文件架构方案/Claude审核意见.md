# Claude 审核意见

## 首轮阻塞项

- 补充 `fpga/rtl/dvi_tx/`，承载 HDMI/TMDS 输出链路。
- 补充 `fpga/rtl/roi_crop/`，避免固定 ROI 策略散落在其他模块中。
- 拆分 `video_pipe/` 为 `raw_unpack/`、`debayer/`、`wb_gamma/`。
- 补充 CPU 启动代码、链接脚本、Makefile、构建脚本和 BSP 入口。
- 明确 IP 引用策略，避免 `mem_test.xml` 引用赛方目录或本机绝对路径。
- 补充 `.gitignore`，覆盖 Efinity、ModelSim、RISC-V IDE 和 CPU 编译产物。

## 非阻塞建议

- 明确 `tests/fpga_sim/` 第一版必须包含 `raw_unpack/`、`roi_crop/`、`feature_extract/`、`cpu_if/`、`osd/`、`dvi_tx/`。
- 明确 `cpu/params/` 的文件格式和责任边界。
- 写清 `integration/` 与 `docs/` 的边界：前者放机器要遵守的接口契约，后者放人要读的设计、调试和操作说明。

最终判定：通过，建议按修正后方案落地。
