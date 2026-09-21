# 模板迁移回顾与当前状态

> `src/` 到 RISC-V-Simulator-Template 的阶段 0--9 迁移已于 2026-08-30 完成。
> 本文不再充当逐日任务清单，而记录终态架构、迁移成立的时序依据、仍有约束力的经验和当前唯一的数据通路遗留项。

## 文档入口

- 当前前端架构：[前端](frontend.md)。
- 当前乱序后端与 RV32M 实现：[后端](backend.md)。
- 当前 LQ/SQ、访存仲裁与内存一致性：[访存](memory.md)。
- 当前 ICache/DCache/IMEM/DMEM 层次：[缓存](cache.md)。
- 模板运行、框架 API 与常见错误：[使用说明](help.md)。
- 回归结果与性能口径的唯一基线：仓库根目录 [`../../docs/benchmarks.md`](../../docs/benchmarks.md)。
- 可综合循环的完整审计与分类：仓库根目录
  [`../../docs/non-synthesizable-loops.md`](../../docs/non-synthesizable-loops.md)。

## 当前结论

| 项目 | 终态 |
|---|---|
| 迁移进度 | 阶段 0--9 全部完成；没有仍在进行的“混跑阶段” |
| 状态更新 | 所有时序模块使用 `work()` 写 `Register::_M_new`，周期末统一 `sync()` |
| 组合通信 | 跨模块端口和中央总线使用 `Wire`；无状态仲裁器本身也是 `Module` |
| CPU 运行时 | 模块按值归 `CPU` 所有，以非拥有指针注册到 `dark::CPU`，每拍调用 `run_once()` |
| 旧机制 | `comb()`、`tick()`、模块快照成员和周期性 `memcpy` 已退出模板树 |
| 执行/写回 | ALU、LQ、MUL、DIV 四路独立结果总线；MUL 与 DIV 有专用 RS 和派发通道 |
| 分支预测 | Tournament：local/global/selector 各 256×2b，8-bit GHR；目标侧 BTB64/RAS8/SARAS16/condSeen512（BHT/Target Cache 已删除） |
| 恢复 | ROB tag、checkpoint、FlushArbiter 和各模块本地恢复状态共同完成整窗 squash |
| 验证 | 双树 `x10` 与等价改动的 `clock` 对拍，加 Release、`_DEBUG` 单写断言和基准表核验 |

迁移收官后的 DCache、MUL、DIV 等扩展都直接按同一 `Module + Register + Wire`
模型接入，不再引入第二套快照机制。

## 阶段 0--9 回顾

| 阶段 | 完成内容 |
|---|---|
| 0 | 引入模板基础设施，建立 `Register` 自同步和混跑验证方法 |
| 1 | 以 FetchUnit 定版状态、输入接线、桥接访问器和单写口手法 |
| 2 | 迁移 FetchQueue、DecodeUnit、ICache、IMEM 等取指生产模块 |
| 3 | 迁移 ALU、AGU、BRU 执行单元 |
| 4 | 迁移 PRF、RS，值与就绪状态统一由 PRF 派生 |
| 5 | 迁移 DMEM、LQ、SQ，保留访存顺序与前向语义 |
| 6 | 迁移 ROB，统一最广的 tag、提交、checkpoint 和恢复读口 |
| 7 | 迁移 FlushArbiter 与 BPU 两个状态 hub |
| 8 | Fetch/CDB/Mem/Dispatch/Issue 等组合决策及中央总线全部 Wire 化 |
| 9 | 删除快照运行时，切换到 `dark::CPU`，完成全模块 `work()+sync()` 收口 |

## 快照与 Register 的时序等价

迁移能逐模块进行的根本原因不是接口相似，而是两套机制表达同一个同步时序：

| 旧快照框架 | 模板框架 |
|---|---|
| 活体模块由 `tick()` 写 next-state | `work()` 用 `<=` 写 `Register::_M_new` |
| 周期开始把活体模块复制到只读快照 | 周期开始读取 `Register::_M_old` |
| 本周期所有模块只读快照 | 本周期所有模块只读已提交状态和由其派生的 `Wire` |
| 下一周期 `comb()` 的复制使写入可见 | 本周期末 `sync()` 令 `_M_new` 成为新的 `_M_old` |

