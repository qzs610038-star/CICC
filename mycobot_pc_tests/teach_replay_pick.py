"""
关节角示教回放 + 空间坐标一致性校验 抓取/放置脚本
====================================================

> 目标脚本对照方案: mycobot_pc_tests/audit_logs/joint_space_teach_replay_plan.md
> 用途边界: PC 端交互调试、定点抓取验证、后续板端控制策略参考
> 不进入正式闭环: 本脚本属于 pymycobot PC 端联调归档，不作为决赛正式识别/控制闭环依赖。

设计要点（相对 teach_and_pick.py 的改造）:
  1. 四点示教: pick_hover / pick / drop_hover / drop，每个点同时记录
     `angles` 和 `coords`，悬停点由用户手工示教，不再自动 Z+60 推导。
  2. 长距离过渡（零位 <-> 悬停点、悬停点 <-> 悬停点、回零保护）
     统一使用 `sync_send_angles` 关节空间回放，绕开 IK 奇异。
  3. 仅短距离 hover <-> pick/drop 谨慎保留 mode=1 直线运动；失败即熔断，
     不自动 fallback 到 mode=0，避免不可预测轨迹。
  4. `get_filtered_coords()` 多次读取异常时返回 None，上层拒绝继续，
     绝不返回最后一次异常读数兜底。
  5. 关节角回放到位后用空间坐标做一致性校验，校验只用于确认到位，
     不再驱动后续规划。
  6. `checked_gripper_action()` 返回值在主流程中必须被处理。

实机测试前请阅读 trial_run_logs.md 中两次失败记录，理解 IK 奇异与
Z=425.4 异常读数的成因。空载分段验证步骤见 plan 第 8 节。
"""

import time
import sys
import math
import serial.tools.list_ports
from pymycobot.mycobot import MyCobot

# ============================================================
# 安全参数（保守值，参见 plan 第 7 节）
# ============================================================
ANG_REPLAY_SPEED = 20          # 关节角回放速度 (%)
ANG_REPLAY_TIMEOUT = 20        # 关节角回放超时 (s)
SHORT_LINEAR_SPEED = 15        # 短距离直线速度 (%)
SHORT_LINEAR_TIMEOUT = 10     # 短距离直线超时 (s)
GRIPPER_SPEED = 50             # 夹爪速度 (%)
GRIPPER_TIMEOUT = 2.5          # 夹爪开环等待 (s)

# 回零目标关节角（§12.1）：保留理论零位 [0,0,0,0,0,0]，不改成本机实测非零值。
# 实测直立姿态 [-10.81, 2.46, 1.49, -8.17, 2.28, 3.07] 相对零位 max_diff=10.81，
# 低于 45 度大偏差阈值，回零安全。
HOME_ANGLES = [0, 0, 0, 0, 0, 0]
# §13.2：大臂安全门只看 1-5 轴，第 6 轴末端旋转不阻断大臂回零。
ARM_JOINT_COUNT = 5
ARM_MAX_DIFF_SAFE = 45.0       # 大臂 1-5 轴大偏差阈值（度）
WRIST6_WARN_DIFF = 90.0        # 第 6 轴末端旋转提示阈值（度，仅告警不阻断）

COORD_VERIFY_TOL = 25.0        # 坐标一致性校验容差 (mm)
COORD_STABLE_TOL = 8.0         # 连续读数稳定性判断 (mm)
COORD_RETRIES = 6              # get_filtered_coords 最大重试次数
ANG_RETRIES = 8                # get_filtered_angles 最大重试次数（§12.3 提高以等稳定读数）
ANG_STABLE_TOL = 3.0           # 连续两次关节角读数稳定性判断 (度，§12.3)
POWER_ON_SETTLE = 1.5          # power_on 后舵机抱紧/读数稳定等待 (s，§12.5)

