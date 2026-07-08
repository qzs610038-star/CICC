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

### 14.8 末段回零到位判定优化与提速方案（V2.5 展望）

依据 `trial_run_15_logs.md` 反馈，虽然 V2.4 将窄义回零耗时压缩至 1.2s，但相对于动作幅度而言，其响应依旧有优化空间。

#### 14.8.1 物理耗时与到位检测延迟瓶颈分析
在回零段（Step 11），从 `home_ready` 姿态（大臂偏差仅约 24°）移动到 `HOME` 直立零位，其物理行程极短且无负载。物理运动通常在 0.5s 内已基本完成，但 `safe_return_home` 使用官方的 `sync_send_angles`（同步阻塞接口），其内部依赖固件的 `is_in_position` 状态。
*   **固件缺陷**：控制板固件的到位判定不灵敏，需要舵机彻底静止且偏差小于严苛死区后才返回到位标志，导致了大量的“无意义尾端等待”（约占 0.5s~1s）。

#### 14.8.2 提速优化方向设计

##### 1. 软件级自主软到位判定（异步化指令）
放弃官方阻塞式 `sync_send_angles`，改用异步非阻塞的 `send_angles` 指令启动回零，并在 Python 脚本端执行自主检测循环：
*   **逻辑**：
    ```python
    mc.send_angles(HOME_ANGLES, 25)
    start_t = time.time()
    while time.time() - start_t < 2.5: # 2.5s 软超时
        actual = get_filtered_angles(mc)
        if actual:
            max_diff = max(abs(actual[i] - HOME_ANGLES[i]) for i in range(6))
            if max_diff <= 1.0: # 软到位阈值（1.0度以内物理上已被视为回正）
                break
        time.sleep(0.05)
    ```
*   **效果预期**：可在物理基本到位时提前 0.6s ~ 0.8s 退出阻塞，使 Step 11 回零响应压减到 0.5s 级别。

##### 2. 航路点平滑过渡（消除 home_ready 停顿）
目前在 `home_ready` 处，前序动作 `drop_hover -> home_ready`（Step 10）必须完全同步到位并静止后，才启动 `home_ready -> HOME`（Step 11）。这带来了“减速刹车-静止等待-重新起步”的停顿开销。
*   **优化方案**：在 Step 10 运行到中后期（例如通过读角度确认与 `home_ready` 的最大轴偏差已小于 5°，或者通过非阻塞控制定时检测），**在机械臂还在运动时，不等待其停稳，直接发送最终的 `HOME` 目标指令**。
*   **效果预期**：将折线过渡平滑为弧线连贯动作，完全免去空中停顿，提升流畅度，消除约 1.0s 机械开销。

##### 3. 回零速度继续微调 (25 $\rightarrow$ 30)
配合自主软到位判定，可安全地将 `HOME_RETURN_SPEED` 提升至 30%，以缩短物理运动行程耗时。

#### 14.8.3 V2.5 落地实现（run-15 后，用户批"方向1+2+3+D0 全上"）

> 状态: 代码已落地 + py_compile 通过 + 6 项 mock 控制流测试全 PASS。属动作链路改动，
>   依 Codex Gate 须 Codex 复核（v2.5_codex_review_packet.md）后再实机验证 run-16。

##### 新增参数
```python
HOME_RETURN_ASYNC_ENABLE = True       # 方向1总开关：False 回退 V2.4 阻塞 sync_send_angles
HOME_RETURN_ASYNC_SPEED = 30          # 方向3：异步回零速度 25->30
HOME_RETURN_ASYNC_SOFT_TOL = 1.0      # 软到位阈值（度）
HOME_RETURN_ASYNC_TIMEOUT = 2.5       # 软超时（s），走 sync 收尾兜底
HOME_RETURN_ASYNC_POLL = 0.05         # 轮询间隔（s）
SMOOTH_HANDOFF_ENABLE = True          # 方向2总开关（固件打断行为未知，独立可回退）
SMOOTH_HANDOFF_NEAR_TOL = 5.0         # 接近 home_ready 的提前下发阈值（度）
```

##### 新增函数
- `get_angles_once(mc)`：单次关节角读数 + 数值合法性校验，**不做连续两次稳定要求**。专为异步轮询设计——运动中连续两次读数自然 >3° 差，`get_filtered_angles` 稳定性门会致盲软到位循环。
- `_send_home_async(mc, label)`：方向1+3。非阻塞 `send_angles(HOME,30)` → 软到位循环（max_diff≤1° 即退出）→ 软超时走 `sync_send_angles` 收尾 + `res` 检查。返回 (status, mode, final_max_diff)。
- `_smooth_handoff_return(mc, home_ready)`：方向2。非阻塞 `send_angles(home_ready)` → 轮询接近(≤5°)即提前 `send_angles(HOME)` → 软到位。未接近时回退 V2.4 阻塞分段。返回三态字符串。

##### 修改
- `safe_return_home` auto 分支 + manual 扶正后分支：`HOME_RETURN_ASYNC_ENABLE=True` 调 `_send_home_async`；`False` 回退 V2.4 阻塞 `sync_send_angles(HOME,25,timeout=12)`。两条路径显式打印结果（D0）。
- `auto_phase_v2` step10+11：`SMOOTH_HANDOFF_ENABLE and HOME_RETURN_ASYNC_ENABLE` 调 `_smooth_handoff_return`；否则走 V2.4 分段。
- 庆祝消息措辞 V2.2→V2.5（D0）。

##### 关键设计点：get_filtered_angles 与异步轮询的冲突（mock 暴露）
mock 测试发现：异步软到位循环若用 `get_filtered_angles`（要求连续两次读数 ≤3° 差），臂运动中连续两次读数自然 >3° 差 → 返回 None → 循环盲转 → 总走 sync 兜底（异步化失效）。**必须用 `get_angles_once`**（单次合法性校验，不要稳定性门）。稳定性要求（§12.3）只适用于静止姿态的示教点记录/回零决策，运动中进度判定需单次有效读数。这是 V2.5 最关键的设计点，run-16 须确认软到位循环真出现"max_diff<=1° 提前退出"日志。

##### 安全守住
- 软超时/未接近均**不抛异常**（防臂在回零/过渡路径掉电），走 sync 收尾或回退阻塞分段。
- 所有路径保留 `res!=1 → "failed" → 扶稳+release` 安全失败。
- `arm_max_diff<=45` 安全门判定完全不变，异步化只动移动下发方式。
- 方向1/方向2 各有独立 enable 开关，异常可一行回退 V2.4。

