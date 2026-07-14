# 单摄分赛区决赛主方案

> 版本：`v1.0-approved`
>
> 日期：2026-07-14
>
> 状态：**用户已批准；M0源码复制/身份审计完成，人工构建与板级复现待执行**
>
> 候选工程：仓库内 `competition_project_single_camera/`
>
> 已跑通参考与手工构建/烧录镜像：`<local-demo-mirror>`
>
> 主线关系：本文件是候选路线的局部计划，服从仓库根 `AGENTS.md`、`CURRENT_STATE.md` 与 `final_project/docs/technical_plans/competition_score_maximization_execution_plan_20260712.md`；M0 板级复现前不替代 `final_project/`。
>
> 工具：Efinity `2025.2.288.4.15`

## 1. 决策摘要

新路线废弃 `final_project` 的双摄视频工程，不废弃已经形成的 CPU、机械臂、Host 测试和安全设计。新视频基线改为已在 J48/ch0 上稳定显示真实摄像头画面的官方 `TJ375N529_SC431HAI2LCD_Demo_V3`。

最终系统只使用一个斜上方摄像头、一个固定投放点和一个可配置识别框。保留 HDMI 到电脑的实时画面，删除第二路摄像头与 MIPI DSI；在不破坏 ch0 画面链的前提下，旁路增加基础统计特征、硬核 QCRV32、APB/CDC 和 OSD。

拿分顺序固定为：

```text
M0 已跑通Demo复现
→ M1 单摄/HDMI裁剪且画面不回归
→ M2 QCRV32 + UART + APB MAGIC
→ M3 任务一五色小闭环（UART）
→ M4 任务一OSD闭环
→ M5 任务二 CUBE/NON_CUBE
→ M6 任务三/四尺寸关系
→ F1 四任务20轮、OSD、ARM_DISABLED
→ F2 板上CPU控制myCobot
```

功能先跑通，再做相机角度、高度、背景、补光、ROI和阈值精度优化。任何未完成标定的字段必须明确显示 `UNAVAILABLE`，不得输出伪精度。

## 2. 权威来源与效力

优先级如下：

1. 用户当前指令。
2. `final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md`。
3. `AGENTS.md` 的架构、安全、审查和 Git 硬边界。
4. 根目录 `CURRENT_STATE.md`。
5. 本方案。
6. 新工程真实源码、同次生成的工程 XML、日志和上板现象；它们是工程事实最终来源。
7. `final_project` 和历史 Demo 文档只作迁移来源与经验库。

本方案获批不等于任何 Gate 已通过。M0 通过前，新工程只是候选主线。

## 3. 官方比赛闭环

系统最终须覆盖四任务各5轮、共20轮，并在10分钟内完成：

| 任务 | 必须识别/判断的核心语义 | 开发优先级 |
|---|---|---:|
| 任务一 | 指定颜色正方体，五色目标 | 1 |
| 任务二 | 指定颜色正方体，拒绝圆柱/锥体 | 2 |
| 任务三 | 正方体与20/30mm参考物边长差等于10mm | 3 |
| 任务四 | 正方体与20/30mm目标物边长差不超过5mm | 3 |

每轮必须输出：实际颜色、形状、尺寸或不可用状态，目标/非目标判断，执行/不执行及理由。目标机械臂响应必须唯一；非目标必须明确 `SKIP`。任务三评分比例存在正文和表格不一致，软件仍完整输出识别、判断和执行三环节，不依赖比例取巧。

## 4. 系统架构

```text
SC431HAI @ J48/ch0
  → 官方Demo既有MIPI CSI接收/RAW链路
  → DDR framebuffer
  → Debayer/Gamma/白平衡（以真实Demo为准）
  ├→ 既有HDMI主链 → OSD叠加 → 电脑
  └→ 非侵入旁路tap
       → 可配置ROI
       → 背景差/颜色面积/前景面积/RGB-Y汇总/bbox/轮廓统计
       → 稳定帧快照 + frame_id
       → CDC/APB
       → QCRV32 CPU
            ├→ classifier / param_table
            ├→ task_matcher
            ├→ 唯一 round_controller
            ├→ UART1菜单与详细日志
            ├→ 结果回写 → FPGA OSD
            └→ F2: UART2 → myCobot
```

### 4.1 FPGA职责

