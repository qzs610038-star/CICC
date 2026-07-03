"""
fpga_robot_mcp.board_tools — 开发板 UART/JTAG/接口契约工具。

实现:
- list_uart_candidates() — 枚举开发板 UART 候选项
- check_jtag_chain() — 探测 JTAG 链
- uart_loopback_test() — UART 回环测试（低风险硬件操作）
- generate_uart_test_plan() — 生成 UART 桥接测试计划文档
"""

from __future__ import annotations

import datetime
import os
import subprocess
import time
from typing import Any

from fpga_robot_mcp import serial_probe
from fpga_robot_mcp.config import load_config


def list_uart_candidates() -> dict:
    """
    枚举开发板 UART 候选项。

    过滤掉蓝牙端口，按类型分组列出。
    """
    try:
        ports = serial_probe.list_ports()
    except NotImplementedError:
        return {"status": "not_implemented", "message": "serial_probe 未实现，无法检测串口"}

    # 过滤蓝牙
    board_ports = [p for p in ports if p.get("type") != "bluetooth" and p.get("type") != "error"]
    # 蓝牙端口（用于区分显示）
    bluetooth_ports = [p for p in ports if p.get("type") == "bluetooth"]

    # 按类型分组
    by_type: dict[str, list[dict]] = {}
    for p in ports:
        t = p.get("type", "unknown")
        by_type.setdefault(t, []).append(p)

    return {
        "status": "ok",
        "data": {
            "all_ports": ports,
            "board_candidates": board_ports,
            "bluetooth_ports": bluetooth_ports,
            "by_type": {k: len(v) for k, v in by_type.items()},
            "total": len(ports),
            "board_uart_count": len(board_ports),
        },
    }


def check_jtag_chain() -> dict:
    """
    探测 JTAG 链上的设备。

    通过 efx_pgm --scan 扫描 JTAG 链。
    本工具只读，不烧录。
    """
    cfg = load_config()
    efx_pgm = cfg.efinity_bin_path() / "efx_pgm.exe"

    if not efx_pgm.is_file():
        return {
            "status": "no_hardware",
            "data": {
                "efx_pgm_exists": False,
                "message": "efx_pgm 未安装或路径不正确",
                "next_step": "请确认 Efinity 安装路径 (D:\\Efinity\\2025.2\\bin\\) 和 FT4232 驱动",
            },
        }

    # 先检查文件存在性
    checks = {
        "efx_pgm_path": str(efx_pgm),
        "efx_pgm_exists": True,
        "efx_pgm_size_kb": efx_pgm.stat().st_size // 1024,
    }

    # 尝试扫描 JTAG 链
    try:
        result = subprocess.run(
            [str(efx_pgm), "--scan"],
            capture_output=True,
            timeout=15,
            encoding="utf-8",
            errors="replace",
        )
        stdout = result.stdout or ""
        stderr = result.stderr or ""
        raw_output = (stdout + "\n" + stderr).strip()

        # 解析输出 - 查找 JTAG 设备信息
        devices = []
        for line in stdout.splitlines():
            line_lower = line.lower()
            if "idcode" in line_lower or "device" in line_lower or "jtag" in line_lower:
                devices.append({"raw": line.strip()})
            # 也尝试查找"Found"或"Detected"模式
            if "found" in line_lower or "detected" in line_lower:
                devices.append({"raw": line.strip()})

        jtag_found = result.returncode == 0

        return {
            "status": "ok" if jtag_found else "scan_failed",
            "data": {
                **checks,
                "scan_successful": jtag_found,
                "return_code": result.returncode,
                "jtag_devices": devices if devices else [],
                "raw_output": raw_output[:500] if raw_output else "",
                "message": f"JTAG 链扫描{'成功' if jtag_found else '失败'}"
                           f"{'，发现 ' + str(len(devices)) + ' 个设备' if devices else ''}",
            },
        }
    except subprocess.TimeoutExpired:
        return {
            "status": "timeout",
            "data": {
                **checks,
                "message": "JTAG 扫描超时（15 秒），请检查 JTAG 下载线连接和 FT4232 驱动",
            },
        }
    except FileNotFoundError:
        return {
            "status": "error",
            "data": {
                **checks,
                "message": f"efx_pgm 路径不存在: {efx_pgm}",
            },
        }
    except Exception as e:
        return {
            "status": "error",
            "data": {
                **checks,
                "message": f"JTAG 扫描异常: {e}",
            },
        }


