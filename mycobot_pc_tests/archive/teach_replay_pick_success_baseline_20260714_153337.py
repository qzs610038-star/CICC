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
  3. §14.3：短距离 hover <-> pick/drop 也改为纯关节角回放
     (`checked_short_angles`)，不再调用 `sync_send_coords()`。原因：
     `mode=0` 只表示关节空间插值，不表示绕过 IK——只要调用
     `sync_send_coords()`，固件端仍必须把笛卡尔目标解算为关节角，
     远端低位/腕部多解/边界点仍可能快速返回 0。
  4. §14.4：短距离 hover/down 点对先做关节连续性校验
     (`validate_short_angle_pair`)，防止示教落在不同 IK 分支导致大幅摆动。
  5. `get_filtered_coords()` 多次读取异常时返回 None，上层拒绝继续，
     绝不返回最后一次异常读数兜底。
  6. `coords` 只用于 `verify_coords_near()` 等只读到位校验，
     不参与任何运动规划（§14.3 验收硬性项）。
  7. §16.3/§16.4 方向分离速度/超时 + 软到位判定：下行（重力辅助）用
     SHORT_DOWN_SPEED=12 / SHORT_DOWN_TIMEOUT=10；上行（抗重力）用
     SHORT_UP_SPEED=16 / SHORT_UP_TIMEOUT=25。run-7 确诊上行失败时
     max_angle_delta 仅 2.2°（固件到位判定未收敛），因此新增软到位：
     sync_send_angles 返回 0 后读实际角度/坐标，若 max_angle_delta ≤ 3.0°
     且 delta_xyz ≤ 25mm 则软通过。上行动作允许软通过，下行保持严格。
  8. §16.5 异常退出策略：普通 RuntimeError 先提示用户扶稳再释放舵机，
     避免臂已抬起时直接掉电下沉。KeyboardInterrupt 急停仍保持立即释放。
  9. `checked_gripper_action()` 返回值在主流程中必须被处理。

