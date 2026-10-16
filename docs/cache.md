# 缓存与存储层次：L1I / L1D / 片上主存

## 2026-10-16 双树AXI同步（当前合同）

D$4 KiB（64组×2路×32 B），4 MSHR＋8请求槽（包含owner），同行Load合并、独立组hit-under-miss、Store有序、4拍/每拍两个bank回填、Load首拍返回。每路8个`SRAM<64,32,8>`及`SRAM<64,22>`，valid独立，tree-PLRU。组合事件缓存只保存端口/事件，sync失效，不保存模块next-state；Register单写及所有work后统一sync保持。
数据读写共享16条整行信用，经共享AXI4-Lite访问每通道16项/服务10拍/256 MiB RAM的Slave。退出SW绕过Cache，完整32-bit结果在B握手确定，不全dirty扫描。下方单miss/旧DMEM/旧RTL声明为历史，活动表见`../../docs/benchmarks.md`。

> **实现分叉（2026-10-16）**：用户仅升级主树为4 KiB两路/四MSHR/同行Load合并，DMEM
> 读写共享总计16项混合队列。模板与RTL代码未改，仍为下面的旧实现，不与主树新周期表等价。
> 新结构见[主树cache.md](../../docs/cache.md)；不能拿旧三端报告证明新主树已同步。

> **当前默认（2026-10-14，按RTL同步）**：D$为4 KiB、128组×1路×32 B，8个
> `SRAM<128,32,8>`、`SRAM<128,21>`（tag20＋dirty1）及独立valid。每拍回填64bit，
> 共4拍；缺失Load首个合格回填拍从旧256bit整行回复返回一次，第4拍才安装完整行、
> 消费回复与释放Cache队首。dirty写实际完成资格、持久取消、boot1拍、立即HALT、
> CDB/依赖派发边界保持；各Register单写、全部work后统一sync，I$仍1 KiB/64 B。
> 当前源码回归结果见共同父目录sync/README.md；下面8 KiB/8拍数字为历史。

> **2026-10-14 4 KiB几何扫描**：两软件/RTL支持32/64/128 B D$行与1/2/4路tree-PLRU。
> 每路独立嵌套SRAM/valid，lookup采样`readWayMask`；Store命中当拍用组合命中路，miss
> 冻结victim way/tag/dirty/data。最低编号invalid优先，否则PLRU；命中/末拍安装更新MRU。
> PLRU及valid只读旧Register、按事件单写；各SRAM每work一次、所有work后统一sync。
> 首拍Load只响应一次、末拍释放Cache队首；回填4/8/16拍，I$保持64 B，不加流水级。
> 扫描当时默认与活动golden为8 KiB/64 B/1路；候选只在独立副本运行，见
> [4 KiB扫描说明](../../../RISC-V-CPU-2026/docs/dcache-4k-sweep.md)。下面为默认/历史记录。

> **2026-10-13双树early restart＋8拍回填**：4项请求队列只查询旧队首；data为16个128×32bit/8bit写
> 粒度SRAM，metadata为128×20bit tag/dirty，valid为128个Register<1>；查询沿保存
> readLineValid，与同步SRAM返回对齐。每拍写相邻两个bank，首个合格回填拍从完整旧DMEM
> 行返回Load一次并清valid，最后一拍才更新metadata、置valid、释放队首及消费DMEM回复；
> `loadResponded: Register<1>`与fillWord/队首分开，取消Load也置该位；命中时序保持。
> DMEM整行读写格式、20拍延迟及双口并行协议不变；HALT提交立即结束、不排空。
> 无SRAM复位/扫描周期，core boot仍1拍；全部work之后统一sync，valid每项单写。
> 三端结构已同步，零/非零SRAM初态四方各15,435拍/19字段一致，289次早响应均不释放队首。
> 旧13,041,323/844,639周期属于无early restart的历史基线；新周期/统计及当前整核门禁见
> 共同父目录sync/README.md，活动golden只在完整验证后刷新。

> **历史HALT协议（2026-10-12）**：删除flushRequested、FLUSH_SCAN/FLUSH_WAIT及
> 全脏行扫描状态，phase恢复为Register<1>的READY/WAIT。正常dirty替换写回保持；
> CPU等待SQ和在途Cache/DMEM排空后DONE，空闲脏行保留。下面全dirty完成记录为历史。

> **SQ→ROB接口（SPEC §67）**：完成位图由SQ的两条Wire输出，ROB只校验并置ready。
> SQ发送、DCache准入及dirty写回保持，Wire随正常sync清缓存；四构建18＋6一致。

