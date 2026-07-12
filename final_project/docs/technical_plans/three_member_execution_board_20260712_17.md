# 三成员决赛保底冲刺执行板（7月12—17日）

> 上位方案：`competition_score_maximization_execution_plan_20260712.md`（v1.2-main）
>
> 目标：7月17日冻结F1最低保底；条件满足时升级F2机械臂执行
>
> 原则：先交可验证证据，再扩大集成；Host/合成源通过不等于真实板级通过

## 1. 三人唯一主责

| 成员 | 每日投入 | 唯一主责 | 不得被其他任务挤占的交付 |
|---|---:|---|---|
| A：FPGA/SoC | 6—8h | 合成源收口、摄像头恢复、QCRV32并入视频工程、APB/CDC/OSD硬件通道 | 7月14日晚前：视频工程内CPU Hello + APB MAGIC |
| B：CPU负责人 | 1—2h，其他成员协助编码 | 决赛契约定版、CPU代码审查、四任务/状态机验收；将纯C实现拆给支援成员 | 7月14日晚前：四任务与round_controller Host/Mock全通过 |
| C：机械臂/现场 | 6—8h | 180°点位、低速带载、治具/现场、板到臂UART物理安全验证 | 7月12日：新点位与5轮记录；7月16日前决定能否开启F2 |

## 2. 文件所有权与冲突红线

### A独占修改/最终合并

```text
final_project/fpga/rtl/top/top.v
final_project/fpga/efinity/mem_test.xml
final_project/fpga/efinity/mem_test.peri.xml
final_project/fpga/efinity/constrain.sdc
SoC/Efinity生成目录与最终soc.h来源
```

- 合成数据源作者先交Review Packet或独立提交，A审核后合入。
- 合成源和SoC集成不得同时在同一份`top.v`上无序开发。
- `mem_test.xml`、`.peri.xml`、SDC、时钟、复位、CDC修改必须经过Codex Gate。

### B独占接口定版/最终审查

```text
final_project/cpu/app/include/task_matcher.h
final_project/cpu/app/src/task_matcher.c
final_project/cpu/app/include/round_controller.h
final_project/cpu/app/src/round_controller.c
final_project/cpu/tests/test_task_matcher.c
final_project/cpu/tests/test_round_controller.c
final_project/cpu/CPU_MODULE_PLAN.txt
```

- 支援成员可以编码，但B负责冻结API、审查diff和接受测试证据。
- 正式`soc.h`到位前，不在C源码写死APB基址。
- Host测试通过前，不把完整状态机塞入`main.c`。

### C独占机械臂点位与实测记录

```text
mycobot_pc_tests/中的新决赛点位/脚本对
mycobot_pc_tests/audit_logs/中的新实测日志
final_project/cpu/app/src/arm_positions.c（进入板控移植阶段后）
机械臂点位、治具、接线与安全检查记录
```

- 不覆盖最后成功的10cm横移基线脚本或旧preset。
- 新建“competition_180”脚本/点位文件；点位不写死进`arm_controller.c`。
- FPGA-to-myCobot端口、电平、共地未确认前，不直接接线动作。

## 3. 成员A任务卡：FPGA/SoC

### 7月12日：合成源收口 + SoC合并准备

按顺序执行：

1. 记录当前视频工程基线：工程路径、HEAD、bitstream、花屏现象、LED状态。
2. 让合成源作者明确：新增RTL文件、Mux插入点、`vs/hs/de/pixel`时序、启用方式和回退开关。
3. 合成源只允许替换CSI后的输入边界，不删除双CSI实例，不改DDR/HDMI/约束。
4. 先完成合成源独立仿真/静态检查和map，再由A决定是否合入。
5. 从赛方RISC-V例程列出SoC IP、生成脚本、顶层端口、时钟/复位、UART、APB槽位和`soc.h`位置。
6. 在无中文、无空格的隔离目录准备“视频工程+空SoC”副本；今天不要求完整APB视觉接口。

今日验收：

- [ ] 合成源有独立文件和明确Mux，不破坏真实CSI路径。
- [ ] 合成源map通过，或形成包含原始错误的Review Packet。
- [ ] SoC合并清单完整，能指出UART、APB、时钟、复位和生成`soc.h`的方法。
- [ ] 没有批量给2,288个所谓I/O分配管脚，没有伪造`.dbg.vdb`。

### 7月13日：新摄像头 + CPU Hello

上午最多用2小时做新摄像头单变量测试：

```text
同一bitstream/接口/排线方向
→ 只换新摄像头
→ 记录I2C、LED18—33和屏幕
```

- 若仍无CSI活动，当天借万用表/示波器，只测电源、复位和MCLK；普通探头不碰MIPI高速差分线。
- 其余时间完成空SoC加入视频工程，目标只为UART输出版本号和持续heartbeat。

当日验收：

- [ ] 摄像头故障边界有新证据，而不是“还是花屏”的口头结论。
- [ ] 视频工程内CPU输出`BUILD_ID`和heartbeat；若失败，保存完整构建/下载日志。

### 7月14日：APB最小闭环

只实现：

```text
REG_MAGIC（只读固定值）
REG_HEARTBEAT（CPU写、FPGA/LED可观察）
```

验收：

- [ ] 正式生成`soc.h`，基址来自生成物而非占位值。
- [ ] CPU读到MAGIC，连续写heartbeat。
- [ ] APB时钟、复位、地址宽度和CDC所有权写入接口记录。
- [ ] 合入后视频fallback/合成图仍正常，资源和Setup/Hold slack有记录。

若7月14日晚未通过：暂停双摄融合、OSD美化和机械臂板控，A与CPU支援全部救SoC/APB。

