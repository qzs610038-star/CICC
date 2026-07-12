# A8 Efinity GUI 审查隔离副本创建记录

> 日期：2026-07-12  
> 状态：隔离副本已创建，尚未打开 GUI 或修改 PLL/SoC 资源。

## 副本

- 源：`D:\final_project\fpga`
- 目标：`C:\fpga_soc_isolated\tj375_video_soc_gui_a8\fpga`
- 排除：`outflow`、`work_*` 构建目录。

`top.v`、`mem_test.xml`、`mem_test.peri.xml`、`constrain.sdc` 与源文件 SHA-256 均一致；复制过程没有写入 D 盘源工程或仓库工程。

## 用途与边界

该副本只用于回答“Efinity GUI 是否能在保持视频时钟依赖可审查的前提下释放/重规划一颗 `PLL_BL*` 供硬核 SoC 使用”。

下一步由用户在 GUI 中打开隔离副本的 `efinity/mem_test.xml`，在 Interface Designer 记录各 PLL 的输入、输出、下游连接和 JTAG 资源。禁止直接编辑 `.peri.xml`、删除 LPDDR4 PLL、运行 PNR 或烧录。
