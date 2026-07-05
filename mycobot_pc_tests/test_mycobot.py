import time
from pymycobot.mycobot import MyCobot

# 填写实际识别到的 COM 端口，通常是 Silicon Labs CP210x USB to UART Bridge
# 例如 'COM4', 'COM5'
PORT = 'COM9'
BAUD = 1000000

def main():
    if PORT == 'COMX':
        print("错误: 请先将代码中的 PORT 修改为你实际连接机械臂的串口号 (例如 'COM3')")
        return

    print(f"尝试连接机械臂 ({PORT} @ {BAUD})...")
    try:
        mc = MyCobot(PORT, BAUD)
    except Exception as e:
        print(f"连接失败: {e}")
        print("请检查：1. 机械臂是否通电 2. USB线是否连接到电脑 3. 串口号是否正确")
        return

    print("====================")
    print("连接成功！当前为只读与安全演示模式。")
    print("====================")

    # 1. 尝试读取机械臂各关节角度
    try:
        angles = mc.get_angles()
        print(f"[状态] 当前关节角度: {angles}")
    except Exception as e:
        print(f"读取关节角度失败: {e}")

    # 2. 尝试读取机械臂坐标
    try:
        coords = mc.get_coords()
        print(f"[状态] 当前三维坐标 (X,Y,Z,Rx,Ry,Rz): {coords}")
    except Exception as e:
        print(f"读取坐标失败: {e}")

    # 3. 极小幅安全演示：点亮底座/顶端 RGB 灯板（视机械臂硬件而定）
    print("\n[演示] 下面将尝试改变机械臂灯板颜色...")
    try:
        mc.set_color(255, 0, 0)
        print(" -> 红色 (Red)")
        time.sleep(1.5)

        mc.set_color(0, 255, 0)
        print(" -> 绿色 (Green)")
        time.sleep(1.5)

        mc.set_color(0, 0, 255)
        print(" -> 蓝色 (Blue)")
        time.sleep(1.5)

        # 恢复默认/白光
        mc.set_color(255, 255, 255)
    except Exception as e:
        print(f"控制灯板失败: {e}")

    print("\n====================")
    print("读取演示完毕。")
    print("注：在未固定机械臂、未确认安全活动范围前，请勿通过脚本执行任意移动指令！")
    print("====================")

if __name__ == "__main__":
    main()
