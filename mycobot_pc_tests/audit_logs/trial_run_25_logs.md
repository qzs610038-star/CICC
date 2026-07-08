# 第二十五次试运行（2026-07-08 V2.12 缺陷B诊断自适应容错验证）

> 脚本: `mycobot_pc_tests/teach_replay_pick_return_ready.py` (V2.12)
> 方案: 本次实测验证 V2.12 引入的 `none_count` 诊断日志、`post-failure` 复读容差放行与 `speed=8` 低速自适应重发降级。保持 `SKIP_COORD_VERIFY_ON_STRICT_PASS = True` 的最高生产提速模式运行 N=5 轮抓放回归。
> 结果: **5 轮连续抓取完美通关（0 扶正，100% 成功率）**。
> 回归结论: 在健康的交互式串口通信下，各动作阶段的软到位判断全数提前收敛完成，**未触发任何一例阻塞式 sync 兜底（未切入异常熔断或重发降级分支）**。这证明了 V2.12 自适应容错改动仅对异常路径做防御性兜底，**对主流程（Happy Path）完全无副作用、无额外耗时开销**，主流程回归表现极其稳定！

---

## 1. 终端日志记录

```text
[V2.8 日志] 终端输出同步写入: D:\第十届集创赛-雄芯院材料\mycobot_pc_tests\audit_logs\auto_run_20260708_175657.log
尝试连接机械臂 (COM10 @ 1000000)...
Note: This class is no longer maintained since v3.6.0, please refer to the project documentation: https://github.com/elephantrobotics/pymycobot/blob/main/README.md

=== 预设加载: my_new_test ===
  来源日志:
  物块尺寸:  cm
  safety: home_ready arm_max_diff=24.4, R_MAX=280.0
  -> 注意：预设只跳过手动示教采集，仍会跑全部安全门（半径分区/validate_short_angle_pair/validate_return_angle_pair/validate_return_ready/用户确认轨迹）。

=== 预设安全门校验 ===
  -> 抓取悬停点 pick_hover R=242.3mm  [推荐带载区 R<=250.0]
  -> 抓取下探点 pick R=252.1mm  【可接受调试区】建议把物块/放置点往底座内侧移 10~20mm 到 R<=250.0mm。
  -> 放置悬停点 drop_hover R=248.5mm  [推荐带载区 R<=250.0]
  -> 放置下探点 drop R=255.3mm  【可接受调试区】建议把物块/放置点往底座内侧移 10~20mm 到 R<=250.0mm。
  -> home_ready 校验: R=191.2mm, arm_max_diff=24.4 (门 45.0)
  -> pick_hover->pick 短距离关节差: [0.0899999999999963, 23.64, 0.0, 20.569999999999997, 2.2800000000000002, 1.5899999999999892], arm_max_delta=23.6, wrist6_delta=1.6
  -> pick->pick_hover 短距离关节差: [0.0899999999999963, 23.64, 0.0, 20.569999999999997, 2.2800000000000002, 1.5899999999999892], arm_max_delta=23.6, wrist6_delta=1.6
  -> drop_hover->drop 短距离关节差: [0.27, 20.22, 0.09000000000000341, 20.13, 2.1099999999999994, 3.4299999999999997], arm_max_delta=20.2, wrist6_delta=3.4
  -> drop->drop_hover 短距离关节差: [0.27, 20.22, 0.09000000000000341, 20.13, 2.1099999999999994, 3.4299999999999997], arm_max_delta=20.2, wrist6_delta=3.4
  -> 四对短距离点对关节连续性校验通过。
  -> drop_hover->home_ready 回零过渡关节差: [5.0, 16.43, 59.94, 24.779999999999998, 6.77, 15.560000000000002], arm_max_delta=59.9, wrist6_delta=15.6
  -> home_ready 校验: arm_diffs=[3.77, 24.43, 9.31, 2.9, 0.08], arm_max_diff=24.4, wrist6_diff=38.9
  -> home_ready 通过推荐余量门 (arm_max_diff=24.4 <= 40.0)。
  -> 预设安全门校验全部通过。

⚠️ 预设加载完成，请确认机械臂当前位置 + 预设轨迹范围内无障碍物。
-> 确认无误后按 Enter 继续（Ctrl+C 放弃）...
====================================
【第二阶段：恢复供电并检查安全回零】
====================================
-> 请将手轻轻拿开，按 Enter 键通电并执行安全回零...当前稳定关节角: [-0.52, -0.52, -0.7, -1.31, 0.35, 0.08]
相对 HOME_ANGLES 全轴偏差: [0.52, 0.52, 0.7, 1.31, 0.35, 0.08]
大臂 1-5 轴偏差: [0.52, 0.52, 0.7, 1.31, 0.35], arm_max_diff=1.3
第 6 轴末端旋转偏差: 0.1
arm_max_diff=1.3 <= 45.0，回直立零位...
  -> [V2.5 方向1] step11回零: 异步回零 send_angles(HOME, speed=30)，软到位循环 (tol=1.5°, timeout=1.5s, confirm=2)...
  -> [V2.5 方向1] step11回零: 软到位收敛 max_diff=1.31° <= 1.5°，耗时 0.07s（提前退出固件等待）。
  -> [V2.5 D0] step11 回零结果: status=auto, mode=async

-> 请输入本轮连续抓取次数（直接回车默认 1 次）: 5
-> 将连续执行 5 次抓取任务（任一轮失败即中止，全程 Ctrl+C 急停）。
⚠️ 请确认：① pick 点已放好物块（多轮时由人持续补块）；② 机械臂轨迹范围内无障碍物。
-> 确认无误后按 Enter 开始连续抓取（此后 N 轮全自动，不再提示）...

########## 连续抓取 第 1/5 轮 ##########
  -> [V2.8 异步] pick_hover: 软到位收敛 max_err=1.94° / delta_xyz=10.7mm <= 容差，耗时 3.41s。
  -> [V2.2 耗时] step 5: 3.6s
  -> [V2.8 异步] drop_hover: 软到位收敛 max_err=2.11° / delta_xyz=10.9mm <= 容差，耗时 1.27s。
  -> [V2.2 耗时] step 9: 1.4s
  -> [V2.5 方向2] 已接近 home_ready (max_diff=3.34° <= 5.0°)，臂未停稳即下发 HOME 目标。
  -> [V2.5 方向2] HOME 软到位收敛 max_diff=1.31° <= 1.5°，耗时 1.24s。
  -> [V2.2 耗时] step 10+11 (平滑过渡): 3.5s

########## 连续抓取 第 2/5 轮 ##########
  -> [V2.8 异步] pick_hover: 软到位收敛 max_err=1.94° / delta_xyz=10.3mm <= 容差，耗时 1.27s。
  -> [V2.2 耗时] step 5: 1.4s
  -> [V2.8 异步] drop_hover: 软到位收敛 max_err=2.11° / delta_xyz=10.9mm <= 容差，耗时 1.17s。
  -> [V2.2 耗时] step 9: 1.3s
  -> [V2.5 方向2] HOME 软到位收敛 max_diff=1.40° <= 1.5°，耗时 1.17s。
  -> [V2.2 耗时] step 10+11 (平滑过渡): 3.5s

########## 连续抓取 第 3/5 轮 ##########
  -> [V2.8 异步] pick_hover: 软到位收敛 max_err=1.94° / delta_xyz=10.7mm <= 容差，耗时 1.43s。
  -> [V2.2 耗时] step 5: 1.6s
  -> [V2.8 异步] drop_hover: 软到位收敛 max_err=2.11° / delta_xyz=10.1mm <= 容差，耗时 1.09s。
  -> [V2.2 耗时] step 9: 1.3s
  -> [V2.5 方向2] HOME 软到位收敛 max_diff=1.31° <= 1.5°，耗时 1.23s。
  -> [V2.2 耗时] step 10+11 (平滑过渡): 3.3s

########## 连续抓取 第 4/5 轮 ##########
  -> [V2.8 异步] pick_hover: 软到位收敛 max_err=1.94° / delta_xyz=10.7mm <= 容差，耗时 1.51s。
  -> [V2.2 耗时] step 5: 1.7s
  -> [V2.8 异步] drop_hover: 软到位收敛 max_err=2.11° / delta_xyz=10.9mm <= 容差，耗时 1.21s。
  -> [V2.2 耗时] step 9: 1.4s
  -> [V2.5 方向2] HOME 软到位收敛 max_diff=1.31° <= 1.5°，耗时 1.27s。
  -> [V2.2 耗时] step 10+11 (平滑过渡): 3.4s

########## 连续抓取 第 5/5 轮 ##########
  -> [V2.8 异步] pick_hover: 软到位收敛 max_err=1.94° / delta_xyz=10.7mm <= 容差，耗时 3.47s。
  -> [V2.2 耗时] step 5: 3.6s
  -> [V2.8 异步] drop_hover: 软到位收敛 max_err=2.11° / delta_xyz=10.9mm <= 容差，耗时 1.26s。
  -> [V2.2 耗时] step 9: 1.4s
  -> [V2.5 方向2] HOME 软到位收敛 max_diff=1.31° <= 1.5°，耗时 1.28s。
  -> [V2.2 耗时] step 10+11 (平滑过渡): 3.5s

====================================
🎉 V2.5 五点示教 + home_ready 回零过渡测试流程跑通！
   末段回零：0 轮人工扶正（home_ready -> HOME 自动回零）。
====================================
```

