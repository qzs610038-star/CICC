# myCobot 带载抓取回零自动化 V2 方案

> 日期: 2026-07-06
> 目标读者: Claude 执行 / Codex 复核
> 适用范围: `mycobot_pc_tests/` PC 端机械臂安全联调
> 核心要求: 新写脚本，不直接修改已经跑通的 `teach_replay_pick.py`

## 0. 执行边界

本方案基于 `teach_replay_pick.py` 已经完整跑通的事实继续迭代，但旧脚本必须作为稳定基线保留。

Claude 执行时必须遵守:

- 不直接修改 `mycobot_pc_tests/teach_replay_pick.py`。
- 新脚本建议命名为 `mycobot_pc_tests/teach_replay_pick_return_ready.py`。
- 新脚本可以复制旧脚本的已验证工具函数，但复制后必须在新文件内独立修改。
- 旧脚本只作为对照和回退入口，不作为本轮实验改动目标。
- 所有实机运动命令继续保留人工确认提示和 Ctrl+C 急停路径。

## 1. 当前实测结论

本轮已经完成带物块抓取/放置跑通，说明关节角示教回放路线成立。残留问题集中在两个方向:

1. 物块或放置位稍远时触发安全保护。
   - 失败点示例: `R=290.1 > 280.0`。
   - 这不是误报，而是工作区已经进入臂展边缘。
   - 带载时远端力矩更大，不应放宽 `R_MAX=280.0` 来绕过。

2. 任务结束后的自动回零仍依赖人工扶正。
   - 结束姿态通常停在 `drop_hover` 附近。
   - 实测 `J2/J3` 偏差可达约 `50~70 deg`。
   - 当前 `safe_return_home()` 在 `arm_max_diff > 45.0` 时进入人工扶正，是合理安全门。
   - 根因不是 HOME 点错误，而是 `drop_hover -> HOME_ANGLES` 缺少一个可控中间姿态。

## 2. V2 设计目标

V2 不追求更远工作区，而是追求更稳定、可重复、少人工干预:

- 把带载抓放工作区收缩到推荐半径内。
- 在示教流程中新增 `home_ready` 中间姿态。
- 自动流程改成 `drop_hover -> home_ready -> HOME_ANGLES`。
- 让 `safe_return_home()` 处理接近零位的最终回零，而不是从远端弯曲姿态硬回零。
- 保持坐标只读校验，不恢复 `sync_send_coords()` 运动规划。

## 3. 工作区规则

继续保留旧脚本中 `R_MAX=280.0` 的硬拦截。不要通过放宽半径让远端点通过。

新增建议:

- 推荐带载工作区: `R <= 250.0 mm`。
- 可接受调试区: `250.0 < R <= 260.0 mm`，需要打印黄色提示。
- 边缘风险区: `260.0 < R <= 280.0 mm`，允许示教但要求用户显式确认，建议重新摆放物块。
- 禁止区: `R > 280.0 mm`，继续硬拦截。

实现建议:

- 在新脚本中增加 `R_RECOMMENDED_LOAD = 250.0`、`R_CAUTION_LOAD = 260.0`。
- `record_teach_point()` 保存 `pick/drop` 和 `pick_hover/drop_hover` 时打印半径诊断。
- 若 `R > 260.0`，提示用户把物块/放置点往底座内侧移动 20~40mm。
- 默认不要因为用户确认就允许 `R > 280.0`。

## 4. 新增示教点

旧脚本四点:

```text
pick_hover / pick / drop_hover / drop
```

V2 五点:

```text
pick_hover / pick / drop_hover / drop / home_ready
```

`home_ready` 定义:

- 空中安全姿态，用于从放置悬停点过渡到直立零位。
- 夹爪尖端朝前或略向下，但不能扫到桌面、物块、线缆。
- `J2/J3/J4` 应尽量接近可安全回零的弯曲程度。
- 建议通过条件: `arm_max_diff <= 45.0` 或尽量接近该阈值。
- 若很难达到 `<=45.0`，至少要比 `drop_hover` 明显更接近 HOME，并由 Codex 再审。

注意:

- `home_ready` 不是抓放工作点，不应套用 `Z_MAX=280.0` 的业务工作区上限。
- 可复用 `is_valid_coord_reading()` 检查读数有效性。
- 需要单独写 `record_return_ready_point()` 或给 `record_teach_point()` 增加不影响旧四点的 mode。

## 5. 新脚本流程

建议新脚本结构:

```text
teach_phase_v2()
  release_all_servos()
  record pick_hover
  record pick
  record drop_hover
  record drop
  record home_ready

prepare_phase()
  power_on()
  safe_return_home()

auto_phase_v2()
  validate short pairs:
    pick_hover <-> pick
    drop_hover <-> drop
  validate return transition:
    drop_hover -> home_ready
    home_ready -> HOME_ANGLES
  gripper open
  go pick_hover
  go pick
  close gripper
  go pick_hover
  go drop_hover
  go drop
  open gripper
  go drop_hover
  go home_ready
  safe_return_home()
```

运动原则:

- 长距离继续用 `sync_send_angles()`。
- 短距离上下探继续用 `checked_short_angles()`。
- 坐标继续只做 `verify_coords_near()`，不参与运动规划。
- 不恢复 `sync_send_coords()`。

## 6. 回零中间点校验

新增校验函数建议:

```python
def validate_return_ready(home_ready):
    all_diffs, arm_diffs, arm_max_diff, wrist6_diff = calc_home_diffs(home_ready["angles"])
    print(...)
    if arm_max_diff > ARM_MAX_DIFF_SAFE:
        print("home_ready 仍未进入自动回零安全门，建议重新示教")
        return False
    return True
```

