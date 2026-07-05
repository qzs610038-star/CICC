import time
import sys
import math
import serial.tools.list_ports
from pymycobot.mycobot import MyCobot

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
        except:
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
        except:
            pass
        print("【错误】无效输入，程序退出。")
        sys.exit(1)

def wait_movement(seconds):
    # 普通延时后备
    time.sleep(seconds)

def checked_sync_coords(mc, coords, speed, mode, timeout):
    """
    带超时与状态断言的坐标发送闭环。
    若超时或底层故障返回 0，则直接抛出异常以打断整体动作流。
    """
    print(f"  -> 下发坐标: {coords}")
    res = mc.sync_send_coords(coords, speed, mode, timeout=timeout)
    if res == 0:
        raise RuntimeError(f"坐标运动超时或未到达预期位置，目标坐标: {coords}")
    return res

def checked_gripper_action(mc, state, speed, timeout=2.5):
    """
    夹爪控制逻辑。
    state: 0 表示张开，1 表示闭合。
    若库不支持读取状态，则降级为安全延时保护。
    """
    action_str = "闭合" if state == 1 else "张开"
    print(f"  -> 下发夹爪动作: {action_str}...")
    mc.set_gripper_state(state, speed)

    # 动态探测反馈 API 是否存在
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

            # 停止变化阻挡判断：连续三次读数相差无几，视为夹住物体停止动作
            if abs(val - last_val) <= 2:
                stable_count += 1
                if stable_count >= 3:
                    print(f"  -> 传感器确认：夹爪受阻停止变化(当前值:{val})，视为动作完成。")
                    return True
            else:
                stable_count = 0
            last_val = val

    print(f"【警告】夹爪 {action_str} 动作在 {timeout}s 内未得到传感器明确到位反馈，继续流程...")
    time.sleep(0.5)
    return False

def is_safe_coord(coords, is_hover=False):
    """三维安全限制检查"""
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

    min_z = 5.0 if not is_hover else 65.0
    if z < min_z:
        print(f"【安全拦截】Z坐标 ({z}) 低于安全高度 ({min_z}mm)，存在撞击风险！")
        return False
    if z > 280.0:
        print(f"【安全拦截】Z坐标 ({z}) 过高，超出推荐工作空间！")
        return False

    radius = math.sqrt(x*x + y*y)
    if radius < 60.0:
        print(f"【安全拦截】目标位置离底座太近 (R={radius:.1f} < 60)，极易自我碰撞！")
        return False
    if radius > 280.0:
        print(f"【安全拦截】目标位置超出最大臂展 (R={radius:.1f} > 280)！")
        return False

    return True

def safe_return_home(mc):
    """改进的安全回零：先抬升高度，再低速同步回零"""
    angles = mc.get_angles()
    if not isinstance(angles, list) or len(angles) < 6:
        print("【错误】无法读取当前角度，拒绝回零！")
        return False

    max_diff = max([abs(a - b) for a, b in zip(angles, [0,0,0,0,0,0])])
    if max_diff > 45.0:
        print(f"\n【警告】当前姿态与零位偏差较大 ({max_diff:.1f}度 > 45度)。直接关节回零会产生大范围弧线扫动。")
        ans = input("确认周边无障碍物，且允许程序以【先高抬，再慢回】的安全策略复位吗？(y/n): ")
        if ans.lower() != 'y':
            return False

        # 大角度下必须执行拉升保护
        coords = mc.get_coords()
        if not isinstance(coords, list) or len(coords) < 6:
            print("【错误】回零保护失败：无法读取当前空间坐标，拒绝回零！")
            return False

        safe_z = max(coords[2], 180.0)
        coords[2] = safe_z
        print(f"执行回零路径保护：直线拉升至安全高度 Z={safe_z:.1f}...")

        # 必须检查保护动作的返回值，失败则终止回零
        res = mc.sync_send_coords(coords, 30, 1, timeout=5)
        if res == 0:
            print("【错误】抬高保护动作失败或超时！拒绝继续回零。")
            return False

    print("正在低速同步回直立零位...")
    # 采用同步低速，阻塞直到位
    res = mc.sync_send_angles([0,0,0,0,0,0], 15, timeout=10)
    if res == 0:
        print("【警告】回零动作超时或被物理阻挡！")
        return False
    return True