所以两者都满足：本周期组合和时序决策只能观察上一拍已提交状态，本拍写入只能在下一拍被观察。
`Wire` 是组合函数，不是寄存器；它按拍懒求值并缓存，`sync()` 清缓存，不会凭空增加一拍。

最终 `dark::CPU::run_once()` 先调用全部模块的 `work()`，再统一 `sync_all()`。
正常注册顺序沿用了旧手写循环，正确性则不应依赖这个顺序：模块不能读取其他模块本拍的
`_M_new`。迁移期曾用打乱 `work()` 顺序验证这一性质；它现在是历史证据，不是现行回归项目。

`CPU::run()` 仍是 host-only 仿真外壳。它必须在 `run_once()` 前采样停机与排空条件，
保持旧框架“周期开始判停”的语义；HALT 后还要等 SQ、DCache 和 DMEM 排空，不能截断已提交 store。

## 历史：混跑期六条硬约束

以下六条只描述阶段 0--9 期间的过渡架构，**当前模板树已经没有混跑边界**。
保留它们是为了说明迁移为何安全，以及未来若重做类似迁移不能省略哪些步骤。

1. **模块必须全有或全无地转换。** `Register` 不可按普通对象 `memcpy`；包含它的模块若仍进入快照复制就是未定义行为，不能只改半个模块。
2. **状态所有权、快照提交和验证适配必须同步修改。** 当时每迁移一个模块，都要同时把 CPU 中的双实例收成单实例、删除对应快照复制、加入周期末 `sync()`，并让验证驱动比较已提交状态而非对象字节。
3. **非零初值必须显式设计。** `Register` 默认只产生零态；RAT 恒等映射、BPU 计数器，以及当时 TAGE 实现的 LFSR seed 等不能依赖构造器偷偷写 `_M_old`，必须有复位/boot 周期及必要的 cycle-0 读视图。
4. **位宽与显式转换是接口的一部分。** `Register`/`Wire` 的布尔和整数转换是 explicit；环形指针、tag 距离、掩码与年龄比较必须先转成明确宽度，不能依赖宿主整型提升。
5. **整对象清零必须删除。** 对含 `Register`、`Wire` 或 lambda 的对象执行 `memset(this, ...)` 是未定义行为；状态要逐成员初始化，固定数组使用自身零初始化。
6. **每个 Register 每拍只能写一次。** 即使两次写入同值，`_DEBUG` 也视为两个物理驱动；push/flush、恢复/发射、写回/直写等冲突必须先归约成优先级或每槽 mux，再从唯一写点提交。

## 终态模块模型

- `Module<Input, Output, Inner>` 同时承载输入 Wire、输出 Wire 和内部 Register；模块实例自身就是 Input 子对象，接线必须落在实例上。
- `work()` 对应 `always_ff`：读 `_M_old`，计算所有 next-state，每个 Register 保持单写口；不使用提前 `return` 隐藏部分写集。
- `Wire` 对应 `always_comb`：构造/接线阶段赋一次 lambda，按拍懒求值；组合模块的 `work()` 可以为空。
- CDB、Mem、Dispatch、Issue 等无状态仲裁器没有 Register。把它们建成 Module 的原因是让 `sync()` 统一清 Wire 缓存，而不是给组合逻辑增加状态。
- checkpoint 是状态复制，不是普通通信总线。RAT、PRF、BPU 的 checkpoint 数组保留为本地 Register 状态，恢复时逐槽写回。活动 `CKPT_CAP=32`，`CKPT_LIVE_MAX=26` 覆盖 ROB/ICache/FQ/IQ 的全部存活 ID；逻辑 ID 与运输载体均为 5 bit（`CKPT_ID_WIDTH`）。PRF 的 `headSeq/tailSeq/PRFHeadCkpt` 统一为 packed 循环序号（`{epoch,index}`，PRF64 为 7 bit）：写入与恢复都取 canonical 值，恢复距离用 `prfSeqDistance` 校验。
- IMEM/DMEM 的大存储阵列和缓存数据阵列不因模板化而机械变成 Register 阵列；端口、控制状态和拍级握手才进入模块同步模型。
- 逻辑索引按 CAP 掩码收紧，并以 `static_assert` 守住容量假设；容量缩减后部分接口载体有意保持原宽度，实际有效范围仍由 CAP 限定。