新增过渡校验建议:

```python
def validate_return_angle_pair(src_point, dst_point, label):
    # 比短距离上下探稍宽松，但仍要拦截单轴巨大跳变
    RETURN_ARM_JOINT_MAX_DELTA = 55.0
    RETURN_WRIST6_MAX_DELTA = 90.0
```

如果 `drop_hover -> home_ready` 单轴变化过大:

- 不要自动执行。
- 要求重新示教 `home_ready`。
- 或增加第二个中间点 `home_ready_1 / home_ready_2`，但这属于 V2.1，先不默认增加复杂度。

## 7. 推荐参数

保持旧脚本已验证参数:

```text
SHORT_DOWN_SPEED = 12
SHORT_DOWN_TIMEOUT = 10
SHORT_UP_SPEED = 16
SHORT_UP_TIMEOUT = 25
SOFT_ANGLE_SUCCESS_TOL = 3.0
SOFT_COORD_SUCCESS_TOL = 25.0
```

可在新脚本中试探优化:

```text
SHORT_UP_SPEED = 20
```

但建议分两轮验证:

1. 第一轮只新增 `home_ready`，保持速度不变，确认回零自动化改善。
2. 第二轮再把上行速度从 `16` 调到 `20`，观察 `drop -> drop_hover` 是否从软通过变成严格通过。

不要在同一轮同时改多个变量，否则无法判断收益来源。

## 8. 实验顺序

### 第 1 轮: 空载 V2 验证

- 物块不放入夹爪。
- 示教五点。
- 执行完整流程。
- 重点观察:
  - `drop_hover -> home_ready` 是否顺滑。
  - `home_ready` 是否让 `safe_return_home()` 不再进入人工扶正。
  - 是否出现扫桌、夹爪线缆扭转、底座附近自碰风险。

通过条件:

- 全流程跑通。
- 末端回零不需要人工扶正。
- `home_ready` 后 `arm_max_diff <= 45.0`。

### 第 2 轮: 带物块低速验证

- 保持同一组五点。
- 物块位置放在推荐区 `R<=250mm`。
- 仍使用保守速度。
- 重点观察:
  - 带载抬起时是否明显下沉。
  - `drop -> drop_hover` 是否仍软通过。
  - 放置后进入 `home_ready` 是否稳定。

### 第 3 轮: 上行参数微调

只有在第 1/2 轮通过后才做:

- 将 `SHORT_UP_SPEED` 从 `16` 调到 `20`。
- 其它参数不动。
- 验证 `drop -> drop_hover` 是否减少软通过。

## 9. Claude 实施清单

Claude 应输出或修改:

- 新增 `mycobot_pc_tests/teach_replay_pick_return_ready.py`。
- 不修改 `mycobot_pc_tests/teach_replay_pick.py`。
- 运行静态检查:

```powershell
python -m py_compile mycobot_pc_tests\teach_replay_pick_return_ready.py
```

- 将实测日志另存为:

```text
mycobot_pc_tests/audit_logs/trial_run_9_return_ready_v2_logs.md
```

日志至少包含:

- 五个示教点的 angles/coords。
- 每个点的半径 R。
- `home_ready` 的 `arm_max_diff`。
- `drop_hover -> home_ready` 的关节差值。
- `home_ready -> HOME_ANGLES` 或 `safe_return_home()` 的结果。
- 是否出现人工扶正。
- 是否空载/带物块。

## 10. Codex 复核门

Claude 完成后交给 Codex 时，需要提供:

- 新脚本路径。
- 与旧脚本的关键差异摘要。
- `py_compile` 结果。
- 实测日志路径。
- 是否修改过旧脚本，必须回答“否”。
- `home_ready` 是否满足 `arm_max_diff <= 45.0`。
- 远端点是否全部满足 `R <= 280.0`，以及是否有点超过 `260.0`。

Codex 审查重点:

- 是否不小心恢复了 `sync_send_coords()` 运动规划。
- 是否把 `R_MAX` 或 `Z_MAX` 粗暴放宽。
- 是否把 `home_ready` 错套了抓放工作区安全检查。
- 是否从 `drop_hover` 直接硬回零。
- 是否保留了失败时人工扶稳再释放舵机的安全路径。

## 11. 当前建议结论

下一步不要继续追求更远的放置点，也不要放宽最大臂展阈值。应优先通过新脚本验证:

```text
五点示教 + home_ready 中间姿态 + 推荐工作半径 R<=250mm
```

如果该方案通过，PC 端调试就从“可抓取”进入“可重复、安全回零”的阶段；后续板上 CPU 控制也可以直接复用这套状态机思想。

## 12. run-10 实跑复核与 V2.1 修改建议

> 依据: `mycobot_pc_tests/audit_logs/trial_run_10_logs.md`
> 状态: V2 五点示教方向有效，但尚未达到“0 轮人工扶正”目标。
> 重要修正: Claude 实现期已将本方案 §6 中 `RETURN_ARM_JOINT_MAX_DELTA=55.0` 修正为 `90.0`，理由成立。`drop_hover -> home_ready` 本质是大幅拉直，小臂 J3 单轴变化可达 60~80 度；55 度会误拒更接近 HOME 的优秀 home_ready。

### 12.1 run-10 关键证据

