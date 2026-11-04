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

## 2026-11-04：正式采用ROB载荷复位分离（优先于以下历史）

用户要求“采纳ROB 复位分离”，仅采用已审计`rename-area-ppa-sequence-20261104-r01/rob-payload-reset`。面积34238.371507 μm²（-1.349543%），Fmax313.341493 MHz（+1.744186%），六项几何IPC1.010966177065（不变），评分2.792146575072e-07（+4.546957%）。IPC×Fmax估算+1.744186%，非墙钟实测。
仅替换`rv32_rob_lanes.sv`：载荷raw不复位，每行initialized复位并屏蔽未初始化读出；valid／ready／完整tag控制复位及写优先级保持。initialized不跟随valid清除，保留取消项replay和失效端口载荷。不带PRF或ROB SRAM候选，不改容量／宽度／BPU／访存协议。现有frontend格式及等宽截取修复原样保留并记录旧指针差异。
44项RTL／公共SRAM与已审计候选同源，复用该候选已完成整核综合／ABC／STA及native／converted局部与原六资格；本次不新综合、不声称另建生产路径RTL模型或全状态形式等价。两软件126项构建输入不变，无需改算法；全新Release／断言四构建原六答案／退休／init+core+drain及外部副本各374104条退休PC／周期匹配。周期8605/17550/3985/5864/155719/168501。旧黄金／镜像／Slave／既有code和三树HEAD／索引保持。
面积<36000及Fmax>300通过，长期IPC>1.0985未达。面积／频率合格候选中评分最优，不是无面积限制的最高IPC或最高评分者；历史排名／否决保持。采用证书`sync/evidence/rob-payload-reset-production-adoption-20261104-r01/adoption.json`；正式指针`sync/evidence/current-production.json`；软件说明`docs/hardware-baseline-rob-payload-reset.md`。
全部综合／ABC／STA继续暂停，未提交／推送；不自动继续新实验／组合／清理，不算完整新优化轮。

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

# 模板状态

2026-11-02第21轮：用户要求“生产采纳最优结果”，已采用无冲突Load接受沿提前查询r02并同步两树。3项DCache构建输入变化、123项保持，主树24 ticks＋58 helpers纪律通过；全新Release/断言四构建原六项答案/退休/课程周期全匹配，外部副本各374104条退休PC/周期匹配RTL。面积34587.314647 μm²、Fmax308.805790 MHz、几何IPC0.930507527921，评分+0.717984%；median退化0.846006%、Fmax−2.412545%、估算吞吐−1.643488%的代价已披露。详见[当前基线](hardware-baseline-disjoint-load.md)。下方记录为对应历史版本。

2026-11-01第20轮：用户要求“应用到生产”，已采用PRF共享写仲裁r02。两软件126项构建输入逐字节保持，
全新Release/断言四构建原六项答案、退休数、课程周期全匹配。面积34590.361867 μm²、Fmax316.440049 MHz、
几何IPC0.923231817701，详见[当前基线](hardware-baseline-prf-global-write.md)。下方为历次记录。

2026-11-01第19轮：用户要求“保存，作为生产”，已采用两项Store位置缓冲r02并同步两树DCache。位置表只训练真实Load命中，miss分配失效同set；合格队首Store复用旧way直接写，PLRU优先级同步。四个DCache构建输入变化、其余122项保持；全新Release/断言四构建原六项答案/退休/课程周期全匹配，两个外部观察副本各374104条退休PC/周期逐条匹配RTL。主树24 ticks＋58 helpers纪律通过；详见[当前基线](hardware-baseline-store-location.md)。

2026-11-01：用户要求“替换生产代码，并同步软件”，已同步正式采用第17轮。PRF用63个单行SRAM、everWritten及7条上一拍写入记录，保留所有分配PC+4写及未ready值，冻结阶段继续读；同步BPU置信度选择和D$同组其他way命中/其他Load字捕获/脏victim写回期间提前返回。隔离候选与生产126项构建输入逐字节一致，四构建原六项答案/退休/课程周期全匹配，411373拍全部64个PRF ready/value及374104条退休PC/周期三端一致。主树24 ticks＋58 helpers纪律通过；完整证据及范围见[第17轮基线](hardware-baseline-prf-microbank.md)。