def main():
    mc = None
    try:
        # 将端口获取移至主函数，避免 import 时阻塞
        PORT = get_port()
        BAUD = 1000000

        print("====================================")
        print("【第一阶段：手动示教记录 (抓取点 & 释放点)】")
        print("====================================")
        print(f"尝试连接机械臂 ({PORT} @ {BAUD})...")
        mc = MyCobot(PORT, BAUD)
        time.sleep(0.5)

        print("⚠️ 警告：机械臂即将放松掉电，请立刻【用手扶稳机械臂】！")
        input("-> 准备好后，按 Enter 键释放舵机...")

        mc.release_all_servos()
        time.sleep(0.5)

        print("\n✅ 机械臂已变软。")
        print("【示教 1：抓取点 (Pick Point)】")
        input("👉 将夹爪拖动到【正方体物块】上方完美夹取位置，按 Enter 键记录...")

        pick_coords = mc.get_coords()
        if not is_safe_coord(pick_coords):
            print(f"❌ 抓取点触发安全拦截，请重新运行脚本。")
            return
        print(f"🎯 抓取点记录成功: {pick_coords}")

        print("\n【示教 2：释放点 (Drop Point)】")
        input("👉 继续手动将夹爪拖动到【放置终点位置】，按 Enter 键记录...")

        drop_coords = mc.get_coords()
        if not is_safe_coord(drop_coords):
            print(f"❌ 释放点触发安全拦截，请重新运行脚本。")
            return
        print(f"🎯 释放点记录成功: {drop_coords}")

        # 计算悬停坐标
        pick_hover = pick_coords.copy()
        pick_hover[2] += 60.0
        drop_hover = drop_coords.copy()
        drop_hover[2] += 60.0

        if not is_safe_coord(pick_hover, True) or not is_safe_coord(drop_hover, True):
            print("❌ 悬停点 Z 轴计算后超出安全高度范围，请重新示教！")
            return

        print("\n====================================")
        print("【第二阶段：恢复供电并检查安全回零】")
        print("====================================")
        input("-> 请将手轻轻拿开，按 Enter 键通电并执行安全回零...")

        mc.power_on()
        time.sleep(1)

        if not safe_return_home(mc):
            print("主动放弃回零，释放舵机保护设备。")
            mc.release_all_servos()
            return

        print("\n====================================")
        print("【第三阶段：全自动三维坐标定点转移】")
        print("====================================")
        print("⚠️ 请确认轨迹范围内无障碍物。如遇危险，请随时按 Ctrl+C 触发急停！")
        input("-> 请将正方体放回【抓取点】，按 Enter 键开始...")

        print("\n1. 张开夹爪准备...")
        checked_gripper_action(mc, 0, 50)

        TIMEOUT = 7
        SPEED = 30

        print("2. 移动到抓取悬停点...")
        checked_sync_coords(mc, pick_hover, SPEED, 1, TIMEOUT)

        print("3. 纯垂直下探抓取点...")
        checked_sync_coords(mc, pick_coords, 20, 1, TIMEOUT)

        print("4. 闭合夹爪抓取目标...")
        checked_gripper_action(mc, 1, 50)

        print("5. 直线垂直抬起带出...")
        checked_sync_coords(mc, pick_hover, SPEED, 1, TIMEOUT)

        print("6. 空中平移到释放点上方...")
        checked_sync_coords(mc, drop_hover, SPEED, 1, TIMEOUT)

        print("7. 纯垂直下降至释放点...")
        checked_sync_coords(mc, drop_coords, 20, 1, TIMEOUT)

        print("8. 张开夹爪放置正方体...")
        checked_gripper_action(mc, 0, 50)

        print("9. 直线抬起空夹爪退出...")
        checked_sync_coords(mc, drop_hover, SPEED, 1, TIMEOUT)

        print("10. 任务完成，准备返回直立安全零位...")
        if not safe_return_home(mc):
            print("终止自动回零，释放舵机。")
            mc.release_all_servos()
            return

        print("\n====================================")
        print("🎉 测试流程完美跑通！")
        print("====================================")

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
