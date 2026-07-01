# Codex 修正说明

已采纳 Claude 审核中的合理建议，并落实到 `final_project/` 骨架：

- 创建 `dvi_tx/`、`roi_crop/`、`raw_unpack/`、`debayer/`、`wb_gamma/` 等 RTL 分层目录。
- 创建固定仿真目录：`raw_unpack/`、`roi_crop/`、`feature_extract/`、`cpu_if/`、`osd/`、`dvi_tx/`。
- 创建 `cpu/params/README.md`，定义颜色阈值、形状参数、尺寸标定、机械臂点位和任务配置文件格式。
- 创建 `integration/README.md` 和 `docs/README.md`，明确接口契约与人读文档边界。
- 创建 `.gitignore`，覆盖主要 Efinity、ModelSim、RISC-V IDE 和 CPU 构建产物。
- 从初赛 `sw/` 迁入启动、链接和 BSP 基线；旧 shape_detect 构建脚本保存在 `cpu/build_tools/legacy_shape_detect/`，正式 `cpu/app/Makefile` 使用新的决赛目录结构。
- 将赛方 `constrain.sdc` 复制到 `fpga/constraints_notes/constrain_baseline.sdc` 作为约束基线。
