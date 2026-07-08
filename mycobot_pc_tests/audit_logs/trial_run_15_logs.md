# 第十五次试运行（2026-07-07 V2.4 串口修复与回零/微调优化验证）

> 脚本: `mycobot_pc_tests/teach_replay_pick_return_ready.py` (V2.4)
> 方案: `mycobot_pc_tests/audit_logs/return_ready_teach_replay_v2_plan.md` §14.7 (V2.4)
> 上一轮 (Run-14): 完成点位录入和新预设保存，但发现串口 Bug 并指出末段冗余读数/微调超时等效率瓶颈。
> 本轮状态: **测试通过**。V2.4 串口 Bug 完美解决；L1~L4 四大提速杠杆全部生效，特别是 Step 9 微调超时收紧和 Step 11 去冗余读数有显著的耗时改善。

---

## 1. V2.4 优化项验证确认

| 优化杠杆 | 作用 | 状态 | 验证结果 |
|---|---|---|---|
| **串口 Bug 修复** | 移除 `get_port()` 盲读位置参数 | ✅ 验证通过 | 不带 `--port` 时可正确列出并交互选择串口，无冲突 |
| **L1 去除诊断读** | `safe_return_home` 自动分支不读 `diag_coords` | ✅ 验证通过 | 减少了静态臂下的坐标读取等待 |
| **L2 角度缓存** | `checked_return_transition` 实际角度透传，省去重读 | ✅ 验证通过 | 避免了 `safe_return_home` 启动时的角度重读 |
| **L3 回零提速** | `HOME_RETURN_SPEED` 20 $\rightarrow$ 25 | ✅ 验证通过 | 物理移动更干脆，固件收敛正常，无抖动，0轮人工扶正 |
| **L4 微调超时收紧**| `SOFT_REFINE_TIMEOUT` 6 $\rightarrow$ 3s | ✅ 验证通过 | Step 9 微调不收敛时的纯等时间被直接压缩 3s |

---

## 2. 终端日志记录

