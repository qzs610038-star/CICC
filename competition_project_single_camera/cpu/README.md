# cpu

板上QCRV32固件目录，当前尚未迁移代码。

计划从`final_project/cpu/`选择性迁移分类器、参数表、任务匹配、唯一`round_controller`和myCobot安全模块。迁移必须删除双摄假设、旧候选APB地址、旧2位目标编码和第二套运行状态机，并适配新Demo生成的`soc.h`。

M2前不创建正式MMIO实现；F1前保持`ARM_ENABLED=0`。
