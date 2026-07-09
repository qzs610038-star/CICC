---
name: learning_guide_generator
description: 每次团队合并个人分支后，根据 Git 差分和 Handoff 记录生成 A/B/C 三轨“15分钟核心经验”讲解指南，遵循生动讲解与严谨分析并存的行文风格。
---

# 团队分支合并讲解指南生成技能 (learning_guide_generator)

本技能供 Agent 在接收到分支合并生成请求（如“我刚刚完成了分支合并 dev_cpu，请为我生成讲解指南”）时使用。

## 1. 触发匹配与数据收集

当被用户唤醒时，Agent 必须执行以下步骤收集变更上下文：
1. **定位合并分支**：获取用户指定的 `[分支名]`。
2. **运行代码差分**：运行 `git diff origin/main HEAD` 或在本地检索对应的 Git 变更记录（若用户运行了 `tools/gen_guides_helper.ps1` 提取了差分，则直接去 `learning_guides/daily_sync/YYYY-MM-DD_merge_[分支名]/` 读取差分补丁和修改清单）。
3. **读取最新 Handoff 事实**：读取 `CURRENT_STATE.md`、`SESSION_HANDOFF.md` 或 `debug_records/` 下最近一版的交接事实，确保业务理解与最新的 Codex 评审结果绝对对齐。

---

## 2. 风格与术语约束

生成的三份讲解指南文件必须无条件遵守 [STYLE_GUIDE.md](file:///d:/第十届集创赛-雄芯院材料/learning_guides/STYLE_GUIDE.md) 中关于风格和关键术语的约束：

- **三轨独立文件输出**：
  - [A: FPGA与视频前端]：[guide_fpga_vision_front.md](file:///d:/第十届集创赛-雄芯院材料/learning_guides/daily_sync/2026-07-08_merge_dev_pc_arm_v2.12/guide_fpga_vision_front.md)
  - [B: 嵌入式CPU与通信控制]：[guide_cpu_arm_control.md](file:///d:/第十届集创赛-雄芯院材料/learning_guides/daily_sync/2026-07-08_merge_dev_pc_arm_v2.12/guide_cpu_arm_control.md)
  - [C: 机械臂控制调试与项目系统维护更新]：[guide_system_integration.md](file:///d:/第十届集创赛-雄芯院材料/learning_guides/daily_sync/2026-07-08_merge_dev_pc_arm_v2.12/guide_system_integration.md)
- **生动讲解原则**：对复杂逻辑进行生动场景化比喻（如刹车晃动、安检解耦、电磁锁紧急释放），不得进行干瘪的纯代码罗列。
- **严谨分析原则**：使用 `file:///` 跳转链接指向真实更改文件。对未测试的分支，严格标注 **“代码审查通过、运行时未触发”** 的如实风险。
- **自学知识划分**：自学板块必须分为“优先赛方资料”和“拓展基础知识”两部分，并准确给出指引。

---

## 3. 生成与归档步骤

1. **新建子目录**：在 `learning_guides/daily_sync/` 下创建 `YYYY-MM-DD_merge_[分支名]/` 子目录。
2. **套用模板实例化**：套用 [daily_guide_template.md](file:///d:/第十届集创赛-雄芯院材料/learning_guides/daily_guide_template.md) 模板，结合今日改动，撰写并输出上述 3 份文档。
3. **更新索引**：向 [README.md](file:///d:/第十届集创赛-雄芯院材料/learning_guides/README.md) 中追加新生成的子目录和指南索引。
4. **输出确认**：向用户汇报生成状态，指出生成子目录的相对路径，并提供一句话的生动比喻摘要。
