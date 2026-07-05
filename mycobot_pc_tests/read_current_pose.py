import time
import sys
from pymycobot.mycobot import MyCobot

PORT = 'COM10'
BAUD = 1000000

def main():
    print(f"Connecting to MyCobot on {PORT} @ {BAUD}...")
    try:
        mc = MyCobot(PORT, BAUD)
        time.sleep(0.5)
    except Exception as e:
        print(f"Connection failed: {e}")
        return

    print("========================================")
    print("Reading current pose (Read-Only):")

    # Read angles
    try:
        angles = mc.get_angles()
        print(f"get_angles() = {angles}")
    except Exception as e:
        print(f"Failed to read angles: {e}")

    # Read coords
    try:
        coords = mc.get_coords()
        print(f"get_coords() = {coords}")
    except Exception as e:
        print(f"Failed to read coords: {e}")

    print("========================================")

if __name__ == "__main__":
    main()
