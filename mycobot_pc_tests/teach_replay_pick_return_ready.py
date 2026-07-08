"""
关节角示教回放 + 空间坐标一致性校验 抓取/放置 + 回零中间点 V2 脚本
====================================================================

> 对照方案: mycobot_pc_tests/audit_logs/return_ready_teach_replay_v2_plan.md
> 基线脚本: mycobot_pc_tests/teach_replay_pick.py （已跑通，本文件不修改它）
> 用途边界: PC 端交互调试、定点抓取验证、后续板端控制策略参考
> 不进入正式闭环: 本脚本属于 pymycobot PC 端联调归档，不作为决赛正式识别/控制闭环依赖。

V2 相对基线 teach_replay_pick.py 的核心变化（plan §2/§4/§5）:
  1. 五点示教: pick_hover / pick / drop_hover / drop / home_ready。
     home_ready 是 drop_hover -> 直立零位之间的空中安全中间姿态。
  2. 自动流程末段改成 drop_hover -> home_ready -> safe_return_home()，
     不再从 drop_hover（arm_max_diff≈70）直接硬回零触发人工扶正。
  3. 新增带载工作区分级: 推荐 R<=250mm，可接受 250<R<=260（黄字提示），
     边缘 260<R<=280（显式确认），禁止 R>280（沿用 R_MAX 硬拦截）。
  4. home_ready 不套抓放业务门 Z_MAX=280 / Z_MIN / R_MIN（它是高位空中姿态，
     直立附近 Z 可达 300~400mm）；只保留 R_MAX=280 物理臂展硬限 + 关节空间
     arm_max_diff<=45 自动回零安全门 + is_valid_coord_reading 读数有效性。
  5. 长距离仍用 sync_send_angles()，短距离上下探仍用 checked_short_angles()，
     坐标只做 verify_coords_near() 只读校验，不恢复 sync_send_coords()。

对 Codex 方案的修正（实现期发现，详见审查记录）:
  A. plan §6 RETURN_ARM_JOINT_MAX_DELTA=55 抬到 90：原值会误拒"更接近 HOME 的
     优秀 home_ready"（drop_hover J3≈-70 -> home_ready J3≈-10 时单轴 delta=60>55）。
     90 仍能拦截异侧分支/荒谬跳变，但不误判同侧拉直的合法过渡。
  B. plan §4 home_ready 的 arm_max_diff<=45 由"建议"改为硬门：示教期加重新示教
     循环，自动期 validate_return_ready 硬拦截。否则 prepare/step11 的
     safe_return_home 仍会进人工扶正，自动化不成立。
  C. plan §5 "校验 home_ready -> HOME_ANGLES" 与 validate_return_ready 冗余，
     只保留 validate_return_ready（其通过则 home_ready->HOME 单轴 delta<=45<90）。
  D. plan §5 drop_hover -> home_ready 用 checked_return_transition（底层
     sync_send_angles + 软到位 + 诊断），而非 checked_sync_angles（无软通过）。
     原因：该过渡后 safe_return_home 会重读实际角度独立判断 arm_max_diff，
     软通过更安全，避免固件 is_in_position 未收敛时整流程中断。
  E. home_ready 保留 R_MAX=280 物理臂展硬限，跳过 Z_MAX/Z_MIN/R_MIN 业务门。
  F. 实测日志 trial_run_9_return_ready_v2_logs.md 需上板运行生成，本脚本只提供
     可编译代码 + 日志模板骨架。

V2.1 相对 V2 的迭代（plan §12 / run-10 复核，只改 home_ready 余量，不动速度）:
  G. 新增 HOME_READY_TARGET_ARM_MAX=40.0：run-10 示教 arm_max_diff=43.1（离 45 门
     仅 1.9°），drop_hover->home_ready 软通过后实际 J2 多偏 2.29°（3° 软容差内正常
     偏差）吃光余量，实际 arm_max_diff=45.4 越界触发人工扶正。普适约束：示教目标
     须 <= 45 - SOFT_ANGLE_SUCCESS_TOL(3) = 42°，取 40° 留 5° 余量。
  H. record_return_ready_point 改两级门：<=40 推荐 / 40~45 警告允许保存 / >45 禁止
     （run-10 落 40~45 区间导致越界，故此区间强警告并建议重示教，但仍给逃生口）。
     示教提示语引用 run-10 扶正后实测可达姿态 [7.99,-36.38,3.51,12.12,-4.48,54.75]。
  I. checked_return_transition 软通过后追加 _verify_actual_pose_for_auto_return：
     读实际角度算 arm_max_diff，>45 则低速重试一次 home_ready(speed=15,timeout=20)，
     仍 >45 不抛异常（避免臂在半空触发异常掉电），交 safe_return_home 人工扶正接管。
  J. safe_return_home 返回三态 "auto"/"manual"/"failed"（旧式 if not 调用兼容），
     auto_phase_v2 末段消息如实区分"0 轮自动回零"/"触发人工扶正后完成"/"回零失败"，
     修正 run-10 日志把人工扶正写成自动回零的措辞。
  K. 不放宽 ARM_MAX_DIFF_SAFE（plan §12.3）；SHORT_UP_SPEED 保持 16 不动（plan §12.5
     单变量原则，回零改善归因到 home_ready 余量）。

V2.2 相对 V2.1 的迭代（plan §13 / run-11 两轮带载复核，焦点从回零转为上行精度）:
  L. run-11 证明 home_ready 问题已关闭（两轮带载均 "auto" 0 轮扶正），
     HOME_READY_TARGET_ARM_MAX=40.0 保留。新焦点：上行软通过残余误差。
  M. 新增 SOFT_REFINE_* 参数 + _soft_refine()：checked_short_angles 软通过后若残差
     >1.2°/>10mm，追加一次低速微调(speed=8, timeout=6s, 最多1轮)，把 run-11 Run B
     step 9 的 max_err=2.5°/delta=16.2mm 压向 <=1.2°/<=10mm。仅 allow_soft_success=True
     启用（step 5/9/10），不恢复 sync_send_coords()。
  N. SHORT_UP_TIMEOUT 25->15s：run-11 证明 25s 在等固件判失败，缩到 15s 让微调更早介入。
  O. 新增 HOME_RETURN_SPEED=20/HOME_RETURN_TIMEOUT=12，替代 safe_return_home 内硬编码
     speed=15/timeout=20（home_ready->HOME 路径短且安全，缩短等待；安全门不变）。
  P. step 5/9/10/11 加 time.time() 耗时日志（plan §13.5-4）。
  Q. 不动 SHORT_UP_SPEED（保持 16，plan §13.3 速度调参后置到 V2.3）；
     不实现 JSON 预设（plan §13.5-6，后置到用户明确要求时）。

V2.3 相对 V2.2 的迭代（plan §14 / run-12 复核，焦点转为预设复用 + 兜底加固）:
  R. run-12 证明 V2.2 参数稳定（step5/9 正常返回1，合计~21.3s vs run-11~90s，0轮扶正），
     但改善来自"短超时 + 内收工作半径 R<=252"共同作用，不能单独归因 SHORT_UP_TIMEOUT。
     _soft_refine 未触发（sync 返回1 未进软通过路径）→ 标记"保留兜底，未实跑验证"。
  S. 新增 JSON 点位复用（plan §14.4 优先级1）：mycobot_pc_tests/presets/*.json +
     CLI --preset <name> 加载 / --teach --save-preset <name> 保存。预设只跳过手动示教采集，
     加载后仍跑全部安全门（is_safe_coord/半径分区/validate_short_angle_pair/
     validate_return_angle_pair/validate_return_ready/用户确认轨迹），不因预设跳过安全校验。
  T. 修 _soft_refine 的 coord_delta=None 日志兜底（plan §14.4 优先级2）：新增 fmt_mm()
     辅助，统一所有 prev_coord_delta/coord_delta 打印，避免 None 时 :.1f 抛 TypeError。
     只改诊断输出，不动运动逻辑。
  U. record_teach_point 加 q 放弃退出口（plan §14.4 优先级3）：示教阶段输入 q 抛
     TeachAbort 异常，main 走安全退出（teach 阶段舵机已掉电，无需再 release）。
  V. 不继续提速（SHORT_UP_SPEED 保持 16）；home_ready 安全门保持 40/45。

V2.4 相对 V2.3 的迭代（plan §14.6/§14.7 / run-14 复核，焦点转为串口参数 Bug 修复 +
  末段回零响应提速；属 myCobot 动作链路改动，须 Codex 复核后实机验证）:
  W. 串口参数 Bug 修复（plan §14.6 / trial_run_14 §1）：get_port() 移除位置参数盲读
     `if len(sys.argv) > 1: return sys.argv[1]`。main() 已用 argparse 接管 --port，
     get_port() 只在 --port 未给时被调用，删盲读后纯做 comports() 自动枚举 + 交互选择，
     与 argparse 完全兼容。trial_run_14 不传 --port 传 --save-preset 时把 "--save-preset"
     误判为串口名导致 could not open port '--save-preset'，本项根治。
     不改 teach_replay_pick.py（plan §0 禁止改基线）与 teach_and_pick.py（无 argparse
     可选参数、无实际 bug、归档脚本）。
  X. 末段回零提速 L1+L2+L3+L4（plan §14.7）：
     L1：safe_return_home auto 分支去掉纯诊断的 diag_coords 读数（只打印不决策，manual
         分支保留）。省 ~0.4-0.8s。
     L2：_verify_actual_pose_for_auto_return 返回校验通过时的实际角度，checked_return_transition
         透传，safe_return_home 新增 cached_angles 参数复用它，跳过 auto 分支冗余的
         get_filtered_angles 重读（臂在 home_ready 静止，两读相隔~0.1s）。省 ~0.25-0.4s。
         cached_angles 默认 None，prepare_phase 路径与旧调用方行为不变；安全门判定
         （calc_home_diffs + 有限值校验 + arm_max_diff 门）不跳过，只省读数。
     L3：HOME_RETURN_SPEED 20->25。该段是全程最安全的移动（近直立位、无负载、距离短、
         单轴 delta<=45），提速 25 风险低于 step5/9 带载远端上行。提速后固件不收敛返回
         0 -> "failed" 安全失败（提示扶稳+release，非撞机）。需单独验证 run。
     L4：SOFT_REFINE_TIMEOUT 6->3s。run-14 step 9 实测微调不收敛（固件 plateau 在 ~2.1°），
         6s 纯等待；微调是 best-effort，失败仍走旧软通过门(3°/25mm) 兜底，逻辑不变。
         省 step 9 ~3s（针对广义末段瓶颈，非回零本身）。
  Y. 不动 SHORT_UP_SPEED（保持 16）、不动 ARM_MAX_DIFF_SAFE（45）/HOME_READY_TARGET_ARM_MAX（40）。
     窄义回零(step10+11) 预期 ~5s->~3s；广义末段(step9+10+11) 预期 ~28s->~23s。

V2.5 相对 V2.4 的迭代（plan §14.8 / run-15 复核，焦点转为末段回零"固件到位判定"瓶颈；
  属 myCobot 动作链路 + 动作参数改动，须 Codex 复核后实机验证）:
  Z. run-15 验证 V2.4 全部生效（step11 2.4->1.2s, step9 23->20s, 0 轮扶正, 串口 Bug 根治）。
     残留瓶颈：step11 的 1.2s 中 ~0.6-1s 是固件 is_in_position 死区的无意义尾端等待
     （物理 0.5s 已到位）。方案 §14.8 给三个方向，用户批"方向1+2+3+D0 全上"。
  AA. 方向1（异步软到位回零）+方向3（速度 25->30）：_send_home_async() 用非阻塞
      send_angles 启动回 HOME + Python 端软到位循环（max_diff<=1° 即退出）+ 软超时
      走阻塞 sync_send_angles 收尾兜底（res!=1 -> "failed" 安全失败）。speed=30。
      HOME_RETURN_ASYNC_ENABLE=False 一行回退 V2.4 阻塞路径。前提已核实：
      pymycobot 4.0.5 有 send_angles/get_angles/is_paused。预期 step11 ~1.2s->~0.5s。
  BB. 方向2（航路点平滑过渡）：_smooth_handoff_return() 把 step10+step11 合并为单一异步流
      ——非阻塞 send home_ready -> 轮询到接近 home_ready(<=5°) 即臂未停稳提前 send HOME
      -> 软到位。消除 home_ready "减速-静止-重启"停顿（~1s）。固件对运动中被打断的行为
      未文档化属未知风险，给 SMOOTH_HANDOFF_ENABLE 独立开关；未接近 home_ready 时
      回退 V2.4 阻塞分段路径（不抛异常，防臂在空中路径掉电）。预期 step10+11 ~3.8s->~1.5s。
  CC. D0 诊断补全：run-15 step11 靠"无警告"反推 res==1 的诊断缺口——异步/阻塞两条路径
      都显式打印回零结果（status/mode 或 res）；庆祝消息措辞 V2.2 -> V2.5。
  DD. 不动 SHORT_UP_SPEED（16）/ARM_MAX_DIFF_SAFE（45）/HOME_READY_TARGET_ARM_MAX（40）/
      R_MAX（280）；不恢复 sync_send_coords()。safe_return_home 的 cached_angles/arm_max_diff
      安全门判定完全不变，异步化只动移动下发方式。

运动/安全参数沿用 V2.4 已验证值，V2.5 只加异步软到位回零 + 平滑过渡 + 诊断补全；
安全门（45/40/R_MAX）与软通过兜底逻辑不变，两项新功能均有 enable 开关可一行回退 V2.4。

V2.6 相对 V2.5 的迭代（plan §14.8.6 / run-16 复核，只微调两个门限常数，不动逻辑；
  用户批"本轮无需过 Codex 门"，属参数微调）:
  EE. run-16 验证 V2.5 方向2 平滑过渡本身成功（1.84°<=5° 提前下发 HOME，固件打断行为
      未出问题——Codex 担心的最高风险点通过）。但方向1/3 异步软到位从未触发：myCobot 280
      物理死区让回零残余角差稳定在 1.1~1.4°（实测 1.31°），HOME_RETURN_ASYNC_SOFT_TOL=1.0°
      永远过不了 -> 空等 2.5s + sync 兜底，step10+11 反弹到 5.0s（比 V2.4 的 3.8s 还慢）。
  FF. HOME_RETURN_ASYNC_SOFT_TOL 1.0->1.5：让 1.31° 残差通过（留 0.19° 余量）。HOME 是终点
      无后续动作，1.5° 物理上完全回正无干涉。取 1.5 而非 Gemini 建议的 2.0，遵循小步单变量
      原则；run-17 仍偶发不收敛再升 2.0。本常量同时用于方向1 step11 与方向2 phase-B 的
      HOME 软到位判定（一致性）。
  GG. HOME_RETURN_ASYNC_TIMEOUT 2.5->1.5：run-16 物理到位约 0.5s，1.5s 留 1s 余量；1.5s 仍比
      2.5s 省 1s 异常等待。取 1.5 而非 Gemini 建议的 1.2——1.2s 余量偏小，负载/摩擦稍慢时
      会被误判超时走 sync 兜底反而更慢；run-17 稳了再考虑 1.2。
  HH. 不动 HOME_RETURN_ASYNC_SPEED(30)/SMOOTH_HANDOFF_NEAR_TOL(5.0)/HOME_RETURN_ASYNC_POLL(0.05)；
      不动安全门（45/40/R_MAX）；不恢复 sync_send_coords()。sync 兜底仍用 V2.4 验证的
      HOME_RETURN_SPEED=25（Codex F1，V2.5 已落地）。预期 step10+11 5.0s->~1.5-2.0s。

V2.7 相对 V2.6 的迭代（连续抓取循环，贴近比赛条件；只动 main 编排层，不碰运动/安全逻辑）:
  II. auto_phase_v2 加 bool 返回值：True=本轮完整跑通（含末段回零，臂在 HOME/夹爪张开）；
     False=中途中止（home_ready 安全门未过 / 回零失败已 release_all_servos）。
     给原有隐式 None 的 return 加一个语义值，不改任何运动/安全判定。
  JJ. main 在 prepare_phase 之后把单次调用改成"终端键入次数 N 的连续抓取循环"：
     - N=1（默认，空回车）行为与改动前单跑完全一致（向后兼容）。
     - 循环只重复 auto_phase_v2 的关节回放，不重跑示教 acquire_points / prepare_phase。
     - 每轮开头的"请将正方体放回抓取点"提示天然承担换块确认，无需新增提示。
     - auto_phase_v2 返回 False 即 break，避免在舵机掉电状态下继续下发运动指令。
  KK. 不动所有速度/超时/容差/安全门常数；不动 prepare_phase/acquire_points；
     auto_phase_v2 内部 0./0b. 只读安全门每轮重跑（零运动风险，保留每轮出发前
     再确认一次轨迹连续性的防御性，刻意不拆到循环外以保持函数自包含）。

V2.8 相对 V2.7 的迭代（流畅度加强三点，贴近比赛条件；用户已确认赛场流程为
  CPU 信号→单轮抓放→人补块到 pick 点，且机械臂始终有人旁边保护）:
  LL. 点2 去每轮 Enter：auto_phase_v2 内的"请将正方体放回抓取点"input 移除，
      N 轮全自动运行。一次性轨迹/物块确认移到 main 循环开始前（只问一次）。
      赛场流程下 pick 点每轮有人补块，不会抓空气；板上 CPU 迁移友好（本就无 Enter）。
  MM. 点3 自动导出终端日志：新增 Tee 类双写 stdout（终端 + UTF-8 文件），main 开头
      安装，自动写到 audit_logs/auto_run_<时间戳>.log。不动任何 print，纯工具功能，
      try/finally 确保异常/Ctrl+C 时文件关闭。PC 专用，不上板。
  NN. 点1 step 9 异步化（A 类，最大收益）：step 9 (drop→drop_hover 带载上行) 改非阻塞
      send_angles + Python 软到位循环（复用 V2.5 _send_home_async 思路），残差≤3° 即
      软通过退出，去除 15s sync timeout + 3s 微调 timeout 的固件死区等待。run-18 实测
      残差稳态 2.1°，必然软通过。step 9 从 20s→~2s，每轮省 18s。
      新增 checked_short_angles_async() 专用函数 + ASYNC_SHORT_* 参数，保留原
      checked_short_angles 不动（step 3/5/7 仍用阻塞版，step 5 已 res==1 严格通过无需改）。
  OO. 点1 B 类压缩：verify_coords_near 只在带载上行软通过后（step 5/9）保留，长距离
      step 2/6 与短下探 step 3/7 的坐标校验改为可跳过（预设回放点位固定，res==1 严格
      通过时坐标必然准）。夹爪 GRIPPER_TIMEOUT 2.5s→1.2s（夹爪物理开合 <1s）。
      新增 SKIP_COORD_VERIFY_ON_STRICT_PASS 开关，默认 True；关闭即回退 V2.7 行为。
  PP. 不动安全门（validate_return_ready/validate_short_angle_pair/validate_return_angle_pair/
      is_safe_coord/R_MAX/arm_max_diff≤45）；不恢复 sync_send_coords()；不动 step 10+11
      回零路径（V2.6 已优化到位 3.4s）；teach_replay_pick.py/teach_and_pick.py 不改。
"""

