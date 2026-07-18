# competition_project_single_camera

第十届集创赛雄芯院方向分赛区决赛的隔离单摄候选工程。

> 当前状态：**候选方案已批准；M0 初始白名单复制完成，M0-09 白平衡增量及主线合并除零防护已登记，等待用户重新构建、烧录和画面复现。**
>
> 已知可运行参考与手工构建/烧录镜像：`<local-demo-mirror>`（由操作者在本机自行映射，不提交绝对路径）
>
> FPGA 工具版本：Efinity `2025.2.288.4.15`

## 当前入口

1. [工作日志](WORK_LOG.md)：新方案批准后每一步实际动作的连续总账，后续必须持续追加。
2. [单摄决赛主方案](docs/technical_plans/single_camera_competition_master_plan_20260714.md)：架构、Gate和停止条件。
3. M0 当前证据与操作入口见 [M0基线 Review Packet](docs/review_packets/m0_demo_baseline_review_packet_20260714.md)、[M0 基线后增量清单](docs/baseline/m0_post_baseline_delta_20260714.csv) 和 [M0人工构建/烧录反馈表](docs/debug_sessions/m0_manual_build_board_check_20260714.md)。
4. 当前事实、阻塞和路线覆盖项以仓库根目录 [`CURRENT_STATE.md`](../CURRENT_STATE.md) 为准。
5. 比赛任务、评分和现场流程以 [`0710 官方细则`](../final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md) 为准。
6. 系统职责和机械臂安全边界继续服从仓库根目录 [`AGENTS.md`](../AGENTS.md)。
7. [单摄板上图像识别最小闭环紧急计划](docs/technical_plans/minimum_board_recognition_loop_plan_20260717.md)：以当前 `main` 为基线，按 `USER2/UART0/APB -> feature mailbox -> 精简 CPU 分类 -> UART0 真实识别` 的 Gate 顺序实施。

## 路线边界

- 视频工程以已跑通的 D 盘官方 Demo 为来源，不迁移 `final_project` 的视频顶层、工程 XML、约束或旧 PNR 结论。
- `final_project` 继续保留为 CPU、机械臂、Host 测试、接口经验和历史证据来源库。
- FPGA 负责 J48/ch0 视频链、ROI/基础统计、APB/CDC 硬件接口和 OSD 像素渲染。
- 板上 QCRV32 CPU 负责五色/形状/尺寸分类、四任务关系判定、逐轮状态机、参数和 myCobot 控制。
- F1 保持 `ARM_ENABLED=0`；F1 通过后才进入 F2 机械臂安全接入。
- PC、UART 菜单和 Host 测试只用于开发，不代替最终板上闭环。

## 目录原则

M0 复制时优先保持 `<local-demo-mirror>` 的相对目录结构，避免在复现摄像头基线的同时重排源码。计划新增的协作目录如下：

```text
competition_project_single_camera/
  README.md
  docs/
    technical_plans/
    debug_sessions/
    review_packets/
  integration/
  cpu/
  tests/
  src/                 # M0 审核通过后从 Demo 复制
  ip/                  # M0 审核通过后复制必要生成 IP
  mem_test.xml         # M0 审核通过后复制
  mem_test.peri.xml
  constrain.sdc
```

M0已按工程引用白名单复制当前Demo基线，未复制`outflow/`、`work_*`、仿真数据库、波形、`.bak`、锁文件或历史bitstream。`cpu/`、`integration/`和`tests/`当前只建立职责入口，尚未迁移代码。

## D盘同步规则

- 本目录当前是隔离候选源码；只有 M0 新构建、匹配 bitstream、烧录、3 次冷启动和 10 分钟画面复现全部通过，并同步更新 `AGENTS.md`、主方案和 `CURRENT_STATE.md` 后，才能升格为正式源码。
- D盘仅作为人工 Efinity 构建、PNR、bitstream 生成和烧录镜像。
- 每次同步前必须比较差异、记录同步文件并保留上一可工作回退点。
- 只增量同步已审核文件，不整树覆盖，不自动删除 D 盘产物。
- 用户只负责综合、烧录和反馈；Codex负责同步与记录。
- 如现场必须手改 D 盘源码，须先声明，并在下一步反向同步回仓库。

## 状态词

- `HOST VERIFIED`：仅 Host/Mock。
- `MAP VERIFIED`：仅映射通过。
- `PNR/STA VERIFIED`：布局布线和时序有对应报告。
- `BOARD VERIFIED`：有匹配 bitstream、烧录和上板现象证据。
- `NOT VERIFIED`：不得由相邻层结果代替。