- `home_ready` 示教点: `angles=[7.29, -43.06, 4.57, 12.91, -6.41, 55.37]`。
- 示教时 `home_ready arm_max_diff=43.1`，满足 `<=45.0`。
- `drop_hover -> home_ready` 过渡关节差: `arm_max_delta=80.2`，主要来自 J3 从 `-75.58` 拉直到 `4.57`。
- 自动回放到 `home_ready` 时触发软通过，实际角度为 `[8.26, -45.35, 3.33, 11.86, -5.53, 54.75]`。
- 软通过后实际 `arm_max_diff=45.4`，刚好超过 `45.0`，因此 `safe_return_home()` 仍进入人工扶正流程。
- 人工扶正 1 轮后角度 `[7.99, -36.38, 3.51, 12.12, -4.48, 54.75]`，`arm_max_diff=36.4`，这是下一轮更优 `home_ready` 的参考姿态。
- step 5、step 9、step 10 都出现 `sync_send_angles 返回 0` 但实际误差约 `2.0~2.3 deg`、坐标误差约 `10~13 mm` 的软通过。

### 12.2 可能原因判断

当前最可能原因不是 HOME 定义错误，也不是 `home_ready` 方向错误，而是 **home_ready 安全余量不足**。

具体链路:

1. 示教目标 J2 为 `-43.06`，距离 `45.0` 安全门只有约 `1.94 deg` 余量。
2. `drop_hover -> home_ready` 是大角度拉直动作，J3 变化 `80.15 deg`，固件到位判定返回 `0`，脚本按软到位机制接受。
3. 软到位后的实际 J2 为 `-45.35`，相对示教目标多偏约 `2.29 deg`。
4. 这个偏差本身仍在软到位容差内，但它吃掉了全部安全余量，使 `arm_max_diff` 从目标 `43.1` 变成实际 `45.4`。
5. `safe_return_home()` 使用实际关节角重新判断，这是正确的安全设计，所以进入人工扶正。

因此，不应把这次看成 V2 方案失败；它证明了 `home_ready` 可以把扶正从 2 轮降到 1 轮，但还需要把 `home_ready` 从“刚过门”改成“留足余量”。

### 12.3 不建议优先放宽的项

暂不建议把 `ARM_MAX_DIFF_SAFE` 从 `45.0` 放宽到 `50.0` 作为下一步首选。

原因:

- run-10 的失败只差 `0.4 deg`，属于目标余量不足，不是安全门明显过严。
- 放宽阈值会改变所有回零入口的安全边界，不只影响 `home_ready`。
- 当前已有更安全的证据点: 人工扶正后的 `[7.99, -36.38, 3.51, 12.12, -4.48, 54.75]` 能把 `arm_max_diff` 降到 `36.4`。

只有在连续 2~3 次把 `home_ready` 目标压到 `arm_max_diff<=40` 后，实际回放仍稳定卡在 `45~50` 区间，才重新评估是否引入更细的“home_ready 专用阈值”。即便引入，也不要全局放宽 `ARM_MAX_DIFF_SAFE`。

### 12.4 V2.1 优先修改建议

Claude 下一轮应只改一个主变量: **home_ready 余量**。先不要同时调整上行速度，以便判断回零改善是否来自 home_ready。

建议修改:

1. 新增 `HOME_READY_TARGET_ARM_MAX = 40.0`。
2. `record_return_ready_point()` 中把 `arm_max_diff<=45.0` 从“可保存硬门”改成两级:
   - `arm_max_diff <= 40.0`: 推荐保存，可进入 0 轮回零验证。
   - `40.0 < arm_max_diff <= 45.0`: 允许保存但打印强警告，说明可能像 run-10 一样软通过后越界。
   - `arm_max_diff > 45.0`: 继续禁止保存。
3. 示教提示语改为更具体:
   - 重点把 J2 绝对值压到约 `35~38 deg`。
   - J3 尽量接近 `0~5 deg`。
   - 可参考 run-10 人工扶正后的姿态 `[7.99, -36.38, 3.51, 12.12, -4.48, 54.75]`。
4. `checked_return_transition()` 软通过后必须增加当前实际姿态校验:
   - 读取实际角度。
   - 计算 `arm_max_diff`。
   - 若实际 `arm_max_diff <= 45.0`，再进入 `safe_return_home()`。
   - 若实际 `arm_max_diff > 45.0`，不要直接进入人工扶正；先低速重试一次 `sync_send_angles(home_ready["angles"], speed=12~15, timeout=15~20)`。
   - 重试后仍 `>45.0`，提示重新示教更直立的 `home_ready`。
5. 第 11 步日志不要写“自动回零通过”，除非 `safe_return_home()` 未进入人工扶正分支。日志应区分:
   - `0 轮自动回零通过`
   - `触发人工扶正 1 轮后完成`
   - `回零失败`

### 12.5 速度参数后置

run-10 的 step 5/9 仍为软通过，说明 `SHORT_UP_SPEED=16` 对上行动作仍偏紧；但下一轮不要同时调整它。

建议顺序:

1. run-11: 只优化 `home_ready`，目标是 0 轮人工扶正。
2. run-12: 若 run-11 达到 0 轮回零，再把 `SHORT_UP_SPEED` 从 `16` 提到 `18`，观察 step 5/9 是否减少软通过。
3. 若 `18` 仍软通过，再评估 `20`；不要直接把多个速度同时调高。

`drop_hover -> home_ready` 的 `RETURN_TRANSITION_SPEED=20` 不建议继续提速。若仍软通过，优先增加一次低速末端校正或把 timeout 从 `20s` 提到 `30s`，而不是继续加速。

### 12.6 run-11 验收标准

run-11 仍建议空载先测。通过条件:

- 五点示教全部保存，抓放点仍满足 `R<=280.0`，推荐尽量 `R<=250.0`。
- `home_ready` 目标 `arm_max_diff<=40.0`。
- `drop_hover -> home_ready` 无碰撞、无线缆拖拽。
- 软通过后实际 `home_ready arm_max_diff<=45.0`。
- 第 11 步 `safe_return_home()` 不进入人工扶正流程。
- 日志明确记录 `0 轮人工扶正`。

