# main 合并登记册

本表只登记实际已落入本地 `main` 的合并；候选审查、未合并分支和远端状态不计入。

| 日期 | 合并前 main SHA | 来源 | 纳入范围 | 舍弃范围 | 当前结论 | 记录 |
|---|---|---|---|---|---|---|
| 2026-07-17 | `24e3983` | `@libaoxun688` `14b9248` | Hard SoC 原子源码、APB0 `REG_MAGIC` | 未带原始证据的状态/板级文档 | 源码已纳入；重建及全部硬件 Gate 待验证 | [记录](records/2026-07-17_main_merge_libaoxun_hard_soc_source_sync.md) |

后续每一行只在 `main` 合并提交完成时登记；合并提交自身可由该记录、来源 SHA 与 `git log --merges` 唯一定位。