> **完整活动整核（SPEC §63/64）**：24镜像12667040拍三端无损逐拍与最终128KiB
> 全等；byte响应使用真实接受Load owner的宽度/符号，不用复用后LQ重解释。观察
> 只缓存实际未变的metadata/byte指纹，全部work后sync保持；AXI/STA另验，README §56。

> **全部dirty完成（SPEC §62）**：DCache phase扩为READY/WAIT/FLUSH_SCAN/FLUSH_WAIT，
> CPU顶层等待SQ/旧事务后发起set0..127扫描；dirty仅实际写完成后清除。软件
> plain payload表示RTL寄存器bank，metadata单写sync保持。16程序9629/2872、最终
> 128KiB独立核验/DONE保持16拍/7负向通过，入口sync/README §54。

> **真实恢复联合（SPEC §61）**：实际Cache/外存与整CPU67程序9001/2872全等，
> 已接受年轻Load取消与迟到返回排空、已提交SQ保留、42返回/新读同拍及11源码
> 负向通过。输入只读old，统一sync保持；最终dirty drain/计时属第19步，README §53。

> **BPU闭环增补（SPEC §60）**：实际SRAM I$/IMEM驱动BTB竞争26程序6690/1848全等；
> checkpoint预算仍ROB16/I$4/FQ4/IQ4=28，参数引脚/编译约束通过。访存4803/764及
> RTL16重新通过，第18所有FU/LSQ恢复继续，入口sync/README §52。

> **真实CPU/RAS闭环（SPEC §59）**：实际I$/IMEM及BPU、重命名、执行、ROB、恢复
> 全链33程序13041/1848三端严格一致；一条接受取指一个CID，pop-push不多分配，
> 回绕/取消身份与完整栈内容核验通过。真实SRAM data/寄存器tag保持，输入old与
> 统一sync无旁路，IPC活动新691640、ISA11967396，README §51。

> **I$同步验收（SPEC §58）**：RTL data使用公共真实sram_fakeram（16×512bit、
> 32bit写粒度），tag为16×22bit寄存器；实际本树I$/IMEM2324/160及前端2259/216
> 与主树/RTL/独立参考全等。嵌套SRAM work一次、统一sync，不能把父输出中的
> requestBuffer Registers作为Wire缓存整体再sync。四实际I$功能负向有效，README §50。

> **最新Cache联合门禁（SPEC §57）**：Store ROB.ready统一为旧SQ槽位图/epoch批量
> 完成，实际RS→LSQ→D$/DMEM→共享PRF/ROB→退休4803步764字段正向严格全等，
> 独立参考344字段含原始Store ready；取消§56屏蔽，入口sync/README §49。

> **当前D$模块对拍（SPEC §55）**：模板实际DCache/DMEM与主树、寄存器RTL和独立
> 参考6378步14字段全等，所有work后统一sync且_DEBUG单写检查通过。RTL一拍命中、
> 连续接受、并行脏读写及写完成映射软件。仅加只读探针友元，行为/golden保持。
> 默认展开65536上升沿数据位无SRAM，慢综合未完成；整核三端联合轨迹另验，README §47。

> **最新软件迁移回归（SPEC §51）**：SQ/LQ字节转发修复已同步模板，含Cache的完整
> 两软件CPU Release/_DEBUG各11个正常镜像正确，18ISA＋6IPC结果/退休/周期/统计
> 一致，活动clock11,967,396/690,814保持。2443步转发链三端架构正确、两软件逐拍
> 全等，7组策略周期差保留。§50已验真实RTL DCache/drain/token功能，完整三端Cache
> FSM周期/I$接入仍依原专项；软件Cache生产代码本轮保持。入口sync/README §43。

> SQ退休/缓冲/发送软件合同已同步RTL（SPEC §45），原SQ至Cache/内存接受才释放。
> 裸core HALT等SQ接受，外层事务/脏行另排空；RTL14/14包含长背压整核功能，完整三端
> Cache FSM/字节交互依对应专项。两软件18＋6strict-cycles活动基线保持。

> LQ提交回收（SPEC §44）完整18＋6及RTL13/13回归通过，新golden clock11,967,396/
> 690,814；缓存端口/FSM完整逐拍仍按对应专项验收。

> 第12步主树初始化后置修复使LQ word竞争三端数据正确（SPEC §43）；模板/Cache保持
> 当前实现，完整18＋6严格golden回归通过。上游完整Cache/FSM及迟到返回依第14/15步验收。

