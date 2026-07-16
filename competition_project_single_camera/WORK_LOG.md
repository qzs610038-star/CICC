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

### [主线合并复核] M0-09 增量登记与除零防护

- 触发：将单摄候选分支合入本地 `main` 前进行约束、证据和 RTL 安全复核。
- 修改文件：`src/uvc_src/white_balance.v`、`docs/baseline/m0_post_baseline_delta_20260714.csv` 及路线/状态文档。
- 实际修改：为三个平均值除法增加 `pixel_cnt == 0` 防护；不修改端口、时钟、复位极性、MIPI、帧缓存、HDMI、SDC、XML 或 IP 生成参数。补充 delta manifest，明确初始 75/75 清单是历史快照，不再描述当前树。
- 约束清点：`constrain.sdc`、`mem_test.xml`、`mem_test.peri.xml` 未作功能修改；两个 IP `settings.json` 仅将生成器的历史绝对 `base_path` 改为相对于各 IP 目录的 `..`，未重新生成 IP。
- 结果：`STATIC PASS / BOARD NOT VERIFIED`。历史 bitstream 不绑定本次修改后的源码。
- 下一步门禁：从当前候选仓库源码完成 Efinity 全流程、匹配源码/bitstream 哈希、烧录、3 次冷启动和 10 分钟画面复现；通过前保持隔离候选身份。

### [M0-10] 用户现场反馈：白平衡修复后蓝变黄、红变紫

- 触发：用户烧录 M0-09 版本后提供新的蓝/红实物对照图，并明确左侧实物为蓝色、右侧实物为红色；画面中分别呈黄色和紫色。
- 用户现场反馈：
  - 截图文件：`C:\Users\20306\AppData\Local\Temp\codex-clipboard-c4d2f4fb-559f-41d1-adc2-1ee16d5fe0fd.png`。
  - 结论：M0-09 不能作为颜色修复完成；自动白平衡改变了色彩，但未恢复正确颜色。
- 复核与结论：
  - 取消“自动白平衡是最高优先级根因”的结论。蓝变黄、红变紫更符合 Bayer 相位或 RAW 双像素顺序错误，而非单纯 R/B 字节整体交换。
  - 正式工程 `src/top.v` 将帧缓存的两个 RAW8 样本固定送入 Debayer；该阶段的 `ch*_g/ch*_b` 仅是 RAW 容器名称，不是颜色通道。
  - 传感器初始化 ROM 设置有效窗口起点为 `X=0x0007`、`Y=0x00b3`，均为奇数。原始 `raw_to_rgb.v` 把首行 Bayer 相位硬编码，未将裁剪起点奇偶性作为参数。
  - 历史 `final_project` 审计记录已将 `{ch*_g,ch*_b}` 与 `{ch*_b,ch*_g}` 的选择登记为真实 Bayer 相位风险，并提供可切换开关；当前正式工程缺少该控制。
  - 外部 SC431 Bayer 阵列检索未获得可验证资料（搜索超时或索引限流），因此不把原生阵列类型标为已证实。
- D 盘同步前核查：用户要求以正式工程 `white_balance.v` 为准。两份文件内容级 `git diff --no-index` 无差异，初始 SHA-256 不同是 LF/CRLF 行尾差异；随后按用户指令以正式工程版本同步到 D 盘。
- NOT VERIFIED：SC431 原生 Bayer 阵列；目前截图是否为本轮完整构建的 bitstream；本轮实际 Map/PNR/bitstream 结果。

### [M0-11] Bayer 相位参数化、HDMI 显式 RGB 与白平衡旁路

- 触发：M0-10 已证明只修正白平衡不足；用户要求全面排查并修复。
- 目标：仅在 RAW→Debayer→HDMI 的颜色解释边界修复问题，保持 MIPI 接收、RAW10 截取、DDR 帧缓存、HDMI 时钟、SDC、XML、IP 和板级引脚不变。
- 修改文件：
  - `src/top.v`。
  - `src/debayer/debayer_top_2to1.v`。
  - `src/debayer/raw_to_rgb.v`。
- 实际修改：
  - 新增 `CH0_BAYER_SWAP_PIXELS` / `CH1_BAYER_SWAP_PIXELS` 参数，显式选择帧缓存导出的两个相邻 RAW 样本顺序。
  - 新增 `CH0_BAYER_ROW_SWAP` / `CH1_BAYER_ROW_SWAP` 参数，将裁剪起点导致的首行 Bayer 相位纳入 Debayer；默认 `1'b1` 对应当前 ROM 的奇数 `Y=0x00b3` 起点。
  - 默认 `CH*_BAYER_SWAP_PIXELS=1'b0`，即使用 `{ch*_g,ch*_b}`；若上板红/蓝仍不正确，可只翻转该单一参数为 `1'b1`，不重构链路。
  - `raw_to_rgb.v` 新增 `BAYER_ROW_SWAP` 参数，以 `r_y_cnt[0] ^ BAYER_ROW_SWAP` 选择两种已有插值分支；未改插值算术、行缓冲或数据位宽。
  - `debayer_top_2to1.v` 将行相位参数传给 `raw_to_rgb`，接口除参数外不变。
  - HDMI 主路径默认旁路自动白平衡，直接使用 Debayer 输出；新增明确的 `{B,G,R,B,G,R}` → `{R,G,B,R,G,B}` 重排，避免 HDMI 将 BGR 数据误作 RGB。
  - 保留 `white_balance` 实例与 M0-09 修复版本，但它不再参与 HDMI 颜色基线；待 Bayer 相位正确后才考虑重新启用。
- 静态验证：
  - `PASS`：工程参数列表闭合。
  - `PASS`：ch0 双像素相位开关、行相位开关和 Debayer 实例参数均连通。
  - `PASS`：Debayer 的行相位参数传递到 `raw_to_rgb`，且 XOR 分支存在。
  - `PASS`：HDMI 使用唯一、显式的 BGR→RGB 重排，并在默认下旁路白平衡。
  - `PASS`：`mem_test.xml` 已引用三个修改 RTL 与 `white_balance.v`，无需修改工程 XML。
  - `NOT VERIFIED`：本机未找到 `iverilog`、`verilator` 或 `vlog`，未做本地 HDL 编译/仿真；Efinity Map/PNR/bitstream 与板级颜色仍由用户验证。
- D 盘写入：待本条后的哈希一致性同步完成后记录。
- 下一步门禁：用户用蓝、红、白物块验证本版。若蓝/红仍呈相反色，只改变 `CH0_BAYER_SWAP_PIXELS`；若两色仍整体错相位但不相反，只改变 `CH0_BAYER_ROW_SWAP`。每次只翻转一个参数并保留截图，禁止触碰 MIPI/DDR/HDMI 时序。

### [M0-12] M0-11 D 盘原子同步与交付核查

- 触发：M0-11 静态检查通过。
- 同步方向：正式工程 `competition_project_single_camera/` → `D:\TJ375N529_SC431HAI2LCD_Demo_V3/`。
- D 盘写入文件（仅 3 个）：
  - `src/top.v`：同步前 `7F6537C5F9544709E66997B685361BC81D25236A6C6C6A166711A8BD4CA0828D`；同步后与正式工程一致为 `1CBEA522BE4ADAFC19B96C87F2AEFA3F75832DC54770AA564DD37AE6FF6DDBF3`。
  - `src/debayer/debayer_top_2to1.v`：同步前 `51D68128153104DCEC1FFA1EBDE35B53A141DCBD4DA478F0D7049E6B92D021C2`；同步后与正式工程一致为 `400CC2CE0FD224D7B1B151944B05CDD4AEC7E976D33D61010409747EAE5E95DC`。
  - `src/debayer/raw_to_rgb.v`：同步前 `F58EAE1405ED86DD747D1E52E03FBA1D08213307FD70435D3311EDBC37A4CCEF`；同步后与正式工程一致为 `0AD7F89132DB7B2011401806CAB0D510B0349A509F83F16B427012E34C788A2B`。
- 保持同步的前置白平衡文件：`src/uvc_src/white_balance.v`，正式工程/D 盘 SHA-256 均为 `A8DF8324CC84BCE4BEE9A4C7E37C58EDA63A51F93DD7B7D335FEDC958B89F199`。
- 未写入：`mem_test.xml`、`.peri.xml`、`constrain.sdc`、任何 IP 设置、`outflow/`、`work_*`、bitstream 或其它 D 盘文件。
- 结果：`SYNC PASS / EFINITY AND BOARD NOT VERIFIED`。
- 用户执行请求：打开 `D:\TJ375N529_SC431HAI2LCD_Demo_V3\mem_test.xml`，完整综合/PNR/bitstream 后烧录；用左蓝右红和白色物块复测。反馈 Map/PNR/bitstream 结果及截图。
- 下一步门禁：若颜色仍不正确，按 M0-11 的单参数翻转顺序处理，先 `CH0_BAYER_SWAP_PIXELS`，后 `CH0_BAYER_ROW_SWAP`；不得同时改两项。

### [M0-13] 补光对照量化、绿色偏强显示校正与暗场结论

- 触发：用户提供两个新画面：未补光暗场和补光画面；用户指出补光后蓝色物块看起来像绿色。
- 用户现场反馈：
  - 暗场截图：`C:\Users\20306\AppData\Local\Temp\codex-clipboard-3c610082-9ed7-4d04-bf1c-42fc959dde8e.png`。
  - 补光截图：`C:\Users\20306\Pictures\Screenshots\屏幕截图 2026-07-14 192101.png`。
- 工作区状态确认：
  - 用户确认正式工程 `src/uvc_src/white_balance.v` 的新版本是有意修改。
  - 该修改仅增加 `pixel_cnt==0` 时的平均值除零防护；当前 `top.v` 的 HDMI 默认旁路白平衡，因此它不是本次补光绿偏的直接原因，予以保留并同步。
- 截图量化（每个物块中心区域下采样平均 RGB）：
  - 未补光蓝块约 `(8.7, 21.3, 13.6)`，红块约 `(15.2, 12.5, 6.9)`；整体信号很低，FPGA 数字提亮只能放大噪声，不能替代补光/传感器曝光。
  - 补光蓝块约 `(94.3, 206.0, 142.3)`，红块约 `(202.4, 127.6, 83.7)`。
  - 此结果表明 R/B 不再发生整体交换：红块仍为 R 主导；蓝块是 G 过强、B 不足，故偏青绿。Bayer 相位修复保持有效，当前问题转为颜色增益与亮度校正。
- 传感器寄存器审计：当前 ROM 含 `3e00/3e01/3e02`（曝光）和 `3e16..3e19`（增益相关）写入；未取得可验证的 SC431HAI 寄存器手册，故本轮不盲改 I2C 曝光/模拟增益。
- 修改文件：
  - `src/top.v`。
- 实际修改：
  - 在 Debayer 后、HDMI 前新增固定点显示校正函数；不接入 MIPI、DDR、帧缓存、Debayer、DSI 或 HDMI 时钟。
  - 默认启用 `HDMI_COLOR_CORRECTION_EN=1'b1`；对每个 RGB 像素进行饱和校正：`R x 300/256 + 12`、`G x 141/256 + 12`、`B x 320/256 + 12`。
  - 补光截图预测：蓝块约从 `(94,206,142)` 调整为 `(122,125,189)`；红块约从 `(202,128,84)` 调整为 `(249,82,116)`。
  - 暗场蓝块预测仅约 `(22,23,29)`；因此现场小闭环的硬件前提是固定、稳定、漫反射的白色补光，不能承诺无补光可用。
- 静态验证：
  - `PASS`：固定点函数有 8 位饱和输出，避免溢出回绕。
  - `PASS`：两个像素的六个字节均为 R/G/B 对应增益，字节顺序未改变。
  - `PASS`：默认 HDMI 路径为 Debayer→BGR/RGB 重排→显示校正→HDMI；白平衡仍保留但默认旁路。
  - `PASS`：`mem_test.xml` 已引用 `src/top.v` 与 `src/uvc_src/white_balance.v`，无需改 XML。
  - `NOT VERIFIED`：本机无 `iverilog`、`verilator`、`vlog`，未做 HDL 仿真；Efinity 与板级验证待用户执行。
- D 盘写入：待本条后的哈希同步记录。
- 下一步门禁：在固定补光下，用蓝、红、白物块复测。若仍有轻微色偏，只调 `HDMI_R_GAIN_Q8` / `HDMI_G_GAIN_Q8` / `HDMI_B_GAIN_Q8`，不改变 Bayer、MIPI 或 DDR；暗场问题以补光治具为解决边界，后续若需要再依据 SC431 手册单独审计曝光寄存器。

### [M0-14] M0-13 D 盘同步与最终一致性核查

- 触发：M0-13 静态验证通过。
- 同步方向：`competition_project_single_camera/` → `D:\TJ375N529_SC431HAI2LCD_Demo_V3/`。
- D 盘写入文件（仅 2 个）：
  - `src/top.v`：同步前 `1CBEA522BE4ADAFC19B96C87F2AEFA3F75832DC54770AA564DD37AE6FF6DDBF3`；同步后仓库/D 盘一致为 `303F52D839D07458B128D71AAD44911F0EA48FD08F8F30EECC1CB2F3C56B693E`。
  - `src/uvc_src/white_balance.v`：同步前 `A8DF8324CC84BCE4BEE9A4C7E37C58EDA63A51F93DD7B7D335FEDC958B89F199`；同步后仓库/D 盘一致为 `44CBE404F18602C7DFE07956CC65F714144E8FA091F0D710917BAB527EE56567`。
- 未写入：`mem_test.xml`、`.peri.xml`、`constrain.sdc`、IP、`outflow/`、`work_*`、bitstream、MIPI I2C ROM或其它 D 盘文件。
- 结果：`SYNC PASS / EFINITY AND BOARD NOT VERIFIED`。
- 用户执行请求：在固定白色补光下，使用 `D:\TJ375N529_SC431HAI2LCD_Demo_V3\mem_test.xml` 完成完整 Efinity flow 后烧录；用蓝、红、白物块复测并反馈 Map/PNR/bitstream 结果与截图。

### [M0-15] 暗场根因复核与 HDMI 低照度 Gamma 提亮

- 触发：用户反馈烧录 M0-13 后画面依旧很暗，并提供截图 `C:\Users\20306\AppData\Local\Temp\codex-clipboard-d71e57ec-610a-4e80-be9c-5461ffb17911.png`。
- 当前源码基线：后续修改继续基于 `competition_project_single_camera/`，每次核查通过后同步至 `D:\TJ375N529_SC431HAI2LCD_Demo_V3/` 供用户烧录。
- 现场图像量化：
  - 蓝物块区域均值约 `(24.6,20.0,31.5)`，最大值约 `39`。
  - 红物块区域均值约 `(34.8,15.4,24.4)`，最大值约 `45`。
  - 背景均值约 `(19.6,12.2,19.0)`，最大值约 `24`。
  - 结论：画面并非 HDMI 丢失或 RGB 通道错误，Raw/Debayer 后的有效显示量级本身接近黑位；M0-13 的 `+12` 黑位抬升无法显著改善此量级。
- 传感器 ROM 审计：
  - 用户指定以正式工程 ROM 为准。
  - 正式工程与 D 盘 ROM 的有效寄存器写入逐条相同，`3e00..3e19` 曝光/增益相关值相同；SHA-256 差异仅为 LF/CRLF 行尾格式。
  - 未取得可验证的 SC431HAI 寄存器手册，故本轮不盲改 `3e00..3e19`，避免破坏帧时序、曝光上限或引入噪声/花屏。
- 修改文件：
  - `src/top.v`。
- 实际修改：
  - 在既有 Debayer→BGR/RGB 重排→固定色彩校正之后、HDMI 之前新增 `hdmi_low_light_gamma`。
  - 默认 `HDMI_LOW_LIGHT_GAMMA_EN=1'b1`；采用单调四段固定点 gamma：`0..31 -> 0..87`、`32..63 -> 90..126`、`64..127 -> 128..180`、`128..255 -> 181..254`。
  - 六个 RGB 字节均经过同一 gamma，保持颜色通道关系；不改变 MIPI、I2C、RAW10、帧缓存、Debayer、DSI、HDMI 时钟、SDC、XML、IP 或端口。
  - 根据当前截图，预测蓝物块约从 `(25,20,32)` 提升到 `(99,64,112)`，红物块约提升到 `(113,56,101)`；背景也会提升到约 `(92,50,93)`，这是显示侧提亮的已知取舍。
- 静态验证：
  - `PASS`：gamma 开关、四段分支、6 个 RGB 字节连接和 HDMI 选择路径存在。
  - `PASS`：边界输出 `0->0`、`31->87`、`32->90`、`63->126`、`64->128`、`127->180`、`128->181`、`255->254`，单调且无 8 位溢出回绕。
  - `PASS`：`mem_test.xml` 已引用 `src/top.v`，无需修改工程 XML。
  - `NOT VERIFIED`：本机无 HDL 编译器；Efinity Map/PNR/bitstream 与板级结果待用户验证。
- D 盘写入：待本条后的哈希同步记录。
- 下一步门禁：先验证本版 Gamma。若仍无法满足识别亮度，下一步必须获取 SC431HAI 手册或可读回的传感器寄存器证据后，单独制定并审查曝光/模拟增益修改；不得继续无证据猜写 I2C 寄存器。

### [M0-16] M0-15 D 盘同步与颜色链一致性核查

- 触发：M0-15 静态检查通过。
- 同步方向：`competition_project_single_camera/src/top.v` → `D:\TJ375N529_SC431HAI2LCD_Demo_V3\src/top.v`。
- D 盘写入：仅 `src/top.v`。
  - 同步前 SHA-256：`303F52D839D07458B128D71AAD44911F0EA48FD08F8F30EECC1CB2F3C56B693E`。
  - 同步后仓库/D 盘一致 SHA-256：`BDDEAEF8EAD672105F985552E819242DC2AB82E6D06F238933A03BAEA7FBEF7A`。
- 一致性核查：当前 `top.v`、`debayer_top_2to1.v`、`raw_to_rgb.v`、`white_balance.v` 均已核对仓库/D 盘 SHA-256 一致。
- 未写入：传感器 I2C ROM、`mem_test.xml`、`.peri.xml`、`constrain.sdc`、IP、`outflow/`、`work_*` 或 bitstream。
- 结果：`SYNC PASS / EFINITY AND BOARD NOT VERIFIED`。
- 用户执行请求：以 D 盘工程完整综合、烧录，确认 Gamma 是否将暗部提升到可辨识水平；同时反馈 Map/PNR/bitstream 与补光状态。若画面仍不可用，停止继续增加显示侧增益，转入“取得 SC431 曝光寄存器证据后再改 I2C”的受控步骤。

### [M0-17] 用户现场反馈：补光对照仍异常，撤回显示侧拟合处理

