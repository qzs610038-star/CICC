# Migration Log

记录每次从 `赛方提供材料/` 或 `初赛demo/` 迁入正式工程的来源、目标、修改和验证结果。

| 日期 | 来源 | 目标 | 内容 | 修改 | 验证 | 风险 |
|------|------|------|------|------|------|------|
| 2026-07-01 | 初赛 `sw/` | `cpu/app` / `cpu/build_tools` | CPU 启动、链接、BSP、旧构建脚本 | 正式 Makefile 改为决赛目录结构 | 待本机工具链验证 | 初赛脚本为 shape_detect 专用 |
| 2026-07-01 | 赛方主 demo `constrain.sdc` | `fpga/constraints_notes/constrain_baseline.sdc` | 约束基线 | 未修改 | 待 Efinity 打开确认 | 仅作基线，不代表最终引脚已定 |