##### 风险（Codex 复核重点）
- **方向2 固件打断行为未文档化（最高风险）**：臂运动中下发新 HOME 目标，固件响应未知。run-16 须人眼盯防，异常即 Ctrl+C + 设 `SMOOTH_HANDOFF_ENABLE=False`。Codex 可能要求默认 False 进 run-16、方向2 单独 run-17。
- 方向1 `send_angles` 非阻塞时序、方向3 速度 30 平稳性、`get_angles_once` 毛刺误判，均待 run-16 实机确认。

#### 14.8.4 run-16 验收建议

1. 先验方向1+3+D0（可临时设 `SMOOTH_HANDOFF_ENABLE=False` 隔离方向2 风险，若 Codex 要求）：
   - step11 应出现 `[V2.5 方向1] 软到位收敛 max_diff<=1.0° 提前退出` 日志，耗时 ~0.5s（vs run-15 的 1.2s）。
   - 若软超时走 sync 收尾，记录 sync res 与 final max_diff；res!=1 则回退 `HOME_RETURN_ASYNC_SPEED` 30→25 再测。
   - D0：step11 显式打印 `status/mode`，不再靠"无警告"反推。
2. 验收方向1+3 稳定后再开启方向2（`SMOOTH_HANDOFF_ENABLE=True`）：
   - step10+11 合并为平滑过渡，应出现 `[V2.5 方向2] 已接近 home_ready ... 提前下发 HOME` 日志。
   - 人眼盯防臂是否异常抖动/停顿/不切目标；异常立即 Ctrl+C + 回退开关。
   - 耗时应低于 run-15 的 step10+11 ~3.8s（目标 ~1.5s）。
3. 全程 0 轮人工扶正、`delta_xyz` 不回升到 run-11 Run B 的 16.2mm。
4. run-16 日志写入 `trial_run_16_logs.md`，至少含：各 step 耗时、软到位收敛/超时/sync 收尾的 mode、final max_diff、是否触发方向2 回退、方向2 臂行为观察。

#### 14.8.5 Codex 复核门

- `get_angles_once` 用于异步轮询是否合理，是否需"连续两次 ≤阈值"防毛刺。
- `_send_home_async` 软超时 sync 收尾 + res 检查是否保留 V2.4 安全失败语义。
- `_smooth_handoff_return` 未接近回退路径是否正确、是否重复执行 home_ready 运动。
- 方向2"未停稳即下发 HOME"前的安全校验（step0b validate_return_angle_pair + validate_return_ready）是否足够。
- 两个 enable 开关是否真能一行回退 V2.4（mock T6 验证了 async 开关，smooth 开关是否需补测）。
- 方向2 固件打断未知风险：是否同意进 run-16，还是要求默认 `SMOOTH_HANDOFF_ENABLE=False`、方向2 单独 run-17。
- `teach_replay_pick.py` / `teach_and_pick.py` 是否未改。

#### 14.8.6 Run-16 瓶颈诊断与 V2.6 门限调整方案（V2.6 展望）

依据 `trial_run_16_logs.md` 实机反馈，平滑过渡（10+11）耗时仍然高达 5.0s，且两度触发“软超时未收敛”回退阻塞 sync 兜底。

##### 1. 瓶颈诊断与根因分析
*   **不收敛现象**：在第二阶段自检回零和第三阶段合并过渡回零中，程序运行了 2.5s 软到位检测，均因 `max_diff=1.31°` 无法满足 `HOME_RETURN_ASYNC_SOFT_TOL = 1.0` 的门限，触发超时，退回阻塞的 `sync_send_angles` 兜底。
*   **物理精度限制**：myCobot 280 电机的物理死区和传感器测量噪声导致其在物理直立位置时，实际读取的角度最大偏差通常波动在 $1.1^\circ \sim 1.4^\circ$。如果软到位门限设置成过于严苛的 $1.0^\circ$，机械臂绝不可能触发软到位提前退出。这不仅丧失了异步提速优势，还带来了 2.5s 的纯空等延迟。

##### 2. 改进建议（V2.6 修改清单）
在接下来的 V2.6 修改中，在不修改核心控制逻辑的前提下，微调两项门限参数：
*   **软到位门限放宽**：将 `HOME_RETURN_ASYNC_SOFT_TOL` 从 `1.0` 放宽至 **`1.5`**（或 `2.0`）。直立附近 $\le 1.5^\circ$ 的轴偏差在物理上已完全回正且无任何物理干涉风险。这能让程序在 $0.5\text{s}$ 物理到位时瞬间判定收敛，避免 2.5s 超时。
*   **软超时时间压缩**：将 `HOME_RETURN_ASYNC_TIMEOUT` 从 `2.5` 压缩至 **`1.2s`** 或 **`1.5s`**。若运动 1.2s 仍未能逼近零位，说明发生受阻，无需空等 2.5s，降低异常等待开销。

##### 3. V2.6 落地（2026-07-07，用户批"本轮无需过 Codex 门"，属参数微调）

> 状态: 代码已落地 + py_compile 通过 + mock 边界测试通过。用户裁定本轮常数微调无需 Codex 复核。

Claude 采纳 Gemini 建议但取**保守档**（小步单变量原则），而非 Gemini 给的上限：
```python
HOME_RETURN_ASYNC_SOFT_TOL = 1.5   # 1.0 -> 1.5（不取 2.0；run-17 仍偶发不收敛再升）
HOME_RETURN_ASYNC_TIMEOUT = 1.5    # 2.5 -> 1.5（不取 1.2；1.2s 余量偏小，负载稍慢会误判超时）
```

取值理由：
- `SOFT_TOL=1.5`：run-16 实测残差 1.31°，1.5° 留 0.19° 余量已够通过；HOME 是终点无后续动作，1.5° 物理上完全回正无干涉。不取 2.0 是为留后续上调空间（先 1.5，run-17 仍偶发不收敛再升 2.0），避免一步到位丢掉诊断信号。
- `TIMEOUT=1.5`：run-16 物理到位约 0.5s，1.5s 留 1s 余量；不取 1.2 是因 1.2s 余量偏小，负载/摩擦稍慢时会被误判超时走 sync 兜底反而更慢。run-17 稳了再考虑 1.2。
- 本 `SOFT_TOL` 同时用于方向1 step11 与方向2 phase-B 的 HOME 软到位判定（一致性）。
- 不动 `HOME_RETURN_ASYNC_SPEED`(30)/`SMOOTH_HANDOFF_NEAR_TOL`(5.0)/`HOME_RETURN_ASYNC_POLL`(0.05)；不动安全门（45/40/R_MAX）；sync 兜底仍用 V2.4 验证的 `HOME_RETURN_SPEED=25`（Codex F1，V2.5 已落地）。

##### 4. mock 边界验证