### 7月15—17日：保底集成

- 7月15日：接入最小`TARGET_CFG_STATUS`、`OPERATOR_EVENT`、`LIVE_FG_AREA`快照和固定`RESULT_STATUS` OSD。
- 7月15日中午双摄仍不稳，保底改为单摄；双摄只留增强分支。
- 7月16日：真实摄像头→统计→CPU→OSD；只修阻塞，不重构管线。
- 7月17日：冻结bitstream、工程路径、生成命令、slack和回退版本。

## 4. 成员B任务卡：CPU得分引擎负责人

B时间有限，因此B承担“定接口、拆任务、审结果”，纯C编码交给可支援成员并行完成。

### 7月12日：冻结契约

1. 写出四任务×5轮真值表。
2. 定版：

```c
competition_target_t
task_mode_t
reason_code_t
round_event_t
round_state_t
round_output_t
```

3. 确认任务规则：

```text
T1/T2: CUBE && color == target_color
T3:    CUBE && abs(size-reference) == 10mm
T4:    CUBE && abs(size-reference) <= 5mm
```

4. 将支援任务拆成两个互不冲突的小包：

- CPU-1：升级`task_matcher`与测试。
- CPU-2：新增`round_controller`与测试。

当日验收：

- [ ] 黑/白目标可配置，不只是在classifier中可识别。
- [ ] `LIVE_FG_AREA`被写成P0输入，`FG_AREA_AVAILABLE=0`明确是降级状态。
- [ ] `round_reset`不在目标配置中，PLACE/REMOVE/ABANDON/RESET是独立事件。
- [ ] 每个状态有超时和人工出口，机械臂运动中禁止盲复位。

### 7月13—14日：Host/Mock闭环

测试必须覆盖：

- 五色、三形状、20/25/30mm和UNKNOWN。
- 四任务边界：`delta=0/5/10mm`。
- 同一观测重复100帧只产生一次动作请求。
- 按键抖动、重复/过期事件、`event_seq + ACK`。
- 每个状态的放弃/软复位；机械臂busy/fault/timeout。
- 20轮Mock和至少1000组随机事件序列。

7月14日验收：

- [ ] 旧测试继续通过。
- [ ] 新四任务真值表全部通过。
- [ ] 无死锁、无重复动作。
- [ ] 失败输出精确到用例和状态，不接受只写“测试失败”。

### 7月15—17日：板上适配

- 7月15日：根据A提供的正式`soc.h`和寄存器语义适配`board_io`；先读固定/合成快照。
- 7月16日：把`round_controller`最小接入`main.c`，先保持`ARM_DISABLED`，跑20轮≤10分钟。
- 仅当C和A完成UART安全门，才接`round_controller → arm_controller`唯一请求。
- 7月17日冻结固件、参数版本、按键表、理由码和构建方法。

## 5. 成员C任务卡：机械臂与现场

### 7月12日：官方180°点位

1. 备份并保持旧10cm基线不变。
2. 建立临时可重复布局，固定机械臂底座、抓取区和目标方向标记。
3. 新建决赛点位：

```text
home_intermediate
pick_hover
pick
lift_safe
drop_hover_180
drop_180_max_reach
return_intermediate
```

4. 用底座角度标记/场地几何证明释放方向为相对起点180°±10°。
5. 先空载，再低速带载至少5轮；记录每轮抓取、释放、跌落、超时和人工干预。

当日验收：

- [ ] 新点位文件独立保存，包含日期、布局版本、角度、臂展、速度、释放高度。
- [ ] 至少5轮低速带载日志和视频。
- [ ] 0跌落、0碰撞；若失败，保留失败日志，不用手扶成功冒充自动成功。

### 7月13—14日：治具与UART安全方案

- 将摄像头支架、机械臂底座和物体定位区设计在同一底板；先做可重复临时版。
- 背景优先准备哑光中性灰候选，避免纯白/纯黑背景破坏五色识别；补光采用固定、漫反射、不过曝方案，等真实画面恢复后用测试矩阵定版。
- 与A一起从原理图确定板上UART候选、管脚、电平、共地和1Mbps支持；只形成接线审查表，不直接动作。

### 7月15—16日：板到臂分级验证

只有A的CPU/APB基线稳定后才开始：

1. UART回环。
2. 示波器确认1Mbps波形和空闲电平。
3. 无运动/只读或灯板指令。
4. 用户确认机械臂固定、姿态安全、急停/断电明确后，才做极小幅动作。
5. 最后接固定抓放动作，并验证一轮只执行一次。

7月16日中午未完成前三步：停止板控动作集成，比赛冻结F1 `ARM_DISABLED`版本。

## 6. 每日合流机制

每天固定两次短会：

- 中午：只报“证据、阻塞、是否触发降级”，不讲长过程。
- 晚上：决定次日唯一P0，更新`CURRENT_STATE.md`或对应日志。

每人必须按同一格式交付：

```text
目标：
修改文件：
运行命令：
PASS/FAIL：
证据路径：
仍未验证：
下一步最小动作：
```

## 7. 今天立即开始的三件事

1. **A立即做**：锁定合成源Mux/接口并输出Review Packet；同时列出SoC例程向视频工程迁移清单，今天不同时大改两份`top.v`。
2. **B立即做**：用1小时冻结四任务、`LIVE_FG_AREA`、理由码和round API；把matcher/round实现拆给支援成员，晚上只审测试结果。
3. **C立即做**：保留旧点位，新建180°决赛点位，完成空载→低速带载5轮并保存日志/视频；剩余时间画临时底板和定位模板。

今天结束时若三项都没有可审查产物，次日不得同时开启更多新任务。
