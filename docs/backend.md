# 后端

当前恢复已按广播前原ROB存活窗口终止：每拍4槽，`offset + RECOVERY_WIDTH >= liveCount`时结束。同步基线读、至少一拍重放、取消后缀PRF回收、Register单写和所有模块统一sync保持；不再固定执行空beat。

Load A/B/C已同步D$连续lookup、旧AGU结果提前请求及Load返回当拍CDB。返回须匹配旧活动FETCHING行/完整tag/未广播/flush，完整Store转发优先，按年龄竞争旧buffer和共享MUL/DIV；未授权仍正常捕获raw，下拍继续竞争，Register单写/统一sync保持。

发射、执行、两路CDB、退休、四拍恢复及MUL/SRT算法以[主树后端文档](../../docs/backend.md)为共同入口。
模板使用Register单写和统一sync，见[框架指南](help.md)；旧版原文见[归档](_archive/20261018/docs/backend.md)。
双提交由ROB的`retirement[COMMIT_WIDTH]`统一输出，消费者先合并同拍意图，再逐Register单写。

译码宽度现与取指独立，仍单重命名/单发射；Integer/Branch默认4项并已加入授权WB旁路/实际离站复用，其他池旧就绪合同不变。见[RS验收](../../../sync/RS-RTL-TO-SW-20261024.md)，尚非完整RTL同步。
