# 访存子系统：加载/存储队列 · 转发 · 保守准入

> **第12/13收口（SPEC §63/64）**：模块/22组Store严格门禁及15真实CPU3835/2872、
> 四负向、满队列/跨Cache/字节/恢复/最终128KiB通过。完整24镜像三CPU12667040
> 拍无损逐拍及退休/内存/分段周期全等，观察正确配LSU lane与不可变owner；README §56。

> **HALT外存完成（SPEC §62）**：已提交SQ先FIFO实际发送，旧Cache/DMEM结束后
> 全dirty写完才DONE；CPU顶层与DCache状态单写sync（顶层形式见§66）。16程序9629/2872及
> 最终128KiB独立核验/7负向通过，SQ head完整seq只读观察与flush期间park owner
> 资格已修正；访存4803/764保持，入口sync/README §54。

> **全流水恢复（SPEC §61）**：67真实CPU程序9001/2872严格全等，LSQ与全流水
> 同链；已提交SQ跨恢复、取消Load迟到返回、ROB/PHY复用通过，11源码负向有效。
> RTL修分配旁路P0资格，软件算法/统一sync保持，后端4803/764回归通过，README §53。

> **最新关联门禁（SPEC §60）**：BTB真实26程序6690/1848和参数边界通过；访存
> 后端4803/764＋六源码负向、RTL16与20＋60审计重新通过。第17当前域完成，
> 第18所有FU/LSQ全流水恢复继续，入口sync/README §52。

> **实际CPU身份/恢复（SPEC §59）**：完整真实CPU I$/IMEM/BPU/ROB/退休/恢复33程序
> 13041/1848三端严格同值，活CID不覆盖、年轻身份取消及栈覆盖值恢复通过。没有
> 给pop/push分别分配checkpoint，模板活动ROB窗口补nextTag；访存后端4803/764
> 及RTL16项通过，18 ISA保持、六IPC691640，入口README §51。

> **指令行边界（SPEC §58）**：实际IMEM16槽/20拍及I$与主树/真实SRAM RTL/参考
> 2324步160字段全等，前端至decoded IQ2259/216全等。逻辑恢复清请求身份，Cache
> 保留；配套物理总线适配器负责旧运输响应的消费/丢弃，不能把旧返回绑定新槽。
> 模板只读友元及统一sync保持，后端4803/764回归通过，README §50。

> **Store完成位图（SPEC §57/67）**：SQ组合把拍初活动、地址/数据齐且未提交项按ROB槽
> 译码ready/epoch位图，两条Wire<ROB_CAP>直送ROB；ROB核对旧有效Store/epoch及恢复年龄后批量置ready。
> 删除逐项SQ输入Wires，无本拍新字段旁路或额外寄存级，已提交Store不参与。历史严格764字段
> Cache联合对拍入口见sync/README §49，SQ退休/发送和统一sync边界保持。

> **当前返回接口（SPEC §55）**：模板DCache/DMEM正常6378/14时钟与主树/RTL/独立
> 参考一致。RTL LSU旧响应消费后同拍捕获下一读，pending身份不被旧返回清掉，真
> core/D$ full程序12次owner重填及实际旧气泡负向通过。模板仅加只读Cache观察友元，
> 统一work/sync保持；完整CPU联合轨迹和最终flush口径另验，入口README §47。

> **最新Cache准入修复（SPEC §52）**：接口统一为
> `SQ::canSendLoadToCache(addr, nBytes, loadTag)`，用解码后的1/2/4B实际宽度检查所有
> 更老未提交Store；未知地址或任意字节重叠阻塞未转发Load，同word不重叠可放行。
> 对应组合Wire改名loadCanSendToCache，Register单写及统一work/sync保持。四个迟到
> 数据覆盖反例及排空对照、两构建19个完整CPU镜像、18＋6严格回归通过，golden保持。
> RTL core12/12；22 suite两软件逐拍全等，原7组策略周期差保持。入口sync/README §44。

> **最新模板同步验收（SPEC §51）**：用户主树最终转发修复已迁移SQ/LQ及CPU接线。
> 通知实际nBytes用Wire<3>，队列Register<2>仍为0/1/2编码；地址通知判断本拍地址，
> 年轻候选按同word，事件广播仅同起点同宽度。查询接Load宽度，仅完整覆盖且数据就绪
> 才按offset右移；LQ单写intent按Load宽度截取/扩展。2443步323字段22 suite两软件
> 逐拍全等、三端架构全正确；两软件完整CPU Release/_DEBUG各11/11、RTL core8/8。
> 7组资格/覆盖策略周期差保持，完整三端周期尚未定版。LQ/SQ及四路/AGU回归通过，
> 两软件Release/_DEBUG各18＋6 strict-cycles保持活动clock11,967,396/690,814。
> 下面§46错值属于修复前记录。入口sync/README §43、差异文档§10。

