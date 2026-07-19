# WSC / QZS 跨成员固定 SHA 集成审查

> 治理侧：`codex/qzs-p0a-p1-host-governance-20260719@fce2cf461ea9bc48f786a70f3276c7e4a84658e7`
>
> CPU/P0-A/P1 Host 侧：`codex/wsc-p1-evidence-packaging-20260719@aaf2058e7e05b8cda905b8096db309c65e0da5ef`
>
> merge-base：`0e5ab490559c58642734b0095753c6cf8787c709`
>
> 合成树：`e689cd5395bcd8b65b0fa1b59f3b491c52c8d08a`
>
> 裁定：`APPROVE FOR MERGE / MERGE NOT PERFORMED`

## 1. 固定来源与范围

- `git ls-remote` 已确认两条远端 ref 分别固定到上述完整 SHA。
- qzs 与 wsc 均直接从 kickoff 分叉；`git rev-list --left-right --count qzs...wsc` 为 `7 10`。
- qzs 相对 kickoff 改动 21 个治理文件，qzs scope PASS；wsc 相对 kickoff 改动 41 个 CPU/P0-A 文件，wsc scope PASS。
- 两侧路径交集为 0；没有整文件覆盖、文本冲突或同文件语义冲突。
- `git merge-tree --write-tree` exit 0，生成 `e689cd5395bcd8b65b0fa1b59f3b491c52c8d08a`。
- 独立 detached 工作树执行 `git merge --no-commit --no-ff` 同样无冲突，`git write-tree` 与 merge-tree 精确一致；未形成提交或移动任何成员 ref。

## 2. 所有权与硬边界

- 合成树相对 qzs 侧新增/修改 41 个文件，全部通过 `team_scope_check -Role wsc`。
- 冻结 `cpu/include/**`、`integration/**`、RTL、XML/peri.xml、SDC、IP 零差异。
- 没有新增 ELF/map/log/bit/bin/hex 制品。
- qzs 治理、状态、证据工具保持 qzs 来源；CPU/P0-A/P1 Host 实现和测试保持 wsc 来源。
- 本审查没有读取、拉取、切换、提交或通知 libaoxun 的活动分支，也没有要求其暂停 UART1/USER2 实验。

## 3. 合成树 fresh 验证

| Gate | 结果 |
|---|---|
| P0-A Host TX-never-ready | `10/10 PASS`，重复 fresh 一次仍 `10/10` |
| P1 Host model | `37/37 PASS` |
| feature adapter | `39/39 PASS` |
| classifier | `54/54 PASS` |
| F1 | `213/213 PASS` |
| committed P1 manifest | `P1_MANIFEST_HASHES=PASS files=4 policy=LF_NORMALIZED` |
| manifest 单文件篡改 | `P1_MANIFEST_TAMPER_NEGATIVE=PASS` |
| P1 runtime replay | schema `11/11`、rounds `20/20`、snapshot tamper PASS、`ARM=0` |
| runtime/G2 | `648/648 PASS`、`VALIDATION_PASS` |
| myCobot skeleton QEMU | PASS；不等于机械臂动作验证 |
| interface freeze | PASS，8 files / 1 surface / UART1_TYPEC |
| offline presubmit | `PASS_WITH_WARNINGS`，freshness WARN=9、FAIL=0 |
| `git diff --check` | PASS |

freshness 的额外 1 个 WARN 来自模拟合并工作树 intentionally dirty；其余为既有 freshness/CBM 警告，没有新增 FAIL。

## 4. 结论边界

```text
CROSS_MEMBER_REVIEW=APPROVE_FOR_MERGE
QZS_SHA=fce2cf461ea9bc48f786a70f3276c7e4a84658e7
WSC_SHA=aaf2058e7e05b8cda905b8096db309c65e0da5ef
MERGE_TREE=e689cd5395bcd8b65b0fa1b59f3b491c52c8d08a
TEXT_CONFLICTS=0
PATH_OVERLAP=0
P0_A_READY=YES
P1_HOST_READY=YES
MERGE_PERFORMED=NO
BOARD_VERIFIED=NO
P0_B=HOLD
ARM_ENABLED=0
```

本裁定允许下一步在专用集成分支合并这两个固定 SHA；不允许动态跟随分支头，也不授权直接合入 `main`、USER2、上板、UART2/J52 或机械臂动作。若真实合并前任一来源 SHA 变化，必须重跑差分审查；若固定 SHA 不变，可复用本次冲突与组合回归结果。

libaoxun 继续其现有 UART1/USER2 关键实验。本次 wsc/qzs 合并不需要其拉取、审阅或协调；涉及其 Hard SoC 原子输入的后续三方集成必须等其主动提供固定 SHA 后另行审查。