import time
import sys
import math
import json
import os
import argparse
import serial.tools.list_ports
from pymycobot.mycobot import MyCobot

# ============================================================
# 安全参数（保守值，参见 plan 第 7 节）
# ============================================================
ANG_REPLAY_SPEED = 20          # 关节角回放速度 (%)
ANG_REPLAY_TIMEOUT = 20        # 关节角回放超时 (s)
# §16.3 / §16.8：方向分离速度/超时——下行（重力辅助）保持低速短超时，
# 上行（抗重力）适当提速并放宽超时窗口。run-7 确证根因为固件到位判定
# 未收敛（max_angle_delta=2.2° 仍返回 0），因此上行超时从 15s 增至 25s。
SHORT_DOWN_SPEED = 12          # 下行关节速度（重力辅助）(%)
SHORT_DOWN_TIMEOUT = 10        # 下行关节超时 (s)
SHORT_UP_SPEED = 16            # 上行动作适当提速，克服逆重力阻力 (%)
# plan §7：第一轮保持 16 不变；第 2/3 轮再单独试探 20，避免同轮多变量。
SHORT_UP_TIMEOUT = 15          # V2.2 §13.3：上行超时 25->15s（run-11 证明 25s 在等固件判失败）
# §16.4 软到位成功判定——sync_send_angles 返回 0 后先读数再决断，
# 物理已接近目标（max_angle_delta <= 3° 且 delta_xyz <= 25mm）时软通过。
SOFT_ANGLE_SUCCESS_TOL = 3.0   # 软到位关节角度容差 (deg)
SOFT_COORD_SUCCESS_TOL = 25.0  # 软到位坐标容差 (mm)，复用 COORD_VERIFY_TOL
SOFT_SETTLE_SECONDS = 0.5      # 软到位前等待舵机稳定 (s)

# V2.2 §13.3 优先级 1：上行软通过后低速二次微调。
# run-11 Run B step 9 实测：sync_send_angles 返回 0，max_err=2.5°、delta_xyz=16.2mm，
# 固件 is_in_position 在 ~2° 残差处判超时，臂物理接近但未精到位。对 3cm 物块
# 接近夹爪极限时有抓取失败风险。二次微调在软通过后追加一次低速短时回放，
# 把残差从 2.0~2.5° 压到 <=1.2° / delta_xyz <=10mm。
# 仅对 allow_soft_success=True 的动作启用（step 5/9 上行 + step 10 回零过渡），
# 不恢复 sync_send_coords()；速度 8 低速避免带载末端晃动；最多 1 轮避免来回抖动。
SOFT_REFINE_ENABLE = True          # 总开关
SOFT_REFINE_ANGLE_TRIGGER = 1.2    # 触发微调的角差阈值 (deg)；>1.2° 才微调
SOFT_REFINE_COORD_TRIGGER = 10.0   # 触发微调的坐标阈值 (mm)；>10mm 才微调
SOFT_REFINE_ANGLE_OK = 1.2         # 微调达标角差 (deg)
SOFT_REFINE_COORD_OK = 10.0        # 微调达标坐标 (mm)
SOFT_REFINE_WARN_COORD = 15.0      # 残差强警告阈值 (mm)；Run B 16.2mm 即此级别
SOFT_REFINE_SPEED = 8              # 微调速度 (%)，低速避免带载晃动
SOFT_REFINE_TIMEOUT = 3            # V2.4 §14.7 L4：微调超时 6->3s（run-14 step 9 实测微调不收敛，6s 纯等待）
SOFT_REFINE_MAX_ROUNDS = 1         # 最多微调轮数

# V2.2 §13.3 优先级 4：回零速度独立参数（home_ready -> HOME 路径短且安全）。
# run-11 step 11 从 arm_max_diff≈34° 回直立，物理 6~8s，原硬编码 speed=15/timeout=20
# 偏保守。提至 20/12s 缩短等待，不改变 ARM_MAX_DIFF_SAFE 安全门（仍 <=45 才走此路径）。
# V2.4 §14.7 L3：HOME_RETURN_SPEED 20->25。该段是全程最安全的移动——近直立位、
# 无负载（夹爪已张开放下物块）、单轴 delta<=45。提速 25 仍远低于 SHORT_UP_SPEED
# 边界风险。提速后固件 is_in_position 若不收敛返回 0 -> safe_return_home 返回
# "failed" -> 提示扶稳+release（安全失败，不撞机）。需单独验证 run + Codex 复核。
HOME_RETURN_SPEED = 25
HOME_RETURN_TIMEOUT = 12

# V2.5 §14.8 末段回零异步软到位 + 航路点平滑过渡。
# run-15 证明 V2.4 把 step11 压到 1.2s，但固件 is_in_position 死区仍吃掉 ~0.6-1s
# 无意义尾端等待（物理 0.5s 已到位）。方向1：放弃阻塞 sync_send_angles，改用非阻塞
# send_angles + Python 端软到位循环，物理到位即退出，预期 step11 ~0.5s。
# 方向3：配合异步提速 HOME_RETURN_SPEED 25->30（异步下提速有叠加意义；阻塞下提速
# 收益被固件等待吃掉）。前提已核实：pymycobot 4.0.5 有 send_angles/get_angles/is_paused。
# 方向2：step10 drop_hover->home_ready 软到位循环中检测到接近 home_ready 时，臂未停稳
# 即提前下发 HOME 目标，消除"减速-静止-重启"停顿。固件对运动中被打断的行为未文档化，
# 属未知风险，故给独立 enable 开关，异常可一行回退到 V2.4 分段执行。
HOME_RETURN_ASYNC_ENABLE = True       # 方向1总开关：False 时回退到 V2.4 阻塞 sync_send_angles
HOME_RETURN_ASYNC_SPEED = 30          # 方向3：异步回零速度 25->30
# V2.6 §14.8.6：软到位阈值 1.0->1.5。run-16 实测 myCobot 280 物理死区让回零残余角差
# 稳定在 1.1~1.4°（实测 1.31°），1.0° 门永远过不了 -> 异步软到位从未触发 -> 空等 2.5s
# + sync 兜底，反而比 V2.4 慢。1.5° 让 1.31° 通过（留 0.19° 余量）；HOME 是终点无后续
# 动作，1.5° 残差物理上完全回正无干涉。取 1.5 而非 2.0 遵循小步单变量原则，run-17 仍
# 偶发不收敛再升 2.0。本常量同时用于方向1（step11）与方向2（平滑过渡 phase-B）HOME 软到位。
HOME_RETURN_ASYNC_SOFT_TOL = 1.5      # 软到位阈值（度）；V2.6 1.0->1.5（run-16 残差 1.31°）
# V2.6 §14.8.6：软超时 2.5->1.5。run-16 物理到位约 0.5s，1.5s 留 1s 余量足够判定收敛；
# 1.5s 仍比 2.5s 省 1s 异常等待。取 1.5 而非 1.2——1.2s 余量偏小，负载/摩擦稍慢时会被
# 误判超时走 sync 兜底反而更慢。若 1.2s 仍省时且无误判，run-17 再压到 1.2。
HOME_RETURN_ASYNC_TIMEOUT = 1.5       # 软超时（s）；V2.6 2.5->1.5，软超时走 sync 收尾兜底
HOME_RETURN_ASYNC_POLL = 0.05         # 软到位轮询间隔（s）
# V2.10：末段回零二次读数确认。语义同 ASYNC_SHORT_CONFIRM_COUNT——连续 N 次读数 max_diff
# 都 <= HOME_RETURN_ASYNC_SOFT_TOL(1.5°) 才判收敛，任一帧回升则归零计数器继续轮询。
# 根因：run-19~22 四批共 11 轮里 3 次（~27%）末段回零走 sync 兜底，每次都因"臂未停稳提前下发
# HOME"后单次读数读到运动中瞬态 ~39° 跳变 → 判未收敛 → sync 7.2s 尖峰（最坏单轮）。
# confirm=2 拒绝单帧下探假收敛，与 step5/9 同构治法。timeout 暂保持 1.5s 不动（隔离归因：
# 先验"单帧瞬态是否兜底主因"，若兜底率不降再单独调 timeout）。
HOME_RETURN_ASYNC_CONFIRM_COUNT = 2     # 末段/初始回零软到位连续确认次数（1=单次，V2.5/V2.9 行为）
# 方向2平滑过渡：step10 软到位循环中 arm_max_diff<=此阈值即提前下发 HOME（臂未停稳）。
# 取 5.0（§14.8 建议的"与 home_ready 最大轴偏差已小于 5°"）。设 None 则禁用方向2。
SMOOTH_HANDOFF_ENABLE = True
SMOOTH_HANDOFF_NEAR_TOL = 5.0         # 接近 home_ready 的提前下发阈值（度）

# V2.8 点1：step 9 (drop→drop_hover 带载上行) 异步软到位参数。
# run-18 实测：step 9 用阻塞 sync_send_angles(timeout=15s) 等固件 is_in_position 判失败
# 返回 0，残差稳态卡 2.1°（三轮完全一致），15s + 3s 微调全在等死区，物理运动只占 1.5s。
# 改非阻塞 send_angles + Python 软到位循环（复用 V2.5 _send_home_async 思路），
# 残差≤SOFT_ANGLE_SUCCESS_TOL(3°) 即软通过退出，去除 18s 固件死区等待。
# 软超时走阻塞 sync_send_angles 收尾兜底（保留 V2.4 安全失败语义：res!=1 抛异常熔断）。
# 仅 step 9 启用（drop→drop_hover 是已知瓶颈）；step 5 已 res==1 严格通过无需改；
# step 3/7 是下行(重力辅助)，固件收敛快，保持阻塞版。
ASYNC_SHORT_ENABLE = True             # 总开关：False 时 step 9 回退阻塞 checked_short_angles
# V2.9：step 5 (pick→pick_hover 带载上行) 异步软到位。与 step 9 同构（逆重力带载短距离
# 回放到 hover 点），run-19 第2/3轮 step 5 卡 3.4~3.5s 固件 is_in_position 死区，根因同 step 9。
# 复用 checked_short_angles_async；独立开关以保留回滚粒度——step 9 已验证，step 5 是新改动，
# 一旦 step 5 出问题可单独回退而不连带关掉 step 9。
ASYNC_SHORT_STEP5_ENABLE = True        # step 5 专用：False 时回退阻塞 checked_short_angles
ASYNC_SHORT_SOFT_TOL = 3.0            # 软到位阈值（度），复用 SOFT_ANGLE_SUCCESS_TOL
ASYNC_SHORT_TIMEOUT = 4.0             # 软超时（s）：物理 ~1.5s，留 2.5s 余量；超时走 sync 兜底
ASYNC_SHORT_POLL = 0.05               # 软到位轮询间隔（s），复用 HOME_RETURN_ASYNC_POLL
# V2.9：二次读数确认。常态 1；设 >1 时，软到位判定要求**连续 N 次**读数都 ≤tol 才判收敛，
# 否则重置计数器继续轮询。修"减速振荡中单帧瞬态下探 ≤tol 造成假收敛"（臂还在晃就退出）。
# 注意本参数只提升退出稳健性，不向下压"单帧瞬态上蹿 >tol 造成延迟退出"那一类方差。
# 取 2：单次确认(N=1)的稳健性补强，对 happy path（真稳定）增加 ~0.05s 确认读数；已稳定的物理
# 收敛点必连续两次 ≤tol，振荡瞬态点（单帧下探）会因下一帧回升而被拒、继续轮询到真稳态。
ASYNC_SHORT_CONFIRM_COUNT = 2       # 软到位连续确认次数（1=单次确认，即 V2.8 行为）
# V2.8 点1 B 类：夹爪开环等待 2.5s→1.2s（夹爪物理开合 <1s，人眼可确认）。
GRIPPER_TIMEOUT = 1.2                 # V2.8：2.5->1.2s（原 V2.7 值 2.5s 纯等待偏长）
# V2.8 点1 B 类：长距离/短下探严格通过(res==1)时跳过 verify_coords_near，省 ~0.6s/次。
# 预设回放点位固定，res==1 严格通过时坐标必然在容差内，校验冗余。
# 带载上行软通过后(step 5/9)仍保留 verify_coords_near（软通过需复核坐标）。
SKIP_COORD_VERIFY_ON_STRICT_PASS = True
# §14.4：短距离 hover/down 点对关节连续性安全门，防止示教落在不同 IK 分支。
# 1-5 轴短距离单轴变化过大说明 hover/down 可能不在同一解分支，回放会大幅摆动。
SHORT_ARM_JOINT_MAX_DELTA = 30.0   # 短距离 1-5 轴单轴最大变化（度）
SHORT_WRIST6_MAX_DELTA = 45.0      # 短距离第 6 轴末端旋转最大变化（度）
# V2 §6：回零过渡 drop_hover -> home_ready 的关节差 sanity guard。
# 比 §14.4 短距离宽松：回零过渡本身就是大幅度拉直再构型（J3 从 -70° 朝 0°
# 拉直，同侧合法过渡单轴变化可达 60~70°）。此处只拦截"荒谬跳变/异侧分支"。
# 修正 A：plan 给 55 会误拒"更接近 HOME 的优秀 home_ready"（同侧 delta 可达 60），
# 故取 90：同侧拉直必过，异侧/翻面（>90）才拦。腕部取 120：腕部旋转本身无害，
# 只拦完全翻面。
RETURN_ARM_JOINT_MAX_DELTA = 90.0
RETURN_WRIST6_MAX_DELTA = 120.0
GRIPPER_SPEED = 50             # 夹爪速度 (%)

# 回零目标关节角（§12.1）：保留理论零位 [0,0,0,0,0,0]，不改成本机实测非零值。
# 实测直立姿态 [-10.81, 2.46, 1.49, -8.17, 2.28, 3.07] 相对零位 max_diff=10.81，
# 低于 45 度大偏差阈值，回零安全。
HOME_ANGLES = [0, 0, 0, 0, 0, 0]
# §13.2：大臂安全门只看 1-5 轴，第 6 轴末端旋转不阻断大臂回零。
ARM_JOINT_COUNT = 5
ARM_MAX_DIFF_SAFE = 45.0       # 大臂 1-5 轴大偏差阈值（度）；safe_return_home 自动回零门
WRIST6_WARN_DIFF = 90.0        # 第 6 轴末端旋转提示阈值（度，仅告警不阻断）

# V2.1 (plan §12.4 / run-10 复核)：home_ready 示教目标余量门。
# run-10 证明：示教 arm_max_diff=43.1（离 45 门仅 1.9°），drop_hover->home_ready
# 软通过后实际 J2 多偏 2.29°（软到位容差 SOFT_ANGLE_SUCCESS_TOL=3° 内的正常偏差），
# 吃光余量使实际 arm_max_diff=45.4 > 45，触发人工扶正。
# 由此得到普适约束：示教目标须 <= ARM_MAX_DIFF_SAFE - SOFT_ANGLE_SUCCESS_TOL，
# 即 45-3=42°，才能保证软通过后不越界。取 40° 留 5° 余量更稳。
# 不放宽 ARM_MAX_DIFF_SAFE（plan §12.3）：放宽会改所有回零入口边界，只该补 home_ready 余量。
HOME_READY_TARGET_ARM_MAX = 40.0   # home_ready 示教推荐门（度）；run-11 目标
# V2.1 §12.4-4：auto 第10步软通过后，对实际姿态做安全门校验；越界则低速重试一次。
# 重试目标仍是示教 home_ready（<=40），当前臂在 ~45，命令朝 HOME 方向挪 ~5°，
# 速度 15 即便再软通过落到 42~43 仍 <45 -> 自动回零；最坏仍 >45 落到 safe_return_home
# 人工扶正，不比不重试差（单向安全）。
HOME_READY_RETRY_SPEED = 15
HOME_READY_RETRY_TIMEOUT = 20

COORD_VERIFY_TOL = 25.0        # 坐标一致性校验容差 (mm)
COORD_STABLE_TOL = 8.0         # 连续读数稳定性判断 (mm)
COORD_RETRIES = 6              # get_filtered_coords 最大重试次数
ANG_RETRIES = 8                # get_filtered_angles 最大重试次数（§12.3 提高以等稳定读数）
ANG_STABLE_TOL = 3.0           # 连续两次关节角读数稳定性判断 (度，§12.3）
POWER_ON_SETTLE = 1.5          # power_on 后舵机抱紧/读数稳定等待 (s，§12.5）

# is_safe_coord 的固定阈值（与 teach_and_pick.py 保持一致；若实际工作点
# 超出该阈值，应先调整阈值常量，而不是绕过过滤）
# 注意：Z_MAX 仅作为"抓取/放置工作区"边界（业务级安全），不作为读数有效性边界，
# 也不适用于 home_ready（直立安全姿态下 Z 可达 ~417mm，§12.1 实测）。
Z_MAX = 280.0
Z_MIN_PICK = 5.0
Z_MIN_HOVER = 65.0
R_MIN = 60.0
R_MAX = 280.0

# V2 §3：带载工作区分级。R_MAX=280 是物理臂展硬限（对所有点生效）；
# 以下三个是带载抓放的业务推荐分级，仅用于 pick/drop 系列示教点诊断。
R_RECOMMENDED_LOAD = 250.0     # 推荐带载半径上限 (mm)
R_CAUTION_LOAD = 260.0         # 可接受调试区上限 (mm)；>260 进入边缘风险区

# 读数有效性边界（§12.2）：仅过滤明显异常的串口读数，不限制工作区。
COORD_VALID_XY = 500.0
COORD_VALID_Z_MIN = -100.0
COORD_VALID_Z_MAX = 500.0
COORD_VALID_RPY = 360.0

