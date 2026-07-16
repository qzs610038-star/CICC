# 单摄新方案工作日志

> 起始日期：2026-07-14
>
> 适用范围：`competition_project_single_camera/` 隔离候选工程，以及受控同步到 `<local-demo-mirror>` 的构建镜像
>
> 记录原则：本文件是新方案获批后的连续动作总账。只追加已发生动作，不提前填写计划为完成，不删除失败记录，不用后续结果改写历史事实。

## 记录格式

后续每个动作按以下格式追加：

```text
### [日期时间] Gate / 动作标题

- 触发：用户指令、前一Gate结果或问题现象。
- 目标：本次只解决什么。
- 输入基线：Git分支/提交、仓库路径、D盘路径、bitstream或日志身份。
- 实际动作：执行的读取、复制、修改、同步、构建或板测。
- 修改文件：真实修改/新增文件；无修改时明确写“无”。
- D盘写入：无 / 具体同步文件。
- 命令与工具：可复现命令、Efinity版本或人工GUI步骤。
- 结果：PASS / PARTIAL / FAIL / BLOCKED。
- 证据：日志、哈希清单、Review Packet、截图或用户反馈路径。
- NOT VERIFIED：本次不能证明的事项。
- 下一步门禁：进入下一动作前必须满足的条件。
```

状态词固定为：

- `PASS`：该动作自己的验收条件全部满足。
- `PARTIAL`：已完成部分动作，仍有明确验收未完成。
- `FAIL`：已执行但未达到该动作的验收条件。
- `BLOCKED`：缺少外部输入、硬件操作或安全授权，当前不能继续。
- `NOT VERIFIED`：没有证据，不以相邻层结果代替。

---

## 2026-07-14

### [需求访谈完成] 单摄新路线边界冻结

- 触发：用户决定放弃`final_project`双摄视频工程，改用已跑通摄像头的官方Demo作为新视频基线。
- 目标：在修改工程前冻结视频、CPU、状态机、OSD、机械臂、协作和D盘同步边界。
- 输入基线：
  - 已跑通参考/构建镜像：`<local-demo-mirror>`
  - 摄像头：J48/ch0，斜上方单摄，固定投放点
  - 显示：只保留HDMI到电脑，不需要MIPI DSI
  - CPU：Efinity硬核QCRV32，最终与视频处于同一bitstream
  - 工具：Efinity `2025.2.288.4.15`
- 实际动作：通过多轮需求访谈冻结以下决策：
  - 新工程路径为仓库内 `competition_project_single_camera/`。
  - 只废弃旧视频工程，选择性迁移CPU、机械臂和Host测试模块。
  - FPGA负责视频前端、ROI/基础统计、APB/CDC和OSD渲染；CPU负责分类、四任务、逐轮状态和myCobot。
  - 开发顺序为任务一五色→任务二`CUBE/NON_CUBE`→任务三/四尺寸→F1→F2。
  - 第一版采用`PLACE`与独立`ABANDON`小闭环；不设`REMOVE`、自动空场互锁或复杂按键菜单。
  - 开发期目标配置走UART；最终结果先UART后OSD。
  - 尺寸未标定前固定输出`UNAVAILABLE`。
  - F1保持`ARM_ENABLED=0`，F1稳定后才进入F2。
  - 仓库新工程是正式源码，D盘仅作人工构建/烧录镜像；Codex负责增量同步，用户负责综合、烧录和反馈。
- 修改文件：无工程代码修改。
- D盘写入：无。
- 结果：`PASS`，需求边界已冻结。
- 证据：`docs/technical_plans/single_camera_competition_master_plan_20260714.md`。
- NOT VERIFIED：所有新工程构建、视频复现、SoC、CPU、特征、OSD、按键和机械臂能力。
- 下一步门禁：用户审核并明确批准总方案。

### [方案文档固化] 建立候选工程文档骨架

- 触发：需求访谈完成，用户要求先形成全局新方案。
- 目标：让新路线、旧工程边界和后续Gate在仓库内可发现。
- 输入基线：仓库`main@d433bca`，工作区已有与本任务无关的未跟踪文件。
- 实际动作：
  - 创建新工程`README.md`和中文总方案。
  - 更新仓库根`README.md`，增加候选新主线入口。
  - 更新`CURRENT_STATE.md`，登记旧视频主线停止演进、新单摄工程为候选主线。
  - 只读核查D盘目录、`src/top.v`、`mem_test.xml`、`.peri.xml`、SDC、HDMI/DSI/双路源码结构。
