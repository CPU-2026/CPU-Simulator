# 前端

2026-10-26模板四宽：`CPU_DECODE_WIDTH=1/2/4`，`CPU_ISSUE_WIDTH=1/2/4`独立配置，
`CPU_RENAME_WIDTH`为发射宽度兼容别名。四宽示例是fetch2/decode4/issue4/commit2，FQ/IQ默认各8项；
计数载体可表示0..4，所有lane保留独立PC/预测PC/CID，IQ只消费旧态有序前缀，无译码到发射直通。
双取供给不变，依靠积压才能同拍译码四条。默认1/1/1/2及主树实现不改。
本节优先于下方历史“旧CPU_ISSUE_WIDTH仅前端兼容”的说明，验证边界见[同步记录](../../../sync/TEMPLATE-WIDE4-20261026.md)。

后续RTL双rename已完成：与模板一样消费旧IQ的0/1/2条有序前缀。固定fetch/decode/commit=2的单/双rename各25课程＋40定向通过独立RTL退休ISA，课程周期/退休数与模板相等；不是主树双发射或全内部逐拍证明。见[RTL移植记录](../../../sync/RTL-DUAL-RENAME-20261025.md)。

2026-10-25：新增`CPU_RENAME_WIDTH=1/2`，默认单发射。双发射只消费拍初IQ中存在的顺序前缀，第二条有效要求IQ至少2项；本拍IQ入队空间按真实issuePopCount（0/1/2）计算。无本拍译码到发射直通，见[接线记录](../../../sync/DUAL-RENAME-WIRING-20261025.md)。下面历史主树/RTL周期结论仅覆盖单发射。

当前生产默认I$请求4、AXIFetch4、BTB32、RAS8；模板BTB tag位宽/位移随容量派生。1/1/2默认与2/1/2、2/2/2可选档均各25例与主树及对应RTL结果/退休/core/total对齐；双取新packet、逐指令CID和恢复期安全hit提前取指已合入。

与主树采用相同配置和时序，统一见[前端设计](../../docs/frontend.md)。
默认取指/译码/发射单宽，取指与译码可用独立参数配置1/2；FQ/IQ默认各为译码宽度两倍，可分别覆盖。双取默认I$2包/4字预算，每条独立CID且两CID保存同一旧GHR；不再使用旧共享CID算法，见[实施记录](../../../sync/WIDTH-DECODE-SYNC-20261024.md)。双提交的已提交RAS顺序折叠两个pop/push事件，SRAM同槽年轻写胜出。
模板专有规则见[框架指南](help.md)；旧版原文见[归档](_archive/20261018/docs/frontend.md)。
