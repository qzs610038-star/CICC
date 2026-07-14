# myCobot PC 端 A 路线正式实验执行清单

> 日期：2026-07-14
> 分支：`codex/mycobot-experiment-main-sync-20260714`（基于 `origin/main@39e8a92`）
> 路线：A — 受控诊断优先，再跑完整抓放；用实际抓取结果反推是否需要回头做精确误差测定。
> 上位约束：`final_project/docs/technical_plans/mycobot_pc_experiment_continuation_plan_20260714.md`、`mycobot_board_bringup_operator_sop_20260712.md` §8、`mycobot_cpu_board_bringup_implementation_plan_20260714.md` §3 安全不变量第 9 条。

## 0. 现场安全前提（已登记）

| 项目 | 内容 |
|---|---|
| 安全员 | 在场，全程手持机械臂独立 12V 电源插头 |
| 断电方式 | 直接拔插机械臂独立 12V 电源（≤2 秒断电） |
| 风险接受边界 | 抓取失败可接受；夹爪/物块损坏不可接受 |
| E3 状态 | HARDWARE-BLOCKED：现场硬件条件无法满足基座刚性固定与外部 180°基准测量，用户选择跳过 E3 直接进入真机示教 |

**急停优先级**：任何异常 → 安全员立即拔 12V 插头 → 操作员 Ctrl+C 软停。物理断电优先于软件 stop。

## 1. 脚本选择与顺序

受控诊断优先，完整抓放在后。按以下顺序执行，前一步未通过不进入后一步。

### 步骤 A1：受控探针诊断（HOME → pick_hover → HOME）

```powershell
cd "D:/第十届集创赛-雄芯院材料"
python mycobot_pc_tests/teach_replay_pick_return_ready.py `
  --port <现场确认的COM口> `
  --probe-pick-hover `
  --save-preset <全新唯一命名，如 e3skip_diag_20260714> `
  --save-preset-log "<来源日志名>" `
  --save-preset-obj "<物块尺寸cm>"
```

**说明**：
- `--probe-pick-hover` 仅执行 HOME → pick_hover 单段诊断，不下探、不闭爪。
- `--save-preset` 用全新唯一命名保存五点示教，不得覆盖任何旧 JSON。
- 该模式会先 `prepare_phase`（通电 + 安全回零），再单段移动到 pick_hover，最后安全回零。
- 日志自动写入 `mycobot_pc_tests/audit_logs/auto_run_<时间戳>.log`。

**通过判据**：
- 机械臂能从 HOME 安全移动到 pick_hover 并回零，无碰撞、无卡死。
- `[诊断模式最终状态]` 显示实际角度/坐标与目标的偏差。
- 若 max_angle_diff ≤ 3° 且空间残差 ≤ 25mm → 可进入 A2。
- 若偏差过大或运动异常 → 停止，记录现象，回头检查示教点位。

### 步骤 A2：受控探针下探（HOME → pick_hover → pick → pick_hover → HOME）

```powershell
python mycobot_pc_tests/teach_replay_pick_return_ready.py `
  --port <COM口> `
  --probe-pick `
  --preset <A1保存的预设名>
```

**说明**：
- `--probe-pick` 执行 HOME → pick_hover → pick（下探）→ pick_hover → HOME。
- 夹爪保持张开，不下发闭爪命令。
- 该步骤验证下探路径的关节连续性和到位精度。

**通过判据**：
- 下探到位，实际坐标与 pick 目标偏差在容差内。
- 抬起回 pick_hover 成功，无卡死。
- 若 step 5（抬起）超时或软通过残差过大 → 记录现象，可尝试调整示教点位或接受当前精度进入 A3。

### 步骤 A3：完整抓放（全自动）

```powershell
python mycobot_pc_tests/teach_replay_pick_return_ready.py `
  --port <COM口> `
  --preset <A1保存的预设名>
```