> 第10步DIV吞吐定版后两软件18 ISA＋6 IPC严格结果/退休/周期/统计一致，新18 ISA clock
> 总和11,967,406，六IPC690,826；RTL13/13通过。Cache完整FSM/端口时序依第14/16步验收（SPEC §40）。

> 第9步三级弹性MUL及真实返回后端通过；两软件18 ISA＋6 IPC严格golden保持，18 ISA
> clock总和12,246,204，六IPC另计690,826。Cache完整三端FSM/端口周期依第14/16步验收（SPEC §38）。

> 第8步方向训练使用BRU真实actualTaken；控制/BPU后端链1518步四方一致，软件18+6
> 总clock12,246,204、RTL13/13通过。缓存/外部返回周期仍以对应专项为准（SPEC §35）。

> 第7步AGU n拍计算、n+1交付/拍末写LSQ地址、n+2地址ready可见；LSQ满不背压地址更新。
> 两软件18+6总clock12,246,204保持，RTL集成13/13；Cache完整三端周期仍按第14步验收。
> 地址整链1195步四方一致，错tag/槽及恢复后旧结果重放拒收；Cache请求接收/返回不在该地址链驱动域。

> 第6步整数链已闭环，18+6严格回归总clock12,246,204保持；Cache/外存事务的
> 完整三端周期不由该整数链证明，仍以专门缓存对拍为验收依据。

> 第5步flush期间严格更老派发和RTL接收路径扩展经11/11回归；缓存/存储时序的专门
> 软硬件逐拍等价仍按对应步骤展开，活动数据见主树benchmarks。

> 第4步FIFO基础域验收：18+6严格周期保持12,246,204，RTL10/10集成通过；缓存
> 几何与回填旁路的活动测试数据以主树benchmarks为准。

> **第3步3B-2**：ICache→FQ与popConsume同拍握手，FQ满时真实出队可同拍补入；
> ICache四项请求满且真实消费队首时也可同拍补入。模板同槽pop/push合并单次Register写，
> `_DEBUG`全量无双写。18+6新周期和统计与主树一致。

> **同行回填旁路补正**：refillHit由实际fillFire与未门控PC匹配产生，供数给同拍
> 新接受的请求槽；raw/ready直接选择最终值，各Register只写一次。n+1起按序返回，
> 无额外SRAM读或重复IMEM请求；普通旧请求仍由refillSlot选择。回填供数计入hit。
> 两树18+6一致，总clock12,246,204；16 lane/满槽/flush/HALT定向测试通过。

> **2026-10-05 3A 容量同步**：D$8 KB、128×64 B、直接映射，I$1 KB 与命中边界保持；
> 18+6 例完整结果、周期及统计与主树一致，框架 `_DEBUG` 零断言。下方 2026-10-01 的
> 16 KB 验收数据保留作历史，活动基线见根级 benchmarks。

> 存储层次为两层 + 片上主存：L1I（ICache）、L1D（DCache），下一级是各 128 KB
> 的指令/数据主存（IMEM / DMEM）。**主存端口延迟固定 20 周期**；两树 ICache
> 使用单端口同步 SRAM，队首命中最早 n+1 拍旁路交付 FQ；DCache入队与查询分开，
> 无更老请求时n拍入队、n+1拍查询、n+2拍命中Load应答或Store写入。
> 两个缓存的缺失回填与脏逐出写回统一走 20 周期端口。访存队列如何驱动它们见
> [访存子系统](memory.md)。
> [← 返回 README](../README.md)

> 实现词汇：主树用周期初快照与 `tick()`，模板树用 `Wire`、`Register` 与
> `work()/sync()`；下文描述持久状态、组合命中和周期边界，不依赖其中任一写法。
>
> **历史双树状态（2026-10-01）**：两树 1 KB SRAM ICache 的队首 n+1 旁路已对齐；本树 Release 与 `_DEBUG`
> 均通过 18/18 x10+cycles 活动 golden 门禁，逐例与主树一致。迁移前路径作历史对照。
> 活动结果见 [根级 benchmarks.md](../../docs/benchmarks.md)，共同契约见
> [根级 cache.md](../../docs/cache.md) 与 [process.md](../../docs/process.md)。
> 同日完成等价参数化后采用 **16 KB / 64 B / 直接映射 DCache**；活动总 clock
> **12,209,929**，相对 64 KB 四路旧配置 **−0.150397%**，满足总 clock 相差 ≤1% 的门槛。