- 残差 1.31°（run-16 实测值）≤ 1.5° → 软到位提前收敛，**无 2.5s 空等、无 sync 兜底**（V2.5 瓶颈消除）✅
- 残差 2.0° > 1.5° → 仍正确回退 sync 兜底（speed=25），**无误收敛**（安全保留）✅
- 固件卡死 → sync 兜底 speed=25（安全失败链路完整）✅

##### 5. run-17 验收建议

1. `python mycobot_pc_tests/teach_replay_pick_return_ready.py --port COM9 --preset my_new_test`
2. 预期第二阶段自检回零 + 第三阶段 step10+11 末尾出现 `[V2.5 方向1/2] 软到位收敛 max_diff=1.31° <= 1.5° 提前退出`，**不再出现软超时 + sync 兜底**。
3. 预期 step10+11 耗时从 run-16 的 5.0s 降至 ~1.5-2.0s（物理到位 ~0.5s + 轮询/读数开销）。
4. 0 轮人工扶正、`delta_xyz` 不回升到 run-11 Run B 的 16.2mm。
5. 若 run-17 仍偶发"软超时未收敛"（残差 >1.5°），按小步原则升 `SOFT_TOL` 1.5→2.0 再测；若 step10+11 仍偏慢但已收敛，再考虑 `TIMEOUT` 1.5→1.2。
6. run-17 日志写入 `trial_run_17_logs.md`，至少含：各 step 耗时、软到位收敛/超时的 mode 与 final max_diff、是否触发 sync 兜底。

##### 6. Run-17 验收结果与 V2.6 成功收敛总结（2026-07-07）

依据 `trial_run_17_logs.md` 实机反馈，Run-17 验收实验取得圆满成功，V2.6 的门限调整完全闭环了提速机制的最后拼图。

*   **第二阶段安全自检回零**：
    启动后检测到实际全轴偏差仅为 $1.31^\circ \le 1.5^\circ$。程序**耗时 0.01s 瞬间判定软到位收敛并提前退出**（mode=async），消除了以往 2.5s 的空等无意义死区，极速完成启动自检。
*   **第三阶段合并过渡回零（Step 10+11）**：
    *   **平滑过渡**：运动中在 $4.92^\circ \le 5^\circ$ 时非阻塞提前下发 HOME 指令，大臂划过 home_ready 时消除了一切刹车顿挫，连贯弧线拉直。
    *   **异步软到位收敛**：运动 1.25s 后检测到最大偏差为 $1.31^\circ$。由于 `SOFT_TOL` 已放宽至 $1.5^\circ$，**程序立即判定“HOME 软到位收敛”并 Break 退出，耗时 1.25s**。完全没有触发 1.5s 软超时与阻塞 sync 兜底！
    *   **回零总时间降幅**：Step 10+11 总耗时从 Run-16（未收敛）的 **5.0s** 暴降至 **3.4s**（包含大摆动+回零的全部物理进程），提速表现非常强劲。
*   **物理精度与安全性**：
    各步骤坐标校验数据依然稳定在 $\approx 10.7\text{mm} \sim 10.9\text{mm}$（Step 5/9）高度可重复区间，回零依然报告 0 轮人工扶正。
*   **结论**：
    V2.6 版本的成功上线，证明了通过放宽软到位判定阈值（1.0° $\rightarrow$ 1.5°）来适配 myCobot 底层电机的物理死区具有决定性意义。异步回零与手势过渡逻辑终于完全走通，PC 端动作链路优化完美收官。

### 14.9 连续多次抓取循环（V2.7，贴近比赛条件）

> 日期: 2026-07-07
> 状态: 代码已落地 + `py_compile` 通过 + 5 项 mock 控制流测试全 PASS。
>   属 main 编排层改动（加循环 + bool 返回值），不碰运动参数/安全门/动作序列。
> 边界: 引入"连续运行"新工况与新的轮间失败模式，建议走一次轻量 Codex Review Packet
>   后再实机验证 run-18。无机械臂运动由本次分析产生。

#### 14.9.1 需求

为贴近实际比赛条件，在确定示教点后能够**连续完成若干次抓取任务，次数由用户通过终端键入**。要求在尽可能少改代码的前提下实现。

#### 14.9.2 循环不变量分析（为何可最小改动）

逐 step 追 `auto_phase_v2` 末态：

| 步骤 | 动作 | 末态 |
| --- | --- | --- |
| 1 | 张开夹爪 | 夹爪开 |
| 2-7 | HOME → pick_hover → pick → 抓 → pick_hover → drop_hover → drop | — |
| 8 | 张开夹爪（放置） | 夹爪开 |
| 9 | drop → drop_hover（短上抬） | — |
| 10+11 | drop_hover → home_ready → HOME | **臂在 HOME，夹爪开** |

**关键不变量**：每一轮结束时臂在 `HOME`、夹爪张开 —— 与 `prepare_phase()` 之后、第一次 `auto_phase_v2()` 之前的状态完全一致（`prepare_phase` 也是 power_on + 回零到 HOME）。因此第 2 轮的 `HOME → pick_hover`（步骤 2）与第 1 轮走同一条路径，不引入任何新轨迹/新风险。run-17 的 step 10+11 软到位残差 `max_diff=1.31°` 是稳态、不跨轮累积（每轮从 HOME 出发再回 HOME）。

**换块确认**：每轮开头现成的 `input("-> 请将正方体放回【抓取点】...")` 提示在每一轮触发，天然承担"把物块从放置点拿回抓取点"的人为确认，无需新增提示。

#### 14.9.3 Claude 修改清单（已落地，共 5 处，均不碰运动/安全逻辑）

1. **文件头 docstring 追加 V2.7 条目**（II/JJ/KK 三条），保持审计链完整。属文档，不影响逻辑。
2. **`auto_phase_v2` docstring 补返回值契约**：`True`=本轮完整跑通（含末段回零，臂在 HOME/夹爪张开）；`False`=中途中止（home_ready 安全门未过 / 回零失败已 `release_all_servos`）。
3. **`validate_return_ready` 未过早退**：`return` → `return False`（此分支未发运动指令，臂仍在 HOME）。
4. **回零失败早退**：`return` → `return False`（舵机已掉电，须通知 main 中止剩余轮次，避免对软臂下发运动指令）。
5. **函数末尾**：加 `return True`（本轮完整跑通，main 可继续下一轮）。
6. **`main()` 把单次调用换成循环**：`prepare_phase` 之后加终端键入次数 N（空回车默认 1，非整数回退 1），`for i in range(N)` 调 `auto_phase_v2`，`False` 即 `break`。N=1 行为与改动前单跑完全一致（向后兼容）。