- 保持 J48/ch0 到 HDMI 的已跑通主链。
- 单路 CSI、必要 DDR/framebuffer、Debayer、Gamma/白平衡和 HDMI。
- 可配置识别框及与显示框共用的 active ROI。
- 背景差、像素统计、bbox、中心和必要的轻量轮廓基础统计。
- 快照/配置/结果的 APB 与 CDC。
- `PLACE`、`ABANDON`按键同步、消抖、16位事件序号、事件保持和ACK清除。
- ASCII OSD像素渲染。

FPGA不做最终颜色、形状、尺寸分类，不做四任务关系判断，不做机械臂状态机。

### 4.2 CPU职责

- 背景标定调度、特征读取和多帧稳定判定。
- 五色分类。
- `CUBE/NON_CUBE`，后续扩展圆柱/锥体细分。
- 20/25/30mm离散尺寸标定与任务三/四关系判断。
- 目标配置、五轮计数、结果/理由锁存和计时。
- F1机械臂统一禁用；F2接入协议、点位、安全互锁、超时和有限重试。

### 4.3 单一正式契约

新工程只保留一套运行时契约：

```text
TARGET_CFG
OPERATOR_EVENT
EVENT_ACK
FEATURE_SNAPSHOT
ROUND_RESULT
round_controller
```

- `task_matcher`只根据稳定识别结果与`TARGET_CFG`产生目标判断和理由。
- `round_controller`独占轮次状态、`PLACE/ABANDON`消费、结果锁存、5轮计数和唯一机械臂请求。
- `competition_contract`中有价值的16位序号、ACK、幂等和提交语义吸收到上述正式接口。
- 不迁移`competition_round_transaction`作为第二套运行状态机。
- 不保留8位事件兼容模式。

## 5. 已确认的交互

### 5.1 第一版按键

按键最终选择由M0/M1审计Demo的GPIO/periphery占用后冻结并写入`integration/io_pin_map.md`。候选优先级为3.3V轻触键：

| 优先功能 | 候选按键 | FPGA管脚 | 电平 | 当前状态 |
|---|---|---|---|---|
| `PLACE` | SW3 | U19 | 3.3V，低有效 | 候选，待Demo占用审计 |
| `ABANDON` | SW4 | V19 | 3.3V，低有效 | 候选，待Demo占用审计 |
| 备用 | SW2 | U17 | 3.3V，低有效 | 暂不使用 |
| 备用 | SW6 | V20 | 3.3V，低有效 | 暂不使用 |

`SW1/F9 CRESET_N`是FPGA重配置按键，禁止用于业务逻辑。1.8V的SW11/SW12/SW14/SW13首版不使用。32位拨码首版全部后延。

按键链固定为：

```text
低有效物理按键
→ 双触发器同步
→ 消抖
→ 下降沿生成事件
→ 16位event_seq并保持valid
→ CPU消费
→ CPU回写ACK后清除
```

### 5.2 小闭环轮次

第一版不做`REMOVE`、自动空场互锁、长按复用或复杂按键菜单：

```text
WAIT_PLACE
→ 操作员已清理并放置物体
→ 按PLACE
→ 固定时间窗多帧采样
→ 锁存识别和判断一次
→ TARGET: F1显示ARM_DISABLED
   NON_TARGET: 显示SKIP和理由
→ 自动回WAIT_PLACE，但保持上一轮结果显示
→ 操作员清理、放置下一物体，再按PLACE
```

- `PLACE`只在`WAIT_PLACE`接受；识别中和结果锁存过程中的重复事件拒绝。
- 下一次有效`PLACE`清除旧显示并开始新一轮。
- `ABANDON`独立结束识别失败/人工放弃轮；开发模式可不计轮，比赛模式计入5轮。
- 每任务第5轮完成后进入`TASK_DONE`，后续`PLACE`无效；新任务`APPLY`后轮数归零。
- F1目标轮允许操作员手动移走，且不自动重发动作。

### 5.3 UART开发菜单

开发阶段通过Type-C UART1配置，比赛阶段只替换输入适配器，不改内部契约：

```text
target red|blue|yellow|black|white
apply
status
place
abandon
calibrate
```

串口`place/abandon`只用于开发和无按键诊断。最终比赛目标配置迁移到拨码/按键，OSD为主显示，UART保留日志镜像。

## 6. 视频保护策略

