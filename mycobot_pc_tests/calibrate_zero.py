import time
from pymycobot.mycobot import MyCobot

PORT = 'COM9'
BAUD = 1000000

def main():
    print("====================")
    print("【机械臂零点自动校准程序】")
    print("====================")
    print("警告：在继续之前，请务必【纯靠手掰】，将机械臂 1 到 6 个关节的【零位刻度线】全部对齐成直线！")
    print("对齐后，机械臂应呈现笔直朝上的姿态。")
    print("--------------------")
    confirm = input("您是否已经对齐刻度并用手扶稳了机械臂？(输入 y 继续，输入其他退出): ")

    if confirm.lower() != 'y':
        print("已取消校准。")
        return

    try:
        mc = MyCobot(PORT, BAUD)
        time.sleep(0.5)

        print("\n开始校准，请保持姿态不要晃动...")
        # 依次校准 1 到 6 关节 (官方 API: set_servo_calibration(servo_id))
        for i in range(1, 7):
            print(f"正在将关节 {i} 当前物理位置烧录为零点...")
            mc.set_servo_calibration(i)
            time.sleep(0.3)

        print("\n====================")
        print("✅ 所有关节零点校准完毕！")
        print("为了让底层的舵机芯片保存并刷新配置，请务必【断开机械臂电源，再重新通电】。")
        print("重启后，您可以运行 test_mycobot.py 检查读数是否归零。")
        print("====================")
    except Exception as e:
        print(f"\n❌ 校准失败: {e}")

if __name__ == "__main__":
    main()