> 注：修改清单第 1 条为 docstring、第 2-5 条合起来是 auto_phase_v2 的 bool 返回值、第 6 条为 main 循环。对应代码改动实为 5 处 Edit（docstring+函数 4 处+main 1 处），上文按语义点编号。

#### 14.9.4 安全考量 —— 为什么必须加返回值

唯一的新风险：**第 N 轮回零失败时 `auto_phase_v2` 内部会 `mc.release_all_servos()` 让臂变软**。若循环不感知、直接跑下一轮，下一轮的 `checked_sync_angles` 会向已掉电的臂下发 `sync_send_angles`，固件行为未定义（可能异动或报错）—— 真实安全隐患。

故改动 3/4/5 把 `auto_phase_v2` 的隐式 `None` 改成 `bool`，main 在 `False` 时 `break`。这 3 处是**唯一必须做的逻辑改动**，且只是给原有 `return` 加一个语义值，不改任何运动/安全判定。

其余失败路径已经安全：
- **抛异常**（`checked_sync_angles` 超时、夹爪未确认等）：原有 `main` 的 `except Exception` 已做"扶稳 → release"，循环自然终止，无需新增处理。
- **Ctrl+C 急停**：原有 `KeyboardInterrupt` 分支 `stop + release` 不变。
- **单轮内 soft-success 残差**（run-17 step 9 的 `delta_xyz=10.9mm`）：单轮内行为，不影响轮间状态（后续步骤仍走到末段回零把臂拉回 HOME），不累积。

#### 14.9.5 未动项（刻意保留）

- `prepare_phase` 只调用一次：每轮末段已回零，无需每轮重新上电+回零（重跑只多一次冗余 `safe_return_home`）。
- `acquire_points` 只调用一次：示教点/预设确定后全程复用，循环里不重新示教。
- `auto_phase_v2` 内部 `0./0b.` 只读安全门每轮重跑：零运动风险，保留"每轮出发前再确认一次轨迹连续性"的防御性；刻意不拆到循环外以保持函数自包含（拆出会破坏自包含性，不属于最小改动）。
- 所有速度/超时/容差/安全门常数（`SHORT_UP_SPEED`/`ARM_MAX_DIFF_SAFE`/`HOME_READY_TARGET_ARM_MAX`/`R_MAX`/`SOFT_REFINE_*`/`HOME_RETURN_ASYNC_*` 等）完全不动。
- `teach_replay_pick.py` / `teach_and_pick.py` 未改（§0 保护）。

#### 14.9.6 已运行验证

- **静态**：`python -m py_compile mycobot_pc_tests\teach_replay_pick_return_ready.py` → `py_compile OK`。
- **mock 控制流**（复刻 main 循环编排，不连真机）：
  - T1：N=3 且每轮 True → 跑满 3 次，无中止。✅
  - T2：第 2 轮 False → 只调用 2 次，**第 3 轮未被调用**（不下发运动指令），打印"中止于第 2 轮"。✅
  - T3：空回车 → 默认 1 次，与改动前单跑一致（向后兼容）。✅
  - T4：非整数 → 回退默认 1 次。✅
  - T5：N=1 单轮 False → 正确中止。✅
- **实机验证**：待 run-18（建议先 N=2）。

#### 14.9.7 run-18 验收建议

1. `python mycobot_pc_tests/teach_replay_pick_return_ready.py --port COM9 --preset my_new_test`，输入 `2`。
2. 验收门：
   - 2 轮均 0 轮人工扶正，轮间臂停在 HOME，`delta_xyz` 仍落在 run-17 的 10.7~10.9mm 区间。
   - 第 2 轮开头的"请将正方体放回抓取点"提示正常出现（换块确认）。
   - 单轮耗时参考 run-17 约 30~35s（step 9 的 20s 是已知瓶颈，非本改动引入），N 轮约 N×33s 量级。
3. 失败路径回归（可选）：若某轮回零失败，确认循环正确中止且不向软臂下发运动指令（mock T2/T5 已覆盖此分支语义）。
4. run-18 日志写入 `trial_run_18_logs.md`，至少含：每轮各 step 耗时、`delta_xyz`、`safe_return_home` 返回值、轮间 HOME 停留确认、是否触发中止。

#### 14.9.8 Codex 复核门

属 myCobot 工况改动（引入连续运行），建议走一次轻量 Codex Review Packet（用 CLAUDE.md 模板）后再实机。复核重点：

- `auto_phase_v2` 返回值契约是否覆盖所有早退路径（`validate_return_ready` 未过 / 回零失败 / 正常完成）。
- `False` 中止是否避免在舵机掉电状态下继续下发运动指令（核心安全点）。
- 轮间不变量（臂在 HOME / 夹爪张开）是否在所有正常完成路径上成立。
- `prepare_phase` / `acquire_points` 只跑一次是否正确（避免每轮重新示教或重新上电回零）。
- N=1 默认路径是否与改动前单跑行为完全一致（向后兼容）。
- 是否未恢复 `sync_send_coords()`、未放宽任何安全门、未改 `teach_replay_pick.py` / `teach_and_pick.py`。

### 14.10 V2.8 流畅度加强三点分析（run-18 后，待落地）

> 日期: 2026-07-07
> 依据: `trial_run_18_logs.md`（V2.7 N=3 全线跑通，三轮耗时数据齐全）
> 状态: **只做分析，未落地代码**。本节先写入方案，交 Codex 审核，按审核结果完善后再逐项落地。
> 用户已确认两点：①赛场实际流程为"CPU 传入抓取信号→机械臂执行一轮抓放→人补块到 pick 点"；②本轮先只做分析。

#### 14.10.1 关键归因澄清：慢的不是"回零"，是 step 9

run-18 三轮实测耗时：

| 段 | 第1轮 | 第2轮 | 第3轮 | 性质 |
| --- | --- | --- | --- | --- |
| step 5 (pick→pick_hover 带载上行) | 1.5s | 1.4s | 1.4s | ✅ 正常，固件 res==1 严格通过 |
| step 10+11 (**真正的回零**) | 3.5s | 3.5s | 3.4s | ✅ V2.6 已优化到位 |
| **step 9 (drop→drop_hover 带载上行)** | **20.0s** | **19.9s** | **20.0s** | ❌ **真正瓶颈** |

**真正的回零(step 10+11)已经只有 3.4s**，其中物理运动 ~1.2s + 读数/轮询 ~2.2s，已接近物理极限。每轮 33s 里 step 9 占 20s(60%)，这才是要优化的对象。step 9 不是回零，是"放完物块后从 drop 抬回 drop_hover"的带载上行。