如果 run-11 仍需人工扶正，Claude 应把目标/实际 `home_ready` 角度差、`arm_max_diff`、是否重试过低速校正写入 `trial_run_11_logs.md`，再交 Codex 复核。

## 13. run-11 两轮带载复核与 V2.2 修改建议

> 依据: `mycobot_pc_tests/audit_logs/trial_run_11_logs.md`
> 状态: V2.1 的 `home_ready` 余量门已经通过两轮带载验证。下一阶段焦点从“回零自动化”转为“上行到位精度、等待时间、点位复用”。
> 重要结论: 不要再优先改 `home_ready` 安全门。`HOME_READY_TARGET_ARM_MAX=40.0` 成立，应保留。

### 13.1 run-11 关键证据

两轮实验均为带载:

- Run A: `2cm` 正方体。
- Run B: `3cm` 正方体。

`home_ready` 余量门验证:

| 指标 | Run A | Run B | 判断 |
| ---- | ----- | ----- | ---- |
| 示教 `home_ready arm_max_diff` | `32.3` | `30.8` | 远低于 `40.0` 推荐门 |
| 实际 `home_ready arm_max_diff` | `34.2` | `32.6` | 远低于 `45.0` 安全门 |
| `safe_return_home` | `auto` | `auto` | 均为 0 轮人工扶正 |

上行到位误差:

| 动作 | Run A | Run B | 判断 |
| ---- | ----- | ----- | ---- |
| step 5 `pick -> pick_hover` | `delta_xyz=11.1mm`, `max_err=2.0deg` | `delta_xyz=12.8mm`, `max_err=2.2deg` | 均为软通过 |
| step 9 `drop -> drop_hover` | `delta_xyz=11.6mm` | `delta_xyz=16.2mm`, `max_err=2.5deg` | Run B 为当前最大误差 |

工作区:

- Run A 业务点半径约 `252.0~258.1mm`，均在可接受调试区。
- Run B 业务点半径约 `260.2~273.8mm`，四个业务点全部在边缘风险区。
- Run B 仍能抓放成功，但其 `drop -> drop_hover` 上行误差最大，应视为边缘区带载放大的风险信号，而不是稳定量产点位。

### 13.2 原因分析

#### A. 回零问题已经关闭

run-10 的失败链路是:

```text
home_ready 示教 arm_max_diff=43.1
-> 软通过后实际 J2 多偏约 2.29deg
-> 实际 arm_max_diff=45.4
-> 触发人工扶正
```

run-11 把 `home_ready` 目标压到 `30~32deg` 后，软通过后的实际姿态仍只有 `32~34deg`，留有约 `10deg` 以上余量。因此 `HOME_READY_TARGET_ARM_MAX=40.0` 是有效修复，应继续保留。

#### B. 当前最大风险是“上行软通过后的残余误差”

Run B step 9:

```text
目标 drop_hover: [-0.35, -57.48, -48.16, 20.56, -0.52, 45.87]
实际角度:       [0.35, -59.94, -49.04, 19.51, 0.08, 45.0]
max_err=2.5deg
delta_xyz=16.2mm
```

这个误差不是坐标读取毛刺，而是关节角残差映射到末端产生的真实位置偏差。3cm 正方体接近夹爪可容忍范围时，`10~16mm` 误差足以让夹取中心偏离，后续可能出现抓空、夹歪或碰撞边缘。

#### C. Run B 边缘半径放大误差

Run B 所有业务点都在 `R>260mm`，其中 drop 点达到 `R=273.8mm`。远端半径会带来三个后果:

- 同样的角度误差映射成更大的末端位移。
- 带载上行时 J2/J3 负载更大，更容易停在软到位容差边缘。
- 夹爪抓 3cm 物块时，位置余量比 2cm 更小。

因此 Run B 的 `16.2mm` 最大误差同时来自 **软通过残余角差** 和 **远端半径放大**。

#### D. 等待偏长来自“等固件判到位”，不是物理运动慢

step 5/9 的 `SHORT_UP_TIMEOUT=25s` 会等到固件 `sync_send_angles` 返回 `0` 后才进入诊断。run-11 表明物理姿态已经接近目标，但固件到位判定不收敛，继续等满 25s 主要是在等失败，不是在等运动完成。

### 13.3 V2.2 优先级

Claude 下一轮不应再改 `home_ready`，也不应优先放宽工作区或安全门。建议按以下顺序执行。

#### 优先级 1: 上行软通过后增加二次微调

目标: 把上行动作软通过后的 `max_err` 从 `2.0~2.5deg` 压到 `<=1.2deg`，把 `delta_xyz` 尽量压到 `<=10mm`。

建议新增参数:

```python
SOFT_REFINE_ENABLE = True
SOFT_REFINE_ANGLE_TRIGGER = 1.2
SOFT_REFINE_COORD_TRIGGER = 10.0
SOFT_REFINE_SPEED = 8
SOFT_REFINE_TIMEOUT = 6
SOFT_REFINE_MAX_ROUNDS = 1
```

建议逻辑放在 `checked_short_angles()` 内，仅对 `allow_soft_success=True` 的动作启用:

1. `sync_send_angles()` 返回 `0` 后读取实际角度和坐标。
2. 若 `max_angle_delta <= 3.0deg` 且 `coord_delta <= 25mm`，先不要立刻软通过。
3. 若 `max_angle_delta > 1.2deg` 或 `coord_delta > 10mm`，追加一次低速微调:

```python
mc.sync_send_angles(target_angles, SOFT_REFINE_SPEED, timeout=SOFT_REFINE_TIMEOUT)
```