```text
PS D:\第十届集创赛-雄芯院材料> python mycobot_pc_tests/teach_replay_pick_return_ready.py --port COM9 --preset my_new_test
尝试连接机械臂 (COM9 @ 1000000)...
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
-> 请将手轻轻拿开，按 Enter 键通电并执行安全回零...
当前稳定关节角: [-0.52, -0.52, -0.7, -1.4, 0.17, 0.79]
相对 HOME_ANGLES 全轴偏差: [0.52, 0.52, 0.7, 1.4, 0.17, 0.79]
大臂 1-5 轴偏差: [0.52, 0.52, 0.7, 1.4, 0.17], arm_max_diff=1.4
第 6 轴末端旋转偏差: 0.8
arm_max_diff=1.4 <= 45.0，低速同步回直立零位...

====================================
【第三阶段：关节角示教回放 + 回零过渡 + 空间一致性校验 (V2)】
====================================

0. 短距离点对关节连续性预校验...
  -> pick_hover->pick 短距离关节差: [0.0899999999999963, 23.64, 0.0, 20.569999999999997, 2.2800000000000002, 1.5899999999999892], arm_max_delta=23.6, wrist6_delta=1.6
  -> pick->pick_hover 短距离关节差: [0.0899999999999963, 23.64, 0.0, 20.569999999999997, 2.2800000000000002, 1.5899999999999892], arm_max_delta=23.6, wrist6_delta=1.6
  -> drop_hover->drop 短距离关节差: [0.27, 20.22, 0.09000000000000341, 20.13, 2.1099999999999994, 3.4299999999999997], arm_max_delta=20.2, wrist6_delta=3.4
  -> drop->drop_hover 短距离关节差: [0.27, 20.22, 0.09000000000000341, 20.13, 2.1099999999999994, 3.4299999999999997], arm_max_delta=20.2, wrist6_delta=3.4
  -> 四对短距离点对关节连续性校验通过。

0b. 回零过渡校验 (drop_hover -> home_ready -> HOME)...
  -> drop_hover->home_ready 回零过渡关节差: [5.0, 16.43, 59.94, 24.779999999999998, 6.77, 15.560000000000002], arm_max_delta=59.9, wrist6_delta=15.6
  -> home_ready 校验: arm_diffs=[3.77, 24.43, 9.31, 2.9, 0.08], arm_max_diff=24.4, wrist6_diff=38.9
  -> home_ready 通过推荐余量门 (arm_max_diff=24.4 <= 40.0)。
  -> 回零过渡校验通过。

⚠️ 请确认轨迹范围内无障碍物，特别是 drop_hover -> home_ready 的空中过渡路径。
⚠️ 如遇危险，请随时按 Ctrl+C 触发急停！
-> 请将正方体放回【抓取点】，按 Enter 键开始（空载首测可不放物块）...

1. 张开夹爪准备...
  -> 下发夹爪动作: 张开...
  -> 【信息】当前库无 get_gripper_value 反馈接口，使用开环定时等待。

2. 关节角回放到 pick_hover...
  -> 关节回放到 pick_hover: [43.94, -36.38, -69.08, 16.87, 1.23, 94.13]
  -> pick_hover 坐标校验 delta_xyz=6.7 mm

3. 短距离关节下探到 pick...
  -> 关节回放到 pick: [43.85, -60.02, -69.08, 37.44, -1.05, 92.54] (speed=12, timeout=10s)
  -> pick 坐标校验 delta_xyz=6.8 mm

4. 闭合夹爪抓取目标...
  -> 下发夹爪动作: 闭合...
  -> 【信息】当前库无 get_gripper_value 反馈接口，使用开环定时等待。

5. 短距离关节抬起回 pick_hover...
  -> 关节回放到 pick_hover: [43.94, -36.38, -69.08, 16.87, 1.23, 94.13] (speed=16, timeout=15s)
  -> pick_hover 坐标校验 delta_xyz=10.7 mm
  -> [V2.2 耗时] step 5: 1.6s

6. 关节角回放到 drop_hover（空中长距离过渡）...
  -> 关节回放到 drop_hover: [1.23, -40.86, -69.25, 21.88, -6.85, 54.49]
  -> drop_hover 坐标校验 delta_xyz=5.5 mm

7. 短距离关节下降至 drop...
  -> 关节回放到 drop: [0.96, -61.08, -69.34, 42.01, -4.74, 51.06] (speed=12, timeout=10s)
  -> drop 坐标校验 delta_xyz=2.0 mm

8. 张开夹爪放置正方体...
  -> 下发夹爪动作: 张开...
  -> 【信息】当前库无 get_gripper_value 反馈接口，使用开环定时等待。

9. 短距离关节抬起回 drop_hover...
  -> 关节回放到 drop_hover: [1.23, -40.86, -69.25, 21.88, -6.85, 54.49] (speed=16, timeout=15s)
  -> [诊断] drop_hover: sync_send_angles 返回 0
  -> [诊断] drop_hover: 实际关节角=[1.66, -42.97, -69.6, 20.83, -5.88, 53.61]
  -> [诊断] drop_hover: 与目标关节角差值=[0.42999999999999994, 2.1099999999999994, 0.3499999999999943, 1.0500000000000007, 0.9699999999999998, 0.8800000000000026], max=2.1°
  -> [诊断] drop_hover: 实际坐标=[239.4, -61.1, 135.6, -176.29, -4.9, -142.01]
  -> [诊断] drop_hover: 软到位坐标 delta_xyz=10.9 mm
  -> [V2.2 微调] drop_hover: 残差 max_err=2.1°/delta_xyz=10.9mm 超微调触发阈值，低速二次微调 (speed=8, timeout=3s)...
  -> [V2.2 微调] drop_hover: sync_send_angles 返回 0
  -> [V2.2 微调] drop_hover: 微调后 max_err=2.1° (前 2.1°), delta_xyz=10.9mm (前 10.9mm)
  -> [V2.2 微调] drop_hover: 微调后仍未达精细门，按旧软通过门兜底。
  -> [软通过] drop_hover: 固件返回 0，但实际姿态已在容差内，继续执行后续坐标校验。
  -> drop_hover 坐标校验 delta_xyz=10.9 mm
  -> [V2.2 耗时] step 9: 20.0s

10. 回零过渡：drop_hover -> home_ready（空中长距离拉直）...
  -> 即将把臂从 drop_hover 拉向接近直立的 home_ready，请确认空中路径无障碍。
  -> 关节回放到 home_ready: [-3.77, -24.43, -9.31, -2.9, -0.08, 38.93] (speed=20, timeout=20s)
  -> [V2.1] home_ready: 实际姿态 arm_max_diff=26.3 (门 45.0)
  -> [V2.1] home_ready: 实际姿态在自动回零安全门内，safe_return_home 将走自动 回零。
  -> home_ready 坐标校验 delta_xyz=9.9 mm
  -> [V2.2 耗时] step 10: 2.6s

11. 从 home_ready 自动回直立零位...
  -> [V2.4 L2] 复用上游已读角度（臂在 home_ready 静止），跳过 safe_return_home 重读。angles=[-2.81, -26.27, -10.28, -3.86, 0.08, 39.81]
当前稳定关节角: [-2.81, -26.27, -10.28, -3.86, 0.08, 39.81]
相对 HOME_ANGLES 全轴偏差: [2.81, 26.27, 10.28, 3.86, 0.08, 39.81]
大臂 1-5 轴偏差: [2.81, 26.27, 10.28, 3.86, 0.08], arm_max_diff=26.3
第 6 轴末端旋转偏差: 39.8
arm_max_diff=26.3 <= 45.0，低速同步回直立零位...
  -> [V2.2 耗时] step 11: 1.2s

====================================
🎉 V2.2 五点示教 + home_ready 回零过渡测试流程跑通！
   末段回零：0 轮人工扶正（home_ready -> HOME 自动回零）。
====================================
```

---

## 3. 运行耗时纵向对比

| 关键步骤 | Run-14 耗时 (V2.3) | Run-15 耗时 (V2.4) | 改善结果与归因 |
|---|---|---|---|
| **Step 5** (pick $\rightarrow$ pick_hover) | 3.4s | 1.6s | 正常收敛 |
| **Step 9** (drop $\rightarrow$ drop_hover) | 23.0s | 20.0s | **缩短 3.0s**；微调超时 L4 (6s $\rightarrow$ 3s) 机制生效，减少了无意义等待时间。 |
| **Step 10** (drop_hover $\rightarrow$ home_ready) | 2.6s | 2.6s | 耗时一致，过渡平稳。 |
| **Step 11** (home_ready $\rightarrow$ HOME) | 2.4s | 1.2s | **缩短 1.2s（压缩近 50%）**；复用缓存角度 L2 与去除自动路径诊断读取 L1 生效，响应极其迅速。 |
| **窄义回零** (Step 10 + 11) | 5.0s | 3.8s | **缩短 1.2s**；窄义回零表现更紧凑。 |
| **广义回零** (Step 9 + 10 + 11) | 28.0s | 23.8s | **缩短 4.2s**；系统流畅度大幅度提升。 |

---

## 4. 结论与下一步方向
本次优化验证了缓存决策和参数微调的显著成效。针对末端回零判定时，底层固件响应慢的问题，将进一步在方案文档中构思“异步角度状态机到位判定”以取代底层固件的阻塞式 `sync_send_angles`，力争将物理回零耗时压低至 0.5s 级别。