> **最新转发现状（SPEC §46）**：模板与主树真实来源/转发链1338步323字段全等，
> RTL在word发生拍/逐Load扫描/未知地址覆盖/字节重叠上不同。模板intent也直接写完整
> Store value，SB→LBU/LB/LW字节错值正常DIVU整核复现；RTL相同七指令3/3正确，
> 内存路径及80 NOP排空控制正确。字节修复/共同策略待定，当前只观察、不改生产/golden。

> **当前SQ软件合同已同步RTL（SPEC §45）**：模板保持原SQ committed/发送pop单写，
> 同拍提交发送、满时下一拍复用。RTL独立FIFO/committed使已退休项跨ROB回绕/恢复保留，
> 退休载荷与发送载荷分离。LQ1432步816字段、SQ872步340字段四方一致、严格退出0；
> RTL14/14及两软件18＋6严格通过，活动clock11,967,396/690,814保持。完整三端来源/
> Cache/字节访存另验，下面§42～44是历史阶段记录。

> 当前LQ三端实际提交同拍回收（SPEC §44），robWillCommit Wire门控head，CDB只标记
> 已广播。1432步816字段全suite一致，更老提交/恢复并发及非队首先写回通过；SQ策略
> 待定，新ISA/六IPC clock11,967,396/690,814。

> **第12步第一项已修复（SPEC §43）**：主树初始化后置对齐模板stateAfter的排他结果，
> 三端正确CDB/架构值及完整18＋6回归通过；模板work保持。LQ释放和Store提交合同
> 尚未统一（§42），不能把整个生命周期称一致。

> 第10步DIV专用结果通道实际消费可同拍接收新工作，strict-younger取消/边界保持已统一；
> 真实返回链5000步500字段一致。18 ISA新clock总和11,967,406；Load/访存生命周期依第12～15步验收。

> 第9步MUL_CAP=1，三级弹性准入读取实际mulCDB/flush；真实乘法返回链2302步508字段
> 四方一致，n+3写PRF/ROB、n+4依赖派发。Load返回及访存生命周期依第12～15步验证（SPEC §38）。

> 第8步actualTaken运输/训练修正及对齐控制后端全链已验收；访存接口边界保持，AGU链
> 1195步回归及两软件18+6严格周期通过，总clock12,246,204。misAlign异常按用户要求排除。

> 第7步AGU已统一单槽同拍交付重填：n拍计算/保存，n+1交付/拍末写地址ready，n+2可见。
> LQ/SQ槽在issue预留，满只停新分配，不阻止AGU写已有条目；取消旧结果后可重填更老项。
> 3182步模块及1195步/2390点200字段真实地址整链四方一致；LQ/SQ.matchesAddress匹配
> 活动槽和完整tag，SQ.planAddressForward(memSlot,address,producerTag)同样过滤错身份广播。
> WB/StoreValue为上游输入；提交释放及Cache返回按第12～15步验收，详见SPEC §33。

> 第6步普通整数ALU CDB/PRF/ROB链已三端验收；LSQ真实目标使用、访存返回及
> 迟到事务的整链验收依第7/12～15步进行，不能以整数域通过替代。

> 第5步恢复策略：RS严格更老者在下游ready时可交付，RTL LSU同步接受更老地址/数据；
> 边界/年轻更新拒绝，专门LSQ恢复接收和普通同槽更新测试通过。

> 第4步PRF按FIFO统一分配，四路写回目标经存活ROB完整tag认证；普通重命名/恢复
> 域三端对拍通过，LSQ专门的生命周期和store提交时序按访存步骤验收。

> 第3步3B完成前端满时替换，LSQ满时准入、释放与转发规则保持；两树全量周期一致。

> **2026-10-05**：LQ/SQ 各 8 项全部可用，4-bit epoch 指针；恢复尾快照和 ROB/Wire 运输
> 均扩为 4 bit。memIndex 保留物理槽，getTail 是序号。D$8 KB/64 B；转发与退役规则保持。

> 负责处理器侧的访存顺序语义：Load/Store 发射即入队，store 数据/地址以组合
> 事件广播给 load（store→load 转发）；load 只有在更老 store 地址均已知且无
> 已知同址冲突时才准入缓存，不再依赖 MDP 违例恢复；
> 每周期经仲裁准入**一个**访存请求交给缓存/主存。
> 相关实现：`LQ`、`SQ`、`MemArbiter`（StaticArbiter）、store 广播组合逻辑。
> [← 返回 README](../README.md)

