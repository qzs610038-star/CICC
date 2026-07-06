# Home_Ready 回零中间姿态——历史实测角度汇总

> 用途：供后续实验直接选用已验证的 home_ready 角度，跳过人工引导示教环节。
> 选择原则：优先选 arm_max_diff 较小（留足够软通过余量）、J2 绝对值 ≤35° 的点。
> 跑新实验时，在 `auto_phase_v2` 调用前用 `checked_sync_angles(mc, home_ready_angles, ...)` 直驱到该姿态即可。

---

## 已验证可用的 home_ready（arm_max_diff ≤ 45，均实现 0 轮人工扶正）

| # | 来源 | 角度 (J1~J6) | arm_max_diff | J2 | Z (mm) | R (mm) | 实际跑通 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | run-12 示教 | [9.22, -31.02, -0.17, -7.29, -8.52, 39.9] | **31.0°** | -31.02 | 347.0 | 201.3 | ✅ run-12 带载 3cm |
| 2 | run-11 Run B 示教 | [-0.79, -30.84, -4.3, 5.36, -4.83, 42.53] | **30.8°** | -30.84 | 355.1 | 200.3 | ✅ run-11 带载 3cm (R~274) |
| 3 | run-11 Run A 示教 | [4.92, -32.34, -0.17, -4.04, -0.35, 56.16] | **32.3°** | -32.34 | 346.6 | 202.3 | ✅ run-11 带载 2cm |
| 4 | run-10 扶正后实测 | [7.99, -36.38, 3.51, 12.12, -4.48, 54.75] | **36.4°** | -36.38 | 363.0 | - | ✅ run-10 人工扶正后 |

## 备选（建议优先重示教）

| # | 来源 | 角度 (J1~J6) | arm_max_diff | 说明 |
| --- | --- | --- | --- | --- |
| 5 | run-10 首示教 | [7.29, -43.06, 4.57, 12.91, -6.41, 55.37] | **43.1°** | run-10 首示教，留余量不足，软通过后被推过 45（实际 45.4 → 需 1 轮扶正） |

---

## 推荐顺序

1. **首选 #1（run-12）**：J2=-31.02°，arm_max_diff=31.0，余量 14°，本轮（2026-07-06）最新实测带载跑通
2. **可替代 #2（run-11 Run B）**：J2=-30.84°，arm_max_diff=30.8，余量 14.2°，但 J1=-0.79 偏左，注意放置场景对称性
3. **备选 #3（run-11 Run A）**：J2=-32.34°，arm_max_diff=32.3，余量 12.7°，兼容性好

---

## 使用方式

在 `auto_phase_v2` 前直接硬编码：

```python
HOME_READY_PRESET = [9.22, -31.02, -0.17, -7.29, -8.52, 39.9]  # run-12
mc.sync_send_angles(HOME_READY_PRESET, 20, timeout=20)
```

或通过命令行参数 `--preset <name>` 选择。**V2.3 已实现 JSON 点位复用**（plan §14.4 优先级1）：

```powershell
# 加载已验证预设（跳过手动示教，仍跑全部安全门）
python mycobot_pc_tests/teach_replay_pick_return_ready.py --preset run12_3cm_inboard

# 示教后保存为新预设
python mycobot_pc_tests/teach_replay_pick_return_ready.py --teach --save-preset run13_candidate --save-preset-log trial_run_13 --save-preset-obj 3 --save-preset-notes "..."
```

预设文件位于 `mycobot_pc_tests/presets/teach_points_<name>.json`，含五点 angles/coords + safety 元数据（radii/home_ready_arm_max_diff/R_MAX/HOME_READY_TARGET_ARM_MAX/ARM_MAX_DIFF_SAFE）+ notes。**加载预设后仍必须执行全部安全门**（is_safe_coord/半径分区/validate_short_angle_pair/validate_return_angle_pair/validate_return_ready/用户确认轨迹），不因预设跳过安全校验。