# V2.3 §14.4 优先级1：JSON 点位预设目录。预设只跳过手动示教采集，
# 加载后仍跑全部安全门（is_safe_coord/半径分区/validate_*/用户确认轨迹）。
# 文件名约定：teach_points_<name>.json，CLI 用 --preset <name> 引用。
PRESETS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "presets")

# V2.8 点3：自动导出终端日志目录。Tee 双写 stdout 把所有 print 同步写到
# auto_run_<时间戳>.log（UTF-8），便于实跑后归档/复盘。PC 专用工具，不上板。
AUTO_LOGS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "audit_logs")


# ============================================================
# V2.3 工具：异常类 + 日志格式化
# ============================================================
class TeachAbort(Exception):
    """V2.3 §14.4 优先级3：用户在示教阶段输入 q 放弃本轮示教。
    teach_phase 已 release_all_servos（舵机已掉电），main 捕获后直接安全退出，
    不需要再 release。与普通 RuntimeError 区分：后者表示硬件/通信故障，
    需扶稳再释放；本异常表示用户主动放弃，舵机已软。"""


def fmt_mm(value):
    """V2.3 §14.4 优先级2：坐标残差日志格式化兜底。
    _soft_refine 中 prev_coord_delta/coord_delta 可能为 None（无 expected_coords
    或读不到实际坐标时），直接 :.1f 会抛 TypeError。统一走本函数。
    """
    return "N/A" if value is None else f"{value:.1f}mm"


class Tee:
    """V2.8 点3：stdout 双写——同时输出到终端和 UTF-8 日志文件。
    main 开头用 `sys.stdout = Tee(path)` 安装，所有 print 自动双写；
    input() 的提示文本走 stdout 也会被记录（日志里能看到交互提示，便于复盘）。
    flush() 同步刷两路；close() 关闭文件。异常/Ctrl+C 路径由 main 的 try/finally
    调 close()，确保日志落盘。PC 专用工具，不参与运动/安全逻辑，不上板。
    """

    def __init__(self, file_path, original_stdout):
        os.makedirs(os.path.dirname(file_path), exist_ok=True)
        self._file = open(file_path, "w", encoding="utf-8")
        self._stdout = original_stdout
        self.path = file_path

    def write(self, msg):
        try:
            self._stdout.write(msg)
        except UnicodeEncodeError:
            enc = getattr(self._stdout, "encoding", None) or "gbk"
            try:
                self._stdout.write(msg.encode(enc, errors="replace").decode(enc))
            except Exception:
                pass
        self._file.write(msg)

    def flush(self):
        try:
            self._stdout.flush()
        except Exception:
            pass
        try:
            self._file.flush()
        except Exception:
            pass

    def close(self):
        try:
            self._file.flush()
            self._file.close()
        except Exception:
            pass

    def isatty(self):
        # 透传到原 stdout，避免 readline/input 行为异常
        return getattr(self._stdout, "isatty", lambda: False)()

    def __getattr__(self, name):
        # fileno()、encoding 等属性透传到原 stdout，保持兼容
        return getattr(self._stdout, name)


# ============================================================
# 串口选择（沿用 teach_and_pick.py 风格）
# ============================================================
def get_port():
    # V2.4 §14.6/§14.7：移除位置参数盲读。main() 已用 argparse 接管 --port，
    # get_port() 只在 --port 未给时被调用，应纯粹做 comports() 自动枚举 + 交互选择。
    # 旧逻辑 `if len(sys.argv) > 1: return sys.argv[1]` 会把 --save-preset 等可选
    # 参数误判为串口名（trial_run_14：could not open port '--save-preset'）。
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("【错误】未检测到任何串口设备，请检查连接！")
        sys.exit(1)

    candidates = []
    for p in ports:
        if "CP210" in p.description or "CH340" in p.description:
            candidates.append(p)

    if candidates:
        print("\n自动检测到以下可能的机械臂串口：")
        for idx, p in enumerate(candidates):
            print(f"  [{idx}] {p.device} - {p.description}")
        ans = input("请输入对应的序号 (直接回车默认选[0]): ")
        if ans.strip() == '':
            return candidates[0].device
        try:
            idx = int(ans)
            if 0 <= idx < len(candidates):
                return candidates[idx].device
        except Exception:
            pass
        print("【错误】无效输入，程序退出。")
        sys.exit(1)
    else:
        print("\n未自动识别到 CP210/CH340 设备。当前系统的所有串口如下：")
        for idx, p in enumerate(ports):
            print(f"  [{idx}] {p.device} - {p.description}")
        ans = input("请输入正确的机械臂串口序号: ")
        try:
            idx = int(ans)
            if 0 <= idx < len(ports):
                return ports[idx].device
        except Exception:
            pass
        print("【错误】无效输入，程序退出。")
        sys.exit(1)


# ============================================================
# 安全 / 滤波工具
# ============================================================
def is_safe_coord(coords, is_hover=False):
    """三维安全限制检查，返回 bool。仅用于 pick/drop 系列工作点。"""
    if not isinstance(coords, list) or len(coords) < 6:
        return False

    for val in coords:
        if not isinstance(val, (int, float)):
            print(f"【安全拦截】坐标包含非数字类型: {coords}")
            return False
        if math.isnan(val) or math.isinf(val):
            print(f"【安全拦截】坐标存在异常数值(NaN/Inf): {coords}")
            return False

    x, y, z, rx, ry, rz = coords

    min_z = Z_MIN_HOVER if is_hover else Z_MIN_PICK
    if z < min_z:
        print(f"【安全拦截】Z坐标 ({z}) 低于安全高度 ({min_z}mm)，存在撞击风险！")
        return False
    if z > Z_MAX:
        print(f"【安全拦截】Z坐标 ({z}) 过高，超出推荐工作空间！")
        return False

    radius = math.sqrt(x * x + y * y)
    if radius < R_MIN:
        print(f"【安全拦截】目标位置离底座太近 (R={radius:.1f} < {R_MIN})，极易自我碰撞！")
        return False
    if radius > R_MAX:
        print(f"【安全拦截】目标位置超出最大臂展 (R={radius:.1f} > {R_MAX})！")
        return False

    return True


def is_valid_coord_reading(coords):
    """
    读数有效性检查（§12.2）：只过滤明显异常的串口读数（非数字、NaN/Inf、
    超出生理可达范围的极端值），不限制抓取/放置工作区。
    直立安全姿态下 Z 可达 ~417mm，因此 Z 上限放宽到 COORD_VALID_Z_MAX=500。
    与 is_safe_coord 区别：is_safe_coord 是业务级工作区安全（Z_MAX=280 等），
    仅在保存示教点/主动发送笛卡尔目标前调用；本函数用于 get_filtered_coords
    内部读数过滤，避免直立高位坐标被误判无效。home_ready 也用本函数。
    """
    if not isinstance(coords, list) or len(coords) < 6:
        return False
    vals = coords[:6]
    if not all(isinstance(v, (int, float)) and math.isfinite(v) for v in vals):
        return False

    x, y, z, rx, ry, rz = vals
    if not (-COORD_VALID_XY <= x <= COORD_VALID_XY
            and -COORD_VALID_XY <= y <= COORD_VALID_XY
            and COORD_VALID_Z_MIN <= z <= COORD_VALID_Z_MAX):
        return False
    if not all(-COORD_VALID_RPY <= a <= COORD_VALID_RPY for a in (rx, ry, rz)):
        return False
    return True


def get_filtered_angles(mc, retries=ANG_RETRIES, stable_tol=ANG_STABLE_TOL):
    """
    多次读取关节角（§12.3）：先过数值合法性（数值型、有限、[-180,180]），
    再要求连续两次读数 6 维最大差值在 stable_tol 内才视为稳定返回。
    不再把首个合法读数立即当作可信姿态——第三次试运行 max_diff=78.9 而
    coords 显示接近直立高位，说明单次合法角度读数仍可能不可信。
    返回 6 维 list 或 None；返回 None 时上层必须拒绝保存示教点/拒绝回零。
    """
    valid = []
    for _ in range(retries):
        try:
            angles = mc.get_angles()
        except Exception as e:
            print(f"【警告】get_angles 抛异常: {type(e).__name__}: {e}，重试中...")
            time.sleep(0.15)
            continue

        if isinstance(angles, list) and len(angles) >= 6:
            vals = list(angles[:6])
            if all(isinstance(v, (int, float)) and math.isfinite(v) for v in vals):
                if all(-180.0 <= v <= 180.0 for v in vals):
                    valid.append(vals)
                    if len(valid) >= 2:
                        prev = valid[-2]
                        cur = valid[-1]
                        delta = max(abs(cur[i] - prev[i]) for i in range(6))
                        if delta <= stable_tol:
                            return cur
        time.sleep(0.15)
    return None


def get_angles_once(mc):
    """
    V2.5 §14.8 方向1/方向2：单次关节角读数 + 数值合法性校验，不做"连续两次稳定"要求。
    专为异步软到位轮询设计——臂在运动中，连续两次读数自然有差（>3°），
    get_filtered_angles 的稳定性门会拒绝运动中的读数返回 None，导致软到位循环
    一直"盲转"最终走 sync 兜底（异步化失效）。运动中的进度判定需要单次有效读数，
    不是稳定读数。稳定性要求（§12.3）只适用于静止姿态的示教点记录/回零决策。

    合法性校验保留：数值型、有限、[-180,180]（过滤串口毛刺/NaN/Inf）。
    返回 6 维 list 或 None（读异常或非法时）。上层软到位循环对 None 容错（跳过本轮）。
    """
    try:
        angles = mc.get_angles()
    except Exception:
        return None
    if not (isinstance(angles, list) and len(angles) >= 6):
        return None
    vals = list(angles[:6])
    if not all(isinstance(v, (int, float)) and math.isfinite(v) for v in vals):
        return None
    if not all(-180.0 <= v <= 180.0 for v in vals):
        return None
    return vals


def get_filtered_coords(mc, retries=COORD_RETRIES, stable_tol=COORD_STABLE_TOL):
    """
    多次读取空间坐标：先过 is_valid_coord_reading 读数有效性，再要求连续
    两次读数 XYZ 差值在 stable_tol 内才视为稳定。多次读取均异常时返回 None，
    不返回最后一次原始读数兜底。

    §12.2：只调用 is_valid_coord_reading()，不再调用 is_safe_coord()。
    工作区安全（Z_MAX=280 等）是业务级判断，由 record_teach_point/
    checked_short_linear 在"保存示教点/主动发送笛卡尔目标前"调用 is_safe_coord。
    直立高位坐标（如 Z=416.9）不会被本函数误判无效。
    """
    valid = []
    for _ in range(retries):
        try:
            coords = mc.get_coords()
        except Exception as e:
            print(f"【警告】get_coords 抛异常: {type(e).__name__}: {e}，重试中...")
            time.sleep(0.15)
            continue

        if is_valid_coord_reading(coords):
            vals = list(coords[:6])
            valid.append(vals)
            if len(valid) >= 2:
                prev = valid[-2]
                cur = valid[-1]
                delta = max(abs(cur[i] - prev[i]) for i in range(3))
                if delta <= stable_tol:
                    return cur
        time.sleep(0.15)
    return None


# ============================================================
# 示教点数据结构 / 采集
# ============================================================
def make_teach_point(name, angles, coords, is_hover):
    return {
        "name": name,
        "angles": angles,
        "coords": coords,
        "is_hover": is_hover,
    }


def print_radius_diagnostic(coords, label):
    """
    V2 §3：打印 pick/drop 系列示教点的带载半径分区诊断。
    返回 True 表示无需额外确认（推荐/可接受区）；
    返回 False 表示处于边缘风险区（260<R<=280），需调用方做显式确认。
    R>R_MAX 已由 is_safe_coord 硬拦截，本函数不会再遇到。
    """
    x, y, z = coords[0], coords[1], coords[2]
    r = math.sqrt(x * x + y * y)
    if r <= R_RECOMMENDED_LOAD:
        print(f"  -> {label} R={r:.1f}mm  [推荐带载区 R<={R_RECOMMENDED_LOAD}]")
        return True
    if r <= R_CAUTION_LOAD:
        print(f"  -> {label} R={r:.1f}mm  【可接受调试区】"
              f"建议把物块/放置点往底座内侧移 10~20mm 到 R<={R_RECOMMENDED_LOAD}mm。")
        return True
    print(f"  -> {label} R={r:.1f}mm  【边缘风险区 R>{R_CAUTION_LOAD}】"
          f"强烈建议重新摆放物块到 R<={R_RECOMMENDED_LOAD}mm，带载远端力矩大。")
    return False


def record_teach_point(mc, name, is_hover):
    """
    采集一个 pick/drop 系列示教点：要求用户手动拖动到位、稳定后回车读取，
    angles + coords 必须同时合法；半径分区诊断 + 边缘风险区显式确认；
    用户二次确认后才保存。

    V2.3：超安全边界或用户取消时不再抛异常退出，而是循环让用户重新拖动，
    避免中途掉电释放舵机。（run-12 用户反馈：第一轮 pick_hover R=288.3 超
    R_MAX=280，脚本直接抛异常退出，需要重跑整个流程。）
    V2.3 §14.4 优先级3：输入 q 放弃本轮示教，抛 TeachAbort 让 main 安全退出
    （teach 阶段舵机已掉电，无需再 release）。
    """
    while True:
        ans = input(f"-> 请手动拖动到【{name}】，保持稳定后按 Enter 读取"
                    f"（输入 q 放弃本轮示教）...")
        if ans.strip().lower() == "q":
            raise TeachAbort(f"用户在示教【{name}】时放弃本轮示教")
        angles = get_filtered_angles(mc)
        coords = get_filtered_coords(mc)

        if angles is None:
            print(f"  -> 【重试】{name}: 无法读取稳定关节角，请重新拖动。")
            continue
        if coords is None:
            print(f"  -> 【重试】{name}: 无法读取稳定空间坐标，请重新拖动。")
            continue
        if not is_safe_coord(coords, is_hover=is_hover):
            print(f"  -> 【重试】{name}: 坐标不在安全范围内，"
                  f"请重新调整物块位置后重新拖动。")
            continue

        # V2 §3：半径分区诊断 + 边缘风险区显式确认
        ok_without_confirm = print_radius_diagnostic(coords, name)
        if not ok_without_confirm:
            ans = input(f"  -> {name} 处于边缘风险区，仍要保存该示教点吗？"
                        f"(y/n): ")
            if ans.strip().lower() == "q":
                raise TeachAbort(f"用户在【{name}】边缘风险区确认时放弃本轮示教")
            if ans.strip().lower() != "y":
                print("  -> 已取消保存，请重新调整物块位置后重新拖动。")
                continue

        print(f"{name} angles = {angles}")
        print(f"{name} coords  = {coords}")
        ans = input("确认保存该示教点吗？(y/n，q 放弃本轮): ")
        if ans.strip().lower() == "q":
            raise TeachAbort(f"用户在保存【{name}】时放弃本轮示教")
        if ans.lower() == "y":
            return make_teach_point(name, angles, coords, is_hover)
        print("  -> 已取消保存，请重新拖动。")


def record_return_ready_point(mc):
    """
    V2.1 §12.4：采集 home_ready 回零中间姿态。与 pick/drop 系列的区别：
      - 不套抓放业务门 Z_MAX=280 / Z_MIN / R_MIN（home_ready 是高位空中姿态，
        直立附近 Z 可达 300~400mm，套 Z_MAX 会误拦合法 home_ready）。
      - 保留 R_MAX=280 物理臂展硬限（对所有点生效）。
      - V2.1 关键门：示教目标 arm_max_diff<=HOME_READY_TARGET_ARM_MAX(40°)。
        run-10 证明 43.1（离 45 门 1.9°）的示教点软通过后被 3° 软容差推过 45；
        故示教目标须留 >= SOFT_ANGLE_SUCCESS_TOL 的余量（45-3=42，取 40 更稳）。
      - 两级门：<=40 推荐 / 40~45 警告但仍允许保存 / >45 禁止。
    示教期加重新示教循环，提示语引用 run-10 扶正后实测可达姿态。
    """
    while True:
        ans = input("-> 请手动拖动到【home_ready 回零中间姿态】"
              "（夹爪朝前/略向下，尽量接近直立，不要扫到桌面/物块/线缆），"
              "稳定后按 Enter 读取（输入 q 放弃本轮示教）...")
        if ans.strip().lower() == "q":
            raise TeachAbort("用户在示教【home_ready】时放弃本轮示教")
        angles = get_filtered_angles(mc)
        coords = get_filtered_coords(mc)

        if angles is None:
            print("【错误】无法读取稳定关节角，请重新拖动。")
            continue
        if coords is None or not is_valid_coord_reading(coords):
            print("【错误】无法读取有效空间坐标，请重新拖动。")
            continue

        x, y, z = coords[0], coords[1], coords[2]
        r = math.sqrt(x * x + y * y)
        # 物理臂展硬限（业务级 Z_MAX/Z_MIN/R_MIN 不适用 home_ready）
        if r > R_MAX:
            print(f"【安全拦截】home_ready 超出最大臂展 (R={r:.1f} > {R_MAX})！")
            continue

        all_diffs, arm_diffs, arm_max_diff, wrist6_diff = calc_home_diffs(angles)
        print(f"home_ready angles = {angles}")
        print(f"home_ready coords  = {coords}  (R={r:.1f}mm, Z={z:.1f}mm)")
        print(f"home_ready 相对 HOME: arm_diffs={arm_diffs}, "
              f"arm_max_diff={arm_max_diff:.1f}, wrist6_diff={wrist6_diff:.1f}")

        # V2.1 §12.4-2：两级门
        if arm_max_diff > ARM_MAX_DIFF_SAFE:
            # >45：禁止保存。run-10 证明离门太近会被软通过推过界，更不能放行。
            print(f"【拦截】home_ready arm_max_diff={arm_max_diff:.1f} "
                  f"> {ARM_MAX_DIFF_SAFE}，未进自动回零安全门。")
            print("-> 示教建议：把大臂/小臂再往直立方向扶，重点把 J2 绝对值"
                  "压到约 35~38°、J3 接近 0~5°（J4/J5 同向小角度）。")
            print(f"-> 参考 run-10 扶正后实测可达姿态 "
                  f"[7.99, -36.38, 3.51, 12.12, -4.48, 54.75] (arm_max_diff=36.4)。")
            continue  # 强制重新示教，不给"仍然保存"逃生口

        if arm_max_diff > HOME_READY_TARGET_ARM_MAX:
            # 40~45：允许保存但强警告，run-10 即落此区间导致软通过越界。
            print(f"【警告】home_ready arm_max_diff={arm_max_diff:.1f} "
                  f"在 {HOME_READY_TARGET_ARM_MAX}~{ARM_MAX_DIFF_SAFE} 余量不足区间。")
            print(f"-> 软到位容差 {SOFT_ANGLE_SUCCESS_TOL}° 可能像 run-10 一样把实际 "
                  f"arm_max_diff 推过 {ARM_MAX_DIFF_SAFE}，导致仍需人工扶正。")
            print("-> 强烈建议重新示教更直立的姿态（J2 绝对值压到 35~38°、"
                  "J3 接近 0~5°）。")
            ans = input("重新示教 home_ready？(y=重新示教 / n=仍然保存此贴边点 / q 放弃本轮): ")
            if ans.strip().lower() == "q":
                raise TeachAbort("用户在 home_ready 余量警告时放弃本轮示教")
            if ans.strip().lower() == "y":
                continue
            # 用户坚持保存贴边点 -> 自动阶段 validate_return_ready 仍按 <=45 放行，
            # 但 auto 第10步新增的实际姿态校验+低速重试会尝试补救。
        else:
            # <=40：推荐，留 >=5° 余量，软通过后仍 <45。
            print(f"-> home_ready 通过推荐余量门 "
                  f"(arm_max_diff={arm_max_diff:.1f} <= {HOME_READY_TARGET_ARM_MAX}，"
                  f"留 {ARM_MAX_DIFF_SAFE - arm_max_diff:.1f}° 软通过余量)。")

        ans = input("确认保存该 home_ready 示教点吗？(y/n，q 放弃本轮): ")
        if ans.strip().lower() == "q":
            raise TeachAbort("用户在保存 home_ready 时放弃本轮示教")
        if ans.strip().lower() == "y":
            return make_teach_point("回零中间点 home_ready", angles, coords, is_hover=True)
        # 否则继续循环重新示教