## 现行验证政策

`reorder_test` 及其仓库内脚手架已经退役，不再是构建或验收依赖；不要在本仓库重建一份临时替代品。
现行闭环是：

1. 主树与模板树对同一镜像比较 `x10`；行为等价改动还必须逐拍比较 `clock`，差一拍就是回归。
2. `x10 & 0xFF` 与现行性能基线只认 [`../../docs/benchmarks.md`](../../docs/benchmarks.md)；本文仅在带日期的架构定案中保留对应验收快照。
3. Release 验证功能与时序，`_DEBUG` 验证 Register 单写、索引和结构不变量。
4. 纯表示变换不得改变 clock；有意的微架构改动必须单独说明原因、验证结果和基线更新，不能用“模板天然漂移”解释差异。
5. 统一 RV32IM 镜像同时覆盖基础整数路径与真实 `mul/div/rem` 路径；不含 M 指令的 `naive` 作为阴性对照，长用例只作为里程碑，不把临时耗时写入本文。

`run_once_shuffle()` 仍是框架能力，也曾证明 Wire/Register 结构没有隐藏的模块执行顺序依赖；
但在测试目录清理后，它不再属于日常验证政策。

## 历史：BPU 双树 clock 对齐复盘

2026-08-31 的逐拍对齐最终确认了六项差异，其中五项是真 bug，一项是批准的冗余门控删除。
修复后双树恢复 `x10 + clock` 逐位一致，“clock 漂移是双端口训练的预期结果”这一旧判断作废。

1. **LFSR boot seed 被覆盖。** BPU commit 每拍无条件回写 LFSR，与 cycle-0 boot seed 同拍双写；Release 丢 seed，`_DEBUG` 直接触发双写。改成仅有步进时回写。
2. **参考树 FI 早训练写到了快照。** FQ 预译码产生的 BTB 早训练没有落到活体状态，JAL target/RET type 实际未持久化；改写真正的 next-state。
3. **模板跳转训练误写间接类型。** `updateJumpPlan` 把 `T_BTB_IND` 硬编码为 1；改为请求的 `isIndirect`，Target Cache 训练也补同一门控。
4. **模板过度压制 BRU 训练。** `needSquash` 曾无条件撤销同拍 BRU 训练，连比 squash 更老的分支也被丢弃；删除总闸，只保留年龄判断。
5. **squash 回卷基准与距离都错。** 回卷使用了 post-FI 的 align tail，并让 8-bit 环形距离按 32-bit 下溢；改用 pre-FI 基准，距离显式按 256 取模。
6. **RET 队头门控被删除。** 对齐调查证明它不是正确性保护，而是冗余的队头阻塞；两树统一接受删除，空 RAS/BTB target 的真正保护放回预测侧。

这次排查还确立了方法：从首个分歧拍向前追到首个状态写入点，不从最终 clock 差反推原因；
同拍端口碰撞也必须先证明写集合真实相交，不能凭抽象结构猜测。

## AGU 访存违例：重放必须使用真实 PC

store 地址由 AGU 解析后，FlushArbiter 会扫描更年轻、已经取值或正在取值的同址 load。
发生违例时，squash 对象是该 **load**，重放地址必须是 `ROB.pc[robSlot(violTag)]`。

曾经错误地读取 `ROB.predictedPC`。load 的 ROB 项不会由 INT/BR/UJ 发射路径填写该字段，
因此值恒为 0，机器会重定向到地址 0 并从启动区重跑。修复是给 FlushArbiter 接入真实
`robPC` Wire，并从违例 load 自身 PC 重取。一般化教训是：恢复目标要按字段的生产者审计，
“字段在 ROB 里存在”不等于“所有 uop 类型都会写它”。

