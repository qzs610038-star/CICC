# Session Handoff - [任务简短名称]

- 交接发起时间: YYYY-MM-DD HH:MM:SS
- 前序会话完成度: [xx]%
- 当前分支/commit/dirty: [main / <sha> / clean]
- 仓库根（声明，仅作来源追溯）: <current_workspace_root>
- 烧录树（声明，如有，仅作来源追溯）: <current_burn_tree_or_empty>
- 路径规范：仓库内路径必须相对项目根，如 `final_project/...`；绝对路径仅作本机来源记录，不得作为接力执行路径。

## [Verified Facts] 高置信接力事实区

> 默认接受，禁止重型重复验证。每条必须带证据路径与时间戳。

1. [事实] — 证据：`path:line` / git <sha> / 日志 `path` — 时间：YYYY-MM-DD HH:MM
2. ...

## [Evidence Index] 证据索引

- 构建日志：
- 仿真日志：
- 上板截图/录像：
- git commit：
- 关键文件路径：

## [Health Check] 接力健康检查

- 脚本：`tools/agent_handoff_health_check.ps1`
- 运行结果：pass / fail / skipped
- 失败项（如有）：
- 警告项（如有）：

## [Contradiction Report] 矛盾与纠偏记录

> 仅在健康检查失败或事实冲突时填写。只针对冲突事实做最小范围纠偏。

- 冲突项：
- 纠偏范围：
- 建议动作：

## [Next Immediate Action] 下一步立即动作

- [ ] 第一步：... （checkpoint：...）
- [ ] 第二步：...