**说明**：
- 无 `--probe-*` 标志，进入完整 auto_phase_v2 流程。
- 流程：张开夹爪 → pick_hover → pick → 闭合夹爪 → pick_hover → drop_hover → drop → 张开夹爪 → drop_hover → home_ready → HOME。
- 运行前会提示输入连续抓取次数（默认 1 次）。
- 每轮结束臂处于 HOME 且夹爪张开。

**通过判据**：
- 完成至少 1 次完整抓放，物块从 pick 点成功转移到 drop 点。
- 全程无碰撞、无卡死、无人工扶正。
- 若抓取失败（物块掉落、未夹起）→ 记录失败现象和步骤编号，判断是否需要重新示教或回头做精确误差测定。

## 2. 关键参数速查

| 参数 | 值 | 说明 |
|---|---|---|
| `ANG_REPLAY_SPEED` | 20 | 长距离关节角回放速度 (%) |
| `SHORT_DOWN_SPEED` | 12 | 下行关节速度（重力辅助）(%) |
| `SHORT_UP_SPEED` | 16 | 上行关节速度（抗重力）(%) |
| `SOFT_ANGLE_SUCCESS_TOL` | 3.0 | 软到位关节角度容差 (deg) |
| `SOFT_COORD_SUCCESS_TOL` | 25.0 | 软到位坐标容差 (mm) |
| `R_MAX` | 280.0 | 最大臂展硬限 (mm) |

## 3. 异常处理流程

### 3.1 运动中异常（RuntimeError / 超时）

1. 脚本自动提示"请用手扶稳机械臂/物块防止下沉/掉落，扶稳后按 Enter 释放舵机"。
2. 安全员立即拔 12V 插头断电。
3. 操作员按 Enter 后脚本尝试 release_all_servos（但已断电，无效）。
4. 记录异常发生的步骤编号和现象。

### 3.2 人工急停（Ctrl+C）

1. 脚本捕获 KeyboardInterrupt，调用 mc.stop() + release_all_servos()。
2. 安全员同步拔 12V 插头（双保险）。
3. 机械臂完全变软后人工扶正归位。

### 3.3 抓取失败（物块掉落/未夹起）

1. 不立即停止，让脚本完成本轮剩余步骤（回零）。
2. 若物块掉落导致后续步骤异常 → 按 3.1 或 3.2 处理。
3. 记录失败步骤、物块状态、实际坐标偏差。

## 4. 实验记录要求

每次运行后，在 `mycobot_pc_tests/audit_logs/` 创建或追加记录：

- **日志文件**：`auto_run_<时间戳>.log`（脚本自动生成）
- **人工记录**：记录以下信息
  - 运行步骤（A1/A2/A3）
  - 使用的 COM 口和预设名
  - 每个步骤的实际坐标和偏差
  - 是否发生异常、急停、抓取失败
  - 物块最终状态（成功转移/掉落/未夹起）
  - 安全员签字确认

## 5. 结果判断与下一步

根据 A3 完整抓放的结果判断：

| 结果 | 判断 | 下一步 |
|---|---|---|
| 抓放成功，物块稳定转移 | 当前示教点位可用 | 可重复运行 A3 多次验证稳定性；考虑迁移到板上 CPU 点表 |
| 抓放失败，但运动轨迹正常 | 可能是夹爪力度或物块位置问题 | 微调示教点位或夹爪参数，重跑 A1→A3 |
| 抓放失败，运动轨迹异常（卡死/碰撞/大幅偏差） | 需要回头做精确误差测定 | 回到 E3 机械前置门，补做基座刚性固定和外部 180°基准测量 |

## 6. 与板上路线的边界

- A 路线所有结果只用于 PC 端开发期点位与指令序列参考。
- 不能跳过板上 G4–G11 门禁。
- `competition_project_single_camera/` 未完成 M0 板级复现前，PC 端 180°实验不能依赖该工程提供真实识别触发。
- 本清单不授权烧录、接线或连接 J52/机械臂控制线；仅授权 PC 端 pymycobot 真机示教与抓放。