“不破坏摄像头画面”是每个FPGA Gate的硬条件，不是最后验收项。

1. M0只复制和复现，不重排目录、不裁剪模块。
2. M1先禁用、后删除ch1；每一步单独构建和上板。
3. 再先禁用、后删除DSI TX；每一步单独构建和上板。
4. 特征提取使用旁路tap，不反压、不控制、不替换HDMI数据。
5. tap插入点在审计真实Demo像素格式后决定；优先选稳定RGB流。若白平衡影响颜色统计，可保留前后低成本探针后用实测选择，但不得改变主链。
6. OSD采用可编译关闭的末端叠加；异常时可回退纯画面基线。
7. 任何画面回归立即停止扩功能并退回上一Git checkpoint。

每个视频Gate的板级标准：冷启动3次均有实时画面，连续运行至少10分钟，无fallback纯色、明显花屏、冻结或持续条纹，分辨率/帧率与基线一致。

## 7. M0前的D盘事实审计

用户确认历史 `<local-demo-mirror>/outflow/mem_test.bit` 可稳定显示 J48/ch0 真实画面；该证据只绑定历史镜像，不能证明后续白平衡增量对应的当前源码。当前只读清点同时发现本地镜像存在：

- `src/`、`ip/`、`mem_test.xml`、`mem_test.peri.xml`、`constrain.sdc`；
- `src/top.v`已启用`FRAME_BUFFER`与`HDMI_OUT_EN`，源码树同时含`mipi_dsi/`和`dvi_tx/hdmi_top.v`，支持按独立Gate裁剪DSI并保留HDMI；
- `outflow_ch0_codex_20260713/`、`outflow_ch0_full_codex_20260713/`；
- 多个`work_*`目录和`.lock`；
- 源码子目录内还混有`.bak`、ModelSim `work/`、`.wlf`、大型波形、文档和压缩包；
- 当前bitstream时间戳为2026-07-13。

因此M0必须先确认哪次日志、XML、源码和bitstream构成可复现基线。不得把整个D盘目录称为未经修改的干净官方原版，也不得删除这些产物。

M0需保存：

- 关键源码、XML、约束和bitstream的SHA-256；
- Efinity版本、构建时间、实际命令/GUI步骤；
- bitstream与构建日志的对应关系；
- J48/ch0、HDMI接法和用户上板现象；
- 排除复制的`work_*`、临时数据库、锁和普通缓存清单。

M0采用白名单复制：工程实际引用的RTL、IP生成配置、初始化文件、XML和约束逐项纳入；`.bak`、仿真`work/`、`.wlf`、临时波形、锁、普通构建缓存和重复压缩包默认排除。若工程XML实际引用其中某项，须先记录用途再纳入，不能按扩展名盲删。

历史`mem_test.bit`只作回退基线。新工程必须由用户重新完整构建、烧录并复现画面，才能通过M0。

## 8. 阶段门

### M0：原样Demo基线复现

实施：

1. 审计D盘身份并生成哈希清单。
2. 复制源码、必要IP生成配置、XML、约束和初始化文件到新工程，保持原相对结构。
3. 不复制普通构建缓存；历史bitstream本体是否入Git按大小和仓库策略审计。
4. 从新工程增量同步回D盘构建镜像。
5. 用户使用Efinity `2025.2.288.4.15`完整构建、PNR、生成bitstream并烧录。

通过条件：新构建与源码可追溯，J48/ch0到HDMI满足3次冷启动和10分钟画面标准，保存日志与bitstream哈希。

### M1：单摄和HDMI裁剪

按四个独立checkpoint推进：

1. ch1顶层禁用，源码/IP仍保留。
2. ch1专属实例和工程项删除。
3. DSI TX禁用，相关生成物保留。
4. DSI资源从工程/periphery正式删除。

每步均需map、PNR、STA、bitstream、上板画面回归和资源差异。禁止一次删除后统一排错。

### M2：QCRV32最小板级闭环

先审计官方RISC-V例程，决定开发期CPU程序采用JTAG下载还是固化；Type-C UART CPU日志当前完全未验证。

最小目标：

```text
CPU启动
→ UART1打印BUILD_ID
→ 从同次soc.h取得APB基址
→ 读取REG_MAGIC
→ 写HEARTBEAT
```