> 实现词汇：主树以周期初快照和 `tick()` 更新状态，模板树以 `Wire`、`Register`
> 和 `work()/sync()` 表达同一边界。本文的“快照”若未特别说明，指 ROB 保存的
> 恢复边界或周期初只读视图，而不是某一树专有的数据结构。

缓存与主存的存储行为（命中/回填/写回/延迟）见 [缓存与存储层次](cache.md)；
本文件只描述 LQ/SQ 与仲裁逻辑。

> **双树行粒度（2026-10-01）**：两树 ICache/IMEM 已对齐为 64 B 行、16-word 返回总线；
> DCache/DMEM 均使用独立的 64 B 字节载荷，D$ 为 16 KB 直接映射，主存延迟均为 20 周期。
> `fillFire/readBlocked` 只参与取指准入，不门控本节 LQ/SQ、`MemArbiter`
> 或 DCache 完成路径。双树活动结果见 [根级 benchmarks.md](../../docs/benchmarks.md)。
> ICache 的队首 n+1 读输出旁路只缩短取指交付路径，不改变本节的 load/store 准入和数据侧延迟。

---

## 1. 边界与职责

```
         AGU（后端执行）                    ROB（后端提交）
        load/store 地址就绪                store 已提交
              │                                 │
    StoreValue RS 数据就绪 ──(组合广播)──► SQ（8槽/8可用）◄── push（发射）
              │ storeNotifies / addrNotify      │
              ▼                                 ▼
       LQ（8槽/8可用）◄─────────────────── SQ 地址/数据事件
              │                                │
              │        MemArbiter（store 优先，每周期 1 请求）
              ▼                                ▼
         DCache ◄────────────────────────（已提交的 store）
              │ loadResp（组合）
              ▼
       LQ 最老已就绪项 ──lqCDB──► PRF / ROB（完成）
```

| 结构 | 容量 | 职责 |
|------|-----:|------|
| `LQ` | 8 个物理槽 / 8 个可用项 | load 条目：地址/值独立就绪、store 转发落值、完成总线 |
| `SQ` | 8 个物理槽 / 8 个可用项 | store 条目：地址/数据两段就绪 + 显式 `committed`；查询拍初快照 |
| `MemArbiter` | —（无状态） | 每周期准入 1 个访存请求；**store 优先**、DCache busy 时停发 |
| StoreValue RS | 4 | store 数据源（数据就绪事件的发生地） |

## 2. 两段就绪与广播事件

访存指令在发射时 push 到 LQ/SQ（`robTag` + `n_bytes` + 无符号位等），地址与
数据分别由两类保留站承担，**地址与数据各自独立就绪**：

- **地址就绪**：AGU 执行 `base + offset`。AGU 最老有效结果是 store 时，组合广播
  **地址事件** `storeAddrNotify = SQ.planAddressForward(memSlot, address)`；
- **数据就绪**：StoreValue RS 中 `isOperandReady(data)` 的条目，组合广播
  **数据事件** `storeNotifies[i] = SQ.planDataForward(memSlot, value)`。

两个广播都基于 **SQ 的周期初只读视图**求值（不依赖模块调用顺序）。对于某个
正在广播的 store，`StoreNotify` 还摘要它与目标 load 之间是否已有更年轻的同址
store（`knownSameAddressOldestTag`）或地址未知 store（`unknownOldestTag`），以免
较老 store 的值越过真正应该覆盖它的 store。

## 3. store → load 转发

load 的地址有效位与值状态**相互独立**。发射时值状态为 `NOTREADY`；AGU 写入
地址本身不会改变值状态。值状态机实际为：

```
NOTREADY ── cache 请求获准 ──► FETCHING ── 匹配的 DCache 应答 ──► READY
    └────────────── store 转发 ───────────────────────────────► READY
FETCHING ────────── store 转发 ───────────────────────────────► READY
```

- `LQ.applyStoreForward` 可把 `NOTREADY` 或已发请求的 `FETCHING` load 直接置为
  `READY`；后到的 DCache 应答只在该槽**周期初已是 `FETCHING`**、`robTag/memIndex`
  匹配且本拍有效状态仍为 `FETCHING` 时接受。周期初状态阻止 squash 后复用同一槽位
  与 tag 的新 load 接收旧应答，本拍有效状态则保证更新鲜的 store 转发值优先；
- AGU 首次解析 load 地址时，SQ 按同word候选查询，最年轻候选须数据就绪且完整覆盖
  Load，并比最后一个未知地址来源年轻才立即转发；返回值按字节偏移移动并截取/扩展；