4. 微调后重新读取实际角度和坐标。
5. 若微调后 `max_angle_delta <= 1.2deg` 或 `coord_delta <= 10mm`，标记为“微调通过”。
6. 若仍未达标但仍满足旧软通过门 `<=3deg / <=25mm`，允许继续，但日志必须标为“软通过但残差偏大”。
7. 对 `pick_hover` / `drop_hover` 这两个带物块上行点，若 `coord_delta > 15mm`，建议打印强警告；Run B 的 `16.2mm` 就属于这个级别。

注意:

- 二次微调只用于关节角目标，不恢复 `sync_send_coords()`。
- 微调速度要低，避免带载末端晃动。
- 微调最多 1 轮，避免来回抖动或长时间等待。

#### 优先级 2: 缩短上行超时窗口

在加入微调的同时，可把:

```python
SHORT_UP_TIMEOUT = 25
```

改为:

```python
SHORT_UP_TIMEOUT = 15
```

原因:

- run-11 证明上行动作物理上已接近目标，25s 主要是在等固件到位判定失败。
- 缩短到 15s 可更早进入诊断和微调。
- 若 15s 下 `max_err` 变大，再回退到 20s，不建议回到 25s。

建议不要在同一轮提高 `SHORT_UP_SPEED`。先用 `16 + 15s + 低速微调` 验证残差是否下降。

#### 优先级 3: 速度调参后置

如果 V2.2 微调后 Run B 仍出现:

```text
max_err > 1.5deg 或 delta_xyz > 12mm
```

再开 V2.3 单独试探速度:

```python
SHORT_UP_SPEED = 18
```

若 `18` 仍不够，再试 `20`。暂不建议直接到 `25`，因为 3cm 物块远端带载时速度过高可能引入摆动、夹爪滑移或落点偏差。

#### 优先级 4: 回零速度可作为低风险优化

`home_ready -> HOME` 已两轮 `auto`，回零路径短且安全。可新增独立参数:

```python
HOME_RETURN_SPEED = 20
HOME_RETURN_TIMEOUT = 12
```

替代 `safe_return_home()` 内硬编码的:

```python
mc.sync_send_angles(HOME_ANGLES, 15, timeout=ANG_REPLAY_TIMEOUT)
```

但这属于体验优化，不影响抓取可靠性。应排在上行误差修复之后。

#### 优先级 5: 点位复用机制

建议新增 JSON 预设机制，但不要和上行微调混在同一个验证目标里。

推荐文件:

```text
mycobot_pc_tests/presets/teach_points_run11_a_2cm.json
mycobot_pc_tests/presets/teach_points_run11_b_3cm_edge.json
```

推荐 CLI:

```powershell
python mycobot_pc_tests/teach_replay_pick_return_ready.py --preset run11_a_2cm
python mycobot_pc_tests/teach_replay_pick_return_ready.py --preset run11_b_3cm_edge
```

JSON 内容至少包含:

- `name`
- `created_from_log`
- `object_size_cm`
- `points.pick_hover.angles`
- `points.pick_hover.coords`
- `points.pick.angles`
- `points.pick.coords`
- `points.drop_hover.angles`
- `points.drop_hover.coords`
- `points.drop.angles`
- `points.drop.coords`
- `points.home_ready.angles`
- `points.home_ready.coords`
- `safety.radii`
- `safety.home_ready_arm_max_diff`
- `notes`

加载预设后仍必须执行:

- 半径分区打印。
- `validate_short_angle_pair()`。
- `validate_return_angle_pair()`。
- `validate_return_ready()`。
- 用户确认轨迹无障碍。

不能因为是预设就跳过安全门。

### 13.4 run-12 建议实验设计

run-12 只验证 V2.2 “微调 + 缩短 timeout”，不要同时调高速度。

建议配置:

```text
SHORT_UP_SPEED = 16
SHORT_UP_TIMEOUT = 15
SOFT_REFINE_ENABLE = True
SOFT_REFINE_SPEED = 8
SOFT_REFINE_TIMEOUT = 6
SOFT_REFINE_MAX_ROUNDS = 1
```

建议使用 Run B 的 3cm 物块作为压力测试，但尽量把业务点移回:

```text
R <= 260mm
```

如果场地必须保留 Run B 的 `R=260~274mm` 边缘区，也可以继续测，但日志要明确标记为“边缘风险压力测试”，不能当作推荐部署点。

验收标准:

- 两轮带载仍完整跑通。
- `home_ready` 仍 0 轮人工扶正。
- step 5 和 step 9 若触发软通过，微调后 `max_err <= 1.2deg` 或 `delta_xyz <= 10mm`。
- 若无法达到上述门槛，至少不能超过 run-11 最大值 `16.2mm`。
- 总等待时间应明显短于 run-11；记录 step 5/9 从下发到继续下一步的耗时。

### 13.5 Claude 修改清单

Claude 应按以下顺序修改:

1. 在 `teach_replay_pick_return_ready.py` 新增软通过微调参数。
2. 在 `checked_short_angles()` 中实现一轮低速微调，保留旧软通过作为兜底。
3. 将 `SHORT_UP_TIMEOUT` 从 `25` 改为 `15`。
4. 增加 step 耗时日志，至少覆盖 step 5、step 9、step 10、step 11。
5. 暂不修改 `SHORT_UP_SPEED`。
6. 暂不实现 JSON 预设，除非用户明确要求下一步优先做点位复用。
7. 运行:

```powershell
python -m py_compile mycobot_pc_tests\teach_replay_pick_return_ready.py
```

8. 实机日志写入:

```text
mycobot_pc_tests/audit_logs/trial_run_12_logs.md
```

日志必须包含:

