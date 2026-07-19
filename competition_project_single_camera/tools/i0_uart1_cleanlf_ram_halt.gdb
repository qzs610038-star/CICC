# Static I0 UART1 Hello RAM-load and PC gate. Hardware execution remains unauthorized.
# The capture process must write CAPTURE_READY before the one permitted continue.
set pagination off
set confirm off
file competition_project_single_camera/embedded_sw/uart1_hello_onchip/build/uart1_hello_onchip.elf
target extended-remote :3333
monitor halt
load
monitor halt
printf "I0_POST_LOAD_HALT_PC="
p/x $pc
if (($pc < 0xF9000000) || ($pc > 0xF9003FFF))
  printf "I0_PC_RANGE_FAIL\\n"
  quit
end
printf "I0_PC_RANGE_PASS\\n"
printf "I0_CAPTURE_REQUIRED_BEFORE_RESUME=CAPTURE_READY\\n"
printf "I0_RESUME_COUNT_LIMIT=1\\n"
continue
printf "I0_RESUME_RETURNED=YES\\n"