# is_safe_coord 的固定阈值（与 teach_and_pick.py 保持一致；若实际工作点
# 超出该阈值，应先调整阈值常量，而不是绕过过滤）
# 注意：Z_MAX 仅作为"抓取/放置工作区"边界（业务级安全），不作为读数有效性边界。
# 直立安全姿态下 Z 可达 ~417mm（§12.1 实测），读数有效性由 is_valid_coord_reading
# 用更宽松的范围判断，避免直立高位坐标被误判无效。
Z_MAX = 280.0
Z_MIN_PICK = 5.0
Z_MIN_HOVER = 65.0
R_MIN = 60.0
R_MAX = 280.0

# 读数有效性边界（§12.2）：仅过滤明显异常的串口读数，不限制工作区。
COORD_VALID_XY = 500.0
COORD_VALID_Z_MIN = -100.0
COORD_VALID_Z_MAX = 500.0
COORD_VALID_RPY = 360.0


# ============================================================
# 串口选择（沿用 teach_and_pick.py 风格）
# ============================================================
def get_port():
    if len(sys.argv) > 1:
        return sys.argv[1]

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
    """三维安全限制检查，返回 bool。"""
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
    内部读数过滤，避免直立高位坐标被误判无效。
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


def record_teach_point(mc, name, is_hover):
    """
    采集一个示教点：要求用户手动拖动到位、稳定后回车读取，
    angles + coords 必须同时合法；用户二次确认后才保存。
    """
    input(f"-> 请手动拖动到【{name}】，保持稳定后按 Enter 读取...")
    angles = get_filtered_angles(mc)
    coords = get_filtered_coords(mc)

    if angles is None:
        raise RuntimeError(f"{name}: 无法读取稳定关节角")
    if coords is None:
        raise RuntimeError(f"{name}: 无法读取稳定空间坐标")
    if not is_safe_coord(coords, is_hover=is_hover):
        raise RuntimeError(f"{name}: 坐标不在安全范围内: {coords}")

    print(f"{name} angles = {angles}")
    print(f"{name} coords  = {coords}")
    ans = input("确认保存该示教点吗？(y/n): ")
    if ans.lower() != "y":
        raise RuntimeError(f"{name}: 用户取消保存")
    return make_teach_point(name, angles, coords, is_hover)


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


def checked_short_linear(mc, target_coords, speed, timeout, label):
    """
    短距离直线动作（mode=1），仅用于 hover <-> pick/drop。
    目标坐标必须通过 is_safe_coord；失败抛异常，不自动 fallback。
    """
    if not is_safe_coord(target_coords):
        raise RuntimeError(f"{label}: 目标坐标不安全: {target_coords}")
    print(f"  -> 短距离过渡到 {label}: {target_coords}")
    # 将 mode 从 1 (空间直线插补) 改为 0 (关节空间插值)，以彻底规避远端工作区奇异点熔断问题。
    # 在 40mm 的极短位移中，关节插值引起的末端摆动偏移极微（通常小于 2mm），不影响精准对齐。
    res = mc.sync_send_coords(target_coords, speed, 0, timeout=timeout)
    if res != 1:
        raise RuntimeError(f"{label}: 短距离直线动作超时或失败 (返回 {res})")
    return True