**step 9 的 20s 构成（逐行还原）**：
```
15.0s  sync_send_angles(timeout=15s) 等固件 is_in_position 判失败返回 0
 3.0s  _soft_refine 微调 sync_send_angles(timeout=3s) 又等失败
~1.2s  4 次 get_filtered_angles/get_filtered_coords（每次要连续两次稳定读数）
~0.5s  verify_coords_near 再读一次坐标
─────
20.0s  其中物理运动只有 ~1.5s（step 5 同动作只要 1.4s），18.5s 全在等死区+读数
```

**根因**：固件 `is_in_position` 死区让 drop→drop_hover 这段带载上行的残差稳定卡在 **2.1°**（run-18 三轮完全一致：2.1°/2.1°/2.1°），永远过不了固件严格到位判定，`sync_send_angles` 每次都等满 timeout 返回 0。这是固件特性，不是脚本 bug。

#### 14.10.2 第 1 点：哪些安全校验可以去除（放宽安全、加强流畅）

用户明确"机械臂始终有人旁边保护，安全措施可放宽"。逐项审计 step 9 链路，分三类：

##### A 类：可去除/压缩（纯固件死区等待，人保护下零风险）

| 校验/等待 | 位置 | 耗时 | 去除方案 | 风险 |
| --- | --- | --- | --- | --- |
| step 9 的 `sync_send_angles` timeout 15s 等失败 | `checked_short_angles` sync 调用 | 15s | step 9 改用**非阻塞 `send_angles` + Python 软到位循环**（复用 V2.5 已验证的 `_send_home_async` 思路），残差≤3° 即软通过退出 | 人保护下零风险；run-18 实测残差稳态 2.1°<3°，物理早已到位 |
| `_soft_refine` 微调 3s 等失败 | `_soft_refine` | 3s | step 9 跳过微调（残差 2.1° 是固件死区，微调三轮都没收敛，纯浪费） | 零风险；微调本就是 best-effort |
| `SOFT_SETTLE_SECONDS=0.5s` 软到位前等待 | 常量 | 0.5s×多次 | 改非阻塞软到位循环后，运动中读数不需要 settle | 零风险 |

**预期收益：step 9 从 20s → ~2s（物理 1.5s + 轮询 0.5s），每轮省 18s，N 轮省 18N 秒。** 这是最大头。

##### B 类：可压缩（读数次数，人保护下可减少）

| 校验 | 位置 | 耗时 | 压缩方案 | 风险 |
| --- | --- | --- | --- | --- |
| `verify_coords_near` 每步都读坐标 | step 2/3/5/6/7/9/10 后，7 次 ×~0.6s ≈ 4s | 预设回放时点位固定，可只在 step 5/9（带载上行软通过后）校验，长距离 step 2/6 不校验 | 低；长距离 res==1 严格通过时坐标必然准，校验冗余 |
| `get_filtered_angles/coords` 要"连续两次稳定" | `get_filtered_*` | 每次 ~0.6s | 软到位循环用 `get_angles_once`（单次，已有），诊断读数也改单次 | 低；稳定性门是为示教点记录设计的，回放期臂静止时单次读数足够 |
| 夹爪开环 `GRIPPER_TIMEOUT=2.5s`×3次 | step 1/4/8 | 7.5s | 改 1.0-1.5s（夹爪物理开合 <1s） | 低；人眼可确认夹爪状态 |

**预期收益：再省 ~6-8s/轮。**

##### C 类：不可去除（去掉会真撞机或破坏状态机）

| 校验 | 位置 | 为什么不能去 |
| --- | --- | --- |
| `validate_return_ready`(home_ready arm_max_diff≤45) | `validate_return_ready` | 回零安全门，去掉则 home_ready 示教错时直接硬回零撞机 |
| `validate_short_angle_pair`/`validate_return_angle_pair` | 同名函数 | 拦截 IK 分支跳变，去掉则回放大摆动撞物 |
| `is_safe_coord`/R_MAX 物理臂展硬限 | `is_safe_coord` | 超臂展会损坏电机 |
| `arm_max_diff≤45` 自动回零门 | `safe_return_home` | 大偏差硬回零会撞，必须保留人工扶正分支 |
| 软通过容差本身(3°/25mm) | `SOFT_*_SUCCESS_TOL` | 容差是物理可达判定；放宽到无限大=不校验=撞机 |

**结论**：C 类全保留。真正能省的是 A+B 类的**固件死区等待和冗余读数**，不是安全门。

#### 14.10.3 第 2 点：输入 N 后全自动运行（去掉每轮 Enter）

**可行，零风险。** 用户已确认赛场流程为"CPU 信号→单轮抓放→人补块到 pick 点"，即 pick 点每轮都有物块，去掉 Enter 不会抓空气。

- 直接删除 `auto_phase_v2` 中 `input("-> 请将正方体放回【抓取点】...")`，N 轮连续跑。
- 赛场模式与去掉 Enter 后的 PC 运行模式一致。
- **FPGA 迁移**：✅ 必须迁移。板上 CPU 本来就没有 Enter，靠传感器检测物块到位。PC 端去掉 Enter 走的就是板上 CPU 的运行模式。注意：N 轮循环本身不上板，上板的是单轮抓放状态机。

#### 14.10.4 第 3 点：自动导出终端日志

**可行，零风险，纯 PC 工具功能。** 推荐 `Tee` 类方案：main 开头装一个双写 stdout（终端 + UTF-8 文件），自动写到 `audit_logs/auto_run_<时间戳>.log`，不动任何 print。改动量 ~15 行，只在 main 头部。

**FPGA 迁移**：⚪ PC 专用，不迁移。板上 CPU 用 UART 日志，这个功能不上板，但不污染状态机，无影响。

#### 14.10.5 FPGA 迁移兼容性总评

| 加强点 | FPGA 迁移兼容性 | 说明 |
| --- | --- | --- |
| 1. 去除 step 9 固件死区等待 | ✅ 正向迁移 | 板上 CPU 同样会遇到固件 is_in_position 死区。PC 端验证"非阻塞 send_angles + 软到位循环"可行，直接指导板上 CPU 用同样状态机（非阻塞下发 + 轮询关节角软到位），而不是等固件 sync 接口。是有价值的迁移验证。 |
| 2. 去掉 Enter 全自动 | ✅ 必须迁移 | 板上 CPU 本来就没有 Enter，靠传感器检测物块。PC 端去掉 Enter 走方案 2B，正是板上 CPU 运行模式。 |
| 3. 日志导出 | ⚪ PC 专用，不迁移 | 板上 CPU 用 UART 日志，Tee 功能不上板。纯工具功能，不污染状态机，迁移时整个脚本不上板，无影响。 |