2026-10-31：生产AXIData关键字先读/寄存字返回、D$整行单拍填充及StoreValue非控制ALU旁路/离站复用已同步。最初只移植Cache/AXI时四例周期有差；将真实StoreValue事件贯通SQ/LQ/RS/Issue后，两树全新Release和断言构建的原六例答案/退休/课程周期全部匹配正式采用RTL。源码和失败证据均保留，见[验收](hardware-baseline-word-production.md)。

2026-10-30：已同步生产双取指2/4/4/4、双ALU/三WB；恢复安全包边界与旧返回包信用准入已对齐。原六perf真实退休/分段周期与主树相同，438378拍11项观察与native RTL一致；保留旧计时口径并显式核对boot区间。最终生产复验见[当前基线](hardware-baseline-fetch2.md)，不扩大为完整状态等价或PPA。

2026-10-29：主树按原comb/tick快照风格完整同步模板生产功能：Tournament预测时上下文、8 KiB D$、
接受沿同步读、Load RS授权WB/实际离站复用、独立1/2/4宽度与全分配消费者前缀。
默认两树断言构建25例严格匹配既有模板表，8冒烟＋39定向/固定seed随机程序同值同退休同周期；
7档各25课程＋8冒烟均同值/同退休/同分段周期（含2/4/4/2、4/4/4/4）；最新补验与证据统一见[同步记录](../../docs/tree-sync-20261029.md)。
本模板功能源码不因反向移植修改，历史采用报告身份保持。

2026-10-27双向同步r02：模板补齐Load RS授权WB就绪/值及issue1/2实际离站复用，issue4原复用保留；
只改CPU/StaticArbiter头及实现三个文件，StoreAddr/StoreValue/MUL/DIV、D$/AXI/Slave源码不改。
隔离四档各52镜像normal/shuffle共416次、r01 issue2基线另104次独立ISA/消费者oracle通过；
真实CPU→实际RTL两池RS+共享AGU，三种issue宽度原生/sv2v共168次、四项源码负向通过。
新增8个纯指令程序四档normal/shuffle共64次及48次RTL重放，四档均实际覆盖四类WB基址生产者。
已写入生产，四档各60镜像normal/shuffle共480次重编复验摘要与隔离候选精确一致；
默认Release/Assert各25例普通入口及各25例严格表校验通过，新模板表为生产实测；历史r01表保留备份。
软件六perf几何IPC收益约0.156%/0.276%/0.789%/0.809%，包含rsort变慢等不利个例，非RTL性能。
记录与当前范围见[双向记录](../../../sync/MUTUAL-OPT-SYNC-20261027.md)。

2026-10-27双向同步r01：将RTL空闲Load接受沿同步SRAM读及offset/size快照移植模板，
只改DCache头/实现两文件；RTL和参考主树源码、既有code及主树基准表保持。
原生/sv2v正向各12431拍、92种offset/size暖命中严格1拍；原模板/丢接受读/错size三项负向均有效。
默认/双取/既有四宽各52镜像normal/shuffle共312次独立ISA通过；生产三档重编后再312次摘要精确匹配。
默认Release/Assert各25例普通入口及各25例严格周期表校验通过，模板表更新为生产实测值。
六perf几何IPC提升分别约0.0096%/0.0075%/0.5769%，但默认/双取vvadd均慢107拍，非逐例都加速。
不重标上一轮证书，不声称全核周期等价或新RTL PPA；后续Load RS及反向关键字早返回见
[双向记录](../../../sync/MUTUAL-OPT-SYNC-20261027.md)。

