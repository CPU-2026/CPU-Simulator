## 2026-11-03：正式采用空IQ整组旁路版（优先于以下历史）

用户在知悉面积／频率代价后要求“采用生产”，已采用`iq-whole-group-ppa-20261103-r01`。面积34819.996867 μm²、Fmax310.303030 MHz、六项几何IPC0.981946949077、评分2.379472345843e-07；相对上一生产IPC+2.807266%、面积+0.387729%、Fmax-1.151515%、评分+9.573112%。IPC×Fmax估算+1.623425%（不是墙钟实测）。本版为已审计评分及IPC最高候选，旧生产和旧评分最优版继续保留。
旧IQ为空时展示旧FQ译码整组，候选展示不依赖后端ready；仅全部有效lane都能分配时授权全部RAT／PRF／ROB／RS／LSQ目标并跳过IQ写入，否则分配数零、整组入IQ。IQ非空保持原连续前缀；按实际有效条数，不要求凑满四条。两软件同步同一门控与单次消费语义，主树只读旧快照/Input逐字段写目标，模板Register单写／统一sync保持；无模块下一状态复制。
保留前端恢复冻结解耦、安全响应鉴权及后端恢复冻结；不改BPU算法／表、访存、容量、执行／写回／退休宽度。不是部分整组旁路，不带入已否决DCache miss接续、接受沿回填字捕获、Store地址通知或第28轮候选。
44项RTL及公共SRAM与已审计候选逐字节认证，复用已完成整核综合／ABC／STA，不重跑综合，也未另建生产路径RTL模型。模型资格来自`iq-whole-group-screen-20261103-r01`的native／converted、局部合同及三个负向控制；PPA归属独立测量目录，不冒称完整状态等价。
两软件全新Release／断言四构建原六项答案／退休数／init+core+drain及外部观察副本退休PC／周期匹配；课程周期9090/18228/3994/6029/163977/168557。旧黄金、镜像、Slave、既有code及三树HEAD／Git索引保持；使用当前生产专用检查器，不套历史黄金周期。
面积<36000与Fmax>300通过，长期几何IPC>1.0985仍未达到。此次为已测候选采用，不是新优化轮次。单候选PPA授权已完成，综合／ABC／STA继续暂停，不恢复第28轮、不自动继续优化。
正式指针`sync/evidence/current-production.json`；证书`sync/evidence/iq-whole-group-production-adoption-20261103-r01/adoption.json`；软件说明`docs/hardware-baseline-iq-whole-group.md`。以下数字均保留各自历史归属。

## 2026-11-03：正式采用前端恢复冻结解耦版（优先于以下历史）

用户在知悉评分和频率代价后要求“应用到生产”，采用`frontend-recovery-decouple-ppa-20261103-r01`。面积34685.510947 μm²、Fmax313.917842 MHz、六项几何IPC0.955133800420、评分2.171584154117e-07；相对上一生产IPC+0.185873%、面积+0.125295%、Fmax-0.735745%、评分-0.245794%。IPC×Fmax估算-0.551240%（非墙钟实测）。本版是已审计IPC最高版，不是评分最优版；评分最优旧生产仍完整保留。
恢复期间继续禁止未知／控制包的新预测请求，按实际返回指令鉴权安全非控制响应，并允许安全FQ→IQ推进；后端分配／派发／写回／退休仍冻结。两软件同步同一门控语义，保留旧快照/Input逐字段写目标和模板Register单写／统一sync纪律。不改BPU预测／训练算法或表、队列容量、存储协议、Slave；不恢复已否决回填字捕获或Store地址通知，不带入第28轮候选。
44项RTL及公共SRAM与已审计候选精确同源，复用其整核综合／ABC／STA，本次采用不重跑综合，也未另建生产路径RTL模型。模型资格来自`frontend-recovery-decouple-screen-20261103-r01`的native／converted及局部定向／负向验证；测量归属独立PPA目录，不冒称新生产路径重建或完整状态等价。
两软件全新Release／断言四构建原六项答案／退休数／init+core+drain及外部观察副本退休PC／周期匹配；课程周期9570/18946/3995/6126/174053/168587。旧黄金、镜像、Slave、既有code、三树HEAD／Git索引保持，专用当前生产检查器验收，不套旧黄金周期。
面积<36000和Fmax>300通过，长期几何IPC>1.0985仍未达到。本次是已测候选的采用，不是新优化轮次。综合／ABC／STA保持暂停；此前单候选综合授权已完成，不恢复第28轮或自动继续优化。
正式指针`sync/evidence/current-production.json`；采用证书`sync/evidence/frontend-decouple-production-adoption-20261103-r01/adoption.json`；软件说明`docs/hardware-baseline-frontend-decouple.md`。以下数字均保留各自历史归属。

## 2026-11-03：采用评分最优生产版；全部综合暂停（优先于以下历史）