**唯一迁移风险**：点 1 把 `sync_send_angles` 改非阻塞软到位循环时，板上 CPU 的 UART 协议要自己实现这个状态机（非阻塞下发 + 读关节角寄存器 + 软到位判定），比直接调固件 sync 接口复杂。但这是**正确方向**——决赛主线本就是板上 CPU 自主控制，不能依赖 PC 端 pymycobot 的 sync 接口。PC 端先验证软到位循环的容差和超时参数，板上 CPU 直接复用这些参数。

#### 14.10.6 综合可行性与落地分批

| 点 | 可行性 | 改动量 | 风险 | 迁移价值 | 建议批次 |
| --- | --- | --- | --- | --- | --- |
| 1. step 9 异步化 | ✅ 收益巨大(20s→2s) | 中（复用 V2.5 异步思路） | 低（人保护+残差稳态） | ✅ 正向 | 第二批（过 Codex 门） |
| 2. 去 Enter 全自动 | ✅ 零风险 | 小 | 低 | ✅ 必须 | 第一批 |
| 3. 日志导出 | ✅ 零风险 | 小（~15 行） | 零 | ⚪ PC 专用 | 第一批 |

**落地分批**：
- 第一批：点 2（去 Enter）+ 点 3（日志导出），低风险，编排/工具层，不必过 Codex 门。
- 第二批：点 1（step 9 异步化），动作链路改动，按 AGENTS.md Codex Gate 走 Codex 复核后再实机验证 run-19。

#### 14.10.7 待 Codex 审核问题

本节分析交 Codex 自动审核，希望 Codex 判断：
1. 点 1 把 step 9 的 `sync_send_angles` 改非阻塞 `send_angles` + 软到位循环（残差≤3° 退出），在 run-18 实测残差稳态 2.1° 的前提下，是否安全可行；软通过容差 3°/25mm 是否需要随非阻塞化调整。
2. 点 1 的 B 类压缩（verify_coords_near 从 7 次减到 2 次、夹爪 timeout 2.5s→1.2s）是否会破坏现有安全门或失败熔断语义。
3. 点 2 去掉 `auto_phase_v2` 中的"请将正方体放回抓取点"Enter，在用户已确认"人补块到 pick 点"的赛场流程下，是否还有需要保留的确认语义（如首轮轨迹确认）。
4. 点 3 的 Tee 双写 stdout 方案是否会干扰现有 `input()`/Ctrl+C/异常捕获路径。
5. 三点合并落地时，是否存在跨点相互作用风险（如点 1 异步化后点 2 的轮间状态机是否仍成立）。
6. 是否同意第一批复用点 2+3、第二批复用点 1 走 Codex 门的分批策略。

#### 14.10.8 V2.8 落地记录（2026-07-07，三点全部应用）

> 状态: 代码已落地 + `py_compile` 通过 + 9 项 mock 控制流测试全 PASS。
> 用户确认赛场流程（CPU 信号→单轮抓放→人补块到 pick 点）+ 机械臂始终有人保护后，
> 三点一并落地（不分批），在分支 `work/mycobot-v2.8-fluency` 上演进。
> Codex 自动审核子代理在审核中途被用户终止（用户决定基于现有分析直接落地），
> 故未采纳 Codex 审核结论；本节为 Claude 自审 + mock 验证结果，实机验证前建议补 Codex 复核。

##### 落地改动清单

**点 2（去 Enter 全自动，LL）**：
- `auto_phase_v2` 内 `input("-> 请将正方体放回【抓取点】...")` 移除，N 轮全自动。
- 一次性轨迹/物块确认移到 `main` 循环开始前（`确认无误后按 Enter 开始连续抓取`），只问一次。
- 保留回零失败扶稳 `input()`（安全路径，不去除）；保留急停提示 print（不阻塞）。

**点 3（日志导出，MM）**：
- 新增 `Tee` 类（双写 stdout：终端 + UTF-8 文件），`__getattr__`/`isatty` 透传原 stdout 保兼容。
- 新增 `AUTO_LOGS_DIR` 常量 + `--no-log` CLI 开关（默认启用日志）。
- `main` 开头装 `sys.stdout = Tee(auto_run_<时间戳>.log)`，`try/finally` 确保异常/Ctrl+C/正常退出时恢复 stdout + 关闭文件落盘。
- 不动任何 `print`；`input()` 提示文本走 stdout 也被记录（日志可复盘交互）。

**点 1（step 9 异步化，NN + B 类 OO）**：
- 新增 `checked_short_angles_async()` 函数：非阻塞 `send_angles` + Python 软到位循环（`get_angles_once` 单次读数，复用 V2.5 关键设计点），残差≤`ASYNC_SHORT_SOFT_TOL`(3°) 即软通过退出；软超时走阻塞 `sync_send_angles` 收尾兜底，`res!=1` 抛异常熔断（保留 V2.4 安全失败语义）。
- 新增参数：`ASYNC_SHORT_ENABLE`(总开关)/`ASYNC_SHORT_SOFT_TOL=3.0`/`ASYNC_SHORT_TIMEOUT=4.0`/`ASYNC_SHORT_POLL=0.05`。
- `auto_phase_v2` step 9 改调 `checked_short_angles_async`（`ASYNC_SHORT_ENABLE=False` 回退原 `checked_short_angles`）。
- B 类：`GRIPPER_TIMEOUT 2.5→1.2s`；新增 `SKIP_COORD_VERIFY_ON_STRICT_PASS=True`，step 2/3/6/7（长距离+短下探）严格通过时跳过 `verify_coords_near`，step 5/9（带载上行软通过）保留无条件复核。
- 不动：软通过容差 3°/25mm、所有安全门（`validate_return_ready`/`validate_short_angle_pair`/`validate_return_angle_pair`/`is_safe_coord`/R_MAX/`arm_max_diff≤45`）、step 10+11 回零路径（V2.6 已 3.4s）、step 3/5/7 仍用阻塞 `checked_short_angles`、`teach_replay_pick.py`/`teach_and_pick.py`。

##### 预期收益

| 项 | V2.7 (run-18) | V2.8 预期 | 说明 |
| --- | --- | --- | --- |
| step 9 耗时 | 20.0s | ~2s | 去除 15s sync timeout + 3s 微调 timeout 死区等待 |
| 夹爪等待/轮 | 7.5s (3×2.5s) | 3.6s (3×1.2s) | GRIPPER_TIMEOUT 2.5→1.2 |
| verify_coords_near/轮 | ~7次×0.6s≈4.2s | ~2次×0.6s≈1.2s | B类跳过长距离+短下探 |
| 单轮总耗时 | ~33s | ~13-15s | 三项合计省 ~18-20s |
| N 轮交互 | 每轮 Enter | 一次性 Enter | 点2 |

##### 已运行验证