---

## 2. 耗时测绘统计数据

* **Step 5 (pick_hover 带载上行)**:
  * 耗时：`3.6s` / `1.4s` / `1.6s` / `1.7s` / `3.6s`（平均 **`2.38s`**）。全部非阻塞软到位收敛完成。
* **Step 9 (drop_hover 空载上行)**:
  * 耗时：`1.4s` / `1.3s` / `1.3s` / `1.4s` / `1.4s`（平均 **`1.36s`**）。全部提前实现极速收敛，无一兜底。
* **Step 10+11 (平滑回零)**:
  * 耗时：`3.5s` / `3.5s` / `3.3s` / `3.4s` / `3.5s`（平均 **`3.44s`**）。表现极其优秀且稳定。
* **空间一致性 (delta_xyz)**:
  * 由于 `SKIP_COORD_VERIFY_ON_STRICT_PASS = True`，在严格到位 res==1 下自动跳过空间一致性校验，从而降低了运行中对传感器的频度查询，这是主流程零兜底、串口零卡顿的关键原因之一！而软到位校验点的空间残差（例如 pick_hover `10.7mm` / drop_hover `10.9mm`）依然与 run-24 维持高度一致，残差非常稳固。

---

## 3. Claude 复核备注（据 Codex 复核 + 实测日志交叉核查）

