# 2026-11-01 第18轮评分最优生产软件基线

模板与主树已复验正式采用的`round18-local-toggle-20261101-r01`。
RTL仅将local三位饱和计数器改为共享翻转逻辑，两软件原有饱和增减语义一致，126项构建输入身份不变。

全新Release及`_DEBUG -O2 -UNDEBUG`四构建原六项答案、实际退休数和课程周期全匹配RTL：
**9259/19595/4363/6636/186111/185415**，以`init+core+drain`计时，init=1。
同源已审计PPA为**34617.306 μm² / 317.815 MHz**，几何IPC为**0.903890966873**。

完整指标、逐项结果与复验命令见[共同基线](../../docs/hardware-baseline-local-toggle.md)，
证书见[软件复验](../../../sync/evidence/software-local-toggle-sync-20261101-r01.json)。
[第17轮PRF逐拍观察](hardware-baseline-prf-microbank.md)保留为历史验证，本次没有重跑该观察。
