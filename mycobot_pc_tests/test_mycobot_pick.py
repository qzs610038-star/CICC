import time
from pymycobot.mycobot import MyCobot

PORT = 'COM9'
BAUD = 1000000
SPEED = 20  # 极低速 20%
GRIPPER_SPEED = 50

# 预定义的关键点位 (关节角度数组 [J1, J2, J3, J4, J5, J6])
# 机械臂已完成零点校准，此时 [0,0,0,0,0,0] 即为标准物理直立零位。
# J1: 底座旋转 (0为正前)
# J2, J3, J4: 俯仰关节
# J5, J6: 腕部末端姿态
ANGLES_HOME = [0, 0, 0, 0, 0, 0]             # 标准直立归零姿态
ANGLES_HOVER = [0, 20, -40, 20, 0, 0]        # 移动到目标正上方准备 (标准前倾)
ANGLES_PICK = [0, 40, -60, 20, 0, 0]         # 探下身子准备夹取 (标准下探)

def wait_movement(seconds):
    """延时等待机械臂走到位"""
    print(f"等待 {seconds} 秒...")
    time.sleep(seconds)

def main():
    print(f"【大范围夹取实验】尝试连接机械臂 ({PORT} @ {BAUD})...")
    try:
        mc = MyCobot(PORT, BAUD)
        time.sleep(0.5)
    except Exception as e:
        print(f"连接失败: {e}")
        return

    # --- 安全检查：判断当前姿态是否离目标初始位姿太远 ---
    angles = mc.get_angles()
    if not isinstance(angles, list) or len(angles) < 6:
        print(f"错误：无法读取当前角度 (返回值: {angles})，退出程序！")
        return

    print(f"当前角度为: {angles}")
    print(f"目标初始零位: {ANGLES_HOME}")

    max_diff = max([abs(a - b) for a, b in zip(angles, ANGLES_HOME)])
    if max_diff > 45.0:
        print(f"【安全拦截】当前姿态与目标零位差距过大 (最大偏差 {max_diff:.1f}度 > 45度)！")
        print("为防止直接移动导致机械臂大幅横扫撞击，程序已自动终止。")
        print("-> 请先运行 stop_mycobot.py，用手将机械臂大致扶正到直立姿态，然后再运行本脚本。")
        return

    print("====================")
    print("【提示】姿态安全检查通过，即将开始大范围运动测试！")
    print("请确保机械臂正前方无障碍物，且随时准备拔掉电源！")
    for i in range(3, 0, -1):
        print(f"{i} 秒后开始...")
        time.sleep(1)

    print("\n--- 1. 初始化状态 ---")
    print("张开夹爪...")
    mc.set_gripper_state(0, GRIPPER_SPEED)
    time.sleep(2)

    print(f"机械臂归零位 {ANGLES_HOME} ...")
    mc.send_angles(ANGLES_HOME, SPEED)
    wait_movement(5) # 归零可能需要较长时间，给予5秒

    print("\n--- 2. 移动到目标上方 ---")
    print(f"下发悬停坐标 {ANGLES_HOVER} ...")
    mc.send_angles(ANGLES_HOVER, SPEED)
    wait_movement(4)

    print("\n--- 3. 下降并夹取 ---")
    print(f"下发夹取坐标 {ANGLES_PICK} ...")
    mc.send_angles(ANGLES_PICK, SPEED)
    wait_movement(4)

    print("闭合夹爪，抓取物体...")
    mc.set_gripper_state(1, GRIPPER_SPEED)
    time.sleep(2)

    print("\n--- 4. 抬起目标 ---")
    print(f"返回悬停坐标 {ANGLES_HOVER} ...")
    mc.send_angles(ANGLES_HOVER, SPEED)
    wait_movement(4)

    print("\n--- 5. 归位并释放 ---")
    print(f"返回零位 {ANGLES_HOME} ...")
    mc.send_angles(ANGLES_HOME, SPEED)
    wait_movement(5)

    print("张开夹爪，释放物体...")
    mc.set_gripper_state(0, GRIPPER_SPEED)
    time.sleep(2)

    print("\n====================")
    print("实验结束！测试安全通过。")
    print("====================")

if __name__ == "__main__":
    main()
