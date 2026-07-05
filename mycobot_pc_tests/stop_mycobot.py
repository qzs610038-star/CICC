import time
from pymycobot.mycobot import MyCobot

PORT = 'COM9'
BAUD = 1000000

def main():
    print("【急停/释放脚本】正在连接...")
    try:
        mc = MyCobot(PORT, BAUD)
        # 给通信一点点缓冲时间
        time.sleep(0.5)

        print("发送释放所有关节指令...")
        # 调用 release_all_servos() 会切断各关节舵机扭矩
        mc.release_all_servos()

        print("====================")
        print("所有关节已放松！电机已掉电。")
        print("注意：如果机械臂之前悬空，现在会因为重力变软下垂，请注意扶住！")
        print("====================")
    except Exception as e:
        print(f"执行急停失败: {e}")

if __name__ == "__main__":
    main()
