# Cache与AXI

2026-10-26四译码/四发射不改本层：仍双取上限、单数据请求及原AXI信用/Slave服务；
扩大的是ROB/PRF/checkpoint和默认FQ/IQ容量，不扩大I$/D$/MSHR或访存端口。
不复用历史PPA或周期作为四宽验收，见[同步记录](../../../sync/TEMPLATE-WIDE4-20261026.md)。

后续双rename逻辑已移植RTL，仍未扩宽Cache/AXI/Slave；对应周期与独立ISA验收见[RTL移植记录](../../../sync/RTL-DUAL-RENAME-20261025.md)。旧PPA不能直接用于双rename。

2026-10-25：可选双Load/双Store是LSQ分配双宽，非双Cache端口；AGU、Cache、AXI、Slave及退出服务均未改。验证边界见[接线记录](../../../sync/DUAL-RENAME-WIRING-20261025.md)。

I$/D$几何、MSHR、共享取数、SRAM端口和回填合同统一见[Cache设计](../../docs/cache.md)。
双提交不拓宽Cache请求口、回填口或AXI信用，两个已提交Store仍在SQ按序逐拍发送。

译码宽度和FQ/IQ容量可以独立配置，但不自动改变Cache/AXI容量。当前默认I$请求4槽/AXIFetch4行，与D$的8请求槽分离；双取默认2包/4字预算及控制流mask已同步，两档分别验收25例通过，见[实施边界](../../../sync/WIDTH-DECODE-SYNC-20261024.md)。

Load A已同步旧Load hit响应/下一SRAM lookup重叠，排除当前readIndex并考虑响应后的最老请求；Fill > Store > lookup保持。两树默认档25/25、8冒烟及真实CPU重叠/串行化负向通过，不增加端口或修改回填信用。
Load B/C已接入旧AGU提前请求与返回当拍CDB，Cache端口和AXI信用不变；合法返回仍正常捕获，不能把物理响应冒充已授权广播。
模板组合事件缓存只保存本拍端口/事件，并在sync失效；旧版原文见[归档](_archive/20261018/docs/cache.md)。