- 修改文件：
  - `competition_project_single_camera/README.md`
  - `competition_project_single_camera/docs/technical_plans/single_camera_competition_master_plan_20260714.md`
  - `README.md`
  - `CURRENT_STATE.md`
- D盘写入：无。
- 命令与工具：PowerShell只读文件检查、`git diff --check`。
- 结果：`PASS`，文档链接和格式检查通过；状态保持“待用户审核”。
- 证据：上述文件及当次Git差异。
- NOT VERIFIED：Demo复制、构建和板级现象。
- 下一步门禁：用户明确批准方案。

### [用户批准] 新方案进入M0执行

- 触发：用户回复“批准执行”。
- 目标：只执行M0身份审计和基线复制，不修改视频功能。
- 实际动作：将总方案版本更新为`v1.0-approved`，M0前禁止项继续有效。
- 修改文件：总方案状态、README和`CURRENT_STATE.md`随后随M0证据更新。
- D盘写入：无。
- 结果：`PASS`，允许执行M0；未授权进入M1。
- 证据：用户当前会话明确批准。
- NOT VERIFIED：M0构建和板级复现。
- 下一步门禁：运行交接健康检查并冻结D盘身份。

### [M0-01] 交接健康检查与工作区冻结

- 触发：M0开始。
- 目标：确认仓库、关键文件和安全边界可用，并识别现有工作区状态。
- 输入基线：`main@d433bca`，工作区dirty。
- 实际动作：
  - 使用进程级`ExecutionPolicy Bypass`运行`tools/agent_handoff_health_check.ps1`。
  - 检查Git状态、根`.gitignore`、D盘`.lock`和Efinity相关进程。
  - 只读统计D盘目录和文件类型。
