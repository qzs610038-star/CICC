# myCobot PC 端实验续跑计划（同步最新 main 后）

日期：2026-07-14

分支：`codex/mycobot-experiment-main-sync-20260714`

基线：`origin/main@39e8a92`

## 1. 目标与边界

本计划只推进开发期 PC 端的 180°五点示教、外部基准记录和无夹爪单段诊断。正式比赛闭环仍由板上 CPU 负责；PC、`pymycobot` 和本计划生成的 JSON 不能进入正式识别/控制闭环。

本计划不授权机械臂动作。当前轮只允许离线检查和只读环境确认；任何释放舵机、关节运动、夹爪动作、接线或烧录都必须另获用户现场确认。

## 2. 已冻结基线

- 成功脚本归档：`mycobot_pc_tests/archive/teach_replay_pick_success_baseline_20260714_153337.py`，SHA-256 为 `A7859EC1A670DB25D8A069728C737EC874FC0A7EADD3EF85BB8EAD464C945913`，不得修改或直接续写。
- 当前诊断脚本：`mycobot_pc_tests/teach_replay_pick_180deg_teach_v1.py`，只提供五点示教、离线校验和无夹爪 probe。
- 旧候选：`mycobot_pc_tests/presets/teach_points_20260712_180deg.json`。它缺少当前安装状态下的完整外部基准证据；机械臂/底座一旦重装或加固，原点位即失效。因此它必须保持候选，离线 `--check-preset` 预期 FAIL。
- 板上 G0–G3 已进入主线，但制品仍为 `NOT_FOR_FLASH`；这不提高 PC 真机动作的放行等级。

## 3. 续跑阶段门

| 阶段 | 操作 | 通过条件 | 当前状态 |
|---|---|---|---|
| E0 基线 | 核对分支、提交和三个脚本哈希 | 分支来自 `39e8a92`；归档与原成功脚本哈希一致 | PASS |
| E1 离线门 | `py_compile`、`--help`、参数拒绝、旧预设校验 | 不连接串口；错误输入 fail closed；旧预设预期 FAIL | PASS |
| E2 只读环境 | 枚举串口、确认 `pymycobot` 可导入 | 只记录端口候选，不自动选 COM，不发命令 | PASS（仅发现蓝牙 COM4/COM5 候选） |
| E3 机械前置 | 基座刚性固定、安装版本标记、外部 180°基准、净空、急停/断电人、物块与台面 | 照片/视频、量角/量距方法和现场双人复核齐全 | **HARDWARE-BLOCKED**：用户确认现场硬件条件无法满足基座刚性固定与外部 180°基准测量；安全员在场，断电方式为直接拔插机械臂独立 12V 电源。E3 不再作为 E4 的强制前置门，改为“受控诊断 → 完整抓放”路线，用实际抓取结果反推是否需要回头做精确误差测定。风险接受边界：抓取失败可接受，夹爪损坏不可接受。 |
| E4 重新示教 | 使用 `--teach-only` 创建全新唯一命名 JSON | 明确 `--port` 和 `--confirm-action-gate`；不加载旧预设；不自动回零/抓放 | **GO（受控诊断优先）**：E3 硬件受限后，用户选择直接进入真机示教。安全员在场，断电方式为拔插 12V 电源。先跑受控诊断（单段、低速、无夹爪），再决定是否进入完整抓放。 |
| E5 外部证据 | 离线 `--annotate-preset` 后再 `--check-preset` | 释放点相对起点 `170°–190°`，最大臂展、基座位移和点位版本证据齐全 | NO-GO |
| E6 无夹爪探针 | 分别执行 `home_ready`、`pick_hover`、`drop_hover`，再决定 `pick/drop` 短段 | 每段独立日志；读回稳定；误差≤2°；基座无位移；每段后单独审查 | NO-GO |
| E7 后续动作设计 | 基于 E4–E6 证据新建脚本/点表，不修改成功基线 | 新 Review Packet 通过后才可讨论低速空载或夹爪 | NO-GO |

## 4. 本轮允许执行的命令

```powershell
$env:PYTHONPYCACHEPREFIX = Join-Path $env:TEMP "cicc_mycobot_pycache"
python -m py_compile mycobot_pc_tests\teach_replay_pick_180deg_teach_v1.py
python mycobot_pc_tests\teach_replay_pick_180deg_teach_v1.py --help
python mycobot_pc_tests\teach_replay_pick_180deg_teach_v1.py --check-preset 20260712_180deg
python -c "import serial.tools.list_ports as p; [print(x.device, x.description, x.hwid) for x in p.comports()]"
python -c "import importlib.util; print('pymycobot', bool(importlib.util.find_spec('pymycobot')))"
```

第三条应退出非零并打印 `[FAIL]`；这是旧预设被安全拒绝的通过证据，不是测试故障。

## 5. 获得现场动作许可后的严格顺序

1. 用户先确认基座、急停/断电、净空、速度上限、现场看护人和准确 COM 口。
2. 只运行一次 `--teach-only`，用新名字保存五点；不得覆盖任何旧 JSON。
3. 退出真机后，离线附加外部证据；只有 `--check-preset` PASS 才能申请 probe。
4. 每次只申请一个 probe，顺序为 `home_ready`、`pick_hover`、`drop_hover`、`pick`、`drop`；前一段日志未审查，不进入后一段。
5. 任一读回不稳、最大误差大于 2°、底座/安装标记位移、外部角度超界或碰撞风险出现，立即停止并使当前预设失效。
6. E6 完成后另开新脚本/点表设计；成功基线归档仍不修改。

## 6. 与板上路线的接口

PC 端通过只会产生“候选点位和动作顺序证据”。转换为板上 CPU 点表前，仍需逐关节限位、单位/缩放、超时、single-flight、停止/读回、夹爪确认和安全互锁审查。G4–G11 必须继续按 `mycobot_cpu_board_bringup_implementation_plan_20260714.md` 独立验收，不能引用 PC PASS 跳门。

单摄候选工程 `competition_project_single_camera/` 尚未完成新构建、匹配 bitstream、3 次冷启动和 10 分钟画面复现；在它升格前，机械臂实验不得依赖该工程提供真实识别触发。