实机测试前请阅读 trial_run_logs.md 中失败记录，
以及 joint_space_teach_replay_plan.md §15 的验证分析。
空载分段验证步骤见 plan 第 8 节与 §14.6。
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
# §16.3 / §16.8：方向分离速度/超时参数——下行（重力辅助）保持低速短超时，
# 上行（抗重力）适当提速并放宽超时窗口。run-7 确证根因为固件到位判定
# 未收敛（max_angle_delta=2.2° 仍返回 0），因此上行超时从 15s 增至 25s。
SHORT_DOWN_SPEED = 12          # 下行关节速度（重力辅助）(%)
SHORT_DOWN_TIMEOUT = 10        # 下行关节超时 (s)
SHORT_UP_SPEED = 16            # 上行动作适当提速，克服逆重力阻力 (%)
SHORT_UP_TIMEOUT = 25          # 上行动作放宽超时窗口 (s)，§16.3 从 15 增至 25
# §16.4 软到位成功判定——sync_send_angles 返回 0 后先读数再决断，
# 物理已接近目标（max_angle_delta <= 3° 且 delta_xyz <= 25mm）时软通过。
SOFT_ANGLE_SUCCESS_TOL = 3.0   # 软到位关节角度容差 (deg)
SOFT_COORD_SUCCESS_TOL = 25.0  # 软到位坐标容差 (mm)，复用 COORD_VERIFY_TOL
SOFT_SETTLE_SECONDS = 0.5      # 软到位前等待舵机稳定 (s)
# §14.4：短距离 hover/down 点对关节连续性安全门，防止示教落在不同 IK 分支。
# 1-5 轴短距离单轴变化过大说明 hover/down 可能不在同一解分支，回放会大幅摆动。
SHORT_ARM_JOINT_MAX_DELTA = 30.0   # 短距离 1-5 轴单轴最大变化（度）
SHORT_WRIST6_MAX_DELTA = 45.0      # 短距离第 6 轴末端旋转最大变化（度）
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
    """
    if not isinstance(target_angles, list) or len(target_angles) < 6:
        raise RuntimeError(f"{label}: 目标关节角非法: {target_angles}")
    if not all(isinstance(v, (int, float)) and math.isfinite(v)
               and -180.0 <= v <= 180.0 for v in target_angles[:6]):
        raise RuntimeError(f"{label}: 目标关节角含非数值或越界: {target_angles}")
    print(f"  -> 短距离关节回放到 {label}: {target_angles} "
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

        if angle_ok and coord_ok:
            print(f"  -> [软通过] {label}: 固件返回 {res}，但实际姿态已在容差内，"
                  f"继续执行后续坐标校验。")
            return True

    raise RuntimeError(f"{label}: 短距离角度动作超时或失败 (返回 {res})")


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
    第三阶段：纯关节角回放 + 空间坐标只读校验（§14.3/§14.4）。
    长距离用 checked_sync_angles；短距离 hover<->pick/drop 用
    checked_short_angles（关节角驱动，不再调用 sync_send_coords）。
    coords 只保留给 verify_coords_near 做到位一致性校验，不参与运动规划。
    任一动作失败即抛异常熔断，不自动 fallback。

    §14.4：实机动作前先校验四对短距离点对的关节连续性，任一校验
    失败则禁止进入实机自动动作，要求用户重新示教对应点位。
    """
    print("\n====================================")
    print("【第三阶段：关节角示教回放 + 空间一致性校验】")
    print("====================================")

    # §14.4：短距离点对关节连续性预校验（只读，不发任何运动指令）。
    # 任一对落在不同 IK 分支即在此拦截，避免实机大幅摆动/腕部翻转。
    print("\n0. 短距离点对关节连续性预校验...")
    validate_short_angle_pair(pick_hover, pick, "pick_hover->pick")
    validate_short_angle_pair(pick, pick_hover, "pick->pick_hover")
    validate_short_angle_pair(drop_hover, drop, "drop_hover->drop")
    validate_short_angle_pair(drop, drop_hover, "drop->drop_hover")
    print("  -> 四对短距离点对关节连续性校验通过。")

    print("\n⚠️ 请确认轨迹范围内无障碍物。如遇危险，请随时按 Ctrl+C 触发急停！")
    input("-> 请将正方体放回【抓取点】，按 Enter 键开始（空载首测可不放物块）...")

    print("\n1. 张开夹爪准备...")
    gripper_action_with_retry(mc, 0, "step1 张开")

    print("\n2. 关节角回放到 pick_hover...")
    checked_sync_angles(mc, pick_hover["angles"], ANG_REPLAY_SPEED, ANG_REPLAY_TIMEOUT, "pick_hover")
    verify_coords_near(mc, pick_hover["coords"], "pick_hover")

    print("\n3. 短距离关节下探到 pick...")
    checked_short_angles(mc, pick["angles"], SHORT_DOWN_SPEED, SHORT_DOWN_TIMEOUT, "pick")
    verify_coords_near(mc, pick["coords"], "pick")

    print("\n4. 闭合夹爪抓取目标...")
    gripper_action_with_retry(mc, 1, "step4 闭合")

    print("\n5. 短距离关节抬起回 pick_hover...")
    checked_short_angles(
        mc, pick_hover["angles"], SHORT_UP_SPEED, SHORT_UP_TIMEOUT, "pick_hover",
        expected_coords=pick_hover["coords"],
        allow_soft_success=True,
    )
    verify_coords_near(mc, pick_hover["coords"], "pick_hover")

    print("\n6. 关节角回放到 drop_hover（空中长距离过渡）...")
    checked_sync_angles(mc, drop_hover["angles"], ANG_REPLAY_SPEED, ANG_REPLAY_TIMEOUT, "drop_hover")
    verify_coords_near(mc, drop_hover["coords"], "drop_hover")

    print("\n7. 短距离关节下降至 drop...")
    checked_short_angles(mc, drop["angles"], SHORT_DOWN_SPEED, SHORT_DOWN_TIMEOUT, "drop")
    verify_coords_near(mc, drop["coords"], "drop")

    print("\n8. 张开夹爪放置正方体...")
    gripper_action_with_retry(mc, 0, "step8 张开")

    print("\n9. 短距离关节抬起回 drop_hover...")
    checked_short_angles(
        mc, drop_hover["angles"], SHORT_UP_SPEED, SHORT_UP_TIMEOUT, "drop_hover",
        expected_coords=drop_hover["coords"],
        allow_soft_success=True,
    )
    verify_coords_near(mc, drop_hover["coords"], "drop_hover")

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
        # §16.5：普通动作异常不应立即 release_all_servos()——
        # run-7 失败时臂已抬到接近 pick_hover（Z=92mm），直接掉电会下沉，
        # 必须提示用户扶稳后再释放。KeyboardInterrupt 仍保持急停释放。
        print("-> 请用手扶稳机械臂/物块防止下沉/掉落，扶稳后按 Enter 释放舵机...")
        if mc:
            try:
                mc.stop()
            except Exception:
                pass
            input()
            mc.release_all_servos()
            print("已在人工扶稳后释放所有舵机。")


if __name__ == "__main__":
    main()