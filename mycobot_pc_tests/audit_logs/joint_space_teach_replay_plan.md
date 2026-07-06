# 关节角示教回放改造执行方案

> 生成日期: 2026-07-05
> 目标脚本: `mycobot_pc_tests/teach_and_pick.py`
> 用途边界: PC 端交互调试、定点抓取验证、后续板端控制策略参考
> 不进入正式闭环: 本脚本仍属于 `pymycobot` PC 端联调归档，不作为决赛正式识别/控制闭环依赖。

---

## 1. 背景结论

前两轮实机试运行暴露出两个关键问题：

1. 直线插补 `sync_send_coords(..., mode=1)` 在远伸姿态或直立零位附近容易触发 IK 奇异、限位或超时。
2. 释放舵机、手动拖拽、重新通电后，`get_coords()` 可能短时间出现异常读数，例如 `Z=425.4`。

因此，不建议继续以“自动计算 `Z+60` 悬停点 + 大量坐标直线运动”为主线。更稳的 PC 端测试路线是：

```text
手动示教关节角 + 记录空间坐标校验值
-> 运行时优先关节空间回放
-> 到位后用空间坐标做一致性校验
-> 仅在短距离下探/抬起阶段谨慎使用直线模式
```

这不是严格意义上的“正运动学自动规划”，而是 **关节角示教回放 + 空间坐标一致性校验**。其优势是绕开大部分 IK 轨迹规划失败；代价是物块位置必须与示教时一致，且关节回放不保证末端走空间直线。

---

## 2. 对 Gemini 原方案的修正意见

`implementation_plan.md` 中提出的两个方向有价值，但不能原样执行。

### 2.1 可采纳部分

- 长距离过渡动作从 `mode=1` 改为更稳定的关节空间运动，方向正确。
- 增加 `get_coords()` 滤波，方向正确。
- `sync_send_coords()` 返回值必须检查，失败必须熔断，方向正确。

### 2.2 必须修正部分

- 不要把所有动作统一改为 `mode=0`。
  - 靠近桌面或物体的短距离下探/抬起，如果用关节插补，末端可能走弧线，反而增加碰撞风险。
  - 稳妥方案是“长距离用关节角回放，短距离可保留直线，若直线仍失败再退回关节角回放”。

- `get_filtered_coords()` 不能在多次异常后返回最后一次原始读数。
  - 多次读取均异常时必须返回 `None` 或抛异常。
  - 上层必须拒绝继续运动，不能用异常坐标兜底。

- 坐标滤波不能只看 Z。
  - 必须检查 6 维均为数值、NaN/Inf、半径 R、Z 上下限。
  - 建议增加连续读数稳定性判断。

---

## 3. 改造目标

修改 `teach_and_pick.py`，让它支持完整示教点：

- `pick_hover`: 抓取悬停点
- `pick`: 抓取下探点
- `drop_hover`: 放置悬停点
- `drop`: 放置下探点

每个点同时记录：

- `angles`: `mc.get_angles()` 返回的 6 关节角
- `coords`: `get_filtered_coords(mc)` 返回的 6 维空间坐标

运行时优先使用 `angles` 回放，并用 `coords` 做到位一致性校验。

---

## 4. 推荐动作序列

### 4.1 示教阶段

从原来的“示教抓取点、释放点”改为四点示教：

1. 释放舵机前提示用户扶稳机械臂。
2. `release_all_servos()`。
3. 依次记录：
   - 抓取悬停点 `pick_hover`
   - 抓取下探点 `pick`
   - 放置悬停点 `drop_hover`
   - 放置下探点 `drop`
4. 每个点记录时执行：
   - `angles = get_filtered_angles(mc)`
   - `coords = get_filtered_coords(mc)`
   - `is_safe_coord(coords, is_hover=...)`
   - 打印完整 `angles` 和 `coords`
   - 要求用户二次确认保存

不要再自动用 `pick_coords[2] += 60` 推导悬停点。悬停点应由用户手工示教，避免 IK 生成不可达或奇异点。

### 4.2 回零/准备阶段

示教完成后：

1. `power_on()`。
2. 等待 1 秒。
3. 调用 `safe_return_home(mc)`。
4. 若当前姿态距离零位过大：
   - **必须用 `get_filtered_angles(mc)` 读取关节角，不能用 raw `mc.get_angles()`**。raw 读数在通电后可能含 NaN/异常值，会使 `max_diff` 变成 NaN，`NaN > 45.0` 为 False，从而跳过保护拉升、直接从未知姿态关节回零——这正是 §1 记录的失效模式。
   - 对 `max_diff` 的每个分量做有限值校验（`math.isfinite`），非有限值或读不到稳定角度时拒绝回零。
   - 先尝试读取稳定坐标；读不到稳定坐标则拒绝回零。
   - 若执行保护拉升，长距离保护动作使用 `mode=0`（**关节空间插值，末端走弧线，不是垂直抬升**）或直接要求人工扶正，不再强制 `mode=1`。用户必须确认弧线范围内无物块/障碍。
   - 所有同步动作检查返回值。
5. **回零失败时不要直接 `release_all_servos()`**：保护拉升可能已部分抬高臂，阻尼释放会让抬高/带载的臂下沉摆动撞物。必须先提示用户"用手扶稳后按 Enter"再释放。

### 4.3 自动阶段

推荐顺序：

```text
张开夹爪
-> 关节角回放到 pick_hover
-> 校验当前 coords 接近 pick_hover.coords
-> 到 pick 点：优先短距离 mode=1；失败则停止，不自动 fallback
-> 闭合夹爪
-> 回 pick_hover：优先短距离 mode=1；失败则停止
-> 关节角回放到 drop_hover
-> 校验当前 coords 接近 drop_hover.coords
-> 到 drop 点：优先短距离 mode=1；失败则停止
-> 张开夹爪
-> 回 drop_hover：优先短距离 mode=1；失败则停止
-> safe_return_home()
```

注意：不要在实机测试时自动从失败的 `mode=1` 下探切到 `mode=0`，因为 fallback 路径可能无法预测。失败后应停止，让用户重新示教或调整点位。

---

## 5. 函数级修改方案

### 5.1 新增点位数据结构

使用普通字典即可，避免引入额外依赖：

```python
def make_teach_point(name, angles, coords, is_hover):
    return {
        "name": name,
        "angles": angles,
        "coords": coords,
        "is_hover": is_hover,
    }
```

### 5.2 新增稳定角度读取

```python
def get_filtered_angles(mc, retries=5):
    last_valid = None
    for _ in range(retries):
        try:
            angles = mc.get_angles()
        except Exception as e:
            print(f"【警告】get_angles 抛异常: {type(e).__name__}: {e}，重试中...")
            time.sleep(0.1)
            continue
        if isinstance(angles, list) and len(angles) >= 6:
            vals = list(angles[:6])
            if all(isinstance(v, (int, float)) and math.isfinite(v) for v in vals):
                if all(-180.0 <= v <= 180.0 for v in vals):
                    last_valid = vals
                    break
        time.sleep(0.1)
    return last_valid
```

若返回 `None`，上层必须拒绝保存示教点。

注意：`except` 不能 bare 吞异常——否则 USB 断连、超时、权限错误等持续异常与"无合法读数"不可区分，用户无法针对性修复。必须打印异常类型和消息。

注意：本实现返回首个合法读数（`break`），未做跨读数稳定性校验。这与 §5.3 的 coords 双读稳定性不对称——若实机出现"通电后 in-range 但错误的 angles 读数"，应在此增加连续两次读数差值校验（`ANG_STABLE_TOL`，单位度）。当前实现符合本方案字面规范，留作后续改进。

### 5.3 新增稳定坐标读取

不要返回最后一次异常值：

```python
def get_filtered_coords(mc, retries=6, stable_tol=8.0):
    valid = []
    for _ in range(retries):
        try:
            coords = mc.get_coords()
        except Exception as e:
            print(f"【警告】get_coords 抛异常: {type(e).__name__}: {e}，重试中...")
            time.sleep(0.15)
            continue
        if is_safe_coord(coords):
            vals = list(coords[:6])
            valid.append(vals)
            if len(valid) >= 2:
                prev = valid[-2]
                cur = valid[-1]
                delta = max(abs(cur[i] - prev[i]) for i in range(3))
                if delta <= stable_tol:
                    return cur
        time.sleep(0.15)
    return None
```

若某些合法工作点超出当前 `is_safe_coord()` 的固定阈值，应先调整阈值常量，而不是绕过过滤。

注意：稳定性仅比较末两次合法读数；**稳定但错误的在界内读数**（如持续 Z 偏移 30mm，两次差 <8mm 且过 `is_safe_coord`）会被当作真值返回。`is_safe_coord` 能捕获 §1 的 Z=425.4 越界，但无法防御"稳定且在界内的错误读数"。这是本方案的残留风险，靠用户示教时目视确认姿态来兜底。

### 5.4 新增示教点采集函数

```python
def record_teach_point(mc, name, is_hover):
    input(f"-> 请手动拖动到【{name}】，保持稳定后按 Enter 读取...")
    angles = get_filtered_angles(mc)
    coords = get_filtered_coords(mc)
    if angles is None:
        raise RuntimeError(f"{name}: 无法读取稳定关节角")
    if coords is None:
        raise RuntimeError(f"{name}: 无法读取稳定空间坐标")
    if not is_safe_coord(coords, is_hover=is_hover):
        raise RuntimeError(f"{name}: 坐标不在安全范围内: {coords}")

    print(f"{name} angles = {angles}")
    print(f"{name} coords  = {coords}")
    ans = input("确认保存该示教点吗？(y/n): ")
    if ans.lower() != "y":
        raise RuntimeError(f"{name}: 用户取消保存")
    return make_teach_point(name, angles, coords, is_hover)
```

