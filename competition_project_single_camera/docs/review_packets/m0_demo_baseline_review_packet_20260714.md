# M0 Demo基线Review Packet

> 日期：2026-07-14
>
> 来源：`<local-demo-mirror>`（历史本地构建镜像别名）
>
> 目标：建立可追溯的新单摄工程源码基线，不改变视频功能
>
> 当前判定：**M0源码复制与身份审计完成；M0 Gate尚未PASS，等待用户重新构建、烧录和画面复现。**

## 1. 执行边界

本次已执行：

- 运行`tools/agent_handoff_health_check.ps1`，结果PASS；唯一警告为未安装`pymycobot`，与M0无关。
- 只读检查D盘工程、Efinity进程、锁、工程XML、IP、源码依赖和历史构建产物。
- 按`mem_test.xml`引用白名单复制源码和必要生成文件到仓库新工程。
- 逐文件比较D盘与仓库副本SHA-256。
- 记录历史bitstream、map、timing、CDC和programmer产物身份。
- 形成D盘首次同步清单；全部项目为`NO_OP_MATCH`，未写回D盘。

本次未执行：

- 未修改任何RTL、XML、SDC或IP设置。
- 未裁剪ch1或DSI。
- 未生成SoC、CPU、APB、按键、特征或OSD。
- 未运行Efinity构建、PNR或烧录。
- 未删除D盘`.lock`、`outflow/`或`work_*`。
- 未连接或驱动机械臂。

## 2. 来源身份

`mem_test.xml`记录：

```text
project       = mem_test
device        = TJ375N529 / Titanium / I3
tool          = 2025.2.288.4.15
last_run_flow = bitstream
last_run_state= pass
```

历史构建时间链从2026-07-13 21:21映射开始，至21:24生成`mem_test.bit`和`mem_test.hex`。`mem_test.pgm.out`确认bitstream生成器使用同一D盘工程的`work_pnr/mem_test.lbf`和`outflow/mem_test.lpf`。

关键哈希见：

- `../baseline/m0_historical_build_artifacts_20260714.csv`
- `../baseline/m0_source_manifest_20260714.csv`

历史可运行bitstream：

```text
path   = <local-demo-mirror>/outflow/mem_test.bit
size   = 12418911 bytes
sha256 = A99F14AB8922783C51A71953288F9A25DE1C70DC3E3962F5508BBF53EF75057C
```

用户确认该bitstream在J48/ch0摄像头输入和HDMI到电脑链路上稳定显示真实画面。该结论是用户提供的板级历史证据，本次未重新烧录复验。

## 3. 白名单复制

共复制75个文件，源/目标哈希`75/75`一致：

- `mem_test.xml`直接引用的53个RTL/SV文件；
- 4个编译期依赖：`Axi_Mux_Param.vh`、`timescale.v`、`i2c_master_defines.v`、`Panel_1080p_reg.mem`；
- 根工程XML、periphery XML、SDC、Debugger配置/模板；
- `csi_rx_controller`与`dsi_tx`两个工程IP的`settings.json`、生成源码、定义头和模板。

工程引用复核结果：53个设计文件、2个IP配置和1个SDC全部存在；4个编译期依赖全部存在。

默认排除：

- `outflow/`、`outflow_ch0_*`、`work_*`、`.lock`；
- `.bak`、ModelSim `work/`、`.wlf`、`.vcd`、`.gtkw`；
- 未被工程引用的Testbench、普通仿真数据库、重复压缩包；
- `ip/ram`，因为当前`mem_test.xml`未声明该IP；
- 历史`mem_test.bit`本体，仅记录身份和回退位置。

## 4. 历史构建结果

历史map资源：

| 资源 | 数量 |
|---|---:|
| `EFX_ADD` | 2416 |
| `EFX_LUT4` | 14540 |
| `EFX_FF` | 11516 |
| `EFX_RAM10` | 211 |
| `EFX_DPRAM10` | 4 |

PNR和bitstream生成完成。CDC报告为`No Synchronizer warnings to report`。

### 时序风险

当前历史基线**不能标记为STA PASS**。`mem_test.timing.rpt`存在多组跨时钟setup负slack：

| Launch → Capture | Setup slack |
|---|---:|
| `mipi_clk → i_sysclk_div2` | `-1.433 ns` |
| `mipi_dphy_tx_SLOWCLK → mipi_dphy_tx_FASTCLK_D` | `-1.374 ns` |
| `mipi_dphy_tx_SLOWCLK → i_sysclk_div2` | `-1.300 ns` |
| `i_sysclk_div2 → mipi_rx_ck1_CLKOUT` | `-1.293 ns` |
| `mipi_rx_ck1_CLKOUT → i_sysclk_div2` | `-1.083 ns` |

