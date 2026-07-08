# V2 Codex 复核结论与 Priority-3 迁移前置

> 本文沉淀 Codex 对 myCobot PC 调试脚本 V2.8~V2.12 改动链的独立复核结论，
> 作为板上 CPU 侧迁移设计的事实依据。**Priority-3 设计文档必须以本文 Codex 三项裁
> 定为准**，不以 Claude/Gemini 叙事为准。
>
> 日期：2026-07-08 ｜ 来源：Codex 复核(codex-rescue) + Claude 据实测日志交叉核查
> 关联：`mycobot_pc_tests/teach_replay_pick_return_ready.py`、
>       `CURRENT_STATE.md`、`trial_run_2[3-5]_logs.md`、`auto_run_20260708_*.log`

---

## 0. Codex 复核范围与裁定总览

Codex 以只读方式复核 [teach_replay_pick_return_ready.py](../../mycobot_pc_tests/teach_replay_pick_return_ready.py)，
覆盖三项问题，未修改文件。三项独立裁定为：

| 项 | 主题 | Codex 裁定 | 落地版本 |
|----|------|-----------|---------|
| (A) | V2.11 EOF→release 兜底修复 | **Safe** | V2.11 |
| (B) | confirm=2 遇 `get_angles_once` 持续 None/瞬态跳变 | **Safe** | V2.12 加 none_count 诊断(不改 confirm 行为) |
| (C) | 缺陷B：sync 兜底 res==0 直接熔断 | **Insufficient evidence** → 已补 post-failure 复读+有界重试 | V2.12 |

**重要**：之前 Gemini 在多份 run 归档与 `CURRENT_STATE.md` 路线覆盖项里把 "串口拥堵/缓冲
区溢出/高频轮询导致 res=0、需强制 50-100ms 降频" 写成 Codex 裁定一。**这是事实错误**。
Codex 未做"通信降频防溢出"裁定——Codex 在 (C) 里明确：run-23 根因优先级是"固件假失败/
确认死区"，并标注"日志未见串口异常文本，证据弱"。该叙事属已在 run-23 复核阶段被 Claude
证据否决的旧假设，Priority-3 文档不得恢复该归因。

---

## 1. Codex (A) — V2.11 EOF→release 兜底修复：Safe

**源码位置**：`main()` 异常路径 `L2170-L2215`，V2.11 修复核心 `L2194-L2214`。

