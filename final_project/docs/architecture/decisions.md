# Architecture Decisions

## ADR-001: 采用 FPGA 前端 + 板上 CPU 决策控制

决策：正式路线采用 FPGA 负责视频前端、ROI、统计特征和 OSD，板上 CPU 负责颜色/形状/尺寸分类、任务判定、参数管理和 myCobot 控制。

原因：纯 RTL 识别和纯 RTL 机械臂控制在阈值调整、异常处理和赛场调试上风险过高；板上 CPU 属于指定 FPGA 平台内部资源，适合承担规则决策和协议控制。
