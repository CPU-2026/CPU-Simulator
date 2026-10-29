# 前端

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