# ============================================================
# 带返回值检查的动作封装
# ============================================================
def checked_sync_angles(mc, angles, speed, timeout, label):
    """关节角同步回放。失败抛异常，不静默继续。"""
    print(f"  -> 关节回放到 {label}: {angles}")
    res = mc.sync_send_angles(angles, speed, timeout=timeout)
    if res != 1:
        raise RuntimeError(f"关节回放超时或未到位: {label} (返回 {res})")
    return True


def checked_short_angles(
    mc,
    target_angles,
    speed,
    timeout,
    label,
    expected_coords=None,
    allow_soft_success=False,
):
    """
    §16.4：短距离关节角回放，用于 hover <-> pick/drop。
    彻底绕开 sync_send_coords() 的笛卡尔目标 IK 解算——固件端只做
    关节空间插值，不再把坐标目标解算为关节角，规避远端低位/腕部
    多解/边界点快速返回 0 的熔断。
    注意：末端不保证严格垂直直线运动，物块位置必须与示教时一致，
    且示教的 hover/down 点必须保证中间关节路径不会扫到物块/桌面/夹具。

    §16.4 新增软到位成功判定：sync_send_angles 返回 != 1 后读取
    实际角度/坐标，若 max_angle_delta <= SOFT_ANGLE_SUCCESS_TOL 且
    delta_xyz <= SOFT_COORD_SUCCESS_TOL，允许软通过（不抛异常）。
    软通过后续仍必须执行 verify_coords_near()。

    V2 修正 D：本函数也用于 drop_hover -> home_ready 的回零过渡
    （见 checked_return_transition），故不强制"短距离"语义——
    它本质是 sync_send_angles + 软到位 + 诊断，长距离调用同样安全。
    """
    if not isinstance(target_angles, list) or len(target_angles) < 6:
        raise RuntimeError(f"{label}: 目标关节角非法: {target_angles}")
    if not all(isinstance(v, (int, float)) and math.isfinite(v)
               and -180.0 <= v <= 180.0 for v in target_angles[:6]):
        raise RuntimeError(f"{label}: 目标关节角含非数值或越界: {target_angles}")
    print(f"  -> 关节回放到 {label}: {target_angles} "
          f"(speed={speed}, timeout={timeout}s)")
    res = mc.sync_send_angles(target_angles, speed, timeout=timeout)
    if res == 1:
        return True

    # §16.4：失败后诊断读取 + 软到位判定
    print(f"  -> [诊断] {label}: sync_send_angles 返回 {res}")
    time.sleep(SOFT_SETTLE_SECONDS)
    actual_angles = get_filtered_angles(mc)
    actual_coords = get_filtered_coords(mc)

    if actual_angles:
        angle_deltas = [abs(actual_angles[i] - target_angles[i]) for i in range(6)]
        max_angle_delta = max(angle_deltas)
        print(f"  -> [诊断] {label}: 实际关节角={actual_angles}")
        print(f"  -> [诊断] {label}: 与目标关节角差值={angle_deltas}, "
              f"max={max_angle_delta:.1f}°")
    else:
        max_angle_delta = None
        print(f"  -> [诊断] {label}: 无法读取稳定实际关节角")

    if actual_coords:
        print(f"  -> [诊断] {label}: 实际坐标={actual_coords}")
    else:
        print(f"  -> [诊断] {label}: 无法读取稳定实际坐标")

    if allow_soft_success and max_angle_delta is not None:
        angle_ok = max_angle_delta <= SOFT_ANGLE_SUCCESS_TOL
        coord_ok = True
        coord_delta = None
        if expected_coords is not None and actual_coords is not None:
            coord_delta = max(abs(actual_coords[i] - expected_coords[i])
                              for i in range(3))
            coord_ok = coord_delta <= SOFT_COORD_SUCCESS_TOL
            print(f"  -> [诊断] {label}: 软到位坐标 delta_xyz="
                  f"{coord_delta:.1f} mm")
        elif expected_coords is not None and actual_coords is None:
            # §16.4 执行要求：expected_coords 已提供但 actual_coords 读不到，
            # 建议不通过以策安全。
            print(f"  -> [诊断] {label}: 无法读取坐标，不允许软通过")
            coord_ok = False

        # V2.2 §13.3 优先级 1：软通过后低速二次微调。
        # 仅对 allow_soft_success=True 启用（step 5/9 上行 + step 10 回零过渡）。
        # 触发条件：角差 > SOFT_REFINE_ANGLE_TRIGGER 或 坐标差 > SOFT_REFINE_COORD_TRIGGER
        # （用"或"——任一超阈值即微调）。微调后重新读角度/坐标，达标判定用"或"
        # （角差<=OK 或 坐标差<=OK 即通过）。仍不达标但 <=旧软通过门(3°/25mm) 则
        # 标"软通过但残差偏大"继续；都不达标且超旧门则抛异常。
        if (SOFT_REFINE_ENABLE and angle_ok and coord_ok
                and actual_angles is not None
                and (max_angle_delta > SOFT_REFINE_ANGLE_TRIGGER
                     or (coord_delta is not None
                         and coord_delta > SOFT_REFINE_COORD_TRIGGER))):
            max_angle_delta, coord_delta, actual_angles, actual_coords = (
                _soft_refine(mc, target_angles, label, max_angle_delta,
                             coord_delta, expected_coords))

        if angle_ok and coord_ok:
            # 微调后重新判定 angle_ok/coord_ok（基于微调后的 max_angle_delta/coord_delta）
            angle_ok = max_angle_delta <= SOFT_ANGLE_SUCCESS_TOL
            if coord_delta is not None:
                coord_ok = coord_delta <= SOFT_COORD_SUCCESS_TOL
            if angle_ok and coord_ok:
                # V2.2 残差强警告（Run B step 9 16.2mm 即此级别）
                if coord_delta is not None and coord_delta > SOFT_REFINE_WARN_COORD:
                    print(f"  -> 【强警告】{label}: 软通过但残差偏大 "
                          f"delta_xyz={coord_delta:.1f}mm > "
                          f"{SOFT_REFINE_WARN_COORD}mm，3cm 物块有抓取失败风险。")
                print(f"  -> [软通过] {label}: 固件返回 {res}，但实际姿态已在容差内，"
                      f"继续执行后续坐标校验。")
                return True

    raise RuntimeError(f"{label}: 关节动作超时或失败 (返回 {res})")


def checked_short_angles_async(
    mc, target_angles, speed, timeout, label, expected_coords=None,
):
    """V2.8 点1：step 9 (drop→drop_hover 带载上行) 异步软到位版短距离回放。

    根因：run-18 实测 step 9 用阻塞 sync_send_angles(timeout=15s)，固件 is_in_position
    死区让残差稳态卡 2.1°（三轮完全一致），永远等满 timeout 返回 0；再加 _soft_refine
    微调 3s 也不收敛，step 9 稳定 20s，其中物理运动只占 1.5s。

    方案：复用 V2.5 _send_home_async 的"非阻塞 send_angles + Python 软到位循环"模式，
    但目标从 HOME 改成任意 target_angles（drop_hover）。残差≤ASYNC_SHORT_SOFT_TOL(3°)
    即软通过退出；软超时走阻塞 sync_send_angles 收尾兜底（res!=1 抛异常熔断，保留
    V2.4 安全失败语义）。用 get_angles_once 单次读数（臂运动中不能用 get_filtered_angles
    的连续两次稳定性门，否则软到位循环盲转——V2.5 mock 暴露的关键设计点）。

    与 checked_short_angles 的区别：
      - 不用阻塞 sync_send_angles 等固件判到位（省 15s 死区等待）。
      - 软通过后不再 _soft_refine（残差 2.1° 是固件死区，微调三轮未收敛，纯浪费 3s）。
      - 软通过容差仍是 SOFT_ANGLE_SUCCESS_TOL(3°)/SOFT_COORD_SUCCESS_TOL(25mm)，不放宽。
      - 软通过后由调用方决定是否 verify_coords_near（step 9 仍校验，因带载上行软通过需复核）。

    安全守住：
      - 目标角度合法性校验同 checked_short_angles（数值型/有限/[-180,180]）。
      - 软超时不抛异常（防臂在运动路径掉电），走 sync 收尾 + res 检查；res!=1 才抛异常。
      - 软通过判定基于实际读数（angle_ok + coord_ok），不假设到位。
      - V2.9 二次读数确认：连续 ASYNC_SHORT_CONFIRM_COUNT 次读数都 ≤tol 才判收敛，
        拒绝减速振荡中"单帧瞬态下探 ≤tol"的假收敛。N=1 退化为 V2.8 单次确认。
    返回 True（软通过或 sync 收尾 res==1）；抛 RuntimeError（sync 收尾 res!=1 或读数异常）。
    ASYNC_SHORT_ENABLE=False 时调用方走原 checked_short_angles，本函数不被调用。
    """
    if not isinstance(target_angles, list) or len(target_angles) < 6:
        raise RuntimeError(f"{label}: 目标关节角非法: {target_angles}")
    if not all(isinstance(v, (int, float)) and math.isfinite(v)
               and -180.0 <= v <= 180.0 for v in target_angles[:6]):
        raise RuntimeError(f"{label}: 目标关节角含非数值或越界: {target_angles}")

    print(f"  -> [V2.8 异步] {label}: 非阻塞 send_angles (speed={speed})，"
          f"软到位循环 (tol={ASYNC_SHORT_SOFT_TOL}°, timeout={ASYNC_SHORT_TIMEOUT}s, "
          f"confirm={ASYNC_SHORT_CONFIRM_COUNT})...")
    mc.send_angles(target_angles, speed)
    start = time.time()
    converged = False
    final_max_delta = None
    final_coord_delta = None
    confirm_count = 0               # V2.9：连续确认计数器
    none_count = 0                  # V2.12：get_angles_once 返回 None 的帧数（Codex B 诊断）
    while time.time() - start < ASYNC_SHORT_TIMEOUT:
        actual = get_angles_once(mc)  # 单次读数：臂运动中不能用 get_filtered_angles 稳定性门
        if actual:
            angle_deltas = [abs(actual[i] - target_angles[i]) for i in range(6)]
            final_max_delta = max(angle_deltas)
            angle_ok = final_max_delta <= ASYNC_SHORT_SOFT_TOL
            # 坐标软通过：expected_coords 给定时才校验，读不到坐标不阻断（角度已判）
            coord_ok = True
            if expected_coords is not None:
                actual_coords = mc.get_coords()
                if isinstance(actual_coords, list) and len(actual_coords) >= 3:
                    final_coord_delta = max(abs(actual_coords[i] - expected_coords[i])
                                            for i in range(3))
                    coord_ok = final_coord_delta <= SOFT_COORD_SUCCESS_TOL
            if angle_ok and coord_ok:
                # V2.9 二次读数确认：要求连续 ASYNC_SHORT_CONFIRM_COUNT 次读数都 ≤tol 才判收敛，
                # 拒绝减速振荡中"单帧瞬态下探 ≤tol"的假收敛（臂还在晃就退出）。
                # N=1 时即原 V2.8 单次确认行为。
                # V2.12（Codex B）：None 不重置 confirm_count——语义是"连续有效 OK 读数"
                # 而非严格"连续轮询 OK 读数"，偶发一帧 None 被容差（优点）。
                confirm_count += 1
                if confirm_count >= ASYNC_SHORT_CONFIRM_COUNT:
                    converged = True
                    break
                # 未达连续确认次数：继续轮询（不 break），下一帧再读
            else:
                # 任一条件未满足：重置连续确认计数器
                confirm_count = 0
        else:
            # V2.12（Codex B 诊断）：累计 None 帧数，便于区分"运动未收敛" vs "串口读数不可用"
            none_count += 1
        time.sleep(ASYNC_SHORT_POLL)
    elapsed = time.time() - start

    if converged:
        cd = fmt_mm(final_coord_delta)
        print(f"  -> [V2.8 异步] {label}: 软到位收敛 max_err={final_max_delta:.2f}° "
              f"/ delta_xyz={cd} <= 容差，耗时 {elapsed:.2f}s（提前退出固件死区等待）。")
        return True

    # V2.12（Codex B 诊断）：软超时未收敛时打印 None 帧数 + 末态 confirm_count，
    # 让下一次熔断能区分"运动真未收敛"与"读数不可用"。
    fd = f"{final_max_delta:.2f}" if final_max_delta is not None else "N/A"
    print(f"  -> [V2.8 异步] {label}: 软超时未收敛（max_err={fd}°, "
          f"none_count={none_count}, confirm_count={confirm_count}），"
          f"阻塞 sync_send_angles 收尾兜底 (speed={speed}, timeout={timeout}s)...")
    res = mc.sync_send_angles(target_angles, speed, timeout=timeout)
    print(f"  -> [V2.8 异步] {label}: 收尾 sync_send_angles 返回 {res}")
    if res != 1:
        # V2.12（Codex C 缺陷B 修复）：sync 返回 0 不直接熔断——固件可能"假失败"
        # （实测 step9 在残差 2.1° 上行 sync 可返回 0，历史参数见 L204-L212/L278-L283）。
        # 先做 post-failure 复读（角度+坐标），区分"真没到位" vs "固件确认假失败"：
        #   - 复读在软容差内 → 软通过 + 强警告（return True，不熔断）；
        #   - 复读超容差 → 重发 1 次已验证低速关节目标（speed=8）再复读；
        #   - 仍超容差 → 按 V2.4 安全失败语义抛异常熔断（保守失败边界保留）。
        # 有界：仅重试 1 次；不无限循环；retry 期间不释放舵机（臂在路径上不掉电）。
        # 板上 CPU 固件迁移时下放同等策略（AGENTS.md 决赛主线硬边界：PC 仅调试，不进闭环）。
        post_angles = get_filtered_angles(mc)
        post_coords = get_filtered_coords(mc)
        post_max_err = None
        post_delta = None
        if post_angles:
            post_max_err = max(abs(post_angles[i] - target_angles[i]) for i in range(6))
        if expected_coords is not None and post_coords:
            post_delta = max(abs(post_coords[i] - expected_coords[i]) for i in range(3))
        print(f"  -> [V2.8 异步] {label}: post-failure 复读 max_err="
              f"{(f'{post_max_err:.2f}' if post_max_err is not None else 'N/A')}° / "
              f"delta_xyz={fmt_mm(post_delta)}")
        # 判定 1：复读在软容差内 → 软通过 + 强警告（保留 SOFT_REFINE_WARN_COORD 强警告语义）
        post_angle_ok = post_max_err is not None and post_max_err <= ASYNC_SHORT_SOFT_TOL
        post_coord_ok = post_delta is None or post_delta <= SOFT_COORD_SUCCESS_TOL
        if post_angle_ok and post_coord_ok:
            warn = ""
            if post_delta is not None and post_delta > SOFT_REFINE_WARN_COORD:
                warn = f" 【强警告】残差 delta_xyz={post_delta:.1f}mm > {SOFT_REFINE_WARN_COORD}mm，3cm 物块有抓取失败风险。"
            print(f"  -> [V2.8 异步] {label}: sync 返回 {res} 但复读实测在软容差内，"
                  f"按软通过处理（不熔断）。{warn}")
            return True
        # 判定 2：复读超容差 → 重发 1 次低速关节目标再复读
        print(f"  -> [V2.8 异步] {label}: 复读超容差，重发低速关节目标 "
              f"(speed={SOFT_REFINE_SPEED}, timeout={SOFT_REFINE_TIMEOUT}s) 再判熔断...")
        try:
            mc.sync_send_angles(target_angles, SOFT_REFINE_SPEED,
                                timeout=SOFT_REFINE_TIMEOUT)
        except Exception as e:
            print(f"  -> [V2.8 异步] {label}: 重发 sync 抛异常 {e}，按熔断处理。")
            raise RuntimeError(f"{label}: 异步软超时 + sync res={res} + 重发异常 ({e})")
        time.sleep(SOFT_SETTLE_SECONDS)
        rt_angles = get_filtered_angles(mc)
        rt_coords = get_filtered_coords(mc)
        rt_max_err = None
        rt_delta = None
        if rt_angles:
            rt_max_err = max(abs(rt_angles[i] - target_angles[i]) for i in range(6))
        if expected_coords is not None and rt_coords:
            rt_delta = max(abs(rt_coords[i] - expected_coords[i]) for i in range(3))
        print(f"  -> [V2.8 异步] {label}: 重发后复读 max_err="
              f"{(f'{rt_max_err:.2f}' if rt_max_err is not None else 'N/A')}° / "
              f"delta_xyz={fmt_mm(rt_delta)}")
        rt_angle_ok = rt_max_err is not None and rt_max_err <= ASYNC_SHORT_SOFT_TOL
        rt_coord_ok = rt_delta is None or rt_delta <= SOFT_COORD_SUCCESS_TOL
        if rt_angle_ok and rt_coord_ok:
            print(f"  -> [V2.8 异步] {label}: 重发后复读在软容差内，按软通过处理（不熔断）。")
            return True
        # 判定 3：重发后仍超容差 → 抛异常熔断（V2.4 安全失败语义）
        fd2 = f"{rt_max_err:.2f}" if rt_max_err is not None else (
            f"{post_max_err:.2f}" if post_max_err is not None else "N/A")
        raise RuntimeError(
            f"{label}: 异步软超时 + sync res={res} + 重发后仍超容差 (max_err={fd2}°)")
    return True



