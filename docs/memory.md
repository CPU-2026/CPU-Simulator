# 访存

当前默认单取指档恢复按原ROB liveCount省略空beat；期间后端仍冻结，LSQ返回和已提交Store/Cache/AXI继续。外部Slave容量/服务与退出协议保持。
可选双取的恢复期安全hit提前取指只开放前端，不扩大本文访存请求、StoreValue和Load广播资格。

LQ/SQ生命周期、raw读出扩展、转发、8项AXI主机信用和MMIO退出统一见[访存设计](../../docs/memory.md)。
LQ/SQ可消费两个实际退休事件；退出Store截断更年轻lane，SQ发送仍单路且不等待Cache才退休。

多译码不增加访存带宽，Load B/C已加入旧AGU地址提前请求/返回当拍CDB；见[实施边界](../../../sync/WIDTH-DECODE-SYNC-20261024.md)。

只有Integer/Branch使用当拍授权WB派发，AGU/StoreValue仍按旧PRF就绪交付；B旁路的是已寄存地址结果，不是WB到AGU的依赖派发。

完整SQ转发排除B提前读并优先于C返回；返回受阻时正常保存raw，实际CDB授权才标记已广播，实际退休才回收LQ。不会为已ready/已广播行重复发送转发唤醒。
模板不额外添加流水级或跨模块next-state旁路；旧版原文见[归档](_archive/20261018/docs/memory.md)。
