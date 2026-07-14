# myCobot 180°点位示教脚本 Codex Review Packet

日期：2026-07-14
审查者：Codex  ǀ 范围：PC 端开发期示教/受控诊断脚本

## 1. 目标与结论

目标是保留原有成功脚本，创建独立的 180°五点示教工具，为后续 `pick_hover / pick / drop_hover / drop / home_ready` 点位确定提供候选 JSON。

结论：代码级审查通过，可进入“离线校验 → 现场人工确认 → 单点低速诊断”的准备阶段；尚未获得机械臂动作许可，不能据此宣称 180°定位或比赛抓放闭环已验证。

## 2. 文件与基线

- 原脚本：`mycobot_pc_tests/teach_replay_pick.py`
- 原脚本归档：`mycobot_pc_tests/archive/teach_replay_pick_success_baseline_20260714_153337.py`
- 新脚本：`mycobot_pc_tests/teach_replay_pick_180deg_teach_v1.py`
- 原脚本当前 SHA256：`A7859EC1A670DB25D8A069728C737EC874FC0A7EADD3EF85BB8EAD464C945913`
- 归档 SHA256：与原脚本一致

新脚本不修改原脚本，不包含自动抓放主流程，不向板上正式点位表写入数据。

## 3. 已落实的安全门

- 串口号必须显式传入；不自动扫描 COM。
- 真机模式必须显式传入 `--confirm-action-gate`。
- `--teach-only` 只能保存新 JSON，不能加载旧点位。
- 探针只发送关节角运动，不发送夹爪命令。
- HOME 前先读当前角度；偏离 HOME 超过 45°时脚本拒绝自动回零。
- 每个运动段都要求 `sync_send_angles` 返回成功且读回最大关节误差不超过 2°；失败立即中止。
- `drop_hover/drop` 真机探针要求点位已经完成外部基准证据校验；候选点不能直接进入 drop 路径。
- 保留原始 stdout 审计日志；异常/中断只尝试发送 `stop`，不自动继续或自动抓放。
- 离线 `--check-preset` 不连接串口，且要求外部基准证据完整。

## 4. 离线验证证据

在仓库根目录执行并通过：

```powershell
python -m py_compile mycobot_pc_tests\teach_replay_pick_180deg_teach_v1.py
python mycobot_pc_tests\teach_replay_pick_180deg_teach_v1.py --help
```

另外已验证：

- 缺少 `--port` 或 `--confirm-action-gate` 时，参数层退出，不连接机械臂。
- `--teach-only` 缺少 `--save-preset` 时，参数层退出。
- 旧候选点位 `20260712_180deg` 的 `--check-preset` 返回 FAIL，未被误当成正式合格点位。
- 合成的五点 + 完整外部证据通过 `validate_preset(require_external=True)`。
- 静态检查未发现旧的 Tee 类、异步回零/自动阶段函数、夹爪命令或笛卡尔坐标运动命令。
- 原脚本工作树无 diff，归档哈希与原脚本一致。

## 5. 尚未完成的门

以下事项未完成前，禁止执行现场真机示教/探针：

1. 按 `mycobot_board_bringup_operator_sop_20260712.md` 完成当前 T0-C/基座与外部基准检查。
2. 明确底座标记、安装版本、外部量角/量距方法，并留下前后照片或视频。
3. 在现场净空、急停可用、低速、人工看护条件下，先做单段 HOME→`home_ready`，再由 Codex 审查日志决定是否继续。
4. 外部测得释放点相对起点旋转 `170°–190°`，并确认释放点位于最大臂展约束内；内部 `get_coords` 读数只能作为辅助，不能替代外部证据。
5. 点位通过后，另行生成板上 CPU 候选点表；PC 点位/脚本不能进入正式识别与控制闭环。

## 6. 建议现场顺序

先运行离线 `--check-preset` 或完成 `--annotate-preset`，再提交带日志、照片/视频和外部测量记录的现场 Review Packet。获得动作许可后，只按单段探针顺序推进：

```text
HOME → home_ready
HOME → pick_hover → HOME
HOME → drop_hover → HOME
必要时再单独验证 pick / drop 的短段路径
```

任何读回不稳定、角度误差超过 2°、基座标记发生位移、夹具/台面存在碰撞风险时，立即停止并回退，不进入抓取释放测试。