## 标识域与 Register 纪律

### InvalidPhy 与 tag 单域

- `InvalidPhy = 0` 是整个物理寄存器域的唯一哨兵；P0 永不分配、永不建立 RAT 映射，当前真实 phy 为 `1..63`。
- `Operand.tag == InvalidPhy` 表示立即数/常量路径；PRF pop/push、RAT 写映射和源操作数解析都要守住非零断言。
- `RobTag` 是 packed `{epoch, slot}` 序列身份：slot 宽度为 `bit_width(ROB_CAP-1)`，
  `ROB_TAG_WIDTH` 再加 1 个 epoch bit。当前 ROB16 为 5 bit；slot 只分配
  `0..ROB_CAP-1`，非二次幂容量的空洞编码由 `robNextTag()` 跳过。
- 不再维护 `robIndex`、`SquashIndex`、`getIndexByTag` 等第二身份域。数组索引只走
  `robSlot(tag)`；年龄比较按 epoch/slot 规则完成，不对含空洞的 packed 数值做 `%`。
- 模板所有语义 RobTag `Wire/Register` 使用 `ROB_TAG_WIDTH`，phy tag、`memIndex`、
  opcode 等独立 7-bit 域不随之收窄。ROB16→ROB12 实测双仓库 18/18 x10 与 cycles
  逐项一致，覆盖 slot 12..15 空洞跳转。
- 用 tag 索引前先验证 ROB 条目保存的完整 tag；删除冗余 index 不等于删除生命周期守卫。

### 零初始化

`Register` 的 `_M_old/_M_new` 从 0 开始，不能用普通构造器建立非零硬件复位值。
RAT 和 BPU 使用显式 boot；BPU 对 cycle-0 预测还提供 boot 常量视图，避免首拍先读到未提交的零表。

状态编码应尽量让零态就是安全空态。例如 RS 保存 `busy` 而不是“空闲为 1”的 `free`，
否则 cycle 0 的组合仲裁会把全零槽误当有效工作。boot 只能保护写路径，不能自动保护首拍组合读者。

### 单写与优先级

同值双写仍然代表两个硬件驱动。正确收敛方式是先收集意图，再用固定优先级或每槽 enable
产生唯一一次 `<=`。flush 通常压过普通推进；若参考实现依赖“先写后覆盖”，模板必须把该顺序
显式编码进 mux，而不是依赖 C++ 语句的最后写胜出。

## RAT 写回收敛

RAT 同拍可能同时遇到 checkpoint 恢复和新指令 rename。每个 `RAT_PRF[i]` 只能有一个写点：

- 恢复值以 checkpoint 的 `_M_old` 为 base；
- 若本拍 issue 分配目的寄存器且 `i == issueDest`，新映射覆盖恢复值；
- 这精确对应参考实现的“先 restore，后 `setRAT_PRF`”优先级；
- 本拍建立的新 checkpoint 捕获的是 **恢复前** RAT 快照，但目的槽写入新 phy，复现旧框架从周期初快照复制后再覆盖目的槽的语义。

因此实现按槽计算 `destHit ? issuePhy : (restore ? checkpointValue : hold)`，并让 checkpoint
写口独立捕获 `destHit ? issuePhy : preRestoreValue`。这既保留拍级语义，也满足 Register 单写断言。

## 状态与容量收敛

### 历史：TAGE 1024 -> 512

四张 TAGE tagged table 的索引宽度由 10 降到 9，每表 1024 项缩为 512 项；T0 及其他结构不随之缩小。
采纳的 W=9 方案使 tagged-table SRAM 约减半（约 28.7 Kb），全量总 clock 与 pi 均略有改善，
已观测单例最差变化约为 `+0.35%`。W=8 虽继续省面积，但单例退化更明显，未采用。

折叠历史的索引掩码、移位、旋转和 squash 重算必须与 9-bit 宽度一起修改；尤其手写 rotate/fold
需要显式 mask。这是已退役 TAGE 架构的容量实验记录，不描述当前 Tournament 实现。

