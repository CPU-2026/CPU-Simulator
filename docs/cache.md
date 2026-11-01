# Cache与AXI

## 2026-11-01 当前生产合同（优先于下方历史）

D$8 KiB/128组/2路/32 B、4 MSHR/8请求。Load可探测在途缺失同组的其他way，deferred miss仍受组锁和旧信用控制；Store保持锁约束。其他旧活动Load在身份、Store风险及字内范围合格时捕获匹配返回word，raw/ready等Register单点写入；请求行可在脏victim写回期间提前返回。owner/MSHR与完整行安装仍按完整事务收尾，AXIData轮转读/寄存字和整行单拍填充保持。两树四构建原六项匹配第17轮，见[当前基线](hardware-baseline-prf-microbank.md)及[共同Cache设计](../../docs/cache.md)。

**2026-10-31现行生产RTL同步（以下旧合同为历史）**：模板与主树分别实现AXIData关键字起始的轮转读序、R接受沿寄存字返回及整行原位置收集；D$合法字响应提前返回，整行单拍填充各word bank。补齐StoreValue非控制ALU当拍旁路及真实离站复用后，两树原六例答案、退休及课程周期全部匹配正式采用RTL，见[验收](hardware-baseline-word-production.md)。r01的0/0/−34/+221/−36/+1233报告保留为修正前证据。

**2026-10-30当前**：默认双取指I$2包、AXIFetch4行、D$8 KiB；4 MSHR/8请求与AXIData8信用保持，不采用critical-word或扩Slave。当前限定六例验收见[基线](hardware-baseline-fetch2.md)，软件统计不是硬件PPA。

**2026-10-29**：主树已同步8 KiB D$、接受沿同步读及身份/offset/size快照，几何不再分树。
4 MSHR/8请求/8项主机信用与原Slave保持，不含critical-word候选。
当前共同合同见[Cache设计](../../docs/cache.md)，验证范围见[同步记录](../../docs/tree-sync-20261029.md)；下文“主树4 KiB”仅为历史。

## 历史阶段记录

2026-10-27双向同步r02不改Cache/AXI源码：仅Load RS更早消费真实授权WB并按实际共享AGU离站腾位。
请求/响应带宽、r01同步SRAM读、四拍回填、MSHR/信用、Slave服务保持；关键字部分行早返回仍未采用。
完整范围与生产复验状态见[双向记录](../../../sync/MUTUAL-OPT-SYNC-20261027.md)。

2026-10-27双向同步r01：模板补齐RTL空闲Load接受沿同步读，新增`DCachePlan::acceptLookup`及
原读offset5/size2快照。旧count=0/readValid=0、真实接受非MMIO Load才开读；同沿分配原请求槽，
下一拍命中资格与响应仍走旧逻辑。Store/非空请求/MMIO不走此路径，Fill > Store > lookup保持。
原生/sv2v每档12431拍的真实端口/事件对拍及92种offset/size暖命中1拍、三项源码负向均通过；
三档52镜像normal/shuffle共312次独立ISA通过。生产复验状态及边界见[双向记录](../../../sync/MUTUAL-OPT-SYNC-20261027.md)。
这不是AXI全链或整核逐拍等价证明；关键字部分行早返回仍未采用。

**2026-10-27当前模板 D$为8 KiB＝128组×2路×32 B**，offset5/index7/tag20，metadata为21 bit；
每路8个128×32-bit字节写data SRAM、一个128×21-bit metadata SRAM，valid/PLRU随组数派生。
参考主树仍为4 KiB。请求槽8（含owner）、MSHR4、Fill > Store > lookup、四拍整行安装、
脏victim完成门控及AXI/Slave保持；不采用 critical-word 部分R旁路。
全128×2容量命中与高/低组写回oracle、原生/sv2v和两项索引源码负向均已通过；
生产复验亦已通过，见[采用记录](../../../sync/ADOPT-BPU-DCACHE-20261027.md)。

2026-10-26四译码/四发射不改本层：仍双取上限、单数据请求及原AXI信用/Slave服务；
扩大的是ROB/PRF/checkpoint和默认FQ/IQ容量，不扩大I$/D$/MSHR或访存端口。
不复用历史PPA或周期作为四宽验收，见[同步记录](../../../sync/TEMPLATE-WIDE4-20261026.md)。

后续双rename逻辑已移植RTL，仍未扩宽Cache/AXI/Slave；对应周期与独立ISA验收见[RTL移植记录](../../../sync/RTL-DUAL-RENAME-20261025.md)。旧PPA不能直接用于双rename。

2026-10-25：可选双Load/双Store是LSQ分配双宽，非双Cache端口；AGU、Cache、AXI、Slave及退出服务均未改。验证边界见[接线记录](../../../sync/DUAL-RENAME-WIRING-20261025.md)。

当前两树I$/D$/MSHR、共享取数、SRAM端口和回填合同见[参考Cache设计](../../docs/cache.md)。
双提交不拓宽Cache请求口、回填口或AXI信用，两个已提交Store仍在SQ按序逐拍发送。

译码宽度和FQ/IQ容量可以独立配置，但不自动改变Cache/AXI容量。当前默认I$请求4槽/AXIFetch4行，与D$的8请求槽分离；双取默认2包/4字预算及控制流mask已同步，两档分别验收25例通过，见[实施边界](../../../sync/WIDTH-DECODE-SYNC-20261024.md)。

Load A已同步旧Load hit响应/下一SRAM lookup重叠，排除当前readIndex并考虑响应后的最老请求；Fill > Store > lookup保持。两树默认档25/25、8冒烟及真实CPU重叠/串行化负向通过，不增加端口或修改回填信用。
Load B/C已接入旧AGU提前请求与返回当拍CDB，Cache端口和AXI信用不变；合法返回仍正常捕获，不能把物理响应冒充已授权广播。
模板组合事件缓存只保存本拍端口/事件，并在sync失效；旧版原文见[归档](_archive/20261018/docs/cache.md)。
