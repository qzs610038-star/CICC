# FPGA 阶段交接

用于阶段结束、切换 Agent、任务暂停或用户要求保留上下文时。遵循全局 handoff 协议：写入 `~/.agents/handoff/{agent}-handoff-{YYYYMMDD_HHMMSS}.json`，并向 `~/.agents/shared/today-summary.md` 追加一条短摘要。

## JSON 模板
```json
{
  "version": "1.0",
  "agent": "cc",
  "model": "opus|sonnet|haiku|deepseek|other",
  "project": "D:\\第十届集创赛-雄芯院材料",
  "session_id": "session_xxx",
  "session_end": "ISO-8601 timestamp",
  "mode": "architecture-plan|execution|review-prep|debug",
  "completed_tasks": [],
  "key_decisions": [],
  "architecture_boundary": "FPGA video front-end/ROI/statistics/OSD; board CPU vision decision/parameters/myCobot control; no pure-FPGA vision or pure-FPGA arm control as main route",
  "modified_files": [],
  "verification": [
    {
      "command": "",
      "cwd": "",
      "result": "",
      "log": ""
    }
  ],
  "open_items": [],
  "codex_review_needed": false,
  "codex_review_packet": "",
  "shared_update": {
    "tasks_read": false,
    "summary_appended": false,
    "handoff_written": false
  }
}
```

## 摘要格式
```md
- YYYY-MM-DD HH:mm | 第十届集创赛-雄芯院材料 | agent=cc | mode=... | done=... | next=... | codex_review_needed=true|false
```
