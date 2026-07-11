# feature_extract

面积、bbox、颜色均值/直方图、亮度统计和前景统计。

当前独立预处理首版：

- `pixel_mask_2ppc.v`：生成可配置红/蓝/黄和通用前景掩码。
- `feature_accumulator_2ppc.v`：累加 ROI 内统计、颜色面积、前景面积和 bbox。
- `feature_snapshot.v`：帧完成时锁存稳定快照，支持直接 ack 测试语义。
- `vision_preprocess_channel.v`：单通道封装，后续由顶层对称实例化两次。

首版不做最终颜色、形状或尺寸分类，也不接 APB、DDR 或 OSD。完整接口和交接约束见 `../../docs/technical_plans/fpga_vision_preprocess_execution_plan_20260711.md`。