用户要求“暂停所有综合任务，将评分最优应用于生产即可”，已采用`round27-lq-address-area-20261103-r01`。面积34642.106287 μm²、Fmax316.244595 MHz、六项几何IPC0.953361761030、评分2.176934924387e-07；相对上一生产IPC+1.415615%、面积-0.104184%、Fmax-0.154416%、评分+5.840915%。IPC×Fmax估算+1.259013%（非墙钟实测）。
采用奇字位置双取指（跨8字节对齐对、不跨I$行、最早控制指令截断）及LQ结果/地址载荷去复位；valid/就绪/身份控制仍复位。不是三Load仲裁组合版，不含第28轮年龄门禁或SQ地址候选；Store位置表仍4项、Store地址/数据RS仍2项、Branch RS仍4项。不改预测/训练算法或表，不恢复被否决字捕获或Store地址通知。
44项RTL及公共SRAM与已审计候选逐字节认证，复用该版本已审计PPA，本次不启动任何综合/ABC/STA。两软件只同步fetch2包几何与恢复时控制截断（模板适配位于BPU.cpp的fetchGroup，不改predict/训练）；保留无效载荷初始化的表示差异，不声称完整状态等价。
两软件全新Release/断言四构建原六项答案/退休数/init+core+drain及外部副本退休PC/周期匹配；课程周期9607/18947/3995/6126/175315/168588。未另做生产路径RTL模型重建；采用资格来自精确同源候选native/converted完整日志和局部契约。
所有综合已暂停；未经用户重新授权，不恢复第28轮或启动新优化综合。第28轮面积任务在dff-map中止，checkpoint/失败与既有产物保留，整轮不标完成。面积/频率门槛通过，长期IPC>1.0985仍未达到；旧黄金/镜像/Slave/既有code、三树HEAD及Git索引保持。
正式指针`sync/evidence/current-production.json`；证书`sync/evidence/score-best-production-adoption-20261103-r01/adoption.json`；软件说明`docs/hardware-baseline-score-best.md`。以下历史数字保留原归属。

## 2026-11-02：正式采用Store命中位置表4项（优先于以下历史）

用户批准“补齐验证和PPA后采纳”，已完成所选候选的整核converted、全新综合/ABC/STA独立审计及两软件Release/断言四构建，正式采用。此前“暂不综合”仅是筛选阶段限制；未修改历史筛选记录。
仅DCache位置元数据2→4，最低无效项优先、已有同行原位训练、victim=(trained+1)&3；不是精确4项LRU。Store地址/数据RS仍2项、发射1/2路，Branch RS仍4项；不改BPU、不恢复已否决字捕获或Store地址通知。
面积34678.235527 μm²、Fmax316.733684 MHz、六项几何IPC0.940054209024、评分2.056799041947e-07；相对第22轮IPC+1.025965%、面积+0.709439%、Fmax-3.093102%、评分-0.471751%。按各自Fmax估算IPC×Fmax-2.098872%（非墙钟实测）。
原六项课程周期9338/19591/4278/6127/177204/168587；native/converted完整日志一致、两软件四构建答案/退休数/init+core+drain匹配，外部观察副本退休PC/周期匹配。44项RTL及公共SRAM、两软件构建输入逐字节认证；复用精确同源已测模型，不声称另做生产路径重建或完整状态等价。
面积<36000及Fmax>300通过，长期IPC>1.0985尚未达到。本次是既有候选的采用补验，不含新面积专项、不冒称完整新优化轮次。旧黄金/镜像/Slave/既有code及Git索引保持；当前验收用专用生产检查器，不套历史黄金周期。
证书：工作区`sync/evidence/store-location4-production-adoption-20261102-r01/adoption.json`；正式指针`sync/evidence/current-production.json`。当前软件说明`docs/hardware-baseline-store-location4.md`，以下测量均保留各自历史归属。


## 2026-11-02：按用户要求删除Store地址通知旁路（优先于历史基线）

Store地址就绪只更新SQ，不再直接向Load通知数据，也不再压住Cache返回。保留Load地址查询SQ、Store数据就绪通知两条转发；未命中两事件的Load等待旧Store提交/写入Cache后读取，保守消歧及完整身份校验不变。
本次全新native/converted原六项及两软件Release/断言四构建答案/退休/课程周期匹配：9338/19591/4278/6317/177506/173548。全新整核综合/ABC/STA独立审计：面积34433.947627 μm²、Fmax326.843281 MHz、几何IPC0.930507527921。
采用证书位于工作区`sync/evidence/store-address-notify-removal-production-20261102-r01/adoption.json`；当前生产以`sync/evidence/current-production.json`为准。下方第21轮等数字保留历史归属，不作为删除后RTL的PPA。

# Cache与AXI

## 2026-11-02 第21轮当前生产

接受沿提前查询在旧请求事件选定后计算，不递归求events()。所有旧Store不同行且非退出MMIO、目标set未锁定、无旧lookup/fill/Store写、读流水空闲或旧Load命中已交付时，新Load可启动同步读；普通请求槽及读快照保持。不是Store写当拍跨bank并发。四构建和外部退休时序通过，见[当前基线](hardware-baseline-disjoint-load.md)与[共同Cache合同](../../docs/cache.md)。下方空队列限制属于旧版。

**2026-11-01第20轮当前**：正式采用PRF共享写仲裁r02，D$两项位置表及Register单写/统一sync合同继续适用。
两软件全新四构建原六项结果/计数匹配，见[当前基线](hardware-baseline-prf-global-write.md)；下方为历次记录。

## 2026-11-01 第19轮生产采用

两项Store位置表保存有效位、行地址和way，属于`DCacheInner`统一sync的Register。
只训练真实SRAM Load命中；按旧态已有同行记录→无效槽（槽0优先）→victim选槽，训练后换victim。
miss分配失效同set记录，合格队首Store直接写原1RW端口；每set PLRU按fill→直接Store→普通read hit单写。
完整准入见[共同Cache合同](../../docs/cache.md)，原六项四构建及逐条退休复验见[当前基线](hardware-baseline-store-location.md)。

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