- 修改文件：无。
- D盘写入：无；未删除`.lock`。
- 命令与工具：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\agent_handoff_health_check.ps1
git status --short --branch
```

- 结果：`PASS`。健康检查无FAIL；唯一警告为未安装`pymycobot`，与M0无关。未发现正在运行的Efinity构建进程。
- 证据：终端健康检查输出；`docs/review_packets/m0_demo_baseline_review_packet_20260714.md`。
- NOT VERIFIED：D盘`.lock`来源和GUI是否仍打开；因此未删除或修改锁文件。
- 下一步门禁：解析`mem_test.xml`实际引用，建立安全复制白名单。

### [M0-02] D盘工程与历史bitstream身份审计

- 触发：健康检查通过。
- 目标：证明当前历史bitstream、工程XML、日志和源码是否属于同一构建时间链。
- 输入基线：`<local-demo-mirror>`。
- 实际动作：
  - 读取`mem_test.xml`的工具版本、器件、`last_run_flow`和`last_run_state`。
  - 核对`outflow/`中map、Debugger、PNR、timing、CDC、programmer、bitstream和hex的时间链。
  - 读取`mem_test.pgm.out`，确认bitstream生成使用同一D盘`work_pnr/mem_test.lbf`与`outflow/mem_test.lpf`。
  - 计算工程XML、periphery、SDC、`src/top.v`和历史bitstream SHA-256。
  - 读取历史map资源、timing和CDC报告。
- 修改文件：无。
- D盘写入：无。
- 结果：`PARTIAL`：
  - 历史flow从map到bitstream成功，时间连续落在2026-07-13 21:21至21:24。
  - 历史bitstream SHA-256为`A99F14AB8922783C51A71953288F9A25DE1C70DC3E3962F5508BBF53EF75057C`。
  - 用户确认该bitstream可稳定显示J48/ch0真实画面。
  - map资源为ADD 2416、LUT4 14540、FF 11516、RAM10 211、DPRAM10 4。
  - CDC报告无Synchronizer warning。
  - timing存在跨时钟setup负slack，最差`mipi_clk -> i_sysclk_div2 = -1.433ns`，因此历史基线不能标记为STA PASS。
- 证据：
  - `docs/baseline/m0_historical_build_artifacts_20260714.csv`
  - `docs/review_packets/m0_demo_baseline_review_packet_20260714.md`
- NOT VERIFIED：负slack路径是否应通过异步约束、例外约束或RTL CDC修复；本轮不判断其可忽略。
- 下一步门禁：建立包含编译期include与初始化文件的完整白名单。

### [M0-03] 生成工程复制白名单

- 触发：D盘身份时间链确认。
- 目标：只复制构建所需源码与生成配置，排除缓存、历史输出和无关资料。
- 实际动作：
  - 从`mem_test.xml`提取53个`design_file`。
  - 核查2个工程IP配置：`csi_rx_controller`和`dsi_tx`。
  - 扫描源码的``include``、`.mem`和初始化依赖。
  - 补入4个编译期依赖：`Axi_Mux_Param.vh`、`timescale.v`、`i2c_master_defines.v`、`Panel_1080p_reg.mem`。
  - 补入工程XML、periphery XML、SDC、Debugger配置/模板和两个IP的生成源码/定义/模板。
  - 明确排除`outflow/`、`outflow_ch0_*`、`work_*`、`.lock`、`.bak`、ModelSim数据库/波形、未引用Testbench、`ip/ram`和历史bitstream本体。
- 修改文件：无。
- D盘写入：无。
- 结果：`PASS`，形成75项白名单；53个设计文件、2个IP设置、SDC和4个编译期依赖均存在。
- 证据：`docs/review_packets/m0_demo_baseline_review_packet_20260714.md`第3节。
- NOT VERIFIED：Efinity从仓库副本实际运行时是否还会访问未显式列出的工具缓存；需新构建证明。
- 下一步门禁：复制白名单并逐文件校验哈希。

### [M0-04] 白名单基线复制到新工程

- 触发：白名单完整性检查通过。
- 目标：建立保持Demo相对结构的正式仓库源码副本。
- 实际动作：复制75个白名单文件到`competition_project_single_camera/`，不重排Demo源码目录。
- 修改文件：新增75个Demo基线文件，包括根工程文件、`src/`必要RTL和`ip/`必要生成文件。
- D盘写入：无，复制方向仅为D盘→仓库。
- 命令与工具：PowerShell`Copy-Item -LiteralPath`逐文件复制，随后逐文件`Get-FileHash -Algorithm SHA256`。
- 结果：`PASS`，`COPIED=75`，源/目标`HASH_MISMATCH=0`。
- 证据：`docs/baseline/m0_source_manifest_20260714.csv`。
- NOT VERIFIED：新工程完整构建和板级画面。
- 下一步门禁：复核工程直接引用与D盘首次同步状态。

### [M0-05] 工程引用复核与首次D盘同步核查

- 触发：白名单复制完成。
- 目标：证明仓库副本没有缺引用，并避免无意义覆盖当前D盘基线。
- 实际动作：
  - 在仓库副本重新解析`mem_test.xml`。
  - 检查53个设计文件、2个IP设置和1个SDC，共56项直接引用。
  - 检查4个编译期include/mem依赖。
  - 对75个仓库文件与D盘对应文件再次计算SHA-256。
  - 生成首次D盘同步表。
- 修改文件：
  - `docs/baseline/m0_source_manifest_20260714.csv`
  - `docs/baseline/m0_initial_d_drive_sync_20260714.csv`
  - `docs/baseline/m0_historical_build_artifacts_20260714.csv`
- D盘写入：无。75项全部为`NO_OP_MATCH`，因此没有执行覆盖。
- 结果：`PASS`，直接引用`56/56`存在，编译期依赖`4/4`存在，最终哈希`75/75`一致。
- 证据：上述3份CSV。
- NOT VERIFIED：人工新构建是否成功。
- 下一步门禁：形成M0 Review Packet、人工构建反馈表和仓库状态入口。

### [M0-06] M0证据包与操作入口落地

- 触发：复制和哈希复核通过。
- 目标：让用户能够不修改工程地执行剩余M0人工验收，并让后续Agent从单一入口恢复上下文。
- 实际动作：
  - 创建M0 Review Packet。
  - 创建人工构建、烧录、3次冷启动和10分钟稳定性反馈表。
  - 创建`docs/`、`cpu/`、`integration/`、`tests/`入口README。
  - 创建新工程专用`.gitignore`，排除`outflow*`、`work_*`、数据库、仿真产物、锁和备份文件。
  - 更新新工程README、总方案、根README和`CURRENT_STATE.md`。
- 修改文件：
  - `docs/review_packets/m0_demo_baseline_review_packet_20260714.md`
  - `docs/debug_sessions/m0_manual_build_board_check_20260714.md`
  - `.gitignore`及各入口README
  - `README.md`、总方案、仓库根`README.md`、`CURRENT_STATE.md`
- D盘写入：无。
- 结果：`PASS`，`git diff --check`通过；关键工程文件未被忽略；证据链接存在。
- 证据：上述文件及最终Git状态。
- NOT VERIFIED：用户人工构建、烧录和板级复现。
- 下一步门禁：用户完整运行Efinity flow并反馈。M0 Gate在此之前保持`PARTIAL / BOARD RECHECK PENDING`，禁止进入M1。

### [当前等待] M0人工构建、烧录与画面复现

- 触发：M0仓库侧动作完成。
- 目标：用当前D盘镜像重新生成bitstream，并证明J48/ch0到HDMI没有回归。
- 实际动作：尚未执行，等待用户。
- 修改文件：无。
- D盘写入：预计仅由Efinity生成`outflow/`和`work_*`产物，用户不手改源码。
- 工具：Efinity `2025.2.288.4.15`。
- 结果：`BLOCKED`，等待人工硬件操作。
- 证据入口：`docs/debug_sessions/m0_manual_build_board_check_20260714.md`。
- NOT VERIFIED：新Map/PNR/bitstream、烧录、3次冷启动、10分钟稳定画面、分辨率/帧率一致性。
- 下一步门禁：上述全部通过后才允许M1A只禁用ch1。

---

## 后续追加要求

1. 每次实际动作完成后立即追加，不等到阶段结束再凭记忆补写。
2. 同一Gate发生失败、回退、重试时分别新增条目，禁止覆盖原失败记录。
3. 用户反馈的上板现象要注明“用户现场反馈”，并链接对应bitstream哈希和构建日志。
4. D盘每次同步必须单独记录源提交、文件清单、同步前后哈希和回退点。
5. Efinity每次构建记录Map/PNR/Setup/Hold/CDC/warning/bitstream，不把Map PASS写成板级PASS。
6. 机械臂相关动作必须另附安全Gate和Review Packet；本日志只引用，不替代安全记录。

### [M0-07] 用户现场反馈：真实画面偏绿、模糊与拍屏伪影初判

- 触发：用户提供已烧录后的 HDMI 画面截图，并反馈“有真实画面，但是画面有点偏绿，有点模糊，不知道是不是摄像机像素低的问题”。
- 目标：在不破坏已跑通的 J48/ch0→HDMI 链路前，区分传感器分辨率、色彩处理、RAW 数据顺序、时序/帧缓存和拍屏伪影的影响。
- 用户现场反馈：
  - 截图文件：会话临时截图（未纳入仓库）。
  - 可见真实场景，说明当前画面不是纯 fallback 色条。
  - 整体有明显绿色偏色。
  - 黑色矩形边缘和背景纹理存在水平拖影/重影；但截图为拍摄 HDMI 显示器的二次图像，显示器像素栅格、刷新扫描和相机曝光可能放大横纹与模糊，不能直接等价为 FPGA 像素错位。
- 实际动作：
  - 使用原图进行视觉检查，未修改 RTL、工程 XML、SDC、IP 或任何生成产物。
  - 运行 `tools/agent_handoff_health_check.ps1`，初次被 PowerShell 执行策略拦截，随后使用进程级 `ExecutionPolicy Bypass` 重试并通过；唯一警告为未安装 `pymycobot`，与 M0 视频画面无关。
  - 读取正式工程 `src/top.v`、`src/debayer/debayer_top_2to1.v`、`src/debayer/raw_to_rgb.v`、`src/debayer/line_buffer.v`、`src/uvc_src/white_balance.v`、`src/dvi_tx/hdmi_top.v` 及 MIPI/帧缓存连接。
  - 核对到当前视频链为 `J48/ch0 -> RAW10(40b) -> 帧缓存 -> RAW8(32b) -> Debayer -> white_balance -> HDMI`。
  - 发现一个待隔离验证的色彩契约风险：`debayer_top_2to1.v` 输出 `rgb_datax2_o` 注释和拼接顺序为 `{B,G,R,B,G,R}`，而 `white_balance.v` 将高低 24 位按 `{R,G,B}` 解包；这可能导致红蓝通道及其增益对象对调，但尚不能单凭截图确认是偏绿根因。
  - 核对 `top.v` 的 RAW 抽取 `{rx_out_data[39:32], rx_out_data[29:22], rx_out_data[19:12], rx_out_data[9:2]}`、ch0 双像素输入 `{ch0_b,ch0_g}` 和 HDMI 端 `R/G/B` 字节映射；本轮不据拍屏图修改这些链路。
- 判断：
  - “像素低”可以造成细节不足，但不能单独解释明显绿色偏色；偏绿优先排查 Bayer/通道顺序、白平衡增益和摄像头曝光/照明。
  - 水平拖影目前有两种同等需要区分的解释：拍摄显示器产生的二次伪影，或真实链路中的双像素/帧缓存/时序问题。必须先获取直接 HDMI 截图或观察物体移动时的边缘变化。
  - 当前 M0 仍保持 `PARTIAL / BOARD RECHECK PENDING`；不因“已有真实画面”将新工程标记为构建、时序或板级闭环通过。
- D 盘写入：无。
- 证据：本日志前述 M0 条目；正式工程源码路径如上；用户截图路径如上。
- NOT VERIFIED：截图是否来自新工程重新构建的 bitstream；Map/PNR/bitstream 状态；三次冷启动与 10 分钟稳定性；直接 HDMI 抓帧结果；实际 Bayer pattern、白平衡增益寄存器和镜头焦距。
- 下一步门禁：用户先反馈截图来源及构建/烧录结果，并完成以下最小观察：
  1. 直接 HDMI 抓图或屏幕截图（避免手机拍屏）。
  2. 让固定高对比物体水平移动，观察重影是否跟随运动或固定在显示器扫描方向。
  3. 放置白色、红色、蓝色物体各一次，判断是否只是红蓝互换/白平衡偏置。
  4. 在不改工程的情况下检查镜头保护膜、焦距和照明；确认后再决定是否制作仅修正 RGB 打包契约的隔离 bitstream。

### [M0-08] 用户现场颜色对照：蓝/红方块验证与白平衡缺陷定位

- 触发：用户提供两块颜色实物的现场对照图，并明确实物位置为“左边蓝色、右边红色”。
- 用户现场反馈：
  - 截图文件：会话临时截图（未纳入仓库）。
  - 左侧蓝色方块在 HDMI 画面中偏青绿；右侧红色方块偏橙棕；背景同样明显偏绿。
- 实际动作：
  - 重新核查 `src/uvc_src/white_balance.v` 的帧同步、颜色解包和增益更新逻辑；未修改 RTL、工程 XML、SDC、IP 或 D 盘文件。
  - 发现 `vs_d` 声明后没有在任何复位分支赋初值，也没有在时钟过程更新为 `vs_in`；因此 `vs_fall = vs_d & (~vs_in)` 无法成为可靠的帧结束脉冲。
  - `gain_r`、`gain_b` 的统计清零与更新均依赖 `vs_fall`，当前自动白平衡无法被视为有效工作。
  - `debayer_top_2to1.v` 的输出封装仍为 `{B,G,R,B,G,R}`，而 `white_balance.v` 的字段命名按 `{R,G,B,R,G,B}` 解包。由于当前增益更新失效且复位后增益为 1.0，该错配尚不必然造成字节交换；但一旦恢复自动白平衡，它会使红/蓝统计和增益应用对象不明确，必须一并规范。
- 判断：
  - 此颜色对照表明红、蓝物体仍可在画面中区分，不能把当前问题简单归结为 HDMI R/B 三线整体交换。
  - 绿色偏色的最高优先级根因是自动白平衡控制失效，导致传感器原始色偏未经校正；实际 Bayer phase、镜头、照明和拍屏伪影仍是次级待验证项。
  - 建议的最小隔离修复仅限 `white_balance.v`：补齐 `vs_d` 的复位和时钟更新，并把接口字段明确为 BGR 后保持 BGR 字节输出；不得改动 `top.v` 的 MIPI、RAW10 截取、帧缓存、HDMI 时钟或时序。
- D 盘写入：无。
- NOT VERIFIED：此截图所用 bitstream 是否为本轮新工程重新构建产物；自动白平衡恢复后的实际画面、Map/PNR/bitstream、三次冷启动和 10 分钟稳定性；Bayer phase 是否完全匹配 SC431HAI 传感器输出。
- 下一步门禁：获得用户对“仅修正 `white_balance.v` 并构建观察”的确认后，先在正式工程创建小范围补丁和静态检查，再逐文件同步到 D 盘供用户手动综合烧录。

### [M0-09] 白平衡帧同步与 BGR/RGB 契约隔离修复

- 触发：用户确认执行 M0-08 提出的最小隔离修复。
- 目标：修复自动白平衡帧同步失效和 Debayer/HDMI 间的颜色字节契约，不修改 MIPI、RAW10、帧缓存、HDMI 时钟、约束、IP 或顶层连线。
- 修改文件：
  - `src/uvc_src/white_balance.v`。
- 实际修改：
  - 在复位分支初始化 `vs_d <= 1'b0`，在每个 `clk` 上升沿采样 `vs_d <= vs_in`，使 `vs_fall` 成为稳定的一拍帧结束检测。
  - 将输入数据字段从隐含的 RGB 解释改为与 `debayer_top_2to1.v` 一致的 `{B,G,R,B,G,R}`：高 24 位为奇像素 B/G/R，低 24 位为偶像素 B/G/R。
  - 保持输出明确为 HDMI 消费者所需 `{R,G,B,R,G,B}`，并保留 R/B 增益分别作用在正确颜色分量上。
  - 未更改模块端口、位宽、时钟、复位极性和 `top.v` 连接；ch0/ch1 两个现存实例共用该模块，行为一致。