---

## 1. 层次与延迟模型

| 层次 | 容量/组织 | 命中 | 缺失 |
|------|-----------|------|------|
| **L1I** · 本树迁移前路径（历史对照） | 8 KB，512 行 × 16 B，**直接映射**、Register 数据阵列 | 命中组合取字、本拍登记请求，最早下一拍进入 FQ | IMEM 整行回填，**20 周期** |
| **L1I** · 两树当前 ICache | 1 KB，16 行 × 64 B，单端口同步 SRAM | n 拍读，队首且无背压时 n+1 拍旁路进入 FQ；否则落槽保存 | IMEM 64 B 行回填，20 周期 |
| **L1D** · DCache | 4 KB，128 组 × 32 B，直接映射，data/metadata同步SRAM | 入队后旧队首查询，查询下一拍Load应答/Store写入 | 主存读 **20 周期**；首个合格回填拍响应Load，本地整行回填 **4 拍**；脏 victim 并行写回 **20 周期** |
| **IMEM / DMEM** | 各 128 KB 字节寻址 | — | — |

主存延迟由两树共同的 `MEM_LATENCY = 20` 定义：取指请求、DCache 回填读和
脏 victim 写回都以该值装载各自的倒计时。性能与命中率基线见仓库根目录
`docs/benchmarks.md`。

### 1.1 缓存几何参数

两树以 `common.hpp` / `common.h` 中的基础配置为源：I$ 为
`NUM_OF_ICACHE_SETS`、`ICACHE_BLOCK_CAP`；D$ 为 `NUM_OF_DCACHE_SETS`、
`DCACHE_BLOCK_CAP`。当前分别为 16 组/64 B 行和
128 组/1 路/32 B 行。组数与行长必须为二次幂。

- `*_OFFSET_BITS` / `*_OFFSET_MASK` 从行长派生；`*_INDEX_BITS` / `*_INDEX_MASK`
  从组数派生；`*_TAG_SHIFT = *_OFFSET_BITS + *_INDEX_BITS`，tag 位宽为
  `RV32_WORD_BITS - *_TAG_SHIFT`（当前 I$ 22 bit、D$ 20 bit）。
- 行首 = `addr & ~*_OFFSET_MASK`；组号 = `(addr >> *_OFFSET_BITS) & *_INDEX_MASK`；
  tag = `addr >> *_TAG_SHIFT`。D$ 写回用 victim 自身的 tag 与组号重建行首。
- I$ 的 `ICACHE_WORDS_PER_LINE`、word 索引掩码、lane 索引位宽和 SRAM 行位宽均从
  行长派生；`LineReturn` / IMEM / CPU 接线共享同一 word 数。D$ 读写载荷长度统一为
  `DCACHE_BLOCK_CAP`；metadata位宽为tag位宽加dirty一位，valid独立为组数项寄存器。
- D$支持1/2/4路tree-PLRU，默认直接映射的替换状态不参与选路；请求队列索引/count
  及byte lane位宽由容量和行长派生。可选配置不代表已经完成完整CPU周期验收。

参数化是同配置表示变换：验收要求 x10、retired、cycles 与缓存/分支统计逐项等价。

**2026-10-01 参数化阶段验收（旧 64 KB 四路配置）**：两树 Release 的活动 18 例与独立 IPC 6 例，及模板
`_DEBUG` 与同编译选项改前检查版的 24 例，均逐项一致；活动总 clock 12,228,320，
独立六例几何 IPC 0.625380057590。仓库外不同几何定向用例覆盖 64 B 数据行、
单-word 指令行、高位 tag 区分、行尾子字访问、有序部分写合并与脏 victim 写回；
两树结果/周期对齐，非二次幂行长或组数在编译期拒绝。

**历史直接映射配置验收（2026-10-01）**：两树 Release 18+6 例结果、retired、新周期与统计一致，模板
`_DEBUG` 24/24 结果和周期通过、零断言。独立六例精确几何 IPC **0.720538578156**；
目标直接映射及两组不同几何的有序缓存边界场景均通过，见 [根级 benchmarks.md](../../docs/benchmarks.md)。

## 2. 指令侧：ICache + IMEM

### 2.1 ICache（L1I）

- **几何**：1 KB 直接映射，16 行 × 64 B；行内 16 个 32-bit word，数据阵列为
  `SRAM<16,512,32>`。行首 `pc & ~0x3F`，行号 `(pc>>6)&15`，word `(pc>>2)&15`，tag `pc>>10`；
  地址切分与控制载体宽度由容量推导。
