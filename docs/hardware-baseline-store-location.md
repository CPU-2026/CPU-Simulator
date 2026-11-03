# 第19轮 Store 位置缓冲生产基线

用户选择`round19-store-location-20261101-r02`，硬件采用与两软件复验已完成。
共同合同、指标、六项周期和复验命令见[主树基线](../../docs/hardware-baseline-store-location.md)。

两项有效行地址/way记录只训练真实SRAM Load命中，miss分配失效同set记录；
合格队首Store用旧位置直接写原端口。训练选槽、Store准入、way与PLRU均对齐RTL；
位置表状态属于`DCacheInner`，每Register条件单写、全部模块work后统一sync。

原六项课程周期为`9259/19595/4363/6411/178930/175811`，比较`init+core+drain`。
四个DCache构建输入变化，其余122项输入与第18轮身份相同。
面积34734.558067 μm²、Fmax307.599880 MHz、几何IPC0.923231817701；
相对第18轮IPC+2.139733%、Fmax−3.214178%、评分+4.629363%，
按各自Fmax估算IPC×Fmax−1.143221%。长期IPC>1.0985尚未达到。

正式证据位于工作区`sync/evidence/`：

- `store-location-production-adoption-20261101-r01/adoption.json`
- `software-store-location-sync-20261101-r01.json`

构建、源码副本和trace位于工作区`.cpu2026-synthesis/software-store-location-sync-20261101-r01/`。
两树全新Release/断言四构建的原六项答案、退休数、课程周期全部匹配；
模板断言构建启用`_DEBUG -O2 -UNDEBUG`，外部观察副本各374104条退休PC/周期逐条匹配RTL。
主树24 ticks＋58 helpers＋CPU clock及负向纪律检查通过；r26最优归档绑定本生产。
旧黄金表和已有`code`保留，使用共同说明中的新构建及专用六例入口。