SoC必须由Efinity GUI/IP Manager正式生成：不手改`.peri.xml`，不手写`soc.h`，APB/UART/IRQ/时钟来自同一次生成物。先做资源审计，再逐个增加`JTAG_USER2`、系统PLL、UART1、一个APB从机、GPIO、UART2。

若删除ch1/DSI后仍无合法硬SoC资源，立即停止实施并提交PLL/JTAG/GPIO/APB冲突报告和候选路线，等待用户选择；不改动已跑通画面链规避冲突。

### M3：任务一五色UART小闭环

范围仅限正方体和颜色任务：

- 上电空框1至2秒完成背景标定；未READY时`PLACE`无效。
- 增加可配置识别框，坐标不在方案阶段写死。
- FPGA旁路输出同帧基础统计。
- CPU多帧判定五色；尺寸固定输出`UNAVAILABLE`。
- UART配置目标颜色；物理/串口`PLACE`启动一次；`ABANDON`可结束本轮。
- 目标输出`TARGET + ARM_DISABLED`，非目标输出`NON_TARGET + SKIP + reason`。

调试顺序：红/蓝/黄各至少10次，黑至少10次，白至少10次，最后五色随机至少25轮。先证明功能闭环，再根据真实特征冻结准确率和阈值。

### M4：任务一OSD闭环

FPGA在HDMI末端叠加固定ASCII、识别框和bbox。最小内容：

```text
TASK / ROUND
TARGET
SEEN: COLOR CUBE SIZE_UNAVAILABLE
DECISION
ACTION
REASON
STATE
ROUND_TIME / TOTAL_TIME
```

OSD和UART必须来自同一份`ROUND_RESULT` active快照。OSD不允许半更新，不依赖PC叠字，并保留编译关闭回退开关。

### M5：任务二 CUBE/NON_CUBE

先稳定区分正方体与非正方体，覆盖任务一、二高分路径；随后再把`NON_CUBE`细分为圆柱和锥体。

允许FPGA增加但不分类的基础统计：bbox填充率、上中下宽度摘要、顶部/底部宽度比例、水平/垂直边缘计数等。最终形状分类仍由CPU完成。

### M6：尺寸和任务三/四

相机角度、高度、固定投放点、背景和补光冻结后，使用固定几何条件下的像素尺寸离散分类，不做通用三维测量。

- 标定20/25/30mm三档。
- 任务三判断`abs(observed-reference)==10mm`。
- 任务四判断`abs(observed-target)<=5mm`。
- 标定完成前尺寸始终为`UNAVAILABLE`，任务三/四不得发布GRAB或伪SKIP。

### F1：无机械臂比赛闭环

通过条件：

- 单个正式bitstream包含J48/ch0视频、QCRV32、APB/CDC和OSD。
- 五色、`CUBE/NON_CUBE`、三尺寸和四任务判断均有真实物体证据。
- 任务一、二、三、四各5轮，`PLACE`推进，无重复识别和机械臂请求。
- OSD/UART一致，目标显示`ARM_DISABLED`，非目标正确`SKIP`并显示理由。
- 20轮不超过10分钟，内部目标8分30秒。
- `ARM_ENABLED=0`为编译和运行时双重门。

### F2：机械臂条件升级

仅在F1稳定后进入。迁移既有`mycobot_protocol`、transport、`arm_controller`和安全测试，但必须适配新SoC/UART2真源与唯一`round_controller`。

放行前必须完成：UART2/J52正式periphery和`soc.h`、1Mbps 8N1无臂100/100帧、电平/线序/共地、急停/断电、底座稳定、180度正负10度最大臂展新点位、低速带载与无跌落证据。未全部通过不得发送真实动作。

## 9. FPGA-CPU接口增量顺序

严格按以下顺序，每级单独验证：

```text
REG_MAGIC / HEARTBEAT
→ PLACE / ABANDON + EVENT_ACK
→ ROI配置
→ FEATURE_SNAPSHOT + frame_id ACK
→ ROUND_RESULT回写
→ OSD active快照
→ UART2/myCobot
```

接口共同规则：

- 正式基址只来自同次生成的`soc.h`。
- 配置采用staging + commit + active，不逐位裸跨CDC。
- 特征快照在`valid`期间保持稳定；CPU按匹配`frame_id` ACK。
- CPU结果整组提交，OSD只消费active快照。
- 事件采用16位序号和显式`ACCEPTED/REJECTED` ACK。
- 地址、时钟、复位、PSTRB/PREADY/PSLVERROR语义未冻结前不写正式MMIO适配。