def _soft_refine(mc, target_angles, label, prev_max_angle_delta,
                 prev_coord_delta, expected_coords):
    """
    V2.2 §13.3 优先级 1 内部辅助：软通过后追加一次低速短时微调。
    在 checked_short_angles 内部调用，仅当残差超 SOFT_REFINE_*_TRIGGER 时触发。
    最多 1 轮（SOFT_REFINE_MAX_ROUNDS=1）。返回微调后的
    (max_angle_delta, coord_delta, actual_angles, actual_coords)。

    不抛异常：微调后仍不达标由调用方按旧软通过门(3°/25mm)判定，保留兜底。
    不恢复 sync_send_coords()：微调目标是关节角，固件只做关节空间插值。
    """
    print(f"  -> [V2.2 微调] {label}: 残差 max_err={prev_max_angle_delta:.1f}°"
          f"/delta_xyz={fmt_mm(prev_coord_delta)} 超微调触发阈值，"
          f"低速二次微调 (speed={SOFT_REFINE_SPEED}, "
          f"timeout={SOFT_REFINE_TIMEOUT}s)...")
    res = mc.sync_send_angles(
        target_angles, SOFT_REFINE_SPEED, timeout=SOFT_REFINE_TIMEOUT)
    print(f"  -> [V2.2 微调] {label}: sync_send_angles 返回 {res}")
    time.sleep(SOFT_SETTLE_SECONDS)
    actual_angles = get_filtered_angles(mc)
    actual_coords = get_filtered_coords(mc)

    if actual_angles is None:
        print(f"  -> [V2.2 微调] {label}: 微调后无法读取实际角度，保留微调前残差。")
        return prev_max_angle_delta, prev_coord_delta, actual_angles, actual_coords

    angle_deltas = [abs(actual_angles[i] - target_angles[i]) for i in range(6)]
    max_angle_delta = max(angle_deltas)
    coord_delta = None
    if expected_coords is not None and actual_coords is not None:
        coord_delta = max(abs(actual_coords[i] - expected_coords[i])
                          for i in range(3))
    # V2.3 §14.4 优先级2：coord_delta/prev_coord_delta 统一走 fmt_mm，避免 None 时
    # :.1f 抛 TypeError（run-12 未暴露因 step5/9/10 都传了 expected_coords，但
    # 未来复用到无期望坐标的动作时会崩）。
    print(f"  -> [V2.2 微调] {label}: 微调后 max_err={max_angle_delta:.1f}° "
          f"(前 {prev_max_angle_delta:.1f}°), delta_xyz={fmt_mm(coord_delta)} "
          f"(前 {fmt_mm(prev_coord_delta)})")
    if max_angle_delta <= SOFT_REFINE_ANGLE_OK or (
            coord_delta is not None and coord_delta <= SOFT_REFINE_COORD_OK):
        print(f"  -> [V2.2 微调] {label}: 微调达标（角差<={SOFT_REFINE_ANGLE_OK}° "
              f"或 delta_xyz<={SOFT_REFINE_COORD_OK}mm）。")
    else:
        print(f"  -> [V2.2 微调] {label}: 微调后仍未达精细门，按旧软通过门兜底。")
    return max_angle_delta, coord_delta, actual_angles, actual_coords


def checked_return_transition(mc, target_point, label):
    """
    V2.1 §12.4-4：drop_hover -> home_ready 的回零过渡。
    底层仍用 sync_send_angles（符合 plan §5 "长距离用 sync_send_angles"），
    但走 checked_short_angles 的软到位 + 诊断路径，而非 checked_sync_angles
    （无软通过、超时即抛）。

    原因：该过渡后 safe_return_home() 会重新读取实际角度并独立判断
    arm_max_diff<=45，因此即使固件 is_in_position 未判定到位、但实际关节角
    已接近 home_ready（max_angle_delta<=3°），软通过让流程继续到
    safe_return_home 是更安全的——避免臂停在半空过渡姿态时整流程中断、
    触发异常掉电。若实际偏差大（>3°），软通过不触发，仍按异常处理。

    V2.1 新增（run-10 复核）：软通过后追加"实际姿态安全门"校验。
      run-10 病灶：示教 home_ready arm_max_diff=43.1（离 45 门 1.9°），软通过
      后实际 J2 多偏 2.29° 使实际 arm_max_diff=45.4 越界，safe_return_home
      仍进人工扶正。软通过只校验"是否接近示教目标"，不校验"实际姿态是否仍
      在自动回零安全门内"。本函数补这一层：
        - 软通过后读实际角度，算实际 arm_max_diff。
        - <=ARM_MAX_DIFF_SAFE：放行进 safe_return_home（走自动回零）。
        - >ARM_MAX_DIFF_SAFE：低速重试一次 sync_send_angles(home_ready, 15, 20s)。
          重试目标仍是示教 home_ready（<=40），命令朝 HOME 方向挪 ~5°，单向安全：
          再软通过落到 42~43 仍 <45 -> 自动回零；最坏仍 >45 落到 safe_return_home
          人工扶正，不比不重试差。
        - 重试后仍 >45：不抛异常（抛了会触发 main 的异常掉电，臂在半空危险），
          而是返回让 auto_phase_v2 落到 safe_return_home 人工扶正状态机。

    V2.4 §14.7 L2：返回校验完成时的实际角度（或重试后的角度），供调用链透传
    给 safe_return_home(cached_angles=...)，避免 auto 末段 safe_return_home
    在同一静止姿态上再读一次 get_filtered_angles（~0.25-0.4s 冗余读数）。
    越界走人工扶正时返回 None（不缓存，safe_return_home 会自行重读）。
    """
    checked_short_angles(
        mc, target_point["angles"], ANG_REPLAY_SPEED, ANG_REPLAY_TIMEOUT, label,
        expected_coords=target_point["coords"],
        allow_soft_success=True,
    )
    # V2.1：软通过后做实际姿态安全门校验（sync_send_angles 返回 1 的严格通过
    # 也走这一层，因固件返回 1 不保证 arm_max_diff<=45，只是"固件判到位"）。
    return _verify_actual_pose_for_auto_return(mc, target_point, label)


def _verify_actual_pose_for_auto_return(mc, target_point, label):
    """
    V2.1 §12.4-4 内部辅助：软通过/严格通过后校验实际 arm_max_diff，
    越界则低速重试一次 home_ready，仍越界则打印诊断交还上层走人工扶正。
    不抛异常——臂在半空过渡姿态时抛异常会触发 main 的异常掉电路径，
    不如让 safe_return_home 的人工扶正状态机接管（它会先扶稳再 release）。

    V2.4 §14.7 L2：返回校验通过时的实际角度，供上层透传给
    safe_return_home(cached_angles=...) 跳过 auto 分支的冗余重读。
    越界/读不到角度/走人工扶正时返回 None（上层不缓存，safe_return_home
    会自行重读，保证安全门判定始终基于新鲜读数）。
    """
    actual = get_filtered_angles(mc)
    if actual is None:
        print(f"  -> [V2.1] {label}: 软通过后无法读取实际角度，"
              f"交 safe_return_home 自行判断。")
        return None
    _, _, arm_max_diff, _ = calc_home_diffs(actual)
    print(f"  -> [V2.1] {label}: 实际姿态 arm_max_diff={arm_max_diff:.1f} "
          f"(门 {ARM_MAX_DIFF_SAFE})")
    if arm_max_diff <= ARM_MAX_DIFF_SAFE:
        print(f"  -> [V2.1] {label}: 实际姿态在自动回零安全门内，"
              f"safe_return_home 将走自动回零。")
        return actual  # V2.4 L2：缓存供 safe_return_home 跳过冗余重读

    # 越界：低速重试一次（命令朝 HOME 方向挪，单向安全）
    print(f"  -> [V2.1] {label}: 实际 arm_max_diff={arm_max_diff:.1f} > "
          f"{ARM_MAX_DIFF_SAFE}（run-10 同款软通过越界），低速重试一次 "
          f"home_ready (speed={HOME_READY_RETRY_SPEED}, "
          f"timeout={HOME_READY_RETRY_TIMEOUT}s)...")
    res = mc.sync_send_angles(
        target_point["angles"], HOME_READY_RETRY_SPEED,
        timeout=HOME_READY_RETRY_TIMEOUT)
    print(f"  -> [V2.1] {label}: 重试 sync_send_angles 返回 {res}")
    time.sleep(SOFT_SETTLE_SECONDS)
    actual = get_filtered_angles(mc)
    if actual is None:
        print(f"  -> [V2.1] {label}: 重试后无法读取实际角度，"
              f"交 safe_return_home 人工扶正。")
        return None
    _, _, arm_max_diff, _ = calc_home_diffs(actual)
    print(f"  -> [V2.1] {label}: 重试后实际 arm_max_diff={arm_max_diff:.1f}")
    if arm_max_diff <= ARM_MAX_DIFF_SAFE:
        print(f"  -> [V2.1] {label}: 重试后进入自动回零安全门，"
              f"safe_return_home 将走自动回零。")
        return actual  # V2.4 L2：缓存重试后角度
    print(f"  -> [V2.1] {label}: 重试后仍 arm_max_diff={arm_max_diff:.1f} > "
          f"{ARM_MAX_DIFF_SAFE}，交 safe_return_home 人工扶正状态机接管。"
          f"（建议下一轮示教更直立的 home_ready，J2 绝对值压到 35~38°。）")
    return None


def validate_short_angle_pair(src_point, dst_point, label):
    """
    §14.4：短距离 hover <-> down 点对的关节连续性校验。
    全角度回放可以绕开固件 IK，但不能无条件信任所有示教角度——
    若 hover/down 落在不同 IK 分支（如 run-5 的 drop_hover -> drop
    关节 3/4/6 差异巨大），直接按角度短距离回放会产生大幅摆动或
    腕部翻转。校验失败抛异常，要求用户重新示教对应点位。
    """
    src = src_point["angles"]
    dst = dst_point["angles"]
    deltas = [abs(dst[i] - src[i]) for i in range(6)]
    arm_max_delta = max(deltas[:5]) if len(deltas) >= 5 else 0.0
    wrist6_delta = deltas[5] if len(deltas) >= 6 else 0.0
    print(f"  -> {label} 短距离关节差: {deltas}, "
          f"arm_max_delta={arm_max_delta:.1f}, wrist6_delta={wrist6_delta:.1f}")
    if arm_max_delta > SHORT_ARM_JOINT_MAX_DELTA:
        raise RuntimeError(
            f"{label}: 1-5轴短距离关节变化过大 "
            f"(arm_max_delta={arm_max_delta:.1f} > {SHORT_ARM_JOINT_MAX_DELTA})，"
            f"疑似示教到不同IK分支，请重新示教")
    if wrist6_delta > SHORT_WRIST6_MAX_DELTA:
        raise RuntimeError(
            f"{label}: 第6轴短距离旋转过大 "
            f"(wrist6_delta={wrist6_delta:.1f} > {SHORT_WRIST6_MAX_DELTA})，"
            f"疑似腕部翻转，请重新示教")
    return True


def validate_return_angle_pair(src_point, dst_point, label):
    """
    V2 §6：回零过渡 drop_hover -> home_ready 的关节差 sanity guard。
    与 validate_short_angle_pair 区别：回零过渡本身就是大幅度拉直再构型
    （J3 从 -70° 朝 0° 拉直，同侧合法过渡单轴变化可达 60~70°），不能用
    短距离的 30° 连续性门。此处只用宽松阈值拦截"荒谬跳变/异侧翻面"。

    修正 A：plan 给 55 会误拒优秀 home_ready——drop_hover J3≈-70 -> home_ready
    J3≈-10（arm_max_diff=10，极好）单轴 delta=60>55 被拒，越好的点越被拒。
    取 90：同侧拉直必过；异侧翻面（J3 从 -70 跨到 +30，delta=100）才拦。
    腕部取 120：腕部旋转物理上无害，只拦完全翻面。
    """
    src = src_point["angles"]
    dst = dst_point["angles"]
    deltas = [abs(dst[i] - src[i]) for i in range(6)]
    arm_max_delta = max(deltas[:5]) if len(deltas) >= 5 else 0.0
    wrist6_delta = deltas[5] if len(deltas) >= 6 else 0.0
    print(f"  -> {label} 回零过渡关节差: {deltas}, "
          f"arm_max_delta={arm_max_delta:.1f}, wrist6_delta={wrist6_delta:.1f}")
    if arm_max_delta > RETURN_ARM_JOINT_MAX_DELTA:
        raise RuntimeError(
            f"{label}: 回零过渡单轴变化过大 "
            f"(arm_max_delta={arm_max_delta:.1f} > {RETURN_ARM_JOINT_MAX_DELTA})，"
            f"疑似 home_ready 示教点选错或落到异侧分支，请重新示教")
    if wrist6_delta > RETURN_WRIST6_MAX_DELTA:
        raise RuntimeError(
            f"{label}: 回零过渡第6轴旋转过大 "
            f"(wrist6_delta={wrist6_delta:.1f} > {RETURN_WRIST6_MAX_DELTA})，"
            f"疑似腕部翻面，请重新示教")
    return True


def validate_return_ready(home_ready):
    """
    V2.1 §6/§12.4：home_ready 自动回零安全门预校验（示教期目标角度，非实跑角度）。
    修正 B：arm_max_diff<=45 是硬门（不是"建议"）。只有通过此门，
    prepare_phase 和 auto_phase_v2 末段的 safe_return_home 才会走自动回零
    分支（sync_send_angles(HOME,15)），不进人工扶正。
    修正 C：home_ready -> HOME_ANGLES 的过渡校验由本函数覆盖——
    arm_max_diff<=45 意味着 home_ready->HOME 单轴 delta<=45<RETURN_ARM_JOINT_MAX_DELTA，
    故无需再单独构造 HOME 点做 pair 校验。
    V2.1：<=40 推荐 / 40~45 警告（实跑可能被软通过推过界，见 run-10）/ >45 拦截。
    本函数只判定"目标示教角度"是否进入安全门；实跑软通过后的实际姿态校验
    由 checked_return_transition 内的 _verify_actual_pose_for_auto_return 完成。
    """
    all_diffs, arm_diffs, arm_max_diff, wrist6_diff = calc_home_diffs(home_ready["angles"])
    print(f"  -> home_ready 校验: arm_diffs={arm_diffs}, "
          f"arm_max_diff={arm_max_diff:.1f}, wrist6_diff={wrist6_diff:.1f}")
    if arm_max_diff > ARM_MAX_DIFF_SAFE:
        print(f"  -> 【拦截】home_ready arm_max_diff={arm_max_diff:.1f} "
              f"> {ARM_MAX_DIFF_SAFE}，未进入自动回零安全门，请重新示教 home_ready。")
        return False
    if arm_max_diff > HOME_READY_TARGET_ARM_MAX:
        print(f"  -> 【警告】home_ready arm_max_diff={arm_max_diff:.1f} "
              f"在 {HOME_READY_TARGET_ARM_MAX}~{ARM_MAX_DIFF_SAFE} 余量不足区间，"
              f"实跑软通过可能像 run-10 一样被推过 {ARM_MAX_DIFF_SAFE}。")
        print(f"  -> 仍按安全门放行，但 auto 第10步的实际姿态校验+低速重试"
              f"会尝试补救；建议重新示教更直立的 home_ready。")
    else:
        print(f"  -> home_ready 通过推荐余量门 "
              f"(arm_max_diff={arm_max_diff:.1f} <= {HOME_READY_TARGET_ARM_MAX})。")
    if wrist6_diff > WRIST6_WARN_DIFF:
        print(f"  -> 【提示】home_ready 第6轴偏差较大 ({wrist6_diff:.1f})，"
              f"回零时夹爪会自转，确认末端线缆/夹爪周边无干涉。")
    return True


def checked_short_linear(mc, target_coords, speed, timeout, label):
    """
    [已禁用 / 仅供历史对照] 短距离直线动作，调用 sync_send_coords()。
    §14.2/§14.3 已确认：短距离继续使用坐标驱动是 run-5 熔断的根因——
    mode=0 只表示关节空间插值，不表示绕过 IK，固件端仍必须把笛卡尔
    目标解算为关节角，远端低位/腕部多解/边界点仍可能快速返回 0。
    自动阶段不得调用本函数；短距离请改用 checked_short_angles()。
    本函数保留仅供历史对照与回归验证，禁止进入 auto_phase 调用链。
    """
    raise RuntimeError(
        f"{label}: checked_short_linear 已禁用（§14.3），自动阶段不得使用坐标驱动，"
        f"请改用 checked_short_angles()。")


def verify_coords_near(mc, expected, label, xyz_tol=COORD_VERIFY_TOL):
    """
    关节角回放后再读当前坐标，只做一致性校验，不用于继续规划。
    delta 取 XYZ 三轴最大绝对差；超容差抛异常。
    home_ready 的 expected Z 可能 >280mm，本函数只做 delta 校验、不做范围校验，
    因此对 home_ready 同样适用。
    """
    actual = get_filtered_coords(mc)
    if actual is None:
        raise RuntimeError(f"{label}: 无法读取当前坐标用于校验")
    delta = max(abs(actual[i] - expected[i]) for i in range(3))
    print(f"  -> {label} 坐标校验 delta_xyz={delta:.1f} mm")
    if delta > xyz_tol:
        raise RuntimeError(
            f"{label}: 当前坐标偏差过大，expected={expected}, actual={actual}"
        )
    return True


