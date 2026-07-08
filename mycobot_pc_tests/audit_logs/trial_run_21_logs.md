# 第二十一次试运行（2026-07-08 V2.9 坐标校验完整恢复与重复精度监测）

> 脚本: `mycobot_pc_tests/teach_replay_pick_return_ready.py` (V2.9)
> 方案: 将 `SKIP_COORD_VERIFY_ON_STRICT_PASS` 临时置为 `False`，强制恢复所有阶段（特别是 `pick` 与 `drop` 关节严格通过 res==1 时）的坐标一致性校验（`verify_coords_near`），用以高频精确测量连续抓取中每次动作终点的三维笛卡尔偏差（`delta_xyz`）。
> 本轮状态: **测试成功且定位极其精准**。3 轮连续抓取流程中各点空间误差高度一致，重复精度极其优异。抓取下探点偏差在 `5.8 ~ 5.9 mm` 之间，放置下探点偏差在 `2.8 ~ 3.0 mm` 之间，无一触发越界，安全通过。

---

## 1. 终端日志记录

```text
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
-> 请将手轻轻拿开，按 Enter 键通电并执行安全回零...当前稳定关节角: [-1.05, -0.52, -0.7, -1.14, 0.43, 0.61]
相对 HOME_ANGLES 全轴偏差: [1.05, 0.52, 0.7, 1.14, 0.43, 0.61]
大臂 1-5 轴偏差: [1.05, 0.52, 0.7, 1.14, 0.43], arm_max_diff=1.1
第 6 轴末端旋转偏差: 0.6
arm_max_diff=1.1 <= 45.0，回直立零位...
  -> [V2.5 方向1] step11回零: 异步回零 send_angles(HOME, speed=30)，软到位循环 (tol=1.5°, timeout=1.5s)...
  -> [V2.5 方向1] step11回零: 软到位收敛 max_diff=1.14° <= 1.5°，耗时 0.01s（提前退出固件等待）。
  -> [V2.5 D0] step11 回零结果: status=auto, mode=async

-> 请输入本轮连续抓取次数（直接回车默认 1 次）: -> 将连续执行 3 次抓取任务（任一轮失败即中止，全程 Ctrl+C 急停）。
⚠️ 请确认：① pick 点已放好物块（多轮时由人持续补块）；② 机械臂轨迹范围内无障碍物。
-> 确认无误后按 Enter 开始连续抓取（此后 N 轮全自动，不再提示）...
########## 连续抓取 第 1/3 轮 ##########

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

⚠️ drop_hover -> home_ready 空中过渡路径需无障碍；如遇危险随时 Ctrl+C 急停。

1. 张开夹爪准备...
  -> 下发夹爪动作: 张开...
  -> 【信息】当前库无 get_gripper_value 反馈接口，使用开环定时等待。

2. 关节角回放到 pick_hover...
  -> 关节回放到 pick_hover: [43.94, -36.38, -69.08, 16.87, 1.23, 94.13]
  -> pick_hover 坐标校验 delta_xyz=6.6 mm

3. 短距离关节下探到 pick...
  -> 关节回放到 pick: [43.85, -60.02, -69.08, 37.44, -1.05, 92.54] (speed=12, timeout=10s)
  -> pick 坐标校验 delta_xyz=5.9 mm

4. 闭合夹爪抓取目标...
  -> 下发夹爪动作: 闭合...
  -> 【信息】当前库无 get_gripper_value 反馈接口，使用开环定时等待。

5. 短距离关节抬起回 pick_hover...
  -> [V2.8 异步] pick_hover: 非阻塞 send_angles (speed=16)，软到位循环 (tol=3.0°, timeout=4.0s)...
  -> [V2.8 异步] pick_hover: 软到位收敛 max_err=1.94° / delta_xyz=10.3mm <= 容差，耗时 1.27s（提前退出固件死区等待）。
  -> pick_hover 坐标校验 delta_xyz=10.7 mm
  -> [V2.2 耗时] step 5: 1.4s

6. 关节角回放到 drop_hover（空中长距离过渡）...
  -> 关节回放到 drop_hover: [1.23, -40.86, -69.25, 21.88, -6.85, 54.49]
  -> drop_hover 坐标校验 delta_xyz=5.5 mm

7. 短距离关节下降至 drop...
  -> 关节回放到 drop: [0.96, -61.08, -69.34, 42.01, -4.74, 51.06] (speed=12, timeout=10s)
  -> drop 坐标校验 delta_xyz=3.0 mm

8. 张开夹爪放置正方体...
  -> 下发夹爪动作: 张开...
  -> 【信息】当前库无 get_gripper_value 反馈接口，使用开环定时等待。

9. 短距离关节抬起回 drop_hover...
  -> [V2.8 异步] drop_hover: 非阻塞 send_angles (speed=16)，软到位循环 (tol=3.0°, timeout=4.0s)...
  -> [V2.8 异步] drop_hover: 软到位收敛 max_err=2.11° / delta_xyz=10.6mm <= 容差，耗时 1.14s（提前退出固件死区等待）。
  -> drop_hover 坐标校验 delta_xyz=10.9 mm
  -> [V2.2 耗时] step 9: 1.3s

10+11. 末段回零：drop_hover -> home_ready -> HOME...
  -> 即将把臂从 drop_hover 拉向接近直立的 home_ready 再回 HOME，请确认空中路径无障碍。
  -> [V2.5 方向2] 平滑过渡：非阻塞 send_angles(home_ready, speed=20)，接近阈值 5.0° 即提前下发 HOME...
  -> [V2.5 方向2] 已接近 home_ready (max_diff=1.84° <= 5.0°)，臂未停稳即下发 HOME 目标。
  -> [V2.5 方向2] HOME 软超时未收敛（max_diff=39.81°），阻塞 sync_send_angles 收尾兜底 (speed=25 [V2.4已验证], timeout=12s)...
  -> [V2.5 方向2] 收尾 sync_send_angles 返回 1
  -> [V2.5 方向2] sync 收尾后 max_diff=1.31°。
  -> [V2.2 耗时] step 10+11 (平滑过渡): 5.5s

====================================
🎉 V2.5 五点示教 + home_ready 回零过渡测试流程跑通！
   末段回零：0 轮人工扶正（home_ready -> HOME 自动回零）。
====================================

########## 连续抓取 第 2/3 轮 ##########

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

⚠️ drop_hover -> home_ready 空中过渡路径需无障碍；如遇危险随时 Ctrl+C 急停。

1. 张开夹爪准备...
  -> 下发夹爪动作: 张开...
  -> 【信息】当前库无 get_gripper_value 反馈接口，使用开环定时等待。

2. 关节角回放到 pick_hover...
  -> 关节回放到 pick_hover: [43.94, -36.38, -69.08, 16.87, 1.23, 94.13]
  -> pick_hover 坐标校验 delta_xyz=6.6 mm

3. 短距离关节下探到 pick...
  -> 关节回放到 pick: [43.85, -60.02, -69.08, 37.44, -1.05, 92.54] (speed=12, timeout=10s)
  -> pick 坐标校验 delta_xyz=5.8 mm

4. 闭合夹爪抓取目标...
  -> 下发夹爪动作: 闭合...
  -> 【信息】当前库无 get_gripper_value 反馈接口，使用开环定时等待。

5. 短距离关节抬起回 pick_hover...
  -> [V2.8 异步] pick_hover: 非阻塞 send_angles (speed=16)，软到位循环 (tol=3.0°, timeout=4.0s)...
  -> [V2.8 异步] pick_hover: 软到位收敛 max_err=1.94° / delta_xyz=10.7mm <= 容差，耗时 1.29s（提前退出固件死区等待）。
  -> pick_hover 坐标校验 delta_xyz=10.7 mm
  -> [V2.2 耗时] step 5: 1.5s

6. 关节角回放到 drop_hover（空中长距离过渡）...
  -> 关节回放到 drop_hover: [1.23, -40.86, -69.25, 21.88, -6.85, 54.49]
  -> drop_hover 坐标校验 delta_xyz=5.5 mm

7. 短距离关节下降至 drop...
  -> 关节回放到 drop: [0.96, -61.08, -69.34, 42.01, -4.74, 51.06] (speed=12, timeout=10s)
  -> drop 坐标校验 delta_xyz=3.0 mm

8. 张开夹爪放置正方体...
  -> 下发夹爪动作: 张开...
  -> 【信息】当前库无 get_gripper_value 反馈接口，使用开环定时等待。

9. 短距离关节抬起回 drop_hover...
  -> [V2.8 异步] drop_hover: 非阻塞 send_angles (speed=16)，软到位循环 (tol=3.0°, timeout=4.0s)...
  -> [V2.8 异步] drop_hover: 软到位收敛 max_err=2.11° / delta_xyz=10.5mm <= 容差，耗时 1.13s（提前退出固件死区等待）。
  -> drop_hover 坐标校验 delta_xyz=10.9 mm
  -> [V2.2 耗时] step 9: 1.3s

10+11. 末段回零：drop_hover -> home_ready -> HOME...
  -> 即将把臂从 drop_hover 拉向接近直立的 home_ready 再回 HOME，请确认空中路径无障碍。
  -> [V2.5 方向2] 平滑过渡：非阻塞 send_angles(home_ready, speed=20)，接近阈值 5.0° 即提前下发 HOME...
  -> [V2.5 方向2] 已接近 home_ready (max_diff=3.34° <= 5.0°)，臂未停稳即下发 HOME 目标。
  -> [V2.5 方向2] HOME 软到位收敛 max_diff=1.31° <= 1.5°，耗时 1.25s.
  -> [V2.2 耗时] step 10+11 (平滑过渡): 3.4s

====================================
🎉 V2.5 五点示教 + home_ready 回零过渡测试流程跑通！
   末段回零：0 轮人工扶正（home_ready -> HOME 自动回零）。
====================================

########## 连续抓取 第 3/3 轮 ##########

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

⚠️ drop_hover -> home_ready 空中过渡路径需无障碍；如遇危险随时 Ctrl+C 急停。

1. 张开夹爪准备...
  -> 下发夹爪动作: 张开...
  -> 【信息】当前库无 get_gripper_value 反馈接口，使用开环定时等待。

2. 关节角回放到 pick_hover...
  -> 关节回放到 pick_hover: [43.94, -36.38, -69.08, 16.87, 1.23, 94.13]
  -> pick_hover 坐标校验 delta_xyz=6.7 mm

3. 短距离关节下探到 pick...
  -> 关节回放到 pick: [43.85, -60.02, -69.08, 37.44, -1.05, 92.54] (speed=12, timeout=10s)
  -> pick 坐标校验 delta_xyz=5.8 mm

4. 闭合夹爪抓取目标...
  -> 下发夹爪动作: 闭合...
  -> 【信息】当前库无 get_gripper_value 反馈接口，使用开环定时等待。

5. 短距离关节抬起回 pick_hover...
  -> [V2.8 异步] pick_hover: 非阻塞 send_angles (speed=16)，软到位循环 (tol=3.0°, timeout=4.0s)...
  -> [V2.8 异步] pick_hover: 软到位收敛 max_err=1.94° / delta_xyz=10.7mm <= 容差，耗时 4.07s（提前退出固件死区等待）。
  -> pick_hover 坐标校验 delta_xyz=10.7 mm
  -> [V2.2 耗时] step 5: 4.2s

6. 关节角回放到 drop_hover（空中长距离过渡）...
  -> 关节回放到 drop_hover: [1.23, -40.86, -69.25, 21.88, -6.85, 54.49]
  -> drop_hover 坐标校验 delta_xyz=5.5 mm

7. 短距离关节下降至 drop...
  -> 关节回放到 drop: [0.96, -61.08, -69.34, 42.01, -4.74, 51.06] (speed=12, timeout=10s)
  -> drop 坐标校验 delta_xyz=2.8 mm

8. 张开夹爪放置正方体...
  -> 下发夹爪动作: 张开...
  -> 【信息】当前库无 get_gripper_value 反馈接口，使用开环定时等待。

9. 短距离关节抬起回 drop_hover...
  -> [V2.8 异步] drop_hover: 非阻塞 send_angles (speed=16)，软到位循环 (tol=3.0°, timeout=4.0s)...
  -> [V2.8 异步] drop_hover: 软到位收敛 max_err=2.11° / delta_xyz=10.9mm <= 容差，耗时 1.15s（提前退出固件死区等待）。
  -> drop_hover 坐标校验 delta_xyz=10.9 mm
  -> [V2.2 耗时] step 9: 1.3s

10+11. 末段回零：drop_hover -> home_ready -> HOME...
  -> 即将把臂从 drop_hover 拉向接近直立的 home_ready 再回 HOME，请确认空中路径无障碍。
  -> [V2.5 方向2] 平滑过渡：非阻塞 send_angles(home_ready, speed=20)，接近阈值 5.0° 即提前下发 HOME...
  -> [V2.5 方向2] 已接近 home_ready (max_diff=1.84° <= 5.0°)，臂未停稳即下发 HOME 目标。
  -> [V2.5 方向2] HOME 软到位收敛 max_diff=1.31° <= 1.5°，耗时 1.32s。
  -> [V2.2 耗时] step 10+11 (平滑过渡): 3.6s

====================================
🎉 V2.5 五点示教 + home_ready 回零过渡测试流程跑通！
   末段回零：0 轮人工扶正（home_ready -> HOME 自动回零）。
====================================
[V2.8 日志] 终端日志已保存: D:\第十届集创赛-雄芯院材料\mycobot_pc_tests\audit_logs\auto_run_20260708_154559.log
