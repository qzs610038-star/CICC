# I0 UART1 clean-LF APB read-only final checkpoint.
# Execute only after recorded UART1 Hello and one-byte echo PASS.
# Exactly one CPU-halted read is permitted: 0xE8100000.

set pagination off
set confirm off
target extended-remote :3333
monitor halt
printf "I0_APB_ADDRESS=0xE8100000\n"
x/wx 0xE8100000
printf "I0_APB_EXPECTED=0x375A0001\n"
