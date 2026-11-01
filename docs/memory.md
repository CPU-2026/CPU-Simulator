# 访存

## 2026-11-01 当前生产合同（优先于下方历史）

同步第17轮D$同组其他way的Load命中及其他旧Load匹配返回word捕获；请求行提前返回不等待脏victim写完成，整行安装与owner/MSHR释放仍按原完整事务条件。SQ/LQ、StoreValue真实派发事件、单AGU及MMIO退出保持；退出冻结阶段PRF继续读SRAM、清写历史，全部模块统一sync。两树原六项答案/退休/课程周期匹配RTL，见[当前基线](hardware-baseline-prf-microbank.md)及[Cache合同](cache.md)。

**2026-10-31当前**：AXIData轮转读/寄存返回字、D$字提前返回与整行填充见[Cache同步状态](cache.md)。StoreValue非控制ALU旁路的真实派发事件同时驱动SQ捕获、LQ通知、RS释放和Issue复用；保持Register单写。六项全匹配见[验收](hardware-baseline-word-production.md)。

**2026-10-30当前**：Load可使用三授权WB口；单AGU、LQ/SQ、Store/退出及外部Slave协议保持。当前限定六例及计时区间见[基线](hardware-baseline-fetch2.md)，不把旧默认验收扩用于新源码。

**2026-10-29**：主树已同步本模板多路LSQ共同前缀、Load RS授权WB/实际离站复用及8 KiB D$接受沿读。
完整tag/epoch、旧信用、包含自身的恢复边界、raw扩展、单AGU/Cache/AXI及MMIO合同保持。
当前规则见[访存设计](../../docs/memory.md)，本轮验证见[同步记录](../../docs/tree-sync-20261029.md)，不扩大历史RTL验收。

## 历史阶段记录

2026-10-27双向同步r02：Load基址可由本拍真正授权的ALU/MUL/DIV/Load写回补齐就绪和值，
经共享最老仲裁进入原寄存AGU；不是未授权物理响应直通，也不改变AGU→LSQ/Load B/C级数。
issue1/2只复用真正离站的Load槽，StoreAddr抢占时不得腾位，issue4原复用保留；
StoreAddr/StoreValue仍只读旧PRF。生产复验见[双向记录](../../../sync/MUTUAL-OPT-SYNC-20261027.md)。

2026-10-27双向同步r01：旧D$ count=0且readValid=0时，真正接受的非MMIO Load在该沿启动同步读；
正常占原请求槽，暖命中最早接受后1拍响应，不是请求→响应组合旁路。
原读身份新增offset/size快照；LQ/CDB、取消、MSHR、整行回填和外部服务不改。
生产复验状态见[双向记录](../../../sync/MUTUAL-OPT-SYNC-20261027.md)。

2026-10-27：D$仅扩为128组、2路、32 B行（8 KiB），请求槽8、MSHR4、单请求/返回口、
AXIData共享8项信用、16项顺序环与外部Slave均不改。LQ/CDB仍按既有raw/符号扩展/授权处理；
尚未加入 critical-word R beat 旁路，原完整行就绪后的early restart保留。
生产采用及复验状态见[记录](../../../sync/ADOPT-BPU-DCACHE-20261027.md)。

2026-10-26四宽：LQ/SQ消费同一0..4条发射前缀，分别按实际Load/Store数推进旧tail，
每条ROB尾快照包含自身及更老lane；信用不包含本拍退休或发送释放。原Load/Store RS各2项，
仍限制实际内存分配前缀，不等于同拍四Load/四Store，更不等于四访存口。单AGU及Cache/AXI/Slave不改。
见[四宽同步记录](../../../sync/TEMPLATE-WIDE4-20261026.md)。

后续RTL也已接入双Load/双Store分配；LQ按旧空槽分配、SQ按Store前缀推进tail，既有完整tag/活动位/committed恢复语义保持。软件尾快照字段不照搬RTL；实际验收见[RTL移植记录](../../../sync/RTL-DUAL-RENAME-20261025.md)。

2026-10-25：`CPU_RENAME_WIDTH=2`支持双Load/双Store分配，分别用旧tail及前缀入队数选择两个不同槽，tail只写一次。ROB每条LSQ恢复边界包含更老lane及自身；旧full仍拒收，不旁路本拍退休/发送释放的额度。AGU/StoreValue、转发及Cache接口带宽保持，见[接线记录](../../../sync/DUAL-RENAME-WIRING-20261025.md)。

当前默认单取指档恢复按原ROB liveCount省略空beat；期间后端仍冻结，LSQ返回和已提交Store/Cache/AXI继续。外部Slave容量/服务与退出协议保持。
可选双取的恢复期安全hit提前取指只开放前端，不扩大本文访存请求、StoreValue和Load广播资格。

LQ/SQ生命周期、raw读出扩展、转发、8项AXI主机信用和MMIO退出统一见[访存设计](../../docs/memory.md)。
LQ/SQ可消费两个实际退休事件；退出Store截断更年轻lane，SQ发送仍单路且不等待Cache才退休。

多译码不增加访存带宽，Load B/C已加入旧AGU地址提前请求/返回当拍CDB；见[实施边界](../../../sync/WIDTH-DECODE-SYNC-20261024.md)。

Integer/Branch/Load使用当拍授权WB派发，StoreAddr/StoreValue仍按旧PRF就绪交付；
Load B旁路的仍是已寄存地址结果，r02新增的WB→Load AGU依赖派发是另一条已授权路径。

完整SQ转发排除B提前读并优先于C返回；返回受阻时正常保存raw，实际CDB授权才标记已广播，实际退休才回收LQ。不会为已ready/已广播行重复发送转发唤醒。
模板不额外添加流水级或跨模块next-state旁路；旧版原文见[归档](_archive/20261018/docs/memory.md)。