- 触发：用户提供新的 HDMI 截图：未补光 `C:\Users\20306\Pictures\Screenshots\屏幕截图 2026-07-14 200709.png`，手动补光 `C:\Users\20306\Pictures\Screenshots\屏幕截图 2026-07-14 200743.png`，反馈画面仍然异常。
- 目标：停止对拍屏样本做固定 RGB 增益和 Gamma 拟合，先恢复可追溯的中性 HDMI 基线，再分离照明/曝光与 Bayer 问题。
- 用户现场反馈与判断：两张图均呈现全局偏紫灰和与补光不一致的显示效果。此前新增的 `R x1.17 + 12`、`G x0.55 + 12`、`B x1.25 + 12` 与分段低照度 Gamma 不是传感器标定结果，不能作为颜色修复方案；继续微调只会加重异常。
- 实际动作：已从正式工程 `src/top.v` 撤除上述固定颜色校正与 Gamma，恢复为 `Debayer -> 显式 BGR 到 RGB 重排 -> HDMI`。保留已验证的 RAW10 抽取、Bayer 像素/行相位参数和 HDMI 旁路白平衡设置。
- 未修改：`mem_test.xml`、`.peri.xml`、`constrain.sdc`、IP、MIPI 时钟、DDR、HDMI 时序、传感器 I2C ROM、`outflow/`、`work_*` 和 bitstream。
- 静态验证：正式工程 `src/top.v` 不含 `HDMI_COLOR_CORRECTION_EN`、`HDMI_LOW_LIGHT_GAMMA_EN`、`hdmi_color_correct` 或 `hdmi_low_light_gamma`；`CH0_BAYER_SWAP_PIXELS`、`CH0_BAYER_ROW_SWAP`、`HDMI_BYPASS_WHITE_BALANCE` 和 `rgb0_data_rgb` 路径仍在。
- 同步前基线：正式工程 `src/top.v` SHA-256 为 `0F869D63507683A061CF507AEEA0733FD3DA8B8C4F9306A3F8FECE84171522EC`；D 盘同名文件仍为含 Gamma 版本，SHA-256 为 `BDDEAEF8EAD672105F985552E819242DC2AB82E6D06F238933A03BAEA7FBEF7A`。
- NOT VERIFIED：Efinity Map/PNR/bitstream、上板图像、SC431HAI 曝光/模拟增益寄存器语义。本机未发现可用 HDL 仿真/编译器，未声明 HDL 仿真通过。
- 下一步门禁：将当前正式工程 `src/top.v` 同步到 D 盘并校验哈希。用户用 D 盘工程重新完整综合、烧录后，在固定补光下回传红/蓝/白物块对照、Map/PNR/bitstream 结果。若中性基线仍明显偏色或过暗，先取得 SC431HAI 寄存器手册或可读回寄存器证据，再单独审查 I2C 曝光试验。

### [M0-18] M0-17 D 盘同步与中性颜色基线核查

- 触发：M0-17 正式工程显示侧固定颜色校正和低照度 Gamma 已撤除。
- 同步方向：`competition_project_single_camera/src/top.v` -> `D:\TJ375N529_SC431HAI2LCD_Demo_V3\src\top.v`。
- D 盘写入：仅 `src/top.v`。
  - 同步前 SHA-256：`BDDEAEF8EAD672105F985552E819242DC2AB82E6D06F238933A03BAEA7FBEF7A`。
  - 正式工程源 SHA-256：`0F869D63507683A061CF507AEEA0733FD3DA8B8C4F9306A3F8FECE84171522EC`。
  - 同步后 D 盘 SHA-256：`0F869D63507683A061CF507AEEA0733FD3DA8B8C4F9306A3F8FECE84171522EC`，与正式工程一致。
- 同步后静态核查：D 盘 `src/top.v` 无 `HDMI_COLOR_CORRECTION_EN`、`HDMI_LOW_LIGHT_GAMMA_EN`、`hdmi_color_correct` 或 `hdmi_low_light_gamma` 残留；保留 `CH0_BAYER_SWAP_PIXELS=0`、`CH0_BAYER_ROW_SWAP=1`、`HDMI_BYPASS_WHITE_BALANCE=1` 和 `rgb0_data_rgb` HDMI 数据路径。
- 未写入：`mem_test.xml`、`.peri.xml`、`constrain.sdc`、IP、传感器 I2C ROM、`outflow/`、`work_*`、bitstream 或其他 D 盘文件。
- 结果：`SYNC PASS / EFINITY AND BOARD NOT VERIFIED`。
- 用户执行请求：使用 `D:\TJ375N529_SC431HAI2LCD_Demo_V3\mem_test.xml` 重新完整综合、PNR、生成 bitstream 并烧录。固定同一补光条件，拍摄红、蓝、白物块对照并回传画面，以及 Map/PNR/bitstream 结果。此版本是后续传感器曝光调查的唯一中性显示基线。

### [M0-19] 用户现场反馈：暗场仍严重且 PNR 时序失败，隔离白平衡除法路径

- 触发：用户回传未补光 HDMI 图 `C:\Users\20306\Pictures\Screenshots\屏幕截图 2026-07-14 202643.png` 和 Efinity Timing 截图，显示 WNS `-34.08 ns`、WHS `0.014 ns`。
- 用户现场反馈与图像判断：不补光画面接近黑位，只依靠 HDMI 后端数字提亮不能生成有效的信号；后续必须基于 SC431HAI 寄存器证据审查曝光/增益，不再重新添加拍屏拟合的 RGB 增益或 Gamma。
- 时序证据：`D:\TJ375N529_SC431HAI2LCD_Demo_V3\outflow\mem_test.timing.rpt` 时间为 2026-07-14 20:25:20。最差路径为 `u0_white_balance/pixel_cnt[22]~FF|CLK -> u0_white_balance/gain_b[8]~FF|D`，同域 `i_sysclk_div2` 约束为 `14.286 ns`，数据路径为 `49.246 ns`、`210` 个逻辑级，因此 WNS 为 `-34.080 ns`。这是硬时序失败，不可以用约束屏蔽或标记为通过。
- 根因分析：HDMI 已经旁路白平衡，但 `u0_white_balance` 仍被 MIPI DSI 实例使用。其每帧求均值和增益更新含运行时除法，导致本次最差路径。直接删除 DSI 不安全：当前 `dsi_tx_top` 还产生 `pixel_data_en`，该信号是既有摄像头/帧缓存复位释放链的一部分。
- 实际修改：在正式工程 `src/top.v` 中移除两路 `white_balance` 实例与所有 `wb*` 网线。保留 `dsi_tx_top` 以保持 `pixel_data_en` 启动链，其像素输入改为 ch0 Debayer 后已显式重排的 `rgb0_data_rgb`。HDMI 仍使用同一 ch0 RGB。
- 静态验证：`src/top.v` 无 `white_balance` 或 `wb0_`/`wb1_` 引用；HDMI 与 DSI 像素输入为 `rgb_hs/rgb_vs/rgb_de/rgb0_data_rgb`。`git diff --check` 通过。`white_balance.v` 仍保留在工程中，但无顶层实例，新综合将其排除。
- 未修改：`white_balance.v`、MIPI RX、RAW10 抽取、DDR、Debayer、HDMI 时钟、`mem_test.xml`、`.peri.xml`、`constrain.sdc`、IP、I2C ROM、`outflow/`、`work_*` 和 bitstream。
- NOT VERIFIED：新 Efinity Map/PNR/bitstream、新 WNS/WHS、板级 HDMI 图像。旧 timing report 仍是修改前产物，不能用来推断本次修改后的时序。
- 下一步门禁：同步正式工程 `src/top.v` 至 D 盘，然后由用户完整重新综合、PNR、烧录。必须回传新的 WNS/WHS、最差路径以及 HDMI 画面；只有 WNS >= 0 才可进入下一步图像调试。

### [M0-20] M0-19 D 盘同步与时序修复前完整性核查

- 触发：M0-19 顶层白平衡实例已移除，需要发布到 D 盘给用户重新 PNR。
- 同步方向：`competition_project_single_camera/src/top.v` -> `D:\TJ375N529_SC431HAI2LCD_Demo_V3\src\top.v`。
- D 盘写入：仅 `src/top.v`。
  - 同步前 SHA-256：`0F869D63507683A061CF507AEEA0733FD3DA8B8C4F9306A3F8FECE84171522EC`。
  - 正式工程源 SHA-256：`11349BEB991BD43E3AC5AD6BAB0A9162CA156772ED97C0CA9D5F1F25B0B0B95E`。
  - 同步后 D 盘 SHA-256：`11349BEB991BD43E3AC5AD6BAB0A9162CA156772ED97C0CA9D5F1F25B0B0B95E`，与正式工程一致。
- 关键文件一致性：`top.v`、`debayer_top_2to1.v`、`raw_to_rgb.v`、`white_balance.v` 在正式工程/D 盘的 SHA-256 均一致。D 盘 `top.v` 中无 `white_balance` 或 `wb0_`/`wb1_` 引用。
- 未写入：`mem_test.xml`、`.peri.xml`、`constrain.sdc`、IP、I2C ROM、`outflow/`、`work_*`、bitstream 或其他 D 盘文件。
- 结果：`SYNC PASS / NEW PNR AND BOARD NOT VERIFIED`。
- 用户执行请求：使用 `D:\TJ375N529_SC431HAI2LCD_Demo_V3\mem_test.xml` 重新完整综合、PNR、生成 bitstream 并烧录。首先回传 WNS/WHS 和最差路径；必须确认 `u0_white_balance` 不再出现于时序报告。再回传 HDMI 画面。

### [M0-21] 新 PNR 反馈：白平衡路径消失，剩余 DSI/CSI 跨时钟约束问题

- 触发：用户回传新 Timing 截图和未补光 HDMI 图。Timing 显示 WNS `-1.308 ns`、WHS `0.013 ns`；画面仍接近黑位。
- 构建证据：`D:\TJ375N529_SC431HAI2LCD_Demo_V3\outflow\mem_test.timing.rpt` 生成于 2026-07-14 20:38:51，Efinity `2025.2.288.4.15`；bitstream 生成于 20:39:08。
- 上一轮修复验证：新 timing report 中 `u0_white_balance` 和 `u1_white_balance` 引用数均为 0，原 `-34.080 ns` 的除法路径已消失。CDC 报告为 `No Synchronizer warnings to report`。
- 当前最差路径：`dsi_tx_top_inst1/w_confdone~FF|CLK -> dsi_tx_top_inst1/dly_cnt[25]~FF|CE`，Launch Clock 为 `mipi_clk`，Capture Clock 为 `i_sysclk_div2`，约束仅 `0.002 ns`，Slack `-1.308 ns`。报告还存在 DSI PHY 的 `mipi_dphy_tx_SLOWCLK -> mipi_dphy_tx_FASTCLK_D` 负路径。
- 后续路径预查：删除 DSI 后，timing report 中仍可见 `mipi_rx_ck0_CLKOUT` / `mipi_rx_ck1_CLKOUT` 与 `i_sysclk_div2` 之间的负路径；起终点位于 CSI IP 的异步 FIFO、Gray 指针同步和复位同步结构。现有 SDC 使用旧别名 `dphy_byte_clk` / `mipi_dphy_rx_inst1_byte_clk`，没有覆盖 STA 实际报告的两个 MIPI RX 时钟名。
- 图像判断：移除白平衡后画面亮度几乎不变，进一步证明暗场不是白平衡或 HDMI 后处理造成。当前不修改 SC431HAI 曝光/增益寄存器，因为本地资料未找到可验证的寄存器手册。
- 结论：WNS 虽大幅改善但仍为硬失败；必须先清除不需要的 DSI 活动逻辑，并修正 CSI 异步时钟组的约束对象，不能用旧 bitstream 继续图像功能开发。

### [M0-22] HDMI-only 启动解耦、DSI 禁用与 CSI 时钟组修正

- 目标：在不改变 ch0 摄像头、DDR、Debayer 和 HDMI 像素链的前提下，移除不使用的 DSI 对启动和时序的影响，并让 STA 正确识别 CSI IP 的异步时钟关系。
- 修改文件：
  - `src/top.v`
  - `constrain.sdc`
- `src/top.v` 修改：
  - 顶层 `arst_n` 不再依赖已不用的 `MIPI_TX_PLL_LOCKED`，保留 `sys_pll_lock & ddr_pll_lock & pll_byteclk_locked`。
  - 新增 `pixel_reset_sync` 两级同步器，将 `i_fb_clk` 域产生的 `ddr_cfg_ok/sys_rst_n` 在 `i_sysclk_div2` 域异步拉低、同步释放。
  - 新增 `pixel_enable_cnt`，在同步释放后保留原 DSI 启动链约 1 秒的等待，再产生 `pixel_data_en`。
  - 使用未定义的 `MIPI_DSI_OUT_EN` 宏隔离原 `reset`、`color_bar_rgb` 和 `dsi_tx_top` 实例；正式 HDMI-only 构建不会综合这些逻辑。
  - 两组共 70 个 MIPI TX 物理输出全部显式置于 `OE=0`、数据为 0、复位有效；`P1_lcd_power_en=0`。
  - `white_balance` / `wb0_` / `wb1_` 顶层引用仍为 0，HDMI 保持 `rgb_hs/rgb_vs/rgb_de/rgb0_data_rgb` 直连。
- `constrain.sdc` 修改：在创建 `mipi_rx_ck0_CLKOUT` 和 `mipi_rx_ck1_CLKOUT` 后，新增 `set_clock_groups -asynchronous -group {i_sysclk_div2} -group {mipi_rx_ck0_CLKOUT} -group {mipi_rx_ck1_CLKOUT}`。该约束使用 STA 真实时钟名，只隔离 CSI IP 已有异步 FIFO/复位同步跨域，不放宽任何同域路径。
- 静态验证：
  - `PASS`：MIPI_DSI_OUT_EN 未定义，DSI 活动实例处于预处理禁用分支。
  - `PASS`：70 个 DSI TX 输出均有唯一的禁用分支赋值。
  - `PASS`：`pixel_data_en` 只由本地 `i_sysclk_div2` 计数器产生。
  - `PASS`：SDC 的实际 MIPI RX 时钟在创建后再进入异步组。
  - `PASS`：`git diff --check`。
  - `NOT VERIFIED`：本机无 `iverilog`、`verilator` 或 `vlog`；未完成新 Map/PNR/bitstream 和板级 HDMI 验证。
- 未修改：MIPI CSI RX RTL/IP、RAW10 抽取、DDR、Debayer、HDMI 时钟和数据链、传感器 I2C ROM、`mem_test.xml`、`.peri.xml`、IP settings、`outflow/`、`work_*` 和 bitstream。
- 下一步门禁：同步 `src/top.v` 与 `constrain.sdc` 到 D 盘，完整重跑 Efinity。必须核查 WNS/WHS、Clock Relationship Summary、CDC 报告，以及报告中 `dsi_tx_top_inst1` 是否完全消失。WNS >= 0 且 HDMI 画面保持后，才能进入 SC431HAI 曝光资料审计和受控曝光试验。

### [M0-23] M0-22 D 盘同步与发布核查

- 同步方向：`competition_project_single_camera/` -> `D:\TJ375N529_SC431HAI2LCD_Demo_V3/`。
- D 盘写入文件：
  - `src/top.v`：同步前 SHA-256 `11349BEB991BD43E3AC5AD6BAB0A9162CA156772ED97C0CA9D5F1F25B0B0B95E`；同步后正式工程/D 盘一致为 `E0F6509169545B16E4146618186929C280DD354EEB6ADD1C2E03FE382E9E7AD1`。
  - `constrain.sdc`：同步前 SHA-256 `7224DE3FFCC6B05A82DDDD27792CF2DC245DCFA254E324D07A796E534CBDDD87`；同步后正式工程/D 盘一致为 `B6E30866ED09CADCA083FCE4D7A2D831A90E3C0E689CBB4D3B97369874AE316D`。
- D 盘同步后核查：`MIPI_DSI_OUT_EN` 定义数为 0；`white_balance` / `wb0_` / `wb1_` 顶层引用数为 0；本地 `pixel_data_en`、DSI 禁用输出和真实 MIPI RX 时钟异步组均存在。
- 未写入：`mem_test.xml`、`.peri.xml`、IP、MIPI CSI RX RTL、I2C ROM、`outflow/`、`work_*`、bitstream 或其他 D 盘文件。
- 结果：`SYNC PASS / NEW MAP-PNR-BOARD NOT VERIFIED`。
- 用户执行请求：完整重跑 D 盘 Efinity flow，不能只复用旧 route/bitstream。回传新的 Timing 总览、Clock Relationship Summary、CDC 报告和 HDMI 画面；确认 `dsi_tx_top_inst1` 不再出现在 map/timing 报告中。

### [M0-24] 用户现场反馈：五色正方体仍全部压在暗部

- 触发：用户回传最新 HDMI 画面并确认画面中放置了白、黑、红、蓝、黄五个正方体。
- 对应构建证据：
  - Timing 报告：`D:\TJ375N529_SC431HAI2LCD_Demo_V3\outflow\mem_test.timing.rpt`，生成时间 `2026-07-14 20:57:32`。
  - bitstream：`D:\TJ375N529_SC431HAI2LCD_Demo_V3\outflow\mem_test.bit`，SHA-256 `F799DA980C427F2869E808A4DFA7C64272B9EEFF6FBDCA1F6BF5DABAD96B2D12`。
  - Timing：WNS `-0.741 ns`，WHS `0.026 ns`；`dsi_tx_top_inst1` 和顶层 `white_balance` 引用均已消失；CDC 报告为 `No Synchronizer warnings to report`。
- 图像量化：最新截图全图 RGB 均值约为 `(10.52, 16.70, 8.14)`，RGB p99 约为 `(37, 64, 32)`，最大值约为 `(42, 68, 37)`。白色和黄色样本也被压入暗部，当前画面不具备可靠五色识别所需的动态范围。
- 附加现象：画面存在明显横向重影/分块，不能把问题仅归因为传感器曝光或像素数；本轮先做传感器增益单变量试验，重影在亮度恢复后单独定位。
- 结论：`BOARD IMAGE FAIL / FIVE-COLOR SEPARATION NOT AVAILABLE`。不得将当前画面描述为五色识别链路已闭环。

### [M0-25] SC431HAI 约 2.98 倍模拟增益试验与 mipi_clk 复位域修复

- 目标：不改动 ch0、RAW10 抽取、DDR、Debayer、HDMI 像素链、曝光时间和帧率，分别处理暗场输入幅度不足与当前 WNS 最差复位跨域路径。
- 修改文件：
  - `src/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_ctrl_top.v`
  - `src/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_reg_rom.v`
  - `src/mipi_csi/soft_mipi_rx_top.v`
- 传感器寄存器证据：Rockchip Linux `drivers/media/i2c/sc431hai.c`，固定 commit `b4ef083dc0c3608e744deabb43dc6b781aadbe6e`。公开驱动定义 `0x3e06/0x3e07` 为数字粗/细增益，`0x3e08/0x3e09` 为模拟粗/细增益；其分段公式证明 `{0x3e08,0x3e09}={0x80,0x3e}` 对应约 `2.98x` 模拟增益。
- 增益修改：
  - `DATA_LENGTH` 从 `161` 增至 `165`。
  - 追加 `3e06=00`、`3e07=80`，保持数字增益约 `1x`。
  - 追加 `3e08=80`、`3e09=3e`，设置模拟增益约 `2.98x`。
  - 保持原曝光寄存器 `3e00=00`、`3e01=ba`、`3e02=d0` 不变；该曝光值与公开驱动的 2560x1440/30fps 模式一致且已接近帧周期上限，因此本轮不继续延长曝光。
- ROM 完整性：共 `165` 项，索引从 `0x00` 连续到 `0xa4`；发送状态机使用 `cnt < DATA_LENGTH`，因此 `DATA_LENGTH=165` 会发送全部 165 项。
- 时序根因：最新最差路径由 `i_sysclk_div2` 域 `reset_pixel_n` 驱动 `mipi_clk` 域 `i2c_rst_cnt[*]` 异步复位端，形成 10 条 WNS `-0.741 ns` 路径。
- 时序修改：在 `soft_mipi_rx_top.v` 中新增由 `arst_n` 输入、在 `mipi_clk` 域同步释放的 `reset_mipi_n`，仅将 `i2c_rst_cnt` 的异步复位由 `reset_pixel_n` 改为 `reset_mipi_n`。两个 CSI 模块实例都会获得各自本地 `mipi_clk` 域复位。
- 保持不变：`src/top.v`、`constrain.sdc`、`mem_test.xml`、`.peri.xml`、IP settings、RAW/DDR/Debayer/HDMI 数据链、曝光时间、帧率、HDMI Gamma 和固定 RGB 增益。
- 静态状态：源代码修改已完成；等待格式检查、D 盘同步和 SHA-256 核对。
- NOT VERIFIED：新 Map/PNR/bitstream、WNS/WHS、CDC、上板 HDMI 亮度、五色分离、白/黄色是否饱和、噪声水平和横向重影。
- 下一步门禁：同步三处源文件到 D 盘后，由用户完整重跑 Map/PNR/bitstream 并烧录。首先要求 WNS/WHS 均非负，再在相同机位、相同环境和相同五色摆放下拍摄对照图。