## 10. CPU与机械臂迁移矩阵

| 旧模块/内容 | 新工程处理 | 必须适配的边界 |
|---|---|---|
| `vision_classifier` | 迁移并重标定 | 单摄快照、真实背景/RGB-Y统计；删除双摄假设 |
| `param_table` | 迁移框架 | 新ROI和真实数据；首版运行时UART调参，冻结后固化 |
| `task_matcher` | 迁移四任务规则 | 单一`TARGET_CFG`和理由码 |
| `round_controller` | 作为唯一正式状态机迁移 | 只保留`PLACE/ABANDON`小闭环，16位事件/ACK，5轮计数 |
| `competition_contract` | 只吸收语义和测试 | 不作为第二套运行控制器 |
| `competition_round_transaction` | 不迁移到固件 | 有价值测试重写到新测试集 |
| `board_io` | 接口重写/适配 | 删除旧候选地址、双摄寄存器和旧2位`TARGET_SEL` |
| `main.c` | 重新集成 | 新`soc.h`、单摄、唯一状态机、F1安全门 |
| `mycobot_protocol` | 迁移 | 协议帧和1Mbps保持 |
| transport/`arm_controller` | F1编译、F2接入 | 新UART2 MMIO/IRQ、点位、安全门和超时 |
| 旧视频RTL/XML/SDC | 不迁移 | 新Demo是唯一视频基线 |

迁移不是文件复制即完成。每个模块必须有接口差异、保留/删除项、Host测试和板级状态记录。

## 11. UART与OSD可观测性

Checkpoint A的UART至少输出：

```text
BUILD_ID
TASK / ROUND
TARGET_COLOR
FRAME_ID
RAW_FEATURES          # 仅debug模式
RECOG_COLOR
RECOG_SHAPE
SIZE UNAVAILABLE|20|25|30
DECISION
ACTION
REASON
STATE
ROUND_TIME / TOTAL_TIME
```

Checkpoint B的OSD显示评分所需最小固定字段。UART和OSD由同一语义枚举生成，不各自拼一套判断文本。

## 12. 三成员协作

| 角色 | 独占修改区 | 主要交付 |
|---|---|---|
| FPGA/视频/SoC | Demo视频源码、XML/periphery、约束、特征RTL、APB/CDC、OSD | 每级构建日志、资源/时序、bitstream身份、画面回归、Review Packet |
| CPU/识别/状态机 | `cpu/`与Host测试 | 单一契约、分类/任务/轮次测试、RISC-V构建、UART日志 |
| 机械臂/治具/标定 | 机械参数、标定数据、治具与机械测试 | 固定点、相机几何、背景/补光、混淆矩阵、F2安全证据 |

协作规则：

- `top.v`、`.peri.xml`、`mem_test.xml`、`constrain.sdc`只由FPGA成员修改。
- `integration/`契约变更必须三方确认。
- 三人使用独立`dev/用户名`分支，通过Review Packet和PR合入。
- 其他成员不得直接修改D盘镜像。
- 每个Gate只增加一个主要变量；合流后先回归再进入下一Gate。

## 13. D盘同步与回退

M0 的新构建、匹配 bitstream、烧录、3 次冷启动和 10 分钟画面复现全部通过，并完成主线文档升格后，仓库新工程才可成为正式源码；此前它保持隔离候选，本地目录仅作构建/烧录镜像。

每次同步固定流程：

1. 检查Git状态和D盘差异。
2. 记录源提交、同步文件、目标路径和同步前哈希。
3. 保存上一可工作checkpoint及bitstream哈希，不覆盖旧`outflow/mem_test.bit`作为唯一回退物。
4. 只增量同步批准文件；不整树覆盖、不删除本地构建产物。
5. 用户手动综合、PNR、烧录并反馈原始结果。
6. 结果写入新工程`docs/debug_sessions/`和`CURRENT_STATE.md`。
7. 通过才提交下一阶段；失败回到上一checkpoint。

若用户必须手改D盘，先声明修改文件，后续先反向差分和同步，禁止让D盘形成不可追溯分叉。

## 14. 验证矩阵

