# 2026-10-31 生产 RTL 六项同步

当前共同合同、逐项实测表、来源和复验命令见
[主树同步记录](../../docs/hardware-baseline-word-production.md)。

模板已补齐 StoreValue 非控制 ALU 当拍旁路及真实离站复用；SQ/LQ/RS/Issue
共享接受资格，Register 单写，所有模块 work 后统一 sync。
六项答案、实际退休数和课程总周期全部匹配主树及正式采用的第10轮 RTL。
课程周期顺序为 `9466/19740/4452/6653/188677/193009`，差值全为 0。
