## 2026-11-07：正式采用跨16B四取指（优先于以下历史）

用户“采纳生产”，在已审计`fetch4-cross16-ppa-20261107-r01`及两软件资格通过后正式发布。面积34947.061567 μm²（+0.713718%），Fmax估计319.301528 MHz（+0.997817%），六项几何IPC1.137201912621（+6.947322%），评分4.372501753378e-07（+30.260993%）。面积<36000、Fmax>300、IPC>1.0985三项均通过；IPC×Fmax估算+8.014461%，非墙钟实测，频率为同理想时钟／无寄生STA估计、非后布局结果。
默认fetch/decode/rename/commit为4/4/4/4，双ALU／三CDB保持。只替换pkg／frontend／ICache三RTL：任意字位置连续最多4条、允许跨16B但不跨64B行、最早CFI截断、cold单条、单预测及逐条PC／CID不变。两请求包槽／派生8条指令信用，FQ8／IQ8／ROB32／PRF64／checkpoint64／I$1KiB保持，四取指仍采用保守恢复冻结，不加入双取指专属恢复扩展；LSU已采用通知提前计算／SQ完成旁路／SQ4-owner0仲裁／Load预取／标准RAS、Cache／AXI／Slave及其他容量执行宽度保持。
两软件默认宽度头与取包几何共4项构建输入变化、122项保持。主树只读旧快照/Input逐字段写目标，模板Register单写／统一sync保持；所有状态writer／预测及训练不改，无模块next-state复制或生产观察探针。全新Release／断言四构建原六答案／退休／init+core+drain及两外部副本各374104条退休PC／周期匹配。周期7620/12379/3740/5477/148089/159739；qsort相对双取指多614拍，其他五项改善。限定原六资格，不是全状态／全输入形式等价。
44RTL／公共SRAM／converted与已审计候选精确同源，复用本次已认证全新整核综合／ABC／STA及native／converted筛选，不重新综合或冒称另建生产路径RTL模型。发布前备份三RTL／四软件／文档／旧指针，旧黄金／镜像／Slave／既有code及三树HEAD索引保持；历史证书不回写。采用证书`sync/evidence/fetch4-cross16-production-adoption-20261107-r01/adoption.json`，正式指针`sync/evidence/current-production.json`；软件说明`docs/hardware-baseline-fetch4-cross16.md`。
单候选授权完成，全部综合／ABC／STA继续暂停，未提交／推送；不自动继续实验、组合、综合或清理，无额外面积专项，不算完整新优化轮。

## 2026-11-06：正式采用LSU通知资格提前计算（优先于以下历史）

用户明确“采用”，在已审计整核PPA及本次两软件资格通过后发布`lsu-notify-precompute-ppa-20261106-r01`。面积34699.405687 μm²（+0.679820%），Fmax316.146959 MHz（+3.056499%），六项几何IPC1.063329023912（不变），评分3.356723806268e-07（+1.669462%）。面积增234.300600 μm²，面积<36000／Fmax>300通过，长期IPC>1.0985仍未达。IPC×Fmax估算+3.056499%，非墙钟实测；同理想时钟无寄生STA估计，不是后布局频率。
只替换`rtl/memory/rv32_lsu.sv`的返回通知优先级组合拓扑：旧LQ×SQ常数行先计算同START同WIDTH／年龄及年轻未知地址或同word屏障，晚到SV／返回身份仅选一位。notify函数／完整事件身份与flush、Load地址事件最终覆盖、全部SQ／LQ状态写入／完整转发／SQ完成旁路／owner取消／SQ4-owner0仲裁／Load预取／标准RAS及容量宽度和Slave保持。不组合ROB候选，不恢复已否决方案。
两软件126项构建输入全部保持，无需算法改写；主树旧快照/Input逐字段写目标、模板Register单写／统一sync纪律不变，不恢复生产观察探针。全新Release／断言四构建原六答案／退休／init+core+drain及两外部副本各374104条退休PC／周期匹配。周期7676/16908/3984/5509/147475/162806全部保持，独立软件raw审计通过；限定原六资格，不是全状态／全输入形式等价。
44RTL／公共SRAM／converted与已测候选精确同源，复用其已审计整核综合／ABC／STA及native／converted局部619252组任意旧态／真实LSU差分／六有效负向和原六完整日志，本次无新综合或生产路径RTL模型重建。发布前备份一RTL／文档／旧指针，旧黄金／镜像／Slave／既有code和三树HEAD索引保持，旧证书不回写。采用证书`sync/evidence/lsu-notify-precompute-production-adoption-20261106-r01/adoption.json`，正式指针`sync/evidence/current-production.json`；软件说明`docs/hardware-baseline-lsu-notify-precompute.md`。
单候选授权完成，全部综合／ABC／STA继续暂停，未提交／推送。不自动继续实验、组合、综合或清理，无额外面积专项，不算完整新优化轮。