2026-10-27：按用户授权正式更新生产模板与RTL的真实 Tournament BPU及8 KiB D$。
BPU保存预测时局部历史/分量决定，D$只扩组数64→128，既有后端宽度与AXI/Slave不变。
前置隔离证书分别完成50镜像/400次及52镜像/416次整核检查，另含原生/sv2v及真实源码负向。
生产路径重编及复验均已通过：模板312次/RTL104次独立ISA、默认Release/Assert各25例严格表校验，
生产预测器及原生/sv2v回归、标准入口冒烟通过；采用后报告单列，不重标前置证书。
参考主树源码与原周期表保持；模板维护独立默认实测表。见[采用记录](../../../sync/ADOPT-BPU-DCACHE-20261027.md)。

2026-10-26：模板同步RTL四译码/四路顺序发射及配套ROB32/PRF64/checkpoint64、FQ8/IQ8。
旧单/双宽静态路径保留；新四路组合计划按实际前缀排名，最近写者RAW/LINK、WAW链和LSQ恢复边界贯通。
Register单写、统一sync、现有执行/Cache/AXI端口和默认配置保持；不再将`CPU_ISSUE_WIDTH`解释为前端旧宏。
本轮新验收与尚未证明的范围单列在[同步记录](../../../sync/TEMPLATE-WIDE4-20261026.md)，不复用下方旧报告冒充四宽结果。
新持久r03已完成：默认25例严格golden＋8冒烟、双宽六perf＋8冒烟、四宽25课程结果/退休检查；
真实四宽CPU26例正常/shuffle共52次同值同拍同事件，观察到decode4=7/issue4=12、ROB峰值29。
重启前轻量检查仅留历史数值，改前双宽快照已丢失，未伪造旧新双宽差分报告；持久报告见同步记录。
2026-10-27收尾修复四宽legacy HALT的ROB结果rd，实际42→0复现后补正；只有一个目的选择表达式和注释变更，
非legacy正常路径有精确受限源码桥接，未重复完整课程；最终全量重编及5项HALT/退出定向通过，最终证书单独保存。

2026-10-25后续：双路顺序重命名及双Load/双Store分配已移植RTL，默认rename仍1，保留单ALU/单AGU/两CDB。RTL rename1/2各25课程＋40定向独立退休ISA通过，课程core/total/退休数与模板逐例相等；单路还与移植前冻结RTL周期/AXI＋退休轨迹hash相等。两项recovery4失败已在冻结快照同样复现，未放宽断言；无新PPA结论。见[RTL移植验收](../../../sync/RTL-DUAL-RENAME-20261025.md)。下方“RTL不改”仅指模板实现阶段。

2026-10-25补验：固定fetch/decode/commit均2，仅切换rename宽度1/2；普通Release与Assert四构建每档25课程＋40定向＋8冒烟全部通过，同档Release/Assert结果/退休/分段周期严格相同。仅双发射带来的25例周期减少4.2058%，六perf减少7.4536%，六perf几何IPC0.660200451413→0.735362160697；231份生产输入/镜像/golden/code摘要不变。未加第二个ALU，默认和golden保持，详见[单变量对照](../../../sync/DUAL-RENAME-AB-20261025.md)。

2026-10-25：模板独立`CPU_RENAME_WIDTH=1/2`已接通，默认仍1。支持双Load/双Store、组内RAW/WAW、两路RS/ROB/PRF/RAT及正确LSQ恢复边界，IQ按实际出队数腾位。改前/改后单宽/双宽断言三构建40定向＋25课程通过，单宽结果/退休/分段周期严格不变；双宽真实CPU正常与shuffle共80次独立退休检查通过，观察到双Load76/双Store92次。主树、RTL、Slave、仓库code和golden不改，范围见[接线记录](../../../sync/DUAL-RENAME-WIRING-20261025.md)。下面三端同步结论仍只覆盖单发射。

