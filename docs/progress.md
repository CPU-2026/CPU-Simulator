# 模板状态

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
