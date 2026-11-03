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

# 模板框架指南

**2026-11-02第21轮当前**：Load不同行Store绕过/接受沿查询已生产采用，事件计划只读旧Register/Input，先选旧工作再算新请求读授权，不递归调用events()；每个SRAM每拍work一次并统一sync。两软件全新Release/断言四构建原六项全匹配，外部副本各374104条退休PC/周期匹配RTL；F盘构建及专用验收入口见[当前基线](hardware-baseline-disjoint-load.md)，旧黄金表保持历史。

**2026-11-01第20轮当前**：PRF共享写仲裁r02已正式采用。两软件126项构建输入身份保持，全新Release和
`_DEBUG -O2 -UNDEBUG`四构建原六项结果/计数全匹配。F盘外部构建及验收入口见[当前基线](hardware-baseline-prf-global-write.md)。
下方PRF逐拍及逐条退休观察属于当时版本。

**2026-11-01第19轮当前**：两项Store位置表状态进入`DCacheInner`自动sync；训练/同set失效在每个Register上互斥单写，Store使用旧位置，PLRU按fill→直接Store→read hit。两树全新Release及`_DEBUG -O2 -UNDEBUG`四构建原六项全匹配，外部观察副本各374104条退休PC/周期逐条一致。构建路径、六项周期及复验入口见[当前基线](hardware-baseline-store-location.md)。

**2026-11-01当前**：已同步第17轮PRF单行SRAM、BPU置信度选择及D$同组命中/返回字捕获。PRF嵌套SRAM每拍work一次，冻结时仍持续读并清有效写历史，统一sync全部Wire/Register。两树Release及`_DEBUG -O2 -UNDEBUG`四构建原六项答案/退休/课程周期匹配RTL；当前F盘构建、正式六项验收入口与三端PRF证据见[新基线](hardware-baseline-prf-microbank.md)。下方旧默认及第10轮数字仅为历史。

**2026-10-31当前**：StoreValue使用非控制ALU授权结果旁路，SQ捕获/LQ通知/RS释放/Issue复用共享接受事件；禁止在该组合路径求共享CDB，以避免Load反馈环。Register仍单写、统一sync。两树Release/断言原六perf全匹配，当前构建和验收见[同步记录](hardware-baseline-word-production.md)。

**2026-10-30当前**：默认2/4/4/4、双ALU/三WB，两个ALU和第二CDB注册并统一sync。当前用[README](../README.md)专用六例入口，旧表/脚本属于历史1/1/1/2。软件旧total不含init=1，课程区间为init+core+drain；详见[基线](hardware-baseline-fetch2.md)。下方旧默认及全量回归命令不作为本轮验证指令。

**2026-10-29**：主树已同步模板生产功能与独立1/2/4宽度；默认两树25例严格周期一致，主树首表与本模板表一致。
FQ默认取指宽度两倍、IQ默认译码宽度两倍；2/4/4/2为FQ4/IQ8，下文旧FQ8说明为历史。
当前同步/验证范围见[记录](../../docs/tree-sync-20261029.md)；历史独立采用证书不重标为本轮验收。

2026-10-27双向同步r02已写入三个模板功能文件，生产路径复验通过；状态与二进制位置以
[双向记录](../../../sync/MUTUAL-OPT-SYNC-20261027.md)为准。新默认表已按生产普通入口实测确认，
不混用r01旧code/旧表；仓库已有code及参考主树表不覆盖。

2026-10-27双向同步r01已写入模板DCache两文件，生产路径复验通过；当前状态及二进制位置以
[双向记录](../../../sync/MUTUAL-OPT-SYNC-20261027.md)为准。已按普通入口实测更新本模板表，
不修改主树表，不覆盖仓库已有`code`，不把旧二进制与新周期表混用。

2026-10-27：生产源码已采用真实 Tournament BPU＋D$8 KiB，默认宽度仍1/1/1/2。
参考主树未采用，本模板课程模式使用自己的[`benchmarks.md`](benchmarks.md)，不再默认读取主树周期表。
`BP_BIN=/path/to/new/code ./test.sh`与`./test_IPC.sh`会自动选此表；直接运行公共脚本时使用
`python3 ../data/course.py test --binary /path/to/new/code --data data/course-testcases --golden docs/benchmarks.md`。
仓库已有code未覆盖，必须重新构建后通过BP_BIN指定新二进制；生产复验状态见[采用记录](../../../sync/ADOPT-BPU-DCACHE-20261027.md)。

