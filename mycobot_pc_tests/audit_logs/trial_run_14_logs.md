# 第十四次试运行（2026-07-07 V2.3 串口参数冲突排查 + my_new_test 示教与预设复用）

> 脚本: `mycobot_pc_tests/teach_replay_pick_return_ready.py` (V2.3)
> 日期: 2026-07-07
> 上一轮 (Run-13): 预设复用及稳定参数验证（待填/历史对照）。
> 本轮状态: **测试成功**。排查并绕过了参数解析 Bug，完成了新点位示教、保存为 `my_new_test`，并使用预设成功复用运行了第二次抓取实验。

---

## 1. 串口参数 Bug 报错与排查记录

### 1.1 错误现象
在不指定 `--port` 参数，且指定 `--save-preset my_new_test` 参数运行脚本时，程序报错：
```text
PS D:\第十届集创赛-雄芯院材料> python mycobot_pc_tests/teach_replay_pick_return_ready.py --save-preset my_new_test
尝试连接机械臂 (--save-preset @ 1000000)...
Note: This class is no longer maintained since v3.6.0, please refer to the project documentation: https://github.com/elephantrobotics/pymycobot/blob/main/README.md

🚨 [异常] 运行中发生错误或运动超时: could not open port '--save-preset': FileNotFoundError(2, '系统找不到指定的文件。', None, 2)
-> 请用手扶稳机械臂/物块防止下沉/掉落，扶稳后按 Enter 释放舵机...
```

### 1.2 原因剖析
*   在 `mycobot_pc_tests/teach_replay_pick_return_ready.py` 中的 `get_port()` 提取逻辑为：
    ```python
    def get_port():
        if len(sys.argv) > 1:
            return sys.argv[1]
    ```
*   由于未指定可选参数 `--port`，`args.port` 为 `None`，进而触发了 `get_port()` 自动探测。
*   由于命令行参数为 `['teach_replay_pick_return_ready.py', '--save-preset', 'my_new_test']`，参数个数大于 1，`sys.argv[1]`（即 `"--save-preset"`）被错误地作为端口名返回，引发连接异常。

### 1.3 临避方案
运行命令时显式指定 `--port <PORT>`。例如本轮实验指定为 `--port COM9`。

---

## 2. 第一次实验：手动录入与预设保存

### 2.1 运行命令
```powershell
python mycobot_pc_tests/teach_replay_pick_return_ready.py --port COM9 --save-preset my_new_test
```

### 2.2 示教调试过程
1.  **连接状态**：成功建立 COM9 通信。
2.  **点位录入**：
    *   `pick_hover`: $R=242.3\text{mm}$（处于推荐带载区），角度录入成功。
    *   `pick`: $R=252.1\text{mm}$（处于可接受调试区），角度录入成功。
    *   `drop_hover`: $R=248.5\text{mm}$（处于推荐带载区），角度录入成功.
    *   `drop`: $R=255.3\text{mm}$（处于可接受调试区），角度录入成功.
3.  **安全回零点 `home_ready` 重新对准**：
    *   **首次读取**：$R=215.2\text{mm}, Z=332.1\text{mm}$，角度为 `[-3.77, -44.56, 15.99, -10.37, 1.31, 38.93]`。计算得出 `arm_max_diff=44.6`，触及并警告在 `40.0~45.0` 的余量不足区间。
    *   **手动纠偏**：用户选择重示教 `y`，手动拖动使大臂更直立。
    *   **二次读取**：$R=191.2\text{mm}, Z=352.7\text{mm}$，角度为 `[-3.77, -24.43, -9.31, -2.9, -0.08, 38.93]`。计算得出 `arm_max_diff=24.4`。顺利通过 $\le 40.0$ 的推荐余量门，留出 $20.6^\circ$ 软到位余量。
4.  **预设保存**：顺利保存至 `D:\第十届集创赛-雄芯院材料\mycobot_pc_tests\presets\teach_points_my_new_test.json`。

### 2.3 自动执行阶段日志
*   **短距离校验**：4对短距离及过渡关节连续性校验通过。
*   **Step 5 抬起**：关节回放到 pick_hover，耗时 $3.4\text{s}$，坐标校验 $\delta_{xyz}=10.7\text{mm}$。
*   **Step 9 抬起**：关节回放到 drop_hover，`sync_send_angles` 返回 0，残差 $\text{max\_err}=2.1^\circ$ / 坐标差 $\delta_{xyz}=10.9\text{mm}$ 触发二次微调。微调后仍为该偏差，走旧软到位兜底。耗时 $23.0\text{s}$，坐标校验 $\delta_{xyz}=10.9\text{mm}$ 通过。
*   **Step 10 过渡**：过渡到 home_ready 顺利，耗时 $2.6\text{s}$。
*   **Step 11 回零**：`safe_return_home` 计算大臂偏差仅为 $26.3^\circ$（在 $45^\circ$ 安全门内），走 `auto` 自动回零，耗时 $2.4\text{s}$。
*   **结论**：测试全流程跑通，0轮人工扶正。

---

## 3. 第二次实验：预设复用回放

### 3.1 运行命令
```powershell
python mycobot_pc_tests/teach_replay_pick_return_ready.py --port COM9 --preset my_new_test
```

### 3.2 预设安全门校验
*   预设 `my_new_test` 加载成功。
*   校验各点业务范围、带载分区（$R$ 在可接受/推荐区）、`home_ready` 直立余量限制（$24.4 \le 40$）、过渡连续性，全部校验通过，顺利进入自动抓取。

### 3.3 自动复现阶段日志
*   **Step 5**：耗时 $1.4\text{s}$，校验 $\delta_{xyz}=10.7\text{mm}$。
*   **Step 9**：`sync_send_angles` 返回 0，再次检测到 $\text{max\_err}=2.1^\circ$ / $\delta_{xyz}=10.9\text{mm}$，触发微调。微调后兜底软通过，耗时 $23.0\text{s}$，坐标校验 $\delta_{xyz}=10.9\text{mm}$ 通过。
*   **Step 10**：耗时 $2.6\text{s}$，$\delta_{xyz}=10.1\text{mm}$。
*   **Step 11**：计算实际大臂最大角差为 $26.3^\circ$，低速同步自动回零成功，耗时 $2.0\text{s}$。

### 3.4 结论
全流程跑通，0轮人工扶正。复用预设免去了手动拖动的过程，且前后两次运行中机械臂表现出高度的动作重合度与精度可重复性。

---

## 4. 后续修改与优化建议（待实施）
在 `mycobot_pc_tests/teach_replay_pick_return_ready.py` 的 get_port() 中进行修复，避开无条件提取 `sys.argv[1]` 作为端口。
可以使用 `argparse` 中已解析的属性，或者过滤掉所有以 `-` 开头的参数。例如：
```python
def get_port():
    # 过滤掉非串口设备或命令行可选参数
    args_list = [x for x in sys.argv[1:] if not x.startswith("-")]
    # 如果除去可选参数和其关联值外，确实有裸参数传入，再作为 port 兼容
    ...
```
方案需经过审查后再决定是否实施。