### 2026-09-20：Tournament 面积定案

方向侧最终采用三张固定 256×2-bit 表：localPHT 由 `(PC>>2)&255` 索引，globalPHT 与
selector 由 `((PC>>2)^GHR)&255` 索引；16-bit GHR 保持投机更新，selector `>=2` 选择
global。三表计数器初值均为 1，条件分支解析时 local/global 同时更新，只有预测分歧时
chooser 才向正确一侧移动。最终条件 taken 采用命中门控方案 A：
`btbHit && directionTaken`，BTB 命中的无条件项覆盖为 true。

目标侧保持 BTB64、BHT256、TargetCache32、RAS8、SARAS16、condSeen512 不变；checkpoint
只保存 16-bit GHR、AlignQueue 头尾和 RAS_top，不再有 TAGE 元数据、折叠历史、分配或
老化状态。模板 BPU 的完整 Register 状态由此前活动 TAGE 的 **24,357 bit** 降至
**12,558 bit**，减少 **11,799 bit（48.44%）**。

A/B 使用同一架构与语料，只改变条件方向是否受 BTB hit 门控：A 为 **12,237,892 cycles、
93.8367%**，B 为 **12,250,503 cycles、93.6618%**；独立 IPC 语料总 cycles 分别为
**495,871 / 495,830**。按 18 例主指标选择 A。最终主树与模板树 Release 在 18 例上
`x10` 和 clock 全部逐项一致，总 cycles **12,237,892**，加权 IPC **0.553703**，分支
正确率 **93.8367%（1,307,716 / 1,393,609）**；独立 IPC 语料 **6/6** 逐项一致。

### 2026-09-20：间接目标缓存删除

方向侧改用 Tournament 后，BHT256 与 TargetCache32（及 `isCall/isIndirect` BTB 元数据）只彼此
服务。两套活动语料（18 例 + IPC 6 例）中 JALR 全部是返回或调用，唯一真间接站点是 `towers`
的 `auipc + jalr x0, -924(x6)`，目标固定，TC 与 BTB last-target 相同；收益不可观测，遂按
面积/效率权衡删除。模板 Register 状态再省 **3,232 bit**（BHT 2,048 + TC 1,024 + valid 32 +
BTB `isCall/isIndirect` 128），完整 BPU 由 **12,558 bit** 降至 **9,326 bit**（相对 TAGE
基线 24,357 bit 为 −61.71%）。
`Plan` 的 `T_BHT/T_TC/T_TCV/T_BTB_CALL/T_BTB_IND` 与 ROB `isCall` 链路一并删除；
主树与模板树 Release 18/18 x10+clock 逐位一致，IPC 6/6 不变。

### FlushArbiter needSquash

四个请求槽删除了逐槽 `needSquash` Register。BRU、CDB、AGU 三条插入路径只会插入真实 squash，
所以不变量是 `requests[i].valid => needSquash`。清除只撤 valid，压缩只搬移有效槽，广播时
“存在最老有效槽”即可组合生成 `needSquash=1`。该压缩省去 4 bit 状态和相应写驱动，不改变协议。

### DIV busy

DIV 删除独立 `busy` Register，逐拍不变量为：

`busy == prepareValid || loopValid || fullAdderValid`

普通输入进入 prepare，迭代在 loop 中推进，最后进入 full-adder；特殊值快路直接产生 result。
`canAccept()` 直接检查三个阶段 valid 与 `resultValid`，因此无需再维护一个会重复表达同一 FSM
占用状态的 bit。flush、接收和结果排空的写冲突也随之减少。

IMEM 的占用计数采用同一思想：当 `valid[]` 与环形窗口有严格不变量时，occupancy、full 和
head-ready 可由位图归约得到，不必保留容易与槽状态失配的冗余计数 Register。

## 固定归约与可综合循环

### ROB ready 写意图