## 2026-11-06：正式采用旧SQ完成事件旁路（优先于以下历史）

用户“采纳当前的SQ完成旁路”后明确“综合”，在本次新整核PPA独立审计、面积<36000／Fmax>300及两软件资格通过后采用`store-completion-events-ppa-20261106-r01`。面积34465.105087 μm²（-0.432579%），Fmax306.770521 MHz（-2.127022%），六项几何IPC1.063329023912（+0.366506%），评分3.301604769298e-07（+0.180573%）。IPC×Fmax估算-1.768312%，非墙钟实测；长期IPC>1.0985仍未达。
仅一LSU完成bitmap：旧有效未提交SQ完整身份的真实地址／数据／非P0匹配WB可通知ROB寄存ready，退休仍读旧ready。SQ／LQ全部状态写入、实际Store退休／访存接受、Store→Load转发、owner／取消、SQ4-owner0仲裁、Cache预取、标准RAS、容量宽度和Slave保持，不恢复已否决地址→Load数据通知，不含三取指。
两软件各SQ组合查询／头文件与CPU接线共6项构建输入同步、120项保持，主树旧快照/Input逐字段写目标，模板Register单写／统一sync保持。只在完成锥读取完整WB，旧SV／LQ转发锥仍只读early-ALU；旧SV owner保存软件数据依赖身份，对应RTL SQ dataTag，不创建模块next-state或生产观察探针。全新Release／断言四构建原六答案／退休／init+core+drain与两外部副本各374104条退休PC／周期匹配。周期7676/16908/3984/5509/147475/162806，相对标准RAS−345/−85/+31/+32/+1105/+953，median IPC+4.494528%，整体+0.366506%，四项回退已披露。限定原六资格，不是全状态／全输入形式等价。
44RTL／公共SRAM／converted与已审计r02精确同源，本次从elaborate全新六checkpoint／整核opt／ABC／STA及独立逐叶面积／248原宏／工具约束／raw退休评分审计，不拼接旧PPA。复用筛选完整双模型资格，不冒称另建生产路径RTL模型。软件r01只因验证入口未导出路径ImportError停止，未构建；r02仅修导入，六源保持，原失败保留。发布前备份一RTL／六软件／文档／旧指针；旧黄金／镜像／Slave／既有code及三树HEAD索引保持，旧证书不回写。采用证书`sync/evidence/store-completion-events-production-adoption-20261106-r02/adoption.json`，正式指针`sync/evidence/current-production.json`；软件说明`docs/hardware-baseline-store-completion.md`。
单候选授权完成，全部综合／ABC／STA恢复暂停，未提交／推送。不自动继续实验、组合、综合或清理，无额外面积专项，不算完整新优化轮。

## 2026-11-06：正式采用标准同link JALR只push修正（优先于以下历史）

