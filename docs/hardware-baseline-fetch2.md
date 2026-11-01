# 当前模板硬件基线

默认2取指/4译码/4分配/4提交、双ALU/三路写回，与主树和生产RTL同步。
配置、六例真实退休/周期及计时区间见[共同基线](../../docs/hardware-baseline-fetch2.md)。

两个单槽ALU及第二个无状态CDB均注册到运行时，恢复/冻结时也统一sync，不能留下跨拍Wire缓存。
双整数派发独立选址与释放，四路分配可复用两个实际离站槽；ROB/PRF消费两个独立ALU完成。
单跳转口按最老控制结果仲裁；Load/MUL/DIV共享第三口，不新增流水级、访存口或Slave带宽。

Release＋`_DEBUG`＋标准assert六例答案/实际退休/分段周期通过，与主树相同；
11项字段原六例共438378拍与native RTL全等，不扩大为完整状态或全输入等价。
生产复验最终状态见[证据](../../../sync/evidence/software-fetch2-sync-20261030-r01/README.md)。
旧黄金表保留历史，当前只使用[README](../README.md#构建与测试)的专用六例入口。