- **静态**：`python -m py_compile mycobot_pc_tests\teach_replay_pick_return_ready.py` → 通过（点 2+3 后、点 1 后、全部完成后三阶段均通过）。
- **mock 控制流**（9 项全 PASS）：
  - T1: Tee 双写终端+文件内容一致，文件 UTF-8 落盘。
  - T2: `auto_phase_v2` 去除每轮换块 Enter，保留回零失败扶稳 input。
  - T3: main 循环前一次性确认在 for 循环之前。
  - T4: 异步软通过分支（残差 5°→2°）正确退出，不触发 sync 兜底。
  - T5: 软超时（残差恒 5°）正确走 sync 收尾兜底。
  - T6: sync 收尾 `res!=1` 抛 RuntimeError 熔断。
  - T7: V2.8 参数默认值正确（ENABLE=True/SKIP=True/GRIPPER=1.2/TOL=3.0）。
  - T8: 目标角度合法性校验（<6 元素）保留。
  - T9: B 类跳过保护 4 处（step2/3/6/7），step5/9 保留无条件复核。

##### 安全守住

- **不抛异常防掉电**：异步软超时不抛异常（防臂在运动路径掉电），走 sync 收尾；只有 sync `res!=1` 才抛异常熔断（V2.4 语义）。
- **安全门全保留**：C 类（`validate_return_ready`/IK 分支校验/R_MAX/`arm_max_diff≤45`）一律不动。
- **软通过容差不放宽**：3°/25mm 是物理可达判定，非安全门，但不放宽以保持与 V2.7 一致的可重复性基准。
- **回零失败扶稳 input 保留**：回零失败时 `release_all_servos` 前仍提示扶稳，不因"全自动"去除。
- **Tee 不干扰控制流**：`isatty`/`__getattr__` 透传原 stdout，`input()`/Ctrl+C/异常捕获路径不变（mock T1 + 编译验证）。

##### 未验证项与风险

- **实机未验证**：所有预期耗时（step 9 20s→2s 等）为基于 run-18 数据的推算，需 run-19 实机确认。
- **异步软到位循环的固件响应**：非阻塞 `send_angles` 在 drop→drop_hover 带载上行路径的固件行为未实测（V2.5 只在 HOME 回零路径验证过）。run-19 须人眼盯防，异常即 Ctrl+C + 设 `ASYNC_SHORT_ENABLE=False` 回退。
- **SKIP_COORD_VERIFY_ON_STRICT_PASS**：跳过长距离/短下探的坐标校验，依赖"res==1 严格通过时坐标必然准"的假设。run-19 须对比 V2.7 的 delta_xyz 数据确认无回归。
- **Codex 复核未完成**：用户终止了 Codex 审核子代理，本节为 Claude 自审。按 AGENTS.md Codex Gate，点 1 属动作链路改动，**实机验证前建议补一次 Codex Review Packet 复核**。

##### run-19 验收建议

1. `python mycobot_pc_tests/teach_replay_pick_return_ready.py --port COM10 --preset my_new_test`
2. 输入 N=2（先验证 2 轮全自动），确认一次性 Enter 后不再有换块 Enter 提示。
3. 验收门：
   - step 9 耗时从 20s 降至 ~2-4s（异步软通过）；若软超时走 sync 兜底，记录 sync res 与 final max_err。
   - 单轮总耗时从 ~33s 降至 ~13-15s。
   - delta_xyz 不回升（pick_hover 仍 ~6.6mm，drop_hover 软通过后 ~10.9mm）。
   - 2 轮均 0 轮人工扶正，轮间臂停在 HOME。
   - 日志自动写入 `audit_logs/auto_run_<时间戳>.log`，内容与终端一致（UTF-8 无乱码）。
4. 回退验证：若 step 9 异步路径异常，设 `ASYNC_SHORT_ENABLE=False` 重跑确认回退 V2.7 行为。
5. run-19 日志写入 `trial_run_19_logs.md`，至少含：各 step 耗时、step 9 异步 mode（软通过/软超时+sync）、delta_xyz、`safe_return_home` 返回值、自动日志路径。

##### Codex 复核门（实机前建议补）

复核重点：
- `checked_short_angles_async` 软超时 sync 收尾 + res 检查是否保留 V2.4 安全失败语义。
- `get_angles_once` 用于异步轮询是否合理（V2.5 已验证同款设计点）。
- `SKIP_COORD_VERIFY_ON_STRICT_PASS` 跳过长距离/短下探坐标校验是否安全（res==1 严格通过时坐标必然准的假设是否成立）。
- `Tee` 的 `isatty`/`__getattr__` 透传是否干扰 `input()`/Ctrl+C/异常捕获。
- 点 1 异步化后点 2 轮间状态机是否仍成立（每轮末态臂在 HOME/夹爪开不变量）。
- `ASYNC_SHORT_TIMEOUT=4.0s` 是否合理（物理 ~1.5s + 余量）。
- 是否未恢复 `sync_send_coords()`、未放宽安全门、未改基线脚本。

#### 14.10.9 run-19 复核与 V2.9 改进方案（step 5 异步化，暂不落地）

> 日期: 2026-07-07
> 依据: `auto_run_20260707_210944.log`（V2.8 N=3 自动日志，Tee 双写产出）
> 状态: V2.8 三点全部实机验证。回零变快确认；发现第 2 轮 step 5 偶发 20s 停顿。
>   本节为分析与改进方案，**暂不落地代码**。

##### 1. run-19 关键证据（三轮耗时）

| 段 | 第1轮 | 第2轮 | 第3轮 | V2.7(run-18) | 判断 |
| --- | --- | --- | --- | --- | --- |
| 第二阶段自检回零 | 0.02s | — | — | 0.59s | ✅ V2.6 异步生效 |
| step 5 (pick→pick_hover 带载上行) | 1.4s | **20.0s** ⚠️ | 1.5s | 1.4-1.5s | ❌ 第2轮偶发死区 |
| step 9 (drop→drop_hover 带载上行) | 3.4s | 1.3s | 3.4s | 20.0s | ✅ V2.8 异步生效 |
| step 10+11 (末段回零) | 3.5s | 3.6s | 3.5s | 3.4s | ✅ V2.6 保持稳定 |
| 单轮总计 | ~13s | ~31s ⚠️ | ~13s | ~33s | 第1/3轮收益兑现 |

**V2.8 收益兑现**：第 1/3 轮单轮 ~13s（vs V2.7 ~33s，省 ~20s），step 9 从 20s 降到 1.3-3.4s，回零变快用户已观察到。

**残留问题**：第 2 轮 step 5 偶发 20s 停顿，单轮回到 ~31s。

