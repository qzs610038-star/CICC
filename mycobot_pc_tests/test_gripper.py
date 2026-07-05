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

    print("====================")
    print("开始夹爪(Gripper)测试...")

    # 1. 尝试读取夹爪当前状态/开合度
    # 注意：部分夹爪硬件可能不支持直接读取当前值，因此添加异常处理
    try:
        gripper_val = mc.get_gripper_value()
        print(f"当前夹爪开合值: {gripper_val}")
    except Exception as e:
        print(f"读取夹爪值失败，可能当前硬件不支持状态反馈: {e}")

    print("--------------------")
    # 速度设置适中，满速为100
    speed = 50

    print("【测试 1】尝试完全张开夹爪 (State 0)...")
    # set_gripper_state(0, speed) -> 0通常代表张开
    mc.set_gripper_state(0, speed)
    time.sleep(3)  # 给夹爪留出动作时间

    print("【测试 2】尝试完全闭合夹爪 (State 1)...")
    # set_gripper_state(1, speed) -> 1通常代表闭合
    mc.set_gripper_state(1, speed)
    time.sleep(3)

    print("【测试 3】尝试设定夹爪到中间位置 (Value 50)...")
    # 部分自适应夹爪支持 0-100 的线性开合度控制
    try:
        mc.set_gripper_value(50, speed)
        time.sleep(3)
    except Exception as e:
        print(f"指定开合度控制命令失败: {e}")

    print("====================")
    print("夹爪测试指令发送完毕！")
    print("注：如果夹爪没有任何反应，请检查：")
    print("  1. 夹爪的连线是否已经稳固插在机械臂最末端的扩展接口上。")
    print("  2. myCobot 的固件版本是否支持该型号夹爪（平行夹爪/自适应夹爪）。")
    print("====================")

if __name__ == "__main__":
    main()