- 静态验证：
  - `PASS`：`vs_d` 复位赋值存在。
  - `PASS`：`vs_d <= vs_in` 采样存在。
  - `PASS`：BGR 输入字段与 Debayer 输出顺序一致。
  - `PASS`：奇/偶像素输出均为 RGB 顺序。
  - 本机未找到 `iverilog`、`verilator` 或 `vlog`，因此未进行本地 HDL 编译/仿真；Efinity 综合、PNR 和板级验收仍由用户执行。
- D 盘同步：
  - 同步方向：`competition_project_single_camera/src/uvc_src/white_balance.v` → `<local-demo-mirror>/src/uvc_src/white_balance.v`。
  - 同步前 D 盘 SHA-256：`BC84655C198CADB948F94B5125D6ED5AA35AD5C0006017E74FF921B62D20F507`。
  - 同步后仓库/D 盘 SHA-256：`DF9D141353D743BDE5FAB4A5D91D9CED9D37441B802B2D3D4A55DE34B284739F`，一致。
  - D 盘写入仅此一个源码文件；未写入 `outflow/`、`work_*`、IP、SDC、XML 或 bitstream。
- 结果：`STATIC PASS / BOARD NOT VERIFIED`。
- NOT VERIFIED：Efinity Map/PNR/bitstream；烧录后蓝/红/白色画面的恢复程度；三次冷启动、10 分钟稳定性、原始 HDMI 抓帧；Bayer phase、镜头焦距和照明条件。
- 下一步门禁：用户以 D 盘工程完成完整 Efinity flow 后，反馈 Map/PNR/bitstream、蓝/红/白物体画面和是否仍有真实拖影；仅在新画面仍明显偏色时再讨论 Bayer phase 或传感器寄存器，禁止盲目改动帧缓存/HDMI 链路。

