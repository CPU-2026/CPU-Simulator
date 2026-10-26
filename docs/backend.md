# 后端

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
