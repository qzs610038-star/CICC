# Static WSC APB probe consumer. The CPU, never the debugger, reads 0xE8100000.
# Run only after the WSC gate, UART capture success, and a separately approved board gate.
set pagination off
set confirm off
file competition_project_single_camera/embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.elf
target extended-remote :3333
monitor halt
load
monitor halt
printf "WSC_PROBE_ENTRY_PC="
p/x $pc
if ($pc != 0xF9000000)
  printf "WSC_PROBE_ENTRY_PC_FAIL\\n"
  quit
end
break *0xF90000C4
printf "WSC_PROBE_RESUME_ONCE_TIMEOUT_MS=1000\\n"
continue
printf "WSC_PROBE_HALT_PC="
p/x $pc
if ($pc != 0xF90000C4)
  printf "WSC_PROBE_HALT_PC_FAIL RAM_READ_COUNT=0\\n"
  quit
end
set $wsc_expected = *(unsigned int *)0xF90000D0
set $wsc_address = *(unsigned int *)0xF90000D4
set $wsc_status = *(unsigned int *)0xF90000E4
set $wsc_observed = *(unsigned int *)0xF90000E8
printf "WSC_RAM_READ_COUNT=4 expected=0x%08x address=0x%08x status=0x%08x observed=0x%08x\\n", $wsc_expected, $wsc_address, $wsc_status, $wsc_observed
if (($wsc_expected != 0x375A0001) || ($wsc_address != 0xE8100000) || ($wsc_status != 0x50415353) || ($wsc_observed != 0x375A0001))
  printf "WSC_PROBE_RESULT_FAIL\\n"
  quit
end
printf "WSC_PROBE_RESULT_PASS\\n"
