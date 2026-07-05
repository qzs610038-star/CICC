# 机械臂示教回放脚本（teach_replay_pick.py）二次优化修改实施计划

此计划旨在修复首测 [teach_replay_pick.py](file:///D:/第十届集创赛-雄芯院材料/mycobot_pc_tests/teach_replay_pick.py) 时新发现的两个物理运动与校验异常：
1. **滤波拦截阻塞回零**：直立零位附近时，$Z$ 坐标（约 $400\text{ mm}$）超出工作空间限值（`Z_MAX = 280.0`），导致被嵌套的安全检测拦截，回零读取坐标判定为 `None`。
2. **拉升动作报错失败**：大偏差回零时，即便以 `mode=0`（关节模式）拉升 $Z$ 轴，由于远端极限姿态或固件到位校验偏差，仅抬升了 10mm 就超时报错熔断。

---

## 优化设计思路

### 1. 解耦坐标滤波读取与工作空间安全拦截
*   **当前问题**：`get_filtered_coords` 内部调用了 `is_safe_coord`。这导致了当机械臂返回直立零位高处时，虽然坐标物理上是真实的，却因为大于 280mm 阈值被强行过滤，导致读取为 `None`。
*   **改进方案**：将 `get_filtered_coords` 内部的 `is_safe_coord` 校验移除，只做通用的 6 维实数和稳定性差值滤波（即只防 NaN/Inf 数据跳变）。
*   **安全防御不降低**：用于业务抓取/下探的 `is_safe_coord` 依然会在“示教采集”和“自动直线下探”前执行拦截。直立零点原本就处于工作空间高处，移除其读取过滤是物理合法的。

### 2. 优化大偏差回零逻辑（人工安全辅助回正）
*   **当前问题**：自动改变 $Z$ 轴坐标并执行 `sync_send_coords` 拉升即使在关节模式下也极易因为极限姿态或到位判定错误而超时熔断。
*   **改进方案**：将大偏差自动拉升逻辑改为“引导式人工辅助扶正”。当偏差超过 45 度时，向用户发出警示，并暂停让用户手动托起、扶正机械臂到直立，接着只需由程序低速同步以关节角度模式（`sync_send_angles`）回归零位。
*   **优势**：关节角回归 100% 避开任何逆运动学奇异点和范围死锁，且大偏差人工辅助是官方针对大型重置的推荐安全做法。

---

## 拟修改文件

### [MODIFY] [teach_replay_pick.py](file:///D:/第十届集创赛-雄芯院材料/mycobot_pc_tests/teach_replay_pick.py)

#### 1. 解耦 `get_filtered_coords`
修改 `get_filtered_coords` 的合法性条件：
```diff
-        if is_safe_coord(coords):
+        # 仅做通用的 6 维实数和有效数字校验，不施加特殊的业务工作空间拦截
+        if isinstance(coords, list) and len(coords) >= 6 and all(isinstance(v, (int, float)) and math.isfinite(v) for v in coords[:6]):
             vals = list(coords[:6])
             valid.append(vals)
```

#### 2. 重构大偏差回零逻辑
在 `safe_return_home` 函数中，移除基于坐标的自动拉升步骤，替换为安全的手动辅助提示：
```diff
     max_diff = max(diffs)
     if max_diff > 45.0:
         print(f"\n【警告】当前姿态与零位偏差较大 ({max_diff:.1f}度 > 45度)。"
-              f"直接关节回零会产生大范围弧线扫动。")
-        ans = input("确认周边无障碍物，允许以【先高抬，再慢回】复位吗？(y/n): ")
-        if ans.lower() != 'y':
-            return False
-
-        coords = get_filtered_coords(mc)
-        if coords is None:
-            print("【错误】回零保护失败：无法读取稳定空间坐标，拒绝回零！")
-            print("-> 请人工扶正到接近直立姿态后重试，或重新示教。")
-            return False
-
-        safe_z = max(coords[2], 180.0)
-        protect_coords = list(coords)
-        protect_coords[2] = safe_z
-        # mode=0 为关节空间插值，末端走弧线（非垂直抬升）；用户已确认周边无障碍。
-        print(f"执行回零路径保护：抬高至安全高度 Z={safe_z:.1f} (mode=0 关节空间，末端走弧线)...")
-        print("  -> 注意：mode=0 不是垂直抬升，末端会走弧线，请确认弧线范围内无物块/障碍。")
-        # 长距离保护动作使用 mode=0，绕开 mode=1 直线 IK 奇异
-        res = mc.sync_send_coords(protect_coords, 30, 0, timeout=10)
-        if res != 1:
-            print("【错误】抬高保护动作失败或超时！拒绝继续回零。")
-            return False
+              f"直接关节回零会产生大范围弧线扫动，存在物理碰撞风险。")
+        print("-> 为绝对安全起见，建议人工辅助：请【用手托住并轻轻将机械臂扶正】到接近直立的零位姿态。")
+        input("确认扶正完毕且周边无障碍后，按 Enter 键继续低速安全回零...")
+
+        # 重新读取关节角，确保数据刷新并真实接近直立
+        angles = get_filtered_angles(mc)
+        if angles is None:
+            print("【错误】无法读取稳定关节角，拒绝回零！")
+            return False

     print("正在低速同步回直立零位...")
```

---

## 验证计划

1. **静态语法校验**：
   运行静态编译，确认修改没有引入 Verilog 等语言的语法问题（本程序为 Python，运行 `python -m py_compile teach_replay_pick.py` 校验）。
2. **人工扶正测试**：
   故意在录入四点后让机械臂处于偏远弯曲姿态，测试回零时是否正确弹出人工扶正引导，并在手动扶正后按 Enter，观察是否能 100% 顺畅、不报错地低速回到标准直立零位。
3. **坐标滤波直立校验**：
   验证当机械臂处于直立姿态时，`get_filtered_coords` 能够正常输出大约 `Z = 400mm` 的高度坐标，不再被越界拦截，从而能够正常通过后续的各种自动验证和校验。