##### 2. "第二次抬升后停顿"根因（用户观察对应第 2 轮 step 5）

日志铁证（auto_run 行 148-159）：
```
5. 短距离关节抬起回 pick_hover...
  -> 关节回放到 pick_hover: ... (speed=16, timeout=15s)
  -> [诊断] pick_hover: sync_send_angles 返回 0       ← 等满 15s
  -> [诊断] pick_hover: 与目标关节角差值 max=2.0°      ← 残差 2.0° 卡死
  -> [V2.2 微调] ... 低速二次微调 (speed=8, timeout=3s)
  -> [V2.2 微调] pick_hover: sync_send_angles 返回 0   ← 又等 3s
  -> [软通过] pick_hover: 固件返回 0...
  -> [V2.2 耗时] step 5: 20.0s                          ← 停顿来源
```

**根因链**：
1. step 5 是 `pick→pick_hover` 带载上行，与 step 9 同构（都是带载短距离上行）。
2. V2.8 只把 step 9 异步化，step 5 仍是阻塞 `checked_short_angles`（§14.10.8 落地清单显式"step 3/5/7 仍用阻塞版"）。
3. run-18 三轮 step 5 都 res==1 严格通过（残差低于固件阈值），V2.8 据此判断"step 5 无需改"——这是基于 run-18 单次样本的判断。
4. run-19 第 2 轮 step 5 偶发残差 2.0° 越过固件严格阈值 → `sync_send_angles` 返回 0 → 等满 15s + 3s 微调 = 20s 停顿。
5. 这是**固件 is_in_position 死区的偶发触发**，非系统性差异（第 1/3 轮 step 5 仍 1.4/1.5s 正常）。固件死区阈值在 ~2° 附近波动，带载上行残差 2.0-2.1° 处于"有时过有时不过"的边界区。

**用户观察的"抬升后停顿"= step 5 的 20s 固件死区等待**，不是回零问题（回零已 3.5s），也不是 step 9 问题（step 9 已被 V2.8 修好，三轮 1.3-3.4s）。

##### 3. V2.8 判断偏差的反思

V2.8 §14.10.8 落地清单写"step 5 已 res==1 严格通过无需改"——这个判断基于 run-18 三轮 step 5 都返回 1。但 run-18 是单次 N=3 样本，固件死区偶发触发需要更大样本才能暴露。run-19 第 2 轮即暴露：step 5 与 step 9 同构，**任何带载短距离上行都有偶发死区风险**，不能因为某几次 res==1 就判定"无需改"。

教训：固件 is_in_position 死区是**概率性**的，残差在 ~2° 边界区时 res 返回 0/1 不确定。对同构动作（带载上行）应统一用异步软到位，不能因单次样本的 res==1 就保留阻塞路径。

##### 4. V2.9 改进方案（暂不落地）

**核心改动：step 5 也改用 `checked_short_angles_async`，与 step 9 统一。**

改动点：
1. `auto_phase_v2` step 5 的 `checked_short_angles(...)` 改为 `checked_short_angles_async(...)`（复用 V2.8 已验证的异步函数，`ASYNC_SHORT_ENABLE` 总开关同时管控 step 5/9）。
2. 保留 step 5 后的 `verify_coords_near`（带载上行软通过需复核坐标，与 step 9 一致）。
3. 不动 step 3/7（下行，重力辅助，固件收敛快，run-19 三轮均 res==1，保留阻塞版；若日后偶发死区再异步化）。
4. 不动其他参数/安全门/回零路径。

**预期收益**：step 5 偶发死区从 20s 降到 ~2-3s（异步软到位循环残差≤3° 退出，run-19 第 2 轮残差 2.0°<3° 必然软通过）。第 2 轮单轮从 ~31s 降到 ~13s，与第 1/3 轮一致。三轮耗时极差从 ~18s 收敛到 ~1s 内。

**风险**：
- step 5 异步化与 step 9 同款，V2.8 已在 step 9 验证非阻塞 `send_angles` 在带载上行路径可行（run-19 三轮 step 9 软通过/收敛均正常），风险已降低。
- 仍属动作链路改动，按 Codex Gate 建议走 Codex 复核后再实机验证 run-20。

##### 5. 其他可观察项（非问题，记录留档）

- **step 9 三轮耗时波动 1.3-3.4s**：异步软到位循环的轮询首次命中时机有波动（残差 2.11° 需要几次轮询才读到收敛读数），属正常。第 2 轮 1.3s 是轮询首次即命中，第 1/3 轮 3.4s 是轮询几次后命中。物理运动 ~1.5s，轮询开销 ~0-1.9s，均可接受。
- **delta_xyz 稳定**：pick_hover 三轮 10.7/11.0/10.7mm，drop_hover 三轮 10.9/10.9/10.9mm，与 V2.7 一致，无回归。
- **0 轮人工扶正**：三轮均 auto，回零安全门稳定。
- **日志自动导出**：Tee 双写生效，auto_run_20260707_210944.log 内容完整、UTF-8 无乱码，点 3 验证通过。
- **全自动运行**：一次性 Enter 后 N 轮无换块 Enter 提示，点 2 验证通过。

##### 6. run-20 验收建议（V2.9 落地后）

1. `python mycobot_pc_tests/teach_replay_pick_return_ready.py --port COM10 --preset my_new_test`，输入 N=3。
2. 验收门：
   - step 5 三轮耗时均 ~1-3s（异步软通过），**不再出现 20s 停顿**。
   - step 9 三轮耗时 ~1-3s（保持 V2.8 收益）。
   - 单轮总耗时三轮均 ~13s，极差 <2s。
   - delta_xyz 无回归（pick_hover ~10.7mm，drop_hover ~10.9mm）。
   - 0 轮人工扶正，日志自动落盘。
3. 回退验证：若 step 5 异步路径异常，设 `ASYNC_SHORT_ENABLE=False` 重跑确认回退 V2.7 行为（step 5/9 都回阻塞）。
4. run-20 日志由 Tee 自动产出 `auto_run_<时间戳>.log`，手动另存为 `trial_run_20_logs.md` 摘录关键指标。

##### 7. 待 Codex 审核问题（V2.9）

1. step 5 改 `checked_short_angles_async` 是否安全可行（与 step 9 同构，V2.8 已在 step 9 验证）。
2. 是否应同时把 step 3/7（下行）也异步化预防偶发死区，还是保留阻塞版（run-19 未触发）。
3. `ASYNC_SHORT_ENABLE` 总开关同时管控 step 5/9 是否合理，是否需要分开关。
4. step 5 异步化后，带载上行残差 2.0° 软通过（≤3°）是否会引入抓取/放置精度风险（run-19 delta_xyz 无回归，但需更大样本确认）。
