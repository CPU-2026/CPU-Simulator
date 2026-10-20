# 模板状态

模板迁移已完成，当前为全模块`work()/sync()`及跨模块Wire接线；不再混用Register与memcpy快照。
最新RTL等价移植包括AXI主机8项信用、四槽恢复窗、D$共享取数、LQ raw读出扩展和字节重叠化简。

2026-10-20新增双提交：ROB统一输出连续前缀，RAT/PRF/LQ/SQ/RAS批量消费，保持Register单写与统一sync。
两软件Release/断言四构建25例同值同退休同周期，单宽25例严格兼容旧golden；
主树对最终RTL25例逐拍/独立ISA、8例冒烟、21例定向及RTL专项通过。
本轮不声称模板对RTL全内部逐拍等价，范围见[双提交记录](../../../sync/DUAL-COMMIT-20261020.md)。
此前四软件构建及主树/模板分别对单提交RTL的25例逐拍/独立ISA通过，属于修改前记录。
结果与边界统一见[同步状态](../../../CPU-SYNC-PLAN.md)及[验收记录](../../../sync/RTL-TO-SW-SYNC-20261018.md)。

[历史迁移记录](_archive/20261018/docs/progress.md)保留原文，不继续在入口累加重复日志。