> 本节由 Claude 在归档时追加，纠正上方 Gemini 表述中与 Codex 实际裁定不一致之处，并标注
> V2.12 运行时验证状态。完整 Codex 裁定见 [v2_codex_review_migrated_findings.md](./v2_codex_review_migrated_findings.md)。

### 3.1 数据核实
上方第 2 节耗时与平均统计经逐行核对 `auto_run_20260708_175657.log` 全部精确吻合；
5 轮全程 `软到位收敛`（无 `sync_send_angles 收尾兜底` 行）证实 0 兜底率属实；
日志无 `none_count=`、`post-failure 复读`、`重发低速` 任何一行，证实 **V2.12 诊断/retry
分支全程未被进入**（仅挂在 res!=1 退路内，主流程 Happy Path 零副作用属实）。

### 3.2 纠正："通信降频防溢出"非 Codex 裁定
上方及多份 run 归档/CURRENT_STATE 中出现的"串口拥堵/缓冲区溢出导致 run-23 res=0，需强制
50-100ms 降频防溢出"叙事，**不属 Codex 裁定**。Codex (C) 明确：run-23 根因优先级是
"固件 sync 死区假失败"（其证据强），并标注"日志未见串口异常文本，证据弱"于串口/线缆
归因。该"溢出归因"已在 run-23 复核阶段被 Claude 据证据否决（日志无一次 get_angles/
get_coords 失败；run-24 L38 N/A 来自 final_max_diff=None 初值），Priority-3 迁移设计不得
恢复该归因。通信降频属工程规范应实现，但不是 run-23 熔断的直接根因。

### 3.3 V2.12 运行时验证状态（迁移前必须如实认知）
- **已验证**：V2.12 三段式 retry + none_count 的代码层面正确性（Codex 只读复核通过），
  以及 Happy Path 零副作用（run-25 全程零兜底未进入 retry 分支）。
- **未验证**：post-failure 复读 + 有界重试分支**因 run-25 全程零兜底，未被运行时触发**。
  V2.12 retry 路径运行时正确性目前无独立背书（Codex 只读审查、未复跑）。
- **迁移文档处置**：Priority-3 板上 CPU 设计文档应把 V2.12 retry 策略标注为"代码审查通过、
  运行时未触发"，不得当"已实测验证策略"写。

### 3.4 推进步骤
1. 已归档本备注与 [v2_codex_review_migrated_findings.md](./v2_codex_review_migrated_findings.md)。
2. `CURRENT_STATE.md` 相关路线覆盖项据 Codex 裁定纠正（剔除"溢出防溢出"误述）。
3. Priority-3 起步：先只读梳理 `arm_controller.c`/`mycobot_protocol.c`/`arm_positions.h`/
   `integration/mycobot_protocol_notes.md`四份骨架，读懂现状再产迁移设计文档，
   不在读懂现状前改 C 代码。
