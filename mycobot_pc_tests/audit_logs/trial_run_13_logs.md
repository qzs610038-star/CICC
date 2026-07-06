# 第十三次试运行（2026-07-06 V2.3 JSON 预设复用 + 当前稳定参数验证）

> 脚本: `mycobot_pc_tests/teach_replay_pick_return_ready.py` (V2.3)
> 对照方案: `mycobot_pc_tests/audit_logs/return_ready_teach_replay_v2_plan.md` §14 (V2.3)
> 基线: `mycobot_pc_tests/teach_replay_pick.py`（未修改）
> 上一轮: run-12 V2.2 带 3cm 跑通，step5/9 正常返回1（未触发软通过/微调），合计~21.3s，0轮扶正。改善来自"短超时+内收工作半径 R<=252"共同作用。
> 状态: **待上板运行**。本文件为日志模板骨架，由 Claude 生成；真实数据由用户上板运行 V2.3 脚本后填写。

---

## 0. V2.3 本轮目标（plan §14.5 run-13 验收）

- **验证预设复用 + 当前 V2.2 稳定参数**，不再继续提速（SHORT_UP_SPEED 保持 16）。
- 用 run-12 预设点位直接回放，跳过手动五点示教，消除人工示教变量。
- 带 3cm 物块，工作半径保持 R<=252mm 或至少 R<=260mm。
- 另做一次示教回归：故意拖出 R>280，确认 record_teach_point 能提示重试并允许重新保存安全点。

---

## 1. V2.3 新增功能确认

| 功能 | 状态 | 说明 |
|------|------|------|
| JSON 预设加载 `--preset <name>` | ✅ 已实现 | 跳过手动示教，仍跑全部安全门 |
| JSON 预设保存 `--save-preset <name>` | ✅ 已实现 | 示教后保存含元数据的 JSON |
| `_soft_refine` 日志兜底 `fmt_mm()` | ✅ 已实现 | coord_delta=None 不再抛 TypeError |
| `record_teach_point` q 放弃退出口 | ✅ 已实现 | 输入 q 抛 TeachAbort 安全退出 |
| `record_return_ready_point` q 退出口 | ✅ 已实现 | 同上 |

预设文件: `mycobot_pc_tests/presets/teach_points_run12_3cm_inboard.json`（run-12 五点 + safety 元数据）

---

## 2. 运行命令

### 2.1 主测试：预设复用回放

```powershell
python mycobot_pc_tests/teach_replay_pick_return_ready.py --preset run12_3cm_inboard
```

### 2.2 示教回归测试（plan §14.5-5）

```powershell
python mycobot_pc_tests/teach_replay_pick_return_ready.py
# 故意把 pick_hover 拖到 R>280，确认循环重试提示，再拖回安全区保存
```

---

## 3. 终端日志记录

### 3.1 预设复用回放

> 上板运行后粘贴完整控制台输出。重点观察"预设安全门校验"段和各 step 耗时。

```text
（待填写：--preset run12_3cm_inboard 的完整输出）
```

### 3.2 示教回归测试（R>280 重试）

```text
（待填写：故意拖出 R>280 后的循环重试提示 + 拖回安全区保存的输出）
```

---

## 4. 预设安全门校验结果

> 加载预设后脚本自动跑的安全门（is_safe_coord/半径分区/validate_short_angle_pair/validate_return_angle_pair/validate_return_ready）。

| 校验项 | 结果 |
|--------|------|
| pick_hover is_safe_coord + 半径分区 | 待填（R=244.4 推荐）|
| pick is_safe_coord + 半径分区 | 待填（R=252.0 可接受）|
| drop_hover is_safe_coord + 半径分区 | 待填（R=240.5 推荐）|
| drop is_safe_coord + 半径分区 | 待填（R=245.3 推荐）|
| home_ready R_MAX + arm_max_diff<=45 | 待填（R=201.3, arm_max_diff=31.0）|
| 四对短距离连续性 | 待填 |
| drop_hover->home_ready 过渡 sanity | 待填（arm_max_delta=74.3 < 90）|
| validate_return_ready | 待填（31.0 <= 40 推荐余量门）|
| 用户确认轨迹无障碍 | 待填 |

