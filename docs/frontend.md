# 前端

当前生产默认I$请求4、AXIFetch4、BTB32、RAS8；模板BTB tag位宽/位移随容量派生。1/1/2默认与2/1/2、2/2/2可选档均各25例与主树及对应RTL结果/退休/core/total对齐；双取新packet、逐指令CID和恢复期安全hit提前取指已合入。

与主树采用相同配置和时序，统一见[前端设计](../../docs/frontend.md)。
默认取指/译码/发射单宽，取指与译码可用独立参数配置1/2；FQ/IQ默认各为译码宽度两倍，可分别覆盖。双取默认I$2包/4字预算，每条独立CID且两CID保存同一旧GHR；不再使用旧共享CID算法，见[实施记录](../../../sync/WIDTH-DECODE-SYNC-20261024.md)。双提交的已提交RAS顺序折叠两个pop/push事件，SRAM同槽年轻写胜出。
模板专有规则见[框架指南](help.md)；旧版原文见[归档](_archive/20261018/docs/frontend.md)。