用户“替换生产并综合”，在本次新整核PPA独立审计、面积<36000及Fmax>300和两软件资格通过后采用`ras-same-link-ppa-20261106-r01`。面积34614.841687 μm²（+0.313261%），Fmax313.437404 MHz（-0.306091%），六项几何IPC1.059446088949（+1.509617%），评分3.295653702718e-07（+5.191507%）。IPC×Fmax估算+1.198905%，非墙钟实测；长期IPC>1.0985仍未达。
仅四RTL frontend／core／rename-plan／RAS分类：JALR pop=rs1Link && rd!=rs1、push=rdLink，x1/x5同link只push，异link保留pop后push。同步投机／BTB类型／标量双宽四宽ROB／真实退休／原始指令恢复，RAS容量8、满空与恢复节奏不变；不组合总线2∶1，不改SQ4／owner0仲裁、Cache／Slave／容量／宽度。
两软件各CPU／StaticArbiter共4项构建输入同步、122项保持，decoded hint沿既有退休／恢复路径；主树旧快照/Input逐字段写目标、模板Register单写／统一sync保持。精确复用r04已审计全新Release／断言四构建原六答案／退休／init+core+drain及两外部副本各374104条退休PC／周期资格，不重复构建、不冒称另建生产路径RTL模型。周期8021/16993/3953/5477/146370/161853；towers少372拍、multiply少413拍，qsort多1拍。局部双模型24576组合、真实RAS／SRAM及七有效RTL负向通过；原三夹具失败证据保持，限定原六资格而非全状态／全输入形式等价。
44RTL／公共SRAM／converted精确同源，本次从elaborate全新六checkpoint／整核opt／ABC／STA，独立Liberty逐叶面积／248原宏形状／工具库约束／raw退休评分审计通过，不拼接旧PPA。发布前备份四RTL／四软件／文档／旧指针；旧黄金／镜像／Slave／已有code和三树HEAD索引保持，旧证书不回写。证书`sync/evidence/ras-same-link-production-adoption-20261106-r01/adoption.json`，正式指针`sync/evidence/current-production.json`；软件说明`docs/hardware-baseline-ras-same-link.md`。
单候选授权已完成，全部综合／ABC／STA恢复暂停，未提交／推送。不自动继续实验、组合、综合或清理，无额外面积专项，不算完整新优化轮。

## 2026-11-06：正式采用SQ≤4／owner=0安全Load让行仲裁（优先于以下历史）

用户“采用SQ <=4 且owner = 0的并进行综合”，在新整核PPA及两软件资格通过后采用`lsu-sq4-owner0-ppa-20261106-r02`。面积34506.745567 μm²（+0.259469%），Fmax314.399754 MHz（+2.364139%），六项几何IPC1.043690365779（+0.759869%），评分3.133003593523e-07（+4.965701%）。IPC×Fmax估算+3.141972%，不是墙钟实测。面积<36000及Fmax>300通过，长期IPC>1.0985未达；rsort+2262拍，其他五项改善。
仅LSU＋core两RTL：旧态全部已分配SQ≤4且LSU运输owner=0时，原最老合法Load可越过全部字节不重叠的已提交／同拍真实退休普通RAM Store一次，随后强制真实Store接受；债务跨flush保持、两处SQ只认真实Store grant，恢复冻结／冲刷／退出不新增让行。不改Cache／Slave／容量／执行宽度／原转发，保留既有Load下一行预取。
两软件9项构建输入同步、117项保持；运输owner计数包括取消后的真实返回，Cache新增纯功能终结事件查询而非改变Cache算法或生产观察探针。主树旧快照/Input逐字段写目标，模板Register单写／统一sync，raw proposal-valid与实际接受分离保持。r01新增顶层标量被原纪律检查拒绝，r02模板SQ计数混用epoch尾指针与物理头槽导致周期漂移，两次失败保留；r03用旧有效行计数，未放宽检查。全新Release／断言四构建原六答案／374104条退休／init+core+drain及两外部副本退休PC周期匹配RTL；周期8022/17406/3953/5849/146369/161862。限定原六资格，不是完整状态／全输入形式等价。
44项RTL／公共SRAM／converted与已审计sq4-owner0精确同源，本次从elaborate全新六checkpoint／整核opt／ABC／STA及独立Liberty逐叶面积／248原宏形状／工具库约束／raw退休评分审计完成，不拼接旧PPA；原r01在ABC因服务器重启中断，五个checkpoint与失败产物保留；r02从elaborate全新重测，无checkpoint继承。r02完成后首审因来源标签变量遮蔽失败，旧失败保留；r03依据原五checkpoint路径与哈希重绑定来源，完整重审同一PPA，不重跑综合／ABC／STA。采用阶段复用r02新PPA与r03审计和已通过的软件r03资格，不重复软件构建，也不冒称另建生产路径RTL模型。备份／旧黄金／镜像／Slave／既有code及三树HEAD索引保持，旧证书不回写。采用证书`sync/evidence/lsu-sq4-owner0-production-adoption-20261106-r03/adoption.json`；正式指针`sync/evidence/current-production.json`；软件说明`docs/hardware-baseline-lsu-sq4-owner0.md`。
单候选授权已完成，全部综合／ABC／STA恢复暂停，未提交／推送；不自动继续实验／组合／综合或清理，无额外面积专项，不算完整新优化轮。

