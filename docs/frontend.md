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

# 前端

## 2026-11-02 第21轮当前生产

前端/BPU算法及默认2/4/4/4未改；D$提前查询改变执行时序，median退休分支误预测增加16次。四构建原六项及外部退休时序通过，不因源码相同继承旧周期，见[当前基线](hardware-baseline-disjoint-load.md)。

**2026-11-01第20轮当前**：正式采用PRF共享写仲裁r02，前端采用下方2/4/4/4及预测时上下文合同。
两软件全新四构建原六项结果/计数匹配，见[当前基线](hardware-baseline-prf-global-write.md)；下方为历次记录。

## 2026-11-01 第19轮生产采用

当前两项Store位置缓冲r02已完成四构建原六项及逐条退休PC/周期复验。
前端按下方默认2/4/4/4、FQ8/IQ8与预测时上下文合同运行，当前证据见[第19轮基线](hardware-baseline-store-location.md)。

## 2026-11-01 当前生产合同（优先于下方历史）

同步第17轮BPU置信度选择：仅PC chooser=3且global强（计数器0/3）或local弱（3/4）时选global。LHT/local/global/chooser容量和预测时上下文训练保持；默认2/4/4/4、FQ8/IQ8与replay安全双取包保持。两树四构建原六项匹配正式生产RTL，见[当前基线](hardware-baseline-prf-microbank.md)及[共同前端](../../docs/frontend.md)。

**2026-10-31当前**：生产访存及StoreValue旁路/复用同步后，原六项答案、退休、课程周期全部匹配正式采用RTL，见[验收](hardware-baseline-word-production.md)。前端保持既有配置/算法；以下10-30状态为历史。

**2026-10-30当前**：默认2/4/4/4，FQ8/IQ8、I$2包；replay仅检查当前有效非控制包，新包不按FQ余量缩短，准入检查旧返回包。当前限定六例验收见[基线](hardware-baseline-fetch2.md)，下方旧默认为历史。

**2026-10-29**：主树已同步本模板生产BPU及独立1/2/4宽度，默认仍1/1/1/2。
FQ默认FETCH_WIDTH×2、IQ默认DECODE_WIDTH×2，2/4/4/2实际为FQ4/IQ8；下文旧FQ8及“主树未同步”属于历史。
当前合同见[主树前端](../../docs/frontend.md)，本轮验证范围见[同步记录](../../docs/tree-sync-20261029.md)，不延用旧RTL/PPA结论。

## 历史阶段记录

2026-10-27双向同步r02仅补Load RS授权WB派发/值与issue1/2真实离站复用，前端/BPU源码不改。
生产复验及新周期表状态见[双向记录](../../../sync/MUTUAL-OPT-SYNC-20261027.md)，不继承旧整核周期。

2026-10-27双向同步r01仅改D$接受沿同步读和原读元数据，不改本节BPU/前端。
新模板周期不能继承旧表，状态及证据见[双向记录](../../../sync/MUTUAL-OPT-SYNC-20261027.md)。

2026-10-27当前 BPU：256×1-bit解析训练 LHT；local PHT256×3-bit，初值3/阈值4，
索引为`((PC>>2)<<1 | savedLocalHistory)&255`；global PHT256×2-bit，按PC xor savedGHR索引；
chooser256×2-bit按PC索引，初值1，值≥2选择global。checkpoint除GHR8外保存局部历史和两分量预测共3位，
训练使用预测时上下文而不是解析时已改变的决定，所有读取仍为拍初旧态。
LHT只按有效条件分支解析更新，不在fetch投机更新；既有存活/年龄过滤、GHR恢复及BTB/RAS不改。
模板逐lane保存上下文；RTL可将包内首CFI上下文广播到两槽，非CFI位不参与条件训练。
生产源码已采用且复验通过，见[采用记录](../../../sync/ADOPT-BPU-DCACHE-20261027.md)。

2026-10-26模板四宽：`CPU_DECODE_WIDTH=1/2/4`，`CPU_ISSUE_WIDTH=1/2/4`独立配置，
`CPU_RENAME_WIDTH`为发射宽度兼容别名。四宽示例是fetch2/decode4/issue4/commit2，FQ/IQ默认各8项；
计数载体可表示0..4，所有lane保留独立PC/预测PC/CID，IQ只消费旧态有序前缀，无译码到发射直通。
双取供给不变，依靠积压才能同拍译码四条。默认1/1/1/2及主树实现不改。
本节优先于下方历史“旧CPU_ISSUE_WIDTH仅前端兼容”的说明，验证边界见[同步记录](../../../sync/TEMPLATE-WIDE4-20261026.md)。

后续RTL双rename已完成：与模板一样消费旧IQ的0/1/2条有序前缀。固定fetch/decode/commit=2的单/双rename各25课程＋40定向通过独立RTL退休ISA，课程周期/退休数与模板相等；不是主树双发射或全内部逐拍证明。见[RTL移植记录](../../../sync/RTL-DUAL-RENAME-20261025.md)。

2026-10-25：新增`CPU_RENAME_WIDTH=1/2`，默认单发射。双发射只消费拍初IQ中存在的顺序前缀，第二条有效要求IQ至少2项；本拍IQ入队空间按真实issuePopCount（0/1/2）计算。无本拍译码到发射直通，见[接线记录](../../../sync/DUAL-RENAME-WIRING-20261025.md)。下面历史主树/RTL周期结论仅覆盖单发射。

当前生产默认I$请求4、AXIFetch4、BTB32、RAS8；模板BTB tag位宽/位移随容量派生。1/1/2默认与2/1/2、2/2/2可选档均各25例与主树及对应RTL结果/退休/core/total对齐；双取新packet、逐指令CID和恢复期安全hit提前取指已合入。

当前共同规则见[参考主树前端设计](../../docs/frontend.md)；两树同配置周期须按本轮实际结果判定，不能仅凭接口一致推断。
默认取指/译码/发射单宽，宽度独立1/2/4；双取默认I$2包/4字预算，每条独立CID且保存同一旧GHR。RAS按实际退休前缀顺序折叠pop/push事件，SRAM同槽年轻写胜出。
模板专有规则见[框架指南](help.md)；旧版原文见[归档](_archive/20261018/docs/frontend.md)。
