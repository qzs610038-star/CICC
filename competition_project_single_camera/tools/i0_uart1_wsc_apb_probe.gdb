# I0 final APB checkpoint. This script consumes the hash-locked WSC probe
# contract, never reads the APB window from the debugger, and reads only WSC's
# four fixed on-chip RAM evidence words after its deterministic breakpoint.
# Run only after the WSC binding gate passes and UART1 Hello/echo is recorded.
# The operator must enforce the WSC 1000 ms timeout; on timeout halt once,
# record PC/reason, and do not read RAM symbols, resume, step, or retry.

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
  printf "WSC_PROBE_ENTRY_PC_FAIL\n"
  quit
end
printf "WSC_PROBE_ENTRY_PC_PASS\n"
break *0xF90000C4
printf "WSC_PROBE_RESUME_ONCE_TIMEOUT_MS=1000\n"
continue
printf "WSC_PROBE_HALT_PC="
p/x $pc
if ($pc != 0xF90000C4)
  printf "WSC_PROBE_HALT_PC_FAIL\n"
  quit
end
printf "WSC_PROBE_HALT_PC_PASS\n"
set $wsc_expected = *(unsigned int *)0xF90000D0
set $wsc_address = *(unsigned int *)0xF90000D4
set $wsc_status = *(unsigned int *)0xF90000E4
set $wsc_observed = *(unsigned int *)0xF90000E8
printf "WSC_RAM_g_apb_probe_expected=0x%08x\n", $wsc_expected
printf "WSC_RAM_g_apb_probe_address=0x%08x\n", $wsc_address
printf "WSC_RAM_g_apb_probe_status=0x%08x\n", $wsc_status
printf "WSC_RAM_g_apb_probe_observed=0x%08x\n", $wsc_observed
if (($wsc_expected != 0x375A0001) || ($wsc_address != 0xE8100000) || ($wsc_status != 0x50415353) || ($wsc_observed != 0x375A0001))
  printf "WSC_PROBE_RESULT_FAIL\n"
  quit
end
printf "WSC_PROBE_RESULT_PASS\n"