### [M0-26] M0-25 D 盘同步与发布核查

- 同步方向：`competition_project_single_camera/` -> `D:\TJ375N529_SC431HAI2LCD_Demo_V3/`。
- D 盘写入文件与 SHA-256：
  - `src/mipi_csi/soft_mipi_rx_top.v`：同步前 `B7D8DD592F24B15F55DE5D6EDCA34A109B0E45F631F1A8E9AC58A5DA94BFC038`；同步后正式工程/D 盘一致为 `0EAADE836681756416D63ED08805BD0235E4B2377FBC512B24F7C4E3320C5F70`。
  - `src/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_ctrl_top.v`：同步前 `FACC87B16657874BEDA8F1051649970A1C8A78E52F0224726904F30C1CFAD21F`；同步后正式工程/D 盘一致为 `234E58D7F5F1AA46CA9D14F044C9D2BA74EC42F55EBB8AB4E79A0F297B59B317`。
  - `src/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_reg_rom.v`：同步前 `DA82D962F02D3E585C97BF81EA057A9E0E9766915449960F98EF6C903B695928`；同步后正式工程/D 盘一致为 `3762EA124998A7CDC5B3C58285AECE0C52B264F05E168FB80F78EB734F20983C`。
- 发布前静态核查：`git diff --check` 通过；ROM 条目数 `165`、索引 `0..164` 连续无缺口；临时公开驱动副本 `_tmp_sc431hai.c` 已删除，未纳入仓库。
- 未写入：`src/top.v`、`constrain.sdc`、`mem_test.xml`、`.peri.xml`、IP settings、`outflow/`、`work_*`、bitstream 和其他 D 盘文件。
- 结果：`SOURCE SYNC PASS / NEW MAP-PNR-BITSTREAM-BOARD NOT VERIFIED`。
- 用户执行请求：从 `D:\TJ375N529_SC431HAI2LCD_Demo_V3\mem_test.xml` 完整重跑 Map、PNR 和 bitstream 后烧录。回传 WNS、WHS、新最差路径、CDC 报告，以及相同位置、相同环境下包含白/黑/红/蓝/黄五个正方体的 HDMI 画面。优先确认 WNS/WHS 均非负，再判断亮度、五色分离、饱和、噪声和横向重影。

### [M0-27] 用户现场反馈：复位域时序修复通过，增益有效但五色画面仍未通过

- 触发：用户完成新一轮完整构建、烧录并回传 Timing 截图和 Windows 直接 HDMI 画面截图 `屏幕截图 2026-07-14 214056.png`。
- 构建证据：
  - Timing 报告：`D:\TJ375N529_SC431HAI2LCD_Demo_V3\outflow\mem_test.timing.rpt`，生成时间 `2026-07-14 21:39:18`。
  - bitstream：`D:\TJ375N529_SC431HAI2LCD_Demo_V3\outflow\mem_test.bit`，生成时间 `2026-07-14 21:39:36`，SHA-256 `0A69113CB3AAC7EB1DF0E6FDFF9964C98FDF9B8E59CE48848C093FB1AA7BE8CC`。
  - WNS `+1.674 ns`，WHS `+0.026 ns`；原 `reset_pixel_n -> i2c_rst_cnt[*]` 负路径不再是失败路径。
  - CDC 报告：`No Synchronizer warnings to report`。
- 时序结论：`TIMING PASS / CDC REPORT PASS`。M0-25 的 `mipi_clk` 域复位修复已由本次真实 PNR 验证有效。
- 图像量化：
  - 整图 RGB 均值约 `(29.34, 46.65, 22.16)`，p99 约 `(104, 187, 91)`，最大值约 `(119, 198, 102)`；相比上一版亮度提升约 `2.5x..3x`，且没有通道达到 255，说明约 `2.98x` 模拟增益已经生效、当前未出现数字饱和。
  - 红色样本均值约 `(41.03, 33.30, 19.33)`；蓝色样本约 `(20.68, 44.86, 28.07)`。红块仍为 R 最大，蓝块仍满足 B 大于 R，因此不支持再次盲目交换 R/B 或 Bayer 横纵相位。
  - 白色样本均值约 `(73.48, 132.82, 61.91)`，`G/R≈1.81`、`G/B≈2.15`；绿色偏置仍明显，五色分类输入尚不合格。
  - 黑色样本均值约 `(6.93, 13.26, 6.16)`，黑色与其他样本已有亮度差，但中性色和蓝/黄仍受绿色偏置影响。
- 图像结构问题：Windows 直接截图中仍存在明显水平方向多重轮廓/拖影，排除“手机拍显示器”作为唯一原因。当前传感器曝光接近整帧，固定支架或物体轻微振动可能产生运动拖影；顶层双像素到 HDMI 的 2:1 拆分也缺少基于 `DE/HS/VS` 的显式相位重对齐。现有证据不足以把任一项定为唯一根因。
- 决策：
  - 保持模拟增益约 `2.98x`，不继续加增益，不追加 Gamma，不盲改 Bayer 相位。
  - 先要求摄像头刚性固定、五个方块完全静止后再取得一张 Windows 直接截图；若重影显著消失，优先缩短曝光并用适量增益补偿；若重影仍保持固定的多级像素偏移，再隔离审计帧缓存和双像素 HDMI 拆分。
  - 重影根因关闭后，再依据中性白/灰 ROI 设计低成本固定点白平衡；不得恢复带运行时除法的旧 `white_balance` 模块。
- 结果：`TIMING PASS / BRIGHTNESS IMPROVED / IMAGE QUALITY FAIL / FIVE-COLOR NOT VERIFIED`。
- 本轮工程写入：仅追加本工作日志；未修改 RTL、SDC、工程 XML、IP、D 盘源文件或 bitstream。

### [M0-28] 静止画面确认重影与 HDMI 输出前端改进

- 触发：用户回传摄像头和五色物块保持静止后的 Windows 直接截图 `codex-clipboard-08b530ba-8093-466d-9f54-7c602809bf26.png`，并要求继续改进。
- 图像复核：静止截图仍保持多级水平轮廓，排除物体移动、相机晃动和手机拍屏作为唯一原因；重影属于当前视频输出链的真实问题。
- 五色 ROI 均值：红约 `(44.10,35.18,20.65)`，黄约 `(61.76,102.34,33.20)`，白约 `(73.68,133.06,62.07)`，蓝约 `(21.74,49.20,31.33)`，黑约 `(6.62,12.55,5.70)`。
- 根因审计：`src/top.v` 原 HDMI 2:1 拆分使用自由翻转 `sel`，没有在每行消隐期根据 `DE/HS/VS` 重新锚定双像素相位。其启动相位依赖 70 MHz/140 MHz 相关时钟的启动关系，复位或相位扰动后可能交换像素对的第一/第二像素。
- 修改文件：`src/top.v`。
- HDMI 相位修改：删除独立的自由运行 `sel <= ~sel`；在 `hdmi_tx_slow_clk` 域检测 `hdmi0_de_out=0` 时将 `sel` 复位为 0，并在有效行内严格按原工程顺序输出低 24 位、再输出高 24 位。该修改不改变 2:1 时钟关系、TMDS 编码器或 HDMI 时序参数。
- 固定点白平衡：
  - 新增顶层参数 `HDMI_FIXED_WB_EN=1`，可单参数关闭。
  - 仅在 HDMI 显示支路使用 `R=1.75x`、`G=1x`、`B=2x`，采用移位加法与 8 位饱和裁剪，不使用乘法器或除法器。
  - 系数依据静止白块 `G/R≈1.81`、`G/B≈2.14`，保守取 1.75 和 2.0；白块预测由约 `(74,133,62)` 变为约 `(129,133,124)`。
  - 原始 `rgb0_data_rgb` 语义保持不变；传感器增益、曝光、CSI、RAW10、DDR、Debayer 和后续识别数据边界均未修改。
- 静态验证：`git diff --check` 通过；饱和边界手工/脚本核查通过；原低半字后高半字像素顺序保持；本机无 `iverilog`、`verilator` 或 `vlog`，未运行 RTL 编译。
- 风险：新增 10 位移位加法和饱和选择位于 `i_sysclk_div2 -> hdmi_tx_slow_clk` 相关时钟路径；上一版该关系 setup slack 为 `+6.228 ns`，但新 PNR 前不得推断仍然通过。行相位修复是明确稳健性修复，但尚不能宣称它是全部多级重影的唯一根因。
- 结果：`SOURCE PATCHED / STATIC CHECK PASS / MAP-PNR-BOARD NOT VERIFIED`。
- 下一步门禁：同步 `src/top.v` 到 D 盘后完整重跑 Map/PNR/bitstream。必须回传 WNS/WHS、`i_sysclk_div2 -> hdmi_tx_slow_clk` slack、CDC 结果和相同五色静止截图；分别判断重影和白平衡是否改善。

### [M0-29] M0-28 D 盘同步与发布核查

- 同步方向：`competition_project_single_camera/src/top.v` -> `D:\TJ375N529_SC431HAI2LCD_Demo_V3\src\top.v`。
- SHA-256：D 盘同步前 `E0F6509169545B16E4146618186929C280DD354EEB6ADD1C2E03FE382E9E7AD1`；同步后正式工程/D 盘一致为 `7A8111B36CE05F51B402BBB9424F53A6728B634F60E81EBE01DD8F279E89DD85`。
- 写入范围：仅 D 盘 `src/top.v`。未写入 `constrain.sdc`、`mem_test.xml`、`.peri.xml`、IP settings、传感器 I2C ROM、其他 RTL、`outflow/`、`work_*` 或 bitstream。
- 结果：`SOURCE SYNC PASS / NEW MAP-PNR-BITSTREAM-BOARD NOT VERIFIED`。
- 用户执行请求：使用 D 盘工程完整重跑 Map、PNR、bitstream 并烧录；旧 bitstream SHA-256 `0A69113CB3AAC7EB1DF0E6FDFF9964C98FDF9B8E59CE48848C093FB1AA7BE8CC` 不包含本次 HDMI 相位和固定点白平衡修改。

### [M0-30] 五色白平衡验收、下游色条排除与 framebuffer 诊断修复

- 触发：用户回传 M0-28 新版五色静止截图，并补充“FPGA 本地产生的标准色条没有重影”。
- 对应构建证据：
  - Timing 报告：`D:\TJ375N529_SC431HAI2LCD_Demo_V3\outflow\mem_test.timing.rpt`，生成时间 `2026-07-14 22:07:32`。
  - bitstream：`D:\TJ375N529_SC431HAI2LCD_Demo_V3\outflow\mem_test.bit`，生成时间 `2026-07-14 22:07:50`。
  - WNS `+1.731 ns`，WHS `+0.026 ns`；`i_sysclk_div2 -> hdmi_tx_slow_clk` setup slack `+4.507 ns`、hold slack `+0.121 ns`；CDC 为 `No Synchronizer warnings to report`。
- 五色量化：红约 `(89,41,57)`，黄约 `(125,112,86)`，白约 `(156,154,151)`，蓝约 `(56,60,85)`，黑约 `(24,17,29)`。白块通道已接近平衡，五色主通道关系正确，固定点白平衡和约 `2.98x` 模拟增益在本轮冻结，不再继续调系数。
- 关键边界：同一 HDMI/TMDS/线缆/采集卡链路的 FPGA 本地标准色条无重影，因此水平重影不在 HDMI 编码器、TMDS 物理输出、线缆、采集卡或显示器。排查范围收缩到真实摄像头数据路径的 `摄像头/CSI -> RAW 打包 -> DDR/framebuffer -> Debayer`。
- CBM 状态：按新 `AGENTS.md` 请求查询 `D-cicc_cbm-main`，服务返回 `project not found or not indexed / No projects indexed yet`。本轮停止依赖图谱，改用候选工程真实 RTL 和真实构建报告做最小范围核查；未重建图谱。
- 发现的确定性 RTL 缺陷：`src/framebuffer/frame_info_det.v` 原先在帧结束时执行 `frame_len_d0 <= frame_end_r0`，把 1 bit 帧结束脉冲写入 24 bit 帧长度历史，导致 `frame_stable` 比较基本失去实际帧长度检测能力。坏帧、短帧或长度波动可能仍被错误标记为稳定并进入 DDR/framebuffer。
- 修改文件：
  - `src/framebuffer/frame_info_det.v`
  - `src/framebuffer/frame_buffer.v`
  - `src/top.v`
- 帧长度门修复：
  - `frame_len_d0` 改为锁存真实 `frame_pix_num`。
  - 仅在连续两个非零帧长度相等时置 `frame_stable=1`。
  - `total_frame_bytes` 复位为 0，并统一使用非阻塞赋值。
- 只读可观测性：`frame_buffer` 导出 `o_frame_stable` 和既有 `data_tx.fifo_rd_underflow`；ch0 映射为 LED20/F3=`frame_stable`、LED21/F2=`fifo_rd_underflow` 锁存。LED21 一旦出现欠流保持点亮，直到重新配置/复位。
- DDR 前诊断源：
  - SW4/V19 为低有效诊断输入。松开保持真实摄像头画面；按住时仅将 ch0 RAW8 payload 替换为四像素同值的灰度棋盘。
  - CSI 的 VS/DE 帧控制、DDR/framebuffer、Debayer、固定点白平衡、HDMI/TMDS 全部仍参与，因此它不同于已知干净的 HDMI 下游标准色条。
  - 若按住 SW4 的 RAW 棋盘也重影，根因在 DDR/framebuffer 或其后至 Debayer；若 RAW 棋盘干净而真实画面重影，根因在摄像头/CSI/RAW 数据侧。
- 保持不变：传感器 ROM、曝光、模拟/数字增益、Bayer 相位、白平衡系数、HDMI 相位修复、SDC、`mem_test.xml`、`.peri.xml` 和 IP settings。公开 Rockchip 驱动显示模式寄存器应在停流时配置后再启流，但为保持本轮变量隔离，尚未修改当前 Demo 的启流顺序。
- 静态验证：`git diff --check` 通过；2 个 `frame_buffer` 实例均连接新增端口；LED2/LED3 各只有一个驱动；传感器 ROM 仍为 165 项连续索引且 SHA-256 与 D 盘上一版一致。本机无 `iverilog`、`verilator`、`vlog`、`yosys` 或 Verible，未运行 RTL 编译/仿真。
- 结果：`SOURCE PATCHED / STATIC CHECK PASS / MAP-PNR-BOARD NOT VERIFIED`。
- 下一步门禁：同步三处 RTL 后完整重跑 Map/PNR/bitstream。烧录后先松开 SW4 截取真实五色画面并记录 LED20/LED21，再按住 SW4 截取 RAW 棋盘并再次记录 LED20/LED21；同时回传 WNS/WHS 与 CDC。

### [M0-31] M0-30 D 盘同步与诊断版发布核查

- 同步方向：`competition_project_single_camera/` -> `D:\TJ375N529_SC431HAI2LCD_Demo_V3/`。
- D 盘写入文件与 SHA-256：
  - `src/framebuffer/frame_info_det.v`：同步前 `BFC24B363E28F18807D5CF3967E4CBC6935A3FFB8327C14FB35685318C2A7766`；同步后正式工程/D 盘一致为 `24806B61653C57C4BCAA218B0417AD7C257FE723CF4A21502A376F93F3229154`。
  - `src/framebuffer/frame_buffer.v`：同步前 `C5BC80D22F721A03168FFD14B625F1D4B49F1DE2E25E03DAC9B2E20383E740A6`；同步后正式工程/D 盘一致为 `8C4C8D7A5DF3C9198ADFE85006B2A24BA4A5D8163A68960D90BC2A8C1120EB7A`。
  - `src/top.v`：同步前 `7A8111B36CE05F51B402BBB9424F53A6728B634F60E81EBE01DD8F279E89DD85`；同步后正式工程/D 盘一致为 `5F30CED8F6392ACCCEFD23FADEE918400E57A736E5CC2E405A48693B3D8C45C1`。
- 未写入：传感器 I2C ROM、`constrain.sdc`、`mem_test.xml`、`.peri.xml`、IP settings、`outflow/`、`work_*` 和 bitstream。
- 操作说明：
  - SW4 松开：真实 J48/ch0 五色画面。
  - SW4 按住：DDR 前灰度 RAW 棋盘诊断画面；必须持续按住拍照，松开自动恢复真实画面。
  - LED20/F3 亮：ch0 连续两帧实际像素数一致；灭：帧长度门未稳定。
  - LED21/F2 亮：本次上电后曾发生 framebuffer 输出 FIFO 欠流；该状态锁存到重新配置/复位。
- 结果：`SOURCE SYNC PASS / NEW MAP-PNR-BITSTREAM-BOARD NOT VERIFIED`。
- 用户执行请求：完整重跑 D 盘工程并烧录；回传 Timing/CDC、SW4 松开与按住的两张 Windows 直接截图、LED20/LED21 在两种状态下的亮灭。旧 2026-07-14 22:07 bitstream 不包含本次修改。

### [M0-30] 帧长度门修复、framebuffer 欠流观测与 RAW 棋盘诊断版

- 触发：用户确认 FPGA 本地产生的标准色条没有重影，真实摄像头画面仍存在水平重影。
- 诊断边界：同一 HDMI/TMDS/线缆/采集卡链路的 FPGA 本地标准色条干净，因此重影范围收缩为 `摄像头/CSI -> RAW 打包 -> DDR/framebuffer -> Debayer`；不得继续把 HDMI 下游作为首要根因。
- 确定性缺陷：`src/framebuffer/frame_info_det.v` 原来把 1 bit `frame_end_r0` 脉冲写入 24 bit `frame_len_d0`，导致 `frame_stable` 基本失去真实帧长度检查能力。
- 修改：
  - `frame_len_d0` 改为锁存 `frame_pix_num`，仅连续两个非零帧长度一致时置 `frame_stable=1`。
  - `total_frame_bytes` 复位为 0，并使用非阻塞赋值。
  - `frame_buffer.v` 导出 `o_frame_stable` 和既有 `data_tx.fifo_rd_underflow`。
  - LED20/F3 映射 ch0 帧长度稳定；LED21/F2 锁存 framebuffer 输出 FIFO 欠流。
  - SW4/V19 按住时曾使用 DDR 前 RAW 灰度棋盘；松开保持真实 J48/ch0 画面。
- 结果：`SOURCE PATCHED / STATIC CHECK PASS / BOARD NOT VERIFIED AT THIS CHECKPOINT`。

### [M0-31] 2026-07-15 13:42 构建与 RAW 棋盘现场反馈

- 构建证据：
  - `D:\TJ375N529_SC431HAI2LCD_Demo_V3\outflow\mem_test.timing.rpt`：`2026-07-15 13:42:24`。
  - `D:\TJ375N529_SC431HAI2LCD_Demo_V3\outflow\mem_test.bit`：`2026-07-15 13:42:41`，SHA-256 `F5EE45227D33B4C83CD3D6C03580A8D2554E77822A5C34591F223C0E887917A0`。
  - 最小 setup slack `+1.480 ns`，最小 hold slack `+0.026 ns`；`i_sysclk_div2 -> hdmi_tx_slow_clk` setup/hold 为 `+4.989/+0.092 ns`。
  - CDC：`No Synchronizer warnings to report`。
