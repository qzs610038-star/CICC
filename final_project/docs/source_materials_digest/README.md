# Source Materials Digest

> 扫描日期：2026-07-02
> 同步原则：只同步 Markdown 摘要；来源路径仅用于本机追溯；原件线下获取，仓库不收录。

本目录给没有 `赛方提供材料/` 和 `初赛demo/` 原始资料包的队友使用。它不是官方资料镜像，而是从本机资料包中提炼出的快速入口、边界说明和后续分析索引。

## 阅读顺序

1. `00_inventory_overview.md`：先了解资料包规模、目录分布和哪些内容没有进仓库。
2. `03_main_video_demo.md`：视觉队友优先读，理解当前可借鉴的视频链路和禁止直接照搬的边界。
3. `01_hardware_board_io.md`：做引脚、MIPI、UART、JTAG 或外设连接前读。
4. `02_efinity_toolchain.md`：配置 Efinity、打开工程或复现官方流程前读。
5. `04_mycobot280_notes.md`：本机机械臂调试主机使用，队友只需了解控制边界。
6. `05_agent_quickstart.md`：给队友本地 agent 的最短任务入口。
7. `06_open_questions_and_validation.md`：把未验证项转成后续上板、视觉和文档工作清单。

RISC-V 官方例程的详细吸收结论已经存在于 `final_project/docs/architecture/riscv_official_examples_integration.md`。本同步包不重复新建 RISC-V 摘要，避免两份文档维护出分歧。

## 分工

- 本机：负责 myCobot 280、CP210x/串口、`pymycobot` 环境、安全姿态、只读状态读取和后续小幅动作验证。
- 队友：负责视觉板块分析、官方视频 demo 经验吸收、文档汇总和比赛材料整理。
- 共同边界：正式比赛闭环仍坚持 FPGA 视频前端、板上 CPU 识别决策和参数管理、板上 CPU 控制 myCobot；PC 与外部工具只用于开发期调试、标定和审查。

## 不同步内容

仓库不收录以下原件：`赛方提供材料/`、`初赛demo/`、安装器、压缩包、视频、PDF、DOCX、Efinity 生成物、ModelSim/Questa 生成物。若确需查看原件，按摘要中的来源路径在本机或线下资料包中查找，不通过 GitHub 同步。
