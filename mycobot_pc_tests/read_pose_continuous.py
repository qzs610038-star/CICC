import time
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
    print("Continuous Pose Reading (10 samples, 1s interval):")
    print("========================================")

    for i in range(10):
        angles = None
        coords = None

        # Read angles
        try:
            angles = mc.get_angles()
        except Exception as e:
            angles = f"Error: {e}"

        # Read coords
        try:
            coords = mc.get_coords()
        except Exception as e:
            coords = f"Error: {e}"

        print(f"Sample {i+1}:")
        print(f"  angles = {angles}")
        print(f"  coords = {coords}")
        print("-" * 40)
        time.sleep(1.0)

    print("Reading complete.")

if __name__ == "__main__":
    main()