ROB 原先用动态长度 `seen[]` 线性查重，既有经验上界越界风险，也不符合固定硬件结构。
现在所有 BRU、SQ、四路 CDB ready 请求先校验完整 RobTag，再幂等归约到零初始化的
逐槽 `readyWrite[]/readyData[]` 写意图；issue 初始化与完成置位按固定优先级归并，最后
固定遍历 `ROB_CAP`，每槽至多执行一次 `isCommitReady <= ...`。`{}` 零初始化与单点写
mux 都是承重条件；遗漏会产生幽灵 ready 或 Register 双写。

### 固定四 lane 字节通路

DMEM/DCache 中 `i < n_bytes` 的运行期循环全部改为固定 `i < 4`，`i < n_bytes` 只作为 lane enable。
这样 1/2/4-byte load/store 明确综合为四路字节网络，而不是数据相关回边；1B/2B 符号扩展也改为
无符号显式掩码，消除了 `1 << 31` 一类宿主 UB。

### PRF 自由表 packed 序号

自由表指针与 checkpoint 由裸 `uint32_t` 计数器改为 packed 环形序号
`{1-bit epoch, bit_width(PRF_CAP-1)-bit index}`。index 只分配 `0..PRF_CAP-1`，字段内
其余编码为空洞；`prfSeqNext` 在末槽翻转 epoch 并归零 index，`prfSlot` 只投影 index。
`prfSeqDistance` 以 index 差加 epoch 对应的 `±PRF_CAP` 重建模 `2*PRF_CAP` 距离，
因此非 2 次幂容量仍能检出过期 checkpoint，且数据通路全程无乘除。checkpoint 写入
**分配后的 canonical head**，squash 恢复可与严格更老 head 的同拍 commit 并存。
PRF33..64 的 seq/checkpoint 载体均为 7 bit，PRF65..128 为 8 bit；phy tag 载体为派生
`PHY_TAG_WIDTH`（当前 P48 为 6 bit）。

非 2 次幂门禁在最终 `CKPT_CAP=32` 配置下完成：P64 主树/模板 Release/模板 `_DEBUG`
均为 18/18 x10+cycles 对活动 golden；P48 Release 18/18 双树逐拍一致且 `_DEBUG`
magic/qsort/tak 零断言；P65 重点+快用例 12/12、`_DEBUG` 3/3；P33 的
magic/qsort/tak 3/3 x10 对 golden且双树 clock 一致（pi 因极限停顿按计划跳过）。
CKPT32 下 PRF seq/checkpoint 存储为 P33..64 的 238 bit、P65..128 的 272 bit；
checkpoint 数组深度 64→32 另在 BPU/RAT/PRF 合计节省 10,592 bit。

### 运输载体就紧（PHY_TAG_WIDTH / CKPT_ID_WIDTH / 队列指针）

容量缩减期有意保留的超宽运输载体已按派生宽度收紧：`include/common.h` 新增
`PHY_TAG_WIDTH = bit_width(PRF_CAP-1)`、`CKPT_ID_WIDTH = bit_width(CKPT_CAP-1)`、
`FQ/IQ/LQ/SQ_PTR_WIDTH = bit_width(CAP-1)` 及对应 `static_assert`；RAT/PRF/ROB/RS、四条 CDB
与七个 RS payload 的 phy tag 由 7→6 bit，ckptId 由 6/8→5 bit，FQ/IQ/LQ/SQ 指针及 ROB 的
LSQ snapshots 收到逻辑宽度（2/2/3/3 bit）。`memIndex` 仍保持 7-bit 编码（store 判别位
bit6），PRF `{epoch,index}` 序号、RobTag 与 BPU 的 256 回绕计数器同样有意不动
（回卷代数语义，不是槽索引）。

纯载体收窄，行为等价：模板 Release 18/18 x10+cycles 对 golden；`_DEBUG` 定向
gcd/magic/qsort/tak/multiarray 全对且零断言；IPC 语料双树（主树 `./code` 与本树 `code`）
逐例 clock/IPC 逐位一致，cycles 加权 IPC **0.696150**。全模板共省 **1,253 bit** 状态位
（phy 1,178 + ckpt 33 + 指针/快照 42）。本次同时用当前基线刷新了
`docs/ipc_benchmarks.md`（旧表为更早容量配置遗留）。