### 5.5 新增关节角同步回放

```python
def checked_sync_angles(mc, angles, speed, timeout, label):
    print(f"  -> 关节回放到 {label}: {angles}")
    res = mc.sync_send_angles(angles, speed, timeout=timeout)
    if res != 1:
        raise RuntimeError(f"关节回放超时或未到位: {label}")
    return True
```

### 5.6 新增空间一致性校验

关节角回放后再读当前坐标，只做校验，不用于继续规划：

```python
def verify_coords_near(mc, expected, label, xyz_tol=25.0):
    actual = get_filtered_coords(mc)
    if actual is None:
        raise RuntimeError(f"{label}: 无法读取当前坐标用于校验")
    delta = max(abs(actual[i] - expected[i]) for i in range(3))
    print(f"  -> {label} 坐标校验 delta_xyz={delta:.1f} mm")
    if delta > xyz_tol:
        raise RuntimeError(f"{label}: 当前坐标偏差过大，expected={expected}, actual={actual}")
    return True
```

### 5.7 保留短距离直线动作封装

```python
def checked_short_linear(mc, target_coords, speed, timeout, label):
    if not is_safe_coord(target_coords):
        raise RuntimeError(f"{label}: 目标坐标不安全: {target_coords}")
    print(f"  -> 短距离直线到 {label}: {target_coords}")
    res = mc.sync_send_coords(target_coords, speed, 1, timeout=timeout)
    if res != 1:
        raise RuntimeError(f"{label}: 短距离直线动作超时或失败")
    return True
```

短距离直线只用于 `hover <-> pick/drop`，不得用于零位到悬停点、悬停点之间、回零保护等长距离过渡。

---

## 6. main 流程改造步骤

### 6.1 替换示教点采集

原流程：

```python
pick_coords = mc.get_coords()
drop_coords = mc.get_coords()
pick_hover = pick_coords.copy()
pick_hover[2] += 60.0
drop_hover = drop_coords.copy()
drop_hover[2] += 60.0
```

替换为：

```python
pick_hover = record_teach_point(mc, "抓取悬停点 pick_hover", True)
pick = record_teach_point(mc, "抓取下探点 pick", False)
drop_hover = record_teach_point(mc, "放置悬停点 drop_hover", True)
drop = record_teach_point(mc, "放置下探点 drop", False)
```

### 6.2 替换自动动作段

推荐伪代码：

```python
gripper_action_with_retry(mc, 0, "step1 张开")

checked_sync_angles(mc, pick_hover["angles"], 20, 20, "pick_hover")
verify_coords_near(mc, pick_hover["coords"], "pick_hover")

checked_short_linear(mc, pick["coords"], 15, 10, "pick")
gripper_action_with_retry(mc, 1, "step4 闭合")

checked_short_linear(mc, pick_hover["coords"], 15, 10, "pick_hover")

checked_sync_angles(mc, drop_hover["angles"], 20, 20, "drop_hover")
verify_coords_near(mc, drop_hover["coords"], "drop_hover")

checked_short_linear(mc, drop["coords"], 15, 10, "drop")
gripper_action_with_retry(mc, 0, "step8 张开")

checked_short_linear(mc, drop_hover["coords"], 15, 10, "drop_hover")

if not safe_return_home(mc):
    # 回零失败时臂可能偏高/带载，提示扶稳再释放，不要直接 release
    input("-> 请扶稳机械臂后按 Enter 释放舵机...")
    mc.release_all_servos()
    return
```

### 6.3 夹爪动作返回值必须熔断（不能"按 y 继续"）

`checked_gripper_action()` 返回 `False` 时，**不能仅提示"是否继续"就让用户按 y 通过**——否则闭合未确认会抓空、张开未确认会带载升空，运行仍报告成功。改为重试一次后仍失败则熔断：

```python
def gripper_action_with_retry(mc, state, label, retries=1):
    for attempt in range(retries + 1):
        if checked_gripper_action(mc, state, GRIPPER_SPEED):
            return True
        if attempt < retries:
            print(f"  -> {label} 夹爪未确认，重试一次...")
            time.sleep(0.5)
    raise RuntimeError(f"{label}: 夹爪动作未确认，熔断停止（避免抓空/未释放）")
```

同时 `checked_gripper_action` 内部 `mc.set_gripper_state(state, speed)` 必须包在 `try/except` 里：瞬时串口异常不应传播到 `main except Exception` 触发 `mc.stop()+release_all_servos()`，否则夹爪小故障会导致整轮 abort 并掉电。失败时返回 `False`，由 `gripper_action_with_retry` 决定重试或熔断。

在当前 `pymycobot 4.0.5` 没有 `get_gripper_value()` 的情况下，`checked_gripper_action` 会开环等待并返回 `True`，重试不会触发；若未来库支持反馈，不能忽略失败返回。

---

## 7. 安全参数建议

先用保守参数：

- 关节角回放速度：`15` 到 `20`
- 短距离直线速度：`10` 到 `15`
- 关节回放 timeout：`20s`
- 短距离直线 timeout：`10s`
- 坐标校验容差：`25mm`
- 示教点稳定读取次数：不少于 `5`
- 实机首测：空载，不夹物块，只验证路径

在路径空载跑通后，再进行低速带物块测试。

---

## 8. 分段验证计划

### 8.1 静态验证

```powershell
python -m py_compile mycobot_pc_tests\teach_and_pick.py
python -c "import importlib.util; p='mycobot_pc_tests/teach_and_pick.py'; s=importlib.util.spec_from_file_location('t', p); m=importlib.util.module_from_spec(s); s.loader.exec_module(m); print('import ok')"
```

### 8.2 无动作函数验证

用假对象验证：

- `is_safe_coord()` 对 NaN、非数字、半径过小、Z 过高返回 `False`
- `checked_sync_angles()` 在 fake 返回 `0` 时抛异常
- `checked_short_linear()` 在 fake 返回 `0` 时抛异常
- `record_teach_point()` 在坐标/角度无效时拒绝保存

### 8.3 实机空载分段验证

1. 只运行示教阶段，确认四个点记录合理。
2. 只执行 `pick_hover` 关节角回放，不执行下探。
3. 执行 `pick_hover -> pick -> pick_hover`，空载、不闭夹爪。
4. 执行 `pick_hover -> drop_hover`，空载。
5. 执行完整空载路径。
6. 最后才带物块低速测试。

每一步都必须手在急停/电源旁，发现抖动、卡顿、超时立即 `Ctrl+C`。

---

## 9. 验收标准

本方案完成后，至少满足：

- 不再依赖自动 `Z+60` 生成悬停点。
- 长距离动作不再使用 `mode=1`。
- 示教点同时保存关节角和坐标。
- 关节角回放后有坐标一致性校验。
- 短距离直线动作失败会熔断，不自动 fallback。
- 多次坐标读取异常时拒绝继续，不返回最后一次异常值。
- `checked_gripper_action()` 返回值在主流程中被处理。
- **`safe_return_home` 用 `get_filtered_angles` 而非 raw `get_angles`，NaN/非有限 max_diff 拒绝回零。**
- **`mode=0` 保护拉升注释明确为关节空间弧线（非垂直抬升），提示用户确认弧线范围。**
- **夹爪失败重试一次后熔断，不再以未确认状态"按 y 继续"。**
- **回零失败时不直接 release，先提示用户扶稳再释放，避免抬高臂下沉撞物。**
- **`set_gripper_state` 包在 try/except 内，夹爪小故障不传播到 main 触发整轮掉电。**
- **`get_filtered_angles`/`get_filtered_coords` 的 `except` 打印异常类型+消息，不静默吞异常。**
- 仍明确标注为 PC 端调试脚本，不进入正式板端闭环。

---

## 11. Code-Review 修复记录（2026-07-05）

对实现脚本 `mycobot_pc_tests/teach_replay_pick.py` 跑了一轮 `/code-review`（max effort，10 finder 角度 + 验证器 + sweep），输出 15 条发现。已修复 6 项，静态编译 + 导入 + 函数级验证通过。修复与对应章节：

| 发现 | 修复内容 | 对应章节 |
|------|---------|---------|
| `safe_return_home` 用 raw `get_angles()`，NaN 使 max_diff=NaN 跳过保护拉升 | 改用 `get_filtered_angles(mc)`；diffs 做有限值校验；非有限或读不到则拒绝回零 | §4.2 |
| `mode=0` 保护拉升注释暗示垂直抬升，实为关节空间弧线 | 修正注释"末端走弧线非垂直"，增加弧线范围提醒 | §4.2 |
| 夹爪失败仅"按 y 继续"，可能抓空/未释放却报告成功 | 新增 `gripper_action_with_retry`，失败重试一次后熔断 | §6.3 |
| 回零失败直接 `release_all_servos`，抬高臂阻尼下沉撞物 | 改为提示用户扶稳后按 Enter 再 release | §4.2 / §6.2 |
| `set_gripper_state` 在 try 外，串口异常导致整轮 abort+掉电 | 移入 try/except，失败返回 False | §6.3 |
| `get_filtered_angles`/`get_filtered_coords` bare except 静默吞异常 | 改为 `except Exception as e` 打印类型+消息 | §5.2 / §5.3 |