## 2026-11-05：正式采用Load触发下一行预取（优先于以下历史）

用户明确“采纳”，采用已审计`load-prefetch-ppa-20261105-r01`。面积34417.443067 μm²（+0.523014%），Fmax307.138572 MHz（-1.979604%），六项几何IPC1.035819497011（+2.458373%），评分2.984787940536e-07（+6.899400%）。IPC×Fmax估算+0.430103%，不是墙钟实测。面积<36000及Fmax>300通过，长期IPC>1.0985未达；towers+10拍，其他五项改善。
仅DCache＋backend两项RTL采用r02同源候选：真实接受普通RAM Load触发下一条32字节行，4项最近源行／单pending目标，最多一个typed内部预取，共享8请求／4MSHR／2bit ID。需求优先、入槽占用≤6／MSHR≤2；未占MSHR冲突可丢弃，已占MSHR保留脏victim／写回／回填所有权。全部CPU返回排除预取类型，不占LSU身份；退出接受后停止新预取。不改容量／宽度／BPU或Slave，不带已否决方案。
两软件10项构建输入同步，116项保持：DCache预取和CPU／MemArbiter原始proposal-valid接线；实际接受事件仍单独门控。主树只读旧快照/Input逐字段写目标，模板Register单写／统一sync保持，无模块下一状态复制或生产观察探针。全新Release／断言四构建原六答案／退休／init+core+drain匹配，两外部副本各374104条退休PC／周期匹配RTL；周期8025/17410/3971/5874/153886/159600。r01两软件rsort均少5拍被门禁拦住；外部状态见证定位满Cache时raw-valid被旧接受门控隐藏，r02仅修软件映射，旧失败／诊断保留，RTL／PPA不变。限定原六资格，不是完整状态或全输入形式等价。
44项RTL／公共SRAM及converted精确同源认证，复用已审计整核综合／ABC／STA和native／converted筛选，本次不新综合，也不冒称另建生产路径RTL模型。旧黄金／镜像／Slave／既有code和三树HEAD索引保持，历史证书不回写。采用证书`sync/evidence/load-prefetch-production-adoption-20261105-r02/adoption.json`，正式指针`sync/evidence/current-production.json`；软件说明`docs/hardware-baseline-load-prefetch.md`。
全部综合继续暂停，未提交／推送；不自动继续实验／组合／综合或清理，无额外面积专项，不算完整新优化轮。

## 2026-11-04：正式采用空FQ＋Store地址RS WB组合（优先于以下历史）