### 固定容量 FlushArbiter

有序插入不再使用 `while (pos < w)`。实现固定扫描 `FLUSHARBITER_CAP=4`，用 `scanning` 在首个
不满足年龄关系的槽后冻结决策；后移也固定遍历四槽，`i > pos` 仅作为写使能。
模板同拍请求仍按 BRU -> CDB -> AGU 的既定优先顺序合并，最后每个槽只有一个 Register 写点。
固定数组的动态索引会综合为 mux，不需要为了“消灭动态索引”再造另一套可变循环。

真正数据相关的 DIV 迭代没有强行展开，而是用有限位宽计数器和 FSM 每拍复用一次 SRT 数据通路。
这也是可综合的：不可接受的是宿主 `while` 在一拍内跑到收敛，不是多周期硬件本身。

## 2026-09-20：BPU 写口收口（Plan::nTab 删除 + RAS times 位宽）

模板 BPU 的 `Plan`（`tab[32] + nTab` + `mergeIn` 查重/写入 + `apply` switch）整体删除，
改为每个训练口一个固定形状的 `BTBWriteIntent`（`lineWrite`/`targetWrite` + `index` + 载荷）。
方向表（localPHT/globalPHT/selector/condSeen）只有 BRU 条件路径一个写口，改为直接写；BTB 的
`actualPC/valid/unconditional/isRet` 与 `target` 分两组按 `fetch > cdb > bru` 仲裁，同行冲突用
`sameBTBLine` 抑制，每个物理 Register 每拍至多一次赋值，双训练口仍共享周期初旧快照。数据通路
循环审计统计 138 个循环、运行期边界 **0**（原 3 处随 Plan 删除）。

三写口 6 种仲裁顺序做了编译期可切换的全量 A/B（18 例 × 6 组，`BPU_BTB_PRIO` 临时脚手架，
已删除）：逐例 `x10`、clock、retired 完全一致，总 clock 均为 **12,237,892**。语料中不存在
"异值同行冲突"，仲裁顺序不可观测，因此保留与主树写序一致的 `fetch > cdb > bru`。

同时把 SARAS 的 `times` 载体从 `Register<32>` 收紧为 **`RAS_TIMES_WIDTH = 9`**（`RASEntry` 与
`AlignEntry` 共 24 处，省 552 bit；完整 BPU 状态 9,326 → **8,774 bit**，相对 TAGE 基线
24,357 bit 为 −63.98%）。9 bit 是保持 golden 的最小位宽：queens 的投机 dedup 链峰值 **323**
（次高 tak 16），8 bit 在 queens 上回绕并导致 CYCLE FAIL；该计数没有容量可推导的静态上界
（dedup 链可超出 `ALIGNQ_CAP` 回卷窗口），溢出只退化预测，不影响体系结构状态。

门禁：Release 18/18 x10+cycles 逐位一致（总 clock 12,237,892、IPC 0.553703、分支
93.8367%），`_DEBUG` 全量 18/18 零双写断言。

## 2026-09-21：BPU GHR / checkpoint 载体收紧

Tournament 的 globalPHT 与 selector 都以 `((PC>>2)^GHR)&255` 取索引，GHR 高 8 位从未被
消费。两树引入 `GHR_WIDTH=8` 与容量断言，live GHR 和 32 份 checkpoint 中的 GHR 同步由
16 bit 收紧为 8 bit，省 **8 + 32×8 = 264 bit**。

`alignHead` 仅在 checkpoint snapshot/restore 间来回复制，不推进、不参与 AlignQueue 索引且无
外部消费者，故从 `TargetPred`、`BPUSnapshot` 和其 32 份 Register 镜像删除，另省
**8 + 32×8 = 264 bit**。完整模板 BPU Register 状态由 **8,774 → 8,246 bit**，相对
TAGE 基线 24,357 bit 为 **−66.15%**。该变换只删除未观察状态，预期 x10 与 clock 均逐位不变。