| 层级 | 最低证据 | 不能外推为 |
|---|---|---|
| Host | 从当前源码重建的断言、边界和20轮模拟 | RISC-V、APB、板级或机械臂 |
| RISC-V build | 同次`soc.h`下编译/链接产物 | CPU已启动 |
| FPGA map | 模块展开、资源和全部warning | PNR、时序、bitstream |
| PNR/STA | Setup/Hold、CDC和可重复bitstream | 真实画面或识别 |
| 板级视频 | 匹配bitstream、3次冷启动、10分钟画面 | 特征正确或CPU闭环 |
| APB | CPU UART、MAGIC/heartbeat、总线证据 | 特征/OSD正确 |
| 识别 | 真实物体原始特征、标签、结果 | 四任务整场 |
| F1 | 20轮、OSD/UART、计时、ARM_DISABLED | 机械臂动作 |
| F2 | UART/电气/点位/动作/急停/录像 | 未测试场景的安全性 |

所有Efinity Gate记录版本、开发板、资源、Setup/Hold、CDC与关键warning。warning不得笼统标记为可忽略。

## 15. 强制停止条件

- 摄像头画面回归：停止扩功能，退回上一视频Gate。
- 删除ch1/DSI后仍无合法QCRV32资源：提交冲突报告，等待用户选择。
- 没有同次SoC生成的`soc.h`：不实现正式MMIO。
- PNR/STA失败：不生成或烧录“正式版本”。
- UART1 CPU日志未证实：不外推CPU启动。
- ROI/特征未以真实帧验证：不冻结分类阈值。
- 尺寸未标定：任务三/四保持`SIZE_UNAVAILABLE`。
- F1未通过：`ARM_ENABLED=0`。
- UART2、电平、线序、急停、结构和点位门未通过：不进入F2动作。

## 16. 压缩时间表

既定时间仍为7月17日基础冻结、7月22日比赛。日期紧张时不跳Gate，只缩范围：

| 时间 | 目标 | 失败动作 |
|---|---|---|
| 7月14日 | 方案审核；获批后完成M0身份审计与复制准备 | 不改RTL |
| 7月15日上午 | M0新构建和上板复现 | 只修基线可追溯性/画面 |
| 7月15日下午 | M1逐级裁剪和SoC资源审计 | 画面回归立即回退 |
| 7月16日上午 | M2 CPU UART + MAGIC/heartbeat | SoC冲突即提交决策报告 |
| 7月16日下午 | M3红蓝黄功能闭环，随后黑白 | 保持ARM_DISABLED，先UART |
| 7月17日 | 冻结当时最高可验证版本和完整回退包 | 不新增架构 |
| 7月18至21日 | 在冻结基线上补OSD、形状、尺寸、整场与可选F2 | 每项仍按Gate放行 |

该时间表表达优先级，不把未完成阶段描述为已完成。若M2未关闭，后续CPU/OSD只能继续做Host开发，不能形成比赛板级版本。

## 17. 审核后首个动作

本候选方案已由用户于 2026-07-14 批准。M0 已完成初始白名单源码复制、源/目标哈希核对、历史构建身份审计和本地镜像首次零差异同步核查；随后 `white_balance.v` 已有受审增量，详见 delta manifest。尚未运行修改后源码的新构建或板级复现。

批准后的第一动作仅为M0：

1. [x] 运行交接健康检查和Git检查。
2. [x] 对D盘关键源、XML、约束、日志和bitstream建哈希清单。
3. [x] 识别M0应复制和排除的文件。
4. [x] 复制到新工程，保持目录结构。
5. [x] 形成M0 Review Packet和D盘首次同步清单。
6. [ ] 用户手动完整构建、烧录并反馈。

## 18. 当前NOT VERIFIED

- 新工程已复制75个白名单文件，但尚未用仓库副本触发新的完整构建。
- D盘当前bitstream与源码/日志已完成身份审计；它仍只是历史可运行基线，不替代新构建。
- 新工程map、PNR、STA、bitstream和上板均未执行。
- ch1/DSI尚未裁剪，释放资源未知。
- 新工程硬核QCRV32资源、生成`soc.h`、UART1启动和APB均未验证。
- 按键候选尚未与D盘periphery占用交叉核查。
- 新单摄特征、背景标定、五色、形状、尺寸和OSD均未实现。
- F1四任务20轮和F2机械臂均未验证。
