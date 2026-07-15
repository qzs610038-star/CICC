# 单摄 Hard SoC 联合候选分支合并说明

> 日期：2026-07-15
> 适用工程：`competition_project_single_camera/`
> 建议源分支：`dev/libaoxun688-single-camera-soc-wip`
> 建议目标分支：最新 `main` 之上的集成/审查分支
> 当前结论：允许进入个人分支和 Pull Request 审查；不允许标记为正式板级闭环。

## 一、给合并人的最短说明

本分支把已经通过联合 FPGA 构建和摄像头回归的 Hard SoC 集成，从本地联合实验副本整理到单摄候选工程中。它包含：

```text
J48/ch0 摄像头视频链
feature_stats_tap 旁路统计
QCRV32 Hard SoC
UART0-only
USER2 JTAG
片上 RAM UART0 Hello 源码与构建入口
```

已经验证：

- Efinity 2025.2 Map、PNR、bitstream generation 通过。
- 最差 Setup/Hold 为 `+1.742ns/+0.018ns`。
- CDC 报告为 `No Synchronizer warnings to report.`。
- 联合 bitstream 烧录后，J48/ch0 HDMI 真实摄像头画面未黑屏、花屏、冻结或发生颜色通道完全错位。
- UART0 Hello ELF 已通过编译、未解析符号和片上 RAM 地址审计。

尚未验证：

- QCRV32 是否已通过 USER2 JTAG 实际取指运行。
- UART0 是否输出完整 Hello 横幅并完成单字符回显。
- CPU 到特征寄存器、OSD、按键状态机、UART2 和机械臂闭环。

因此，合并后的状态必须写成：

```text
HARD SOC + VIDEO BUILD/PNR/BOARD-VIDEO PASS
CPU EXECUTION + UART0 NOT VERIFIED
```

禁止写成“CPU 已上板运行”“正式工程已经闭环”或“可以连接机械臂”。

## 二、分支准备要求

旧 `dev/libaoxun688` 当前相对 `main` 落后较多，不建议直接承载本次系统级集成。建议从最新 `main` 新建独立分支：

```powershell
git fetch origin
git switch main
git pull --ff-only origin main
git switch -c dev/libaoxun688-single-camera-soc-wip
```

如果本地工作树不干净，不要强行切换或执行 `git reset --hard`。应先确认改动归属，再用新工作树、临时提交或人工白名单迁移隔离本次内容。

建议拆成两个提交：

```text
feat(single_camera): add hard soc joint integration
docs(single_camera): document uart hello board gate
```

## 三、允许进入分支的内容

### 1. FPGA/SoC 工程真源

以下文件属于同一系统级变更，必须作为一个 Review Packet 审查：

```text
competition_project_single_camera/mem_test.xml
competition_project_single_camera/mem_test.peri.xml
competition_project_single_camera/constrain.sdc
competition_project_single_camera/src/top.v
competition_project_single_camera/ip/EfxSapphireHpSoc_slb/
```

`ip/EfxSapphireHpSoc_slb/` 至少应保留 Efinity 重新打开和构建所需的可复现输入及生成源码：

```text
settings.json
EfxSapphireHpSoc_slb.v
EfxSapphireHpSoc_slb_define.vh
EfxSapphireHpSoc_slb_tmpl.v
EfxSapphireHpSoc_slb_tmpl.vhd
EfxSapphireHpSoc_wrapper.v
hard_ip_args.ini
ipm_pt_map.json
source/Axi4PeripheralTop.v
source/peri_config
```

`ipm/*.pickle` 属于工具缓存，不进入分支。

### 2. CPU Hello 可复现源码

允许提交：

```text
competition_project_single_camera/cpu_bringup/uart_hello_onchip/src/main.c
competition_project_single_camera/cpu_bringup/uart_hello_onchip/makefile
competition_project_single_camera/cpu_bringup/uart_hello_onchip/build.ps1
competition_project_single_camera/cpu_bringup/uart_hello_onchip/README.md
```

Hello 当前依赖同次 Hard SoC 生成的 `soc.h`、`default_i.ld`、`start.S` 和官方 standalone make 片段。合并前必须二选一：

1. 提交去除 `.metadata/` 和无关示例后的最小 BSP 依赖集；或
2. 在 README 中给出由 `settings.json` 重新生成 BSP 的明确步骤，并让构建脚本在 BSP 缺失时 fail closed。