- step 5/9 微调前后 `max_err`。
- step 5/9 微调前后 `delta_xyz`。
- 是否触发强警告 `coord_delta > 15mm`。
- 每个关键 step 的耗时。
- 是否仍 0 轮人工扶正。

### 13.6 Codex 复核门

Claude 完成后，Codex 复核重点:

- 是否仍未恢复 `sync_send_coords()`。
- 微调是否只针对 `allow_soft_success=True` 的关节角动作。
- 微调是否最多 1 轮。
- `SHORT_UP_SPEED` 是否保持 `16`。
- `home_ready` 安全门是否保持 `HOME_READY_TARGET_ARM_MAX=40.0` 与 `ARM_MAX_DIFF_SAFE=45.0`。
- Run B 边缘半径是否在日志中被清楚标注，不能被写成推荐工作区。

## 14. run-12 复核与 V2.3 下一步修改建议

### 14.1 run-12 结论

`trial_run_12_logs.md` 显示 V2.2 带 3cm 物块完整跑通，且等待时间显著下降:

| 项目 | run-11 现象 | run-12 结果 | 结论 |
| --- | --- | --- | --- |
| step 5 `pick -> pick_hover` | 约 25s 后软通过 | 4.5s 正常返回 `1`，`delta_xyz=10.7mm` | 等待时间问题明显缓解 |
| step 9 `drop -> drop_hover` | 约 25s 后软通过，最大曾到 `16.2mm` | 11.9s 正常返回 `1`，`delta_xyz=9.9mm` | 已回到可接受范围 |
| step 10 `drop_hover -> home_ready` | 约 20s | 3.1s，实际 `arm_max_diff=32.8` | 中间姿态仍有效 |
| step 11 `home_ready -> HOME` | 约 20s | 1.8s，`safe_return_home="auto"` | 0 轮人工扶正目标成立 |

当前可作为稳定基线的参数:

```python
SHORT_UP_SPEED = 16
SHORT_UP_TIMEOUT = 15
SOFT_REFINE_ENABLE = True
SOFT_REFINE_SPEED = 8
SOFT_REFINE_TIMEOUT = 6
SOFT_REFINE_MAX_ROUNDS = 1
HOME_RETURN_SPEED = 20
HOME_RETURN_TIMEOUT = 12
HOME_READY_TARGET_ARM_MAX = 40.0
ARM_MAX_DIFF_SAFE = 45.0
```

### 14.2 根因判断更新

不要把 run-12 改善单独归因到 `SHORT_UP_TIMEOUT 25 -> 15`。本轮同时发生了两个有利变化:

1. 上行超时缩短后，step 5/9 不再等满 25s，固件直接正常返回 `1`。
2. 成功轮业务点全部收回到 `R<=252mm`，明显优于 run-11 Run B 的远端边缘区 `R=260~274mm`。

因此更稳妥的判断是：**短超时 + 内收工作半径共同改善了等待和到位误差**。如果后续必须把物块放到 `R>260mm`，仍应按“边缘风险压力测试”记录，不应写成推荐部署点。

### 14.3 已验证与未验证边界

已实跑验证:

- `home_ready` 余量门有效：run-12 示教 `arm_max_diff=31.0`，实跑后 `32.8`，均远低于 `45.0`。
- `HOME_RETURN_SPEED=20` 与 `HOME_RETURN_TIMEOUT=12` 在 run-12 中可用，回零 1.8s 且未触发人工扶正。
- `record_teach_point()` 循环重试方向正确：越过安全边界时不再应直接中断整轮示教。

尚未实跑验证:

- `_soft_refine()` 在 run-12 没有触发。它只能标记为“保留兜底逻辑”，不能标记为“微调机制已验证”。
- `record_teach_point()` 的 V2.3 循环重试需要单独做一次“故意拖到 R>280 后重新拖回安全区”的回归测试。
- `home_ready_points.md` 只是历史点位库，尚未实现命令行预设加载。

### 14.4 Claude 下一步最小修改清单

优先级 1：实现点位复用，减少重复示教变量。

- 新增 `mycobot_pc_tests/presets/` 目录。
- 新增 JSON 预设文件，至少保存 run-12 的五点:
  - `pick_hover`
  - `pick`
  - `drop_hover`
  - `drop`
  - `home_ready`
- CLI 建议:

```powershell
python mycobot_pc_tests/teach_replay_pick_return_ready.py --preset run12_3cm_inboard
python mycobot_pc_tests/teach_replay_pick_return_ready.py --teach --save-preset run13_candidate
```

加载预设后仍必须执行全部安全门:

- `is_safe_coord()`。
- 半径分区打印。
- `validate_short_angle_pair()`。
- `validate_return_angle_pair()`。
- `validate_return_ready()`。
- 用户确认轨迹无障碍。

优先级 2：保守修正 `_soft_refine()` 的日志格式兜底。

当前 `_soft_refine()` 使用 `prev_coord_delta:.1f` 打印，默认 `prev_coord_delta` 一定是数字。现有 step 5/9/10 都传入 `expected_coords`，所以 run-12 未暴露问题；但若未来复用到没有期望坐标的动作，`prev_coord_delta=None` 会在打印阶段报错。

建议新增一个小格式化函数:

```python
def fmt_mm(value):
    return "N/A" if value is None else f"{value:.1f}mm"
```

并把 `_soft_refine()` 中所有 `prev_coord_delta` / `coord_delta` 的日志打印统一走 `fmt_mm()`。这只改诊断输出，不改变运动逻辑。

优先级 3：给 `record_teach_point()` 增加退出提示。

当前循环重试能避免整轮异常退出，但用户若连续读不到合法点，只能反复回车。建议把提示改成:

```text
保持稳定后按 Enter 读取，输入 q 放弃本轮示教...
```

