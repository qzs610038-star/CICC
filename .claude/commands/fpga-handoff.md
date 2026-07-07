# FPGA 阶段交接

用于阶段结束、切换 Agent、任务暂停或用户要求保留上下文时。遵循全局 handoff 协议：写入 `~/.agents/handoff/{agent}-handoff-{YYYYMMDD_HHMMSS}.json`，并向 `~/.agents/shared/today-summary.md` 追加一条短摘要。

本项目交接采用“高置信接力 + 只读健康握手 + 明确纠偏出口”协议。`verified_facts` 默认被接力 Agent 接受，但不得绕过 `tools/agent_handoff_health_check.ps1` 的只读检查；若检查失败或事实冲突，进入 `contradiction_report`，只针对冲突事实做最小范围纠偏。

## JSON 模板

```jsonc
{
  "version": "1.1",
  "agent": "cc",
  "model": "opus|sonnet|haiku|deepseek|other",
  "project": "第十届集创赛-雄芯院材料",
  "session_id": "session_xxx",
  "session_end": "ISO-8601 timestamp",
  "mode": "architecture-plan|execution|review-prep|debug",

  "repo_root_declared": "<current_workspace_root_resolved_at_handoff_generation>",
  "burn_tree_declared": "<optional_current_burn_tree_or_empty>",
  "path_policy": {
    "repo_internal_paths": "relative_to_repo_root",
    "absolute_paths": "provenance_only_not_execution_target"
  },
  "head_declared": {
    "branch": "main",
    "commit": "<sha>",
    "dirty": false
  },

  "cbm_status": {
    "project": "D-cicc_cbm_link",
    "checked": false,
    "queries": []
  },
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

  "verified_facts": [
    {
      "fact": "Example: top-level HDMI input path now selects the intended camera stream",
      "evidence": "final_project/fpga/rtl/top/top.v:<line>; git <sha>",
      "timestamp": "YYYY-MM-DDTHH:mm:ss"
    }
  ],
  "evidence_index": [
    {
      "kind": "build_log|sim_log|board_photo|git_commit|file_path",
      "path": "relative/path/from/repo/root",
      "note": ""
    }
  ],
  "health_check": {
    "script": "tools/agent_handoff_health_check.ps1",
    "ran": false,
    "result": "pass|fail|skipped",
    "fail_items": [],
    "warn_items": []
  },
  "contradiction_report": [
    {
      "conflict": "",
      "scope": "minimal",
      "resolution": ""
    }
  ],

  "open_items": [],
  "next_immediate_action": [
    {
      "step": 1,
      "action": "",
      "checkpoint": ""
    }
  ],
  "codex_review_needed": false,
  "codex_review_packet": "",
  "shared_update": {
    "tasks_read": false,
    "summary_appended": false,
    "handoff_written": false
  }
}
```

## 强制要求

- `repo_root_declared`、`burn_tree_declared`、`head_declared` 必填，但不得在模板中硬编码某一位队友的本机目录。
- 生成 handoff 时由脚本动态写入当前工作区根路径；仓库内部证据路径必须统一写成相对项目根的路径，例如 `final_project/...`。
- 绝对路径只用于来源追溯，不能作为接力 Agent 的执行目标。
- `verified_facts` 每条必须带 `evidence` 和 `timestamp`；缺失则触发 `contradiction_report`。
- 机械臂相关事实不得包含任何未确认安全条件下的动作建议；交接恢复时只允许先做只读状态检查。

## 摘要格式

```md
- YYYY-MM-DD HH:mm | 第十届集创赛-雄芯院材料 | agent=cc | mode=... | done=... | next=... | codex_review_needed=true|false
```
