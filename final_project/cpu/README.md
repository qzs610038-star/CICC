# CPU 工程区

本目录承载板上 CPU 正式闭环：识别分类、任务匹配、参数管理、myCobot 协议和动作状态机。

- `bsp_vendor/`：从初赛或 Efinity 工程复制的最小 BSP。
- `app/`：正式应用源码、启动文件、链接脚本和 Makefile。
- `params/`：阈值、标定、点位和任务配置。
- `build_tools/`：Windows 批处理、部署脚本和旧工程迁移基线。