- **请求状态**：4 项环形队列中 `valid` 表示占用，`ready` 表示结果就绪；新 hit/miss
  均先置 `valid=1, ready=0`，occupancy 由占用位归约。FQ 只消费 ready 队首。
- **命中时序**：n 拍接受请求并读 SRAM，保存 `readIndex/readLaneIndex`；n+1 拍当
  `headReadReady() = head.valid && !head.ready && readValid && readIndex==head` 时，
  `isReturnReady()/returnRaw()` 直接选择旧 SRAM 输出，FQ 可接收则当拍交付。
  PC/预测 PC/ckptId 仍来自队首槽；无新读时清 `readValid`。
- **消费/落槽互斥**：`readConsumed = popConsume && headReadReady()`，被消费的旁路
  结果跳过落槽，避免 pop 清 ready 与落槽置 ready 同拍双写。背压或非队首读仍登记原槽，
  防止后续 SRAM 输出覆盖；不能借旁路越过更老请求。旁路不额外占用 SRAM 端口。
- **回填选槽**：保留 `refillValid()` / `refillSlot()` 两个组合函数，从旧 head 开始固定
  扫描 4 槽，只匹配存活、未 ready、行地址相同且不是待落槽 SRAM hit 的请求。
  一份返回完成第一个匹配槽，critical word 直接取自 `LineReturn`，不广播完成重复行请求。
- **统一握手**：`fillFire = !needSquash && refillValid()`，同一信号驱动 IMEM
  `lineConsumed`、选中请求完成、tag 更新和 SRAM 全 lane 回填写；无匹配时 IMEM 保持返回。
- **单端口仲裁**：回填写优先于新命中读；`readBlocked = fillFire && hit(fetchPC)`
  接入 BPU 取指准入，只反压取指。新 miss 可与回填写同拍准入。
  `fetchPC` 取未门控的 FetchUnit PC，不能用已准入门控的 `BPU.outPC` 形成组合反馈环。
- **嵌套同步**：`ICacheData` 位于 Inner；非 squash 拍由 ICache 调用一次 SRAM `work()`，
  父模块 `sync()` 递归提交读输出 Register、清端口 Wire 缓存。SRAM 不单独注册到 `dark::CPU`。
- **恢复/停机**：squash 清请求占用、就绪和待读 valid，保留缓存行，压过落槽和回填写；
  HALT 检测要求队首 ready 且 FQ 不满，与交付同时锁存 `haltFetched`。
- 统计：`VERBOSE=icache` 输出 hits/misses/hit-rate。

### 2.2 IMEM（指令主存）

- 16 项请求队列，64 B 对齐，`remainCycle=20` 递减；完成时返回 `LineReturn`
  16-word 总线（`{lineAddr, data[0..15]}`），critical word 索引为 `(pc>>2)&15`。
- ready 队首持续提供返回，直到 `fillFire` 确认被接收；squash 优先清除投机请求。
- 与 DCache 回填共享同一主存延迟口径（20 周期），数据侧为独立的 32 B 字节载荷。

## 3. 数据侧：DCache（L1D）

### 3.1 组织与替换

- 4 KB = 128组×32 B，直接映射、写回＋写分配。
- data为8个`SRAM<128,32,8>`，每bank四个byte lane；metadata为`SRAM<128,21>`，
  bit0..19为tag、bit20为dirty。valid为主树`bool validBits[128][1]`/模板`Register<1>`数组，
  控制复位不需要清SRAM；宿主冷构造的零存储不代表硬件初始化事务。查询同一组时并行读全部bank和metadata；
  每个bank只有一个1RW端口，回填每拍只写两个相邻bank（64bit），不会整行并行写入。
- 几何和metadata掩码统一在`common.hpp/common.h`，从组数/行长派生。

### 3.2 请求队列与队首串行服务

- 保留4项RequestBuffer，`head/count`管理占用；请求携带地址、值、宽度及完整ROB/LQ身份。
  入队不读取SRAM；MemArbiter仅按旧`isRequestFull()`停发，即使本拍完成也不满槽替换。
- 只查询旧队首。无更老请求时n拍入队、n+1拍读data/metadata并采样valid至`readWayMask`、n+2拍服务命中。
  Load直接组合返回raw给LQ，由LQ截取/符号扩展；Store实际写byte lane及dirty后才出队。
