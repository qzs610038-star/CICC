"""Read only a myCobot 280 Atom system version over an explicitly named COM port.

This script intentionally contains no motion, gripper, servo, power, updater, or
firmware-flash API calls.  It is a Stage-0 evidence collector only and is not
part of the FPGA-board control path.
"""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

import pymycobot
from pymycobot import MyCobot280
from serial.tools import list_ports


def port_metadata(port_name: str) -> dict[str, object]:
    for port in list_ports.comports():
        if port.device.upper() == port_name.upper():
            return {
                "device": port.device,
                "description": port.description,
                "hwid": port.hwid,
                "vid": port.vid,
                "pid": port.pid,
                "serial_number": port.serial_number,
            }
    return {"device": port_name, "enumerated": False}


def run(port_name: str, baudrate: int, timeout: float) -> dict[str, object]:
    result: dict[str, object] = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "mode": "STAGE0_READ_ONLY",
        "python": sys.executable,
        "pymycobot_version": getattr(pymycobot, "__version__", "unknown"),
        "port": port_metadata(port_name),
        "baudrate": baudrate,
        "timeout_seconds": timeout,
        "permitted_api": "MyCobot280.get_system_version",
        "motion_or_firmware_api_called": False,
    }
    robot = None
    try:
        robot = MyCobot280(port_name, baudrate=baudrate, timeout=timeout)
        result["system_version"] = robot.get_system_version()
        result["query_status"] = "OK"
    except Exception as exc:  # Evidence must retain an unavailable response.
        result["query_status"] = "NOT_AVAILABLE"
        result["error_type"] = type(exc).__name__
        result["error"] = str(exc)
    finally:
        serial_port = getattr(robot, "_serial_port", None)
        if serial_port is not None:
            serial_port.close()
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True,
                        help="Manually confirmed myCobot COM port; no automatic selection.")
    parser.add_argument("--baudrate", type=int, default=1_000_000)
    parser.add_argument("--timeout", type=float, default=0.75)
    parser.add_argument("--output", type=Path,
                        help="Optional JSON evidence path; otherwise prints JSON to stdout.")
    args = parser.parse_args()

    result = run(args.port, args.baudrate, args.timeout)
    rendered = json.dumps(result, ensure_ascii=False, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered + "\n", encoding="utf-8")
    print(rendered)
    return 0 if result["query_status"] == "OK" else 2


if __name__ == "__main__":
    raise SystemExit(main())
