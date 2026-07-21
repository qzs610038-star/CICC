# Agent 操作 runbook

## 恢复与只读检查

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\agent_handoff_health_check.ps1
git status --short --branch
powershell -NoProfile -ExecutionPolicy Bypass -File tools\project_freshness_check.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\agent_context_budget.ps1
```

## 单摄安全 Gate

先从 `CURRENT_STATE.md` 指定的当前 SHA 原子生成 SoC UART1 并冷构建，形成 Map/PNR/STA/CDC、warning、bitstream/ELF hash 证据。固定同一输入 hash 后，可在一次批准窗口内连续执行匹配 bitstream + `USER2` + `0xF9000000` 片上 RAM + Type-C UART1 `115200 8N1` Hello/回显 + 只读 APB MAGIC。中间不重复确认；输入/hash/接线或失败现象变化时重开。禁止 USER1、Flash、DDR、UART2/J52、机械臂接线或动作。

## myCobot 只读环境检查

```powershell
python -c "import serial.tools.list_ports as p; [print(x.device,x.description,x.hwid) for x in p.comports()]"
```

任何动作命令均不属于本 runbook；必须另立 Review Packet 并由用户确认安全条件。