- 回填/Store写优先于查询；Store缺失在相应bank回填拍合并合法Store字节，末拍metadata置脏。
  命中Store仅写对应byte lane，不形成Load响应。
- 缺失拍保存旧tag、valid/dirty及完整`victimLine`。读回填和脏写回各有sent标记，
  只在实际接受时置位；可并行发送，不增加phase FSM或两条外存FIFO。
- 回填必须已有读接受且DMEM回复就绪；脏victim还要求写已接受并完成。`fillWord`依次为
  0/2/4/6，各拍写一对bank；前3拍保持metadata和队首，不消费整行回复。
  第4拍`fillFire`同时更新metadata、置对应valid、消费回复及释放队首；旧tag和组号重建写回地址。
  Load在首个合格拍从完整旧DMEM行返回一次，与目标bank位置无关；取消Load也置
  `loadResponded`，但不写LQ。首拍清valid，末拍置valid，半行不可被年轻请求查询。
  DMEM请求/回复格式、20拍延迟及双口并行协议不变，不新增完整回填行缓冲。
- 恢复对已接受Load保存跨拍`cancelled`，禁止迟到响应污染复用ROB/LQ身份；仍完成物理
  事务并释放队列，已提交Store不取消。只有`readValid`拍使用SRAM输出，之后用保存载荷。
- 模板8个data SRAM和metadata SRAM嵌入DCache Inner，各work一次、父模块正常sync提交，
  无额外注册或提前sync；`fillWord`每拍单写，主树查询与tick不读取写目标。
  主树仅读旧快照/Input逐字段写CPUstate；组合响应/外存请求不增加寄存级。
- HALT提交立即结束，不等待已提交SQ、D$/DMEM事务或全脏行写回。

## 4. DMEM（双口主存）

- **读/写双口各自独立**：`readBusy`/`writeBusy`、`readExecute`/`writeExecute`
  分开计数递减，因此回填读与脏逐出写回可**并行在飞**（各 20 周期）；
- 只接受 DCache 转发的**行级请求**（读行/写行），不再接受任何处理器侧的
  Load/Store 原语；
- 完成读时从持久 DMEM 存储阵列取 32 B 填回复缓冲；因此回填能看到此前脏逐出
  已写回的新值。写端口经 `writeLine` 更新同一阵列；
- 回复保持有效至DCache的`fillFire/replyConsumed`，持有回复时禁止新读，写口仍独立。

## 5. 关键不变量与画像

- **脏逐出不变量**：同组不同 tag 的 store 强制逐出时，写回地址必须由 victim
  的 tag 与组号重建；后续回填必须读到写回后的数据；
- **命中率画像**：18 基准的 I$ / D$ 命中率与逐分型预测准确率在
  仓库根目录 `docs/benchmarks.md`（D$ 覆盖 load+store 全部访存）；
- **验证**：模板`_DEBUG`检验Register单写；外部真实CPU测试检查独立退休/内存参考、
  SRAM无效输出poison和乱序work，不能以历史寄存器D$三端报告证明当前RTL等价。

## 6. 关键规格

| 项 | 规格 |
|----|------|
| 两树 ICache | 1 KB · 16 行 × 64 B · 单端口同步 SRAM · 4 项请求队列 |
| 本树验收 | Release / `_DEBUG` 各 18/18 x10+cycles 对活动 golden；逐例与主树一致 |
| DCache | 4 KB · 128 组 × 1 路 × 32 B · 直接映射 · 写回 + 写分配 |
| 命中交付时序 | L1I 队首最早 n+1 拍交付FQ；L1D n拍入队、最早n+1查询/n+2服务 |
| 主存延迟 | 固定 **20 周期**（IMEM 回填、DCache 回填、脏逐出写回共用） |
| D$本地回填 | 8个32bit bank，每拍写一对，共4拍；首拍Load早返回，末拍安装完整行并消费回复 |
| DMEM | 读/写双口独立在飞（各 20 周期）；仅行级请求 |
| 存储阵列 | IMEM/DMEM 各 128 KB；缓存/主存阵列均为模块持久状态 |

## 相关文档

- [`../README.md`](../README.md) — 数据通路图 / 周期模型 / 性能口径
- [`frontend.md`](frontend.md) — 取指如何消费 L1I（命中组包 / 缺失回填）
- [`memory.md`](memory.md) — LQ/SQ 与 MemArbiter 如何驱动 DCache
- 仓库根目录 `docs/benchmarks.md` — 主存延迟口径与命中率实测
