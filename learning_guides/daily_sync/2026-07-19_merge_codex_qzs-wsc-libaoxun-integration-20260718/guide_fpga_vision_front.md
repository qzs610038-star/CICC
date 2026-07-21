# 学习指南：FPGA 与视频前端 - UART1 Hard SoC 原子批次
> 生成日期: 2026-07-19 | 对应分支合并: libaoxun `72cc281` → 集合分支
> 预计阅读时间: 10-15 分钟 | 面向角色: A

---

## 1. 今日最核心的改动与经验

### 📌 核心点一: UART1 不是改一个开关，而是整套“同批身份证”

* **改动背景与问题**：旧工程只生成 UART0，无法证明 Type-C UART1 路线。新批次把 UART0/UART1/UART2 固定为 `0/1/0`，见 [settings.json](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/ip/EfxSapphireHpSoc_slb/settings.json#L61-L63)。
* **源码剖析**：管脚真源同步把 RX/TX 放到 `GPIOR_96/GPIOR_100`，见 [mem_test.peri.xml](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/mem_test.peri.xml#L178-L182)。IP、wrapper、`.peri.xml`、BSP、`soc.h`、Hello 和制品 manifest 作为一个原子批次合并，不能从旧分支拼装。
* **经验总结**：这像机场联程托运。登机牌、行李牌和护照必须属于同一趟航班；任意一张来自旧航班，即使姓名相同，也不能证明行李会到同一目的地。

### 📌 核心点二: BUILD PASS 与板级 PASS 是两道门

* **改动背景与问题**：批次已经绑定 bitstream、Hello ELF、82 项输入和 21 项制品，但本轮没有在 QZS 主机重跑 Efinity，也没有编程板卡。
* **源码剖析**：固定批次与非声明边界见 [Goal 1 最终审计](file:///D:/第十届集创赛-雄芯院材料/docs/agent_context/QZS_GOAL1_LIBAOXUN_UART1_I0_BUILD_FINAL_AUDIT_20260719.md#L18-L22)；当前状态明确保留 USER2/UART1/APB 为 `NOT VERIFIED`，见 [CURRENT_STATE.md](file:///D:/第十届集创赛-雄芯院材料/CURRENT_STATE.md#L61-L64)。
* **经验总结**：离线构建像消防系统通过图纸审查，板级 smoke 才是现场通水。图纸合格不能替代喷头真的出水。当前结论是“代码审查通过、运行时未触发”。

---

## 2. 可迁移的系统级工程经验

* **避坑警示**：不要手改生成 wrapper 或 `soc.h`；不要用旧 UART0 bitstream/ELF、COM 口或 0-byte 记录给 UART1 背书。任何输入、工具版本或制品 hash 变化都要重开批次。
* **通用方法论**：把“源码身份、工具身份、制品身份、板级现象”分层记录。合并成功只证明 Git 树成立，不自动继承 Map/PNR/STA/CDC 或板上证据。

---

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：
  * **推荐学习内容**：Efinity Hard SoC IP Manager、Interface Designer 管脚分配、Map/PNR/STA/CDC 报告。
  * **应用与意义**：能独立回答“UART1 是在哪里生成、在哪里路由、哪个报告证明时序、哪个现象才算板级成功”。

* **拓展基础知识**：
  * **推荐学习内容**：可复现构建、内容寻址 hash、硬件配置的原子版本管理。
  * **应用与意义**：把 FPGA 工程从“某台电脑能编”升级为“任何审查者都能固定同一输入与制品身份”。