- 上板截图：
  - `屏幕截图 2026-07-15 134418.png`：真实摄像头画面，存在整体偏紫。
  - `屏幕截图 2026-07-15 134430.png`：按住 SW4 的 RAW 棋盘，出现橙蓝细条且整体偏紫。
- 量化：真实画面左右中性亮区均表现为 R/B 高于 G；RAW 棋盘全图均值约 `(160,134,179)`。
- 判定：本次 RAW 棋盘不是可靠的中性参考。棋盘跨 Bayer 邻域会被 Debayer 插值成彩色边缘，且诊断数据仍经过 `R=1.75x/G=1x/B=2x` 显示白平衡，因此不能仅凭橙蓝棋盘判定 DDR/Debayer 损坏。
- NOT VERIFIED：LED20/LED21 现场亮灭未反馈；真实摄像头重影根因仍未关闭。
- 结果：`TIMING PASS / CDC PASS / DIAGNOSTIC PATTERN INVALID FOR ISOLATION / PURPLE CAST CONFIRMED`。

### [M0-32] 候选工程恢复、中性 RAW 诊断与轻度减紫

- 触发：候选工程在工作期间回到 M0-29 源码，D 盘仍保留已构建的 M0-30/M0-31 诊断版；用户明确批准以 D 盘诊断版为准反向恢复并继续修改。
- 恢复：将 D 盘已构建版的帧长度修复、framebuffer 观测端口、LED20/LED21 和 SW4 诊断边界恢复到候选工程。
- 诊断修正：
  - 删除棋盘 X/Y 计数器，SW4 按住时向 DDR 前写入固定 `32'h80808080`，使四个 RAW 样本全部为中灰。
  - SW4 诊断模式绕过相机专用 HDMI 固定白平衡；CSI 的 VS/DE、DDR/framebuffer、Debayer、2:1 HDMI 拆分和 TMDS 仍全部参与。
  - 预期诊断画面为近似均匀中性灰。若仍出现稳定条带、重影或局部异常，才可继续归因到 DDR/framebuffer、Debayer 或后续像素编排。
- 真实画面轻度减紫：仅调整 HDMI 显示支路固定点系数，由 `R=1.75x/G=1x/B=2.0x` 改为 `R=1.625x/G=1x/B=1.875x`；不改变传感器 ROM、曝光、模拟/数字增益、CSI、RAW、DDR、Debayer 或识别数据边界。
- 算术核查：全部 8 bit 输入范围内 R/B 的 10 bit 中间结果无溢出和减法下溢；`git diff --check` 通过；两个 framebuffer 实例新增端口均完整；LED20/LED21 各只有一个驱动。
- D 盘同步：
  - `src/top.v`：候选工程/D 盘 SHA-256 `744D5B96E45C6A1ABCAEEC908C2C2CB8B8C8B8DA5F7181A39E2F47A3CA9D32C8`。
  - `src/framebuffer/frame_buffer.v`：候选工程/D 盘 SHA-256 `8C4C8D7A5DF3C9198ADFE85006B2A24BA4A5D8163A68960D90BC2A8C1120EB7A`。
  - `src/framebuffer/frame_info_det.v`：候选工程/D 盘 SHA-256 `24806B61653C57C4BCAA218B0417AD7C257FE723CF4A21502A376F93F3229154`。
- 未修改：`constrain.sdc`、`mem_test.xml`、`.peri.xml`、IP settings、传感器 I2C ROM、`outflow/`、`work_*` 和 bitstream。
- 结果：`SOURCE RESTORED AND SYNCED / STATIC CHECK PASS / NEW MAP-PNR-BITSTREAM-BOARD NOT VERIFIED`。
- 下一步门禁：从 D 盘工程完整重跑 Map/PNR/bitstream 并烧录；回传 WNS/WHS、CDC、SW4 松开真实画面、SW4 按住中性灰画面，以及 LED20/LED21 亮灭。

### [M0-33] 中灰 RAW 诊断通过，关闭 DDR/framebuffer/Debayer 重影怀疑

- 触发：用户完成 M0-32 的完整构建、烧录和板级拍摄，反馈 LED20 亮、LED21 不亮，并回传真实摄像头与按住 SW4 的中灰画面。
- 匹配构建证据：
  - `D:\TJ375N529_SC431HAI2LCD_Demo_V3\outflow\mem_test.bit`：`2026-07-15 14:06:11`，SHA-256 `2DAB0FE864EAB4EE1DCC566BC5E7CE93E50A4AD94309D7032C949367B8E84D41`。
  - Timing：最小 setup slack `+1.611 ns`，最小 hold slack `+0.026 ns`；`i_sysclk_div2 -> hdmi_tx_slow_clk` setup/hold 为 `+3.538/+0.162 ns`。
  - CDC：`No Synchronizer warnings to report`。
- 上板判定：
  - LED20/F3 亮：连续非零帧长度稳定。
  - LED21/F2 不亮：本次上电后没有观测到 framebuffer 输出 FIFO 欠流。
  - 按住 SW4 的 `32'h80808080` 中灰画面均匀、无条带、无重影、无紫偏；它仍经过 CSI 帧时序、DDR/framebuffer、Debayer、HDMI 2:1 拆分和 TMDS。
- 结论：`DDR/framebuffer -> Debayer -> HDMI` 段在当前时序、当前位流和均匀输入下通过；不再把它们作为真实场景重影的首要根因。真实画面残余的软边/拖影优先转查摄像头镜头焦距、自动/长曝光引起的运动模糊、传感器 RAW 有效载荷或 CSI 前端。
- 未修改文件：本 Gate 仅记录板级证据，不修改 RTL、SDC、工程 XML、IP、I2C ROM 或 bitstream。
- 结果：`TIMING PASS / CDC PASS / FRAME STABLE / NO FRAMEBUFFER UNDERFLOW / DOWNSTREAM DATA-PATH PASS`。

### [M2-01] 官方 Hard SoC 例程与视频资源冲突审计

- 触发：用户暂停摄像头光学优化，批准开始 F1 CPU 最小集成。
- 目标：先推进板上 CPU UART Hello，不改已通过 J48/ch0 到 HDMI 板级诊断的视频链。
- 审计来源：
  - 官方 TJ375C529 eMMC 例程 `赛方提供材料/例程/RISC-V例程/02_eMMC Test/ti375c529_emmc_v1.1_cn/`。
  - 官方例程提供 Hard SoC、生成 BSP `soc.h`、默认 linker 和 standalone `uartEchoDemo`；这些是生成方法和 UART 启动参考，不是可直接复制到视频工程的源码。
  - 当前候选工程 `mem_test.peri.xml` 的 `soc_info` 为空，视频已经使用 `PLL_BL0/PLL_BL1/PLL_BL2/PLL_TR0` 与 `JTAG_USER1`。
- 审计结论：官方 Hard SoC 系统 PLL 只能使用 `PLL_BL0/PLL_BL1/PLL_BL2`，当前视频已占满三者；并且旧 SoC 生成会与视频 `clk_25m/GPIOT_P_50`、`ddr_clk_ref/GPIOL_25` 形成重复 GPIO 风险。不得直接合并 `.peri.xml`、手改生成 RTL 或硬抄官方示例 `soc.h`。
- 交付：新增 `docs/review_packets/m2_hard_soc_resource_replanning_operator_packet_20260715.md`，冻结 Interface Designer 的受控重规划顺序、必须回传的生成物和 fail-closed 条件。
- 未修改：视频 RTL、`mem_test.xml`、`mem_test.peri.xml`、`constrain.sdc`、IP、传感器 I2C ROM、D盘工程、bitstream 和 CPU 应用源码。
- 结果：`OFFICIAL FLOW AUDITED / SOC INSERTION BLOCKED PENDING GUI RESOURCE REPLAN / VIDEO BASELINE PRESERVED`。
- 下一步门禁：用户在 Interface Designer 生成最小 Hard SoC 并回传同次 `.peri.xml`、生成 `soc.h`、linker、wrapper、资源截图及警告；Codex 审核通过后才写 UART Hello 并进行 Map/PNR。

### [M2-02] Interface Designer 实物资源截图完成，Hard SoC 直接接入关闭

- 触发：用户回传当前 D盘 `mem_test` Interface Designer 的 PLL、JTAG 与三路时钟 GPIO 截图。
- 已验证资源：
  - `PLL_TR0`=`MIPI_TX_PLL`，使用 `clk_25m/GPIOT_P_50`。
  - `PLL_BL0`=`lpddr4_pll`，使用 `ddr_clk_ref/GPIOL_25`。
  - `PLL_BL1`=`pll_inst1`，使用 `clk_74p25m/GPIOL_32`。
  - `PLL_BL2`=`pll_inst4`，使用核心时钟 `i_fb_clk`/`mipi_clk`。
  - `JTAG_USER1`=`jtag_inst1`。
  - 三路外部时钟 GPIO 均为已分配 PLL 输入，包封装引脚分别为 L17、V3、U4。
- 结论：Hard SoC 合法系统 PLL 集合 `PLL_BL0/PLL_BL1/PLL_BL2` 与当前活跃视频 PLL 集合完全重合；JTAG 可理论切到 USER2，但不能解决系统 PLL 冲突。直接 SoC 接入、手改 XML/生成 RTL、复制官方 `soc.h` 均关闭。
- 交付：新增 `docs/review_packets/m2_hard_soc_video_resource_conflict_report_20260715.md`，记录证据、禁止项与候选路线 A（保持视频、先推进 CPU 软件）/B（独立 GUI 重规划实验）。
- 未修改：视频 RTL、工程 XML、SDC、IP、传感器 ROM、D盘源码与 bitstream。
- 结果：`RESOURCE CONFLICT CONFIRMED / DIRECT HARD SOC INSERTION NO-GO / DECISION REQUIRED`。

### [M2-03] 候选 A：F1 单摄 CPU 纯软件核心 Host 回归

- 触发：Hard SoC 直接接入因 PLL 资源冲突关闭；用户批准先保持视频工程不动，推进 CPU 纯软件核心。
- 目标：在没有 SoC、APB、UART、GPIO、OSD wire ABI 或机械臂传输依赖的条件下，固化 F1 的单摄识别判定和逐轮事务语义。
- 实际动作：
  - 新增 `cpu/include/single_camera_f1.h`、`cpu/src/single_camera_f1.c`、Host 测试及其 PowerShell 入口。
  - 固化 `PLACE` 与独立 `ABANDON` 小闭环；下一次有效 `PLACE` 开始下一轮，无 `REMOVE` 状态。
  - 同一 `PLACE` 序号在采集中视为去抖重复事件，不会创建第二轮；每轮最终结果仅锁存一次。
  - 覆盖颜色、正方体形状、尺寸关系、未标定尺寸、超时、弃置和连续 20 轮的主机回归。
  - 修复测试中的 C 临时对象取址、轮次期望与测试脚本 CPU 根路径；修复实现中重复 `PLACE` 的判定顺序。
- 修改文件：
  - `cpu/include/single_camera_f1.h`
  - `cpu/src/single_camera_f1.c`
  - `cpu/tests/test_single_camera_f1.c`
  - `cpu/tests/run_single_camera_f1_host.ps1`
  - `cpu/README.md`
  - 本工作日志。
- D 盘写入：无。该轮仅为独立 CPU 纯软件核心，未改动视频、Efinity、SoC 工程输入或 bitstream。
- 命令与结果：

```powershell
$env:Path = 'C:\Users\20306\Desktop\MinGW\bin;' + $env:Path
powershell -ExecutionPolicy Bypass -File .\competition_project_single_camera\cpu\tests\run_single_camera_f1_host.ps1
git diff --check
```

  - `single_camera_f1: 87/87 passed`
  - `git diff --check` 通过。
- 结果：`HOST VERIFIED`。
- NOT VERIFIED：Hard SoC 生成、板上 CPU 启动、按键 GPIO、UART 目标输入/结果输出、CPU 与 FPGA 的寄存器/CDC/OSD 契约、视觉特征输入、机械臂传输和实际动作均未实现或验证；`ARM_ENABLED=0` 时命中只产生 `EXECUTE_ARM_DISABLED` 语义，绝不发出机械臂请求。
- 下一步门禁：仅在不改动已验证视频链路的前提下，审计并选择可复用的 `final_project` CPU 分类器和参数框架；禁止迁移旧双摄 `board_io`、旧 APB 地址、`REMOVE` 状态机或第二套轮次状态机。

### [M2-04] 候选 A：选择性迁移为单摄 CPU 分类适配层

- 触发：M2-03 已完成 F1 事务语义回归，需要为未来 FPGA 特征输入预留可测试的单摄分类入口。
- 审计结论：`final_project/cpu/app/src/vision_classifier.c` 依赖 `board_io.h` 的双摄 `feature_snapshot_t`，并将尺寸绑定到 `cam1 height_px`，不能原样迁移；`task_matcher.c` 自带抓取坐标和独立轮次锁，不能与唯一 `PLACE` 状态机并存；`param_table` 的双槽/NVM 依赖旧配置，暂不迁移。
- 实际动作：新增单摄 `sc_features_t` 和 `sc_classify_features()`，输入仅含颜色面积、前景面积、ROI 亮度和 bbox 尺寸，输出既有 `sc_observation_t`。红/蓝/黄面积优先判色，缺少显著色面积时以 ROI 平均亮度尝试白/黑；以填充率优先识别正方体。尺寸固定为 0；圆柱/锥体结果仅是未标定启发式，F1 只依赖 `CUBE/NON_CUBE`。
- 修改文件：`cpu/include/single_camera_classifier.h`、`cpu/src/single_camera_classifier.c`、`cpu/tests/test_single_camera_classifier.c`、`cpu/tests/run_single_camera_classifier_host.ps1`、`cpu/README.md` 和本工作日志。
- D 盘写入：无。未改动视频 RTL、工程 XML、约束、IP、SoC 生成物或 bitstream。
- 验证：`single_camera_f1: 87/87 passed`；`single_camera_classifier: 18/18 passed`；`git diff --check` 通过；新分类器静态扫描未发现 `board_io`、APB、CAM1、UART 或 GPIO 平台依赖。
- 结果：`HOST VERIFIED`。
- NOT VERIFIED：默认阈值不是当前摄像头实测标定值；当前 FPGA 视频工程尚未提供这些 ROI 特征；五色真实画面、白/黑判色、正方体/非正方体及圆柱/锥体现场准确率均未验证；未接入 SoC、按键、UART、OSD 或机械臂。
- 下一步门禁：先定义并审查最小单摄 FPGA 到 CPU 特征契约及其 CDC/寄存器实现方案；在合法 Hard SoC 资源生成前，该契约只能作为文档和 Host mock，禁止手填 APB 地址或修改已验证视频链路。

### [M2-05] 候选 A：单摄 FEATURE_SNAPSHOT 契约与 fail-closed Host 适配

- 触发：M2-04 分类器已有无地址输入结构，需要冻结未来 FPGA 特征源的最小语义，避免沿用 `final_project` 的双摄和候选 APB 地址。
- 源码审计：当前 ch0 画面实际路径为 `frame_buffer -> debayer_top_2to1 -> rgb0_data_rgb -> HDMI`；建议 feature tap 在 `src/top.v` 的 Debayer 后 `i_sysclk_div2` 域，以 `rgb_vs/rgb_hs/rgb_de` 和每拍两个 RGB 像素统计。现有工程未实现 ROI、颜色面积、前景面积、bbox、特征快照或 CPU 可读接口。
- 实际动作：新增 `integration/single_camera_feature_contract.md`，作为唯一特征契约，冻结旁路 tap、双像素展开、ROI/背景/颜色 mask 语义、字段宽度、帧原子性、`frame_id` ACK 和 CDC fail-closed 规则。特征源固定为 Debayer 后、HDMI 显示专用白平衡之前的 `rgb0_data_rgb`；旁路不得回压或改变 framebuffer、Debayer、HDMI。
- Host 适配：新增 `single_camera_feature_adapter`，只允许 `FRAME_STABLE`、`ROI_VALID`、`STATS_VALID`、`SOURCE_CH0` 同时成立，且拒绝诊断、溢出和 snapshot overrun 后才调用分类器。
- 修改文件：`integration/single_camera_feature_contract.md`、`integration/README.md`、`cpu/include/single_camera_feature_adapter.h`、`cpu/src/single_camera_feature_adapter.c`、`cpu/tests/test_single_camera_feature_adapter.c`、`cpu/tests/run_single_camera_feature_adapter_host.ps1`、`cpu/README.md` 和本工作日志。
- D 盘写入：无。未修改视频 RTL、工程 XML、约束、IP 或 bitstream；已核验 D 盘与候选工程的三个已验证视频源哈希仍一致。
- 验证：`single_camera_f1: 87/87 passed`；`single_camera_classifier: 18/18 passed`；`single_camera_feature_adapter: 17/17 passed`；契约与适配器的 7 个 flag 名称一致；`git diff --check` 通过。
- 结果：`HOST CONTRACT VERIFIED`。
- NOT VERIFIED：FPGA feature tap、统计 RTL、ROI/背景/颜色阈值标定、snapshot CDC、APB、SoC、CPU 启动、UART、OSD 和真实五色识别均未实现或验证。知识图谱 CLI 当前只返回旧 `D-cicc_cbm_link` 而非仓库声明的主图谱，故本轮以真实源码为准。
- 下一步门禁：等待用户批准 feature tap 小 Gate 后，先写独立 RTL testbench，再新增旁路统计模块和受控顶层连接；任何 RTL 接入均需完整 Map/PNR/STA/CDC 与 HDMI 回归，且仍不得修改 SoC 资源或机械臂链路。

### [M2-06] Feature Tap 小 Gate：禁用采集的 ch0 旁路统计 RTL

- 触发：用户批准 feature tap 小 Gate。
- 目标：将单摄特征统计器以只读旁路方式纳入真实 Efinity 工程，先验证它不会使 J48/ch0 到 HDMI 画面回归；本 Gate 不开放 CPU 读取或特征采集。
- 实际动作：
  - 新增 `src/feature_stats/feature_stats_tap.v` 与独立 `tests/rtl/feature_stats_tap_tb.v`。模块按契约处理 Debayer 后每拍两像素 RGB，支持 ROI、红/蓝/黄面积、前景面积、亮度、bbox、帧锁存、ACK 和 fail-closed flags。
  - 静态审查中修复双像素坐标推进、颜色比较位宽、亮度/背景差累加位宽和小位宽 overflow 测试边界。
  - 在 `src/top.v` 从 ch0 `rgb0_data_rgb`、`rgb_vs/rgb_de`、`ch0_frame_stable` 和 `raw_diag_en` 接入旁路实例；所有视频主链输出保持原连线。
  - `i_capture_enable` 固定为 `1'b0`，ROI/背景/颜色参数均为安全占位常量，输出全部接到 `_unused` 信号，ACK 固定关闭。因此当前 bitstream 不会发布特征快照，也不能触发任何 CPU、OSD、UART 或机械臂行为。
  - 将 RTL 加入 `mem_test.xml` 设计文件列表；未改动 framebuffer、Debayer、HDMI、`constrain.sdc`、`.peri.xml`、IP 或传感器 I2C ROM。