不能只提交 `main.c` 和一个已构建 ELF，否则队友无法复现。

### 3. 已有单摄特征与 CPU Host 内容

以下内容可以随个人分支提交，但应与 Hard SoC 系统文件分开审查：

```text
competition_project_single_camera/src/feature_stats/
competition_project_single_camera/tests/rtl/
competition_project_single_camera/cpu/include/
competition_project_single_camera/cpu/src/
competition_project_single_camera/cpu/tests/
competition_project_single_camera/integration/
```

其中 FPGA 只产生 ROI/统计特征和硬件通道；颜色、形状、尺寸分类以及逐轮事务继续属于板上 CPU。不得借合并把分类状态机放回 RTL。

### 4. 文档与证据索引

允许提交：

```text
CURRENT_STATE.md
competition_project_single_camera/WORK_LOG.md
competition_project_single_camera/docs/debug_sessions/
competition_project_single_camera/docs/review_packets/
```

`WORK_LOG.md` 是追加式动作日志；发生冲突时保留双方条目，禁止用一侧整文件覆盖另一侧的新记录。

## 四、禁止上传的内容

以下内容只留本机，不得进入个人分支或 PR：

```text
.mcp.json
outflow*/
work_*/
.metadata/
*.bit
*.hex
*.elf
*.bin
*.o
*.vdb
*.qdb
*.db
*.map
*.rpt
*.log
ip/EfxSapphireHpSoc_slb/ipm/*.pickle
```

工程外备份目录也不得上传：

```text
C:\Users\20306\Desktop\赛题资料\CICC_backups\
D:\TJ375N529_SC431HAI2LCD_Demo_V3_cpu_video_joint_20260715_161500\outflow\
```

不要上传许可证、本机 Efinity 安装路径配置、IDE workspace 或本机串口枚举缓存。

## 五、四个系统文件的冲突处理

### `mem_test.peri.xml`

- 它是 Interface Designer 生成的系统资源真源。
- 禁止逐行手工拼接两个版本。
- 只有目标分支仍基于同一个单摄候选 periphery 基线时，才能整体采用本分支版本。
- 若目标分支也修改过 PLL、DDR、GPIO、JTAG、QCRV32 或 MIPI/HDMI资源，立即停止自动合并，回到 Interface Designer 重新生成并执行 Check Design。
- 合并后必须保持：DDR AXI0启用、DDR AXI1关闭、视频占用USER1、QCRV32占用USER2、UART0 RX/TX=`GPIOR_165/GPIOR_145`、SoC reset=`GPIOL_79`。

### `mem_test.xml`

- 必须同时保留 `feature_stats_tap.v` 和 `EfxSapphireHpSoc_slb` 工程登记。
- 必须保留 Hard SoC include path。
- `debugger.auto_instantiation` 必须为 `off`，否则 automated flow 会再次尝试占用已被 QCRV32 使用的 USER2。
- 不得把旧 Debug Wizard profile 当作第二个 USER2 实例重新加入。

### `src/top.v`

- 属于系统级连线文件，禁止使用“ours/theirs”盲选。
- 合并人必须确认摄像头、framebuffer、Debayer、feature tap、白平衡和 HDMI 数据路径没有被删除或改源。
- 现有视频状态机继续唯一驱动 DDR `CFG_START/RST/SEL`；SoC wrapper只读取 `CFG_DONE`，禁止形成双驱动。
- 已关闭的 `axi1_*`、旧 `CLK_5M`、旧 `pll_inst1_CLKOUT0` 和旧 `jtag_inst2_*` 不得恢复。

### `constrain.sdc`

- 必须与 `top.v` 和 `.peri.xml` 同批审查。
- 保留 `soc_system_clk=594MHz` 和 `soc_memory_clk=237.6MHz` 定义。
- 不得恢复已关闭 `axi1_*` 或旧 PLL 输出约束。
- 保留既有 CSI recovered-clock 异步约束；不得因本次合并扩大 false path。

## 六、合并前检查命令

先确认没有本机产物进入暂存区：

```powershell
git status --short
git diff --cached --name-only
git diff --cached --check
git diff --cached --name-only | Select-String -Pattern '(^|/)(outflow|work_|\.metadata)(/|$)|\.(bit|hex|elf|bin|o|vdb|qdb|db|rpt|log)$'
```