若输入 `q`，再统一走原有安全释放流程退出，而不是继续死循环。该项不影响 run-12 成功路径，但能降低现场调试压力。

### 14.5 run-13 验收建议

run-13 不建议继续调高速度，先验证“预设复用 + 当前 V2.2 参数”:

1. 使用 run-12 预设点位直接回放，跳过手动五点示教。
2. 带 3cm 物块，工作半径保持 `R<=252mm` 或至少 `R<=260mm`。
3. 记录 step 5/9/10/11 耗时、`delta_xyz`、`safe_return_home` 返回值。
4. 验收门:
   - 全流程跑通。
   - `home_ready` 仍 0 轮人工扶正。
   - step 5/9 `delta_xyz <= 12mm`，不能回到 run-11 的 `16.2mm`。
   - 总耗时保持在 30s 量级。
5. 另做一次示教回归：故意拖出 `R>280`，确认 `record_teach_point()` 能提示重试并允许重新保存安全点。

只有 run-13 预设复用稳定后，才考虑把边缘半径 `R=260~274mm` 作为压力测试单独验证；不要把边缘点位作为默认使用方案。

### 14.6 串口可选参数识别 Bug 与修复建议

在第十四次试运行（2026-07-07）时，发现若在使用 `--save-preset` 或 `--preset` 等可选参数时未显式指定 `--port`，会触发串口开启失败报错：`could not open port '--save-preset'`。

**1. 故障根因**：
脚本中的 `get_port()` 采用了 `if len(sys.argv) > 1: return sys.argv[1]` 的老旧位置参数提取逻辑。当命令行使用其他可选参数但未指定 `--port` 时，自动识别到的 `sys.argv[1]`（即 `"--save-preset"` 等）被误判为串口名称传入 `MyCobot` 造成报错。

**2. 修复方案建议**：
在未来的维护或优化中，应彻底移除这种非 argparse 风格的盲目提取。只留下基于 `serial` 库自动识别与交互选择串口的逻辑，由 `argparse` 接管参数解析：
```python
def get_port():
    # 彻底废除位置参数盲读：
    # if len(sys.argv) > 1:
    #     # return sys.argv[1]

    ports = serial.tools.list_ports.comports()
    ...
```
该漏洞与修复建议同样适用于以下脚本：
*   `mycobot_pc_tests/teach_replay_pick_return_ready.py` (V2.3)
*   `mycobot_pc_tests/teach_replay_pick.py`
*   `mycobot_pc_tests/teach_and_pick.py`

### 14.7 run-14 复核与 V2.4 修改建议（串口 Bug 修复 + 末段回零提速）

> 依据: `mycobot_pc_tests/audit_logs/trial_run_14_logs.md`
> 状态: V2.3 预设复用稳定（前后两次 0 轮人工扶正、动作重合度高）。本轮焦点转为
>   ① 串口参数 Bug 根治；② 用户附加要求——加快最终回零响应速度。
> 边界: 本节改动属 myCobot 动作链路（L2/L3/L4 触及 safe_return_home 签名与动作参数），
>   依 AGENTS.md Codex Gate，须 Codex 复核后再实机验证 run-15。

#### 14.7.1 串口 Bug 修复范围修正（对 §14.6 / Gemini 建议的收敛）

§14.6 称"修复同样适用于 teach_replay_pick.py 和 teach_and_pick.py"——**此范围需收敛**：

| 脚本 | 有 argparse/可选参数? | Bug 实际触发? | 是否改? |
| --- | --- | --- | --- |
| `teach_replay_pick_return_ready.py` (V2.3) | 有 | **会**（--save-preset 被误读） | **改** |
| `teach_replay_pick.py` | 无 | 不会（无可选参数可被误判） | **不改**（方案 §0 硬规则：不直接修改此基线） |
| `teach_and_pick.py` | 无 | 不会（同上） | **不改**（无实际 bug + 归档脚本） |

两个无 argparse 的脚本只是"代码风格老旧"，但没有任何可选参数会被 `sys.argv[1]` 误判为串口，**不存在实际 bug**；且 `teach_replay_pick.py` 受方案 §0 明确保护。故 V2.4 只修 V2.3 脚本一个文件——删 `get_port()` 里 `if len(sys.argv) > 1: return sys.argv[1]` 两行。`main()` 已用 argparse 接管 `--port`，`get_port()` 只在 `--port` 未给时被调用，删盲读后纯做 `comports()` 自动枚举 + 交互选择，与 argparse 完全兼容。

#### 14.7.2 末段回零提速——真实耗时归因

trial_run_14 关键 step 耗时：

| Step | 含义 | Run1 | Run2 | 说明 |
| --- | --- | --- | --- | --- |
| 5 | pick → pick_hover（带载上行） | 3.4s | 1.4s | 正常 |
| **9** | **drop → drop_hover（带载上行）** | **23.0s** | **23.0s** | ⚠️ 瓶颈：触发软通过+微调且微调不收敛 |
| 10 | drop_hover → home_ready | 2.6s | 2.6s | 正常 |
| 11 | home_ready → HOME（回零） | 2.4s | 2.0s | 正常 |

严格意义的"回零"（step 10+11）≈5s，已经不慢。真正大头是 step 9 的 23s（放物块后抬起，属回零前序）。所以提速分两档：窄义回零(step10+11) 与 广义末段(step9+10+11)。

step 11 真正"动"之前有冗余读数开销：`_verify_actual_pose_for_auto_return` 读一次角度、`verify_coords_near` 读一次坐标（两者必要），但 `safe_return_home` 又重读一次角度（与第1步冗余、臂静止）+ 读一次 `diag_coords` 纯诊断坐标（只打印不决策、完全冗余）。每次 `get_filtered_*` 静态臂约 0.25-1s。这是"响应速度"的可压缩空间。