**Codex 裁定要点**：
- `TeachAbort`(#L2170-2174)、顶层 `KeyboardInterrupt`(L2175-2181)各自处理，安全。
- 普通 `Exception` 且 `mc is None`(L2182-2188)：只打印异常不 release，可接受（连接前异常）。
- `input()` 正常 Enter(L2199-2200→finally L2208 release)：保留"扶稳后释放"原语义。
- `input()` 抛 `EOFError`：L2201-2203 标记 → finally 立即 release 打印 EOF 兜底——这是
  run-23 截断点的正确修复方向。
- `input()` 时 Ctrl+C：L2201 捕获并立即 release，等价急停释放。
- 硬件安全目标达成。

**Codex 指出的遗留(既有设计，非 V2.11 引入)**：
- L2182-L2214 把任意普通异常转成"打印一行 + release"，不重抛、无 traceback，降低定位质量。
- `input()` 抛非 EOF/KeyboardInterrupt 异常（如 stdin 层 OSError）不会被 L2201 捕获，但
  finally L2204-L2214 仍会 release；随后新异常外传；日志可能误打"人工扶稳后释放"。

**处置**：不在迁移前修。属既有设计，只影响调试可观测性、不影响硬件安全。
记入 Priority-3 文档"已知可观测性遗留"清单，留给板上 CPU 侧系统性实现
（C 侧可统一异常分类 + 日志带 type/traceback + 非交互启动的 EOF 兜底）。

---

## 2. Codex (B) — confirm=2 遇 get_angles_once 持续 None/瞬态跳变：Safe

**源码位置**：`get_angles_once()` L594-L616（异常/非法返回 None）；
短距离异步 confirm 循环 L973-L1025（已 V2.12 加 none_count）；
回零异步 confirm 循环 L1448-L1495；
平滑回零阶段 A/B L1528-L1602。

**Codex 裁定要点**：
- `get_angles_once()` 不抛读数异常：`mc.get_angles()` 异常直接 return None，非法长度/
  非数值/越界也 None（L605-L616）。
- 各异步循环有硬 timeout 兜底，持续 None 不会无限循环，最终走 sync 兜底：短距离
  ASYNC_SHORT_TIMEOUT=4.0 (L982)，回零 HOME_RETURN_ASYNC_TIMEOUT=1.5 (L1456-1470)，
  平滑回零阶段 A 最多等 ANG_REPLAY_TIMEOUT=20s 后回退分段 (L1548-1559)、阶段 B 最多等
  1.5s 后 sync 兜底 (L1585-1596)。
- **关键语义细节**：None 不重置 confirm_count —— 实际语义是"连续有效 OK 读数"而非
  严格"连续轮询 OK 读数"，偶发一帧 None 被容差（优点，不是缺陷）。

**Codex 推荐修复方向(已采纳为 V2.12)**：不需为无限循环加 None 熔断；
只做稳健性改进：`actual is None` 时累计并打印 `none_count`，
便于区分"运动未收敛"与"串口读数不可用"。confirm 归零/计数逻辑不变。

**实测印证(run-24 L38)**：run-24 开机初始回零出现 `max_diff=44.12°` sync 兜底一次
（confirm=2 未能压制该单帧强瞬态）——证实 Codex (B) "confirm=2 不能根治强瞬态跳变"，
只能降低概率。run-25 本次同位置回正 max_diff=1.31° completion 0.07s，
说明该 44° 是偶发瞬态、非稳定故障。

---

## 3. Codex (C) — 缺陷B：sync 兜底 res==0 直接熔断：Insufficient → 已补

**源码位置**：`checked_short_angles_async()` 软超时后 sync 兜底段（V2.12 已改
`L1023-L1091`）。step5 调用 L1851-L1854、step9 调用 L1882-L1885。
run-23 失败点：step9 `drop_hover`（log L395-L400 L5/5 step9 sync 返回 0 抛异常）。

**Codex 裁定要点**：
- "返回0直接熔断"作为硬件安全失败是**保守的（正确）**，但作为缺陷B修复**不充分**：
  run-23 已记录 `max_err=2.11°` 却无 post-failure 角度/坐标复读，无法区分"真没到位"
  vs"固件确认假失败"。
- 对照 run-22 L391-L395 同一第 5/5 轮 step9 用 `max_err=2.11°/delta_xyz=10.9mm` 软到位
  成功 → 同一轨迹、同一门限下 run-23 更像偶发确认/读数/固件到位判定问题，不是点位
  安全门本身错误。
- **Codex 给的根因排序**(run-23)：①固件 sync 死区假失败(历史参数 L204-L212/L278-L283
  承认上行 sync 在 ~2° 残差可返回 0)；②4s 窗口内 confirm=2 只拿到一次有效 OK 或 OK 夹
  None/抖动未达二次确认（无逐帧读数日志，不能实锤）；③线缆/串口时序/机械负载热态偶
  发慢收敛（日志未见串口异常文本，**证据弱于前两项**）。

**Codex 推荐修复方向(已采纳为 V2.12)**：
1. PC 脚本内加有界 retry/诊断 wrapper：sync 返回 0 后立即复读角度和坐标；
   连续确认在软容差内则软通过并强警告；否则最多重发一次低速/同速关节目标，再失败才熔断。
2. 正式比赛闭环仍应把同等策略下放到板上 CPU 固件 —— PC/pymycobot 只保留调试和标定，
   不应成为最终控制链路（AGENTS.md 决赛主线硬边界一致）。

**V2.12 落地实现(L1031-L1091)** 三段式判定：
- `res==1`：正常 return True。
- `res!=1`+复读(post-failure get_filtered_angles/coords)在软容差内 (`max_err≤3.0°`、
  `delta_xyz≤25mm`)：软通过 + 强警告（保留 SOFT_REFINE_WARN_COORD=15mm 强警告），不熔断。
- `res!=1`+复读超容差：重发 1 次已验证低速关节目标 (`speed=SOFT_REFINE_SPEED=8`、
  `timeout=SOFT_REFINE_TIMEOUT=3s`) → 再复读 → 仍超容差才 `raise RuntimeError` 熔断
  （V2.4 保守失败边界保留）。有界仅 1 次、不无限循环、retry 期间不释放舵机。
- 重发 sync 抛异常：按熔断处理。

---

## 4. V2.12 运行时验证状态（迁移前必须如实认知）

**已验证**：
- run-25(N=5)全流程软到位收敛、0 兜底、0 人工扶正、Happy Path 零副作用 ——
  证实 V2.12 诊断/retry 逻辑**仅挂在 res!=1 故障退路内**，主流程零开销。
- 代码层面经 Codex 复核（三段式判定逻辑闭环、边界正确、安全语义保留）。

**未验证（迁移前如实记录的风险）**：
- V2.12 的 post-failure 复读 + 有界重试分支，因 run-25 全程零兜底，**该分支未被运行时
  触发**。日志中无 `post-failure 复读` / `重发低速` / `none_count=` 任何一行（因全程未
  进兜底分支）。
- Codex 对 (C) 的复核是**只读审查**，未复跑 retry 路径。**V2.12 retry 路径的运行时
  正确性目前没有独立背书**，Priority-3 文档不得把它当"已实测验证策略"写，只能当
  "代码审查通过、运行时未触发"记录。

**追验证建议**(非必需，触发条件)：
- 后续若 run-N 再出现 step5/9 sync 兜底(res==0)，观察是否按 V2.12 三段式判定 →
  若实测命中 post-failure 复读在软容差内放行，则 V2.12 价值闭环证实。
- 或人为注入临时 mock（脚本临时让 `sync_send_angles` 模拟返回0），主动验证 retry 分支。
  但该法有运动风险（臂会在 hover 位置再做一次 speed=8 收敛动作），需确认无障碍物。

---

## 5. Priority-3 迁移设计规范（以本文件为准）

按 AGENTS.md 决赛主线硬边界：PC/pymycobot 只保留调试和标定，不进正式识别/控制闭环。
myCobot 协议封包、点位表、动作序列、互锁、超时、异常处理下放到板上 CPU 固件；
FPGA RTL 只提供 UART/FIFO/寄存器等硬件通道。

基于 Codex 三项裁定，Priority-3 板上 CPU 侧(
`final_project/cpu/app/src/arm_controller.c`、`mycobot_protocol.c`、
`final_project/cpu/params/arm_positions.h`)应实现的迁移规范：

### 5.1 末段回零/短距离上行 等待策略（对应 PC 端已验证模式）
- 非阻塞 `send_angles` + 软到位轮询(Python `poll=0.05s` → C 侧定时器节拍)，软容差
  `~3°/~25mm`；不靠固件 `is_in_position` 死等（已证其有假失败）。
- 二次读数确认(confirm 概念)：连续 N 次读数都 ≤tol 才判收敛，拒绝单帧瞬态假收敛。
- 软超时走有界 sync 收尾兜底，最终熔断保留保守失败语义。

### 5.2 超时/熔断分级（对应 Codex C）
- 固件 `sync_send_angles` 返回 0 不直接熔断 → 先 post-failure 复读角度/坐标。
- 复读在软容差内 → 软通过 + 强警告日志（**不熔断**）。
- 复读超容差 → 有界重试 1 次（已验证低速）→ 再复读 → 仍超差才熔断释放。
- 重试有界、不无限循环、retry 期不掉电、最终熔断保守。

### 5.3 诊断能力（对应 Codex B）
- 软到位轮询循环累计 `none_count`（读数失败帧数），熔断时输出，便于区分"运动未收敛"
  vs"读数不可用"。
- 异常日志带 type/traceback（Codex A 遗留，C 侧可系统性实现，不照搬 PC 端"只打印一行"）。

### 5.4 通信规范（**不以"防溢出"为根因**）
- C 侧轮询节拍规范化（如定时器 50-100ms）属**工程规范**，应实现（避免死循环挤总线）
  ，但**不得在迁移文档中标注为"修复 run-23 熔断的根因"** —— Codex 已裁决 run-23 根因
  优先级是"固件假失败"而非"串口溢出"（证据弱）。
- UART 读必须加数据帧校验，偶发丢包执行非阻塞自动重试 —— 此为工程稳健性，非 run-23
  熔断的直接修复。

### 5.5 迁移前先只读梳理四份现有骨架
`final_project/cpu/app/src/arm_controller.c`、`mycobot_protocol.c`、
`final_project/cpu/params/arm_positions.h`、`final_project/integration/mycobot_protocol_notes.md`
先读懂现有代码骨架与契约，再产迁移设计文档；不在读懂现状前开始改 C 代码。

---

## 6. 证据路径索引

| 项 | 日志/源码 |
|----|----------|
| Codex 全部裁定 | codex-rescue 复核输出（已 inline 引用源码行号） |
| run-23 step9 熔断 | `audit_logs/auto_run_20260708_165548.log` L395-L400 |
| run-22 step9 同点软通过 | `audit_logs/auto_run_20260708_162208.log` L391-L395 |
| run-24 开机回零 44° 兜底 | `audit_logs/auto_run_20260708_172705.log` L37-L38 |
| run-25 全程零兜底零副作用 | `audit_logs/auto_run_20260708_175657.log`（无 sync 兜底行） |
| V2.11 EOF 修复 | `teach_replay_pick_return_ready.py:2182-2214` |
| V2.12 三段式 retry + none_count | `teach_replay_pick_return_ready.py:977-1028, 1031-1091` |