- 修改文件：`src/feature_stats/feature_stats_tap.v`、`tests/rtl/feature_stats_tap_tb.v`、`src/top.v`、`mem_test.xml`、`docs/debug_sessions/m2_feature_tap_manual_build_board_check_20260715.md` 和本工作日志。
- D 盘写入：已同步并 SHA-256 核验 `src/top.v`=`746BAD51743E46DBCD2D89DBFF8CED3A89F00999DF0B5312B62A2D00B1C5D343`、`src/feature_stats/feature_stats_tap.v`=`F2E132C6BBA1934AAF7B0E2D8E306990890E21ADBC7B03F45BF2A05F194FE6FC`、`mem_test.xml`=`F0D01C43D483176F7754952606CF8502F1850766ACB12EFE14CFB8E9BD26F3ED`。
- 静态验证：工程 XML 引用存在、模块端口存在、顶层 `i_capture_enable=1'b0`、候选工程/D 盘同步后哈希一致、`git diff --check` 通过。当前环境未检测到 `iverilog`、`verilator`、`vlog` 或 `vsim`，故独立 RTL testbench 为 `NOT RUN`；用户负责完整 Efinity Map/PNR/bitstream/烧录。
- 结果：`SOURCE SYNC PASS / RTL SIM NOT RUN / MAP-PNR-STA-CDC-BOARD NOT VERIFIED`。
- 旧 bitstream：D 盘 `outflow/mem_test.bit` 仍为 2026-07-15 14:06:11、SHA-256 `2DAB0FE864EAB4EE1DCC566BC5E7CE93E50A4AD94309D7032C949367B8E84D41`，不包含本 Gate 修改，不得作为验收证据。
- 下一步门禁：用户按 `docs/debug_sessions/m2_feature_tap_manual_build_board_check_20260715.md` 完整构建、烧录并回传资源/Setup/Hold/CDC/新 bitstream 身份和 J48/ch0 HDMI 回归；通过后再讨论是否在独立下一 Gate 开启 feature capture 与合法 SoC 资源重规划。

### [M2-06a] Efinity GUI 覆盖设计清单导致 feature tap Map 失败，已恢复

- 触发：用户在 D 盘 Efinity Map 收到 `VERI-1063`：`top.v` 实例化 `feature_stats_tap` 时找不到模块。
- 证据：`D:\TJ375N529_SC431HAI2LCD_Demo_V3\outflow\mem_test.err.log` 记录 2026-07-15 15:18:56 的失败；失败后 D 盘 `mem_test.xml` 时间更新为 15:18:57，实际已不含 `src/feature_stats/feature_stats_tap.v`，而 `src/feature_stats/feature_stats_tap.v` 文件仍存在且与候选工程哈希一致。
- 根因：Efinity 在打开工程状态下以 GUI 缓存的旧 design-file 列表重写了 `mem_test.xml`，将手工同步的新增 RTL 条目移除；不是模块代码缺失或视频链路错误。
- 恢复动作：用户确认关闭 Efinity 及 Map/PNR/bitstream 进程后，将候选工程 `mem_test.xml` 单文件重新同步到 D 盘。同步后 D 盘 `mem_test.xml` SHA-256 为 `F0D01C43D483176F7754952606CF8502F1850766ACB12EFE14CFB8E9BD26F3ED`，第 43 行包含 feature tap 条目；D 盘 RTL SHA-256 为 `F2E132C6BBA1934AAF7B0E2D8E306990890E21ADBC7B03F45BF2A05F194FE6FC`。
- 结果：`PROJECT FILE RECOVERED / MAP NOT RERUN`。
- 下一步门禁：从关闭状态重新打开 `D:\TJ375N529_SC431HAI2LCD_Demo_V3\mem_test.xml` 后，先确认 Design Files 列表可见 `src/feature_stats/feature_stats_tap.v`，再完整运行 Map；若 GUI 再次保存后丢失该条目，停止构建并回传 GUI Design Files 截图与新的 `mem_test.xml`。

### [M2-06b] Feature tap Verilog-2000 字面量语法失败，已修复

- 触发：恢复 design-file 条目后，Efinity 报告 `feature_stats_tap.v` 第 267、303、305、308、313 行 `VERI-1137`，并连带出现 `else` 上下文、flag 重复声明和 flag 非 task 错误。
- 根因：新增 RTL 误用了 C 语言无符号整数字面量 `0u`。Efinity 工程使用 `verilog_2k`，不接受 `u` 后缀；第一个 `0u` 破坏解析后导致其余错误级联，模块并非真的重复声明。
- 修复：将 5 处 `0u` / `!= 0u` 改为与参数宽度一致的标准 Verilog `{COUNT_WIDTH{1'b0}}` 比较；未修改模块接口、统计语义或视频连接。
- 验证：候选与 D 盘 RTL 均无 `[0-9]u`；`git diff --check` 与三套 CPU Host 回归通过。D 盘修复后 `src/feature_stats/feature_stats_tap.v` SHA-256=`10A5D6D6DFE1148ECC4561F24C4F5E41436E43FBD105130A21885A35B072A465`。
- 结果：`SOURCE SYNC PASS / EFINITY MAP NOT RERUN`。
- 下一步门禁：重新运行 Map；先确认 `VERI-1137` 与 `VERI-1063` 均消失，再继续 PNR/bitstream。若出现新的 module 内语法错误，停止在 Map 阶段并回传新的 `mem_test.err.log`。

### [M2-06c] 禁用 Feature Tap 的完整构建与 J48/ch0 HDMI 回归

- 触发：修复 `feature_stats_tap.v` 的 Verilog-2000 字面量错误后，用户重新完成 D 盘完整构建、烧录并确认摄像头画面未回退。
- 目标：证明统计 RTL 的禁用旁路实例可被 Efinity 综合、布局布线和生成 bitstream，且不破坏现有 J48/ch0 到 HDMI 画面链路。
- 输入基线：`D:\TJ375N529_SC431HAI2LCD_Demo_V3`；Feature capture 仍固定关闭，未接 CPU/SoC/APB/UART/OSD/机械臂。
- 实际动作：
  - 核查 D 盘最新 Map/PNR/timing/CDC/bitstream 的时间链；新 `mem_test.bit` 时间为 2026-07-15 15:32:07。
  - 核查 Map 资源为 `EFX_LUT4=10887`、`EFX_FF=9434`、`EFX_RAM10=163`、`EFX_DPRAM10=4`。
  - 核查 Setup 最小 slack 为 `+1.766ns`，Hold 最小 slack 为 `+0.026ns`；CDC 报告为 `No Synchronizer warnings to report.`。
  - 计算 bitstream SHA-256：`45427C12AFE874C6032614B3D241EFD3BCBABFF395970D4D80FFFE8165F78535`。
  - 用户上板回传 J48/ch0 HDMI 正常摄像头画面，确认没有视频回退；此前同一路径已回传 LED20/F3 亮、LED21/F2 灭。
  - 复核候选工程与 D 盘 `src/top.v` SHA-256 同为 `746BAD51743E46DBCD2D89DBFF8CED3A89F00999DF0B5312B62A2D00B1C5D343`，`src/feature_stats/feature_stats_tap.v` 同为 `10A5D6D6DFE1148ECC4561F24C4F5E41436E43FBD105130A21885A35B072A465`。
- 修改文件：`docs/debug_sessions/m2_feature_tap_manual_build_board_check_20260715.md`、本工作日志；无 RTL、约束、`.peri.xml`、IP 或 D 盘源码写入。
- D 盘写入：无。本条只读取和记录用户已生成的 D 盘构建产物；D 盘源码与候选工程关键 RTL 哈希一致。
- 结果：`PASS`，禁用 feature tap 的源码、完整 Efinity 构建和板级 HDMI 回归均通过。
- NOT VERIFIED：独立 RTL testbench 仍未运行；未记录新的 10 分钟连续画面观察；`i_capture_enable=1'b0`，故特征快照、ROI 标定、CPU/SoC、APB、UART、OSD、按键和机械臂均未实现或验证。
- 下一步门禁：CPU 上板只允许进入独立的 Interface Designer Hard SoC 资源重规划实验。不得手改 `mem_test.peri.xml`、生成 SoC RTL、`soc.h`、约束或现有视频 PLL；先生成最小 UART0-only SoC artifact bundle，再由 Codex 审查。

### [M2-07] Hard SoC GUI 实验前的工程外备份

- 触发：用户要求执行资源重规划操作包的第一步。
- 目标：在 Efinity Interface Designer 操作前，保留当前 D 盘已验证视频工程的四个关键输入文件，确保资源重规划失败时可按原样恢复。
- 输入基线：`D:\TJ375N529_SC431HAI2LCD_Demo_V3`；Efinity 进程仍在运行，本次不关闭、不保存、不修改该工程。
- 实际动作：将 `mem_test.peri.xml`、`mem_test.xml`、`constrain.sdc`、`src\top.v` 复制到工程目录之外的 `C:\Users\20306\Desktop\赛题资料\CICC_backups\TJ375N529_SC431HAI2LCD_Demo_V3\20260715_154846\`，并逐文件复算 SHA-256 确认备份与源文件一致。
- 修改文件：本工作日志、备份目录中的 `BACKUP_MANIFEST.md`；未修改候选工程 RTL、D 盘工程、约束、IP、生成物或 bitstream。
- D 盘写入：无。只读取 D 盘源文件。
- 结果：`PASS`。备份清单记录四个相对路径、源文件时间、字节数及 SHA-256；四项均 hash match。
- 下一步门禁：用户可在 Interface Designer 中进行资源可行性判断，但在生成后、Map/PNR 前必须回传生成物和资源截图供 Codex 审查。

### [M2-08] Interface Designer 现有 PLL/JTAG 资源截图留证

- 触发：完成工程外备份后，用户按操作包在未改动状态下回传 Interface Designer 资源截图。
- 目标：确认当前视频工程对 Hard SoC 所需 PLL/JTAG 资源的实际占用，避免仅凭历史配置推断。
- 实际证据：
  - `MIPI_TX_PLL` 使用 `PLL_TR0`，外部参考为 `clk_25m/GPIOT_P_50`，25MHz。
  - `lpddr4_pll` 使用 `PLL_BL0`，外部参考为 `ddr_clk_ref/GPIOL_25`，100MHz；该 PLL 是 DDR 相关时钟。
  - `pll_inst1` 使用 `PLL_BL1`，外部参考为 `clk_74p25m/GPIOL_32`，74.25MHz；该 PLL 是现有系统/HDMI 时钟依赖。
  - `pll_inst4` 使用 `PLL_BL2`，核心参考为 `i_fb_clk`，25MHz；该 PLL 是 MIPI 字节时钟依赖。
  - `jtag_inst1` 使用 `JTAG_USER1`。
  - 左侧树显示 `Quad-Core RISC-V (0)`，当前工程尚未创建 Hard SoC。
- 修改文件：本工作日志；未修改 D 盘工程、视频 RTL、`.peri.xml`、约束、IP 或生成物。
- D 盘写入：无。
- 结果：`PASS`，资源冲突的现场证据与已有审计结论一致；目前没有空闲的 Hard SoC 合法系统 PLL。
- NOT VERIFIED：尚未展开每个 `PLL Resource` 下拉列表，不能仅凭当前截图断言 Interface Designer 不存在任何可用的 GUI 合法重规划组合。
- 下一步门禁：仅展开 `PLL_BL0`、`PLL_BL1`、`PLL_BL2` 对应实例的 `PLL Resource` 下拉列表查看候选项；不得选择、应用、保存或生成。若不存在不破坏在用视频 PLL 的候选项，则关闭本轮 SoC 上板路径并回到 CPU Host/特征标定工作。

### [M2-09] PLL 资源下拉候选项只读审查

- 触发：用户分别展开 `lpddr4_pll`、`pll_inst1`、`pll_inst4` 的 `PLL Resource` 下拉列表并回传截图，未选择任何新项。
- 实际证据：三个下拉框均显示同一通用资源池：`PLL_BL0/1/2`、`PLL_BR0/1/2`、`PLL_TL0/1/2`、`PLL_TR0/1/2` 及 `None`。当前选择仍分别是 `PLL_BL0`、`PLL_BL1`、`PLL_BL2`。
- 判定：此列表只证明 Interface Designer 可以显示全器件 PLL 名称，不能证明任一替代资源可承载当前 PLL 的参考时钟输入、输出频率、GCLK 分发、DDR/MIPI 专用连接或物理区域约束。因此不得据此把任何视频 PLL 迁移到 `PLL_BR*`、`PLL_TL*` 或 `PLL_TR*`。
- 修改文件：本工作日志；未修改 D 盘工程、视频 RTL、`.peri.xml`、约束、IP 或生成物。
- D 盘写入：无。
- 结果：`READ-ONLY INSPECTION PASS / REPLANNING FEASIBILITY NOT VERIFIED`。
- 下一步门禁：关闭下拉框且不保存当前工程。后续只能在完整工程副本中，通过 Interface Designer 实际选择候选资源并运行其设计校验来判断是否合法；校验或生成出现 DDR、MIPI、HDMI、时钟或 GPIO 资源错误即停止，不触碰当前已验证工程。

### [M2-10] Hard SoC PLL 重规划实验工程副本

- 触发：用户已关闭 Efinity，允许建立独立副本验证 PLL 资源重规划可行性。
- 目标：隔离 Hard SoC GUI 试验，确保原 D 盘工程仍是可直接烧录的 J48/ch0 HDMI 回退基线。
- 前置检查：确认无 `efinity`、`efx_run`、`efx_map`、`efx_pnr`、`efx_pt` 或 `efx_pgm` 进程。
- 实际动作：完整复制 `D:\TJ375N529_SC431HAI2LCD_Demo_V3` 到 `D:\TJ375N529_SC431HAI2LCD_Demo_V3_soc_replan_20260715_160020`。源和副本总文件字节数均为 `211229338`；关键文件逐项 SHA-256 一致。
- 修改文件：本工作日志、实验副本内 `EXPERIMENT_MANIFEST.md`；不修改原工程、候选工程 RTL、约束、`.peri.xml`、IP 或 bitstream。
- D 盘原工程写入：无。仅新建同级实验副本目录。
- 结果：`PASS`。原工程与实验副本均保留，实验副本的基线和允许范围已写入清单。
- 下一步门禁：仅打开实验副本 `D:\TJ375N529_SC431HAI2LCD_Demo_V3_soc_replan_20260715_160020\mem_test.xml`，在 Interface Designer 中尝试一个候选 PLL 迁移并运行其设计校验；不得在原工程中操作，不得 Map/PNR/烧录，任何错误均停在副本并回传。

### [M2-11] 实验副本 `PLL_BR1` 迁移校验失败，关闭 PLL 重规划路线

- 触发：用户在实验副本中将视频 `pll_inst1` 从 `PLL_BL1` 临时切换为 `PLL_BR1` 并执行 Interface Designer 设计校验。
- 实际证据：Interface Designer 的副本路径显示为 `D:\TJ375N529_SC431HAI2LCD_Demo_V3_soc_replan_20260715_160020`；校验报错 `pll_rule_refclk`：`pll_inst1` 的参考时钟被关联至未定义的 `GPIOB_P_39/GPIOB_PN_39` 外部时钟输入。`clk_74p25m` 同时出现 `gpio_rule_alt_conn` warning，说明原有 `pll_clkin` 连接已不再被有效 PLL 使用。
- 判定：`PLL_BR1` 不能在保持 `clk_74p25m/GPIOL_32` 参考时钟和 HDMI 系统时钟链路的条件下替代 `PLL_BL1`。该路径会破坏当前视频时钟连接，不能用于为 Hard SoC 释放 `PLL_BL1`。
- 修改文件：本工作日志；GUI 修改仅存在于实验副本的内存状态，未允许保存、生成、Map、PNR、bitstream 或烧录。
- D 盘原工程写入：无。原工程 `D:\TJ375N529_SC431HAI2LCD_Demo_V3` 仍是未经本实验改写的可烧录基线。
- 结果：`FAIL AS EXPECTED / PLL_BR1 MIGRATION NO-GO / HARD SOC PLL REPLAN PATH CLOSED`。
- 下一步门禁：用户将实验副本 `pll_inst1` 改回 `PLL_BL1` 后关闭 Efinity且不保存；不得继续试探其他 PLL。后续工作回到不依赖板上 SoC 的 CPU Host 逻辑、feature tap 采集启用前准备和真实场景阈值标定。

### [M2-12] DSI 资源释放假设审计，确认 `PLL_BL2` 不可释放

- 触发：`PLL_BR1` 替换 `PLL_BL1` 失败后，审计是否能因项目不使用 MIPI DSI 而释放 `PLL_BL2` 给 Hard SoC。
- 实际证据：
  - `src/top.v` 的 DSI 业务模块确实由 `` `ifdef MIPI_DSI_OUT_EN `` 包围，当前 HDMI-only 配置不启用该模块。
  - 但 `pll_inst4/PLL_BL2` 的输出并不只服务 DSI：它生成 `mipi_clk`、`i_sysclk_div2`、`hdmi_tx_slow_clk`、`hdmi_tx_fast_clk`。
  - `mipi_clk` 是两路 `soft_mipi_rx_top` 的输入；`i_sysclk_div2` 是视频处理与 feature tap 时钟；`hdmi_tx_slow_clk/hdmi_tx_fast_clk` 是 HDMI 发射时钟。
  - `PLL_TR0/MIPI_TX_PLL` 才输出 DSI PHY 相关 `mipi_dphy_tx_*` 时钟，但 Hard SoC 合法系统 PLL 不接受 `PLL_TR0`。
- 判定：去除未启用的 DSI 业务逻辑或 `PLL_TR0` 不能释放任何 Hard SoC 可用的 `PLL_BL*`；删除或迁移 `PLL_BL2` 会同时破坏 MIPI CSI、视频处理和 HDMI。该路线关闭。
- 修改文件：本工作日志；未修改 D 盘原工程、实验副本、RTL、`.peri.xml`、约束、IP 或 bitstream。
- D 盘原工程写入：无。
- 结果：`READ-ONLY AUDIT PASS / PLL_BL2 RELEASE NO-GO / HARD SOC INSERTION REMAINS BLOCKED`。
- 下一步门禁：恢复并关闭实验副本且不保存。M2 的板上 Hard SoC 路线暂停；优先推进不依赖 SoC 的视觉可观测性与阈值标定准备，待获得官方支持的共享 PLL/Hard SoC 视频参考设计或另行批准系统级视频时钟重构后再重开 SoC 议题。

### [M2-13] 更正 CPU 接入路径：以 Hard SoC 接管 `PLL_BL1`

- 触发：用户指出 CPU 是正式闭环的硬要求，要求明确最终接入方法；因此重新逐信号审计现有 `PLL_BL1` 的实际下游用途和官方 Hard SoC 参考工程。
- 更正结论：此前“所有 `PLL_BL*` 均不可释放”的表述过度保守。`PLL_BL2` 的确不可释放，因为它驱动 MIPI CSI、视频处理和 HDMI；但当前 `pll_inst1/PLL_BL1` 的两个时钟输出 `pll_inst1_CLKOUT0`、`CLK_5M` 在 `src/` 内没有下游消费者。顶层只实际使用其锁定信号 `sys_pll_lock` 参与 `arst_n`。
- 正式接入方向：在独立 CPU+视频联合工程中，由 Interface Designer 删除旧 `pll_inst1`，释放 `PLL_BL1`；随后创建最小 Hard SoC，选择 `PLL_BL1`、使用现有 74.25MHz `clk_74p25m/GPIOL_32` 作为其合法 GUI 参考时钟，使用 `JTAG_USER2`，仅开 UART0。由同一次生成物提供新的系统 PLL lock、SoC wrapper、`soc.h`、BSP 和 linker；顶层仅在拿到真实生成端口后，将旧 `sys_pll_lock` 复位依赖改接到生成的 SoC system-PLL lock。
- 官方交叉证据：TJ375C529 官方 Hard SoC 工程显示 SoC 系统/内存时钟由 Interface Designer 选择的 `PLL_BL*` 提供，SoC `SYS_CLK_SOURCE`/`MEM_CLK_SOURCE` 由同次生成定义；不得复用其固定 `soc.h`、地址、UART GPIO 或 wrapper。
- 禁止项：不得把 `PLL_BL1` 迁到 `PLL_BR1`；不得移动 `PLL_BL0` 或 `PLL_BL2`；不得手改 `.peri.xml`、生成 RTL、`soc.h` 或约束；不得先写 CPU MMIO 地址或机械臂链路。
- 修改文件：本工作日志；未修改 D 盘原工程、实验副本、RTL、`.peri.xml`、约束、IP 或 bitstream。
- D 盘原工程写入：无。
- 结果：`CPU INTEGRATION PATH IDENTIFIED / GENERATION AND BOARD PROOF NOT VERIFIED`。
- 下一步门禁：用户恢复并关闭当前实验副本且不保存；另建 CPU+视频联合实验副本，从原工程开始，在 GUI 中删除旧 `pll_inst1` 后创建最小 Hard SoC 占用 `PLL_BL1`。生成后立刻停止并回传生成物，由 Codex 审查端口、PLL、UART 和 BSP，再允许改顶层复位适配与进行 Map/PNR。

