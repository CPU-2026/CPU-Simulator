# 第21轮无冲突Load提前查询生产基线

用户要求“生产采纳最优结果”，正式采用`round21-accepted-disjoint-load-20261102-r02`。
完整合同、六项基线、指标权衡与F盘构建说明见[共同生产基线](../../docs/hardware-baseline-disjoint-load.md)。

- 面积34587.314647 μm²、Fmax308.805790 MHz、几何IPC0.930507527921、评分1.935224301924e-7。
- 相对第20轮IPC+0.788070%、Fmax−2.412545%、评分+0.717984%；median IPC−0.846006%，估算IPC×Fmax−1.643488%。
- 无旧lookup/fill/Store写、读流水可用、所有旧Store不同行/非退出MMIO、目标set未锁定时，接受沿启动真实同步读。不是跨bank同拍读写。
- 事件计划先求旧工作，再求接受沿资格，避免递归events()；Register单写/统一sync保持。
- 两软件3项DCache构建输入变化、123项保持；四个全新Release/断言构建原六项全匹配，两个外部副本各374104条退休PC/周期匹配RTL。
- 六项周期9338/19591/4278/6317/177506/173548；[软件证书](../../../sync/evidence/software-disjoint-load-sync-20261102-r01.json)。
- 第17轮完整PRF观察保持历史归属。旧黄金、镜像、Slave、code和两Git索引保持；只验原六perf。