- store 离开 ROB 后可能因 DCache busy 继续留在 SQ，因此提交状态由 ROB commit
  显式置位，不再用 retained `robTag` 与当前 ROB head 推断。转发的新旧关系按 SQ
  从 head 到 tail 的队列位置确定；只有未提交、仍在 ROB 活跃窗内的 store 才做
  RobTag 年龄比较；
- **未知地址或字节重叠的更老 store 会阻塞 cache 请求**。
  `SQ::canSendLoadToCache(addr, nBytes, loadTag)` 检查更老未提交Store的完整字节范围，
  任意重叠都阻塞尚未转发的Load，不能仅检查同起点或Store完整覆盖Load。
  同word内不重叠的已知Store不阻塞。已提交SQ队首仍由请求仲裁优先发送、Cache busy门控。

## 4. 保守准入与恢复边界

保守准入使内存顺序违例在正常路径上不可发生：一个 load 进入 cache 时，所有更老
未提交 store 的地址都已经解析，且没有字节范围重叠的 store。更老 store 后续不会再改变
地址，因此不会出现已执行 load 被更老 store 追溯覆盖的情况。

`SQ::replyToLoadRequest` 与 `SQ::canSendLoadToCache` 共同完成准入安全性：地址解析当拍
优先尝试 store→load 转发；没有可转发值时，`canSendLoadToCache` 再决定是否允许 cache
请求。`LQ` 的 include-self 尾快照仍保留给统一的 ROB/分支 squash 边界，不代表 load
仍会触发 squash。

## 5. 请求准入（MemArbiter → DCache）

`MemArbiter::arbitrate(LQ, SQ, ROB, DCache, squash)` 每周期给出至多 1 个
`MemDispatchDecision`：

- **store 优先互斥**：`SQ.head.committed`，或正位于 ROB 头且满足统一
  `storeWillCommit` 谓词的 SQ 头 store，优先于乱序 load；后者允许在同一周期完成
  ROB 提交并进入 DCache。squash 周期仅允许严格早于 `SquashTag` 的 ready head 提交；
- **busy 门控**：DCache `isBusy()`（缺失在途）时**不准入**——DCache 注释保证
  "`!isBusy()` 时必须无条件接受 decision"，因为该拍 store 已从 SQ 弹出；
- 请求携带完整身份：`{op, value/address, isSigned, n_bytes, robTag, memIndex}`
  （store 的 memIndex 带 `MEM_STORE_BIT` 高位标记；`memSlot` 为低 6 位，LQ/SQ/
  DCache 共槽位域）。

DCache 按两段 FSM 接受请求并回 `loadResp`（组合、过 squash 门），命中 1 拍
自答；缺失回填与脏逐出等存储行为见 [cache.md](cache.md)。

## 6. 完成总线

LQ 从 head 起按程序序扫描，选择**最老的 `READY` 且尚未广播**的 load；它不要求
物理 LQ head 本身已经就绪，因此语义是 oldest-ready，而不是 FIFO head-only。
lqCDB 每周期最多广播一个 `{value, robTag, memIndex}` 到 PRF 与 ROB；它可与
ALU/MUL/DIV 三路结果同周期并行。

CDB只标记已广播；robWillCommit Wire接实际旧态提交事件，队首tag匹配时同拍单写
head回收LQ。严格更老提交/恢复并发亦释放，满时旧full停收、下一拍复用，三端
实测15写回/16提交释放/17新分配（SPEC §44）。

## 7. 关键规格

| 项 | 规格 |
|----|------|
| LQ / SQ | 各 8 个物理/有效槽；4-bit epoch 序号区分满空，数组仅用 slot |
| 地址/数据保留站 | StoreAddr 4（配合 AGU）+ StoreValue 4 |
| 请求带宽 | 每周期 1 个访存请求（store 优先、DCache busy 停发） |
| 转发 | 数据事件（StoreValue RS 就绪）+ 地址事件（AGU 最老有效 store 结果），基于 SQ 快照组合求值 |
| 访存消歧 | 已知同址老 store 阻塞；地址未知老 store 也阻塞，load 不越过未解析 store |
| 违例恢复 | 不再设置 MDP load-violation squash 通路；顺序由保守准入和 store→load 转发保证 |
| 完成 | LQ oldest-ready、未广播项 → `lqCDB`（含 memIndex 通路） |
| 命中路径 | DCache 命中 load 1 拍自答 `loadResp`（见 [cache.md](cache.md)） |

## 相关文档

- [`../README.md`](../README.md) — 数据通路图 / 周期模型
- [`backend.md`](backend.md) — 发射 push、提交 store、FlushArbiter 恢复
- [`cache.md`](cache.md) — DCache/DMEM 的命中、回填与写回