### [M2-14] CPU+视频联合 SoC 实验工程副本

- 触发：用户关闭前一轮 `PLL_BR1` 迁移失败的实验副本，允许按修正后的 `PLL_BL1` 接管路线创建新的隔离实验环境。
- 目标：从未经 PLL 迁移的原工程副本开始，验证 Hard SoC 能否在 Interface Designer 中合法接管 `PLL_BL1`，同时不改动 DDR、MIPI CSI 和 HDMI 的 `PLL_BL0/PLL_BL2`。
- 前置检查：确认无 `efinity`、`efx_run`、`efx_map`、`efx_pnr`、`efx_pt` 或 `efx_pgm` 进程；再次固定原工程四个关键文件的 SHA-256。
- 实际动作：完整复制 `D:\TJ375N529_SC431HAI2LCD_Demo_V3` 到 `D:\TJ375N529_SC431HAI2LCD_Demo_V3_cpu_video_joint_20260715_161500`。源和副本总文件字节数均为 `211229338`；`mem_test.peri.xml`、`mem_test.xml`、`constrain.sdc`、`src\top.v` 均逐项 SHA-256 一致。
- 修改文件：本工作日志、联合实验副本内 `EXPERIMENT_MANIFEST.md`；未修改原工程、候选工程 RTL、约束、`.peri.xml`、IP 或 bitstream。
- D 盘原工程写入：无。仅新建同级联合实验副本目录。
- 结果：`PASS`。联合实验的起点是当前已验证视频工程，而不是前一轮失败实验的脏状态。
- 下一步门禁：仅打开联合实验副本 `mem_test.xml`；在 Interface Designer 中删除旧 `pll_inst1` 后创建最小 Hard SoC 使用 `PLL_BL1`、`JTAG_USER2`、UART0-only。生成后立刻停止，禁止 Map/PNR/烧录，回传生成物审查。

### [M2-15] 停止手工裸 SoC/PLL 拼装，切换到官方 IP Manager 生成流

- 触发：用户在联合实验副本中手工创建 `qcrv32_inst1/SOC_0` 和新的 `PLL_BL1`，截图显示新 PLL 默认引用 `External Clock 0 / GPIOB_P_05`、仅有 `CLKOUT0`，SoC 的系统/内存时钟实例与引脚尚为空。
- 证据与判定：
  - Efinity 2025.2 自带 `hlp-id-ti-riscv.html` 明确说明：使用 IP Manager 生成 Interface Designer blocks 后，不得再在 Interface Designer 修改其设置，否则会破坏设计。
  - Titanium Hard RISC-V 文档明确规定：`PLL_BL1 CLKOUT2` 才能驱动系统时钟，`PLL_BL1 CLKOUT1` 才能驱动内存时钟；当前手工新 PLL 只有 `CLKOUT0`，不满足硬核物理接口。
  - 手工添加 Interface Designer 的裸 SoC 块不会自动建立与当前配置匹配的 Hard SoC IP wrapper、外围 RTL、BSP、`soc.h` 和 linker，无法满足正式 CPU 软件平台真源要求。
  - 既有隔离生成证据 `C:\fpga_soc_isolated\tj375_soc_a2_20260712` 已证明官方 IP Manager 流可生成 wrapper、外围 RTL、BSP 和 `soc.h`；新联合工程应基于该生成方法重新生成，但参数必须改为本工程的 `PLL_BL1` 和 74.25MHz 参考输入，不能复制旧地址或旧生成物。
- 当前 GUI 状态：所有修改仍未保存到磁盘；联合副本 `mem_test.peri.xml` SHA-256 仍为原始 `507D0ABC16A8AB076C06D0550303760FF15897388233DACD8161AD000E2B0768`。
- 修改文件：本工作日志；未修改原 D 盘工程、候选工程 RTL、约束、IP 或 bitstream。
- 结果：`MANUAL RAW SOC/PLL FLOW STOPPED / OFFICIAL IP MANAGER FLOW REQUIRED`。
- 下一步门禁：关闭联合实验副本且不保存当前 GUI 改动。随后在联合副本中通过 IP Manager 新建最小 Hard SoC IP，参数目标为 `PLL_BL1`、74.25MHz 参考、整数 PLL 模式、UART0、JTAG USER2、无 UART2/机械臂；IP Manager 生成完成后再打开 Interface Designer 审查自动生成的 SoC/PLL/GPIO/JTAG 资源，禁止手工重配生成块。

### [M2-16] Hard SoC IP Manager 最小配置逐页审查（生成前）

- 触发：用户在 CPU+视频联合实验副本中通过 `Tools -> Open IP Catalog` 打开 Efinity 2025.2 IP Catalog，并选择 `Sapphire High Performance SoC`（内部 IP 名 `efx_hard_soc`，版本 `1.22.0`）。
- 本机 IP 真源：`D:\Efinity\2025.2\ipm\ip\efx_hard_soc`；目标器件页为 `TITANIUM / 529`。IP 固定模块名为 `EfxSapphireHpSoc_slb`，GUI 中只读；后续顶层实例名可另行命名，不能手改生成模块名。
- HRB 已确认配置：`FPGA USER TAP`，资源改为 `JTAG_USER2`；保留硬核固定 AXI4 接口；不启用 Co-Debug、AXI4 Slave、custom instruction、AXI pipeline、write-buffer bypass 或 OCR application。
- SLB-i 最小配置：保留 peripheral interconnect、Interface Designer peripheral GPIO block 和 UART0；关闭官方开发套件预定义管脚、UART1/2、SPI0/1/2、I2C0/1/2、GPIO0/1 和 watchdog。`Required SoC interrupt ports` 应为 `1`。
- SLB-ii 最小配置：APB3 0-4、SD Host Controller 和 Triple-speed Ethernet MAC 全部关闭；视觉 APB 留到 UART Hello Gate 通过后再加入。
- PLL 资源审查：默认 `PLL_BL0` 和 `PLL_TR0` 均与现有视频工程冲突，禁止使用。IP Manager 的系统 PLL 与 Peripheral PLL 自动 Interface Designer 生成均须关闭；系统时钟入口仍选择 `PLL_BL1/Clock 1`，由现有 `pll_inst1` 在生成后经单独审查改为 Hard SoC 所需的 `CLKOUT1=297MHz` memory clock、`CLKOUT2=594MHz` system clock。该组合沿用 74.25MHz 参考及现有 594MHz 整数 VCO，不使用 fractional PLL 或 spread spectrum。
- Peripheral clock 契约：生成参数设为 `200MHz`，后续 wrapper 的 `io_peripheralClk` 复用现有稳定 `axi0_ACLK=200MHz`；不新增 Peripheral PLL。IP Manager 隐藏的系统 PLL feedback 默认值为 100MHz，无法与 74.25MHz 参考保持纯整数关系，因此不得让 IP Manager 自动生成系统 PLL。
- LPDDR4 边界：下一页必须关闭自动加入新的 LPDDR4 controller；当前视频工程已有 `DDR_0/lpddr4_pll`，首个 UART Hello 固件仅使用 Hard SoC 片上 RAM，不允许生成第二套 DDR 控制器或改动现有视频 DDR 链路。
- 当前磁盘状态：仅更新本工作日志；Hard SoC 尚未点击 `Generate`，联合实验副本尚无获准的生成物，未运行 Map/PNR，未烧录。
- 结果：`PRE-GENERATION REVIEW IN PROGRESS / NOT YET GENERATED / NOT BOARD VERIFIED`。
- 下一步门禁：用户按上述 PLL 字段配置并回传页面；随后审查 LPDDR4、Embedded software、Deliverables、Summary。只有 Summary 逐项通过后才允许点击一次 `Generate`，生成后必须立即停止并由 Codex 审查所有生成物和资源冲突。

### [M2-17] 首次 Hard SoC 生成被 DDR AXI1 硬规则阻止

- 触发：用户在联合实验副本中确认 Summary 后首次点击 `Generate`；生成范围仅为 `Peripheral_Generator`、`Source`、`PT_Configuration`、`Peripheral_Post_Script`、`Embedded_SW`，未包含 Example Design 或 Testbench。
- 失败原文：`Rule:ddr_rule_axi_1_qcrv32, Msg:DDR_0 AXI 1 Interface cannot be used when QCRV32 is configured`，随后 `Hard IP Build Execution: Failure` 与 `Generate status: Error`。
- 根因：现有 `ddr_inst1/DDR_0` 同时启用了 AXI target0 与 AXI target1；器件在配置 Hard SoC/QCRV32 后会占用 DDR_0 的 AXI1 硬连接，因此 AXI1 不能继续导出为普通 FPGA fabric 接口。
- 视频依赖审计：两个 `frame_buffer` 先分别形成 AXI master，再经写/读 `axi_interconnect` 汇聚，最终仅连接顶层 `axi0_*`。全 `src/` 中 `axi1_*` 只出现在 `src/top.v` 的接口声明和 `assign axi1_ARESETn = ddr_cfg_ok`，没有 framebuffer 或其他读写消费者。
- 最小修复：仅在联合实验副本的 Interface Designer 中关闭 `ddr_inst1 -> AXI 1`，保留 AXI0、DDR 配置端口、现有 LPDDR4 参数与视频 PLL。成功生成并审查生成端口后，再删除 `src/top.v` 中遗留的 AXI1 端口与复位赋值；不得把视频流量迁移到其他 DDR 端口，也不得改 framebuffer 数据链路。
- 失败后磁盘核查：`mem_test.peri.xml` SHA-256 仍为 `507D0ABC16A8AB076C06D0550303760FF15897388233DACD8161AD000E2B0768`，`soc_info` 仍为空；`ip/` 未出现 Hard SoC 目录，`embedded_sw/` 未生成 BSP。失败未形成可用生成物。
- 结果：`EXPECTED HARD-RULE FAILURE / ROOT CAUSE IDENTIFIED / VIDEO AXI0 PATH UNAFFECTED`。
- 下一步门禁：先关闭失败对话框和 IP Configuration，不再次 Generate；在联合实验副本 Interface Designer 中只关闭 DDR AXI1并运行设计校验，回传 DDR 配置与校验结果截图。校验通过后才重新打开 IP Catalog，复核/重填最小 Hard SoC 配置并再次生成。

### [M2-18] 联合实验副本关闭 DDR AXI1并通过 Interface Designer 校验

- 实际操作：用户仅在联合实验副本 `ddr_inst1/DDR_0 -> AXI 1` 页取消 `Enable Target 1`；AXI0、DDR 基础参数、PLL、GPIO、视频 RTL 均未修改。
- 磁盘证据：联合实验副本 `mem_test.peri.xml` 的 `axi_target1` 已变为 `is_axi_enable="false"`，该 AXI1 块下的导出 pin name 由 `axi1_*` 正常清空；AXI0、DDR 参数、PLL、GPIO、MIPI/HDMI 块无差异。该文件 SHA-256 由基线 `507D0ABC...` 变为 `168C965C5155E28F4255A681B13EC39F0673C786E3B76F18977347CC32973E7B`。
- 校验结果：Interface Designer `Check design...done. 4 issue/s.`，4 项均为 warning，无 error。规则分别是既有 `lvds_rule_tx_distance`、`mipi_ln_rule_rx_distance`、两项 `mipi_ln_rule_tx_distance`。
- warning 复核：原 D 盘可烧录视频基线 `outflow/mem_test.pt.rpt` 第 853-856 行已有完全相同的 4 条 warning（同实例、同规则、同描述对象）；本次 AXI1 关闭没有新增 HSIO/MIPI/LVDS 警告。
- 判定：`DDR AXI1 DISABLE VALIDATION PASS / NO NEW INTERFACE WARNINGS`。这些既有物理距离 warning 仍保留为板级已知风险，不因本次复核而从项目中删除。
- 下一步门禁：保存 Interface Designer 修改后，Codex 核查 XML 最小差分；再重新打开 Hard SoC IP 配置，复核 `PLL assignments=0`、`DDR assignment=0`、`JTAG_USER2`、UART0-only 和 peripheral 200MHz，并进行第二次 Generate。仍禁止 Map/PNR/烧录。

### [M2-19] 最小 Hard SoC 第二次生成成功及生成物审计

- 触发：关闭 `DDR_0 AXI 1` 并通过 Interface Designer 校验后，用户按已审核参数重新生成 `Sapphire High Performance SoC`，Efinity 返回生成成功。
- 实验范围：仅 `D:\TJ375N529_SC431HAI2LCD_Demo_V3_cpu_video_joint_20260715_161500`；未修改原 D 盘烧录基线 `D:\TJ375N529_SC431HAI2LCD_Demo_V3`，未同步到候选正式工程，未运行 Map/PNR，未生成或烧录新的联合 bitstream。
- 生成结果：
  - IP：`efx_hard_soc 1.22.0`，固定模块名 `EfxSapphireHpSoc_slb`。
  - 生成目录：`ip\EfxSapphireHpSoc_slb`，共 12 个文件、828653 字节。
  - BSP：`embedded_sw\efx_hard_soc`，共 480 个文件、7276936 字节。
  - 生成时间链：核心 IP、wrapper、`settings.json`、BSP 和 `soc.h` 均在 2026-07-15 20:14 同批生成。
- 生成参数复核：`UART0-only`、`PERI_FREQ=200MHz`、`JTAG_USER2`、系统/内存源均为 `Clock 1`；IP Manager 未自动加入 System PLL、Peripheral PLL 或第二套 LPDDR4 controller。`DDR_0 AXI0` 保持启用，`AXI1` 保持关闭。
- BSP 真源：`UART0=0xE8010000`，`SYSTEM_CLINT_HZ=200000000`，`SYSTEM_RAM_A=0xF9000000`、大小 16KB。生成的 `default.ld` 仍为 `ORIGIN=0x00001000, LENGTH=324K`，不指向这块 16KB 片上 RAM；首个 UART Hello 必须另建应用专用 linker，禁止修改生成 linker。
- 关键文件 SHA-256：
  - `EfxSapphireHpSoc_slb.v`：`0A67C38D7E352CD2E4833E43D2815B81BECDF426FA7610A316C1A3CC6CEE2CC5`
  - `EfxSapphireHpSoc_wrapper.v`：`C63628B93453A054B74F53EAE268441F400B7D749E10239D537053021A0AC428`
  - `settings.json`：`80A4BF306641C5B26F8B27677E38AAD97E50992F426B8B89778033F73399478F`
  - `soc.h`：`87A09A739226C2A46DF33F6F995758D1BF81DD33AE8DE3E41EE0B18143833629`
  - `mem_test.peri.xml`：`3837ABCD92824F5E8ADE28EFF7F7DC4203F0C390A274739997B6CAF488F3C1E2`
- 审计发现与当前阻塞：
  - `PLL_BL1/pll_inst1` 仍是旧视频工程配置，只导出 `pll_inst1_CLKOUT0` 和 `CLK_5M`；缺少 Hard SoC 指定的 `CLKOUT1` memory clock 与合规的 `CLKOUT2` system clock。
  - UART0 的 `system_uart_0_io_rxd/txd` 均为 `gpio_def=""`，尚未分配开发板物理引脚。
  - `src/top.v` 尚未实例化生成 wrapper，仍有已经关闭的 `axi1_*` 遗留顶层接口，SoC reset/PLL lock、`io_peripheralClk`、未使用 AXI A 接口和 DDR 配置状态机隔离均未完成。
- 结果：`HARD SOC IP/BSP GENERATION PASS / TOP-LEVEL AND CLOCK INTEGRATION NOT VERIFIED / MAP-PNR-FLASH NO-GO`。本门只证明官方生成流已经打通，不能描述为 CPU 已上板或摄像头与 CPU 已联合运行。
- 下一步门禁：先在联合实验副本执行生成后的完整 `Validate Design` 并确认无 error；随后仅在 GUI 中配置并校验 `PLL_BL1` 的 Hard SoC 合法输出（74.25MHz 参考、整数模式、`CLKOUT1=297MHz`、`CLKOUT2=594MHz`）。取得校验结果和最终导出端口后，Codex 才能修改联合副本 `top.v`。在此之前继续禁止 Map/PNR/烧录，且不得修改生成的 `.peri.xml`、wrapper、`settings.json`、`soc.h` 或生成 linker。

### [M2-20] Hard SoC 生成后的首次完整 Interface Designer 校验

- 用户回传 Message Viewer 截图，校验结果为 4 个 error、4 个 warning。
- 4 个 error：
  - `io_gpio_sw_n / gpio_rule_resource`：Resource name is empty。
  - `system_uart_0_io_rxd / gpio_rule_resource`：Resource name is empty。
  - `system_uart_0_io_txd / gpio_rule_resource`：Resource name is empty。
  - `qcrv32_inst1 / qcrv32_rule_mem_clk_resource`：`PLL(PLL_BL1).CLKOUT1` driving QCRV32 memory clock is not configured。
- 4 个 warning：`lvds_rule_tx_distance`、`mipi_ln_rule_rx_distance`、两项 `mipi_ln_rule_tx_distance`，与原可烧录视频基线的 4 项已知 warning 相同；本次未出现新增视频链路 warning。
- 官方 TJ375C529 RISC-V 例程管脚审计：UART0 RX=`GPIOR_165`、TX=`GPIOR_145`，当前联合实验工程中均未被其他 GPIO 占用，可作为后续 UART0 候选管脚；官方 reset `io_gpio_sw_n=GPIOL_52` 不能直接照搬，因为本视频工程已将 `GPIOL_52` 分配给 `i_sw[0]`，且 `i_sw[0]` 同时复位现有系统、DDR、MIPI TX 和 byte-clock PLL。
- 判定：`VALIDATION EXECUTED / EXPECTED INTEGRATION ERRORS CONFIRMED / MAP-PNR-FLASH NO-GO`。先修复 `PLL_BL1 CLKOUT1/CLKOUT2`，再统一设计视频与 SoC 的复位入口；不得把两个逻辑 GPIO 重复分配到 `GPIOL_52`。
- 下一步门禁：三个未分配 GPIO 暂不填写。先打开 `pll_inst1/PLL_BL1` 完整 GUI 配置页，保留 74.25MHz 外部参考、整数模式和 `sys_pll_lock`，由 Codex根据实际字段指导增加 Hard SoC memory/system clock；配置前后均不得 Map/PNR/烧录。

### [M2-21] `PLL_BL1` 首次补齐 Hard SoC 输出及 memory clock 上限纠偏