### [M0-10] dsi_tx_top.v 路径可移植性修复

- 触发：M0 命令行 Map 因 `$readmemh` 无法打开 `/src/mipi_dsi/Panel_1080p_reg.mem` 而 FAIL（MULTI-FACTOR：HDL 可移植性缺陷 + 环境触发）。
- 目标：将唯一生效的硬编码绝对路径改为受控工程相对路径，不改初始化内容或视频逻辑。
- 修改文件：
  - `src/mipi_dsi/dsi_tx_top.v`。
- 实际修改：
  - 第 144 行：`.INITIAL_CODE("/src/mipi_dsi/Panel_1080p_reg.mem")` → `.INITIAL_CODE("src/mipi_dsi/Panel_1080p_reg.mem")`（仅删除开头 `/`）。
  - 未修改 `panel_config.v`、`true_dual_port_ram.v`、`mem_test.xml`、`constrain.sdc`、IP、mem 文件或任何其他 RTL。
- 修改前 SHA-256：`789BD0EAF9C3AE4D2B5D01410A41E6FBEB3F1E406AF1D5F29ECAF2EE4924559D`
- 修改后 SHA-256：`DDAF952AE26A599A988235FBE5EB2815AA9C8FDE66B7346EB359A24996F031D7`
- `git diff`：确认仅一行变更，仅删除路径开头 `/`。
- 旧 Map 失败日志归档至：`docs/debug_sessions/evidence/m0_map_fail_pre_path_fix_20260715/`（5 个文件，SHA-256 与 outflow 原文件一致）。
- D 盘写入：无。
- 结果：`MAP PASS / RECORD CORRECTION PENDING / PNR HOLD`。
- Map 重测结果（2026-07-15）：PASS（退出码 0），旧 `$readmemh` 错误已消除。资源 ADD=4367/LUT4=19399/FF=11800/DSP48=4/RAM10=211/DPRAM10=4/SRL8=1。新运行段 warn.log：1042 非空行，223 VERI/VDB WARNING（15 类别），785 EFX-0256（详见 `m0_efx0256_interface_audit_20260715.md`），VERI-2561 INFO 47。整文件 warn.log：1177 行/1173 非空行。
- 记录修正（2026-07-15，Codex2 指令）：warning 统计从 224/16 类修正为 223/15 类，删除虚假 OTHER=1；`Found 587 warnings` 不计入统计。Efinity 曾自动改写 `mem_test.xml`，已恢复到构建前精确 Git 字节状态。Delta manifest 已扩展列以区分 initial manifest SHA（`6385FD5A...`）、pre-fix SHA（`789BD0EA...`）和 post-fix SHA（`DDAF952A...`）。
- 接口审计修正（2026-07-15，Codex2 第二轮）：
  - AXI1 698 个：EXPECTED → TBD/BLOCKING（`is_axi_enable="true"`——活跃 DDR 目标）。
  - MIPI DSI ch0 70 个：EXPECTED/DSI 整体未激活 → TBD/仅 ch0 未驱动（ch1 由 `dsi_tx_top_inst1` 驱动）。
  - JTAG 2 个：新发现 `jtag_inst2` 不在 `mem_test.peri.xml` 中。
  - LED 2 个：保持 UNEXPECTED（`top.v` 仅驱动 `led[1:0]`）。
  - 删除"785 项无 DDR 主通道问题"。
  - 新增 `m0_efx0256_interface_audit_20260715.md` 修正版 + `../review_packets/m0_pnr_interface_resolution_plan_20260715.md`。