# ============================================================
# 夹爪（沿用 teach_and_pick.py，开环返回 True；主流程必须处理 False）
# ============================================================
def checked_gripper_action(mc, state, speed, timeout=GRIPPER_TIMEOUT):
    """
    state: 0 张开 / 1 闭合。pymycobot 4.0.5 无 get_gripper_value 时
    开环等待并返回 True；若未来库支持反馈，不可忽略失败返回。
    """
    action_str = "闭合" if state == 1 else "张开"
    print(f"  -> 下发夹爪动作: {action_str}...")
    # set_gripper_state 移入 try，避免瞬时串口异常传播到 main except Exception
    # 导致整轮 abort + release_all_servos 掉电（夹爪小故障不应中断整轮）。
    try:
        mc.set_gripper_state(state, speed)
    except Exception as e:
        print(f"【错误】夹爪 {action_str} 下发失败: {type(e).__name__}: {e}")
        return False

    if not hasattr(mc, "get_gripper_value"):
        print("  -> 【信息】当前库无 get_gripper_value 反馈接口，使用开环定时等待。")
        time.sleep(timeout)
        return True

    start_time = time.time()
    last_val = -999
    stable_count = 0

    while time.time() - start_time < timeout:
        try:
            val = mc.get_gripper_value()
        except Exception:
            val = -1
        time.sleep(0.2)

        if isinstance(val, int) and val >= 0:
            if state == 0 and val > 70:
                print("  -> 传感器确认：夹爪已完全张开。")
                return True
            if state == 1 and val < 30:
                print("  -> 传感器确认：夹爪已完全闭合。")
                return True
            if abs(val - last_val) <= 2:
                stable_count += 1
                if stable_count >= 3:
                    print(f"  -> 传感器确认：夹爪受阻停止变化(当前值:{val})，视为动作完成。")
                    return True
            else:
                stable_count = 0
            last_val = val

    print(f"【警告】夹爪 {action_str} 动作在 {timeout}s 内未得到传感器明确到位反馈。")
    time.sleep(0.5)
    return False


def gripper_action_with_retry(mc, state, label, retries=1):
    """
    夹爪动作带重试。第一次失败后重试一次；仍失败则抛异常熔断，
    不再以未确认夹爪状态继续（避免抓空/未释放却报告成功）。
    开环模式下 checked_gripper_action 返回 True，重试不会触发。
    """
    for attempt in range(retries + 1):
        if checked_gripper_action(mc, state, GRIPPER_SPEED):
            return True
        if attempt < retries:
            print(f"  -> {label} 夹爪未确认，重试一次...")
            time.sleep(0.5)
    raise RuntimeError(f"{label}: 夹爪动作未确认，熔断停止（避免抓空/未释放）")


# ============================================================
# 安全回零辅助（§13.2 / §13.3）
# ============================================================
def calc_home_diffs(angles):
    """
    §13.2：把"大臂安全门"和"第 6 轴末端旋转"拆开。
    返回 (all_diffs, arm_diffs, arm_max_diff, wrist6_diff)。
    大臂安全门只用 arm_max_diff（1-5 轴）；第 6 轴仅诊断/告警，不阻断回零。
    """
    all_diffs = [abs(a - b) for a, b in zip(angles, HOME_ANGLES)]
    arm_diffs = all_diffs[:ARM_JOINT_COUNT]
    arm_max_diff = max(arm_diffs) if arm_diffs else 0.0
    wrist6_diff = all_diffs[5] if len(all_diffs) >= 6 else 0.0
    return all_diffs, arm_diffs, arm_max_diff, wrist6_diff


def prompt_manual_prehome(mc, max_rounds=2):
    """
    §13.3：大偏差人工扶正状态机。
    舵机已上电锁紧时用户无法手掰扶正，因此流程必须是：
      扶稳提示 -> release_all_servos -> 用户扶正 -> power_on -> 等待 -> 重读
    返回扶正后通过大臂安全门的 angles，或 None（用户放弃/仍大偏差）。
    不调用 sync_send_coords / protect_coords（§13.3 注意事项）。
    """
    for idx in range(max_rounds):
        input("当前大臂偏差较大。请先用手扶稳机械臂，按 Enter 后释放舵机...")
        mc.release_all_servos()
        time.sleep(0.5)

        ans = input("请手动扶到【夹爪尖端朝前】的预回零姿态；完成后按 Enter 上电读取，输入 q 放弃: ")
        if ans.strip().lower() == "q":
            return None

        mc.power_on()
        time.sleep(POWER_ON_SETTLE)

        angles = get_filtered_angles(mc)
        coords = get_filtered_coords(mc)
        print(f"扶正后稳定关节角: {angles}")
        print(f"扶正后稳定空间坐标: {coords}")

        if angles is None:
            print("【错误】扶正后仍无法读取稳定关节角。")
            continue

        all_diffs, arm_diffs, arm_max_diff, wrist6_diff = calc_home_diffs(angles)
        print(f"扶正后大臂 1-5 轴偏差: {arm_diffs}, arm_max_diff={arm_max_diff:.1f}")
        print(f"扶正后第 6 轴偏差: {wrist6_diff:.1f}")
        if arm_max_diff <= ARM_MAX_DIFF_SAFE:
            return angles

        print(f"arm_max_diff={arm_max_diff:.1f} 仍 > {ARM_MAX_DIFF_SAFE}，请继续扶正。")

    return None


# ============================================================
# 安全回零（§13.2/§13.3/§13.4：大臂安全门 + 人工扶正状态机）
# ============================================================
def _send_home_async(mc, label):
    """
    V2.5 §14.8 方向1+方向3：异步软到位回零。
    用非阻塞 send_angles 启动回 HOME，Python 端软到位循环检测 max_diff<=阈值即退出，
    避免固件 is_in_position 死区的无意义尾端等待（run-15 step11 的 1.2s 中 ~0.6-1s 是
    固件等待）。预期 step11 ~0.5s。

    收尾策略（用户已批"未收敛走 sync 收尾"）：
      - 软超时（HOME_RETURN_ASYNC_TIMEOUT=2.5s）内 max_diff<=HOME_RETURN_ASYNC_SOFT_TOL(1.0°)
        -> converged，返回 ("auto", "async", final_max_diff)。
      - 软超时未收敛 -> 不抛异常（臂在回零路径，掉电危险），改用阻塞 sync_send_angles
        收尾一次并检查 res，保留 res!=1 -> "failed" 安全失败路径。
        收尾成功返回 ("auto", "async_then_sync", final_max_diff)；res!=1 返回 ("failed",...)。

    安全守住：
      - 不跳过 arm_max_diff<=45 安全门（由调用方 safe_return_home 判定后才进本函数）。
      - 未收敛兜底走 sync + res 检查，不放弃对"未真到位"的拦截。
      - HOME_RETURN_ASYNC_ENABLE=False 时调用方走 V2.4 阻塞路径，本函数不被调用。
    返回 (status, mode, final_max_diff)：status∈{"auto","failed"}，mode 描述走的路径。
    """
    print(f"  -> [V2.5 方向1] {label}: 异步回零 send_angles(HOME, speed={HOME_RETURN_ASYNC_SPEED})，"
          f"软到位循环 (tol={HOME_RETURN_ASYNC_SOFT_TOL}°, timeout={HOME_RETURN_ASYNC_TIMEOUT}s, "
          f"confirm={HOME_RETURN_ASYNC_CONFIRM_COUNT})...")
    mc.send_angles(HOME_ANGLES, HOME_RETURN_ASYNC_SPEED)
    start = time.time()
    converged = False
    final_max_diff = None
    confirm_count = 0               # V2.10：连续确认计数器
    while time.time() - start < HOME_RETURN_ASYNC_TIMEOUT:
        actual = get_angles_once(mc)  # 单次读数：臂在运动，不能用 get_filtered_angles 的稳定性门
        if actual:
            diffs = [abs(actual[i] - HOME_ANGLES[i]) for i in range(6)]
            final_max_diff = max(diffs)
            if final_max_diff <= HOME_RETURN_ASYNC_SOFT_TOL:
                # V2.10 二次读数确认：连续 N 次读数都 <=tol 才判收敛，
                # 拒绝减速/回零过程中"单帧瞬态下探"的假收敛。N=1 退化 V2.5 行为。
                confirm_count += 1
                if confirm_count >= HOME_RETURN_ASYNC_CONFIRM_COUNT:
                    converged = True
                    break
            else:
                confirm_count = 0
        time.sleep(HOME_RETURN_ASYNC_POLL)
    elapsed = time.time() - start
    if converged:
        print(f"  -> [V2.5 方向1] {label}: 软到位收敛 max_diff={final_max_diff:.2f}° "
              f"<= {HOME_RETURN_ASYNC_SOFT_TOL}°，耗时 {elapsed:.2f}s（提前退出固件等待）。")
        return "auto", "async", final_max_diff
    # 软超时未收敛：阻塞 sync 收尾兜底（不抛异常，防臂在回零路径掉电）
    fd = f"{final_max_diff:.2f}" if final_max_diff is not None else "N/A"
    print(f"  -> [V2.5 方向1] {label}: 软超时未收敛（max_diff={fd}°），"
          f"阻塞 sync_send_angles 收尾兜底 (speed={HOME_RETURN_SPEED} [V2.4已验证], "
          f"timeout={HOME_RETURN_TIMEOUT}s)...")
    # Codex F1：sync 兜底是 "failed" 前最后一道安全网，必须用 V2.4 已验证的 speed=25，
    # 不能用未验证的 HOME_RETURN_ASYNC_SPEED=30——否则 speed 30 有问题时 async 和 sync
    # 兜底会级联失败。主路径 speed=30 在非阻塞 send_angles 上，失败走 timeout -> 这里 25 收尾。
    res = mc.sync_send_angles(HOME_ANGLES, HOME_RETURN_SPEED,
                              timeout=HOME_RETURN_TIMEOUT)
    print(f"  -> [V2.5 方向1] {label}: 收尾 sync_send_angles 返回 {res}")
    if res != 1:
        print("【警告】回零动作超时或被物理阻挡！（异步软超时 + sync 收尾均未到位）")
        return "failed", "async_then_sync", final_max_diff
    actual = get_angles_once(mc)  # sync 返回 1 后读末态残差（诊断用，单次即可）
    if actual:
        final_max_diff = max(abs(actual[i] - HOME_ANGLES[i]) for i in range(6))
    print(f"  -> [V2.5 方向1] {label}: sync 收尾后 max_diff="
          f"{(f'{final_max_diff:.2f}' if final_max_diff is not None else 'N/A')}°。")
    return "auto", "async_then_sync", final_max_diff


def _smooth_handoff_return(mc, home_ready):
    """
    V2.5 §14.8 方向2：航路点平滑过渡回零。
    把 step10(drop_hover->home_ready) + step11(home_ready->HOME) 合并为单一异步流：
      1. 非阻塞 send_angles(home_ready) 启动过渡。
      2. 轮询实际角度，当与 home_ready 的 max_diff <= SMOOTH_HANDOFF_NEAR_TOL(5°) 时
         ——臂还在运动、未停稳——立即非阻塞 send_angles(HOME) 下发最终目标。
      3. 继续轮询直到与 HOME 的 max_diff <= HOME_RETURN_ASYNC_SOFT_TOL(1°) 软到位。
      4. 软超时兜底：步骤2未在 SMOOTH_HANDOFF_NEAR_TOL 对应超时内接近 home_ready，
         或步骤3未收敛，回退阻塞 sync_send_angles 收尾 + res 检查。

    收益：消除 home_ready 处"减速-静止-重启"停顿（~1s）+ 固件尾端等待（~0.6-1s），
    预期 step10+11 从 run-15 的 ~3.8s 压到 ~1.5s 量级。

    安全守住（比 V2.4 step10+11 更严的点上不能放松）：
      - 调用前 auto_phase_v2 的 step0b 已跑 validate_return_angle_pair + validate_return_ready
        （只读安全门，确认 home_ready arm_max_diff<=45 且 drop_hover->home_ready 单轴 delta<=90）。
      - 提前下发 HOME 前必须实测确认臂已接近 home_ready（<=5°），不是假设。
        若读不到角度或一直未接近，不提前下发，回退阻塞路径（V2.4 行为）。
      - 最终软到位/sync 收尾判定与 _send_home_async 一致：未收敛走 sync + res 检查，
        res!=1 -> "failed" 安全失败，保留扶稳+release 路径。
      - 不抛异常：臂在空中过渡路径时抛异常会触发 main 异常掉电（臂下沉危险），
        任何异常都转为回退或 "failed"，由 auto_phase_v2 的 failed 分支提示扶稳后 release。
      - SMOOTH_HANDOFF_ENABLE=False 或回退时，调用方走 V2.4 step10+step11 分段路径。

    返回三态字符串（与 safe_return_home 一致）："auto" / "manual"(本函数不产生) / "failed"。
    本函数只处理 auto 路径（home_ready 已过 validate_return_ready 门），不会走人工扶正。
    """
    hr_angles = home_ready["angles"]

    # ---- 阶段A：非阻塞发 home_ready，轮询到接近（<=5°）即提前发 HOME ----
    print(f"  -> [V2.5 方向2] 平滑过渡：非阻塞 send_angles(home_ready, speed={ANG_REPLAY_SPEED})，"
          f"接近阈值 {SMOOTH_HANDOFF_NEAR_TOL}° 即提前下发 HOME...")
    mc.send_angles(hr_angles, ANG_REPLAY_SPEED)
    near_start = time.time()
    near_timeout = ANG_REPLAY_TIMEOUT  # 复用回零过渡超时（20s），实际接近应远快于此
    handoff_sent = False
    while time.time() - near_start < near_timeout:
        actual = get_angles_once(mc)  # 单次读数：臂在运动，不能用 get_filtered_angles 的稳定性门
        if actual:
            hr_diff = max(abs(actual[i] - hr_angles[i]) for i in range(6))
            if hr_diff <= SMOOTH_HANDOFF_NEAR_TOL:
                # 实测接近 home_ready -> 提前下发 HOME（臂未停稳）
                print(f"  -> [V2.5 方向2] 已接近 home_ready (max_diff={hr_diff:.2f}° "
                      f"<= {SMOOTH_HANDOFF_NEAR_TOL}°)，臂未停稳即下发 HOME 目标。")
                mc.send_angles(HOME_ANGLES, HOME_RETURN_ASYNC_SPEED)
                handoff_sent = True
                break
        time.sleep(HOME_RETURN_ASYNC_POLL)

    if not handoff_sent:
        # 一直未接近 home_ready：固件打断行为异常或路径卡阻，回退 V2.4 阻塞路径。
        # 不抛异常——臂可能在过渡路径上，回退到阻塞 sync 让固件接管收尾。
        print(f"  -> [V2.5 方向2] 未在 {near_timeout}s 内接近 home_ready，"
              f"回退 V2.4 阻塞分段路径（sync_send_angles home_ready + safe_return_home）。")
        res = mc.sync_send_angles(hr_angles, ANG_REPLAY_SPEED, timeout=ANG_REPLAY_TIMEOUT)
        print(f"  -> [V2.5 方向2] 回退 home_ready sync_send_angles 返回 {res}")
        if res != 1:
            print("【警告】回退路径 home_ready 未到位，交 safe_return_home 人工扶正状态机。")
            return "failed"
        # 回退后走标准 safe_return_home（它会重读角度判 arm_max_diff 门）
        return safe_return_home(mc)

    # ---- 阶段B：已提前下发 HOME，轮询软到位（与 _send_home_async 阶段一致）----
    home_start = time.time()
    converged = False
    final_max_diff = None
    confirm_count = 0               # V2.10：连续确认计数器（同 _send_home_async 阶段）
    while time.time() - home_start < HOME_RETURN_ASYNC_TIMEOUT:
        actual = get_angles_once(mc)  # 单次读数：臂在运动，不能用 get_filtered_angles 的稳定性门
        if actual:
            final_max_diff = max(abs(actual[i] - HOME_ANGLES[i]) for i in range(6))
            if final_max_diff <= HOME_RETURN_ASYNC_SOFT_TOL:
                # V2.10 二次读数确认：拒绝"臂未停稳提前下发 HOME 后读运动中瞬态 ~39° 跳变"
                # 判假收敛/假未收敛。N=1 退化原 V2.5 单次确认行为。
                confirm_count += 1
                if confirm_count >= HOME_RETURN_ASYNC_CONFIRM_COUNT:
                    converged = True
                    break
            else:
                confirm_count = 0
        time.sleep(HOME_RETURN_ASYNC_POLL)
    elapsed = time.time() - home_start
    if converged:
        print(f"  -> [V2.5 方向2] HOME 软到位收敛 max_diff={final_max_diff:.2f}° "
              f"<= {HOME_RETURN_ASYNC_SOFT_TOL}°，耗时 {elapsed:.2f}s。")
        return "auto"
    # 软超时未收敛：阻塞 sync 收尾兜底
    fd = f"{final_max_diff:.2f}" if final_max_diff is not None else "N/A"
    print(f"  -> [V2.5 方向2] HOME 软超时未收敛（max_diff={fd}°），"
          f"阻塞 sync_send_angles 收尾兜底 (speed={HOME_RETURN_SPEED} [V2.4已验证], "
          f"timeout={HOME_RETURN_TIMEOUT}s)...")
    # Codex F1：sync 兜底用 V2.4 已验证 speed=25，不用未验证的 30（防级联失败）。
    res = mc.sync_send_angles(HOME_ANGLES, HOME_RETURN_SPEED,
                              timeout=HOME_RETURN_TIMEOUT)
    print(f"  -> [V2.5 方向2] 收尾 sync_send_angles 返回 {res}")
    if res != 1:
        print("【警告】回零动作超时或被物理阻挡！（平滑过渡软超时 + sync 收尾均未到位）")
        return "failed"
    actual = get_angles_once(mc)  # sync 返回 1 后读末态残差（诊断用，单次即可）
    if actual:
        final_max_diff = max(abs(actual[i] - HOME_ANGLES[i]) for i in range(6))
    print(f"  -> [V2.5 方向2] sync 收尾后 max_diff="
          f"{(f'{final_max_diff:.2f}' if final_max_diff is not None else 'N/A')}°。")
    return "auto"


