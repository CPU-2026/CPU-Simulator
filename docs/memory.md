# 访存

LQ/SQ生命周期、raw读出扩展、转发、8项AXI主机信用和MMIO退出统一见[访存设计](../../docs/memory.md)。
LQ/SQ可消费两个实际退休事件；退出Store截断更年轻lane，SQ发送仍单路且不等待Cache才退休。
模板不额外添加流水级或跨模块next-state旁路；旧版原文见[归档](_archive/20261018/docs/memory.md)。
