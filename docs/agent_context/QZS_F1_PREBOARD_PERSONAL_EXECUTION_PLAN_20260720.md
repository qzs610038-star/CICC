# qzs F1 Preboard 个人执行方案

> 负责人：qzs
>
> 目标分支：`codex/qzs-f1-preboard-data-evidence-20260720`
>
> 状态上限：`QZS_PREBOARD_INPUTS_READY`；不得宣称板级 PASS

## 一、目标与交付边界

qzs 负责把真实采样、黄金特征、证据身份、H1 接板包和集成门做成可重复输入；
不实现 CPU 分类器，不修改 RTL/SoC，不占用 libaoxun，不修改冻结接口。

允许写入：`docs/**`、`competition_project_single_camera/docs/**`、
`competition_project_single_camera/tools/**`、`tools/**`。如任务自然要求写入
`cpu/**`、`src/**`、XML/IP/BSP 或冻结 `integration/**`，立即停下并形成 Review Packet。

## 二、任务分解

| 步骤 | 内容 | 依赖 | 预估有效工时 | 环境/空间 | 完成判据 |
|---|---|---|---:|---|---|
| Q0 | 从发布 SHA 建独立分支/工作区，记录基线与 dirty 状态 | 计划已推送 | 0.5h | Git/PowerShell；<100MB | branch、HEAD、scope 基线可审计 |
| Q1 | 落地 `QW-CALIBRATION-SAMPLE-v1` schema、模板、正负例和 validator | Q0 | 2h | Python 3.13；<100MB | 合法样例 PASS，缺字段/额外字段/hash 篡改 FAIL |
| Q2 | 固定相机采集协议与 batch 身份；先 45、后 135 样本 | Q1 | 4-8h | 相机/补光；原图预计 1-5GB，放 Git 外 | manifest 无绝对路径，hash/标签/覆盖率完整 |
| Q3 | 实现 Host 黄金特征提取与数据质量报告 | Q1,Q2 | 4h | Python；优先复用已有库，不自动联网安装 | 与冻结 RGB/ROI/mask/bbox/sum 语义一致，负例 fail-closed |
| Q4 | 生成交给 wsc 的固定 batch 和独立 holdout 划分 | Q3 | 1.5h | Python/PowerShell | 五文件 hash、object 隔离、样本覆盖表 PASS |
| Q5 | 准备 B0 UART 捕获 verifier 与 H1 Review Packet | Q1；不依赖 UART 当前可用 | 3h | PowerShell/Python | verifier 可校验固定自检 summary；H1 五 checkpoint 与停止条件齐全 |
| Q6 | 固定 wsc SHA，复验模型/回放/范围，建立双人集成候选 | Q4 + wsc W1-W4 | 3h | Git/Host 编译环境 | merge-tree、scope、fresh tests、tamper、未验证项均有记录 |

## 三、具体实施要求

### Q0：安全启动

1. 运行 handoff health check、`git status --short --branch`、`git worktree list`；
2. 从用户交接的最终发布 SHA新建 sibling worktree；
3. 运行 `tools/team_scope_check.ps1 -Role qzs -BaseRef <seed> -TargetRef HEAD`；
4. 不运行全量 `fetch --prune`；如需远端事实，用精确 ref 或 `git ls-remote`。

### Q1：schema 与 verifier

计划新增：

```text
competition_project_single_camera/docs/calibration/f1_preboard/
  README.md
  qw_calibration_sample_v1.schema.json
  templates/
competition_project_single_camera/tools/f1_preboard/
  validate_calibration_batch.py
  build_sha256_manifest.py
  tests/
```

validator 必须覆盖接口合同第 2 节全部不变量。测试临时产物放系统临时目录，
不在仓库留下 raw image、数据库或绝对路径。

### Q2：采集与标注

- 相机固定 J48/ch0 对应视角；记录高度、俯仰、焦距、曝光、白平衡、补光、背景、ROI；
- 每个 object 具有稳定 `object_id`，同一物体的重复帧不得跨 train/holdout；
- 优先完成 45 个 cube 样本以解锁尺寸探索，再补齐 135 和负例；
- 每次重拍不覆盖旧条目，用新 `sample_id` 并保留废弃原因；
- 任何标签不确定条目进 `NEGATIVE/HOLDOUT_REVIEW`，不得悄悄修标签迎合模型。

