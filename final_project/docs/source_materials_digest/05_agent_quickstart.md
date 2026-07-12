# 05 Agent Quickstart

> 来源路径仅用于本机追溯；原件线下获取，仓库不收录。

## 给队友本地 Agent 的目标

你看到的是 GitHub 同步后的精简仓库，不包含 `赛方提供材料/` 和 `初赛demo/` 原件。请先阅读本目录摘要，再围绕 `final_project/` 做视觉分析、文档汇总和接口建议。

## 推荐读取顺序

1. `AGENTS.md`：架构、安全、审查和协作硬边界。
2. `final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md`：最新官方任务、评分与现场流程。
3. `CURRENT_STATE.md`：当前完成度、阻塞、暂缓项和下一步。
4. `CLAUDE.md`：Claude 专属执行方式；其他 Agent 可据此理解入口分工。
5. `final_project/README.md`：正式工程入口。
6. `final_project/docs/source_materials_digest/README.md` 与 `03_main_video_demo.md`：来源摘要与经验库。
7. `final_project/integration/video_pipeline.md`、`fpga_cpu_interface.md`、`register_map.md`：接口契约；其中占位地址不得当作硬件事实。
8. `final_project/docs/architecture/riscv_official_examples_integration.md`：RISC-V 例程吸收结论。
9. `分赛区决赛实施开发路线.md`：仅在需要历史实施路线或经验时阅读。

## 视觉队友任务入口

- 梳理 MIPI -> RAW -> debayer/gamma -> ROI -> feature_extract -> CPU 分类 -> OSD 的数据流。
- 提出 ROI 统计特征清单：颜色均值/比例、亮度、饱和度、bbox、面积、填充率、宽高比、尺寸标定量。
- 给 `final_project/fpga/rtl/feature_extract/README.md` 和 `final_project/integration/video_pipeline.md` 补充可执行的接口建议。
- 汇总“必须上板验证”的视觉问题，不把官方 demo 构建结果当作决赛证据。

## 文档汇总任务入口

- 统一术语：FPGA 视频前端、板上 CPU 决策、myCobot 由板上 CPU 控制。
- 文档中涉及原始资料时统一写：原件线下获取，仓库不收录。
- 不写“仓库里有 EDA 软件、培训视频、机械臂安装包、官方 PDF/DOCX 原件”等会误导 clone 用户的说法。

## 不要做

- 不再从官方 PDF/DOCX 另建重复转写；比赛规则统一引用 `competition_manual/` 下的规范 Markdown。
- 不复制官方 RTL/C 源码到摘要。
- 不建议把纯 FPGA 视觉识别或纯 RTL 机械臂控制作为决赛主线。
- 不修改约束、IP settings、工程 XML、RTL 顶层或机械臂动作脚本，除非另有明确任务和审查。