def verify_coords_near(mc, expected, label, xyz_tol=COORD_VERIFY_TOL):
    """
    关节角回放后再读当前坐标，只做一致性校验，不用于继续规划。
    delta 取 XYZ 三轴最大绝对差；超容差抛异常。
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
def safe_return_home(mc):
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
         - 通过则低速回零；用户放弃或仍大偏差则返回 False。
    """
    angles = get_filtered_angles(mc)
    if angles is None:
        print("【错误】无法读取稳定关节角，拒绝回零！")
        print("-> 请人工扶正到接近直立姿态后重试，或重新示教。")
        return False

    all_diffs, arm_diffs, arm_max_diff, wrist6_diff = calc_home_diffs(angles)
    if not all(math.isfinite(d) for d in all_diffs):
        print(f"【错误】关节角偏差含非有限值，拒绝回零！angles={angles}")
        return False

    # §13.5：日志术语区分大臂与第 6 轴
    print(f"当前稳定关节角: {angles}")
    print(f"相对 HOME_ANGLES 全轴偏差: {all_diffs}")
    print(f"大臂 1-5 轴偏差: {arm_diffs}, arm_max_diff={arm_max_diff:.1f}")
    print(f"第 6 轴末端旋转偏差: {wrist6_diff:.1f}")

    # 诊断坐标（只用于诊断，不构造回零目标）
    diag_coords = get_filtered_coords(mc)
    print(f"当前稳定空间坐标: {diag_coords}")

    if arm_max_diff <= ARM_MAX_DIFF_SAFE:
        # §13.4：第 6 轴较大时提示末端旋转风险，但不阻断回零
        if wrist6_diff > WRIST6_WARN_DIFF:
            print(f"【提示】第6轴末端旋转偏差较大 ({wrist6_diff:.1f}度)，"
                  f"回零时夹爪会自转，请确认末端线缆/夹爪周边无干涉。")
        print(f"arm_max_diff={arm_max_diff:.1f} <= {ARM_MAX_DIFF_SAFE}，低速同步回直立零位...")
        res = mc.sync_send_angles(HOME_ANGLES, 15, timeout=ANG_REPLAY_TIMEOUT)
        if res != 1:
            print("【警告】回零动作超时或被物理阻挡！")
            return False
        return True

    # 大偏差分支：人工扶正状态机（§13.3）
    print(f"\n【警告】当前大臂 1-5 轴与零位偏差较大 (arm_max_diff={arm_max_diff:.1f}度 > {ARM_MAX_DIFF_SAFE}度)。")
    print("-> 不自动执行笛卡尔保护拉升。进入人工扶正流程。")
    angles = prompt_manual_prehome(mc)
    if angles is None:
        print("【错误】人工扶正未通过大臂安全门，拒绝自动回零。请人工扶正到直立姿态后重跑。")
        return False

    all_diffs, arm_diffs, arm_max_diff, wrist6_diff = calc_home_diffs(angles)
    if wrist6_diff > WRIST6_WARN_DIFF:
        print(f"【提示】第6轴末端旋转偏差较大 ({wrist6_diff:.1f}度)，"
              f"回零时夹爪会自转，请确认末端线缆/夹爪周边无干涉。")
    print(f"扶正通过，arm_max_diff={arm_max_diff:.1f}，低速同步回直立零位...")
    res = mc.sync_send_angles(HOME_ANGLES, 15, timeout=ANG_REPLAY_TIMEOUT)
    if res != 1:
        print("【警告】回零动作超时或被物理阻挡！")
        return False
    return True


# ============================================================
# 主流程
# ============================================================
def teach_phase(mc):
    """第一阶段：释放舵机后四点示教。"""
    print("====================================")
    print("【第一阶段：手动示教记录 (四点: pick_hover/pick/drop_hover/drop)】")
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

    print("\n=== 示教点汇总 ===")
    for p in (pick_hover, pick, drop_hover, drop):
        print(f"  {p['name']}: angles={p['angles']}  coords={p['coords']}")
    return pick_hover, pick, drop_hover, drop


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
    if not safe_return_home(mc):
        # 回零失败时臂可能偏高/带载，直接 release_all_servos（阻尼）会让臂下沉摆动撞物。
        # 先提示用户扶稳，确认后再释放。
        print("\n【警告】回零失败，机械臂当前位置可能偏高或偏离零位。")
        print("-> 请【用手扶稳机械臂】防止下沉，确认扶稳后按 Enter 释放舵机...")
        input()
        mc.release_all_servos()
        print("已释放舵机。请人工扶正到接近直立姿态后重试。")
        return False
    return True