#### 14.7.3 四个提速杠杆（用户已批 L1+L2+L3+L4 全套）

*   **L1（去冗余诊断读）**：`safe_return_home` auto 分支去掉纯诊断的 `diag_coords` 读数（只打印不决策）；manual 分支保留（扶正诊断有价值）。省 ~0.4-0.8s，风险零。
*   **L2（缓存角度）**：`_verify_actual_pose_for_auto_return` 返回校验通过时的实际角度（越界/走人工扶正返回 None）；`checked_return_transition` 透传；`safe_return_home` 新增 `cached_angles=None` 参数，传入则跳过自身 `get_filtered_angles` 重读。省 ~0.25-0.4s。`cached_angles` 默认 None → `prepare_phase` 路径与旧调用方行为不变；安全门（calc_home_diffs + 有限值校验 + arm_max_diff 门）不跳过，只省读数。
*   **L3（回零速度 20→25）**：`HOME_RETURN_SPEED` 20→25。该段是全程最安全的移动——近直立位、无负载（夹爪已张开放下物块）、距离短、单轴 delta≤45。提速 25 风险低于 step5/9 带载远端上行。提速后固件不收敛返回 0 → `safe_return_home` 返回 `"failed"` → 提示扶稳+release（**安全失败，不撞机**）。需单独验证 run。
*   **L4（收紧 step9 微调超时 6→3s）**：`SOFT_REFINE_TIMEOUT` 6→3s。run-14 step9 实测微调不收敛（固件 plateau 在 ~2.1°），6s 纯等待；微调 best-effort，失败仍走旧软通过门(3°/25mm) 兜底，逻辑不变。省 step9 ~3s（针对广义末段瓶颈）。

预期：窄义回零 step10+11 ~5s→~3s；广义末段 step9+10+11 ~28s→~23s。

#### 14.7.4 Claude 修改清单（已落地）

1. `get_port()` 删位置参数盲读两行（§14.6 根治，只改 V2.3 脚本）。
2. `SOFT_REFINE_TIMEOUT` 6→3s（L4）。
3. `HOME_RETURN_SPEED` 20→25（L3）。
4. `_verify_actual_pose_for_auto_return` 返回实际角度；`checked_return_transition` 透传；`safe_return_home(cached_angles=...)` 复用（L1+L2）。`auto_phase_v2` step11 传入缓存角度。
5. 文件头 docstring 追加 V2.4 W/X/Y 条目。
6. 运行 `python -m py_compile mycobot_pc_tests/teach_replay_pick_return_ready.py` → 通过。
7. mock-serial 验证 `get_port()` 在 `['--save-preset','my_new_test']`（无 --port）下不再返回 `"--save-preset"`，正确进入 comports 枚举/退出。

#### 14.7.5 未动项

`SHORT_UP_SPEED` 保持 16；`ARM_MAX_DIFF_SAFE`=45 / `HOME_READY_TARGET_ARM_MAX`=40 不变；`SHORT_UP_TIMEOUT`=15 不变；`teach_replay_pick.py` 与 `teach_and_pick.py` 不改（§0 保护 + 无实际 bug）。

#### 14.7.6 run-15 验收建议

1. 先验证串口 Bug：`python mycobot_pc_tests/teach_replay_pick_return_ready.py --save-preset <name>`（不传 --port）应正常进入 comports 自动枚举，不再报 `could not open port '--save-preset'`。
2. 带 3cm 物块，工作半径 `R<=252mm`，用 run-14 的 `my_new_test` 预设或重新示教。
3. 记录 step 5/9/10/11 耗时、`delta_xyz`、`safe_return_home` 返回值、是否触发微调及微调是否收敛。
4. 验收门：
   *   全流程跑通，0 轮人工扶正。
   *   step 11 回零 `safe_return_home="auto"`，且 `res==1`（L3 提速到 25 后固件仍收敛）。若 `res!=1` 返回 `"failed"`，回退 `HOME_RETURN_SPEED` 25→20 再测（不要回到 15）。
   *   窄义回零(step10+11) 耗时应明显低于 run-14 的 ~5s（目标 ~3s）。
   *   step 9 若触发微调，记录微调耗时（L4 后应 ≤3s）与微调前后 `max_err`/`delta_xyz`。
   *   `delta_xyz` 不应回升到 run-11 Run B 的 16.2mm。
5. 只有 run-15 稳定后，才考虑把边缘半径 `R>260mm` 作为压力测试单独验证。

#### 14.7.7 Codex 复核门

Codex 复核重点：
*   `get_port()` 盲读是否彻底删除、是否未误伤 `--port` 既有路径。
*   `safe_return_home(cached_angles=...)` 是否仅在 auto 分支省读数、是否仍走 calc_home_diffs + 有限值校验 + arm_max_diff 安全门（不能跳过安全判定）。
*   `cached_angles=None` 默认是否保证 `prepare_phase` 的 `safe_return_home(mc)` 旧调用行为不变。
*   `HOME_RETURN_SPEED=25` 是否仅在 arm_max_diff≤45 的 auto/manual 末段生效，是否保留了 `res!=1 → "failed" → 扶稳+release` 的安全失败路径。
*   `SOFT_REFINE_TIMEOUT=3` 是否保留了"微调失败走旧软通过门(3°/25mm)兜底"的逻辑。
*   是否未恢复 `sync_send_coords()`、未放宽 `ARM_MAX_DIFF_SAFE`/`HOME_READY_TARGET_ARM_MAX`/`R_MAX`。
*   `teach_replay_pick.py` 与 `teach_and_pick.py` 是否确实未被修改。
