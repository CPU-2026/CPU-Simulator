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

# 访存

## 2026-11-02 第21轮当前生产

新Load可越过Cache内所有不同行的旧Store；同行、退出MMIO、目标set锁阻止提前查询。旧lookup/fill/Store写保留优先级，读流水空闲或旧Load命中实际交付才授权接受沿同步读，最早下一拍响应。LSU消歧/转发和Store发送合同不变，不是跨bank同拍读写。四构建及外部退休时序见[当前基线](hardware-baseline-disjoint-load.md)。

**2026-11-01第20轮当前**：正式采用PRF共享写仲裁r02，两项Store位置表、单AGU及退出合同沿用下方实现。
两软件全新四构建原六项结果/计数匹配，见[当前基线](hardware-baseline-prf-global-write.md)；下方为历次记录。

## 2026-11-01 第19轮生产采用

队首非MMIO Store未ready/无miss/未deferred且命中旧位置记录，无回填/set锁，
读流水空闲或当拍交付正常Load命中时，直接写原SRAM端口。位置表只训练真实Load命中，
miss分配失效同set记录；Register条件单写、统一sync。详见[Cache合同](cache.md)及[当前验收](hardware-baseline-store-location.md)。

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
