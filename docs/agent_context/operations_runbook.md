# Agent 操作 runbook

## 恢复与只读检查

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\agent_handoff_health_check.ps1
git status --short --branch
powershell -NoProfile -ExecutionPolicy Bypass -File tools\project_freshness_check.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\agent_context_budget.ps1
```

## 单摄安全 Gate

先从 `CURRENT_STATE.md` 指定的当前 SHA 冷构建并形成 Map/PNR/STA/CDC、warning、bitstream/ELF hash 证据。随后只允许匹配 bitstream + `USER2` + `0xF9000000` 片上 RAM + UART0 115200 Hello。禁止 USER1、Flash、DDR、UART2/J52、机械臂接线或动作。

## myCobot 只读环境检查

```powershell
python -c "import serial.tools.list_ports as p; [print(x.device,x.description,x.hwid) for x in p.comports()]"
```

任何动作命令均不属于本 runbook；必须另立 Review Packet 并由用户确认安全条件。