最后一条命令必须无输出。

再检查关键工程语义：

```powershell
Select-String competition_project_single_camera\mem_test.xml -Pattern 'feature_stats_tap|EfxSapphireHpSoc_slb|auto_instantiation'
Select-String competition_project_single_camera\mem_test.peri.xml -Pattern 'GPIOR_165|GPIOR_145|GPIOL_79|JTAG_USER2|soc_system_clk|soc_memory_clk'
Select-String competition_project_single_camera\constrain.sdc -Pattern 'soc_system_clk|soc_memory_clk|axi1_|CLK_5M|pll_inst1_CLKOUT0'
```

预期：

- `feature_stats_tap` 和 `EfxSapphireHpSoc_slb` 均存在。
- `auto_instantiation=off`。
- 三个 GPIO 和两个 SoC 时钟存在。
- SDC 中能找到两个 SoC 时钟，不应找到有效的 `axi1_`、`CLK_5M` 或旧 `pll_inst1_CLKOUT0` 约束。

## 七、合并后的验证顺序

### Gate A：静态和工程完整性

1. `git diff --check` 通过。
2. Efinity 2025.2 能打开 `competition_project_single_camera/mem_test.xml`。
3. Interface Designer Check Design 为0 error；只允许复现已记录的4项 MIPI/LVDS物理距离 warning，不允许新增 warning。

### Gate B：FPGA重新构建

1. Map通过。
2. PNR通过。
3. Setup和Hold均非负。
4. CDC为 `No Synchronizer warnings to report.`。
5. 记录新 bitstream 哈希，但不要提交 bitstream。

### Gate C：板级视频回归

1. 烧录新构建的匹配 bitstream。
2. 确认 J48/ch0 HDMI 为真实摄像头画面。
3. 确认无黑屏、花屏、冻结或颜色通道完全错位。

### Gate D：CPU Hello

1. 使用 FPGA USER2 JTAG，不得使用USER1。
2. 仅下载 Hello ELF 到 `0xF9000000` 片上 RAM，不写Flash、不初始化外部DDR。
3. 看到完整UART0横幅。
4. 向已确认的UART0端口发送一个ASCII字符并收到回显。

只有 Gate A-D 全部通过，才允许把状态从 `WIP/NOT VERIFIED` 改为：

```text
CPU EXECUTION + UART0 PASS
```

## 八、回退与停止条件

出现下列任一情况立即停止合并或上板扩展：

- Interface Designer出现新error/warning。
- `top.v` 视频主链和Hard SoC系统连线发生无法人工解释的混合冲突。
- Map/PNR/STA/CDC结果不再匹配已知通过边界。
- 摄像头画面黑屏、花屏、冻结或明显比联合bitstream回退。
- JTAG选择USER1、要求创建第二个USER2、准备擦写Flash或PC/ELF入口不是`0xF9000000`。
- CPU Hello未输出或字符无法回显。

回退时回到合并前提交，不使用 `git reset --hard` 清理包含他人工作的共享脏工作树。保留失败日志和截图，在 `WORK_LOG.md` 追加新的失败条目。

## 九、PR 描述建议

```text
目标：将单摄 J48/ch0 视频工程与最小 QCRV32 Hard SoC 集成纳入候选分支。

已验证：Efinity 2025.2 Map/PNR/bitstream、非负 Setup/Hold、CDC无同步器警告、联合bitstream的J48/ch0 HDMI视频回归。

未验证：USER2 JTAG加载后的CPU实际取指、UART0 Hello与回显、CPU特征读取、OSD、按键、UART2和机械臂。

安全边界：不修改final_project；不提交outflow/work/bit/elf；不连接机械臂；CPU Hello通过前保持WIP。
```

## 十、证据索引

- 当前状态：`CURRENT_STATE.md`
- 全部动作：`competition_project_single_camera/WORK_LOG.md` M2-26至M2-32
- CPU Hello操作：`competition_project_single_camera/docs/debug_sessions/m2_uart0_onchip_hello_operator_guide_20260715.md`
- Hard SoC资源历史：`competition_project_single_camera/docs/review_packets/m2_hard_soc_resource_replanning_operator_packet_20260715.md`
- 资源冲突历史：`competition_project_single_camera/docs/review_packets/m2_hard_soc_video_resource_conflict_report_20260715.md`