构建/课程测试见[README](../README.md)，公共架构见[主树](../../README.md)。
长用例使用Release；断言版务必同时启用`_DEBUG`并取消`NDEBUG`，不要覆盖已有仓库`code`。
默认双提交；`-DCPU_COMMIT_WIDTH=1`可构建单提交，但新RS配置须单独验证周期，不能套旧RS单提交golden。参数既可作为C++编译宏，也支持同名CMake cache。
本轮新表仅覆盖默认双提交。单提交尚无本轮实测周期表，不能套用历史单提交表或仅改测试参数冒充当前配置验收；
测试参数不会修改二进制的编译宽度。

## 基本类型

2026-10-26起`CPU_ISSUE_WIDTH`是后端顺序分配宽度，允许1/2/4；`CPU_RENAME_WIDTH`保留为完全同义别名，
同时指定但不相等会编译失败。此规则覆盖下方历史“旧CPU_ISSUE_WIDTH仅前端”的说明。
四宽独立构建示例：

```bash
cmake -S . -B /tmp/opencode/template-four -DCMAKE_BUILD_TYPE=Release \
  -DCPU_FETCH_WIDTH=2 -DCPU_DECODE_WIDTH=4 -DCPU_ISSUE_WIDTH=4 -DCPU_COMMIT_WIDTH=2 \
  -DCPU_RUNTIME_OUTPUT_DIRECTORY=/tmp/opencode/template-four \
  -DCMAKE_CXX_FLAGS=-D_DEBUG -DCMAKE_CXX_FLAGS_RELEASE='-O3 -UNDEBUG'
cmake --build /tmp/opencode/template-four -j2
```

四宽默认ROB32/PRF64/checkpoint64；FQ/IQ按fetch/decode分别派生，默认单发射仍ROB16。
decode4/issue1或2须显式缩小FQ/IQ（例如各4项）以满足checkpoint32预算，不能截断CID或放宽预算断言。
四宽不能套单发射golden周期，结果和独立退休数必须校验；可复用`sync/check_template_wide4.py`和
`sync/check_template_wide4_cpu.py`，报告均输出到仓库外，真实观察器只打开外部副本CPU访问标签。

后端双重命名使用独立配置`-DCPU_RENAME_WIDTH=2`（默认1），不是旧前端`CPU_ISSUE_WIDTH`。例如独立构建：

```bash
cmake -S . -B /tmp/opencode/rename-build -DCMAKE_BUILD_TYPE=Release \
  -DCPU_FETCH_WIDTH=2 -DCPU_DECODE_WIDTH=2 -DCPU_RENAME_WIDTH=2 \
  -DCPU_RUNTIME_OUTPUT_DIRECTORY=/tmp/opencode/rename-build/bin \
  -DCMAKE_CXX_FLAGS=-D_DEBUG -DCMAKE_CXX_FLAGS_RELEASE='-O3 -UNDEBUG'
cmake --build /tmp/opencode/rename-build -j4
```

双宽支持双Load/双Store入队，但不增加AGU/Cache/CDB带宽。周期会变化，不能拿单发射golden验收双发射周期；默认单发射必须保持改前结果/退休/分段周期。记录见[双重命名接线](../../../sync/DUAL-RENAME-WIRING-20261025.md)。

该逻辑随后已移植RTL：固定fetch/decode/commit=2时，单/双rename各25课程＋40定向独立ISA通过，课程core/total/退休数与模板相等；双rename不是主树支持项。RTL运行方式和既有失败边界见[移植记录](../../../sync/RTL-DUAL-RENAME-20261025.md)。

