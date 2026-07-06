# 第九次试运行（2026-07-06 V2 五点示教 + home_ready 回零过渡）

> 脚本: `mycobot_pc_tests/teach_replay_pick_return_ready.py`
> 对照方案: `mycobot_pc_tests/audit_logs/return_ready_teach_replay_v2_plan.md`
> 基线: `mycobot_pc_tests/teach_replay_pick.py`（未修改，作为对照/回退入口）
> 状态: **待上板运行**。本文件为日志模板骨架，由 Claude 生成；真实数据由用户上板运行 V2 脚本后填写。
> 上一轮里程碑: run-8 完整抓放跑通，但末端回零需人工扶正 2 轮（drop_hover arm_max_diff≈70.7）。

---

## 0. V2 本轮目标

- 新增 `home_ready` 回零中间姿态，把末段从 `drop_hover -> safe_return_home`（人工扶正）
  改成 `drop_hover -> home_ready -> safe_return_home`（自动回零）。
- 验证带载工作区分级（推荐 R<=250，可接受 250<R<=260，边缘 260<R<=280）。
- 第 1 轮保持速度参数不变（SHORT_UP_SPEED=16），只验证 home_ready 改善回零自动化。

---

## 1. 终端日志记录

> 上板运行后粘贴完整控制台输出到此节。

```text
（待填写：python mycobot_pc_tests/teach_replay_pick_return_ready.py 的完整输出）
```

---

## 2. 五个示教点数据

> 上板运行后填写。每个点记录 angles / coords / R / 分区。

| 示教点 | angles (J1..J6) | coords (x,y,z,rx,ry,rz) | R (mm) | 分区 | arm_max_diff |
|--------|-----------------|--------------------------|--------|------|--------------|
| pick_hover | 待填 | 待填 | 待填 | 待填 | - |
| pick | 待填 | 待填 | 待填 | 待填 | - |
| drop_hover | 待填 | 待填 | 待填 | 待填 | - |
| drop | 待填 | 待填 | 待填 | 待填 | - |
| home_ready | 待填 | 待填 | 待填 | - (不套业务门) | 待填 |

分区说明：推荐 R<=250 / 可接受 250<R<=260 / 边缘 260<R<=280 / 禁止 R>280。
home_ready 不套 Z_MAX=280 抓放业务门，只过 R_MAX=280 物理臂展 + arm_max_diff<=45。

---

## 3. home_ready 自动回零安全门

- home_ready arm_max_diff = 待填
- 是否 <= 45.0（ARM_MAX_DIFF_SAFE）：待填（通过/未通过）
- 若未通过：是否重新示教更接近直立的 home_ready：待填

---

## 4. 回零过渡关节差

| 过渡 | arm_max_delta | wrist6_delta | 阈值 (90/120) | 结果 |
|------|---------------|--------------|---------------|------|
| drop_hover -> home_ready | 待填 | 待填 | 90 / 120 | 待填 |

---

## 5. 自动阶段分步结果

| 步骤 | 动作 | 速度/超时 | 结果 | 到位精度 |
|------|------|-----------|------|---------|
| 1 | 张开夹爪 | 开环 2.5s | 待填 | - |
| 2 | 长距离 -> pick_hover | 20/20s | 待填 | delta_xyz=待填 |
| 3 | 短距离下探 -> pick | 12/10s | 待填 | delta_xyz=待填 |
| 4 | 闭合夹爪 | 开环 2.5s | 待填 | - |
| 5 | 短距离抬起 -> pick_hover | 16/25s | 待填 | delta_xyz=待填 |
| 6 | 长距离 -> drop_hover | 20/20s | 待填 | delta_xyz=待填 |
| 7 | 短距离下降 -> drop | 12/10s | 待填 | delta_xyz=待填 |
| 8 | 张开夹爪 | 开环 2.5s | 待填 | - |
| 9 | 短距离抬起 -> drop_hover | 16/25s | 待填 | delta_xyz=待填 |
| 10 | 回零过渡 -> home_ready | 20/20s (软到位) | 待填 | delta_xyz=待填 |
| 11 | home_ready -> HOME 自动回零 | 15/20s | 待填 | arm_max_diff=待填 |

---

## 6. 末端回零是否需要人工扶正

- 是否进入 prompt_manual_prehome（arm_max_diff>45 分支）：待填
- 人工扶正轮数：待填（目标：0 轮，即 home_ready 让 safe_return_home 走自动回零）
- 最终 arm_max_diff（回零前）：待填

---

## 7. 空载 / 带物块

- 本轮为：待填（空载 / 带物块）
- 带载抬起是否明显下沉：待填
- drop -> drop_hover 是否仍软通过：待填

---

## 8. 物理观察

- drop_hover -> home_ready 空中过渡是否顺滑：待填
- home_ready 姿态是否扫到桌面/物块/线缆：待填
- 底座附近是否有自碰风险：待填

---

## 9. 通过条件核对（plan §8 第 1 轮）

- [ ] 全流程跑通
- [ ] 末端回零不需要人工扶正（0 轮）
- [ ] home_ready 后 arm_max_diff <= 45.0
- [ ] 远端点全部 R <= 280.0
- [ ] 是否有点 R > 260.0（边缘风险区）：待填

---

## 10. 遗留问题 / 下一轮

- 待填（如 step 9 仍软通过，可考虑第 3 轮 SHORT_UP_SPEED 16->20）
- 待填（如 home_ready 仍偶尔 >45，考虑 V2.1 双中间点 home_ready_1/home_ready_2）
