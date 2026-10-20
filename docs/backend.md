# 后端

发射、执行、两路CDB、退休、四拍恢复及MUL/SRT算法以[主树后端文档](../../docs/backend.md)为共同入口。
模板使用Register单写和统一sync，见[框架指南](help.md)；旧版原文见[归档](_archive/20261018/docs/backend.md)。
双提交由ROB的`retirement[COMMIT_WIDTH]`统一输出，消费者先合并同拍意图，再逐Register单写。