- 用户通过 PLL Clock Calculator 保留 74.25MHz 外部参考、Integer/Local Feedback、`sys_pll_rstn` 和 `sys_pll_lock`，新增 `CLKOUT1=soc_memory_clk`，并将 `CLKOUT2` 改为 `soc_system_clk`；首次填写分别为 297MHz 和 594MHz。
- 再次 Check Design 后，原 `PLL_BL1.CLKOUT1 is not configured` error 已消失，证明 Hard SoC system/memory 固定输出编号和资源连接正确。
- 新出现 `qcrv32_rule_mem_clk_resource` warning：`The memory clock frequency exceeds the maximum specification of 250MHz.`。该 warning 属于硬核频率规格，禁止忽略。
- 纠偏：此前工作日志和操作指导中的 `CLKOUT1=297MHz` 方案不合规，现予以废止。当前自动计算组合的内部频率为 2376MHz，保留 `CLKOUT2=594MHz` 时，`CLKOUT1=237.6MHz`（整数除 10）是低于 250MHz 上限且最接近上限的合规候选；仍须再次 Check Design 验证。
- 当前校验剩余：3 个未分配 GPIO error、原有 4 个 MIPI/LVDS warning、1 个超频 warning。未运行 Map/PNR，未烧录。
- 下一步门禁：重新打开 `pll_inst1 -> Automated Clock Calculation`，仅把 Clock 1 从 297MHz 改为 237.6MHz；Clock 0=74.25MHz、Clock 2=594MHz、反馈、相位、reset/lock 均保持不变。Finish 后再次 Check Design，必须确认超频 warning 消失，才进入 GPIO/复位整合。

### [M2-22] Hard SoC PLL 校验通过及最小 GPIO 资源规划

- 用户将 `PLL_BL1 CLKOUT1/soc_memory_clk` 从 297MHz 修正为 237.6MHz，保留 `CLKOUT2/soc_system_clk=594MHz`、`CLKOUT0=74.25MHz`、Integer/Local Feedback、0°相位、`sys_pll_rstn` 和 `sys_pll_lock`。
- Check Design 结果：QCRV32 memory clock 超频 warning 已消失；当前只剩 3 个未分配 GPIO error 和与原视频基线相同的 4 个 MIPI/LVDS warning。判定 `HARD SOC PLL INTERFACE VALIDATION PASS`，但顶层、时序、PNR、bitstream 和板级运行仍未验证。
- GPIO 规划审计：
  - UART0 RX 采用官方 TJ375C529 RISC-V 例程资源 `GPIOR_165`，封装脚 E9，3.3V；当前视频工程空闲。
  - UART0 TX 采用官方 TJ375C529 RISC-V 例程资源 `GPIOR_145`，封装脚 E10，3.3V；当前视频工程空闲。
  - `io_gpio_sw_n` 不复用官方例程的 `GPIOL_52/U19`，因为该脚已是视频工程 `i_sw[0]`，负责现有四路 PLL 复位。第一版改用空闲轻触键 SW2/U17，对应 `GPIOL_79`、3.3V、按下为低，作为独立 SoC reset，避免按 CPU reset 时重置 DDR/MIPI/HDMI。
- 电平边界：三个生成 GPIO 当前默认显示 1.8V，必须在 Interface Designer 中改为 `3.3 V LVCMOS`；不得将 3.3V 板载按键或 UART 通道配置为 1.8V。
- 下一步门禁：只在联合实验副本为三个 GPIO 填写上述 resource 与 3.3V I/O standard，随后再次 Check Design。若出现 resource conflict、bank voltage、HSIO distance 或任何新增 warning/error，立即停止并回传；仍禁止 Map/PNR/烧录。

### [M2-23] 三个 SoC GPIO 在 Resource Assigner 中完成临时分配

- 用户回传 Resource Assigner 的 Instance View：`io_gpio_sw_n=U17/GPIOL_79`、`system_uart_0_io_rxd=E9/GPIOR_165`、`system_uart_0_io_txd=E10/GPIOR_145`，三项均与 M2-22 规划一致，未覆盖 `i_sw[0]/U19/GPIOL_52` 或其他现有视频 GPIO。
- `io_gpio_sw_n` Base 页已截图确认 `input + 3.3 V LVCMOS`；RX/TX 的 I/O Standard 尚缺截图或校验结果确认。
- 当前 GUI 修改尚未保存，因此磁盘 `mem_test.peri.xml` 仍保留三个空 `gpio_def` 和生成器默认 1.8V，SHA-256 仍为 `90A837032907690FC8F8D53D6457267CE464851ADC80EEE14E85EA1BF4494365`；该现象不表示 GUI 分配失败。
- 结果：`RESOURCE ASSIGNMENT IN GUI PASS / UART I/O STANDARD AND DESIGN CHECK PENDING / MAP-PNR-FLASH NO-GO`。
- 下一步门禁：分别选中 UART0 RX/TX，保持 RX=input、TX=output并将两者 Base 页改为 `3.3 V LVCMOS`，其他属性不改；随后 Check Design。必须先取得 0 error 或仅有已知 warning 的验证结果，才允许保存联合实验副本。

### [M2-24] Hard SoC 时钟与 GPIO 的 Interface Designer 校验通过

- 用户确认 UART0 RX=`GPIOR_165/E9/input/3.3 V LVCMOS`，UART0 TX=`GPIOR_145/E10/output/3.3 V LVCMOS`，TX drive strength 保持 4mA；独立 SoC reset 为 `io_gpio_sw_n=GPIOL_79/U17/input/3.3 V LVCMOS`。
- Check Design 结果为 0 error、4 warning。QCRV32 system/memory clock、PLL non-fractional、GPIO resource、电平、DDR AXI1、JTAG 和 Hard SoC 资源规则均未再报错。
- 剩余 4 项为原可烧录视频基线中已存在且实例/规则相同的物理距离 warning：`tm ds_data2/lvds_rule_tx_distance`、`mipi_rx_dp01/mipi_ln_rule_rx_distance`、`mipi_tx_ck0/mipi_ln_rule_tx_distance`、`mipi_tx_dp12/mipi_ln_rule_tx_distance`。本次不将其删除或泛化为可忽略，只判定 Hard SoC 接入没有新增 Interface Designer issue；后续仍必须以 PNR、时序、CDC 和 J48/ch0 HDMI 板级回归复核。
- 结果：`INTERFACE DESIGN VALIDATION PASS / SAVE AND DISK AUDIT PENDING / TOP-LEVEL MAP-PNR-FLASH NO-GO`。
- 下一步门禁：仅保存 CPU+视频联合实验副本的 Interface Designer 修改并关闭 Efinity。关闭后由 Codex 审计落盘 `.peri.xml` 最小差分、真实生成端口、PLL/DDR/GPIO/JTAG 配置，再修改联合副本 `src/top.v`；原 D 盘烧录基线和候选正式工程暂不改动。

### [M2-25] Interface PASS 落盘审计、工程外 checkpoint 与 peripheral clock 补充门

- 用户保存并关闭 Interface Designer 后，后台残留 `efinity.exe` PID 32320；按门禁停止写入，用户结束进程后继续。健康检查通过，只有与本任务无关的 `pymycobot` 未安装 warning。
- 落盘审计确认：`io_gpio_sw_n=GPIOL_79/3.3V`、UART0 RX=`GPIOR_165/3.3V`、TX=`GPIOR_145/3.3V`；`PLL_BL1 CLKOUT1=soc_memory_clk/237.6MHz`、`CLKOUT2=soc_system_clk/594MHz`；DDR AXI1关闭；QCRV32/JTAG_USER2/Clock 1 配置与截图一致。`mem_test.xml` 已自动登记 Hard SoC IP 和 include path。
- 在工程外建立 checkpoint：`C:\Users\20306\Desktop\赛题资料\CICC_backups\TJ375N529_cpu_video_joint\20260715_211500_interface_pass`，保存 `mem_test.peri.xml`、`mem_test.xml`、`constrain.sdc`、`src/top.v`、Hard SoC `settings.json` 和生成 RTL；哈希见该目录 `BACKUP_MANIFEST.md`。
- 顶层前置审计发现：QCRV32 的硬接口 pin name 仍为 `io_peripheralClk`，而本方案复用的现有稳定 200MHz 时钟是 `axi0_ACLK`。Hard SoC 数据手册确认 `IO_PERIPHERALCLK` 是硬核输入并为外设与 AXI slave 接口提供时钟；不能只在 RTL wrapper 侧改名而让硬核 pin 保持另一个悬空顶层端口。
- 结果：`INTERFACE PASS CHECKPOINT SAVED / PERIPHERAL CLOCK PIN NAME CORRECTION REQUIRED / TOP EDIT DEFERRED`。
- 下一步门禁：重新打开联合实验副本，在 `qcrv32_inst1 -> Clock/Control` 中只将 `Periphery Controller Clock Pin Name` 从 `io_peripheralClk` 改为 `axi0_ACLK`，不反相、不改 system/memory clock/reset。Check Design 必须仍为 0 error且不新增 warning；保存关闭并审计落盘后，才修改 `src/top.v`。

### [M2-26] Hard SoC 顶层适配与失效约束最小清理

- 时间：2026-07-15 21:36（Asia/Shanghai）。
- 触发：用户确认 Efinity 已关闭，批准继续联合实验副本的 Hard SoC 顶层适配。
- 适用工程：仅 `D:\TJ375N529_SC431HAI2LCD_Demo_V3_cpu_video_joint_20260715_161500`。
- 前置门：
  - 使用 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\agent_handoff_health_check.ps1` 完成只读健康检查；结果 PASS，唯一 warning 为本机未安装 `pymycobot`，与本轮 SoC/视频 RTL 无关。
  - 读取并遵守 `.agents/skills/fpga_vision/SKILL.md` 与 `.agents/skills/cpu_mycobot/SKILL.md`；本轮不涉及 CPU 识别软件或机械臂动作。
  - Codebase Memory 服务当前没有加载 `D-cicc_cbm-main`，因此按仓库规则退回真实 `top.v`、`mem_test.peri.xml`、生成 wrapper、template、`mem_test.isf` 和官方初赛例程核查。
- 修改前备份：
  - Interface PASS checkpoint：`C:\Users\20306\Desktop\赛题资料\CICC_backups\TJ375N529_cpu_video_joint\20260715_211500_interface_pass`。
  - 顶层修改前备份：`C:\Users\20306\Desktop\赛题资料\CICC_backups\TJ375N529_cpu_video_joint\20260715_212500_top_pre_soc_adapter\src\top.v`。
- `src/top.v` 实际修改：
  - 删除已失效 `CLK_5M`、`pll_inst1_CLKOUT0`、旧 `jtag_inst2_*` 和全部已关闭的 `axi1_*` 顶层端口；删除遗留 `assign axi1_ARESETn = ddr_cfg_ok`。
  - 新增 `soc_memory_clk`、`soc_system_clk`、QCRV32 reset、UART0、USER2 JTAG、完整 AXI-A、`axiAInterrupt` 和 `userInterruptA-H` 顶层硬接口。
  - 实例化生成模块 `EfxSapphireHpSoc_slb`，其 63 个端口全部且仅连接一次；未修改生成 wrapper。
  - `io_peripheralClk` 接现有 `axi0_ACLK=200MHz`；`pll_peripheral_locked` 接 `ddr_pll_lock`，`pll_system_locked` 接 `sys_pll_lock`。
  - wrapper 的 `cfg_done` 只读接现有 `ddr_inst_CFG_DONE`；wrapper 的 `cfg_start/cfg_sel/cfg_reset` 接独立 unused wire。现有视频状态机继续唯一驱动 `ddr_inst_CFG_START/RST/SEL`，关闭双驱动风险。
  - UART0 中断接 `userInterruptA`；未使用 `userInterruptB-H` 明确拉低。`io_peripheralReset`、`io_systemReset` 保持 QCRV32 硬核输出到 fabric 的方向，不互相环回。
  - 摄像头、framebuffer、AXI0、Debayer、feature tap、白平衡和 HDMI 像素链未修改。
- `constrain.sdc` 实际修改：
  - 删除 81 处已关闭 `axi1_*` 约束、4 处 `CLK_5M` 引用、3 处旧 `pll_inst1_CLKOUT0` 引用。
  - 新增 `soc_system_clk` 1.684ns（594MHz）与 `soc_memory_clk` 4.209ns（237.6MHz）时钟定义。
  - 保留现有 CSI recovered-clock 异步约束、AXI0、MIPI、HDMI及项目后加约束；未机械重生成或覆盖整份文件。
- 静态验证：
  - wrapper/instance 端口：`63/63`，missing=0、extra=0、duplicate=0。
  - `top.v` 与 `constrain.sdc` 中 `axi1_`、`CLK_5M`、`pll_inst1_CLKOUT0`、`jtag_inst2_` 残留均为 0。
  - 关键 `.peri.xml` 硬接口方向核对通过；与修改前 `top.v` 的 `git diff --no-index` 仅出现顶层端口、SoC实例、中断拉低和 AXI1 复位删除，视频处理主体无差分。
  - 本机无 `iverilog`、`verilator`、`vlog`；按门禁未运行 `efx_map`、PNR、bitstream 或烧录。
- 当前哈希：
  - 联合副本 `src/top.v`：`E79CB53A9960455B3B033AC2C329E90FCF81F3299BD36AA5C34DA221F6CB6834`。
  - 联合副本 `constrain.sdc`：`3C0A58F318A3981984D7C544F26F9844FE85E27E680BDFA4C17B0988D0360994`。
  - 联合副本 `mem_test.peri.xml` 未手改，仍为 `5B530FD3F7FCDAEE1F6429482DE275C1014CB40988662A57926768E7A38B424D`。
- 未修改/未同步证明：
  - 原烧录基线 `D:\TJ375N529_SC431HAI2LCD_Demo_V3\src\top.v` 仍为 `746BAD51743E46DBCD2D89DBFF8CED3A89F00999DF0B5312B62A2D00B1C5D343`。
  - 原烧录基线 `constrain.sdc` 仍为 `B6E30866ED09CADCA083FCE4D7A2D831A90E3C0E689CBB4D3B97369874AE316D`。
  - 未同步 `competition_project_single_camera` RTL/SDC；该目录本轮只更新本工作日志。
- 已知风险：生成 IP 的内部 LPDDR4-init `cfg_count` 没有显式复位，这是官方生成 RTL 现状；禁止手改生成物。若后续 CPU 无法解除 reset，需结合官方例程、Map warning 和板级 UART/JTAG现象做最小诊断。
- 结果：`TOP-LEVEL STATIC ADAPTER PASS / MAP-PNR-BOARD NOT VERIFIED`。本结果不表示 CPU 已启动、UART 已输出、视频与 CPU 已联合上板或时序已签核。
- 下一步人工门：用户在联合实验副本运行 Efinity 综合/Map；回传完整 error/warning。Map 通过后再运行 PNR，并记录 Setup/Hold、CDC、4项既有 MIPI/LVDS warning和新增 warning；通过后才生成联合 bitstream，首先回归 J48/ch0 HDMI，确认摄像头画面不退化，再进行 SW2 reset、UART0 Hello/JTAG 验证。通过之前不得覆盖原 D 盘烧录基线或同步正式候选 RTL。

### [M2-27] 首次联合 Map PASS 与 Debugger USER2 冲突修复

- 时间：2026-07-15 21:46（Asia/Shanghai）。
- 用户执行：在联合实验副本运行 Efinity automated flow。综合/Map 本体完成，随后 Debugger Step 3 `Instantiate Jtag` 失败。
- Map 真实结果：
  - `EFX_MAP` 完成，用时 `38.5929642s`。
  - 资源：`EFX_ADD=2065`、`EFX_LUT4=11204`、`EFX_FF=9957`、`EFX_RAM10=165`、`EFX_DPRAM10=4`。
  - 2026-07-15 21:39:54 本轮 `mem_test.err.log` 没有新增 RTL/elaboration error；旧日志中历史 `feature_stats_tap` error 不属于本轮。
  - post-synthesis netlist 报告 118 条 warning，尚未逐项关闭，不得整体写成可忽略。
- Debugger 失败原文：`Jtag resource = JTAG_USER2 has been occupied`，`efx_pt_jtag_util.py --action add --jtag_user JTAG_USER2` 返回 exit code 3。
- 根因审计：
  - 原视频工程 `jtag_inst1` 已占用 `JTAG_USER1`。
  - Hard SoC 的 `soc_jtag_inst1` 已占用 `JTAG_USER2`，用于 QCRV32 USER TAP。
  - `mem_test.xml` 仍设置 `debugger.auto_instantiation=on`，旧 `debug_profile.wizard.json` 也指定 `USER2`，导致综合后 Debugger 再次申请已被 CPU 占用的 USER2。
  - USER1/USER2 均已有合法用途，不能把 CPU 移到 USER1，也不能让 Debugger覆盖 USER2。
- 修改前工程外备份：`C:\Users\20306\Desktop\赛题资料\CICC_backups\TJ375N529_cpu_video_joint\20260715_214613_pre_disable_autodebug`；备份 `mem_test.xml` SHA-256 为 `21AC8C2E161E86F3021CEDD54BC113DD193B6278BF4188D3A5D87588EE1AEFA7`，清单见同目录 `BACKUP_MANIFEST.md`。
- 实际修复：仅将联合实验副本 `mem_test.xml` 的 `<efx:param name="auto_instantiation" value="on" .../>` 改为 `value="off"`。保留 debugger profile 文件但自动流程不再插入 Debugger JTAG。
- 修复验证：
  - XML 解析 PASS，`AUTO_INSTANTIATION=off`。
  - 与备份的差分只有该单字段 `on -> off`。
  - 新 `mem_test.xml` SHA-256：`394685F36E7B0A6301C9E9C03A195357B7DE10CC058DBBECA566901E1FC8CDC6`。
  - `mem_test.peri.xml` 仍为 `5B530FD3F7FCDAEE1F6429482DE275C1014CB40988662A57926768E7A38B424D`。
  - `src/top.v` 仍为 `E79CB53A9960455B3B033AC2C329E90FCF81F3299BD36AA5C34DA221F6CB6834`。
  - `constrain.sdc` 仍为 `3C0A58F318A3981984D7C544F26F9844FE85E27E680BDFA4C17B0988D0360994`。
- 未修改边界：未改 `mem_test.peri.xml`、Hard SoC生成 IP、USER1/USER2资源、视频 RTL、时钟复位、约束或原 D 盘烧录基线；未同步正式候选工程。
- 结果：`MAP PASS / DEBUGGER RESOURCE CONFLICT ROOT-CAUSED AND CONFIG FIXED / PNR NOT VERIFIED`。
- 下一步人工门：重新打开联合实验副本并运行正常 automated flow。预期不再出现 `Start Debugger Step Instantiate Jtag`；必须继续取得 PNR、Setup/Hold、CDC和warning结果。若流程仍进入 Debugger Step 3或出现新的 error，立即停止并回传完整尾部日志；暂不烧录。

### [M2-28] 联合 PNR/bitstream PASS 与 J48/ch0 HDMI 板级回归

- 时间：2026-07-15 22:03（Asia/Shanghai）。
- 用户执行：关闭 Debugger自动实例化后重新运行联合实验工程 automated flow，生成新 bitstream，并在板上烧录后回传 J48/ch0 HDMI 实拍。
- 构建结果：
  - Map、placement、routing、STA和bitstream generation均完成。
  - `outflow/mem_test.bit` 时间为 2026-07-15 21:50:58，SHA-256：`AA133887F3D5CE19768C35C3E1775019D370AAC66FE95ABEE46503A63BA96F31`。
  - `outflow/mem_test.hex` 时间为 2026-07-15 21:51:02；bitstream generator 正常结束。
- 最终时序：
  - 最差 Setup Slack：`+1.742ns`，路径位于生成的 Hard SoC UART/AXI peripheral wrapper，时钟为 `axi0_ACLK=200MHz`。
  - 最差 Hold Slack：`+0.018ns`，时钟为 `axi0_ACLK=200MHz`。
  - 所有 Clock Relationship Summary 中列出的 Setup/Hold slack 均为非负；本次没有时序违例。
  - STA报告未列出 `soc_system_clk`/`soc_memory_clk` 的 fabric数据路径，因为它们直接驱动 QCRV32硬核；Interface/Periphery最终报告已确认 `soc_system_clk=594MHz`、`soc_memory_clk=237.6MHz`。
- CDC：`outflow/mem_test.cdc.rpt` 明确为 `No Synchronizer warnings to report.`。
- Interface/Periphery：QCRV32 `SOC_0` 已进入最终报告；只保留与原视频基线相同的4项 `mipi_ln_rule_*_distance` / `lvds_rule_tx_distance` warning。本条只记录其为既有项，不将其从风险清单删除。
- 板级视频证据：
  - 用户实拍：`C:\Users\20306\AppData\Local\Temp\codex-clipboard-79d27dce-11c1-47e0-baf7-ebf840424b3f.png`，SHA-256：`412CEE704039DF063CBCFBFBE56003A37BF1EFFD1C13C39CB8A81A7C946D9659`。
  - 画面中五个不同颜色方块均可见，J48/ch0 HDMI有真实摄像头输出，未出现黑屏、花屏、整帧冻结或颜色通道完全错位。
  - 成像仍存在暗部、轻微偏色、右侧拖影与白色高光过曝；这些属于此前成像质量问题，当前只判定视频链路未因 Hard SoC 接入回退，不判定识别精度或图像质量已经达标。
- 结果：`M0 VIDEO REGRESSION PASS + HARD SOC PNR/BITSTREAM PASS / CPU EXECUTION NOT YET VERIFIED`。
- CPU下一门审计：生成 BSP 的片上 RAM 为 `SYSTEM_RAM_A=0xF9000000/16KB`，UART0为 `0xE8010000/115200`。首个 UART Hello 必须使用生成的 `default_i.ld`（片上 RAM），不能使用指向 `0x00001000/324KB` 的 `default.ld`；不得修改生成 linker。当前仍未生成/加载 Hello ELF，未证明 CPU 已取指、JTAG可调试或UART有输出。
- 下一步门禁：关闭 Efinity 后，在联合实验副本旁创建独立最小 UART Hello 应用，复用生成 BSP和 `default_i.ld`，先做构建与 ELF/段地址审计；用户再通过 QCRV32 USER2 JTAG加载到片上 RAM并观察 UART0。此阶段不接机械臂、不迁移识别主循环、不同步原烧录基线。

### [M2-29] QCRV32 片上 RAM UART0 Hello 构建闭环

- 时间：2026-07-15，用户确认Efinity关闭后执行。
- 适用范围：仅CPU取指与UART0收发验证；不访问视觉特征、OSD、外部DDR、UART2或myCobot。
- 工具链：Efinity RISC-V IDE 2025.2自带 `riscv-none-embed-gcc 8.3.0` 与 GNU Make 4.2.1。
- 官方生成物审计：
  - `soc.h`：片上RAM `0xF9000000/16KB`，UART0 `0xE8010000`，peripheral/CLINT频率200MHz。
  - 生成的 `default_i.ld` 指向片上RAM，适合本次Hello；`default.ld` 指向旧 `0x00001000/324KB`，禁止使用。
  - 官方 `start.S` 在 `-DSMP` 下只允许hart0进入`main`，其余hart等待，因此保留`-DSMP`避免四核重复打印。
- 新建独立应用：`D:\TJ375N529_SC431HAI2LCD_Demo_V3_cpu_video_joint_20260715_161500\cpu_bringup\uart_hello_onchip`。
- 文件：
  - `src/main.c`：只配置UART0为115200 8N1，打印唯一横幅并轮询回显。
  - `makefile`：固定生成BSP、`default_i.ld`、`-DSMP -Wall -Wextra -Werror`。
  - `build.ps1`：固定2025.2工具链，构建后自动审计唯一LOAD段、片上RAM边界和未解析符号。
  - `README.md`：中文构建与上板边界。
- 首次构建结果：严格`-Werror`因官方生成`bsp.h`包含的`semihosting.h/print.h/uart.h/clint.h`自身注释与未使用静态函数warning失败；Hello业务代码没有报错。未通过关闭`-Werror`规避。
- 最小修复：Hello只包含生成的干净地址真源`soc.h`，局部实现官方UART寄存器语义；未修改任何生成BSP头文件、driver或linker。
- 最终构建：PASS。
  - ELF：`build/uart_hello_onchip.elf`，SHA-256 `30A499F5EA2E91AF531FA3991EEB8F2CC9757847E27977EE8238060ADF7A2757`。
  - Entry：`0xF9000000`。
  - 唯一LOAD段：`0xF9000000..0xF9000A2F`，MemSiz `0xA30=2608B`，占16KB的15.92%。
  - `ELF_LOAD_AUDIT=PASS`，没有未解析符号；`_sp=0xF9000A30`。
  - HEX SHA-256：`8F4AF6178128CB72CD84DB5B0F048B1A35B7939DC045A08EB461F51E052616F4`。
  - BIN SHA-256：`C4C499DECD9349770C1703C2056825C25D02147FA95656CBA4A7EE80D6A76647`。
- UART候选：只读枚举为COM9/COM10/COM13，均为FTDI `VID:PID=0403:6011`不同通道；尚未从资料锁定E9/E10对应的唯一COM，禁止猜测后向机械臂通道发送数据。
- 用户操作说明：`competition_project_single_camera/docs/debug_sessions/m2_uart0_onchip_hello_operator_guide_20260715.md`。
- 结果：`UART HELLO ELF BUILD/ADDRESS AUDIT PASS / USER2 JTAG LOAD AND UART BOARD OUTPUT NOT VERIFIED`。
- 下一步门禁：用户在Efinity RISC-V IDE中新建TJ375/QCRV32硬件Debug配置，ELF选择上述文件，JTAG必须是FPGA USER2，只下载到RAM、不写Flash；每到Target/JTAG关键页先截图确认。看到完整横幅并成功回显一个字符后，才允许记录CPU执行与UART0 PASS。

### [M2-30] 关机后恢复检查与 CPU Hello 续跑入口

- 时间：2026-07-15（Asia/Shanghai），用户要求从暂停点继续。
- 交接健康检查：运行 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\agent_handoff_health_check.ps1`，结果 PASS；唯一 warning 为本机未安装 `pymycobot`，与本轮 CPU/UART0 测试无关，不安装、不连接机械臂。
- 联合 bitstream 复核：`D:\TJ375N529_SC431HAI2LCD_Demo_V3_cpu_video_joint_20260715_161500\outflow\mem_test.bit` SHA-256 仍为 `AA133887F3D5CE19768C35C3E1775019D370AAC66FE95ABEE46503A63BA96F31`，与已通过 J48/ch0 HDMI 回归的联合位流一致。
- Hello ELF 复核：`D:\TJ375N529_SC431HAI2LCD_Demo_V3_cpu_video_joint_20260715_161500\cpu_bringup\uart_hello_onchip\build\uart_hello_onchip.elf` SHA-256 仍为 `30A499F5EA2E91AF531FA3991EEB8F2CC9757847E27977EE8238060ADF7A2757`，与已通过片上 RAM 地址审计的 ELF 一致。
- 恢复时只读枚举结果：未发现 COM 端口；未发现 Efinity/RISC-V IDE/OpenOCD 进程。该现象与关机后板卡或 JTAG-IF USB 尚未重新连接/上电一致，但尚未做物理确认，不据此诊断硬件故障。
- 未修改边界：未修改联合副本、原 D 盘烧录基线、正式候选 RTL/SDC/XML、生成 BSP/linker、bitstream 或 ELF；未同步联合副本到正式候选工程。
- 结果：`RECOVERY HEALTH CHECK PASS / BITSTREAM+ELF HASH MATCH / BOARD UART PORTS NOT ENUMERATED`。
- 下一步门禁：重新连接并上电开发板/JTAG-IF USB，确认 COM 端口重新出现后，在 Efinity RISC-V IDE 中建立 TJ375/QCRV32 硬件 Debug 配置；只通过 FPGA `USER2` 将上述 ELF 下载到片上 RAM `0xF9000000`。禁止 USER1、Flash erase/program、外部 DDR 初始化和机械臂连接。每到 Target/JTAG 关键页面先截图确认。