### 未改动的 PLAUSIBLE 项（设计性约束，留作后续）

- `get_filtered_angles` 仍只取首个合法读数，未做跨读数稳定性校验（符合 §5.2 字面规范；若实机出现 in-range 但错误的 angles 读数，再补 `ANG_STABLE_TOL` 双读校验）。
- 重复实现 `is_safe_coord`/`get_port`/`checked_gripper_action`/`safe_return_home`（本方案明确要求新文件对照执行，非崩溃；后续若合并到 `teach_and_pick.py` 应抽公共模块）。
- 物块位置无自动校验（PC 端调试脚本，依赖用户放置；板端闭环应由视觉/CPU 判定）。
- `auto_phase` 短距离直线后无 `verify_coords_near`（`sync_send_coords` 返回 1 即 `is_in_position==1`，API 已保证到位；符合 §4.3）。

### 实机验证待办

修复后仍需按 §8.3 分段实机空载验证，重点观察：
1. 通电后 `safe_return_home` 是否稳定读 angles（不再因 NaN 误判）。
2. 保护拉升 mode=0 弧线轨迹是否避开物块。
3. 夹爪开环模式下 `gripper_action_with_retry` 是否正常通过（4.0.5 应返回 True 不触发重试）。

---

## 10. 给执行 Agent 的注意事项

- 不要运行会让机械臂动作的命令，除非用户明确要求并确认现场安全。
- 不要把本方案写成“生产级安全”或“毫无风险”。
- 修改前先备份当前 `teach_and_pick.py` 或提交 diff 摘要。
- 修改后先做静态验证，再让用户执行分段实机测试。
- 若实机仍在某个短距离 `mode=1` 动作失败，不要自动改为全局 `mode=0`，应回到示教点位和姿态重新评估。

---

## 12. 第四次只读测试后的增补修复要求（2026-07-05）

