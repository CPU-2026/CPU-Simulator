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

# 后端

## 2026-11-02 第21轮当前生产

PRF共享写仲裁、执行宽度、单AGU及LQ/SQ合同保持。D$提前查询只改变请求启动时刻，Register单写和统一sync不变；四构建原六项及两个外部副本各374104条退休PC/周期匹配RTL。见[当前基线](hardware-baseline-disjoint-load.md)，完整PRF观察仍属第17轮。

## 2026-11-01 第20轮生产采用（优先于以下历史）

RTL把同tag写仲裁提到行译码之前，保持高序号WB及年轻分配优先，P0、未ready值与上一拍旁路语义一致。
模板沿用既有PRF单写及统一sync实现；两软件126项构建输入身份保持，全新四构建原六项结果/计数匹配，
见[当前基线](hardware-baseline-prf-global-write.md)。下方第17轮PRF及第19轮逐条退休观察为历史证据。

## 2026-11-01 第19轮生产采用

两项Store位置缓冲r02已同步；两软件外部观察副本各374104条退休PC/周期逐条匹配生产RTL，
四个生产构建原六项答案/退休数/课程周期全部通过。PRF与执行资源采用下方合同，
当前证据见[第19轮基线](hardware-baseline-store-location.md)；下方完整PRF状态观察属于第17轮。

## 2026-11-01 当前生产合同（优先于下方历史）

PRF的P1–P63各为`SRAM<1,32>`，P0常零；ready/everWritten独立，四分配＋三WB共7条上一拍写入记录覆盖写后读。每个分配都写PC+4，包括未ready项；年轻分配优先，高序号WB优先。退出冻结仍work嵌套SRAM一次并清有效写历史，统一sync全部Register/Wire。保持默认2/4/4/4、双ALU/三CDB、单AGU及StoreValue非控制ALU授权旁路，MUL RS未采用同拍复用。411373拍完整PRF及374104条退休PC/周期三端一致，见[当前基线](hardware-baseline-prf-microbank.md)。

**2026-10-31当前**：StoreValue使用实际授权的非控制ALU结果当拍旁路，所有真实离站槽可复用。SQ捕获/LQ通知/RS释放/Issue信用共用同一事件，新分配优先；共享CDB不进入此提前旁路。六项全匹配见[验收](hardware-baseline-word-production.md)。

**2026-10-30当前**：默认2/4/4/4、双单槽ALU、独立CDB0/1＋共享Load/MUL/DIV CDB2。双整数离站槽均可复用，ROB/PRF接双ALU完成；第二ALU/CDB参与注册及冻结sync。当前限定六例验收见[基线](hardware-baseline-fetch2.md)，下方旧默认为历史。

**2026-10-29**：主树已按其comb/tick风格同步本模板1/2/4路共同分配前缀及Load RS授权WB/真实离站复用。
Integer/Branch/Load可旁路，StoreAddr/StoreValue/MUL/DIV旧就绪不变；新项不当拍执行。
当前合同见[主树后端](../../docs/backend.md)，验证范围见[同步记录](../../docs/tree-sync-20261029.md)。下文“主树仍单发射”仅为历史。

## 历史阶段记录

2026-10-27双向同步r02：Load旧活动项像Integer/Branch一样使用两路实际授权WB补足就绪和值，
Load→AGU消费者同步接入，旧ready值优先/P0不匹配WB。StoreAddr/StoreValue/MUL/DIV仍只看旧就绪。
issue1/2优先旧空槽，满时仅复用共享AGU实际接受的旧Load槽，第二lane排除第一lane实际占用；
issue4已有复用保留。新项不当拍派发，RS原push优先、Register单写/统一sync保持。
三文件与隔离候选字节一致后写入生产，采用后复验状态见[双向记录](../../../sync/MUTUAL-OPT-SYNC-20261027.md)。

