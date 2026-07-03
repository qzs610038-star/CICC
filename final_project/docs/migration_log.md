# Migration Log

记录每次从 `赛方提供材料/` 或 `初赛demo/` 迁入正式工程的来源、目标、修改和验证结果。

| 日期 | 来源 | 目标 | 内容 | 修改 | 验证 | 风险 |
|------|------|------|------|------|------|------|
| 2026-07-01 | 初赛 `sw/` | `cpu/app` / `cpu/build_tools` | CPU 启动、链接、BSP、旧构建脚本 | 正式 Makefile 改为决赛目录结构 | 待本机工具链验证 | 初赛脚本为 shape_detect 专用 |
| 2026-07-01 | 赛方主 demo `constrain.sdc` | `fpga/constraints_notes/constrain_baseline.sdc` | 约束基线 | 未修改 | 待 Efinity 打开确认 | 仅作基线，不代表最终引脚已定 |
| 2026-07-03 | 赛方主 demo `TJ375N529_SC431HAI2LCD_Demo_V3/` | `fpga/efinity/` + `fpga/ip_vendor/` + `fpga/rtl/` | PR1 视频链路迁移（方案乙）：摄像头→HDMI 最小链路工程文件+IP+RTL | `mem_test.xml` 路径 src→`../rtl/`、ip→`../ip_vendor/`（含56条 design_file、2条 ip_info、2条 include）；`dsi_tx_top.v:144` INITIAL_CODE 改 `../rtl/mipi_dsi/Panel_1080p_reg.mem`；`.gitignore` 增 `ipm/`；新增3条 design_file；`top.v` 零改动 | 代码评审完成（9 finding，2 blocking已修复）；待 Efinity 2025.2.288.4.15 打开验证（阶段 1 判据 8 条） | 见 review_packets/PR1 + execution_summary 评审修复记录；DSI IP 版本 2025.2.288.3.8 低于工程；MIPI TX DPHY 物理引脚悬空驱动；`P1_lcd_rstp` vs `P1_o_lcd_rstn` 隐式 net |
