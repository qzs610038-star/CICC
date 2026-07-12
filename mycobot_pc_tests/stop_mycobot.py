import sys
import time
import argparse
import serial.tools.list_ports
from pymycobot.mycobot import MyCobot

def get_port():
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

def main():
    parser = argparse.ArgumentParser(description="myCobot 280 急停/释放所有关节扭矩工具")
    parser.add_argument("--port", default=None, help="指定串口（如 COM10），不填则自动检测")
    args = parser.parse_args()

    PORT = args.port if args.port else get_port()
    BAUD = 1000000

    print(f"【急停/释放脚本】正在连接 {PORT} @ {BAUD}...")
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
