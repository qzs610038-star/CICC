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
