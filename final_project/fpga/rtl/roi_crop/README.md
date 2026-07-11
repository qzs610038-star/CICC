# roi_crop

固定 ROI 裁剪、ROI 坐标寄存器和 ROI valid/de/hs/vs 处理。

当前独立预处理首版：

- `vision_stream_adapter_2ppc.v`：将 Debayer 的 `{B1,G1,R1,B0,G0,R0}` 拆成两个 RGB888 像素，并输出 `x0/x1/y` 坐标。
- `roi_window_2ppc.v`：按半开区间 `[x0,x1) x [y0,y1)` 分别判断两个像素是否命中 ROI。

模块仅在像素域工作；APB 配置和 CDC 在后续集成阶段实现。完整协作约束见 `../../docs/technical_plans/fpga_vision_preprocess_execution_plan_20260711.md`。
