---
name: pc_mycobot_debug
description: 总结 myCobot 280 在 PC 端使用 Python（pymycobot）进行免牵引示教、JSON预设点位加载、串口Bug规避以及异步软到位平滑回零的调试流程与参数规范。
---

# myCobot 280 PC端免手动引导与快速复用调试指南

本技能指南归纳了使用 PC 端 Python 库 `pymycobot` 对大象机器人 myCobot 280 机械臂进行健康度测试、五点示教回放以及快速复用调试的标准步骤与参数规范。用于指导未来相关 Agent 快速避坑、实现高效的实机动作链路校验。

---

## 1. 串口配置与可选参数 Bug 避坑

### 1.1 串口劫持 Bug 背景
在老旧的 `get_port()` 代码风格中，如果用户在运行脚本时指定了其他可选参数（例如 `--preset` 或 `--save-preset`），但未通过命令行指定 `--port`，脚本可能会盲读命令行第一个参数并误将其当做串口设备名（报 `could not open port '--save-preset'` 等错误）。

### 1.2 避坑调用规范
*   **规则 1**：在使用任何可选参数启动回放脚本时，**必须显式提供 `--port` 选项**指定实际识别到的 COM 口：
    ```powershell
    python mycobot_pc_tests/teach_replay_pick_return_ready.py --port COM9 --preset my_new_test
    ```
*   **规则 2**：如果不带任何命令行参数，可以利用程序的自动枚举与交互功能选择串口：
    ```powershell
    python mycobot_pc_tests/teach_replay_pick_return_ready.py
    ```

---

## 2. PC端免手动引导：预设复用调试法

### 2.1 预设的生成与存储
利用 `--save-preset <name>` 将拖拽示教记录的五点信息及安全元数据，持久化保存为 JSON 文件：
*   **预设目录**：`mycobot_pc_tests/presets/`
*   **文件命名**：`teach_points_<name>.json`
*   **存储元数据**：包含各个点的关节角、三维笛卡尔坐标、工作半径分区（带载安全评估）及 `home_ready` 回零安全差值。

### 2.2 预设复用执行流
通过 `--preset <name>` 引用已录制的点位直接进行抓取复现，彻底消除手动示教变量：
1.  **安全门前置校验（不可因预设跳过）**：
    *   **业务范围门**：通过 `is_safe_coord` 限制工作半径在物理极限 $R \le 280\text{mm}$ 内，并对 $250\text{mm} < R \le 260\text{mm}$ （可接受调试区）和 $R > 260\text{mm}$（边缘风险区）进行警示。
    *   **过渡连续性门**：利用 `validate_short_angle_pair` 校验相邻点位关节最大差值（大臂 1-5 轴 $\le 30^\circ$，第6轴腕部 $\le 45^\circ$），防止逆运动学多解导致回放时大幅摆动。
    *   **回零门限校验**：使用 `validate_return_ready` 检查回零过渡点 `home_ready` 的大臂偏差是否满足 `arm_max_diff <= 45°` 的安全门槛。
2.  **安全轨迹放行**：提示用户确认轨迹无障碍，按 `Enter` 键即可一键启动平滑回放。

---

## 3. 异步软到位与平滑过渡提速设计（V2.6 标准）

为消除传统回零（分段阻塞 `sync_send_angles`）带来的机械刹车停顿以及固件到位判定滞后（空等 $\approx 1\text{s}$），必须应用以下 V2.6 标准参数与控制逻辑：

### 3.1 核心常量规范
```python
HOME_RETURN_ASYNC_ENABLE = True       # 启用异步回零
HOME_RETURN_ASYNC_SPEED = 30          # 异步段最终回零速度
HOME_RETURN_ASYNC_SOFT_TOL = 1.5      # 软到位判定阈值（度），宽于 1.0 以适配 myCobot 机械精度波动死区 (实测 1.31° 左右)
HOME_RETURN_ASYNC_TIMEOUT = 1.5       # 软超时时长（s），缩短无意义空等
SMOOTH_HANDOFF_ENABLE = True          # 启用过渡打断
SMOOTH_HANDOFF_NEAR_TOL = 5.0         # 接近中间姿态打断阈值（度）
```

### 3.2 控制逻辑与安全兜底设计
1.  **单次角度轮询**：
    在异步进度轮询中，严禁使用带两次读取稳定性滤波的 `get_filtered_angles`（会导致运动中读取失败卡死），必须使用单次数据读取 `get_angles_once(mc)` 快速提取残余角差。
2.  **平滑手势打断（Step 10+11 合并）**：
    下发过渡指令（`drop_hover -> home_ready`）后，在臂未停稳、且角差 $\le 5.0^\circ$ 时，不执行物理停顿，**立即非阻塞下发最终的 `HOME` 指令**。
3.  **异步软到位判定与 Break**：
    在下发 `HOME` 后，持续轮询。一旦偏差 $\le 1.5^\circ$ 判定软到位收敛，立即 `break` 退出并返回成功。
4.  **安全失败防御（防级联故障）**：
    如果轮询超过 `1.5s` 仍未收敛（如发生摩擦受阻），**决不能在半空异常掉电**。必须利用阻塞的 `sync_send_angles(HOME_ANGLES, speed=25, timeout=12s)` 执行最终收尾：
    *   *注意*：收尾速度必须使用经 V2.4 实测安全验证的 `25`（而非 `30`），防止由于高速引起级联判定失败。
    *   收尾返回非 1 则报告 `failed`，由主循环安全制动（人工扶稳后 release），确保物理安全。

---

## 4. 常用调试工作流推荐

当面临机械臂 PC 端联调任务时，建议采取以下执行顺序：
1.  **物理零点确认**：使用 `test_mycobot.py` 查看当前只读信息。若关节有偏移，使用 `calibrate_zero.py` 进行刻度对齐并写零，注意必须**重新插拔电源**使其生效。
2.  **夹爪检测**：通过 `test_gripper.py` 验证开合响应与固件版本接口。
3.  **点位抓取测试**：使用本调试技能，调用 `teach_replay_pick_return_ready.py` 进行预设加载或示教，记录日志于 `trial_run_*_logs.md`。
4.  **紧急泄力**：调试完毕或发生干涉时，随时使用 `stop_mycobot.py` 释放舵机扭矩，注意托扶防臂坠落。