- NOT VERIFIED：PNR/STA/CDC/bitstream、板级。仅改变文件定位，不改变初始化内容或视频逻辑；板级闭环仍需完整构建。
- 下一步门禁：Codex2 审查修正后的 EFX-0256 接口审计和 PNR 前接口归属解决方案。批准后方可进入 PNR。

### [主线合并复核] M0-09 增量登记与除零防护

- 触发：将单摄候选分支合入本地 `main` 前进行约束、证据和 RTL 安全复核。
- 修改文件：`src/uvc_src/white_balance.v`、`docs/baseline/m0_post_baseline_delta_20260714.csv` 及路线/状态文档。
- 实际修改：为三个平均值除法增加 `pixel_cnt == 0` 防护；不修改端口、时钟、复位极性、MIPI、帧缓存、HDMI、SDC、XML 或 IP 生成参数。补充 delta manifest，明确初始 75/75 清单是历史快照，不再描述当前树。
- 约束清点：`constrain.sdc`、`mem_test.xml`、`mem_test.peri.xml` 未作功能修改；两个 IP `settings.json` 仅将生成器的历史绝对 `base_path` 改为相对于各 IP 目录的 `..`，未重新生成 IP。
- 结果：`STATIC PASS / BOARD NOT VERIFIED`。历史 bitstream 不绑定本次修改后的源码。
- 下一步门禁：从当前候选仓库源码完成 Efinity 全流程、匹配源码/bitstream 哈希、烧录、3 次冷启动和 10 分钟画面复现；通过前保持隔离候选身份。