def uart_loopback_test(port: str, baudrate: int = 115200) -> dict:
    """
    在指定 UART 上执行回环测试帧。

    警告: 本工具需 safety 门控（HARDWARE_SIDE_EFFECT），但不连接机械臂。

    参数:
        port: COM 口名称
        baudrate: 波特率，默认 115200

    返回:
    {
        "port": port,
        "baudrate": baudrate,
        "test_passed": true,
        "bytes_sent": 32,
        "bytes_received": 32,
        "match": true,
        "duration_ms": 15.3,
    }
    """
    try:
        import serial
    except ImportError:
        return {"status": "error", "message": "pyserial 未安装", "data": {"port": port, "test_passed": False}}

    test_data = b"FPGA_ROBOT_MCP_LOOPBACK_TEST_\n"
    start_time = time.time()

    try:
        ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            timeout=2,
            write_timeout=2,
        )
    except Exception as e:
        return {
            "status": "error",
            "message": f"无法打开串口 {port}: {e}",
            "data": {"port": port, "baudrate": baudrate, "test_passed": False},
        }

    try:
        # 发送
        bytes_sent = ser.write(test_data)
        ser.flush()

        # 读取回环
        bytes_received = 0
        received_data = b""
        try:
            received_data = ser.read(len(test_data))
            bytes_received = len(received_data)
        except Exception:
            pass

        duration_ms = round((time.time() - start_time) * 1000, 1)
        match = received_data == test_data
        test_passed = match and bytes_sent == bytes_received

        return {
            "status": "ok",
            "data": {
                "port": port,
                "baudrate": baudrate,
                "test_passed": test_passed,
                "bytes_sent": bytes_sent,
                "bytes_received": bytes_received,
                "match": match,
                "duration_ms": duration_ms,
                "message": "回环测试通过" if test_passed else (
                    "回环测试未通过 — 可能未连接回环线 (TX→RX)"
                    if bytes_received == 0 else
                    f"数据不匹配：发送 {bytes_sent} 字节，收到 {bytes_received} 字节"
                ),
            },
        }
    except Exception as e:
        duration_ms = round((time.time() - start_time) * 1000, 1)
        return {
            "status": "error",
            "message": f"回环测试异常: {e}",
            "data": {"port": port, "baudrate": baudrate, "test_passed": False, "duration_ms": duration_ms},
        }
    finally:
        try:
            ser.close()
        except Exception:
            pass


def generate_uart_test_plan() -> dict:
    """
    生成板上 CPU 到 myCobot 串口桥接测试计划（只生成文档，不操作硬件）。
    """
    now = datetime.datetime.now()

    markdown = f"""# 板上 CPU → myCobot 280 UART 桥接测试计划

**生成时间**: {now.strftime('%Y-%m-%d %H:%M:%S')}
**来源**: fpga_robot_mcp MCP 服务 board_tools.generate_uart_test_plan()

---

## 1. 测试目的

验证板上 CPU (QCRV32) 通过 UART 与 myCobot 280 之间的串口通信链路是否正常。

## 2. 硬件连接

| 项目 | 规格 |
|------|------|
| FPGA 开发板 | WZZY_FPGA / TJ375N529 |
| 板上 CPU | QCRV32 (RISC-V) |
| UART 通道 | UART2 TO Peripherals (J13/J15) |
| 波特率 | 1000000 (1Mbps) |
| 电平 | 3.3V CMOS (TTL) |
| 机械臂 | myCobot 280 |
| 接线 | TXD ↔ 机械臂 RX, RXD ↔ 机械臂 TX, GND ↔ 机械臂 GND |

> **注意**: 接线前需确认 FPGA UART 管脚电平为 3.3V，与 myCobot 280 控制器的电平兼容。

## 3. 测试步骤

### 阶段 A: 回环测试 (PC 端)

1. 在 PC 上用 USB-TTL (CP210x) 连接 FPGA UART2 的 TX/RX/GND
2. 短接 USB-TTL 的 TX-RX，执行回环测试
3. 用 MCP 工具 `board_uart_loopback_test` 验证通路
4. 移除短接线，连接 USB-TTL TX→FPGA RX, USB-TTL RX→FPGA TX
5. 用 PC 串口终端发送已知帧，FPGA 应回显

### 阶段 B: CPU 固件发送测试

1. 烧录含 UART 发送测试的 CPU 固件
2. CPU 通过 UART2 周期性发送握手帧 (如 "ARM_OK\\n")
3. PC USB-TTL 接收端应稳定收到该帧
4. 检查帧间隔、帧格式和波特率误差

### 阶段 C: CPU ↔ myCobot 联动测试

1. 确认 myCobot 280 已正确安装和供电
2. 确认机械臂处于正确初始姿态
3. 连接 FPGA UART2 TX/RX/GND 到 myCobot 控制器串口
4. 烧录含 "读取机械臂版本号" 的 CPU 固件
5. 验证 CPU 能收到 myCobot 返回的版本响应
6. 验证 CPU 能解析响应并更新状态寄存器

### 阶段 D: 带内控制测试 (已建立通信后)

1. CPU 发送 myCobot 协议角度读取命令
2. 确认返回角度值合理 (各关节在 0-180° 范围)
3. CPU 发送 RGB 灯板设置命令 (非运动)
4. 确认 myCobot 灯板颜色变化

## 4. 预期结果

| 阶段 | 预期 |
|------|------|
| A | PC 回环测试通过 (TX↔RX 短接) |
| B | CPU 固件发送的握手帧稳定可达 |
| C | CPU 可读取 myCobot 版本号 |
| D | CPU 可读取角度、设置 RGB (不涉及运动) |

## 5. 安全注意事项

- 通电前确保机械臂处于正确初始姿态
- 连接 FPGA UART 前用万用表确认电平为 3.3V
- 任何关节动作指令前必须完成阶段 C 的只读通信验证
- 首次动作测试速度上限不超过 30
- 随时准备按下急停按钮
- 未经 Codex 审查不得执行关节动作或夹爪控制
"""
    return {
        "status": "ok",
        "data": {
            "test_plan_markdown": markdown,
            "sections": ["回环测试", "CPU 固件发送", "CPU ↔ myCobot 联动", "带内控制"],
            "length_bytes": len(markdown),
        },
    }
