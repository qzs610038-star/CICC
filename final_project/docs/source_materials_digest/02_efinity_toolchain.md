# 02 Efinity Toolchain

> 来源路径仅用于本机追溯；原件线下获取，仓库不收录。

## 来源入口

- `赛方提供材料/EDA软件/`
- `赛方提供材料/EDA软件培训文档及视频/01 软件培训文档/易灵思软件培训.pdf`
- `赛方提供材料/EDA软件培训文档及视频/`
- `赛方提供材料/efinity-2025.2.288.4.15-windows-x64-patch/`
- `赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/mem_test.xml`
- `赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/mem_test.peri.xml`

## 工具链结论

- 当前资料围绕 Efinity 2025.2。队友需要在自己电脑单独安装 Efinity 和必要补丁，clone 仓库不会得到安装包或补丁原件。
- Efinity 工程入口通常是 `.xml` 工程文件，例如主 demo 的 `mem_test.xml`。但正式决赛工程应以 `final_project/fpga/` 下的工程规划为准，不在官方 demo 原目录内修改。
- `outflow/`、`work_syn/`、`work_pnr/`、`work_pt/`、`work_dbg/`、`work_sim/`、`.db`、`.qdb`、`.rpt`、`.log` 等属于工具生成物，不作为同步资料。
- RISC-V IDE、OpenOCD、debug profile、linker、`soc.h` 一类配置必须由最终工程重新生成或核对，不能硬抄官方例程。

## 队友操作建议

- 视觉队友优先读 `03_main_video_demo.md`，再按需要配置 Efinity 打开原始 demo 或正式工程。
- 文档汇总时不要写“仓库包含 EDA 软件/补丁/培训视频”。统一写成：原件线下获取，仓库不收录。
- 对任何 Efinity warning，不默认视为可忽略；需要结合最新构建日志、timing report、目标开发板和上板现象判断。