2026-10-27双向同步r01仅移植RTL已有的D$空闲Load接受沿同步读，当轮后端源码及带宽保持。
当时未采用的Load RS优化现已由上方r02继续，不重标r01证书。
独立ISA与生产复验状态见[双向记录](../../../sync/MUTUAL-OPT-SYNC-20261027.md)，不声称整核逐拍相等。

2026-10-27：BPU checkpoint新增3位预测时上下文，D$改为8 KiB；本轮不改后端宽度、执行/CDB/提交口或恢复流水级。
Register单写、统一sync及既有单/双/四发射选项保持。单取/双取和已有四宽选项的生产回归已通过，
下方旧周期/性能结论不作为新配置验收，见[采用记录](../../../sync/ADOPT-BPU-DCACHE-20261027.md)。

2026-10-26：模板接入RTL四路顺序重命名合同。`CPU_ISSUE_WIDTH=4`选择ROB32/PRF64/checkpoint64、
6-bit完整ROB身份及8槽恢复窗；单/双宽保持ROB16和原路径。RAT/PRF/ROB/七池RS/LQ/SQ共用同一连续前缀，
按实际目的数和各内存池分别排名；RAW选最近实际写者，LINK可转发PC+4，WAW串接oldPhy且最年轻映射胜出。
ROB/PRF/LSQ只用旧信用，HALT截断后缀；RS四宽按可用行排名，Integer/Branch/Load允许实际旧离站行复用。
组合计划缓存每次sync失效，不是状态寄存器或模块next-state复制；原消费者逐Register单写和统一sync保持。
不新增执行/写回/提交/Cache端口；不把本次逻辑同步当作最新RTL所有优化的全内部逐拍证明。
下方双宽历史限制仍仅描述旧路径，验收见[同步记录](../../../sync/TEMPLATE-WIDE4-20261026.md)。

后续已将双rename/双Load/双Store分配移植RTL，主树仍单发射；RTL组内LINK依赖入站捕获PC+4/ready以匹配模板下拍读PRF，执行单元和CDB带宽不扩宽。最新配置及验收见[RTL移植记录](../../../sync/RTL-DUAL-RENAME-20261025.md)，下方“主树和RTL未改”为模板最初阶段的范围。

当前恢复已按广播前原ROB存活窗口终止：每拍4槽，`offset + RECOVERY_WIDTH >= liveCount`时结束。同步基线读、至少一拍重放、取消后缀PRF回收、Register单写和所有模块统一sync保持；不再固定执行空beat。
双取配置恢复期仅在当前PC为非控制流I$ hit时开放Fetch/FQ/IQ；RS/FU/ROB/FlushArbiter/CDB仍冻结，所有模块sync继续清Wire缓存。

Load A/B/C已同步D$连续lookup、旧AGU结果提前请求及Load返回当拍CDB。返回须匹配旧活动FETCHING行/完整tag/未广播/flush，完整Store转发优先，按年龄竞争旧buffer和共享MUL/DIV；未授权仍正常捕获raw，下拍继续竞争，Register单写/统一sync保持。

发射、执行、两路CDB、退休、四拍恢复及MUL/SRT算法以[主树后端文档](../../docs/backend.md)为共同入口。
模板使用Register单写和统一sync，见[框架指南](help.md)；旧版原文见[归档](_archive/20261018/docs/backend.md)。
双提交由ROB的`retirement[COMMIT_WIDTH]`统一输出，消费者先合并同拍意图，再逐Register单写。

取指、译码和重命名宽度独立。`CPU_RENAME_WIDTH=1/2`默认1；双宽按程序顺序分配ROB/PRF/RAT/七池RS/LQ/SQ，允许同拍双Load/双Store，RAW仅转发物理tag，WAW保留年轻映射且oldPhy沿两条指令串接。Integer/Branch旧空槽优先并可复用未被另一lane占用的实际离站槽，其他池旧就绪合同不变。每个Register单写、所有work后统一sync；不增加执行单元或CDB带宽。见[接线记录](../../../sync/DUAL-RENAME-WIRING-20261025.md)，主树和RTL未改，不能套用历史三端逐拍结论。