### [M2-31] 板卡不在场后的离线 softTap/FTDI 审计

- 时间：2026-07-15（Asia/Shanghai）。
- 用户现场边界：用户明确开发板当前不在身边。因此停止 CPU Hello 板级操作，不把无 COM 端口误判为硬件故障，不越过 `CPU EXECUTION + UART0` 验收门。
- 进程处理：终止正在运行的官方资料检索并关闭本轮启动的 Efinity RISC-V IDE；复核结果为 `IDE_CLOSED`。未启动 OpenOCD、GDB或串口发送。
- UART 引脚复核：联合副本 `mem_test.peri.xml` 中 `system_uart_0_io_rxd=GPIOR_165`、`system_uart_0_io_txd=GPIOR_145`、3.3V LVCMOS；官方 TJ375 RISC-V工程使用相同 GPIO 方向组合。该项只证明 SoC UART0 GPIO 选择与官方方法一致，不证明板级串口已通。
- FTDI 历史枚举：Windows 注册表只读记录为子接口 A=`COM9`、C=`COM10`、D=`COM13`，均属于同一 FTDI `VID:PID=0403:6011`；B没有 COM记录。尚无原理图证据把 A/C/D 中某个端口唯一对应到 E9/E10，因此仍执行“先监听横幅，确认后才发单字符”的规则。
- OpenOCD审计：
  - 联合 BSP `external.cfg` 使用 FTDI `VID:PID=0403:6011` channel 0。
  - 联合 BSP `debug_softTap.cfg` 为4核QCRV32创建 USER TAP调试目标，work area为 `0xF9000000`，启动时仅halt目标。
  - 同一生成配置仍保留模板旧值 `instr_addr=0x00001000`；不得让该值覆盖已审计 ELF 的 `_start=0xF9000000`。
- 官方 launch审计：TJ375官方 `uartEchoDemo_softTap.launch` 使用 `Debug in RAM`、加载ELF和symbols、OpenOCD `external.cfg + debug_softTap.cfg`、停在`main`；未包含Flash program/erase命令。但它绑定旧Eclipse项目和旧绝对路径，未原样复制，也未在无板状态下伪造联合 `.launch`。
- 文档更新：本次同步更新 `CURRENT_STATE.md` 和 `docs/debug_sessions/m2_uart0_onchip_hello_operator_guide_20260715.md`，明确板卡阻塞、softTap配置事实、`0x00001000`停止条件和FTDI证据边界。
- 未修改边界：未修改联合副本的 FPGA/SoC/CPU 源码、生成BSP/OpenOCD配置、工程XML、SDC、bitstream或ELF；未修改原D盘烧录基线；未同步正式候选RTL；未连接机械臂。
- 结果：`OFFLINE DEBUG FLOW AUDITED / BOARD GATE PAUSED / CPU EXECUTION + UART0 NOT VERIFIED`。
- 下一步门禁：开发板可用后先重新枚举端口并确认联合 bitstream仍在板上；启动RISC-V IDE后逐页确认当前BSP、QCRV32、USER2、Debug in RAM、ELF入口`0xF9000000`。看到完整横幅后才确定UART0 COM并发送一个ASCII字符做回显。通过前不迁移识别主循环，不同步联合改动到正式候选工程。

### [M2-32] 个人分支上传与队友合并交接说明

- 时间：2026-07-15（Asia/Shanghai）。
- 用户目标：准备把联合实验成果整理到个人分支，并输出一份供队友合并使用的中文说明。
- 审查结论：联合副本可以作为 `WIP/联合SoC候选版` 迁入 `competition_project_single_camera/` 并上传个人分支；CPU Hello尚未通过USER2 JTAG和UART0板级验证，因此不得标记为正式闭环或替代 `final_project/`。
- 分支建议：旧 `dev/libaoxun688` 相对当前 `main` 已落后较多，建议从最新 `main` 新建 `dev/libaoxun688-single-camera-soc-wip`，不要在当前共享脏工作树中盲目切换或清理。
- 差分审计：候选工程与联合副本的 `feature_stats_tap.v`、`frame_buffer.v`、`frame_info_det.v` 哈希已经一致；待迁移的系统层为 `mem_test.xml`、`mem_test.peri.xml`、`constrain.sdc`、`src/top.v`、Hard SoC IP/BSP以及UART0 Hello可复现源码。
- 上传边界：提交源码、工程真源、必要生成IP、最小BSP依赖、测试和文档；禁止提交 `outflow/work_*`、bit/hex/elf/bin、数据库、报告日志、`.metadata`、IP pickle缓存、`.mcp.json`和工程外备份。
- 新增交接：`docs/review_packets/m2_single_camera_soc_branch_merge_handoff_20260715.md`。文档包含分支策略、白名单/黑名单、四个系统文件冲突规则、合并前检查、Gate A-D验证顺序、停止条件、回退方式和PR描述模板。
- 系统文件门：`mem_test.peri.xml` 禁止逐行拼接；`mem_test.xml` 必须同时保留feature tap、Hard SoC登记和`auto_instantiation=off`；`top.v`禁止ours/theirs盲选；SDC必须与顶层和periphery同批审查。
- 未执行：本条只生成合并交接文档，尚未把联合副本覆盖到候选工程，尚未新建/切换/提交/推送分支，也未运行新的Efinity构建或上板操作。
- 结果：`TEAM MERGE HANDOFF WRITTEN / WIP UPLOAD BOUNDARY FROZEN / MIGRATION AND COMMIT NOT YET EXECUTED`。
- 下一步：按交接文档建立新个人分支并执行白名单迁移；迁移后先做Git差分和工程完整性审查，再决定是否提交。CPU Hello上板门仍保持 `NOT VERIFIED`。

### [M2-33] Hard SoC 系统真源补交与个人分支发布

- 日期：2026-07-16（Asia/Shanghai）。
- 管理员要求：只允许选择一个同时匹配bitstream `AA133887...F31`、Setup/Hold `+1.742/+0.018ns`、CDC无同步器warning和J48/ch0板级视频正常的完整工程副本；禁止按时间选取或跨副本拼接。
- 唯一来源：`TJ375N529_SC431HAI2LCD_Demo_V3_cpu_video_joint_20260715_161500`。已直接复核同一副本的bitstream哈希、STA行、CDC原文、Efinity版本、Check Design问题表、Map/Placement/Routing报告和工程资源字段；板级J48/ch0结果由M2-28同一bitstream记录与截图哈希绑定。
- 分支：从最新 `origin/main@4e35b05453c1cd30c943bb3d567fd0316ca6bdde` 建立独立worktree分支 `dev/libaoxun688-hard-soc-source-sync-20260716`；未切换、清理或覆盖共享主工作区。
- 第一次提交：`2d4b3b7b3d0c59d88ece0669534ae38de02ed938`，提交四个系统工程真源、管理员点名的10个Hard SoC文件、UART0 Hello四个入口文件和7个最小BSP依赖。
- 系统契约复核：`feature_stats_tap + EfxSapphireHpSoc_slb`登记完整、`auto_instantiation=off`、AXI1关闭、视频USER1、QCRV32 USER2、UART0=`GPIOR_165/GPIOR_145`、SoC reset=`GPIOL_79`。
- 可移植化：只将Hard SoC `settings.json` 的 `--base_path` 改为相对 `..`；`build.ps1`移除安装路径硬编码并新增7个最小BSP文件fail-closed检查；README补充无本机路径的构建与重生步骤。其余列入Review Packet的来源文件与唯一副本SHA-256一致。
- Hello复现：用提交的最小BSP重新构建成功，2608B/16KiB，入口 `0xF9000000`，唯一LOAD段至 `0xF9000A30`，无未解析符号，`ELF_LOAD_AUDIT=PASS`。ELF及其他构建产物保持忽略，未提交。
- 禁止项检查：第一提交不含outflow/work/.metadata、bit/elf/map/rpt/log、pickle、许可证文件、用户目录、Efinity安装路径、来源绝对路径、`.mcp.json`或串口枚举缓存。
- Review Packet：`docs/review_packets/m2_hard_soc_source_sync_review_20260716.md`，记录唯一来源指纹、Efinity `2025.2.288.4.15`、Check Design `0 error/4 warning`、Map/PNR、Setup/Hold、CDC、bitstream、板级视频和25个文件SHA-256。
- 验证边界：来源联合副本为 `HARD SOC + VIDEO BUILD/PNR/BOARD-VIDEO PASS`；USER2 JTAG实际取指、UART0完整横幅和回显仍为 `CPU EXECUTION + UART0 NOT VERIFIED`。未连接机械臂，未合并main。
- 下一步：提交本条日志和Review Packet，推送个人分支，等待管理员审核；不得自行合并main。

### [M2-34] 管理员合并复核、状态收敛与新离线构建

- 日期：2026-07-16（Asia/Shanghai）。
- 合并范围：管理员锁定并合入 `dev/libaoxun688-hard-soc-source-sync-20260716@0604d33f3fe76851e1dfe738403875b4a7d0721c`；该分支相对基线 `main@4e35b05453c1cd30c943bb3d567fd0316ca6bdde` 为 0 behind / 2 ahead，合并无冲突。
- 真源裁定：补交的 25 个系统/IP/BSP/Hello 文件齐备，四个系统文件、Hard SoC wrapper、UART0 GPIO、USER1/USER2、AXI0/AXI1 与 DDR CFG 唯一驱动契约复核一致。2026-07-15 的“Hard SoC 系统真源缺失/HOLD”已被本条替代。
- Hello 新构建：在合并工作树用 Efinity RISC-V GCC 8.3.0 重建 PASS；2608 B / 16 KiB，入口 `0xF9000000`，唯一 LOAD 段至 `0xF9000A30`，`ELF_LOAD_AUDIT=PASS`。
- FPGA 新构建：Efinity 2025.2 从同一合并工作树执行 Map/Interface/PNR/PGM 全流程 PASS；最差 Setup/Hold `+1.742ns/+0.018ns`；CDC 为 `No Synchronizer warnings to report.`；Interface Design Issues 为 4 个既有物理距离 warning，post-synthesis netlist 另有 118 个 warning。
- 路径说明：Efinity map 直接使用中文绝对路径时因旧组件字符转换失败；改用仓库既有 ASCII junction 指向同一工作树后 PASS。未把该本机路径写入任何工程配置。
- 新 bitstream：SHA-256 `1D697F0DBA62CEDA3A8877729FF29A314F9BBA1A24CDCDFEDB751C7CF4B8AECC`；只保存在仓库外临时验证目录，未提交，且尚未上板。来源旧 bitstream `AA1338...` 的 J48/ch0 视频证据不继承到本次新 bitstream。
- 文档收敛：更新 `CURRENT_STATE.md`、`AGENTS.md`、`CLAUDE.md`、`fpga_vision`/`cpu_mycobot` Skills、Review Packet，并生成 2026-07-16 A/B/C 三轨学习指南；不再要求队友重复补交已存在的 Hard SoC 真源。
- 安全边界：未烧录、未连接机械臂、未运行任何动作。USER2 实际连接、片上 RAM 取指、UART0 115200 横幅与回显仍为 `NOT VERIFIED`。
- 下一门：仅使用匹配的新 bitstream、FPGA `USER2` 与片上 RAM Hello 完成 UART0 横幅/回显；禁止 `USER1`、Flash、外部 DDR、UART2/J52 和机械臂。