本节根据 [trial_run_logs.md](file:///D:/第十届集创赛-雄芯院材料/mycobot_pc_tests/audit_logs/trial_run_logs.md) 中“第三次试运行”和“第四次只读测试”的实测结果追加，供 Claude 执行代码修复时直接采用。

### 12.1 新增实测事实

用户已通过 `COM10` 运行只读脚本，在“夹爪尖端朝前”的人工扶正直立姿态下连续采样 10 次，读数完全一致：

```text
get_angles() = [-10.81, 2.46, 1.49, -8.17, 2.28, 3.07]
get_coords() = [27.3, -67.9, 416.9, -94.22, 3.23, -98.75]
```

由此得到两条修复边界：

- `HOME_ANGLES` 暂不需要从 `[0, 0, 0, 0, 0, 0]` 改为非零实测值。真实直立姿态相对理论零位最大偏差仅 `10.81` 度，低于 `45` 度大偏差阈值。
- 直立安全姿态下 `Z` 可达 `416.9mm`，因此 `Z_MAX = 280.0` 只能作为抓取/放置工作区边界，不能作为 `get_filtered_coords()` 的传感器读数有效性边界。

第三次试运行中出现 `max_diff=78.9`，但同时 `get_coords()` 读到 `Z=401.7`。结合第四次读数，这更像是上电后角度读数暂态/旧值/不稳定值，而不是 `HOME_ANGLES` 定义错误。

### 12.2 必须修改 1：拆分“读数有效性”与“工作区安全”

当前错误点：`get_filtered_coords()` 内部调用 `is_safe_coord(coords)`，导致直立高位坐标被 `Z_MAX=280.0` 错误拦截。

应新增一个只负责串口读数有效性的函数，例如：

```python
def is_valid_coord_reading(coords):
    if not isinstance(coords, list) or len(coords) < 6:
        return False
    vals = coords[:6]
    if not all(isinstance(v, (int, float)) and math.isfinite(v) for v in vals):
        return False

    x, y, z, rx, ry, rz = vals
    if not (-500.0 <= x <= 500.0 and -500.0 <= y <= 500.0 and -100.0 <= z <= 500.0):
        return False
    if not all(-360.0 <= a <= 360.0 for a in (rx, ry, rz)):
        return False
    return True
```

执行要求：

- `get_filtered_coords()` 只能调用 `is_valid_coord_reading()`，不能调用 `is_safe_coord()`。
- `is_safe_coord()` 保留为业务级安全函数，只在“保存示教点”和“主动发送工作区笛卡尔运动目标前”调用。
- `record_teach_point()` 的逻辑应保持两层检查：先 `get_filtered_coords()` 确认读数稳定，再 `is_safe_coord(coords, is_hover=...)` 判断该点是否适合作为抓取/放置工作点。
- `verify_coords_near()` 用 `get_filtered_coords()` 读取当前坐标即可，不要因为当前姿态高于 `Z_MAX` 就把读数判为无效。

### 12.3 必须修改 2：`get_filtered_angles()` 增加稳定性校验

当前风险点：`get_filtered_angles()` 只要读到第一个数值合法、范围在 `[-180, 180]` 内的角度列表就返回。第三次试运行中 `coords` 显示接近直立高位，但 `max_diff` 却为 `78.9`，说明单次合法角度读数仍可能不可信。

建议改为“连续两次或三次读数稳定才返回”：

```python
ANG_STABLE_TOL = 3.0

def get_filtered_angles(mc, retries=8, stable_tol=ANG_STABLE_TOL):
    valid = []
    for _ in range(retries):
        try:
            angles = mc.get_angles()
        except Exception as e:
            print(f"【警告】get_angles 抛异常: {type(e).__name__}: {e}，重试中...")
            time.sleep(0.15)
            continue

        if isinstance(angles, list) and len(angles) >= 6:
            vals = list(angles[:6])
            if all(isinstance(v, (int, float)) and math.isfinite(v) for v in vals):
                if all(-180.0 <= v <= 180.0 for v in vals):
                    valid.append(vals)
                    if len(valid) >= 2:
                        prev = valid[-2]
                        cur = valid[-1]
                        delta = max(abs(cur[i] - prev[i]) for i in range(6))
                        if delta <= stable_tol:
                            return cur
        time.sleep(0.15)
    return None
```

执行要求：

- 不要再把首个合法角度读数立即当作可信姿态。
- 若读不到稳定角度，`safe_return_home()` 必须拒绝自动回零，并提示用户重新扶稳/重试。
- 在 `safe_return_home()` 中打印完整 `angles` 和 `diffs`，便于判断后续实机日志。

### 12.4 必须修改 3：禁用大偏差分支中的自动笛卡尔保护拉升

第三次试运行证明，`sync_send_coords(protect_coords, 30, 0, timeout=10)` 几乎立刻失败。`mode=0` 虽然不是直线插补，但目标仍然是笛卡尔坐标，仍需要底层 IK 解算和坐标到位判定。

因此，前文 §4.2 / §11 中“保护拉升 mode=0 弧线轨迹”的待办应被本节覆盖：**下一版不要继续自动构造 `protect_coords` 并发送坐标保护拉升。**

建议新的 `safe_return_home()` 策略：

```text
1. power_on 后先等待 1.5~2.0 秒，让舵机抱紧和读数稳定。
2. 读取稳定 angles。
3. 打印 angles、diffs、max_diff。
4. 若 max_diff <= 45：
   - 低速 sync_send_angles(HOME_ANGLES, 15, timeout=ANG_REPLAY_TIMEOUT)。
5. 若 max_diff > 45：
   - 不再执行 protect_coords 笛卡尔拉升。
   - 打印当前 coords（若可读）辅助诊断。
   - 提示用户保持扶稳并手动移动到夹爪尖端朝前的预回零姿态。
   - 用户确认后重新读取 angles。
   - 仍 >45 时返回 False，不自动回扫。
```

如果后续确实需要自动化大偏差回零，只允许采用“额外示教的关节中转点”方案，例如新增 `home_ready_angles` 或 `safe_transit_angles`，运行时用 `sync_send_angles()` 回放该关节姿态。不要根据当前坐标硬改 Z。

### 12.5 建议修改 4：上电后的稳定等待与诊断打印

在 `prepare_phase()` 或 `safe_return_home()` 内，`mc.power_on()` 后建议增加：

```python
time.sleep(1.5)
```

并在回零前打印：

```python
print(f"当前稳定关节角: {angles}")
print(f"相对 HOME_ANGLES 偏差: {diffs}, max_diff={max_diff:.1f}")
```

若可读坐标，也打印：

```python
coords = get_filtered_coords(mc)
print(f"当前稳定空间坐标: {coords}")
```

注意：坐标打印失败不能阻塞小偏差关节回零；坐标只用于诊断和工作区动作校验，不再用于构造自动回零保护目标。

### 12.6 验证顺序

Claude 完成代码修改后，应按以下顺序验证：

1. 静态检查：

```powershell
python -m py_compile mycobot_pc_tests\teach_replay_pick.py
python -m py_compile mycobot_pc_tests\read_current_pose.py mycobot_pc_tests\read_pose_continuous.py
```

2. 只读脚本安全确认：

- `read_current_pose.py` 和 `read_pose_continuous.py` 不应包含 `send_*`、`sync_send_*`、`set_*`、`release_*`、`power_on()`、`power_off()`。

3. 无动作函数级验证：

- 用假对象验证 `get_filtered_coords()` 接受 `[27.3, -67.9, 416.9, -94.22, 3.23, -98.75]` 这类高位直立读数。
- 用假对象验证 `is_safe_coord()` 仍会拒绝将 `Z=416.9` 当作抓取/放置工作点。
- 用假对象验证 `get_filtered_angles()` 对连续稳定角度返回，对单次跳变角度返回 `None` 或继续重试。
- 用假对象验证 `safe_return_home()` 在 `max_diff > 45` 时不会调用 `sync_send_coords()`。

4. 实机验证仍必须分段：

- 先只运行读姿态脚本。
- 再运行修复后的示教阶段和回零阶段，不进入自动抓取。
- 确认“夹爪尖端朝前”姿态上电后 `safe_return_home()` 读到的 `max_diff` 接近 `10.81` 而不是 `78.9`。
- 只有回零稳定后，才继续后续空载路径验证。

### 12.7 更新后的验收标准

在 §9 原验收标准基础上，新增以下硬性验收项：

- `get_filtered_coords()` 不再因 `Z=401.7/416.9` 这类直立高位坐标返回 `None`。
- `is_safe_coord()` 仍能限制抓取/放置工作空间，不能因为放宽读数过滤而放宽主动运动边界。
- `get_filtered_angles()` 必须要求连续稳定读数，不能首个合法值直接通过。
- `safe_return_home()` 的大偏差分支不能调用 `sync_send_coords()` 或任何笛卡尔保护拉升。
- `HOME_ANGLES` 暂保留 `[0, 0, 0, 0, 0, 0]`，不改成本机实测非零值。
- 日志输出必须能看见回零前的 `angles`、`diffs`、`max_diff`，便于下一轮实机复核。

---

## 13. 第三次试运行后的二次增补修复要求（2026-07-05）

本节根据 [trial_run_3_logs.md](file:///D:/第十届集创赛-雄芯院材料/mycobot_pc_tests/audit_logs/trial_run_3_logs.md) 的实机日志追加，覆盖并细化 §12 中的 `safe_return_home()` 交互回零策略。Claude 执行修复时应以本节为准。

### 13.1 新增实测事实

第三次试运行暴露两个新的实机问题：

1. **手动扶正提示与舵机状态冲突**
   当前 `prepare_phase()` 先执行 `mc.power_on()`，随后 `safe_return_home()` 检测到大偏差才提示用户“手动移动到预回零姿态”。但此时舵机已经上电锁紧，用户无法手掰扶正。
   这不是用户操作问题，而是交互状态机问题：要求用户手动移动前，程序必须先让机械臂变软。

2. **第 6 轴末端旋转误触发大偏差拦截**
   第二次实验中，直立高位坐标为：

   ```text
   get_coords() = [49.0, -82.5, 419.1, -83.7, 65.31, -103.96]
   ```

   这说明机械臂大臂已经接近直立安全姿态。但关节角为：

   ```text
   get_angles() = [-14.67, 0.08, -16.17, 18.63, -4.39, 65.21]
   ```

   若按 6 个轴一起计算 `max_diff`，第 6 轴 `65.21` 度会触发 `>45` 大偏差拦截。
   但若只看 1-5 轴，大臂最大偏差仅 `18.63` 度，已经满足低速回零条件。

### 13.2 必须修改 1：把“大臂安全门”和“第 6 轴末端旋转”拆开

不要再用 6 个轴的整体 `max_diff` 判断大臂是否可低速回零。安全回零的主要风险是 1-5 轴导致的大臂扫动、下坠或碰撞；第 6 轴主要影响夹爪自身旋转，不应单独阻断大臂小偏差回零。

建议新增常量：

```python
ARM_JOINT_COUNT = 5
ARM_MAX_DIFF_SAFE = 45.0
WRIST6_WARN_DIFF = 90.0
```

建议新增辅助函数：

```python
def calc_home_diffs(angles):
    all_diffs = [abs(a - b) for a, b in zip(angles, HOME_ANGLES)]
    arm_diffs = all_diffs[:ARM_JOINT_COUNT]
    arm_max_diff = max(arm_diffs)
    wrist6_diff = all_diffs[5] if len(all_diffs) >= 6 else 0.0
    return all_diffs, arm_diffs, arm_max_diff, wrist6_diff
```

诊断打印应改为：

```python
all_diffs, arm_diffs, arm_max_diff, wrist6_diff = calc_home_diffs(angles)
print(f"当前稳定关节角: {angles}")
print(f"相对 HOME_ANGLES 全轴偏差: {all_diffs}")
print(f"大臂 1-5 轴偏差: {arm_diffs}, arm_max_diff={arm_max_diff:.1f}")
print(f"第 6 轴末端旋转偏差: {wrist6_diff:.1f}")
```

判断条件应改为：

```python
if arm_max_diff <= ARM_MAX_DIFF_SAFE:
    if wrist6_diff > WRIST6_WARN_DIFF:
        print(f"【提示】第6轴末端旋转偏差较大 ({wrist6_diff:.1f}度)，回零时夹爪会自转，请确认末端线缆/夹爪周边无干涉。")
    res = mc.sync_send_angles(HOME_ANGLES, 15, timeout=ANG_REPLAY_TIMEOUT)
```

执行要求：

- 不要简单删除第 6 轴信息；第 6 轴仍要打印和可选告警。
- 大偏差安全门只看 `arm_max_diff`，而不是 6 轴整体 `max_diff`。
- 日志中仍保留全轴偏差，便于后续判断末端夹爪旋转是否异常。
- `HOME_ANGLES` 仍保留 `[0, 0, 0, 0, 0, 0]`；第 6 轴在真正回零时仍可回到 0，只是不参与“大臂是否危险”的拦截。

### 13.3 必须修改 2：大偏差手动扶正前先释放舵机

当前代码在大偏差分支中直接提示用户扶正，但舵机已经上电锁紧。下一版必须改成“扶稳 -> 释放 -> 用户扶正 -> 上电 -> 稳定读取 -> 重新判断”。

建议替换大偏差分支为如下状态机：

```text
若 arm_max_diff > ARM_MAX_DIFF_SAFE：
  1. 打印当前 angles / all_diffs / arm_diffs / wrist6_diff / coords。
  2. 提示：当前大臂偏差较大，禁止自动回零；如需人工扶正，请先用手扶稳机械臂。
  3. 用户按 Enter 后调用 mc.release_all_servos()，让机械臂变软。
  4. 提示用户手动扶到“夹爪尖端朝前、接近直立”的预回零姿态。
  5. 用户按 Enter 后调用 mc.power_on()。
  6. 等待 POWER_ON_SETTLE。
  7. 调用 get_filtered_angles(mc) 和 get_filtered_coords(mc)。
  8. 重新计算 arm_max_diff。
  9. 若 arm_max_diff <= ARM_MAX_DIFF_SAFE，则低速 sync_send_angles(HOME_ANGLES, 15)。
 10. 若仍 >45，最多允许再重复 1 轮；仍失败则返回 False。
```

伪代码示例：

```python
def prompt_manual_prehome(mc, max_rounds=2):
    for idx in range(max_rounds):
        input("当前大臂偏差较大。请先用手扶稳机械臂，按 Enter 后释放舵机...")
        mc.release_all_servos()
        time.sleep(0.5)

        ans = input("请手动扶到【夹爪尖端朝前】的预回零姿态；完成后按 Enter 上电读取，输入 q 放弃: ")
        if ans.strip().lower() == "q":
            return None

        mc.power_on()
        time.sleep(POWER_ON_SETTLE)

        angles = get_filtered_angles(mc)
        coords = get_filtered_coords(mc)
        print(f"扶正后稳定关节角: {angles}")
        print(f"扶正后稳定空间坐标: {coords}")

        if angles is None:
            print("【错误】扶正后仍无法读取稳定关节角。")
            continue

        all_diffs, arm_diffs, arm_max_diff, wrist6_diff = calc_home_diffs(angles)
        print(f"扶正后大臂 1-5 轴偏差: {arm_diffs}, arm_max_diff={arm_max_diff:.1f}")
        print(f"扶正后第 6 轴偏差: {wrist6_diff:.1f}")
        if arm_max_diff <= ARM_MAX_DIFF_SAFE:
            return angles

        print(f"arm_max_diff={arm_max_diff:.1f} 仍 > {ARM_MAX_DIFF_SAFE}，请继续扶正。")

    return None
```

`safe_return_home()` 使用该函数时：

```python
if arm_max_diff > ARM_MAX_DIFF_SAFE:
    angles = prompt_manual_prehome(mc)
    if angles is None:
        return False
    all_diffs, arm_diffs, arm_max_diff, wrist6_diff = calc_home_diffs(angles)
```

注意事项：

- 释放舵机前必须提示用户扶稳，不能静默 `release_all_servos()`。
- 用户确认扶正后必须重新 `power_on()`，并等待 `POWER_ON_SETTLE`。
- 若用户输入 `q` 放弃，返回 `False`，由外层 `prepare_phase()` 继续提示用户扶稳后安全退出。
- 不要在大偏差分支重新引入 `sync_send_coords()` 或 `protect_coords`。

### 13.4 必须修改 3：小偏差路径允许第 6 轴较大但需提示

第三次日志中的关键姿态：

```text
angles = [-14.67, 0.08, -16.17, 18.63, -4.39, 65.21]
```

应被判断为：

```text
all_diffs = [14.67, 0.08, 16.17, 18.63, 4.39, 65.21]
arm_diffs = [14.67, 0.08, 16.17, 18.63, 4.39]
arm_max_diff = 18.63
wrist6_diff = 65.21
```

因此应走“小偏差低速回零”路径，而不是再次要求用户扶正。

但如果第 6 轴偏差很大，例如 `>90` 或 `>120`，应打印末端旋转提示，提醒用户确认夹爪、线缆、物块和桌面周边无干涉。第 6 轴不参与大臂扫动安全门，不代表第 6 轴旋转完全没有末端干涉风险。

### 13.5 建议修改 4：更新日志术语

原日志输出中的：

```text
相对 HOME_ANGLES 偏差: ..., max_diff=...
```

容易让执行者误以为 6 轴整体最大偏差仍是安全门。下一版建议改为：

```text
相对 HOME_ANGLES 全轴偏差: [...]
大臂 1-5 轴偏差: [...], arm_max_diff=...
第 6 轴末端旋转偏差: ...
```

告警文案也应从：

```text
当前姿态与零位偏差较大
```

改为：

```text
当前大臂 1-5 轴与零位偏差较大
```

这样后续日志能直接看出究竟是大臂危险，还是末端第 6 轴旋转偏差。

### 13.6 验证要求

Claude 完成代码修复后，除 §12.6 原验证外，新增以下假对象验证：

1. **第 6 轴偏差不阻断回零**

```python
angles = [-14.67, 0.08, -16.17, 18.63, -4.39, 65.21]
```

期望：

- `arm_max_diff == 18.63`
- `safe_return_home()` 允许调用 `sync_send_angles(HOME_ANGLES, 15, ...)`
- 不调用 `release_all_servos()`
- 不调用 `sync_send_coords()`

2. **1-5 轴大偏差触发人工扶正流程**

```python
angles = [-74.44, -71.98, -52.55, 36.21, -1.66, 62.22]
```

期望：

- `arm_max_diff == 74.44`
- 不调用 `sync_send_angles()` 直接回零
- 不调用 `sync_send_coords()`
- 先提示用户扶稳，再调用 `release_all_servos()`
- 用户确认扶正后调用 `power_on()`，等待后重读角度

3. **大偏差扶正成功路径**

先返回：

```python
[-74.44, -71.98, -52.55, 36.21, -1.66, 62.22]
```

扶正后返回：

```python
[-14.67, 0.08, -16.17, 18.63, -4.39, 65.21]
```

期望：

- 第一轮进入释放/扶正流程
- 第二轮用 `arm_max_diff=18.63` 通过
- 最终调用 `sync_send_angles(HOME_ANGLES, 15, ...)`
- 全流程不调用 `sync_send_coords()`

4. **用户放弃路径**

用户输入 `q` 后：

- 返回 `False`
- 不调用 `sync_send_angles()`
- 不调用 `sync_send_coords()`
- 外层继续提示用户扶稳后释放/退出

静态检查仍需：

```powershell
python -m py_compile mycobot_pc_tests\teach_replay_pick.py
```

### 13.7 更新后的验收标准

在 §12.7 基础上追加：

- `safe_return_home()` 的大臂安全门只使用 1-5 轴偏差，即 `arm_max_diff`。
- 第 6 轴偏差必须继续打印，不得静默丢弃；较大时提示末端旋转风险。
- 大偏差人工扶正前必须先提示用户扶稳，再调用 `release_all_servos()`。
- 用户扶正后必须重新 `power_on()` 并等待 `POWER_ON_SETTLE`，再读取稳定角度。
- `[-14.67, 0.08, -16.17, 18.63, -4.39, 65.21]` 这类直立高位姿态应允许低速回零。
- `[-74.44, -71.98, -52.55, 36.21, -1.66, 62.22]` 这类远端低位姿态应触发人工扶正流程，而不是自动回零。
- 大偏差流程仍禁止 `sync_send_coords()` / `protect_coords` / 笛卡尔拉升。

---

## 14. 第五次试运行后的短距离动作重构要求（2026-07-05）

本节根据 [trial_run_5_logs.md](file:///D:/第十届集创赛-雄芯院材料/mycobot_pc_tests/audit_logs/trial_run_5_logs.md) 追加，覆盖并修正前文中“短距离 hover <-> pick/drop 可继续使用 `sync_send_coords(..., mode=0/1)`”的旧假设。Claude 下一轮修复 `teach_replay_pick.py` 时应以本节为准。

### 14.1 新增实测事实

第五次试运行已经验证以下链路通过：

- 示教四点成功，且 `pick_hover` / `pick` / `drop_hover` / `drop` 均同时记录了 `angles` 和 `coords`。
- 大偏差人工扶正流程成功，扶正后 `arm_max_diff=21.1`，可以低速回直立零位。
- 自动阶段可关节角回放到 `pick_hover`，且到位后坐标校验 `delta_xyz=3.9 mm`，说明长距离关节角回放方案成立。

实际熔断点只发生在第三阶段第 3 步：

```text
短距离下探到 pick:
target coords = [156.1, -185.8, 70.7, 176.37, -2.42, 135.62]
异常: pick: 短距离直线动作超时或失败 (返回 0)
```

对应当前代码为：

```python
res = mc.sync_send_coords(target_coords, speed, 0, timeout=timeout)
```

关键判断：`mode=0` 只表示关节空间插值，不表示绕过 IK。只要调用的是 `sync_send_coords()`，固件端仍必须把笛卡尔坐标目标解算为关节角；远端低位姿态、腕部姿态多解或边界点仍可能快速返回 `0`。

### 14.2 根因结论

run-5 的根因不是串口、夹爪、HOME 零位或回零流程，而是短距离动作仍在使用笛卡尔坐标控制。

前文把短距离动作从 `mode=1` 改为 `mode=0`，只能避免“空间直线插补”本身的一部分奇异风险，不能避免 `sync_send_coords()` 的目标 IK 解算失败。因此，继续延长 timeout、继续调整 `mode` 或继续围绕坐标下探微调，都不是根因修复。

### 14.3 必须修改 1：运行阶段彻底禁止坐标驱动

下一版 `auto_phase()` 中，除只读校验外，不应再调用 `sync_send_coords()`。所有运动驱动都应使用示教阶段记录的 `angles`：

```python
def checked_short_angles(mc, target_angles, speed, timeout, label):
    print(f"  -> 短距离关节回放到 {label}: {target_angles}")
    res = mc.sync_send_angles(target_angles, speed, timeout=timeout)
    if res != 1:
        raise RuntimeError(f"{label}: 短距离角度动作超时或失败 (返回 {res})")
    return True
```

执行要求：

- `checked_short_linear()` 应删除、停用或改名为 `checked_short_angles()`，不要继续保留实际调用 `sync_send_coords()` 的实现。
- `auto_phase()` 第 3/5/7/9 步应传入 `point["angles"]`，不是 `point["coords"]`。
- `coords` 只保留给 `verify_coords_near()` 做到位后一致性校验，不参与任何运动规划。
- 任何自动阶段出现 `sync_send_coords(` 都应视为本节验收失败。

建议更新后的自动阶段骨架：

```python
checked_sync_angles(mc, pick_hover["angles"], ANG_REPLAY_SPEED, ANG_REPLAY_TIMEOUT, "pick_hover")
verify_coords_near(mc, pick_hover["coords"], "pick_hover")

checked_short_angles(mc, pick["angles"], SHORT_ANGLE_SPEED, SHORT_ANGLE_TIMEOUT, "pick")
verify_coords_near(mc, pick["coords"], "pick")

gripper_action_with_retry(mc, 1, "step4 闭合")

checked_short_angles(mc, pick_hover["angles"], SHORT_ANGLE_SPEED, SHORT_ANGLE_TIMEOUT, "pick_hover")
verify_coords_near(mc, pick_hover["coords"], "pick_hover")

checked_sync_angles(mc, drop_hover["angles"], ANG_REPLAY_SPEED, ANG_REPLAY_TIMEOUT, "drop_hover")
verify_coords_near(mc, drop_hover["coords"], "drop_hover")

checked_short_angles(mc, drop["angles"], SHORT_ANGLE_SPEED, SHORT_ANGLE_TIMEOUT, "drop")
verify_coords_near(mc, drop["coords"], "drop")

gripper_action_with_retry(mc, 0, "step8 张开")

checked_short_angles(mc, drop_hover["angles"], SHORT_ANGLE_SPEED, SHORT_ANGLE_TIMEOUT, "drop_hover")
verify_coords_near(mc, drop_hover["coords"], "drop_hover")
```

### 14.4 必须修改 2：新增短距离关节连续性安全门

全角度回放可以绕开固件 IK，但不能无条件信任所有示教角度。run-5 中 `drop_hover -> drop` 出现明显关节分支跳变：

```text
drop_hover angles = [-73.65, -57.8, 78.9, -174.61, 0.11, 131.21]
drop angles       = [-73.65, -68.81, -59.15, 40.34, -4.74, 65.03]
```

这两个点在空间上只是悬停到下探，但关节 3/4/6 差异巨大。如果直接按角度短距离回放，可能产生大幅摆动或腕部翻转。因此，改为纯角度驱动的同时，必须增加 hover/down 点对的关节连续性校验。

建议新增：

```python
SHORT_ARM_JOINT_MAX_DELTA = 30.0
SHORT_WRIST6_MAX_DELTA = 45.0

def validate_short_angle_pair(src_point, dst_point, label):
    src = src_point["angles"]
    dst = dst_point["angles"]
    deltas = [abs(dst[i] - src[i]) for i in range(6)]
    arm_max_delta = max(deltas[:5])
    wrist6_delta = deltas[5]
    print(f"  -> {label} 短距离关节差: {deltas}, arm_max_delta={arm_max_delta:.1f}, wrist6_delta={wrist6_delta:.1f}")
    if arm_max_delta > SHORT_ARM_JOINT_MAX_DELTA:
        raise RuntimeError(f"{label}: 1-5轴短距离关节变化过大，疑似示教到不同IK分支，请重新示教")
    if wrist6_delta > SHORT_WRIST6_MAX_DELTA:
        raise RuntimeError(f"{label}: 第6轴短距离旋转过大，疑似腕部翻转，请重新示教")
```

执行要求：

- 在自动阶段开始前先校验：
  - `pick_hover -> pick`
  - `pick -> pick_hover`
  - `drop_hover -> drop`
  - `drop -> drop_hover`
- 任一校验失败时，禁止进入实机自动动作，要求用户重新示教对应点位。
- `pick_hover -> pick` 在 run-5 中关节变化平滑，可作为下一轮首个空载验证段。
- run-5 的 `drop_hover/drop` 不应直接用于完整纯角度回放；必须重新示教，直到 hover/down 角度变化连续。

### 14.5 建议修改 3：常量与日志术语更新

建议把短距离速度常量从 `SHORT_LINEAR_*` 改为 `SHORT_ANGLE_*`，避免执行者误以为仍在做坐标直线：

```python
SHORT_ANGLE_SPEED = 12
SHORT_ANGLE_TIMEOUT = 10
```

日志文案同步替换：

- `短距离直线下探到 pick` -> `短距离关节下探到 pick`
- `短距离直线抬起回 pick_hover` -> `短距离关节抬起回 pick_hover`
- `短距离过渡到 ...` -> `短距离关节回放到 ...`

注意：这意味着末端不保证严格垂直直线运动。物块位置必须与示教时一致，且示教的 hover/down 点必须保证中间关节路径不会扫到物块、桌面或夹具。

### 14.6 分段验证要求

Claude 修改后先做静态和假对象验证，不要直接让用户跑完整抓取。

静态检查：

```powershell
python -m py_compile mycobot_pc_tests\teach_replay_pick.py
```

源码检查：

```powershell
Select-String -Path mycobot_pc_tests\teach_replay_pick.py -Pattern "sync_send_coords"
```

期望：自动阶段和短距离动作封装中不再出现 `sync_send_coords()`。若保留该调用，只允许存在于未被自动流程调用的历史函数中，并必须有明确注释说明“禁用/保留仅供对照”。

假对象验证：

1. `validate_short_angle_pair(pick_hover, pick)` 对 run-5 的 pick 点对通过。
2. `validate_short_angle_pair(drop_hover, drop)` 对 run-5 的 drop 点对失败，提示重新示教。
3. `checked_short_angles()` 在 fake 返回 `1` 时通过，返回 `0` 时抛异常。
4. `auto_phase()` 的第 3/5/7/9 步不调用 `sync_send_coords()`。

实机验证顺序：

1. 空载，只跑 `pick_hover -> pick -> pick_hover`，不夹物块。
2. 若第 1 步稳定，再启用夹爪开合，但仍不进入 drop 段。
3. 重新示教平滑的 `drop_hover/drop` 后，只跑 `drop_hover -> drop -> drop_hover` 空载。
4. 两个短距离段均稳定后，才跑完整空载路径。
5. 完整空载路径稳定后，才低速带物块测试。

### 14.7 更新后的验收标准

在 §13.7 基础上追加以下硬性验收项：

- 自动运行阶段不得使用 `sync_send_coords()` 驱动机械臂运动。
- `coords` 只用于 `verify_coords_near()` 等只读校验，不参与目标运动发送。
- `checked_short_linear()` 不得继续作为自动流程的一部分；若保留，必须标记为禁用/历史对照。
- 新增 `checked_short_angles()` 并用于 `pick_hover <-> pick`、`drop_hover <-> drop`。
- 新增短距离关节连续性校验，防止 hover/down 示教点落在不同 IK 分支。
- run-5 的 `drop_hover/drop` 角度跳变应被校验拦截，不能直接进入完整自动回放。
- 第下一轮实机验证必须先从 `pick_hover -> pick -> pick_hover` 空载分段开始。

---

## 15. 第六次试运行后的上行短距离超时验证方案（2026-07-06）

本节根据 [trial_run_6_logs.md](file:///D:/第十届集创赛-雄芯院材料/mycobot_pc_tests/audit_logs/trial_run_6_logs.md) 追加，覆盖 §14 中“纯关节短距离回放应直接跑通 pick_hover -> pick -> pick_hover”的预期。Claude 下一轮修改或实验前，应先按本节做根因区分，不要直接叠加复杂恢复逻辑。

### 15.1 新增实测事实

第六次试运行已经证明 §14 的重构方向有效，但暴露了新的方向性超时问题：

- `python -m py_compile mycobot_pc_tests\teach_replay_pick.py` 已通过。
- 自动阶段已经不再使用 `sync_send_coords()` 驱动短距离运动。
- 四对短距离点对关节连续性预校验通过：
  - `pick_hover->pick`: `arm_max_delta=15.4`, `wrist6_delta=0.9`
  - `drop_hover->drop`: `arm_max_delta=16.8`, `wrist6_delta=0.6`
- `pick_hover` 关节回放成功，坐标校验 `delta_xyz=7.3 mm`。
- `pick_hover -> pick` 短距离关节下探成功，坐标校验 `delta_xyz=5.7 mm`。
- `pick -> pick_hover` 短距离关节上抬失败，`sync_send_angles()` 返回 `0`，当前参数为 `SHORT_ANGLE_SPEED=12`、`SHORT_ANGLE_TIMEOUT=10`。

本轮关键角度：

```text
pick_hover angles = [38.4, -56.6, -53.08, 19.59, -4.83, 85.69]
pick angles       = [38.4, -70.13, -53.17, 34.98, -2.28, 84.81]
```

下行成功与上行失败的差异主要集中在：

```text
J2: -56.6 -> -70.13  下行成功；-70.13 -> -56.6  上行失败
J4:  19.59 -> 34.98  下行成功； 34.98 -> 19.59  上行失败
```

### 15.2 对 Claude 根因分析的复核结论

“人工扶正/回零链路引入不可恢复偏差”目前证据不足，低概率作为直接原因。理由：

- 第 2 步已经从回零后成功回放到 `pick_hover`，并通过 `delta_xyz=7.3 mm` 校验。
- 第 3 步已经从 `pick_hover` 成功下探到 `pick`，并通过 `delta_xyz=5.7 mm` 校验。
- 若扶正/回零导致坐标系或示教角度不可用，失败更可能出现在第 2 步或第 3 步，而不是只出现在第 5 步反向上抬。

更合理的当前假设是：**上行抗重力运动 + 低速/短 timeout 参数组合 + `sync_send_angles()` 到位确认窗口** 共同导致返回 `0`。这不是第 5 次的 IK 问题；第 6 次已经把问题推进到关节角运动的物理/固件确认层。

但当前日志还不能区分以下三种情况：

1. 物理上确实未在 10s 内完成上行动作。
2. 物理上基本到位，但 `sync_send_angles()` 的到位确认/串口响应未在 10s 内完成。
3. step 4 夹爪闭合后改变了末端负载、摩擦或腕部微小姿态，使第 5 步更难收敛。

因此，下一轮不应立刻加入“失败后重试”或“自动释放”等逻辑；应先用最小条件变化确认根因。

### 15.3 必须验证 1：只延长上行超时窗口

第一优先级验证：仅把短距离关节动作超时窗口从 `10s` 放宽到 `20s`，保持速度、点位、夹爪流程不变。

临时验证常量：

```python
SHORT_ANGLE_SPEED = 12
SHORT_ANGLE_TIMEOUT = 20
```

判定：

- 若 `pick -> pick_hover` 通过，且坐标校验通过：根因优先判定为 `10s` 窗口不足或到位确认偏慢。
- 若仍返回 `0`：不能继续简单加 timeout，应进入 §15.4 / §15.5 的单段探针，区分夹爪闭合影响和上行动作本身问题。

注意：本验证是“条件变化”，不是最终架构修复。不要因为 20s 通过就马上把所有动作一律无脑改成 20s；应继续判断是否只对上行动作需要更长窗口。

### 15.4 必须验证 2：单独测试 `pick -> pick_hover` 上行动作

第二优先级验证：绕过完整自动流程中的夹爪张开/闭合，只单独测试从 `pick` 到 `pick_hover` 的关节角动作。

推荐手工流程：

```text
1. 示教得到 pick_hover_angles / pick_angles。
2. 回零准备完成后，先发送 pick_angles，让机械臂到 pick。
3. 不执行夹爪闭合。
4. 单独发送 sync_send_angles(pick_hover_angles, 12, timeout=10)。
5. 记录返回值、是否肉眼到位、是否能通过 verify_coords_near(pick_hover.coords)。
```

判定：

- 若单段 `timeout=10` 也返回 `0`：上行方向/速度/timeout 是主因，夹爪闭合不是必要条件。
- 若单段 `timeout=10` 通过，但完整流程 step 5 失败：夹爪闭合或流程时序是重要因素，应进入 §15.5。
- 若单段 `timeout=20` 通过但 `timeout=10` 失败：可固化为“上行动作需要更长超时”。

Claude 若需要写辅助代码，应只加低风险的“单段探针/调试入口”，不要改动主流程安全策略。探针必须显式提示用户扶稳、空载、确认动作范围，并且不应自动释放舵机。

### 15.5 必须验证 3：夹爪状态影响

第三优先级验证：比较有无 step 4 夹爪闭合对上行动作的影响。

实验 A：

```text
pick_hover -> pick -> pick_hover
不执行 gripper_action_with_retry(mc, 1, "step4 闭合")
```

实验 B：

```text
pick_hover -> pick -> 闭合夹爪 -> pick_hover
```

判定：

- A 通过、B 失败：夹爪闭合导致的末端负载/摩擦/腕部扰动不可忽略。后续应在闭合后增加短暂停顿和姿态复核，或提高闭合后上行动作速度/timeout。
- A、B 都失败：夹爪不是主因，优先调整上行动作参数。
- A、B 都通过：第六次可能含偶发串口/确认超时，后续需要重复 2-3 次验证稳定性。

### 15.6 后续代码修改候选

只有在 §15.3 至 §15.5 至少完成一项验证后，才考虑固化代码修改。

候选 A：方向分离参数。

```python
SHORT_DOWN_SPEED = 12
SHORT_DOWN_TIMEOUT = 10
SHORT_UP_SPEED = 16
SHORT_UP_TIMEOUT = 20
```

在 `auto_phase()` 中：

```python
checked_short_angles(mc, pick["angles"], SHORT_DOWN_SPEED, SHORT_DOWN_TIMEOUT, "pick")
checked_short_angles(mc, pick_hover["angles"], SHORT_UP_SPEED, SHORT_UP_TIMEOUT, "pick_hover")
checked_short_angles(mc, drop["angles"], SHORT_DOWN_SPEED, SHORT_DOWN_TIMEOUT, "drop")
checked_short_angles(mc, drop_hover["angles"], SHORT_UP_SPEED, SHORT_UP_TIMEOUT, "drop_hover")
```

候选 B：动作完成后的坐标复核保留不变。

即使 `sync_send_angles()` 返回 `1`，仍继续执行 `verify_coords_near()`；不要因为加长 timeout 或提高速度而删掉坐标校验。

候选 C：超时后诊断而不是立即盲目重试。

如果未来要在 `checked_short_angles()` 中处理 `res == 0`，第一版应先读取并打印：

```python
actual_angles = get_filtered_angles(mc)
actual_coords = get_filtered_coords(mc)
```

然后计算与目标角度/坐标的差异。只有当“已经接近目标，但确认超时”被日志证明后，才考虑一次低速补发或确认重试。不要在未知位置上自动重试大动作。

### 15.7 暂不建议的修改

- 不要恢复 `sync_send_coords()`：第 5 次已证明坐标 IK 是独立风险源。
- 不要一失败就 `release_all_servos()`：如果机械臂已抬起或带载，释放会带来下沉风险。仍应保留“提示扶稳后释放”的模式；当前 `main except` 若仍直接 release，后续应单独审查。
- 不要立刻把所有短距离动作都提速到很高：先验证上行段，再决定是否只提高上行。
- 不要把失败后自动重试作为第一修复：重试可能掩盖物理未到位、夹爪干涉或串口确认问题。

### 15.8 更新后的验收标准

在 §14.7 基础上追加以下硬性验收项：

- 下一轮必须优先做 `SHORT_ANGLE_TIMEOUT=20` 对照，或单段 `pick -> pick_hover` 探针；不能直接叠加复杂恢复逻辑。
- 如果 20s 通过，方案中应明确区分“下行动作参数”和“上行动作参数”，不要只写“统一加 timeout”。
- 如果单段 `pick -> pick_hover` 在无夹爪闭合时通过，而完整流程失败，后续方案必须把夹爪闭合后的暂态/负载作为一等风险处理。
- `checked_short_angles()` 失败时的后续版本应增加实际角度/坐标诊断打印，先判断“未到位”还是“确认超时”，再决定是否补发。
- 自动阶段继续禁止 `sync_send_coords()`。
- 坐标校验 `verify_coords_near()` 继续保留，不能因纯关节驱动或加长 timeout 而删除。

---

## 16. 第七次试运行后的软到位判定与上行参数修正方案（2026-07-06）

本节根据 [trial_run_7_logs.md](file:///D:/第十届集创赛-雄芯院材料/mycobot_pc_tests/audit_logs/trial_run_7_logs.md) 追加。第七次试运行新增的失败诊断已经把问题从“推测上行超时”推进到“确认为固件到位判定未收敛”。Claude 下一轮应以本节为准执行代码修正。

### 16.1 新增确证事实

第七次试运行中，`pick -> pick_hover` 上行动作仍在第 5 步返回 `0`，但失败后诊断读取到：

```text
目标 pick_hover angles = [42.09, -63.63, -45.79, 14.23, 1.49, 87.89]
实际关节角            = [41.3, -65.83, -46.23, 13.27, 2.19, 87.09]
关节差值              = [0.79, 2.20, 0.44, 0.96, 0.70, 0.80]
max_angle_delta       = 2.2 deg

目标 pick_hover coords = [234.3, 129.0, 104.6, ...]
实际坐标               = [233.6, 123.2, 92.0, ...]
XYZ 最大偏差约          = 12.6 mm
```

这说明：

- 机械臂不是没动，也不是走错方向。
- 纯关节角驱动已经避开了 run-5 的 `sync_send_coords()` / IK 问题。
- 第 5 步失败时，物理位置已经完成绝大部分上行动作，6 轴最大残差只有 `2.2 deg`，XYZ 仍在当前 `COORD_VERIFY_TOL=25 mm` 容差内。
- `sync_send_angles()` 返回 `0` 的直接原因更像是固件 `is_in_position` 精确到位判定没有在 timeout 内满足，而不是上位机目标错误。

### 16.2 根因定论

run-7 将 run-6 的推测确证为：

> 上行抗重力路径的末段 PID 收敛速度不足，导致 `sync_send_angles()` 的固件到位判定在当前窗口内未返回成功。

具体表现：

- 下行 `pick_hover -> pick` 是重力辅助方向，10s 内完成且 `verify_coords_near()` 通过。
- 上行 `pick -> pick_hover` 是 J2 抬升抗重力方向，连续 run-6 / run-7 在相同步骤复现失败。
- run-7 失败诊断显示实际只差 `2.2 deg`，不是“运动失败”，而是“接近目标但固件严格到位判定未收敛”。

因此，下一步不要再围绕以下方向消耗时间：

- 不要恢复或微调 `sync_send_coords()`。
- 不要继续怀疑人工扶正/回零链路作为直接原因。
- 不要把它归为随机串口丢帧；串口至少成功返回了后续实际角度/坐标诊断。

### 16.3 必须修改 1：上下行短距离参数分离

保留下行慢速安全策略，提高上行窗口与必要速度。建议第一版采用保守分离：

```python
SHORT_DOWN_SPEED = 12
SHORT_DOWN_TIMEOUT = 10

SHORT_UP_SPEED = 16
SHORT_UP_TIMEOUT = 25
```

如果第八次仍出现“实际角度差 <3 deg 但返回 0”，不要继续盲目加速度；应启用 §16.4 的软到位判定。若需要第二档参数，再考虑：

```python
SHORT_UP_SPEED = 20
SHORT_UP_TIMEOUT = 30
```

执行要求：

- `pick_hover -> pick` 和 `drop_hover -> drop` 使用 `SHORT_DOWN_*`。
- `pick -> pick_hover` 和 `drop -> drop_hover` 使用 `SHORT_UP_*`。
- 日志必须打印当前使用的 speed / timeout，避免后续实验不知道实际参数。
- 下行参数不因上行失败而同步提高，防止下探撞物风险增加。

### 16.4 必须修改 2：新增“软到位成功”判定

`sync_send_angles()` 返回 `0` 后，不应立即熔断；应先读取实际姿态，判断是“未到位”还是“固件确认超时但物理已接近目标”。

建议新增常量：

```python
SOFT_ANGLE_SUCCESS_TOL = 3.0       # deg
SOFT_COORD_SUCCESS_TOL = 25.0      # mm, 可复用 COORD_VERIFY_TOL
SOFT_SETTLE_SECONDS = 0.5
```

建议把 `checked_short_angles()` 改成可选软判定版本：

```python
def checked_short_angles(
    mc,
    target_angles,
    speed,
    timeout,
    label,
    expected_coords=None,
    allow_soft_success=False,
):
    print(f"  -> 短距离关节回放到 {label}: {target_angles} (speed={speed}, timeout={timeout}s)")
    res = mc.sync_send_angles(target_angles, speed, timeout=timeout)
    if res == 1:
        return True

    print(f"  -> [诊断] {label}: sync_send_angles 返回 {res}")
    time.sleep(SOFT_SETTLE_SECONDS)
    actual_angles = get_filtered_angles(mc)
    actual_coords = get_filtered_coords(mc)

    if actual_angles:
        angle_deltas = [abs(actual_angles[i] - target_angles[i]) for i in range(6)]
        max_angle_delta = max(angle_deltas)
        print(f"  -> [诊断] {label}: 实际关节角={actual_angles}")
        print(f"  -> [诊断] {label}: 与目标关节角差值={angle_deltas}, max={max_angle_delta:.1f} deg")
    else:
        max_angle_delta = None

    if actual_coords:
        print(f"  -> [诊断] {label}: 实际坐标={actual_coords}")
    else:
        print(f"  -> [诊断] {label}: 无法读取稳定实际坐标")

    if allow_soft_success and max_angle_delta is not None:
        angle_ok = max_angle_delta <= SOFT_ANGLE_SUCCESS_TOL
        coord_ok = True
        if expected_coords is not None and actual_coords is not None:
            coord_delta = max(abs(actual_coords[i] - expected_coords[i]) for i in range(3))
            coord_ok = coord_delta <= SOFT_COORD_SUCCESS_TOL
            print(f"  -> [诊断] {label}: 软到位坐标 delta_xyz={coord_delta:.1f} mm")
        if angle_ok and coord_ok:
            print(f"  -> [软通过] {label}: 固件返回 {res}，但实际姿态已在容差内，继续执行后续坐标校验。")
            return True

    raise RuntimeError(f"{label}: 短距离角度动作超时或失败 (返回 {res})")
```

自动阶段调用时，上行动作允许软通过，下行动作仍可保持严格：

```python
checked_short_angles(
    mc,
    pick_hover["angles"],
    SHORT_UP_SPEED,
    SHORT_UP_TIMEOUT,
    "pick_hover",
    expected_coords=pick_hover["coords"],
    allow_soft_success=True,
)
verify_coords_near(mc, pick_hover["coords"], "pick_hover")
```

执行要求：

- 软通过只允许用于短距离上行动作，且必须满足角度和坐标双重约束。
- 若 `actual_angles` 读不到，不能软通过。
- 若 `expected_coords` 已提供但 `actual_coords` 读不到，建议不要软通过；至少第八次实机前保持严格。
- 即使软通过，后续仍必须执行 `verify_coords_near()`。
- 软通过日志必须显式打印，不能静默吞掉 `sync_send_angles()` 返回 `0`。

### 16.5 必须修改 3：异常退出不要立即释放舵机

run-7 仍然在自动阶段异常后打印：

```text
安全起见，已紧急释放所有舵机。
```

但本次失败时机械臂已经抬到接近 `pick_hover`，实际 Z 为 `92.0 mm`。直接 `release_all_servos()` 可能导致机械臂下沉或带物块掉落。此前回零失败分支已经采用“提示扶稳后再释放”的策略，自动阶段异常也应一致。

建议把 `main except Exception` 中的自动释放改为：

```python
except Exception as e:
    print(f"\n🚨 [异常] 运行中发生错误或运动超时: {e}")
    if mc:
        try:
            mc.stop()
        except Exception:
            pass
        print("-> 请【用手扶稳机械臂/物块】后按 Enter 释放舵机...")
        input()
        mc.release_all_servos()
        print("已在人工扶稳后释放所有舵机。")
```

执行要求：

- `KeyboardInterrupt` 可继续走急停逻辑，但普通动作超时/RuntimeError 不应静默立即掉电。
- 如果后续有“夹物块”测试，异常退出前尤其必须提示扶稳。

### 16.6 验证要求

静态验证：

```powershell
python -m py_compile mycobot_pc_tests\teach_replay_pick.py
```

假对象验证：

1. `sync_send_angles()` 返回 `1` 时，`checked_short_angles()` 直接通过。
2. `sync_send_angles()` 返回 `0`，实际角度最大差 `2.2 deg`，实际坐标 XYZ 最大差 `12.6 mm`，且 `allow_soft_success=True` 时，应返回 True 并打印软通过。
3. 同样返回 `0`，但 `max_angle_delta > 3.0 deg` 时，必须抛异常。
4. 返回 `0` 且坐标差 `>25 mm` 时，必须抛异常。
5. 下行动作 `allow_soft_success=False` 时，即使角度接近，也应按严格策略抛异常，除非 Claude 明确决定下行也启用软判定并说明理由。
6. 普通 `RuntimeError` 进入 `main except` 时，应调用 `stop()`，提示用户扶稳，再 `release_all_servos()`。

实机第八次验证顺序：

1. 空载运行到 `pick -> pick_hover`。
2. 观察第 5 步是硬通过、软通过，还是仍失败。
3. 若软通过，确认紧接着的 `verify_coords_near(pick_hover)` 是否通过。
4. 只有 `pick_hover -> pick -> pick_hover` 稳定后，再进入 `drop_hover/drop` 段。
5. 带物块测试前至少完成 2 次空载完整路径。

### 16.7 更新后的验收标准

在 §15.8 基础上追加：

- `pick -> pick_hover` 连续 run-6 / run-7 复现返回 `0` 应被记录为系统性上行到位判定问题，不再按随机失败处理。
- 上行短距离动作必须使用独立的 speed / timeout 参数。
- `checked_short_angles()` 必须能在返回 `0` 后读取实际角度/坐标，并按软到位门限决定是否继续。
- 软到位门限建议第一版为 `max_angle_delta <= 3.0 deg` 且 `delta_xyz <= 25 mm`。
- 软通过后仍必须执行 `verify_coords_near()`。
- 自动阶段普通异常退出不得立即 `release_all_servos()`，必须提示用户扶稳后释放。
- 继续禁止 `sync_send_coords()` 回到自动运动链路。

### 16.8 历史补充：run-7 前已实施但仍不足的代码修改

本节原为 `§15.9 已实施的代码修改`。它记录的是第七次试运行前，Claude 已按 §15.6 / §15.8 实施的方向分离参数和失败诊断打印。run-7 已证明这些修改有价值：诊断打印直接揭示了 `max_angle_delta=2.2 deg` 的软到位事实；但 `SHORT_UP_SPEED=16` / `SHORT_UP_TIMEOUT=15` 仍不足以让固件硬判定通过。因此后续执行应以 §16.3 / §16.4 为准，在此基础上继续增加上行窗口并加入软到位判定。

Claude 根据 §15.6 Candidate A（方向分离参数）和 §15.6 Candidate C（失败诊断打印）直接实施以下代码修改，跳过 §15.3-§15.5 的单独验证步骤。理由是用户已在实机确认 run-6 复现且 Codex 分析完成，不再需要重复条件分离。

#### 16.8.1 常量修改（方向分离参数）

`teach_replay_pick.py` 中删除统一常量 `SHORT_ANGLE_SPEED=12` / `SHORT_ANGLE_TIMEOUT=10`，拆分为方向分离参数：

```python
SHORT_DOWN_SPEED = 12         # 下行关节速度（重力辅助）(%)
SHORT_DOWN_TIMEOUT = 10       # 下行关节超时 (s)
SHORT_UP_SPEED = 16           # 上行动作适当提速，克服逆重力阻力 (%)
SHORT_UP_TIMEOUT = 15         # 上行动作放宽超时窗口 (s)
```

选择依据：
- 下行（pick_hover→pick、drop_hover→drop）在 run-6 已验证 12%/10s 可靠通过，保持不动。
- 上行（pick→pick_hover、drop→drop_hover）run-6 熔断。Codex 根因判定为上行抗重力 + 低速/短 timeout 耦合。
- 速度从 12 提高到 16（不高到 20 以免碰撞，§15.6 Candidate A 建议 16）。
- 超时从 10s 放宽到 15s（比 §15.3 建议的 20s 更保守，避免掩盖其他问题）。

#### 16.8.2 失败诊断打印（§15.6 Candidate C）

`checked_short_angles()` 在 `sync_send_angles` 返回 `!= 1` 时，不再直接抛异常，而是先读取并打印：

```python
actual_angles = get_filtered_angles(mc)
actual_coords = get_filtered_coords(mc)
angle_deltas = [abs(actual_angles[i] - target_angles[i]) for i in range(6)]
```

然后才抛异常。这满足 §15.8 第 4 项验收标准："增加实际角度/坐标诊断打印，先判断'未到位'还是'确认超时'，再决定是否补发。"

不在未知位置上自动重试（符合 §15.7 第 3 项暂不建议的修改）。

#### 16.8.3 auto_phase 脚步更新

auto_phase 第 3/5/7/9 步使用方向分离参数：

| 步骤 | 方向 | 速度 | 超时 | run-6 状态 |
|------|------|------|------|-----------|
| 3. pick_hover→pick | 下行 | 12% | 10s | ✅ 已验证通过 |
| 5. pick→pick_hover | 上行 | 16% | 15s | ❌ 之前熔断，待验证 |
| 7. drop_hover→drop | 下行 | 12% | 10s | 待验证（run-5 drop 点对不可用） |
| 9. drop→drop_hover | 上行 | 16% | 15s | 待验证（需重新示教） |

#### 16.8.4 验收标准更新

在 §15.8 基础上追加以下验收项：

- 方向分离常量实施后重新跑 run-6 相同的完整流程（含夹爪开合），确认 step 5 通过。
- pick→pick_hover 上行先验证通过后，再推进到 drop 段（重新示教平滑的 drop 点对）。
- 诊断打印不会误触发（仅当 sync_send_angles 返回非 1 时才执行）。
- 自动阶段继续禁止 sync_send_coords()。
- 坐标校验 verify_coords_near() 继续保留，不对短距离动作做任何坐标运动发送。

#### 16.8.5 剩余风险

- 未做 §15.4 的单段探针验证：无法区分"上行 16%/15s 直接可用"和"夹爪闭合是必要条件但被更高的速度/timeout 掩盖"。如果后续 drop 段上行仍失败，应先回到 §15.4 的分离验证。
- 未做 §15.5 的夹爪状态影响比较：如果完整流程通过但某次偶发超时，原因可能是夹爪而不是方向。
- 方向分离参数只在 auto_phase 中应用；checked_short_angles 函数签名未改，future 用户不会意外忘传参数。