同钟域主要setup为正，所有列出的hold slack为正。负slack关系可能包含需要补充异步/多周期约束或真实CDC修复的路径，但在审查前不得标记为可忽略。M1删除ch1/DSI时须逐项比较这些关系的来源和变化。

## 5. D盘同步

`../baseline/m0_initial_d_drive_sync_20260714.csv`记录75项仓库副本与D盘源的比较，全部为`NO_OP_MATCH`。因此首次同步没有执行文件覆盖，也没有改动D盘。

后续仓库新工程为正式源码，D盘为人工构建/烧录镜像。同步必须由Codex按已审核差异增量执行。

## 6. M0剩余验收

用户需要按`../debug_sessions/m0_manual_build_board_check_20260714.md`完成：

1. 使用Efinity `2025.2.288.4.15`从D盘当前镜像重新运行完整构建。
2. 保存map、PNR、timing、CDC、bitstream日志和新bitstream哈希。
3. 手动烧录。
4. J48/ch0到HDMI冷启动3次均出现实时画面。
5. 连续运行至少10分钟，无fallback纯色、明显花屏、冻结或持续条纹。
6. 反馈分辨率/帧率是否与历史基线一致。

这些证据齐全前，M0保持`PARTIAL / BOARD RECHECK PENDING`，不得进入M1裁剪。

## 6bis. M0-10 路径可移植性修复（2026-07-15）

- 触发：M0 命令行 Map 因 `$readmemh` 无法打开 `/src/mipi_dsi/Panel_1080p_reg.mem` 而 FAIL，根因为 `dsi_tx_top.v:144` 硬编码绝对路径，与当前 Windows 构建环境不兼容。
- 修复内容：
  - `dsi_tx_top.v:144`：`.INITIAL_CODE("/src/mipi_dsi/Panel_1080p_reg.mem")` → `.INITIAL_CODE("src/mipi_dsi/Panel_1080p_reg.mem")`（仅删除开头 `/`）。
  - 仅改变文件定位；不改变初始化内容或视频逻辑。
- 修改前 SHA-256：`789BD0EAF9C3AE4D2B5D01410A41E6FBEB3F1E406AF1D5F29ECAF2EE4924559D`
- 修改后 SHA-256：`DDAF952AE26A599A988235FBE5EB2815AA9C8FDE66B7346EB359A24996F031D7`
- 未修改文件：`panel_config.v`、`true_dual_port_ram.v`、`mem_test.xml`、`constrain.sdc`、IP `settings.json`、所有 `.mem` 文件、`top.v` 及所有其他 RTL。
- 旧 Map 失败证据：归档于 `docs/debug_sessions/evidence/m0_map_fail_pre_path_fix_20260715/`。
- 状态：`MAP PASS / RECORD CORRECTION PENDING / PNR HOLD` — Map 重测 PASS（退出码 0），旧 `$readmemh` 错误已消除。PNR/STA/CDC/bitstream 和板级仍待验证。
- Map 重测数据：资源 ADD=4367/LUT4=19399/FF=11800/DSP48=4/RAM10=211/DPRAM10=4/SRL8=1。warn.log 整文件 1177 行/1173 非空行，新运行段 1042 非空行；223 VERI/VDB WARNING（15 类别）；785 EFX-0256；VERI-2561 INFO 47。
- 记录修正（2026-07-15，Codex2）：warning 统计从 224/16 类修正为 223/15 类；`Found 587 warnings` 不计入。Efinity 曾自动改写 `mem_test.xml`（`last_run_flow` bitstream→syn），已恢复到构建前精确字节状态。Delta manifest 已扩展列。
- 接口审计修正（2026-07-15，Codex2 第二轮）：AXI1 698 个 EXPECTED→TBD/BLOCKING（`is_axi_enable="true"`）、DSI 70 个"整个未激活"→"仅 ch0 未驱动"（ch1 由 `dsi_tx_top_inst1` 驱动）、JTAG jtag_inst2 不在 peri.xml 中。新增 PNR 前接口归属解决方案 `m0_pnr_interface_resolution_plan_20260715.md`。
- 证据：`WORK_LOG.md` M0-10、`docs/baseline/m0_post_baseline_delta_20260714.csv`、`docs/debug_sessions/m0_path_fix_map_retest_20260715.md`、`docs/debug_sessions/m0_efx0256_interface_audit_20260715.md`（修正版）、`docs/review_packets/m0_pnr_interface_resolution_plan_20260715.md`。

## 7. 当前NOT VERIFIED

- 仓库副本触发的新完整Efinity构建。
- 新bitstream与仓库副本的身份对应。
- 本轮重新烧录和板级画面复现。
- 历史负slack路径的约束正确性或CDC安全性。
- ch1/DSI裁剪后的资源、时序和视频回归。
- QCRV32、UART1、APB、按键、特征、OSD和机械臂。