def auto_phase(mc, pick_hover, pick, drop_hover, drop):
    """
    第三阶段：关节角回放 + 短距离直线 + 坐标校验。
    长距离用 checked_sync_angles；短距离 hover<->pick/drop 用 checked_short_linear。
    任一动作失败即抛异常熔断，不自动 fallback。
    """
    print("\n====================================")
    print("【第三阶段：关节角示教回放 + 空间一致性校验】")
    print("====================================")
    print("⚠️ 请确认轨迹范围内无障碍物。如遇危险，请随时按 Ctrl+C 触发急停！")
    input("-> 请将正方体放回【抓取点】，按 Enter 键开始（空载首测可不放物块）...")

    print("\n1. 张开夹爪准备...")
    gripper_action_with_retry(mc, 0, "step1 张开")

    print("\n2. 关节角回放到 pick_hover...")
    checked_sync_angles(mc, pick_hover["angles"], ANG_REPLAY_SPEED, ANG_REPLAY_TIMEOUT, "pick_hover")
    verify_coords_near(mc, pick_hover["coords"], "pick_hover")

    print("\n3. 短距离直线下探到 pick...")
    checked_short_linear(mc, pick["coords"], SHORT_LINEAR_SPEED, SHORT_LINEAR_TIMEOUT, "pick")

    print("\n4. 闭合夹爪抓取目标...")
    gripper_action_with_retry(mc, 1, "step4 闭合")

    print("\n5. 短距离直线抬起回 pick_hover...")
    checked_short_linear(mc, pick_hover["coords"], SHORT_LINEAR_SPEED, SHORT_LINEAR_TIMEOUT, "pick_hover")

    print("\n6. 关节角回放到 drop_hover（空中长距离过渡）...")
    checked_sync_angles(mc, drop_hover["angles"], ANG_REPLAY_SPEED, ANG_REPLAY_TIMEOUT, "drop_hover")
    verify_coords_near(mc, drop_hover["coords"], "drop_hover")

    print("\n7. 短距离直线下降至 drop...")
    checked_short_linear(mc, drop["coords"], SHORT_LINEAR_SPEED, SHORT_LINEAR_TIMEOUT, "drop")

    print("\n8. 张开夹爪放置正方体...")
    gripper_action_with_retry(mc, 0, "step8 张开")

    print("\n9. 短距离直线抬起回 drop_hover...")
    checked_short_linear(mc, drop_hover["coords"], SHORT_LINEAR_SPEED, SHORT_LINEAR_TIMEOUT, "drop_hover")

    print("\n10. 任务完成，准备返回直立安全零位...")
    if not safe_return_home(mc):
        # 回零失败时臂可能偏高/带载，直接 release 会让臂下沉。提示扶稳再释放。
        print("\n【警告】自动回零失败，机械臂可能偏高或带载。")
        print("-> 请【用手扶稳机械臂】防止下沉，确认扶稳后按 Enter 释放舵机...")
        input()
        mc.release_all_servos()
        print("已释放舵机。请人工扶正后重试或重新示教。")
        return

    print("\n====================================")
    print("🎉 关节角示教回放测试流程跑通！")
    print("====================================")


def main():
    mc = None
    try:
        PORT = get_port()
        BAUD = 1000000

        print(f"尝试连接机械臂 ({PORT} @ {BAUD})...")
        mc = MyCobot(PORT, BAUD)
        time.sleep(0.5)

        pick_hover, pick, drop_hover, drop = teach_phase(mc)

        if not prepare_phase(mc):
            return

        auto_phase(mc, pick_hover, pick, drop_hover, drop)

    except KeyboardInterrupt:
        print("\n🚨 [中断] 捕获到人工急停 (Ctrl+C)！正在紧急制动...")
        if mc:
            mc.stop()
            time.sleep(0.1)
            mc.release_all_servos()
            print("已切断所有舵机动力，机械臂完全变软。")
    except Exception as e:
        print(f"\n🚨 [异常] 运行中发生错误或运动超时: {e}")
        if mc:
            mc.stop()
            time.sleep(0.1)
            mc.release_all_servos()
            print("安全起见，已紧急释放所有舵机。")


if __name__ == "__main__":
    main()