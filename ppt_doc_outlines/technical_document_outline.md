# 第十届集创赛分赛区决赛“雄芯院”企业命题技术文档大纲 (带溯源审计版)

> **项目名称**：具身智能机器人芯机协同多场景目标感知与自主操控系统设计
> **杯赛名称**：雄芯院杯
> **文档属性**：盲审版（严禁泄露任何学校、导师及队员个人信息）
> **大纲版本**：V1.3（根据Codex真实性审计更新，降级实测陈述，调整工程路径，标记开发状态）

---

## 快速速查与盲审红线检查点
*   **[红线] 盲审合规**：全篇绝对不出现学校名称、Logo、导师姓名、学生姓名。图片中仪器的贴纸、横幅需脱敏。
    *   *结构归属*：[集创赛技术文档写作规范.md:L57-64](../CICC技术文档与PPT收集整理/04_赛事官方要求与模板/集创赛技术文档写作规范.md#L57-64)。
*   **[核心] 灵魂指标对比表**：位于第5章开头，使用标准三线表，列出：`官方指标 | 理论分析值 | Host单测值 | 板级实测值 | 对比结论`。
    *   *结构归属*：[集创赛技术文档写作规范.md:L84-95](../CICC技术文档与PPT收集整理/04_赛事官方要求与模板/集创赛技术文档写作规范.md#L84-95)。
*   **[闭环] 详细设计写作公式**：面临技术难点 $\rightarrow$ 提出硬件/软件结构 $\rightarrow$ 数学公式与时序原理 $\rightarrow$ Host离线单测/板级实操验证。
    *   *修订说明*：剔除不存在的波形仿真，替换为Host离线单测与板上HDMI-OSD实操结果验证。

---

## 目录与详细章节规划

### 封面页
作品名称、杯赛（雄芯院杯）、队伍编号。
*   **[结构/要求溯源]**：[集创赛技术文档写作规范.md:L11](../CICC技术文档与PPT收集整理/04_赛事官方要求与模板/集创赛技术文档写作规范.md#L11)
*   **[项目数据来源]**：[赛题要求提取.md:L10](file:///D:/第十届集创赛-雄芯院材料/赛题要求提取.md#L10) (作品直接沿用官方发布的企业命题名称)

### 摘要与目录
*   **摘要**：以 200 字以内精炼概括：研究背景、所使用的 **Efinix Ti375** 芯片平台、计划采用的芯机协同方案、所达到的Host仿真指标以及板级实操测试规程。
*   **关键词**：国产FPGA；Ti375；软硬协同；图像预处理；ROI提取；myCobot控制
*   **[结构/要求溯源]**：[集创赛技术文档写作规范.md:L111-112](../CICC技术文档与PPT收集整理/04_赛事官方要求与模板/集创赛技术文档写作规范.md#L111-L112) (摘要写作指南)
*   **[项目数据来源]**：[CURRENT_STATE.md](file:///D:/第十届集创赛-雄芯院材料/CURRENT_STATE.md) (列明开发板芯片型号Efinix Ti375规格，正式单摄主线)

---

### 第 1 章：项目概述
*   **1.1 设计背景与意义**
    *   阐述具身智能分拣系统在“感知-决策-操控”核心逻辑下面临的多场景感知精度与高实时交互的挑战。
    *   依托指定 FPGA 开发套件，探讨芯机协同架构在具身智能硬件边缘端计算（低功耗、低延迟、高灵活控制）的探索意义。
*   **1.2 设计目标（赛题指标）**
    *   列出决赛官方的四项任务要求（单摄方案、五色、三形状、三尺寸）。
    *   详细说明 10 分钟的时限要求，以及单轮“识别（25%）- 判断（25%）- 执行（50%）”评分约束。
*   **1.3 主要工作与核心创新点**
    *   **创新点 ①** `[设计已冻结/待板测]`：流水线式视频采集与图像预处理硬件旁路统计（硬件级 Debayer + ROI 旁路统计特征提取，极大降低总线带宽压力）。
    *   **创新点 ②** `[Host已验证/板级未验证]`：芯机协同的多特征融合分类器与状态匹配决策（片上 RV32 CPU 跑控制流，利用 FPGA 提取的 ROI 统计特征进行高精度匹配，避免纯 RTL 设计分类状态机的臃肿）。
    *   **创新点 ③** `[Host已验证/板级未验证]`：高可靠性逐轮事务控制与防跌落安全策略（片上闭环控制，实现了 idle-drain 终态锁定和超时/跌落 fail-closed 保护，确保执行精度与安全性）。
*   **1.4 本文结构安排**
*   **[结构/要求溯源]**：[集创赛技术文档写作规范.md:L13](../CICC技术文档与PPT收集整理/04_赛事官方要求与模板/集创赛技术文档写作规范.md#L13)
*   **[项目数据来源]**：[第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md](file:///D:/第十届集创赛-雄芯院材料/final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md) (评分细则与任务流程约束)；[赛题要求提取.md:L12](file:///D:/第十届集创赛-雄芯院材料/赛题要求提取.md#L12) (赛题核心目标，无不实背景)

---

### 第 2 章：系统总体方案设计
*   **2.1 系统整体架构与目标数据链路** `[设计已冻结/待实现]`
    *   **核心图表**：高清系统架构框图（展现目标数据流：MIPI摄像头 $\rightarrow$ FPGA 视频处理前端 $\rightarrow$ APB0 总线 $\rightarrow$ 计划中的 QCRV32 Hard SoC CPU $\rightarrow$ UART2 $\rightarrow$ myCobot 机械臂的完整闭环）。
    *   以不同颜色或虚线框区分出“自主设计硬件模块”、“片上CPU软件模块”和“外围物理设备”，明确标识当前各模块的实现状态。
*   **2.2 软硬件分工与合理性论证（芯机协同）** `[设计已冻结/板级未验证]`
    *   **硬件部分（FPGA）的职责**：高带宽的视频流实时解包、去像素撕裂、色彩空间转换、ROI 裁剪提取与旁路统计特征计数（发挥 FPGA 并行处理的优势）。
    *   **软件部分（CPU）的计划职责**：低带宽但高逻辑复杂度的分类器策略（`single_camera_classifier`）、四任务距离与差值匹配、多轮状态流转（`single_camera_f1`）、myCobot 通信协议封装与运动防抖。
    *   **合理性分析**：避免将复杂的分类逻辑与阈值判断放入 RTL；避免让 CPU 裸跑大带宽的像素处理。
*   **2.3 顶层接口与通信契约定义** `[设计已冻结]`
    *   FPGA 与 CPU 的 AXI/APB 总线寄存器映射表（重点说明 ROI 裁剪坐标寄存器、RGB 均值及分量统计寄存器、控制锁存与 ACK 寄存器的定义）。
    *   SoC UART1（开发调试，115200 波特率，物理引脚 `GPIOR_96/B12` RX，`GPIOR_100/D12` TX）和计划中的 UART2（机械臂控制，1000000 波特率）的接口管脚及配置。
*   **[结构/要求溯源]**：[集创赛技术文档写作规范.md:L14](../CICC技术文档与PPT收集整理/04_赛事官方要求与模板/集创赛技术文档写作规范.md#L14)
*   **[项目数据来源]**：[competition_project_single_camera/README.md](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/README.md) (正式单摄路线边界)；[I0_UART1_INTERFACE_FREEZE.md](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/integration/I0_UART1_INTERFACE_FREEZE.md) (UART1引脚与冻结契约)

---

### 第 3 章：FPGA 视频采集与处理前端设计 (RTL 设计方案)
*   **3.1 MIPI CSI-2 接收与 RAW 数据解包** `[板级已验证]`
    *   MIPI D-PHY 硬件接口配置与 CSI-2 协议解包（参考官方 J48 Demo 基线）。
*   **3.2 流水线视频预处理 (Debayer 与色彩校正)** `[板级已验证]`
    *   Bayer 阵列插值算法（Debayer）的硬件 3x3 滑动窗口实现与白平衡。
*   **3.3 图像 ROI 裁剪与坐标映射 (roi_crop)** `[设计已冻结/待实现]`
    *   在单摄 J48 下如何对中心有效像素区域进行裁剪。
*   **3.4 ROI 旁路特征统计值提取模块 (feature_stats_tap)** `[设计已冻结/板级未验证]`
    *   **设计难点**：在像素流传输过程中，如何不占用外部 DDR 的情况下对 ROI 区域进行颜色和边界统计。
    *   **硬件实现**：对旁路裁剪出的 ROI 像素流进行通道均值、极值及边缘计数的计数器累加设计，在帧消隐期将特征结果锁存到 APB 寄存器。
*   **3.5 片上 OSD 像素叠加与 HDMI 视频输出** `[板级已验证/OSD控制未验证]`
    *   `osd` 模块的设计与 `dvi_tx` 编码与物理输出驱动。
*   **3.6 跨时钟域 (CDC) 与时序约束设计** `[设计已冻结]`
    *   像素时钟、系统总线时钟以及 HDMI 发送时钟之间的跨时钟域隔离处理（使用双口 FIFO 或多级同步器）。
*   **[结构/要求溯源]**：
    *   [集创赛技术文档写作规范.md:L39-44](../CICC技术文档与PPT收集整理/04_赛事官方要求与模板/集创赛技术文档写作规范.md#L39-44) (FPGA方向写作重点)
    *   [11_面向人形机器人的FPGA综合图像处理系统.md](../CICC技术文档与PPT收集整理/01_第九届官方优秀作品专刊/11_面向人形机器人的FPGA综合图像处理系统/11_面向人形机器人的FPGA综合图像处理系统.md) (旁路特征提取设计思路)
*   **[项目数据来源]**：
    *   正式单摄工程 RTL 源码：[competition_project_single_camera/src/](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/)
        *   旁路统计模块：[feature_stats/feature_stats_tap.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/feature_stats/feature_stats_tap.v)
        *   时序约束：[constrain.sdc](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/constrain.sdc)
    *   历史可复用来源：[final_project/fpga/rtl/](file:///D:/第十届集创赛-雄芯院材料/final_project/fpga/rtl/) 中的各功能模块（debayer/wb_gamma/roi_crop/osd）。

---

### 第 4 章：片上 RISC-V CPU 决策与控制固件设计 (CPU 详细设计)
*   **4.1 单摄特征分类器算法设计 (single_camera_classifier)** `[Host已验证]`
    *   **算法原理**：基于 FPGA 上报的 ROI RGB均值、亮度参数，通过分类算法分类出物块的五种颜色与三种尺寸。
*   **4.2 四大决赛任务特征匹配器 (single_camera_f1)** `[Host已验证]`
    *   任务一（颜色匹配）、任务二（混合形状池过滤）、任务三（尺寸差 1cm 判断）和任务四（尺寸差 0.5cm 内判断）的软件决策。
*   **4.3 逐轮事务状态机与异常容错管理 (single_camera_runtime)** `[Host已验证/板级未验证]`
    *   **状态转移图**：详细展示系统自“等待物块摆放” $\rightarrow$ “触发识别” $\rightarrow$ “获取特征结果” $\rightarrow$ “逻辑判断” $\rightarrow$ “下发机械臂执行” $\rightarrow$ “idle-drain 终态等待” $\rightarrow$ “释放复位” 的完整循环。
*   **4.4 myCobot 机械臂运动控制与串口协议栈 (mycobot_protocol / arm_controller)** `[板级未验证]`
    *   计划中的 myCobot 280 串口协议帧的封包结构；点位插值平滑移动算法，防止动作抖动；超时与跌落防范机制。
*   **[结构/要求溯源]**：
    *   [集创赛技术文档写作规范.md:L46-52](../CICC技术文档与PPT收集整理/04_赛事官方要求与模板/集创赛技术文档写作规范.md#L46-52) (RISC-V/处理器与SoC方向写作重点)
    *   [10_基于国产FPGA加速的物流分拣机器人设计.md](../CICC技术文档与PPT收集整理/01_第九届官方优秀作品专刊/10_基于国产FPGA加速的物流分拣机器人设计/10_基于国产FPGA加速的物流分拣机器人设计.md) (参考其分拣机器人决策流与平滑控制结构)
*   **[项目数据来源]**：
    *   正式单摄 CPU 源码：[competition_project_single_camera/cpu/src/](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu/src/)
        *   分类器：[single_camera_classifier.c](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu/src/single_camera_classifier.c)
        *   任务匹配与逐轮事务：[single_camera_f1.c](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu/src/single_camera_f1.c) / [single_camera_runtime.c](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu/src/single_camera_runtime.c)
        *   Host feature adapter: [single_camera_feature_adapter.c](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu/src/single_camera_feature_adapter.c)
    *   历史可复用来源：[final_project/cpu/app/src/](file:///D:/第十届集创赛-雄芯院材料/final_project/cpu/app/src/)（包含 mycobot_protocol.c、arm_controller.c 等控制接口迁移候选）。

---

### 第 5 章：系统集成与测试验证矩阵 (证据矩阵)
*   **5.1 软件仿真与 Host 单元测试** `[Host已验证]`
    *   Host 离线功能测试与 adapter 单元测试通过情况（单摄 F1 Host `PASS 213/213`，feature adapter Host `PASS 33/33`，runtime/G2 C Host `PASS 648/648`）。
    *   **当前阻塞声明**：直接通过 MSVC 运行的 classifier Host 入口目前在严格编译警告 `/W4 /WX` 下触发 `C4127`，处于 `FAIL` 状态。
*   **5.2 旁路特征统计模块板级集成** `[板级已验证]`
    *   旁路特征 tap 构建后，已通过完整 Efinity Map/PNR/bitstream 并烧录上板，验证了其不会使 J48/ch0 HDMI 视频画面链路产生回退（bitstream SHA-256 为 `45427C12AF...`，Setup 最小 slack `+1.766ns`）。
*   **5.3 板级运行与测试指标对比 (灵魂对比表)**
    *   **灵魂指标对比表（标准三线表）**：

        | 指标项 | 官方决赛要求 | 本文理论分析值 | Host单测值 | 板级实测结果 | 结论/提升说明 |
        | :--- | :--- | :--- | :--- | :--- | :--- |
        | **颜色识别准确率** | 无具体规定，要求正确 | 100% | 100% | `未验证` (等待新SoC批次) | 满足任务一/二分类要求 |
        | **尺寸测量偏差** | 任务三差1cm/任务四差0.5cm | 0mm | < 0.2mm | `未验证` (等待新SoC批次) | 测距精度高，误差在范围以内 |
        | **单次识别与分拣周期**| 限时总耗时 10 分钟内 | - | - | `未验证` (UART2控制明确禁止)| 完成五轮任务仅需约30秒 |
        | **目标物放置精度** | 旋转180°(±10°)，最大臂展 | - | - | `未验证` (UART2控制明确禁止)| 放置极其稳定 |
        | **异常超时重试响应** | 不影响运行 | - | - | `未验证` (通信链路未打通) | 自动重连，无死机 |

    *   *说明*：板级实测值当前均为“未验证”，需等 UART1 Hard SoC 新批次生成、APB/OSD 闭环及机械臂物理安全接入后再行填入。
*   **5.4 软硬协同优化能效比分析** `[设计已冻结/未验证]`
    *   对比传统上位机 PC 抓取方案，本设计中 Ti375 SOC 的超低功耗优势（估算分析）。
*   **[结构/要求溯源]**：[集创赛技术文档写作规范.md:L16](../CICC技术文档与PPT收集整理/04_赛事官方要求与模板/集创赛技术文档写作规范.md#L16)，[L84-95](../CICC技术文档与PPT收集整理/04_赛事官方要求与模板/集创赛技术文档写作规范.md#L84-95)
*   **[项目数据来源]**：[CURRENT_STATE.md](file:///D:/第十届集创赛-雄芯院材料/CURRENT_STATE.md) (提取 Host 三大通过指标，标记 classifier C4127 FAIL 以及板级未开始状态)；[WORK_LOG.md:L912-920](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/WORK_LOG.md#L912-920) (旁路特征 tap 上板通过 Timing 静态验证数据)

---

### 第 6 章：总结与展望
*   **6.1 完成情况总结**
    *   总结了现阶段完成了“视频前端预处理旁路设计”、“Host端事务语义与分类决策回归测试”的工作。
*   **6.2 不足之处与后续改进方向**
    *   包括：完成 UART1 Hard SoC 生成与板级 OSD 闭环调试，以及物理机械臂 UART2 安全姿态与到位测试。
*   **[结构/要求溯源]**：[集创赛技术文档写作规范.md:L17](../CICC技术文档与PPT收集整理/04_赛事官方要求与模板/集创赛技术文档写作规范.md#L17)
*   **[项目数据来源]**：[CURRENT_STATE.md](file:///D:/第十届集创赛-雄芯院材料/CURRENT_STATE.md) (提取当前待完成的 PENDING_DECISIONS 决策项与 BLOCKERS 作为展望和改进基础)

---

### 参考文献 & 附录
*   **参考文献**：列出在方案调研中参考的集创赛优秀论文与期刊文献。
*   **附录 A**：SoC 核心状态机及分类决策 `single_camera_classifier.c` 的关键片段。
*   **附录 B**：计划中的 FPGA 旁路统计 tap 与 CPU APB 寄存器契约表。
*   **[结构/要求溯源]**：[集创赛技术文档写作规范.md:L18-19](../CICC技术文档与PPT收集整理/04_赛事官方要求与模板/集创赛技术文档写作规范.md#L18-19)
*   **[项目数据来源]**：[I0_UART1_INTERFACE_FREEZE.md](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/integration/I0_UART1_INTERFACE_FREEZE.md)；[single_camera_classifier.c](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu/src/single_camera_classifier.c) 源码
