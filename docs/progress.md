# 模板状态

模板迁移已完成，当前为全模块`work()/sync()`及跨模块Wire接线；不再混用Register与memcpy快照。
最新RTL等价移植包括AXI主机8项信用、四槽恢复窗、D$共享取数、LQ raw读出扩展和字节重叠化简。

四软件构建及主树/模板分别对当前RTL的25例逐拍/独立ISA通过，golden未改。
结果与边界统一见[同步状态](../../../CPU-SYNC-PLAN.md)及[验收记录](../../../sync/RTL-TO-SW-SYNC-20261018.md)。

[历史迁移记录](_archive/20261018/docs/progress.md)保留原文，不继续在入口累加重复日志。