可选双取指已合入两软件：packet按I$控制流mask/8B对齐/最早CFI截断，单口查询所选CFI，每条独立CID且两CID保存同一旧GHR；恢复期仅对当前PC非控制流hit开放前端，后端冻结。2/1/2与2/2/2各25例与真实RTL逐例结果/独立退休ISA/退休数/core/total精确相等；六perf IPC分别0.663320349025/0.660200451413。两档各3例关键事件全运行段MATCH，两树实际CPU CID回绕/无活ID重用/旧GHR/恢复资格及共8份源码负向通过。默认仍1/1/2，候选默认兼容性25例及8冒烟通过；合入后重编生产CPU消费者的三档各8冒烟和默认25例严格golden也全部通过，期间源码/golden摘要不变。外部Slave和仓库code保持。

当前默认单取指档已合入并与主树及真实RTL 1/1/2完成全部25例结果、独立RTL退休ISA、退休数/core/total一致。I$请求4、AXIFetch4、BTB32、RAS8；恢复按原存活ROB窗口终止，省略空beat。六perf IPC0.641865180802；vvadd/median/qsort主树与RTL完整运行段的已选关键事件MATCH。新默认golden严格复核25/25通过，期间126份生产输入与golden摘要不变。以下Load A/B/C数值是各阶段历史，不能代替当前值；双取指两档仍在隔离副本验收。

Load B/C已合入：旧AGU对预留活动LQ行提前请求，完整SQ转发排除冗余读；合格Cache返回同拍参与原共享CDB，受阻正常捕获、授权只广播一次。B/C分别两树25/25＋8冒烟，真实CPU提前请求/完整转发/返回当拍广播及4份源码负向通过；两树模块专项含跨epoch年龄、错身份/取消、符号扩展、较老MUL争用后真实LQ捕获与下一拍只广播一次。当前六perf IPC0.600667659268；不是完整三端等价。

Load A已合入：同步SRAM旧Load hit返回可与下一lookup重叠，Fill/Store优先和读旧态保持。默认两树断言25/25、8冒烟、实际CPU各14次重叠及两份串行化源码负向通过；新golden严格复核通过，六perf IPC0.580338190880。AGU提前请求/返回当拍CDB与前端同步仍待完成。

RS阶段已合入：Integer/Branch各4项、两路授权WB旁路、满站离站复用，保持Register单写/统一sync。默认两软件断言构建25/25结果/退休/周期一致，原表留历史，新表按实测更新；专项/负向及仍未同步的RTL部分见[RS验收](../../../sync/RS-RTL-TO-SW-20261024.md)。

2026-10-24新增独立`CPU_FETCH_WIDTH`/`CPU_DECODE_WIDTH`及FQ/IQ容量控制，默认保留原配置；旧`CPU_ISSUE_WIDTH`仅兼容前端设置，不表示双发射。RTL已加入可选真实双译码，但模板旧双取/共享CID算法及后端仍待按RTL同步，历史逐拍结论不覆盖当前工作区。实现与阶段验收见[实施记录](../../../sync/WIDTH-DECODE-SYNC-20261024.md)。

模板迁移已完成，当前为全模块`work()/sync()`及跨模块Wire接线；不再混用Register与memcpy快照。
最新RTL等价移植包括AXI主机8项信用、四槽恢复窗、D$共享取数、LQ raw读出扩展和字节重叠化简。

2026-10-20新增双提交：ROB统一输出连续前缀，RAT/PRF/LQ/SQ/RAS批量消费，保持Register单写与统一sync。
两软件Release/断言四构建25例同值同退休同周期，单宽25例严格兼容旧golden；
主树对最终RTL25例逐拍/独立ISA、8例冒烟、21例定向及RTL专项通过。
本轮不声称模板对RTL全内部逐拍等价，范围见[双提交记录](../../../sync/DUAL-COMMIT-20261020.md)。
此前四软件构建及主树/模板分别对单提交RTL的25例逐拍/独立ISA通过，属于修改前记录。
结果与边界统一见[同步状态](../../../CPU-SYNC-PLAN.md)及[验收记录](../../../sync/RTL-TO-SW-SYNC-20261018.md)。

[历史迁移记录](_archive/20261018/docs/progress.md)保留原文，不继续在入口累加重复日志。
