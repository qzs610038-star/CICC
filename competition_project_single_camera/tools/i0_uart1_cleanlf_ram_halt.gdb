# I0 UART1 clean-LF RAM-only load, halt, and PC gate.
# Run only after the approved volatile FPGA configuration and approved USER2
# OpenOCD session. This script has no Flash, SPI, PROM, DDR, UART0, or APB use.

set pagination off
set confirm off
file C:/cicc_i0_uart1_design_lf_20260719_v4/competition_project_single_camera/embedded_sw/uart1_hello_onchip/build/uart1_hello_onchip.elf
target extended-remote :3333
monitor halt
load
monitor halt
printf "I0_PC="
p/x $pc
if (($pc < 0xF9000000) || ($pc > 0xF9003FFF))
  printf "I0_PC_RANGE_FAIL\n"
  quit
end
printf "I0_PC_RANGE_PASS\n"
printf "I0_RESUME_CONTROLLED=AFTER_PC_RANGE_PASS\n"
continue
printf "I0_RESUME_STOPPED=YES\n"
