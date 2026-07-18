# R0 UART0 CH340 Passive Listen Evidence

Date: 2026-07-18 (Asia/Shanghai)

Decision: `UART0 HELLO NOT OBSERVED / R0 REMAINS BLOCKED`

## Confirmed Physical Route

The user confirmed during this session that UART0 reaches the PC through a USB-to-TTL CH340 adapter. Therefore only the two currently enumerated CH340 ports are valid UART0 candidates for this evidence:

| Port | USB identity | Windows location |
|---|---|---|
| COM12 | VID:PID `1A86:7523` | `1-9` |
| COM17 | VID:PID `1A86:7523` | `1-4.1` |

The earlier passive COM10/COM13 check in this session targeted FTDI channels and is excluded from the UART0 conclusion. It transmitted zero bytes and made no hardware state change.

## Passive Capture Parameters

- `115200` baud, 8 data bits, no parity, 1 stop bit.
- No software or hardware flow control.
- DTR and RTS disabled.
- UART transmit byte count: `0`.
- No Programmer, OpenOCD, GDB, USER1/USER2, Flash, external DDR, UART2/J52, or myCobot action was started in this session.

## Results

| Window | COM12 | COM17 |
|---|---:|---:|
| 2026-07-18 17:37:54 to 17:38:24 | opened, 0 RX bytes | opened, 0 RX bytes |
| 2026-07-18 17:39:10 to 17:40:10 | opened, 0 RX bytes | opened, 0 RX bytes |

No Hello text, partial text, or undecodable bytes were received. The expected banner remains:

```text
TJ375 CPU+VIDEO UART0 HELLO
ONCHIP_RAM=0xF9000000 UART0=115200 8N1
Type characters to verify echo.
```

## Interpretation And Boundary

This evidence proves only that neither enumerated CH340 port delivered UART bytes during the two capture windows. It does not independently prove that a CPU/SoC reset occurred inside either window. It also does not distinguish among CH340 port selection, TX/RX crossover, common-ground continuity, the `GPIOR_145/E10` physical route, or UART0 TX generation.

R0 remains blocked at the UART0 banner. Keep R1-R5, UART transmit/echo, USER1, Flash, external DDR, UART2/J52, and myCobot out of scope. The next check should use one operator-confirmed CH340 port and capture while a CPU/SoC reset is explicitly timestamped; if RX remains at zero bytes, inspect TX/RX/GND continuity and the E10-to-CH340 RX route before changing RTL or firmware.