前端配置可直接用CMake cache：`-DCPU_FETCH_WIDTH=2 -DCPU_DECODE_WIDTH=2 -DCPU_COMMIT_WIDTH=2`，并可设置`CPU_FQ_ENTRIES`/`CPU_IQ_ENTRIES`。默认容量各为译码宽度两倍；仍单发射，旧`CPU_ISSUE_WIDTH`仅在未提供新宽度时兼容原前端设置。双取已同步RTL packet/逐指令CID/恢复期安全hit提前取指，2/1/2与2/2/2均25例三端周期相等；默认保持1/1/2，其他容量组合不能套用该结论，见[实施记录](../../../sync/WIDTH-DECODE-SYNC-20261024.md)。

Integer/Branch默认各4项；当前Integer/Branch/Load允许授权WB当拍派发及实际离站复用，StoreAddr/StoreValue/MUL/DIV不能借此读取新PRF值。

当前默认单取指档已与RTL 1/1/2完成25例结果/退休/core/total对齐，六perf IPC0.641865180802；旧RS/Load A/ABC整表保留。I$请求/AXIFetch/BTB/RAS容量参数分别是`CPU_FETCH_REQUEST_ENTRIES`/`CPU_AXI_FETCH_ENTRIES`/`CPU_BTB_ENTRIES`/`CPU_RAS_ENTRIES`，默认4/4/32/8。其他宽度须分别验收，不能套用本档golden；Slave服务和退出口径不变。

| 类型 | 语义 |
|---|---|
| `Register<N>` | 读旧值，`<=`写下一值；每拍最多写一次，sync提交 |
| `Wire<N>` | 只接线一次；本拍懒求值并缓存，sync清缓存 |
| `Bit<N>` | 定宽组合中间值，不是跨周期状态 |
| `Module<In, Out, Inner>` | 输入、输出与内部状态；work求下一状态，sync递归同步成员 |
| `dark::CPU` | 调度模块work，随后统一sync |

位宽上限32 bit；宽SRT状态拆lo/hi。同步数组使用`std::array`，聚合反射上限64个直接成员。
Register零初始化；非零复位状态通过隔离boot建立，不能靠构造赋新值绕过同步边界。
批量退休也不能对同一Register重复赋值：RAT/RAS先合并掩码和年轻写优先级，
PRF批量回收用局部FIFO游标，LQ/ROB分别计算最终head后写一次。

## 接线与更新纪律

```text
读取旧Register/Input Wire → 组合谓词与写意图 → work单点写Register
所有模块work结束 → sync所有Register/Wire → 下一拍
```

- 模块继承自己的Input，接线写在模块实例上；不要另建同名Input对象。
- 输出Wire在构造/`wire_output()`中接一次；不要在work里每拍重新接线。
- 多源更新先算最终写意图，再对每个Register写一次；相同值双写也违规。
- 禁止模块整对象复制、memcpy Register、读取其他模块本拍next-state作旁路。
- 无状态仲裁器使用Wire和空work，不添加寄存级。CPU裸Wire不受模块sync管理，不用于组合缓存。
- work用明确if/else表达优先级；helper读旧态，不用早退出隐藏写冲突。
- `Bit/Register`禁裸乘除；MUL用Booth/CSA，DIV用SRT，不能用宿主结果代替数据通路。

`_DEBUG`检查Register双写、Wire重复接线和未接线读取。需要测试观察时使用仓库外副本，不恢复生产Probe/Tester。

## 恢复与存储模块

四拍恢复只暂停前端/RS/FU/ROB/FlushArbiter的work；RAT/PRF/RAS继续重放，Cache/AXI/LSQ继续推进，
**全部模块仍sync**。不能跳过整个`run_once()`或只清运输模块的Wire。

ROB拥有共享四槽恢复窗的输出Wire；CPU只接旧恢复游标。D$组合缓存只存端口/事件，不能充当模块next-state。
嵌套SRAM由父模块每拍work一次，父sync递归提交输出并清Wire。
LQ内部value为raw，只有`getValue()`/CDB读出才扩展。

## 调试与参考

`VERBOSE=clock,branch,icache,dcache`输出到stderr。`reorder_test`已退役，不作为日常入口。
更改时序前先核对[共同规范](../../../CPU-SYNC-SPEC.md)，再跑[验证入口](../../../sync/README.md)。

完整API示例、历史迁移陷阱见[原指南](_archive/20261018/docs/help.md)；其中旧容量、HALT和过时命令不作为当前规则。