def safe_return_home(mc, cached_angles=None):
    """
    安全回零策略（§13 覆盖 §12.4 的大偏差交互）：
      1. 读取稳定 angles（连续两次稳定，§12.3）。
      2. 用 calc_home_diffs 拆分大臂 1-5 轴与第 6 轴（§13.2）。
      3. 打印 angles / all_diffs / arm_diffs / arm_max_diff / wrist6_diff / coords。
      4. arm_max_diff <= ARM_MAX_DIFF_SAFE：
         - 若 wrist6_diff > WRIST6_WARN_DIFF，打印末端旋转提示（§13.4）。
         - 低速 sync_send_angles(HOME_ANGLES, 15)。
      5. arm_max_diff > ARM_MAX_DIFF_SAFE：
         - 不自动笛卡尔保护拉升（§12.4/§13.3）。
         - 调 prompt_manual_prehome：扶稳->release->扶正->power_on->重读。
         - 通过则低速回零；用户放弃或仍大偏差则返回 "failed"。

    V2：auto_phase_v2 末段先过渡到 home_ready（arm_max_diff<=45）再调本函数，
    因此正常路径走分支 4 自动回零，不再进分支 5 人工扶正。

    V2.1 §12.4-5 返回值改为三态字符串，供上层区分"0 轮自动回零"与
    "触发人工扶正后完成"，避免日志把人工扶正写成自动回零（run-10 日志的
    修正项）：
      - "auto"   : 走分支 4 自动回零成功（0 轮人工扶正）。
      - "manual" : 进分支 5 人工扶正状态机，扶正后回零成功（1~2 轮）。
      - "failed" : 无法读角度 / 含非有限值 / 回零动作超时 / 扶正未通过。
    仍为 truthy("auto"/"manual") / falsy("failed")，旧式 `if not safe_return_home`
    调用兼容。

    V2.4 §14.7 L1+L2：cached_angles 可选参数。auto_phase_v2 末段调用时可传入
    _verify_actual_pose_for_auto_return 刚读完的实际角度（臂在 home_ready 静止），
    跳过本函数的 get_filtered_angles 重读，并跳过 auto 分支纯诊断的
    diag_coords 读取（diag_coords 只打印不决策，auto 分支去掉它对安全门判定
    零影响）。manual 分支仍保留 diag_coords 诊断（扶正场景诊断有价值）。
    cached_angles 默认 None：prepare_phase 路径与旧调用方行为完全不变。
    安全约束：cached_angles 仍走 calc_home_diffs + 有限值校验 + arm_max_diff
    安全门判定，不跳过任何安全逻辑，只省冗余读数。

    V2.5 §14.8 方向1+方向3：auto 分支与 manual 扶正后回零，在 HOME_RETURN_ASYNC_ENABLE
    时改走 _send_home_async（非阻塞 send_angles + Python 端软到位循环 + 软超时走 sync
    收尾兜底），预期 step11 1.2s->~0.5s。速度用 HOME_RETURN_ASYNC_SPEED=30（方向3）。
    HOME_RETURN_ASYNC_ENABLE=False 时回退 V2.4 阻塞 sync_send_angles(HOME,25,timeout=12)，
    一行回退，安全门判定不变。manual 分支扶正后回零同样适用异步路径。
    """
    if cached_angles is not None:
        angles = cached_angles
        print(f"  -> [V2.4 L2] 复用上游已读角度（臂在 home_ready 静止），"
              f"跳过 safe_return_home 重读。angles={angles}")
    else:
        angles = get_filtered_angles(mc)
    if angles is None:
        print("【错误】无法读取稳定关节角，拒绝回零！")
        print("-> 请人工扶正到接近直立姿态后重试，或重新示教。")
        return "failed"

    all_diffs, arm_diffs, arm_max_diff, wrist6_diff = calc_home_diffs(angles)
    if not all(math.isfinite(d) for d in all_diffs):
        print(f"【错误】关节角偏差含非有限值，拒绝回零！angles={angles}")
        return "failed"

    # §13.5：日志术语区分大臂与第 6 轴
    print(f"当前稳定关节角: {angles}")
    print(f"相对 HOME_ANGLES 全轴偏差: {all_diffs}")
    print(f"大臂 1-5 轴偏差: {arm_diffs}, arm_max_diff={arm_max_diff:.1f}")
    print(f"第 6 轴末端旋转偏差: {wrist6_diff:.1f}")

    if arm_max_diff <= ARM_MAX_DIFF_SAFE:
        # V2.4 §14.7 L1：auto 分支跳过纯诊断的 diag_coords 读取（只打印不决策，
        # 省 ~0.4-0.8s 读数开销）。manual 分支保留 diag_coords（扶正诊断有价值）。
        # §13.4：第 6 轴较大时提示末端旋转风险，但不阻断回零
        if wrist6_diff > WRIST6_WARN_DIFF:
            print(f"【提示】第6轴末端旋转偏差较大 ({wrist6_diff:.1f}度)，"
                  f"回零时夹爪会自转，请确认末端线缆/夹爪周边无干涉。")
        print(f"arm_max_diff={arm_max_diff:.1f} <= {ARM_MAX_DIFF_SAFE}，回直立零位...")
        # V2.5 §14.8 方向1+方向3：异步软到位回零（非阻塞 send_angles + 软到位循环
        # + 软超时走 sync 收尾兜底）。HOME_RETURN_ASYNC_ENABLE=False 回退 V2.4 阻塞路径。
        # 安全门（arm_max_diff<=45）已在本 if 判定通过，异步化只动移动下发方式。
        if HOME_RETURN_ASYNC_ENABLE:
            status, mode, _ = _send_home_async(mc, "step11回零")
            # D0：显式记录回零结果，run-15 靠"无警告"反推 res==1 的诊断缺口补上
            print(f"  -> [V2.5 D0] step11 回零结果: status={status}, mode={mode}")
            return status
        # V2.4 阻塞回退路径（HOME_RETURN_ASYNC_ENABLE=False）
        res = mc.sync_send_angles(HOME_ANGLES, HOME_RETURN_SPEED,
                                  timeout=HOME_RETURN_TIMEOUT)
        print(f"  -> [V2.5 D0] step11 阻塞回零 sync_send_angles 返回 {res}")
        if res != 1:
            print("【警告】回零动作超时或被物理阻挡！")
            return "failed"
        return "auto"

    # 大偏差分支：人工扶正状态机（§13.3）
    print(f"\n【警告】当前大臂 1-5 轴与零位偏差较大 (arm_max_diff={arm_max_diff:.1f}度 > {ARM_MAX_DIFF_SAFE}度)。")
    print("-> 不自动执行笛卡尔保护拉升。进入人工扶正流程。")
    # V2.4 L1：manual 分支保留 diag_coords 诊断读数（扶正场景诊断有价值）。
    diag_coords = get_filtered_coords(mc)
    print(f"当前稳定空间坐标: {diag_coords}")
    angles = prompt_manual_prehome(mc)
    if angles is None:
        print("【错误】人工扶正未通过大臂安全门，拒绝自动回零。请人工扶正到直立姿态后重跑。")
        return "failed"

    all_diffs, arm_diffs, arm_max_diff, wrist6_diff = calc_home_diffs(angles)
    if wrist6_diff > WRIST6_WARN_DIFF:
        print(f"【提示】第6轴末端旋转偏差较大 ({wrist6_diff:.1f}度)，"
              f"回零时夹爪会自转，请确认末端线缆/夹爪周边无干涉。")
    print(f"扶正通过，arm_max_diff={arm_max_diff:.1f}，回直立零位...")
    # V2.5 §14.8：manual 扶正后回零同样用异步软到位路径（扶正后 arm_max_diff 已 <=45，
    # 路径同样短且安全）。HOME_RETURN_ASYNC_ENABLE=False 回退 V2.4 阻塞路径。
    if HOME_RETURN_ASYNC_ENABLE:
        status, mode, _ = _send_home_async(mc, "扶正后回零")
        print(f"  -> [V2.5 D0] 扶正后回零结果: status={status}, mode={mode}")
        # manual 分支：异步失败仍是 "failed"；异步成功则改报 "manual"（走人工扶正后完成）
        if status == "failed":
            return "failed"
        return "manual"
    res = mc.sync_send_angles(HOME_ANGLES, HOME_RETURN_SPEED,
                              timeout=HOME_RETURN_TIMEOUT)
    print(f"  -> [V2.5 D0] 扶正后阻塞回零 sync_send_angles 返回 {res}")
    if res != 1:
        print("【警告】回零动作超时或被物理阻挡！")
        return "failed"
    return "manual"


# ============================================================
# 主流程（V2：五点示教）
# ============================================================
def teach_phase_v2(mc):
    """第一阶段：释放舵机后五点示教（pick_hover/pick/drop_hover/drop/home_ready）。"""
    print("====================================")
    print("【第一阶段：手动示教记录 (五点: pick_hover/pick/drop_hover/drop/home_ready)】")
    print("====================================")

    print("⚠️ 警告：机械臂即将放松掉电，请立刻【用手扶稳机械臂】！")
    input("-> 准备好后，按 Enter 键释放舵机...")
    mc.release_all_servos()
    time.sleep(0.5)
    print("\n✅ 机械臂已变软。")

    pick_hover = record_teach_point(mc, "抓取悬停点 pick_hover", is_hover=True)
    pick = record_teach_point(mc, "抓取下探点 pick", is_hover=False)
    drop_hover = record_teach_point(mc, "放置悬停点 drop_hover", is_hover=True)
    drop = record_teach_point(mc, "放置下探点 drop", is_hover=False)
    print("\n-> 接下来示教 home_ready：drop_hover 与直立零位之间的空中安全中间姿态。")
    home_ready = record_return_ready_point(mc)

    print("\n=== 示教点汇总 ===")
    for p in (pick_hover, pick, drop_hover, drop, home_ready):
        print(f"  {p['name']}: angles={p['angles']}  coords={p['coords']}")
    return pick_hover, pick, drop_hover, drop, home_ready


def prepare_phase(mc):
    """第二阶段：通电 + 安全回零。"""
    print("\n====================================")
    print("【第二阶段：恢复供电并检查安全回零】")
    print("====================================")
    input("-> 请将手轻轻拿开，按 Enter 键通电并执行安全回零...")
    mc.power_on()
    # §12.5：等待舵机抱紧和读数稳定，避免通电后暂态/旧值（第三次试运行 max_diff=78.9
    # 而 coords 显示接近直立高位，疑为上电后角度读数未稳定）。
    time.sleep(POWER_ON_SETTLE)
    # V2：此时臂应停在 home_ready（arm_max_diff<=45），safe_return_home 走自动回零分支。
    # V2.1：safe_return_home 返回 "auto"/"manual"/"failed"；prepare 阶段臂在示教
    # home_ready（非软通过场景），正常应 "auto"。若意外进 "manual"/"failed" 仍按
    # 原逻辑处理（manual 算回零成功可继续；failed 中止）。
    result = safe_return_home(mc)
    if result == "failed":
        # 回零失败时臂可能偏高/带载，直接 release_all_servos（阻尼）会让臂下沉摆动撞物。
        # 先提示用户扶稳，确认后再释放。
        print("\n【警告】回零失败，机械臂当前位置可能偏高或偏离零位。")
        print("-> 请【用手扶稳机械臂】防止下沉，确认扶稳后按 Enter 释放舵机...")
        input()
        mc.release_all_servos()
        print("已释放舵机。请人工扶正到接近直立姿态后重试。")
        return False
    if result == "manual":
        print("-> 第二阶段回零触发了人工扶正（prepare 阶段非预期，但仍完成回零）。")
    return True


def auto_phase_v2(mc, pick_hover, pick, drop_hover, drop, home_ready):
    """
    第三阶段：纯关节角回放 + 空间坐标只读校验 + home_ready 回零过渡（V2）。
    长距离用 checked_sync_angles；短距离 hover<->pick/drop 用 checked_short_angles；
    drop_hover->home_ready 用 checked_return_transition（软到位+诊断）；
    末段 safe_return_home 在 home_ready（arm_max_diff<=45）走自动回零。
    coords 只保留给 verify_coords_near 做到位一致性校验，不参与运动规划。
    任一动作失败即抛异常熔断，不自动 fallback。

    V2.7 返回值：True=本轮完整跑通（含末段回零，臂在 HOME/夹爪张开）；
    False=中途中止（home_ready 安全门未过 / 回零失败已 release_all_servos）。
    main 的连续抓取循环据此决定是否继续下一轮（False 即 break，不再对软臂下发运动指令）。
    """
    print("\n====================================")
    print("【第三阶段：关节角示教回放 + 回零过渡 + 空间一致性校验 (V2)】")
    print("====================================")

    # §14.4：短距离点对关节连续性预校验（只读，不发任何运动指令）。
    print("\n0. 短距离点对关节连续性预校验...")
    validate_short_angle_pair(pick_hover, pick, "pick_hover->pick")
    validate_short_angle_pair(pick, pick_hover, "pick->pick_hover")
    validate_short_angle_pair(drop_hover, drop, "drop_hover->drop")
    validate_short_angle_pair(drop, drop_hover, "drop->drop_hover")
    print("  -> 四对短距离点对关节连续性校验通过。")

    # V2 §6：回零过渡校验（drop_hover -> home_ready + home_ready 自动回零安全门）
    print("\n0b. 回零过渡校验 (drop_hover -> home_ready -> HOME)...")
    validate_return_angle_pair(drop_hover, home_ready, "drop_hover->home_ready")
    if not validate_return_ready(home_ready):
        print("-> home_ready 未通过自动回零安全门，禁止进入实机自动动作。")
        print("-> 请重新运行并示教更接近直立的 home_ready。")
        return False  # V2.7：通知 main 中止后续轮次（此分支未发运动指令，臂仍在 HOME）
    print("  -> 回零过渡校验通过。")

    # V2.8 点2：去掉每轮 Enter 阻塞，实现输入 N 后全自动运行。
    # 赛场流程已确认：CPU 信号→单轮抓放→人补块到 pick 点，pick 点每轮有物块，不会抓空气。
    # 轨迹无障碍/物块就位的一次性确认移到 main 循环开始前（每轮不再阻塞）。
    # 保留急停提示 print（不阻塞），Ctrl+C 路径不变。
    print("\n⚠️ drop_hover -> home_ready 空中过渡路径需无障碍；如遇危险随时 Ctrl+C 急停。")

    print("\n1. 张开夹爪准备...")
    gripper_action_with_retry(mc, 0, "step1 张开")

    print("\n2. 关节角回放到 pick_hover...")
    checked_sync_angles(mc, pick_hover["angles"], ANG_REPLAY_SPEED, ANG_REPLAY_TIMEOUT, "pick_hover")
    # V2.8 B类：长距离严格通过(res==1)时坐标必然准，SKIP_COORD_VERIFY_ON_STRICT_PASS 跳过校验省 ~0.6s。
    if not SKIP_COORD_VERIFY_ON_STRICT_PASS:
        verify_coords_near(mc, pick_hover["coords"], "pick_hover")

    print("\n3. 短距离关节下探到 pick...")
    checked_short_angles(mc, pick["angles"], SHORT_DOWN_SPEED, SHORT_DOWN_TIMEOUT, "pick")
    if not SKIP_COORD_VERIFY_ON_STRICT_PASS:
        verify_coords_near(mc, pick["coords"], "pick")

    print("\n4. 闭合夹爪抓取目标...")
    gripper_action_with_retry(mc, 1, "step4 闭合")

    print("\n5. 短距离关节抬起回 pick_hover...")
    _t = time.time()
    # V2.9：step 5 改异步软到位（与 step 9 同构——逆重力带载短距离回放 hover 点，
    # run-19 第2/3轮卡 3.4~3.5s 固件 is_in_position 死区，根因同 step 9）。
    # ASYNC_SHORT_STEP5_ENABLE=False 时回退原阻塞 checked_short_angles（V2.8 行为）。
    # 复用 checked_short_angles_async：软超时走 sync 收尾 + res 熔断，保留 V2.4 安全失败语义。
    # 取舍：异步软通过跳过同步版的 _soft_refine 微调（step 9 已证明该死区微调不收敛、纯浪费 3s）；
    # step 5 历史上多为 res==1 严格通过、几乎不触发微调，跳过影响可忽略。残差由 <1° 可能变为 ≤3°，
    # step 5 是抓起后悬停，≤3° 不影响夹爪保持——run-19 实测 step 9 在 2.1° 残差下放置精度稳定可佐证。
    if ASYNC_SHORT_STEP5_ENABLE:
        checked_short_angles_async(
            mc, pick_hover["angles"], SHORT_UP_SPEED, SHORT_UP_TIMEOUT, "pick_hover",
            expected_coords=pick_hover["coords"],
        )
    else:
        checked_short_angles(
            mc, pick_hover["angles"], SHORT_UP_SPEED, SHORT_UP_TIMEOUT, "pick_hover",
            expected_coords=pick_hover["coords"],
            allow_soft_success=True,
        )
    verify_coords_near(mc, pick_hover["coords"], "pick_hover")
    print(f"  -> [V2.2 耗时] step 5: {time.time() - _t:.1f}s")

    print("\n6. 关节角回放到 drop_hover（空中长距离过渡）...")
    checked_sync_angles(mc, drop_hover["angles"], ANG_REPLAY_SPEED, ANG_REPLAY_TIMEOUT, "drop_hover")
    if not SKIP_COORD_VERIFY_ON_STRICT_PASS:
        verify_coords_near(mc, drop_hover["coords"], "drop_hover")

    print("\n7. 短距离关节下降至 drop...")
    checked_short_angles(mc, drop["angles"], SHORT_DOWN_SPEED, SHORT_DOWN_TIMEOUT, "drop")
    if not SKIP_COORD_VERIFY_ON_STRICT_PASS:
        verify_coords_near(mc, drop["coords"], "drop")

    print("\n8. 张开夹爪放置正方体...")
    gripper_action_with_retry(mc, 0, "step8 张开")

    print("\n9. 短距离关节抬起回 drop_hover...")
    _t = time.time()
    # V2.8 点1：step 9 改异步软到位（drop→drop_hover 带载上行是已知瓶颈，run-18 稳态 20s）。
    # ASYNC_SHORT_ENABLE=False 时回退原阻塞 checked_short_angles（V2.7 行为）。
    if ASYNC_SHORT_ENABLE:
        checked_short_angles_async(
            mc, drop_hover["angles"], SHORT_UP_SPEED, SHORT_UP_TIMEOUT, "drop_hover",
            expected_coords=drop_hover["coords"],
        )
    else:
        checked_short_angles(
            mc, drop_hover["angles"], SHORT_UP_SPEED, SHORT_UP_TIMEOUT, "drop_hover",
            expected_coords=drop_hover["coords"],
            allow_soft_success=True,
        )
    verify_coords_near(mc, drop_hover["coords"], "drop_hover")
    print(f"  -> [V2.2 耗时] step 9: {time.time() - _t:.1f}s")

    print("\n10+11. 末段回零：drop_hover -> home_ready -> HOME...")
    print("  -> 即将把臂从 drop_hover 拉向接近直立的 home_ready 再回 HOME，请确认空中路径无障碍。")
    _t = time.time()
    # V2.5 §14.8 方向2：SMOOTH_HANDOFF_ENABLE 时把 step10+step11 合并为平滑过渡异步流
    # （非阻塞 send home_ready -> 接近即提前 send HOME -> 软到位），消除 home_ready 停顿。
    # 回退条件：未接近 home_ready / 读不到角度 -> _smooth_handoff_return 内部回退 V2.4 阻塞分段。
    # SMOOTH_HANDOFF_ENABLE=False 或 HOME_RETURN_ASYNC_ENABLE=False -> 走 V2.4 step10+step11 分段。
    if SMOOTH_HANDOFF_ENABLE and HOME_RETURN_ASYNC_ENABLE:
        result = _smooth_handoff_return(mc, home_ready)
        print(f"  -> [V2.2 耗时] step 10+11 (平滑过渡): {time.time() - _t:.1f}s")
    else:
        # V2.4 分段路径（回退）：阻塞 step10 + 阻塞 step11
        print("  -> [V2.5] 平滑过渡未启用，走 V2.4 分段路径。")
        home_ready_actual_angles = checked_return_transition(mc, home_ready, "home_ready")
        verify_coords_near(mc, home_ready["coords"], "home_ready")
        print(f"  -> [V2.2 耗时] step 10: {time.time() - _t:.1f}s")
        _t11 = time.time()
        result = safe_return_home(mc, cached_angles=home_ready_actual_angles)
        print(f"  -> [V2.2 耗时] step 11: {time.time() - _t11:.1f}s")
    if result == "failed":
        # 回零失败时臂可能偏高/带载，直接 release 会让臂下沉。提示扶稳再释放。
        print("\n【警告】自动回零失败，机械臂可能偏高或带载。")
        print("-> 请【用手扶稳机械臂】防止下沉，确认扶稳后按 Enter 释放舵机...")
        input()
        mc.release_all_servos()
        print("已释放舵机。请人工扶正后重试或重新示教。")
        return False  # V2.7：舵机已掉电，通知 main 中止剩余轮次，避免对软臂下发运动指令

    print("\n====================================")
    if result == "auto":
        print("🎉 V2.5 五点示教 + home_ready 回零过渡测试流程跑通！")
        print("   末段回零：0 轮人工扶正（home_ready -> HOME 自动回零）。")
    else:  # "manual"
        print("⚠️ V2.5 流程跑通，但末段回零触发了人工扶正后完成。")
        print("   说明 home_ready 示教余量不足或软通过把实际姿态推过 45°，")
        print("   下一轮建议示教更直立的 home_ready（目标 arm_max_diff<=40，"
               "J2 绝对值压到 35~38°）。")
    print("====================================")
    return True  # V2.7：本轮完整跑通（含末段回零），main 可继续下一轮