用户要求“采纳，并开始综合”，已在全新PPA及两软件资格通过后采用`fq-sa-combined-ppa-20261104-r01`。面积34706.754007 μm²、Fmax307.969925 MHz、原六项几何IPC1.010966177065、评分2.670710512892e-07。相对上一生产IPC+2.780824%、面积+0.223820%、Fmax-3.248120%、评分+7.489459%。IPC×Fmax估算-0.557621%（非墙钟实测）。排名不自动授权其他候选。
仅旧FQ／IQ均空时返回包直通译码，整组分配或整组进入IQ；宽前端proposal独立ready，真实上下文按实际接受出队，旧路径保持。旧Store地址RS使用已授权CDB值派发，真正获共享AGU接受才返还信用，新分配不得同拍派发。三项RTL变化，不加其他释放信用、已否决PRF排名／Store地址通知／回填字捕获／miss接续，不改BPU算法／表、容量、执行宽度、访存协议或Slave。
44项RTL及公共SRAM与已审计组合精确同源；本次同源全新整核综合／ABC／STA已完成并独立Liberty逐叶面积、约束、六阶段checkpoint及评分审计，不拼接旧PPA。资格来自`fq-sa-combined-screen-20261104-r01`局部native／converted两组oracle、四源码负向和原六项完整日志；不声称完整状态／全输入形式等价或另建生产路径RTL模型。
两软件同步空FQ单次消费与SA旁路，共13项构建输入变化、113项保持；主树只读旧快照/Input逐字段写目标，模板Register单写／统一sync保持。全新Release／断言四构建原六项答案／退休数／init+core+drain及外部副本退休PC／周期匹配。课程周期8605/17550/3985/5864/155719/168501。旧黄金、镜像、Slave、既有code及三树HEAD／索引保持。
面积<36000及Fmax>300通过，长期几何IPC>1.0985仍未达。此次为已筛选组合的PPA及采用，没有额外面积专项，不算完整新优化轮。单候选综合授权已结束，全部综合／ABC／STA恢复暂停，不自动启动其他实验。
正式指针`sync/evidence/current-production.json`；证书`sync/evidence/fq-sa-combined-production-adoption-20261104-r02/adoption.json`；软件说明`docs/hardware-baseline-fq-sa-combined.md`。以下记录保留历史归属。

## 2026-11-04：正式采用ROB Load身份行并行校验版（优先于以下历史）

用户明确要求“可以，采用”，已采用`rob-load-identity-ppa-20261104-r01`。面积34629.246727 μm²、Fmax318.308984 MHz、六项几何IPC0.983613617806、评分2.484625496498e-07；相对上一生产IPC+0.000000%、面积-0.273049%、Fmax+5.377681%、评分+5.955512%。IPC×Fmax估算+5.377681%（不是墙钟实测）。本版为已审计评分最优、最高IPC并列中评分优胜者；排名不自动授权后续采用。
仅`rv32_rob_lanes.sv`独立Load lookup valid改为各行并行完整身份检查后归约；仍检查stored tag低位与常数row、query slot及高位epoch，非split输出全零。ROB全部状态写入和其他输出保持，无新流水级／接口／容量／执行宽度变化，不带入已否决PRF排名方案，不改PRF、RS信用、写回旁路、BPU、访存协议或Slave。整组IQ旁路、非MUL／DIV真实离站信用、前端恢复安全鉴权和后端冻结均保持。
44项RTL与公共SRAM同已审计候选逐字节认证；复用已有整核综合／ABC／STA，本次采用不重跑PPA。模型资格来自`rob-load-identity-screen-20261103-r02`的native／converted组合切片、实际ROB队列oracle、三个源码负向及原六项完整日志，测量根单独归属20261104；不声称另建生产路径RTL资格模型或完整状态／全输入形式等价。旧20261103中断展开记录保留，不冒称已完成或checkpoint续算。
此为RTL组合拓扑等价改写，两软件全部126项构建输入保持，无需改算法。两软件全新Release／断言四构建原六项答案／退休数／init+core+drain及外部观察副本退休PC／周期匹配；课程周期9090/18228/3994/5968/163977/168556。主树旧快照/Input纪律与模板Register单写／统一sync保持，不恢复生产观察探针。旧黄金、镜像、Slave、既有code及三树HEAD／Git索引保持。
面积<36000与Fmax>300通过，长期几何IPC>1.0985仍未达到。本次是既有已测候选采用，不算新优化轮；全部综合／ABC／STA继续暂停，不自动继续新候选。历史指标与否决记录保留，不伪称已清理历史源码／模型重新可用。
正式指针`sync/evidence/current-production.json`；证书`sync/evidence/rob-load-identity-production-adoption-20261104-r01/adoption.json`；软件说明`docs/hardware-baseline-rob-load-identity.md`。以下记录保留各自历史归属。