---

## 5. 自动阶段分步结果 + 耗时

| 步骤 | 动作 | 速度/超时 | 结果 | delta_xyz | 耗时 | 对比 run-12 |
|------|------|-----------|------|-----------|------|-------------|
| 1 | 张开夹爪 | 开环 2.5s | 待填 | - | - | - |
| 2 | 长距离 -> pick_hover | 20/20s | 待填 | 待填 | 待填 | run-12: 6.6mm |
| 3 | 短距离下探 -> pick | 12/10s | 待填 | 待填 | 待填 | run-12: 7.2mm |
| 4 | 闭合夹爪 | 开环 2.5s | 待填 | - | - | - |
| 5 | 短距离抬起 -> pick_hover | 16/15s | 待填 | 待填 | 待填 | run-12: 10.7mm / 4.5s |
| 6 | 长距离 -> drop_hover | 20/20s | 待填 | 待填 | 待填 | run-12: 4.1mm |
| 7 | 短距离下降 -> drop | 12/10s | 待填 | 待填 | 待填 | run-12: 3.7mm |
| 8 | 张开夹爪 | 开环 2.5s | 待填 | - | - | - |
| 9 | 短距离抬起 -> drop_hover | 16/15s | 待填 | 待填 | 待填 | run-12: 9.9mm / 11.9s |
| 10 | 回零过渡 -> home_ready | 20/20s | 待填 | 待填 | 待填 | run-12: 10.1mm / 3.1s |
| 11 | home_ready -> HOME | 20/12s | 待填 | - | 待填 | run-12: 1.8s / "auto" |

---

## 6. 末段回零

- step 11 safe_return_home 返回：待填（目标 "auto"）
- 人工扶正轮数：待填（目标 0）
- 控制台最终消息：待填（期望 "🎉 V2.2 ... 0 轮人工扶正"）

---

## 7. 预设复用 vs 手动示教对比

| 指标 | run-12（手动示教） | run-13（预设复用） | 说明 |
|------|-------------------|-------------------|------|
| 示教耗时 | ~数分钟（手动五点） | ~0s（加载即用） | 预设复用省时 |
| step 5 delta_xyz | 10.7mm | 待填 | 应相当（同点位）|
| step 9 delta_xyz | 9.9mm | 待填 | 应相当 |
| step 11 耗时 | 1.8s | 待填 | 应相当 |
| 0 轮扶正 | ✅ | 待填 | 应保持 |
| 总耗时 | ~21.3s | 待填 | 应相当或更短（省示教）|

**关键观察**：预设复用应让 step 5/9/10/11 的 delta_xyz 和耗时与 run-12 相当（同点位同参数），验证可重复性。

---

## 8. 示教回归测试结果（plan §14.5-5）

- 故意拖出 R>280 的点：待填（pick_hover / drop_hover / ...）
- 是否触发循环重试提示（而非抛异常退出）：待填
- 拖回安全区后是否成功保存：待填
- q 放弃退出口是否测试：待填（是/否；若测，确认安全退出）

---

## 9. 通过条件核对（plan §14.5 run-13 验收）

- [ ] 全流程跑通（预设复用回放）
- [ ] home_ready 仍 0 轮人工扶正
- [ ] step 5/9 delta_xyz <= 12mm，不能回到 run-11 的 16.2mm
- [ ] 总耗时保持在 30s 量级
- [ ] 示教回归：R>280 触发重试提示，拖回后成功保存
- [ ] 预设安全门校验全部通过

---

## 10. 遗留问题 / 下一轮

- 待填
- 若 run-13 预设复用稳定：可考虑把边缘半径 R=260~274 作为压力测试单独验证（plan §14.5）。
- 若预设复用出现 delta_xyz 明显偏离 run-12：检查是否机械臂零位漂移或物块位置未对齐预设。
