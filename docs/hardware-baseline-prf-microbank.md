# 2026-11-01 第17轮生产软件基线

本模板已与主树同步正式采用的`round17-prf-microbank-20261101-r02`：
PRF 63×1×32 SRAM＋上一拍写入旁路、BPU置信度选择、D$同组其他way命中及返回字捕获/脏victim写回期间提前返回。

两树生产路径Release及`_DEBUG -O2 -UNDEBUG`四构建的原六项答案、实际退休数和课程周期全部匹配RTL。
课程周期依次为**9259/19595/4363/6636/186111/185415**（median/multiply/vvadd/towers/qsort/rsort），
计时区间为`init+core+drain`，init=1。
三端全部64个PRF的ready/value在411373个post-boot周期逐字节一致，374104条退休PC及周期逐条一致。

默认仍2/4/4/4、双ALU/三CDB、单AGU；嵌套PRF SRAM每拍work一次，冻结阶段仍读并统一sync。
主树24 ticks＋58 helpers＋CPU clock审计通过，模板Register单写断言通过。

完整配置、六项表、同源硬件PPA、SHA256证据及新建F盘构建命令见[共同基线](../../docs/hardware-baseline-prf-microbank.md)。
软件完成证书见[同步认证](../../../sync/evidence/software-prf-microbank-sync-20261101-r01.json)。
[第10轮验收](hardware-baseline-word-production.md)保留历史；当前只验默认配置原六perf。