## 2026-11-03：正式采用非MUL／DIV全RS信用旁路版（优先于以下历史）

用户在知悉频率／评分代价后要求“采纳到生产”，已采用`rs-credit-reuse-ppa-20261103-r01`。面积34724.060467 μm²、Fmax302.064897 MHz、六项几何IPC0.983613617806、评分2.344970490776e-07；相对上一生产IPC+0.169731%、面积-0.275521%、Fmax-2.654867%、评分-1.449979%。IPC×Fmax估算-2.489642%（不是墙钟实测）。本版是已审计最高IPC候选，不是评分最优版；旧评分最优指标与审计记录保留，历史源码／仿真模型／网表缓存按用户清理授权处理，不承诺仍完整存在。
仅Store地址RS补开实际离站信用同拍复用；整数／Branch／Load／Store数据RS原已开启并保持，MUL／DIV仍不复用。共享AGU必须实际选中并接受旧Store地址指令才能返还SA信用，不能把Load获胜或仅有ready候选算作SA离站。新分配写入优先于旧释放，不允许新项同拍派发；不增加写回当拍数据旁路，Store地址RS仍只看旧就绪。
两软件默认四宽同步同一实际AGU离站信用；仅主树StaticArbiter与模板CPU功能接线变化，两个RS文件只更新注释。主树只读拍初快照/Input逐字段写目标，模板Register单写／统一sync保持；无模块下一状态复制。不改容量、执行／写回／退休宽度、BPU算法／表、存储协议或Slave，不恢复已否决Store地址通知Load数据、回填字捕获、DCache miss接续或第28轮方案。原整组IQ旁路、前端恢复安全鉴权与后端冻结保持。
44项RTL及公共SRAM与已审计候选逐字节认证，复用已完成的整核综合／ABC／STA，本次不重跑综合，也不声称另建生产路径RTL模型。模型资格来自`rs-credit-reuse-screen-20261103-r02`的native／converted、七池列表oracle、共享AGU及三个有效负向；PPA归属独立测量目录，不冒称完整状态／全输入等价。
两软件全新Release／断言四构建原六项答案／退休数／init+core+drain和外部观察副本退休PC／周期匹配；课程周期9090/18228/3994/5968/163977/168556。采用阶段与缓存清理互斥；旧黄金、镜像、Slave、既有code和三树HEAD／Git索引保持；当前验收使用专用生产检查器，不套历史黄金周期。
面积<36000和Fmax>300通过，长期几何IPC>1.0985未达。此次是已测候选采用，不算新优化轮次。所有综合／ABC／STA继续暂停，不恢复第28轮或自动继续优化；历史指标索引只重绑定新生产，不伪称已删历史源码／模型仍可复核。
正式指针`sync/evidence/current-production.json`；证书`sync/evidence/rs-credit-reuse-production-adoption-20261103-r01/adoption.json`；软件说明`docs/hardware-baseline-rs-credit-reuse.md`。以下数字保留各自历史归属。

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