def _preset_path(name):
    """V2.3：预设名 -> 文件路径。name 是裸名（如 run12_3cm_inboard），
    实际文件 teach_points_<name>.json 位于 PRESETS_DIR。"""
    return os.path.join(PRESETS_DIR, f"teach_points_{name}.json")


def load_preset(name):
    """V2.3 §14.4 优先级1：从 JSON 加载预设五点。
    返回 (pick_hover, pick, drop_hover, drop, home_ready, meta)。
    meta 含 name/created_from_log/object_size_cm/safety/notes 供打印追溯。
    不做安全校验——校验由 acquire_points 后续 validate_* 完成。
    """
    path = _preset_path(name)
    if not os.path.isfile(path):
        raise FileNotFoundError(
            f"预设不存在: {path}（请在 {PRESETS_DIR} 下放 teach_points_{name}.json）")
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    pts = data["points"]
    pick_hover = make_teach_point("抓取悬停点 pick_hover",
                                  pts["pick_hover"]["angles"], pts["pick_hover"]["coords"], True)
    pick = make_teach_point("抓取下探点 pick",
                            pts["pick"]["angles"], pts["pick"]["coords"], False)
    drop_hover = make_teach_point("放置悬停点 drop_hover",
                                  pts["drop_hover"]["angles"], pts["drop_hover"]["coords"], True)
    drop = make_teach_point("放置下探点 drop",
                            pts["drop"]["angles"], pts["drop"]["coords"], False)
    home_ready = make_teach_point("回零中间点 home_ready",
                                  pts["home_ready"]["angles"], pts["home_ready"]["coords"], True)
    return pick_hover, pick, drop_hover, drop, home_ready, data


def save_preset(name, pick_hover, pick, drop_hover, drop, home_ready,
                created_from_log="", object_size_cm="", notes=""):
    """V2.3 §14.4 优先级1：保存五点到 JSON 预设。
    含 plan §13.3 优先级5 要求的元数据字段，便于追溯。
    """
    os.makedirs(PRESETS_DIR, exist_ok=True)
    path = _preset_path(name)
    # 计算各点半径 + home_ready arm_max_diff 写入 safety 字段
    def _r(p):
        c = p["coords"]
        return round(math.sqrt(c[0] ** 2 + c[1] ** 2), 1)
    _, _, hr_arm_max, _ = calc_home_diffs(home_ready["angles"])
    data = {
        "name": name,
        "created_from_log": created_from_log,
        "object_size_cm": object_size_cm,
        "points": {
            "pick_hover": {"angles": pick_hover["angles"], "coords": pick_hover["coords"]},
            "pick": {"angles": pick["angles"], "coords": pick["coords"]},
            "drop_hover": {"angles": drop_hover["angles"], "coords": drop_hover["coords"]},
            "drop": {"angles": drop["angles"], "coords": drop["coords"]},
            "home_ready": {"angles": home_ready["angles"], "coords": home_ready["coords"]},
        },
        "safety": {
            "radii": {
                "pick_hover": _r(pick_hover), "pick": _r(pick),
                "drop_hover": _r(drop_hover), "drop": _r(drop),
                "home_ready": _r(home_ready),
            },
            "home_ready_arm_max_diff": round(hr_arm_max, 1),
            "R_MAX": R_MAX,
            "HOME_READY_TARGET_ARM_MAX": HOME_READY_TARGET_ARM_MAX,
            "ARM_MAX_DIFF_SAFE": ARM_MAX_DIFF_SAFE,
        },
        "notes": notes,
    }
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    print(f"  -> [V2.3] 预设已保存: {path}")
    return path


def print_preset_summary(data):
    """V2.3：加载预设后打印元数据 + 五点摘要 + 半径分区，供用户确认。"""
    print(f"\n=== 预设加载: {data.get('name', '?')} ===")
    print(f"  来源日志: {data.get('created_from_log', '?')}")
    print(f"  物块尺寸: {data.get('object_size_cm', '?')} cm")
    safety = data.get("safety", {})
    print(f"  safety: home_ready arm_max_diff={safety.get('home_ready_arm_max_diff', '?')}, "
          f"R_MAX={safety.get('R_MAX', '?')}")
    notes = data.get("notes", "")
    if notes:
        print(f"  notes: {notes}")
    print("  -> 注意：预设只跳过手动示教采集，仍会跑全部安全门（半径分区/"
          "validate_short_angle_pair/validate_return_angle_pair/validate_return_ready/"
          "用户确认轨迹）。")


def validate_preset_points(pick_hover, pick, drop_hover, drop, home_ready):
    """V2.3 §14.4：加载预设后必须执行的安全门（与示教期等价，但不重新采集）。
    1. is_safe_coord 业务门（pick/drop 系列）
    2. 半径分区诊断（pick/drop 系列）
    3. home_ready 读数有效性 + R_MAX 物理臂展 + arm_max_diff<=45 硬门
    4. validate_short_angle_pair 四对短距离连续性
    5. validate_return_angle_pair drop_hover->home_ready 过渡 sanity
    6. validate_return_ready home_ready 自动回零安全门
    任一失败抛异常，禁止进入实机自动动作。
    """
    print("\n=== 预设安全门校验 ===")
    # 1+2：pick/drop 业务门 + 半径分区
    for p, is_hover in ((pick_hover, True), (pick, False),
                        (drop_hover, True), (drop, False)):
        if not is_safe_coord(p["coords"], is_hover=is_hover):
            raise RuntimeError(f"预设安全门拦截: {p['name']} 坐标不在业务安全范围: {p['coords']}")
        print_radius_diagnostic(p["coords"], p["name"])
    # 3：home_ready 物理臂展 + arm_max_diff
    hr_c = home_ready["coords"]
    if not is_valid_coord_reading(hr_c):
        raise RuntimeError(f"预设安全门拦截: home_ready 坐标读数无效: {hr_c}")
    hr_r = math.sqrt(hr_c[0] ** 2 + hr_c[1] ** 2)
    if hr_r > R_MAX:
        raise RuntimeError(f"预设安全门拦截: home_ready 超物理臂展 R={hr_r:.1f} > {R_MAX}")
    _, _, hr_arm_max, _ = calc_home_diffs(home_ready["angles"])
    print(f"  -> home_ready 校验: R={hr_r:.1f}mm, arm_max_diff={hr_arm_max:.1f} "
          f"(门 {ARM_MAX_DIFF_SAFE})")
    if hr_arm_max > ARM_MAX_DIFF_SAFE:
        raise RuntimeError(
            f"预设安全门拦截: home_ready arm_max_diff={hr_arm_max:.1f} > {ARM_MAX_DIFF_SAFE}，"
            f"请重新示教更直立的 home_ready 并更新预设")
    # 4：短距离连续性
    validate_short_angle_pair(pick_hover, pick, "pick_hover->pick")
    validate_short_angle_pair(pick, pick_hover, "pick->pick_hover")
    validate_short_angle_pair(drop_hover, drop, "drop_hover->drop")
    validate_short_angle_pair(drop, drop_hover, "drop->drop_hover")
    print("  -> 四对短距离点对关节连续性校验通过。")
    # 5：回零过渡 sanity
    validate_return_angle_pair(drop_hover, home_ready, "drop_hover->home_ready")
    # 6：home_ready 自动回零安全门（含 40~45 警告）
    if not validate_return_ready(home_ready):
        raise RuntimeError("预设安全门拦截: home_ready 未通过 validate_return_ready")
    print("  -> 预设安全门校验全部通过。")


def acquire_points(mc, args):
    """V2.3：根据 CLI 参数决定示教 or 加载预设，返回五点。
    - 无 --preset：走 teach_phase_v2 手动示教（--save-preset 时保存）
    - 有 --preset：加载预设 + validate_preset_points（不示教、不掉电）
    预设路径不释放舵机、不要求用户拖动；但 prepare_phase 仍会 power_on + safe_return_home。
    """
    if args.preset:
        pick_hover, pick, drop_hover, drop, home_ready, data = load_preset(args.preset)
        print_preset_summary(data)
        validate_preset_points(pick_hover, pick, drop_hover, drop, home_ready)
        # 预设加载后仍要求用户确认轨迹无障碍（plan §14.4：不能因预设跳过）
        print("\n⚠️ 预设加载完成，请确认机械臂当前位置 + 预设轨迹范围内无障碍物。")
        input("-> 确认无误后按 Enter 继续（Ctrl+C 放弃）...")
        return pick_hover, pick, drop_hover, drop, home_ready

    points = teach_phase_v2(mc)
    if args.save_preset:
        save_preset(
            args.save_preset, *points,
            created_from_log=args.save_preset_log or "",
            object_size_cm=args.save_preset_obj or "",
            notes=args.save_preset_notes or "")
    return points


def main():
    mc = None
    # V2.3 §14.4 优先级1：CLI 参数
    parser = argparse.ArgumentParser(
        description="myCobot 280 五点示教回放 + home_ready 回零过渡 (V2.3)")
    parser.add_argument("--port", default=None,
                        help="指定串口（如 COM10），不填则自动检测")
    parser.add_argument("--preset", default=None,
                        help="加载 JSON 预设（裸名，对应 presets/teach_points_<name>.json），"
                             "跳过手动示教但仍跑全部安全门")
    parser.add_argument("--save-preset", dest="save_preset", default=None,
                        help="示教完成后保存为 JSON 预设（裸名）")
    parser.add_argument("--save-preset-log", dest="save_preset_log", default="",
                        help="保存预设时记录的来源日志名")
    parser.add_argument("--save-preset-obj", dest="save_preset_obj", default="",
                        help="保存预设时记录的物块尺寸(cm)")
    parser.add_argument("--save-preset-notes", dest="save_preset_notes", default="",
                        help="保存预设时的备注")
    parser.add_argument("--no-log", dest="no_log", action="store_true",
                        help="V2.8 点3：禁用自动终端日志导出（默认启用，写到 "
                             "audit_logs/auto_run_<时间戳>.log）")
    args = parser.parse_args()

    # V2.8 点3：安装 Tee 双写 stdout，自动导出终端日志到 UTF-8 文件。
    # --no-log 可禁用。装在 try 之前，确保后续 get_port/连接/示教/运动全过程都被记录。
    # try/finally 确保异常/Ctrl+C/正常退出时文件关闭落盘。
    tee = None
    original_stdout = sys.stdout
    if not args.no_log:
        log_path = os.path.join(
            AUTO_LOGS_DIR, f"auto_run_{time.strftime('%Y%m%d_%H%M%S')}.log")
        tee = Tee(log_path, original_stdout)
        sys.stdout = tee
        print(f"[V2.8 日志] 终端输出同步写入: {log_path}")
    try:
        PORT = args.port if args.port else get_port()
        BAUD = 1000000

        print(f"尝试连接机械臂 ({PORT} @ {BAUD})...")
        mc = MyCobot(PORT, BAUD)
        time.sleep(0.5)

        pick_hover, pick, drop_hover, drop, home_ready = acquire_points(mc, args)

        if not prepare_phase(mc):
            return

        # V2.7：连续多次抓取循环（贴近比赛条件）。
        # auto_phase_v2 末段已回零到 HOME 且夹爪张开，每轮结束臂处于 HOME；
        # 示教点与安全门已在上游确定，循环只重复关节回放，不重跑示教/prepare_phase。
        # auto_phase_v2 返回 False（home_ready 安全门未过 / 回零失败已 release）即 break，
        # 避免在舵机掉电状态下继续下发运动指令。
        # V2.8 点2：去掉 auto_phase_v2 内每轮 Enter，输入 N 后全自动运行；
        # 此处保留 N 轮开始前的一次性确认（物块就位 + 轨迹无障碍 + 急停提醒），
        # 确认后 N 轮连续跑，不再阻塞。赛场流程为 CPU 信号→单轮抓放→人补块到 pick 点。
        ans = input("\n-> 请输入本轮连续抓取次数（直接回车默认 1 次）: ").strip()
        try:
            repeat = max(1, int(ans)) if ans else 1
        except ValueError:
            print("【提示】输入非整数，按默认 1 次执行。")
            repeat = 1
        print(f"-> 将连续执行 {repeat} 次抓取任务（任一轮失败即中止，全程 Ctrl+C 急停）。")
        print("⚠️ 请确认：① pick 点已放好物块（多轮时由人持续补块）；"
              "② 机械臂轨迹范围内无障碍物。")
        input("-> 确认无误后按 Enter 开始连续抓取（此后 N 轮全自动，不再提示）...")
        for i in range(repeat):
            if repeat > 1:
                print(f"\n########## 连续抓取 第 {i + 1}/{repeat} 轮 ##########")
            ok = auto_phase_v2(mc, pick_hover, pick, drop_hover, drop, home_ready)
            if not ok:
                print(f"\n【中止】第 {i + 1} 轮未正常完成（已释放舵机），"
                      f"剩余 {repeat - i - 1} 轮不再执行。")
                break

    except TeachAbort as e:
        # V2.3 §14.4 优先级3：用户主动放弃示教。teach 阶段已 release_all_servos，
        # 舵机已软，无需再 release。直接退出。
        print(f"\n🛑 [放弃] {e}")
        print("-> 已放弃本轮示教，舵机在示教阶段已掉电变软。可重新运行或人工扶正归位。")
    except KeyboardInterrupt:
        print("\n🚨 [中断] 捕获到人工急停 (Ctrl+C)！正在紧急制动...")
        if mc:
            mc.stop()
            time.sleep(0.1)
            mc.release_all_servos()
            print("已切断所有舵机动力，机械臂完全变软。")
    except Exception as e:
        print(f"\n🚨 [异常] 运行中发生错误或运动超时: {e}")
        # §16.5：普通动作异常不应立即 release_all_servos()——
        # run-7 失败时臂已抬到接近 pick_hover（Z=92mm），直接掉电会下沉，
        # 必须提示用户扶稳后再释放。KeyboardInterrupt 仍保持急停释放。
        # V2.3：TeachAbort 已单独处理（舵机已软，不进此分支）。
        print("-> 请用手扶稳机械臂/物块防止下沉/掉落，扶稳后按 Enter 释放舵机...")
        if mc:
            try:
                mc.stop()
            except Exception:
                pass
            # V2.11 缺陷A修复（run-23 EOF 死锁）：管道喂入启动时 stdin 在 input() 前耗尽
            # 会抛 EOFError，绕过 release_all_servos() 进程死亡 → 臂上电锁死悬空发热。
            # 修复：包住 input()，EOF 或任何异常都立即 release（放弃"扶稳再释放"语义，
            # 烫伤/机械锁死比缓慢下沉更危险——run-23 已由人工手动 release 解救过一次）。
            arm_released = False
            try:
                input()
            except (EOFError, KeyboardInterrupt):
                print("-> ⚠️ stdin 不可用（EOF/中断），跳过扶稳确认，立即释放舵机防上电锁死。")
                arm_released = True
            finally:
                # 无论 input() 走哪条路径（正常 Enter / EOF / KeyboardInterrupt），都释放。
                # 正常路径：用户已扶稳再释放；EOF 路径：立即释放防卡死。
                try:
                    mc.release_all_servos()
                    if arm_released:
                        print("已紧急释放所有舵机（EOF 兜底，请人工扶回安全姿态）。")
                    else:
                        print("已在人工扶稳后释放所有舵机。")
                except Exception as release_e:
                    print(f"-> ⚠️ 释放舵机指令发送失败: {release_e}，请人工断电扶稳。")
    finally:
        # V2.8 点3：无论正常退出/异常/Ctrl+C，都恢复 stdout 并关闭日志文件落盘。
        if tee is not None:
            sys.stdout = original_stdout
            tee.close()
            # 用原 stdout 打印日志路径（此时终端已恢复，不进日志文件）
            original_stdout.write(f"[V2.8 日志] 终端日志已保存: {tee.path}\n")


if __name__ == "__main__":
    main()
