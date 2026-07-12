import sys
import os
import time
import math
import argparse

# 保证能加载同目录的 teach_replay_pick_return_ready 模块
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from teach_replay_pick_return_ready import (
    MyCobot, load_preset, get_port, prepare_phase, safe_return_home,
    ANG_REPLAY_SPEED, ANG_REPLAY_TIMEOUT, checked_sync_angles
)

def main():
    parser = argparse.ArgumentParser(description="J4 轴高空 pick_hover 单变量偏置测试工具")
    parser.add_argument("--port", default=None, help="指定串口（如 COM9），不填则自动检测")
    parser.add_argument("--preset", default="20260712_180deg", help="要测试的角度预设名")
    parser.add_argument("--j4-offset", type=float, default=0.0, help="J4 关节角的偏置量(deg)")
    args = parser.parse_args()

    PORT = args.port if args.port else get_port()
    BAUD = 1000000

    print(f"尝试连接机械臂 ({PORT} @ {BAUD})...")
    mc = MyCobot(PORT, BAUD)
    time.sleep(0.5)

    # 加载预设
    try:
        pick_hover, pick, drop_hover, drop, home_ready, data = load_preset(args.preset)
    except Exception as e:
        print(f"【错误】加载预设失败: {e}")
        sys.exit(1)

    print("\n====================================")
    print(f"【J4 单变量偏置测试：--j4-offset = {args.j4_offset:.2f}°】")
    print("====================================")

    # 1. 恢复供电回零
    if not prepare_phase(mc):
        print("【错误】第二阶段回零失败。")
        sys.exit(1)

    # 2. 计算偏置后的 pick_hover 关节角
    original_angles = list(pick_hover["angles"])
    biased_angles = list(original_angles)
    biased_angles[3] += args.j4_offset  # 对第 4 轴 (J4，下标3) 增加偏置
    target_coords = pick_hover["coords"]

    print(f"\n[偏置信息]")
    print(f"  原 `pick_hover` 角度: {original_angles}")
    print(f"  偏置后 `pick_hover` 角度: {biased_angles}")
    print(f"  期望空间物理坐标: {target_coords}")

    # 3. 回放到偏置角度（ignore_error_exit=True，以便返回 0 后仍记录数据）
    print("\n关节回放移动中...")
    checked_sync_angles(mc, biased_angles, ANG_REPLAY_SPEED, ANG_REPLAY_TIMEOUT, "pick_hover_biased", expected_coords=target_coords, ignore_error_exit=True)

    # 4. 终态数据采集与对比
    actual_angles = mc.get_angles()
    actual_coords = mc.get_coords()

    has_angles = isinstance(actual_angles, list) and len(actual_angles) == len(biased_angles)
    has_coords = isinstance(actual_coords, list) and len(actual_coords) >= 3

    print(f"\n[偏置最终状态对比]")
    print(f"  目标角度: {biased_angles}")
    print(f"  实际角度: {actual_angles if has_angles else '读取失败'}")
    print(f"  目标坐标(期望值): {target_coords}")
    print(f"  实际坐标: {actual_coords if has_coords else '读取失败'}")

    if has_angles:
        diffs = [abs(a - t) for a, t in zip(actual_angles, biased_angles)]
        print(f"  偏置后关节绝对差: {['%.2f' % d for d in diffs]}，最大差: {max(diffs):.2f}°")
    if has_coords:
        dist = math.sqrt(sum((actual_coords[i] - target_coords[i]) ** 2 for i in range(3)))
        print(f"  空间物理残差(与期望目标的欧氏距离): {dist:.2f} mm")
        print(f"  物理各轴偏差 (X/Y/Z): dX={actual_coords[0]-target_coords[0]:.1f} mm, dY={actual_coords[1]-target_coords[1]:.1f} mm, dZ={actual_coords[2]-target_coords[2]:.1f} mm")

    # 5. 安全回零
    print("\n  -> 实验结束，正在安全回零...")
    res_home = safe_return_home(mc)
    if res_home == "auto":
        print("  -> 机械臂已自动安全回零，到达直立终态。")
    elif res_home == "manual":
        print("  -> 机械臂在人工扶正辅助下完成回零，到达直立终态。")
    else:
        print("  ⚠️ [错误] 机械臂回零失败！")

    print(f"\n✅ [--j4-offset={args.j4_offset:.2f}°] 实验运行完成 (回零状态: {res_home})。")

if __name__ == "__main__":
    main()
