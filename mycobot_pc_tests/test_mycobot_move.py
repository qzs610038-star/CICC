import time
from pymycobot.mycobot import MyCobot

PORT = 'COM9'
BAUD = 1000000

def main():
    print(f"尝试连接机械臂 ({PORT} @ {BAUD})...")
    try:
        mc = MyCobot(PORT, BAUD)
        time.sleep(0.5)
    except Exception as e:
        print(f"连接失败: {e}")
        return

    # 1. 严格要求先读到有效状态才允许动作
    angles = mc.get_angles()
    if not angles or len(angles) < 6:
        print("错误：获取当前角度失败或数据不完整，为安全起见退出程序！")
        return

    print(f"读取到当前关节角度: {angles}")

    # 2. 计算目标角度：只动关节 6 (最末端，法兰盘那节)，偏移 10 度
    target_angles = angles.copy()
    target_angles[5] += 10.0  # Python列表索引从0开始，5对应第6关节

    # 简单限位保护：如果加10度后超过了安全范围（假设接近180度极限），则改为反向转10度
    if target_angles[5] > 170.0:
        target_angles[5] -= 20.0

    print(f"计划安全微动，目标角度: {target_angles}")
    print("准备开始移动... (速度已极度限制为 20，满速 100)")

    # 给予两秒反应时间
    for i in range(2, 0, -1):
        print(f"{i} 秒后开始移动...")
        time.sleep(1)

    # 3. 下发运动指令
    mc.send_angles(target_angles, 20)
    print("指令已下发，观察最末端法兰盘是否缓慢旋转了约10度。")

    # 等待其走到位
    time.sleep(3)

    # 4. 恢复原位
    print(f"准备恢复原始角度: {angles}")
    time.sleep(1)
    mc.send_angles(angles, 20)
    print("恢复指令已下发，等待归位...")
    time.sleep(3)

    print("安全运动测试圆满结束！")

if __name__ == "__main__":
    main()