### Q3：Host 黄金特征

实现必须逐项对照冻结 feature contract：两像素字节序、闭区间 ROI、三色 mask、
背景差前景、`sum_luma=R+G+B` 累加、包含端点 bbox、无前景 bbox=0、21/31 位溢出。
板前输出统一标 `HOST_CALIBRATION_PROVISIONAL`。若缺少图像库，记录
`DEPENDENCY_BLOCKED` 并先完成 schema/manifest；不得静默换算法。

### Q4：向 wsc 发布 batch

一次发布一个只读 batch：batch_id、qzs SHA、capture profile hash、四个数据文件 hash、
样本覆盖、异常条目、holdout 列表。发布后修正数据必须新建 batch，不原地改写后
仍沿用旧 hash。

### Q5：接板准备

1. B0 verifier 只接受一条匹配 build/cases/pass/digest/arm=0 的 summary；重复、缺字段、
   digest 错误、额外 arm 字段或串口乱码均 FAIL；
2. H1 Packet 预写五级：tap -> snapshot/ACK/CDC -> CPU snapshot -> result/OSD -> input；
3. P0-B 文档只刷新事实为 `P0_A_READY=YES`，但仍因 USER2 实际失败条件/用户批准未满足而 `HOLD`；
4. 不执行 Efinity、USER2、UART 或板卡命令。

### Q6：审查和集成

只审查 wsc 固定 SHA。先 `git diff --name-only` 和 team scope，再 merge-tree/no-commit
探测，之后 fresh 跑受影响测试和篡改负例。建立新双人集成分支，不修改现有种子
和正式 main。结论必须逐项区分 Host、RISC-V profile、B0 board selftest、feature/APB、
OSD 与机械臂。

## 四、依赖图

```text
Q0 -> Q1 -> Q2 -> Q3 -> Q4 -------------------> Q6
       \-----------------> Q5 -----------------> Q6
                         wsc W1/W2/W3/W4 ------> Q6
```

Q5 可在没有相机数据、没有 UART 的情况下并行完成；Q6 必须等待 wsc 固定 SHA。

## 五、验收命令框架

实际脚本创建后将命令回填到本方案的执行记录或独立 evidence packet；最低包含：

```powershell
python competition_project_single_camera/tools/f1_preboard/validate_calibration_batch.py --batch <batch-dir>
python competition_project_single_camera/tools/f1_preboard/build_sha256_manifest.py --batch <batch-dir> --verify
powershell -NoProfile -ExecutionPolicy Bypass -File tools/interface_freeze_check.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/team_scope_check.ps1 -Role qzs -BaseRef <seed> -TargetRef HEAD
git diff --check <seed>..HEAD
```

必须另跑至少三个负例：修改一字节 artifact hash、复制 sample_id、同一 object 跨
train/holdout；三者都必须非零退出。

## 六、风险矩阵

| 风险 | 概率 | 影响 | 缓解/停止条件 |
|---|---|---|---|
| 相机/光照不固定导致标定漂移 | 中 | 阈值无法上板复用 | capture profile hash；变化即新 batch |
| 135 样本来不及 | 中 | 任务二证据不足 | 先 45 cube；明确降级，不虚报完整能力 |
| Host 黄金算法偏离 RTL | 中 | 上板同批快照不一致 | 逐字段语义测试；H1 以 FPGA snapshot 对照升级证据 |
| 原图误提交 Git | 中 | 仓库膨胀/隐私风险 | 默认 Git 外；只提交 manifest/hash/小型脱敏样例 |
| qzs 越权修改 CPU/RTL | 低 | 合并阻断 | team scope 每 checkpoint 执行；越界立即停止 |
| libaoxun 活动树被干扰 | 低 | UART 排障被打断 | 不进入其 worktree/ref，不发新任务，不拉中间 SHA |

## 七、qzs 完成口径

必做 Q0-Q5 通过后可写 `QZS_PREBOARD_INPUTS_READY=YES`。Q6 完成且 wsc 也通过后，
才可共同写 `F1_PREBOARD_RC=YES`。始终保留：

```text
BOARD_VERIFIED=NO
P0_B=HOLD
ARM_ENABLED=0
```
