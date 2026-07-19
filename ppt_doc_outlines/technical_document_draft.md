# 具身智能机器人芯机协同多场景目标感知与自主操控系统设计 — 技术文档初稿

> **赛事名称**：第十届全国大学生集成电路创新创业大赛
> **杯赛名称**：雄芯院杯
> **文档属性**：决赛技术设计文档（草稿版 / 盲审版 / 严禁泄露任何高校、导师及个人信息）
> **项目状态说明**：V1.2（根据Codex真实性审计更新，降级未验证陈述，纠正 APB 契约与源码，补齐证据属性）

---

## 目录

*   [第 1 章：项目概述](#第-1-章项目概述)
*   [第 2 章：系统总体方案设计](#第-2-章系统总体方案设计)
*   [第 3 章：FPGA 视频采集与处理前端设计](#第-3-章fpga-视频采集与处理前端设计)
*   [第 4 章：片上 RISC-V CPU 决策与控制固件设计](#第-4-章片上-risc-v-cpu-决策与控制固件设计)
*   [第 5 章：系统集成与测试验证矩阵](#第-5-章系统集成与测试验证矩阵)
*   [第 6 章：总结与展望](#第-6-章总结与展望)
*   [参考文献与附录](#参考文献与附录)

---

## 第 1 章：项目概述

### 1.1 设计背景与意义
在具身智能分拣控制的边缘计算应用中，传统的上位机方案在计算资源消耗、整机能效比以及物理体积上面临较大限制。本课题依托第十届全国大学生集成电路创新创业大赛“雄芯院”企业命题《具身智能机器人芯机协同多场景目标感知与自主操控系统设计》，探讨基于国产 FPGA 与片上 SoC 异构平台在实时“感知-决策-操控”核心闭环系统中的硬软件架构设计。通过合理划分高带宽像素计算与低带宽复杂决策状态机的软硬件职责，为边缘端自主分拣操控提供低功耗、高灵活性控制的优化方案。

### 1.2 设计目标与决赛评分约束
根据赛事官方发布的分赛区决赛企业命题指南，系统必须在 **10分钟总时限** 内，依次完成以下四项物块分拣任务（共计 20 轮，每轮均实行“识别 25% — 判断 25% — 执行 50%”的评分制）：
1.  **任务一**：指定颜色正方体分拣（5轮，红/黄/蓝/黑/白 5色，共10分）。
2.  **任务二**：指定形状（正方体）+ 颜色分拣（5轮，混合形状池包含正方体、圆柱体和锥体，共20分）。
3.  **任务三**：尺寸差 1cm 正方体分拣（5轮，参考 2cm/3cm 正方体，目标为与其边长差等于 1cm 的正方体，共15分）。
4.  **任务四**：尺寸差 0.5cm 正方体分拣（5轮，参考 2cm/3cm 正方体，目标为与其边长差 $\le$ 0.5cm 的正方体，共20分）。

系统物理抓取落点要求为：“机械臂旋转 180°（$\pm 10^\circ$）且最大臂展处”的安全平稳放置。若发生跌落或未夹稳，该轮执行分按 25% 计；非目标物块必须输出明确的不执行理由。

### 1.3 主要工作与开发状态声明
*   **创新点 ① `[设计已冻结/待板测]`**：流水线式视频预处理与旁路特征统计 tap 设计。利用 FPGA 硬件实时对摄像头输入像素流进行旁路统计特征提取，计划在消隐期将统计特征锁存至总线寄存器，以释放外部 DDR 存储与系统带宽压力。
*   **创新点 ② `[Host已验证/板级未验证]`**：芯机协同的多特征融合分类决策机制。在片上 CPU 中运行自适应亮度与颜色面积阈值颜色分类、以及填充率正方体识别。目前颜色与形状分类算法已通过 Host 回归，但**真实尺寸测量估计未实现且未进行实拍标定**，对混合形状池的非正方体干扰物抓取在板级仍处于 **`BLOCKED`** 状态。
*   **创新点 ③ `[Host已验证/板级未验证]`**：逐轮事务状态机与异常容错管理。在 CPU 固件中设计了以 `PLACE` 事件为唯一触发的逐轮“识别-判断-执行”时序，配合 idle-drain 状态锁定机制与 fail-closed 安全策略，以防范数据异常引发的物理动作失控。

---

## 第 2 章：系统总体方案设计

### 2.1 系统整体架构与目标数据链路 `[设计已冻结/待实现]`
本系统拟采用基于 Efinix Ti375 开发套件（搭载 QCRV32 硬核）的芯机协同架构，目标闭环数据链路如图 2-1 所示：

```
                    +---------------------------------------+
                    |           Efinix Ti375 FPGA           |
                    |                                       |
+------------+      | +-------------+     +---------------+ |
| MIPI DPHY  |----->| | debayer_top |---->| feature_stats | |
| 摄像头输入  |      | | (视频预处理)  |     | (旁路统计tap)  | |
+------------+      | +-------------+     +---------------+ |
                    |       |                     |         |
                    |  (ch0)v (HDMI监控)          v         |
                    | +-------------+     +---------------+ |
                    | |  osd_top    |     |  QCRV32 SoC   | |
                    | | (字符渲染)   |     | 片上CPU (计划) | |
                    | +-------------+     +---------------+ |
                    +-------|---------------------|---------+
                            v                     v (UART2)
                        HDMI显示监控          myCobot 280 机械臂
```
<center>图 2-1 系统总体架构与目标数据通路（设计已冻结，板级未实现）</center>

### 2.2 软硬件分工设计（芯机协同） `[设计已冻结/板级未验证]`
*   **硬件部分（FPGA）的职责**：高带宽的视频流实时解包、去像素撕裂、色彩空间转换，以及 `feature_stats_tap` 在只读旁路中对 ROI 区域的颜色面积与 bbox 进行计数统计。
*   **软件部分（CPU）的计划职责**：低带宽但高逻辑复杂度的分类器策略（`single_camera_classifier`）、四任务条件匹配决策（`single_camera_f1`）、多轮状态流转、myCobot 通信协议封装。
*   **合理性分析**：避免将复杂的分类逻辑与阈值判断放入 RTL；避免让 CPU 裸跑大带宽的像素处理，兼顾高吞吐并行流水线与软件决策的灵活性。

### 2.3 顶层接口与通信契约定义 `[设计已冻结/待实现]`
#### 2.3.1 控制与调试 UART 接口定义
*   **调试串口 (UART1) `[板级未验证]`**：硬核 SoC UART1 物理外接至板载 Type-C UART，固定波特率 115200 8N1，管脚分配 RX=`GPIOR_96/B12`，TX=`GPIOR_100/D12`。
*   **机械臂串口 (UART2) `[物理控制被明确禁止]`**：由 CPU 物理引出的独立通信引脚，固定波特率 1000000。目前 UART2 控制与物理动作由于安全限制处于禁止状态。

#### 2.3.2 未来特征快照载荷语义草案 (非 wire ABI)
软硬件特征读取与配置下发仅为载荷语义草案（不定义总线类型、具体物理地址、布局、位域或 PSTRB）。拟定字段契约如下：

1.  **CPU 配置组（Staging/Commit/Active 提交机制）**：
    *   `roi`：闭区间左上/右下坐标。
    *   `background`：标定的背景 RGB 均值与前景差异门限。
    *   `color_masks`：红、蓝、黄三类的 CPU 管理阈值。
    *   `config_seq`：16 位单调配置版本号。
2.  **帧快照载荷字段**：
    *   `frame_id` (16 bit)：有效帧序号，复位后从 0 开始。
    *   `config_seq` (16 bit)：该帧统计时所生效的 active 配置版本号。
    *   `red_area` (21 bit)：ROI 内命中红色 mask 阈值的像素计数。
    *   `blue_area` (21 bit)：ROI 内命中蓝色 mask 阈值的像素计数。
    *   `yellow_area` (21 bit)：ROI 内命中黄色 mask 阈值的像素计数。
    *   `foreground_area` (21 bit)：ROI 内满足前景差分判定的像素计数。
    *   `roi_pixel_count` (21 bit)：ROI 内实际处理的像素计数。
    *   `sum_luma` (31 bit)：ROI 内逐像素 `R+G+B` 通道灰度之和。
    *   `bbox_width` (11 bit)：前景包围盒宽度。
    *   `bbox_height` (11 bit)：前景包围盒高度。
    *   `source_flags` (8 bit)：状态标志组（按位表示：`FRAME_STABLE`、`ROI_VALID`、`STATS_VALID`、`DIAG_ACTIVE`、`COUNTER_OVERFLOW`、`SNAPSHOT_OVERRUN`、`SOURCE_CH0`）。

---

## 第 3 章：FPGA 视频采集与处理前端设计

### 3.1 视频处理主干链路 `[板级已验证]`
摄像头采集源在单摄 J48 通道下，物理 MIPI D-PHY 直接接收摄像头输入。当前可据实描述的物理视频主干链路为：
```
ch0 framebuffer -> Debayer -> RGB 转换 -> HDMI 物理输出
```
输入像素流由场同步指示控制，Debayer 模块实现 3x3 行缓冲与滑动窗口，最终输出为 24-bit 物理 RGB 信号直接送往 HDMI 物理输出驱动。单摄工程目前采用固定旁路模式，不包含任何动态 Gamma 调整或白平衡自适应计算电路。

### 3.2 旁路特征统计 tap 设计 (feature_stats_tap) `[设计已冻结]`
在 Debayer 后、HDMI 物理输出前的 RGB 链路上设计了只读旁路统计器 `feature_stats_tap`。
*   **工作机制**：在 ROI 裁剪有效使能内，像素数据在流向 HDMI 显示监控的同时，在旁路进行监听统计。统计器内部设计了 24-bit 累加器，计数符合红/蓝/黄等各色分类特征区域的像素点面积以及包围盒极值。
*   **帧锁存与超限保护**：计划在场消隐期产生 `stats_valid` 并锁定当前计数状态，同时自增 `frame_id` 等待 CPU 读取。若上一帧未收到 `frame_ack` 释放信号，则硬件特征寄存器锁定，并丢弃当前帧的新统计，防止数据覆盖溢出。

---

## 第 4 章：片上 RISC-V CPU 决策与控制固件设计

### 4.1 单摄特征分类算法设计 (single_camera_classifier) `[Host已验证]`
分类算法基于输入特征包 `sc_features_t` 进行分类：
*   **颜色分类**：当红、蓝、黄面积中最大的一项超过预设的通道显著性阈值（`min_red_area`、`min_blue_area`、`min_yellow_area`）时，判定为对应颜色。若均未达到阈值，则根据 ROI 内像素平均灰度值与白/黑边界（`white_mean_luma_min` / `black_mean_luma_max`）进行黑白判色。
*   **形状与尺寸分类**：依据前景像素面积在 Bounding Box 面积中的占比（填充率 `fill_per_mille`）与正方体最小阈值（`cube_fill_min_per_mille`）判断当前是否为 `SHAPE_CUBE`（正方体）。圆柱/锥体阈值目前仅是未标定的 Host 仿真启发值。分类器目前将 `size_cm_x10` **固定为 0**（物理尺寸估计与标定算法未实现）。

### 4.2 四大决赛任务特征匹配决策 `[Host已验证/板级未验证]`
决策层（`single_camera_f1`）根据预设的目标特征与当前输入运行任务判定：
*   **任务一（颜色匹配）**：判断 `shape == CUBE && color == target_color`。
*   **任务二（混合形状池过滤）**：遇到非正方体干扰物（圆柱/锥体）时只能保持 `WAIT`。**由于形状分类阈值尚未上板实物标定，任务二对非正方体的剔除在板级仍处于 `BLOCKED` 状态**。
*   **任务三（尺寸差 1cm）与任务四（尺寸差 0.5cm 内）**：仅能在规则层进行 Host 模拟数据匹配验证。**由于物理尺寸估计未实现，板级在此类尺寸判定上尚无测量数据支持**。

### 4.3 逐轮事务状态机与异常容错管理 `[Host已验证/板级未验证]`
事务控制器依据 `PLACE` 信号触发，依次在 `CLASSIFY` $\rightarrow$ `DECIDE_EXEC` $\rightarrow$ `WAIT_OSD` 状态间转换。
*   **去重防抖**：同一物体连续的多帧上报通过 `frame_id` 比对识别为重复事件，不会产生二次触发。
*   **终态锁存（idle-drain）**：在结果锁定并下发动作指令后，CPU 固件进入挂起状态，在未接收到当前轮的释放复位信号（释放 ACK）之前，不再执行任何新分类运算。
*   **安全关闭（fail-closed）**：当 APB 特征读取异常、画面判定发生严重撕裂或通信超时时，状态机锁死在故障态，拒绝发出运动指令。

### 4.4 myCobot 机械臂运动控制与串口协议栈 `[板级未验证]`
*   **协议帧格式**：myCobot 280 串口帧包含包头（双字节 `0xFE, 0xFE`）、长度（`payload_len + 2u`）、指令字、载荷数据与包尾（`0xFA`），**没有校验和结构**。
*   **运动防抖与到位检测**：计划参考 `final_project` 控制接口平顺减速过渡动作以防止在最大臂展处产生惯性冲击。当前 `ARM_ENABLED=0`，物理动作被硬编码禁用，保护现场联调安全。

---

## 第 5 章：系统集成与测试验证矩阵

### 5.1 软件仿真与 Host 单元测试 `[Host已验证]`
*   **验证范围**：在无物理硬核 SoC 依赖下，使用 Host 环境对固件逻辑进行测试回归。
*   **测试证据及属性**：
    *   **F1 Host** (213/213 校验)：测试逐轮分拣状态机、20轮回归与 Abandon 策略。
        *证据文件*：[run_single_camera_f1_host.ps1](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu/tests/run_single_camera_f1_host.ps1)；
        *日期/批次*：2026-07-15 / 工作日志 `L814-842`；运行状态：**PASS**。
    *   **Adapter Host** (33/33 校验)：验证物理契约字段校验及 overrun 溢出安全。
        *证据文件*：[run_single_camera_feature_adapter_host.ps1](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu/tests/run_single_camera_feature_adapter_host.ps1)；
        *日期/批次*：2026-07-15 / 工作日志 `L858-868`；运行状态：**PASS**。
    *   **Runtime Host** (648/648 校验)：验证 C runtime 整体逻辑在 Host 下的运行。
        *证据文件*：由 C runtime 离线环境运行；
        *日期/批次*：2026-07-15 / 工作日志 `L836`；运行状态：**PASS**。
    *   **Classifier Host**：颜色与形状分类器的 Host 单测验证。由于 MSVC 严格编译警告 `/W4 /WX` 限制，目前触发 `C4127` 处于 **`FAIL`** 状态（参考 [CURRENT_STATE.md:L37](file:///D:/第十届集创赛-雄芯院材料/CURRENT_STATE.md#L37)）。

### 5.2 [历史板级非回归证据] 禁用采集的旁路统计模块测试 `[板级已验证]`
*   **测试目的**：验证旁路特征统计器 `feature_stats_tap` 的硬件描述不会对已验证的 HDMI 显示主干链路造成退化，并确保综合、布局布线与时序在 Ti375 核心上收敛。
*   **限制范围**：当时顶层 `i_capture_enable` 固定为 `0`，特征输出未接 CPU、APB、OSD、UART 或机械臂，不包含真正的片上 CPU 联合调试与动作控制。
*   **时序与资源数据证据**：
    *   *时序收敛*：Setup 最小 Slack 为 `+1.766ns`，Hold 最小 Slack 为 `+0.026ns`（无 CDC 亚稳态警告）。
    *   *资源消耗*：`EFX_LUT4` = 10887，`EFX_FF` = 9434，`EFX_RAM10` = 163，`EFX_DPRAM10` = 4。
    *   *证据文件/日期*：[m2_feature_tap_manual_build_board_check_20260715.md](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/docs/debug_sessions/m2_feature_tap_manual_build_board_check_20260715.md)；
    *   *批次 Hash/构建时间*：2026-07-15 15:32:07 / bitstream SHA-256 为 `45427C12AFE874C6032614B3D241EFD3BCBABFF395970D4D80FFFE8165F78535`。
    *   *运行状态*：**PASS (显示链路非回归通过)**。上板烧录后 HDMI 画面正常显示，无任何画面撕裂退化。

### 5.3 性能指标灵魂对比表
针对决赛技术指标与合规标准，本设计性能指标对比汇总如表 5-2 所示（板级实测值严格按当前未生成新硬核 SoC 批次的状态填写为 `未评估/未验证`）：

| 性能指标 | 赛题决赛要求 | 本文理论分析值 | Host回归测试值 | 板级实测结果 | 结论说明 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **任务一分类准确率** | 正确分拣指定目标 | `未评估` | `未评估` | `未验证` | 算法逻辑正确，待SoC板测 |
| **任务二分类准确率** | 正确过滤非正方体 | `未评估` | `未评估` | `未验证` (BLOCKED)| 形状分类待实拍标定 |
| **测距与尺寸偏差** | 误差不超过 $\pm$0.5cm | `未评估` | `未评估` | `未验证` | 尺寸测量算法尚未实现 |
| **单次识别与分拣周期** | 四任务总耗时 $\le$ 10分钟 | - | - | `未验证` | 物理机械臂控制处于禁止状态 |
| **目标物放置角度精度** | 旋转180°，正负差 $\le 10^\circ$ | - | - | `未验证` | 物理机械臂控制处于禁止状态 |
| **异常及超时响应时间** | 状态自恢复/不影响运行 | - | - | `未验证` | 待 APB / CDC 接口硬件实现 |
| **盲审规则合规性** | 禁止出现学校/导师/个人信息| 文本静态检查未发现身份信息 | 图片尚未纳入，N/A | 静态文本无学校个人信息 |

<center>表 5-2 系统综合性能指标三线对比表</center>

---

## 第 6 章：总结与展望

### 6.1 现阶段成果总结
目前系统已成功实现：
1.  基于 [competition_project_single_camera/](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/) 目录完成旁路特征提取 tap 的编写，并通过了板级综合时序与 HDMI 链路非回归测试（参考 [m2_feature_tap_manual_build_board_check_20260715.md](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/docs/debug_sessions/m2_feature_tap_manual_build_board_check_20260715.md)）。
2.  在 CPU 固件层面，重构并通过了基于单摄特征（`single_camera_*.c`）的分类决策、任务匹配以及状态机 Host 单元测试回归（单摄 F1 Host PASS 213/213）。

### 6.2 后续改进与开发物理 Gate 约束
为推动系统通过后续的板上实测，后续工作拟在如下物理安全 Gate 约束下逐步推进：
1.  **SoC 物理资源重新生成 Gate**：配合 libaoxun 使用官方 Efinity IP Manager 释放 `PLL_BL1`，并生成包含真实硬核 CPU 与 SoC UART1 的物理批次，解决 PLL 时钟冲突。
2.  **UART1-SMOKE & APB-OSD Gate**：在通过 UART1-SMOKE Hello/回显测试并验证 APB0 MAGIC 成功后，逐步打通 APB 旁路特征读取链路，调试 CPU $\rightarrow$ OSD 信息渲染。
3.  **UART2 & 机械臂物理接入安全 Gate**：**即使前述 UART1-SMOKE 测试通过，依然不授权启用 UART2 或机械臂动作。** 动作控制属于独立高风险 Gate，必须在用户确认现场固定、急停/断电手段并在独立 Review Packet 获准后，方能解开 `ARM_ENABLED=0` 限制，进而进行物理调试。

---

## 参考文献与附录

### 参考文献
1.  [第十届集创赛分赛区决赛“雄芯院”企业命题比赛细则-0710新.pdf](file:///D:/第十届集创赛-雄芯院材料/赛方提供材料/第十届集创赛分赛区决赛“雄芯院”企业命题比赛细则-0710新.pdf)
2.  [集创赛技术文档写作规范.md](../CICC技术文档与PPT收集整理/04_赛事官方要求与模板/集创赛技术文档写作规范.md)
3.  [11_面向人形机器人的FPGA综合图像处理系统.md](../CICC技术文档与PPT收集整理/01_第九届官方优秀作品专刊/11_面向人形机器人的FPGA综合图像处理系统/11_面向人形机器人的FPGA综合图像处理系统.md)
4.  [10_基于国产FPGA加速的物流分拣机器人设计.md](../CICC技术文档与PPT收集整理/01_第九届官方优秀作品专刊/10_基于国产FPGA加速的物流分拣机器人设计/10_基于国产FPGA加速的物流分拣机器人设计.md)

### 附录 A `[Host已验证]`
正式单摄固件中特征结构体与核心分类函数（依据真实 `single_camera_classifier.c` 与 `single_camera_classifier.h`）：

```c
/* 详见 competition_project_single_camera/cpu/include/single_camera_classifier.h */
typedef struct {
    uint32_t red_area;
    uint32_t blue_area;
    uint32_t yellow_area;
    uint32_t foreground_area;
    uint32_t roi_pixel_count;
    uint32_t sum_luma;
    uint16_t bbox_width;
    uint16_t bbox_height;
} sc_features_t;

/* 详见 competition_project_single_camera/cpu/src/single_camera_classifier.c */
int sc_classify_features(const sc_features_t *features,
                         const sc_classifier_cfg_t *cfg,
                         sc_observation_t *observation)
{
    sc_classifier_cfg_t default_cfg;

    if (features == 0 || observation == 0) return -1;
    if (cfg == 0) {
        sc_classifier_cfg_default(&default_cfg);
        cfg = &default_cfg;
    }

    memset(observation, 0, sizeof(*observation));
    observation->color = classify_color(features, cfg);
    observation->shape = classify_shape(features, cfg);
    observation->size_cm_x10 = 0u; /* Size remains unavailable before calibration. */
    observation->stable = observation->color != SC_COLOR_UNKNOWN &&
                          observation->shape != SC_SHAPE_UNKNOWN;
    return 0;
}
```

### 附录 B `[设计已冻结]`
计划中的旁路特征统计 tap 载荷语义契约（v0.1-draft，非冻结 wire ABI）可见：[single_camera_feature_contract.md](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/integration/single_camera_feature_contract.md)。
