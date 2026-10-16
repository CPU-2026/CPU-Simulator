# RISC-V Simulator Template 使用指南

## 当前运行规则（2026-10-16）

默认加载`data/course-testcases/*/program.data`原始课程镜像。D$4 KiB/2-way/4 MSHR/8请求槽，I/D共享AXI4-Lite；SW写`0x80000000`，B握手结束并打印完整32-bit结果。`test.sh`跑25例，`test_IPC.sh`跑6个perf例，BP_BIN可选择外部Release构建。
根级`python3 data/course.py test --binary <主树> --peer <模板>`同时核验结果、课程动态退休数及逐例总/core/drain周期；`smoke`检查合法255指令/完整32bit/栈与RAM顶/字节扩展/并发冷Load/脏逐出/错误路径退出。旧镜像只可`LEGACY_HALT=1`运行，其旧周期表保留为历史。
断言构建使用`-DCMAKE_CXX_FLAGS=-D_DEBUG -DCMAKE_CXX_FLAGS_RELEASE='-O3 -UNDEBUG'`，确保Release默认的`-DNDEBUG`不遮蔽标准断言。CPU冻结核心时仍sync所有模块，不能只清transport的Wire。

> **当前默认（2026-10-14）**：D$已按RTL切到4 KiB/128组/1路/32 B，metadata为
> tag20＋dirty1。首个合格回填拍只返回一次Load，第4拍才安装完整行与消费DMEM回复。
> `run_sram_dcache.py`默认检查三端真实配置，不改测试副本几何；`run_sram_cpu.py`
> 不带`--geometry`时验证真实默认，并记录`actual_geometry`供early-restart专项使用。
> I$与Register单写/统一sync不变；旧8 KiB报告不作为当前源码验证证据。

> **4 KiB几何扫描**：`sync/dcache_sweep.py`在私有副本中编译两软件和转换RTL，
> 不覆盖仓库`code`或临时改写生产常量；`--reuse`保留完成项、`--synth-jobs`只并行
> 独立转换快照。`run_sram_cpu.py --geometry LINE SETS WAYS --out ...`同样只改验证副本。
> 更改行大小不改变I$；1/2/4路统一tree-PLRU，嵌套SRAM逐路work、正常统一sync。

> **2026-10-13 early restart**：`loadResponded: Register<1>`仅属于当前Cache队首。
> 首个合格回填拍（仍等待dirty写实际完成）从旧DMEM完整行供Load字节并单写置位；
> 取消Load也置位，不重复返回。直到末拍才清位/释放队首/消费整行，valid首拍清末拍置；
> Load响应与嵌套SRAM不增加寄存级，各work完成后统一sync。立即HALT不排空。
> 最新三端/软件四构建验证与活动周期见共同父目录sync/README.md；下面较早统计为历史。

> **2026-10-13 valid寄存器更新**：D$ valid为128项Register<1>，metadata SRAM仅tag19+dirty1；
> 查询沿采样readLineValid，不提前sync，不从新写SRAM读取。第8拍填完并置valid，各Register
> 每拍单写。无SRAM初始化扫描；最新对拍/回归入口为共同父目录sync/README.md，下面为历史阶段验收。

> **当前同步/结束规则（2026-10-13）**：D$采用4项请求队列和嵌套data/metadata SRAM，
> HALT提交立即结束、无SQ/Cache/DMEM排空。Release/`_DEBUG`各18 ISA＋6 IPC严格匹配
> 根级golden及主树结果/周期/退休数/保留统计；clock总计12,987,988/759,991，drain=0。
> 本轮报告位于`/tmp/opencode/tpl-dcache-regression.json`及`tpl-dcache-directed.json`。
> 此处数值为早期SRAM单拍回填历史记录；当前RTL已经同步，最新验收不沿用旧全dirty门禁。

> **模板同步要点**：Load响应和DMEM请求保持组合Wire，不能恢复旧lb/rq输出寄存级。
> 请求n拍入队、n+1拍由旧队首查询SRAM、命中n+2拍服务；旧full时即使完成也不补入。
> Store缺失回填要合并Store字节，取消Load需保存跨拍cancelled而非仅查当拍squash。
> 两SRAM各work一次，由DCache父模块正常sync提交，不独立注册、不提前sync。
> 七组真实程序通过全128组/子字/回绕/延迟写/保持回复/恢复/HALT，且普通、无效输出
> poison、乱序work及二者组合均与主树和独立退休参考一致；IPC qsort另验139893条退休。

> **CPU顶层状态**：仅保留initialized Register，CPU::clock在work前单写、全部work后
> sync_member一次；finishPhase及结束FSM已删除。仍为27个注册模块，不额外同步模块输出。

> **部分项/完整整核门禁（SPEC §63/64）**：根目录`run_partial_acceptance.py`与
> `run_cpu_benchmarks.py`。前者15程序3835/2872及4源码负向，后者24镜像12667040
> 拍无损pre/post/独立退休/最终128KiB全等。gzip非抽样；长pi须给足时间，实测RTL
> 约900秒/单次本环境，分次报告可保留case_origin汇总。非法opcode取消/提交另用
> `run_exception_cpu.py`，新exception Wire需接所有ROB夹具，入口sync/README §55/56。

> **结束/计时门禁（SPEC §62）**：根目录`python3 -B sync/run_completion_cpu.py`。
> boot1拍隔离、core在HALT提交冻结、全部dirty实际写完才DONE；16程序9629/2872、
> 最终128KiB核验/DONE保持16拍/7负向通过。`build_software.py`生成/tmp四构建，
> 两树Release/_DEBUG各18＋6 strict新total11973998/693042，core保持；README §54。

> **恢复门禁（SPEC §61）**：根目录`ulimit -c 0 && python3 -B sync/run_recovery_cpu.py --quiet`，
> 固定67程序9001/2872严格全等、覆盖齐全及11源码负向。`--no-negative`完整正向可
> 用`--resume-negative <目录>`接续，要求源码摘要完全一致。子集/现有binary为诊断，
> 默认退出码要求完整验收成功，断言失败本身不算负向有效。入口sync/README §53。

> **BTB/参数门禁（SPEC §60）**：根目录运行`python3 -B sync/run_btb_acceptance.py`
> 与`python3 -B sync/run_bpu_parameters.py`。前者26真实三CPU程序6690/1848全等、
> 七实际源码负向；后者13合法/9非法RTL引脚cases与22软件header约束通过。软件
> 算法/活动表保持，完整命令及诊断子集边界sync/README §52。

> **用户RAS/checkpoint门禁（SPEC §59）**：`python3 -B sync/run_ras_cpu.py`运行完整
> 三CPU：33程序13041拍1848字段严格同值，7实际源码功能负向有效。JALR双link含
> 同寄存器先pop再push，checkpoint一条接受取指一次分配，恢复完整arch内容＋存活
> ROB含边界。BPU输入robNextTag必须接真实尾；HALT PC元数据已补齐。strict-cycles
> 现也核验IPC表，ISA11967396保持，六IPC新691640（+826）。入口README §51。

> **前端门禁（SPEC §58）**：`python3 -B sync/run_icache_sync.py`（2324/160）与
> `python3 -B sync/run_frontend_sync.py`（2259/216）驱动实际模块并严格比较四方。
> 模板所有work后sync，pre/post换输入仅清Input Wire与fillFire/readBlocked/
> SRAM Input缓存，不能对含请求Registers的ICacheOutput再次sync。两构建各18＋6
> 严格golden保持，真实SRAM macro/tag结构和7实际负向通过，入口README §50。

> **Store完成门禁（SPEC §57）**：`python3 -B sync/run_cache_chain.py`现比较全部764
> 字段，取消Store ready屏蔽；当前SQ组合输出槽位/epoch两条Wire，ROB校验并单写Register（§67），
> 无同拍新地址/数据完成旁路。Release/_DEBUG各18 ISA＋6 IPC strict-cycles通过；
> 位图身份/高索引/恢复探针为`python3 -B sync/run_capacity.py tb_store_ready`。

> **当前Cache门禁（SPEC §55）**：`python3 -B sync/run_dcache_sync.py`驱动实际两
> 软件Cache/DMEM及寄存器RTL，6378/14全等，_DEBUG单写、全部128组、冷热/子字及
> 并行脏请求通过；真实两个RTL负向有效。默认只做128×512bit时钟bank展开检查，
> `--synth-check`是慢通用映射，曾300秒超时，不宣称技术综合完成。命令见README §47。

> **最新接口/门禁（SPEC §52）**：Cache准入为canSendLoadToCache(addr,nBytes,tag)，
> 实际Load宽度参与任意字节重叠检查，组合Wire为loadCanSendToCache。扩展转发链仍
> 22 suite2443步323字段两软件全等；完整CPU两构建各19/19、RTL core12/12。
> run_main_forward_assessment.py包含四个迟到数据偏移镜像、排空对照及实际旧相等
> 条件重编负向。两软件Release/_DEBUG各18＋6严格回归保持golden；命令/报告见
> sync/README §44。下面§51及更早章节为此前轮次记录。

> **最新入口（SPEC §51）**：`python3 -B sync/run_store_forward.py --extended`，2443步
> 323字段22 suite两软件逐拍一致、三端架构全正确；加`--main-core PATH --template-core PATH`
> 验两完整CPU各11个镜像和RTL core8/8。7组正常策略周期差使`--require-equivalent`
> 仍返回1；默认观察要求软件/RTL各端正确，不能靠两软件同错通过。模板完整Release与
> _DEBUG均18＋6通过，Register单写无断言；活动golden保持。临时CMake输出、完整命令
> 与报告见sync/README §43；下面§46的错值属于修复前记录。

> **最新观察入口**：`python3 -B sync/run_store_forward.py`，1338步323字段，观察退出0、
> `--require-equivalent`实际退出1。两软件全等，8个转发suite与RTL不同；加
> `--main-core PATH --template-core PATH`验证正常DIVU/SB/Load镜像两软件错值及RTL
> core3/3正确，排空控制正确。具体例子README §38/SPEC §46及sync差异文档；生产/golden保持。

> **当前入口**：`python3 -B sync/run_lsq_lifecycle.py --require-equivalent`，LQ1432步
> 816字段、SQ872步340字段四方一致，退出0；SQ按软件退休/缓冲/发送，旧耦合RTL实际
> 重编负向有效。RTL14/14和两软件18＋6strict-cycles通过，活动golden保持；README
> §37/SPEC §45记录完整协议及范围。下列§42～44退出码是历史阶段记录。

> 当前LQ提交释放入口`python3 -B sync/run_lsq_lifecycle.py`要求lq_three_way_equivalent=true，
> 1432步816字段全suite一致；模板新增robWillCommit实际提交Wire，RTL接robRetireInput。
> SQ合同仍不同，整体严格模式仍返回1。18＋6新golden严格通过，README §36/SPEC §44。

> 当前第一项修复：`python3 -B sync/run_lsq_lifecycle.py`要求lq_data_correct=true，
> 主树/模板/RTL正确CDB及架构值通过，并实际编译旧顺序作负向（SPEC §43、README §35）。
> 全部生命周期严格模式仍因后两项合同差异返回1，不再因主树错值失败。18＋6严格回归通过。

> 第12步入口：`python3 -B sync/run_lsq_lifecycle.py`，定向加`--random-steps 0`，
> `--require-equivalent`当前返回1。LQ1365步816字段/SQ565步272字段观察验证：生命周期
> 合同不同，主树新分配/旧返回同拍错值，模板/RTL正确。退出0表示问题复现成功，
> 不是整体等价；协议、范围及首次分歧见README §34、SPEC §42。

> 第11步入口：共同目录`python3 -B sync/run_wb_chain.py`，定向加`--random-steps 0`。
> 真实四生产者/CDB共享PRF/ROB，默认3648步590字段、定向2641步四方一致；16掩码、
> 四WB/提交/issue、下拍依赖及给定真实JALR恢复通过。输入没有WB答案，内存按请求返回。
> 真实单PRF口负向检出故障，生产及golden保持；完整scope/协议见README §33、SPEC §41。

> 第10步定版：`python3 -B sync/run_div.py --require-equivalent`和
> `python3 -B sync/run_div_chain.py`均返回0；纯定向分别加`--random-cases 0`/`--random-steps 0`。
> 模块55256步28字段及真实返回链5000步500字段四方一致；消费同拍接收、严格年轻flush/
> 边界保护已实施，outReady Wire正确接线，接收分支收敛resultValid单写。18 ISA clock
> 新golden11,967,406，六IPC保持690,826。范围及协议见sync/README.md §32、SPEC §40。

> 第10步现状入口：共同目录`python3 -B sync/run_div.py`，纯定向加`--random-cases 0`。
> 默认55210步28字段，独立数值/自身控制参考通过，两软件相同；消费同拍准入和边界flush
> 与RTL不同。`--require-equivalent`当前返回1，退出0的现状模式不表示三端整体等价。
> 生产work未改，只加DIVTester只读友元；范围及协议见sync/README.md §31、SPEC §39。

> 第9步定版入口：共同目录`python3 -B sync/run_mul.py`和`python3 -B sync/run_mul_chain.py`，
> 纯定向均加`--random-steps 0`。模块2207步/4414点21字段所有suite四方一致，真实返回链
> 2302步/4604点508字段四方一致；纯定向978/1292步通过。三级弹性/单槽结果、关闭级整体保持，
> n+3 CDB/n+4依赖；模板正向mulCanAccept接partialCanAccept，各Register条件单写。
> 背压是夹具合成输入，恢复为给定合法ADD边界；完整scope见sync/README.md §30、SPEC §38。

> 第8步定版入口：共同目录`python3 sync/run_control_chain.py`，定向加`--random-instructions 0`；
> 默认1518步/3036点2230字段真实RAT/PRF/RS/执行/BPU/提交/恢复四方一致。
> `run_control_observe.py --aligned-only`2306步共同字段一致；BPU训练实际taken，misAlign排除。
> 软件18+6总clock12,246,204，完整边界见sync/README.md §27和SPEC §35。

> 第8步现状入口：共同目录`python3 sync/run_control_observe.py`（定向加`--random-steps 0`）。
> 默认2374步/4748点56字段，各端符合各自现状参考；正常PC域一致，训练方向/未对齐差异保留。
> 退出0为观察成功，不代表整体三端等价；完整scope及目标草案见sync/README.md §26、SPEC §34。

> 第7步地址整链：共同目录`python3 sync/run_agu_chain.py`（纯定向加`--random-steps 0`），
> 1195步/2390点200字段normal/identity四方一致；接收与Store地址广播匹配生产者tag。
> WB/StoreValue是上游输入，consumer提交及Cache返回另验，详见sync/README.md §25。

> 第7步入口：共同目录`python3 sync/run_agu.py`验证真实三端AGU单槽结果级，3182步/
> 6364点与独立参考一致；`python3 sync/run_capacity.py tb_agu_lsu`验证RTL真实LSU接收。
> 正向接口aguCanAccept，n拍计算/保存、n+1交付、n+2地址ready可见；详见sync/README.md §24。

> 第6步整链入口：共同目录`python3 sync/run_alu_chain.py`（纯定向加
> `--random-steps 0`）。当前普通整数2536步/5072点、474字段三端与独立参考全等，
> CDB数据来自实际ALU，真实派发准入与PRF/ROB提交已串联。详见sync/README.md第23节。

> 第4步`python3 sync/run_rename.py`验证真实ROB/RAT/PRF与独立oracle；软件
> matchesTag存活窗口和RTL FIFO分配均已对齐，默认1293步/2586点、420字段四方
> 完全相同，返回0；`--random-steps 0`纯定向293步/586点也通过。报告逐端列出结果。

> 第3步3A+3B已完成：共同目录`python3 sync/run_iq.py`验证主树、模板、RTL
> FQ/IQ/HALT与独立deque oracle；`sync/run_capacity.py`验证RTL取指预留及ICache
> 预算；`sync/run_cache_lookahead.py`验证同行回填直接供数给新槽，模板Register单写。
> 两树18+6与新活动golden匹配，总clock12,246,204。

> **2026-10-05**：公共容量同步3A已落地（FQ/IQ4、LQ/SQ8 全容量 epoch，D$8 KB）；
> 18+6例与主树完整结果/周期/统计一致。原16 KB基线留作历史，当前活动表已复核更新。
> 对拍/容量测试与主树 tick 只写审计入口见共同父目录 sync/README.md。

本文是 `RISC-V-Simulator-Template/` 的实际使用入口。除特别说明外，命令都在该目录执行。

## 1. 仓库用途

本目录包含两部分：

- `include/` 是 header-only 的 C++20 周期建模框架，提供 `Bit`、`Register`、`Wire`、
  `dark::Module` 和 `dark::CPU`。
- `src/` 是框架上的完整 RV32IM Tomasulo 风格乱序 CPU 模型，用于验证 ISA 结果、逐周期时序、
  分支恢复、Cache、MUL/DIV 和性能取舍。

它是面向 RTL 的周期模型，不是只执行指令语义的功能模拟器，也不能把整份 C++ 直接交给 HLS
综合。组合逻辑是受支持的：通常由 `Wire` lambda、普通纯函数以及无状态 `Module` 表达；
`Register` 则表示时序状态。

## 2. 环境与构建

### 2.1 前置条件

- CMake 3.10 或更高版本，建议使用较新的 CMake。
- 支持 C++20 的宿主编译器，推荐 GCC 12 或更高版本。
- Linux 或 WSL。回归脚本还使用 Bash、`awk`、`grep`、`sed` 等常见命令行工具。
- 长用例必须使用 Release；未优化构建会显著变慢。

项目通过 CMake 把 `src/include/` 和 `include/` 加入头文件搜索路径。使用框架时采用当前写法：

```cpp
#include "tools.h"
```

不要写旧文档中的 `#include "include/tools.h"`。

### 2.2 Release 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

未显式指定 `CMAKE_BUILD_TYPE` 时，本目录的 CMake 也会默认选择 `Release`。默认可执行文件不是
`build/code`，而是模板目录根部的：

```text
./code
```

现代 CMake 可给构建命令追加 `--parallel`。若希望产物留在构建目录，配置 cache 变量
`CPU_RUNTIME_OUTPUT_DIRECTORY`：

```bash
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCPU_RUNTIME_OUTPUT_DIRECTORY="$PWD/build-release"
cmake --build build-release
./build-release/code < data/testcases/gcd.data
```

不同 build 目录若都使用默认输出位置，会依次覆盖同一个 `./code`；并行维护 Release 与检查版时，
建议为每个构建指定独立输出目录。

### 2.3 `_DEBUG` 框架检查版

`_DEBUG` 启用框架自己的检查，包括未接线 `Wire`、`Wire` 重复接线、同周期 `Register` 双写和
动态切片越界。它与 CMake 的 `Debug` 构建类型、标准 C/C++ 的 `NDEBUG` 不是同一个开关。
推荐在 Release 优化下单独定义 `_DEBUG`：

```bash
cmake -S . -B build-assert \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS=-D_DEBUG \
  -DCPU_RUNTIME_OUTPUT_DIRECTORY="$PWD/build-assert"
cmake --build build-assert
./build-assert/code < data/testcases/gcd.data
```

Release 通常仍会定义 `NDEBUG`，因此普通 `assert` 可能关闭，而框架 `_DEBUG` 检查仍然开启。

## 3. 运行程序与查看结果

模拟器不接收程序路径参数，而是从标准输入读取课程使用的 Verilog hexadecimal `.data` 镜像：

```bash
./code < data/testcases/gcd.data
# 178
```

标准输出只有停机时架构寄存器 `x10` 的低 8 位，即 `x10 & 0xff`。`gcd.data` 的当前结果是
`178`。输入程序必须包含本仓库约定的 HALT 指令字 `0x0ff00513`；否则顶层会持续运行。

### 3.1 `VERBOSE` 诊断

诊断写到标准错误，不会污染标准输出的 x10：

```bash
VERBOSE=clock ./code < data/testcases/gcd.data
```

当前输出包含：

```text
clock: 186
timing: init=1 core=186 drain=0 total=186
ipc: 0.220430 retired=41 cycles=186
178
```

`clock` 与core/IPC周期相同，均截止HALT提交拍；初始化单列，drain恒为0。
只提取标准错误统计时可用：

```bash
VERBOSE=clock ./code < data/testcases/gcd.data 2>&1 >/dev/null
```

`VERBOSE` 接受逗号分隔的主题，`1` 或 `all` 打开全部主题：

| 主题 | 用途 |
| --- | --- |
| `clock` | 结束时打印 clock、retired 和 IPC |
| `branch` | 结束时打印整体分支预测准确率 |
| `icache`、`dcache` | 两个名字目前都打开同一组 ICache/DCache 汇总 |
生产代码只保留活动表所需汇总；逐指令轨迹、物理寄存器事件、PROFILE、RAS峰值、
CDB争用和每PC错误定位已删除，旧诊断主题不再解析。

例如：

```bash
VERBOSE=clock,branch,icache ./code < data/testcases/gcd.data
```

只比较周期时使用 `VERBOSE=clock`；`branch` 和 `all` 也只输出结束汇总。

## 4. 回归与性能脚本

模板目录中的两个脚本以父仓库文档为权威数据源。相对模板目录的根级 SSOT 路径是
`../docs/benchmarks.md` 和 `../docs/ipc_benchmarks.md`；模板 `docs/` 不再保留同名副本。

| 脚本 | 输入 | 根级 SSOT 或输出 | 行为 |
| --- | --- | --- | --- |
| `./test.sh [pattern]` | `data/testcases/*.data` | `../docs/benchmarks.md` | 严格检查 RV32IM 镜像的退出码、x10 与 cycles，报告 retired、IPC、分支率；完整运行含 `pi` |
| `./test_IPC.sh` | `data/testcases_ipc/*/*.data` | 写入 `../docs/ipc_benchmarks.md` | 要求每例自校验 `x10 == 0`，全通过后重新生成根级报告 |

常用命令：

```bash
./test.sh gcd
./test.sh '*sort*'
./test.sh

BP_BIN=./build-assert/code ./test.sh gcd
BP_BIN=./build-release/code ./test_IPC.sh
```

`BP_BIN` 可覆盖待测二进制。`test.sh` 将 x10 和 cycles 都作为硬门槛，同时拒绝缺失语料、
缺失 golden 与零用例。`test_IPC.sh` 在任一自校验失败时保持原报告不变；运行前应确认
确实要改写根级 IPC 报告。

**2026-10-13 基线说明**：两树1 KB/64 B SRAM ICache及8 KB/64 B data/metadata SRAM DCache
已对齐；Release与`_DEBUG`各18＋6匹配共同活动表，ISA总clock **12,987,988**，
六IPC总clock **759,991**，精确几何IPC **0.576544287649**。
`test.sh` 始终严格检查 result 与 cycles。同步范围和当前 n+1 队首交付时序见
[cache.md](cache.md) 与本页开头的同步说明。

## 5. 文档地图

当前实现说明优先阅读以下本地文档：

| 文档 | 内容 |
| --- | --- |
| [README](../README.md) | 总体架构、程序镜像、流水线和目录结构 |
| [backend.md](backend.md) | 发射、派发、写回、提交、MUL 与 SRT DIV；后端算法 SSOT |
| [frontend.md](frontend.md) | 取指、预译码、译码、Tournament/BTB/RAS 与恢复 |
| [cache.md](cache.md) | ICache、DCache、IMEM/DMEM 和延迟模型 |
| [memory.md](memory.md) | LQ/SQ、转发、违例检测与内存请求准入 |
| [progress.md](progress.md) | 从旧快照模型迁移到 `work()+sync()` 的回顾、验证政策和关键经验 |
| [fmax-critical-path-analysis.md](fmax-critical-path-analysis.md) | RTL 映射后的关键路径和 fmax 风险 |

跨两棵实现共用或由根级脚本生成的文档，以父仓库版本为准：

| 根级文档 | 内容 |
| --- | --- |
| [../../docs/benchmarks.md](../../docs/benchmarks.md) | x10、cycles、IPC、分支和 Cache 的回归 SSOT |
| [../../docs/process.md](../../docs/process.md) | 大数组 SRAM 化阶段、端口契约与当前验证结果 |
| [../../docs/ipc_benchmarks.md](../../docs/ipc_benchmarks.md) | `test_IPC.sh` 生成的 IPC 报告 |

## 6. 一拍到底发生什么

框架的核心周期语义如下：

```text
本拍开始：所有 Register 读取 _M_old
    |
    +-- dark::CPU 先递增 cycles
    +-- 依次调用每个 Module::work()
    |     Wire 首次读取时执行 lambda，此后本拍读缓存
    |     reg <= value 只写 _M_new，立即重读 reg 仍是 _M_old
    |
本拍结束：依次 Module::sync()
          Register: _M_old = _M_new
          Wire: 清除本拍 lazy cache
          _DEBUG: 清除 Register 的本拍已写标记
```

因此，正确模块只读取本拍稳定的旧状态，通过 `<=` 安排下拍状态。跨模块组合通路可以由多层
Wire 组成，只要它是一张无环 DAG；组合回路会递归求值，不能使用。

## 7. 值类型

三种值类型的位宽范围都是 1 到 32 bit。原生整数写入较窄类型时会像 Verilog 一样截断；两个
框架位类型之间运算或赋值通常要求位宽一致。读取框架值需要显式转换，或用一元 `+` 取得同宽
`Bit`。

### 7.1 `Bit<N>`：组合中间值

`Bit` 是可复制的定宽临时值，适合组合计算、切片和拼接，不保存跨拍状态：

```cpp
Bit<8> value = 0x1ff;                 // 截断为 0xff
value.set<7, 4>(0xAu);                // 写 [7:4]
Bit<4> low = value.range<3, 0>();     // 固定区间
Bit<4> mid = value.slice<4>(0);       // 从 bit 0 起取 4 bit
Bit<8> joined{value.range<7, 4>(), low};
```

还可使用 `sign_extend`、`zero_extend`、`to_signed` 和 `to_unsigned`。`range` 的上下界是模板参数，
`slice` 的起点可在运行时给出。

### 7.2 `Register<N>`：old/new 双缓冲寄存器

```cpp
Register<8> count;
count <= count + 1;                   // 读 old，写 new
```

- 默认构造后 old/new 都是 0，不能通过构造函数直接设置非零初值；非零复位值应通过明确的
  boot/reset 周期写入。
- 本拍 `count <= ...` 后再次读取 `count`，看到的仍是 `_M_old`。
- 周期末 `sync()` 才把 `_M_new` 提交为 `_M_old`。
- 本拍不写时保持上次 `_M_new`，也就是保持寄存器值。
- `_DEBUG` 下同一寄存器一拍只能执行一次 `<=`，即使两次写入相同值也算双写。
- `Register` 禁止复制和移动，不能用 `memcpy`、`memset` 或按值传参处理。

双写通常应改成先计算唯一的 next value，再在单一位置执行一次 `<=`；不要依赖 C++ 语句顺序让
“最后一次写胜出”。

### 7.3 `Wire<N>`：一次接线、每拍懒求值

`Wire` 保存的是求值函数，不是一个可在每拍反复赋值的变量：

```cpp
Register<8> source;
Wire<8> direct;
Wire<8> plusOne;

direct = source;                       // 直接连接同宽 Register
plusOne = [&] { return source + 1; };  // 组合逻辑，返回 Bit<8>
```

- 每根 Wire 在初始化/接线阶段连接一次，通常放在模块构造函数或顶层 `wire()` 中。
- 不要在 `work()` 中给 Wire 重新赋 lambda；`_DEBUG` 会报告 `Wire is assigned twice`。
- 每拍第一次读取时才执行 lambda，并缓存结果；同拍后续读取复用缓存。
- 周期末模块 `sync()` 只清除 Wire 缓存，不改变连接，下一拍首次读取会重新求值。
- 未连接 Wire 被读取时，`_DEBUG` 会报告 `Empty wire is called`。
- Wire 同样禁止复制和移动。lambda 必须捕获生命周期足够长的对象。

返回 `Register` 本身会尝试复制时，可直接使用 `wire = reg`，或显式返回引用/`Bit`：

```cpp
Wire<8> byRef = [&]() -> const auto & { return source; };
Wire<8> byBit = [&] { return +source; };
```

Wire 必须属于已注册模块的 Input/Output/Inner，或由调用者显式执行 `sync_member`。把裸 Wire 放在
CPU 普通成员中却不参与同步，会导致 lazy cache 跨拍不清、读到陈旧值。

## 8. 聚合、同步与 `Module`

### 8.1 可同步聚合

`sync_member(x)` 递归支持：

- 自身提供 `sync()` 的对象，例如 `Register`、`Wire` 和 `Module`。
- `std::array`，元素继续递归同步。
- 满足 `std::is_aggregate_v` 的简单聚合，成员继续递归同步。
- 使用 `SyncTags<Base...>` 声明同步基类的类型。

普通反射聚合的有效上限是 64 个直接成员。超过时应按语义嵌套子结构；一个 `std::array` 只算
一个直接成员，数组元素会递归处理。C 风格数组不受支持，统一改用 `std::array`。

Input、Output 和 Inner 应使用简单 `struct`，避免用户自定义构造函数、私有数据成员等破坏聚合
属性。需要同步私有基类时使用标签：

```cpp
struct Ports { Wire<1> ready; };
struct State { Register<8> count; };

struct Combined : private State, public Ports {
  friend class dark::Visitor;
  using Tags = dark::SyncTags<Ports, State>;
};
```

### 8.2 定义模块

模块由 Input、Output 和可选 Inner 三个聚合组成：

```cpp
#include "tools.h"

struct CounterInput {
  Wire<1> enable;
};

struct CounterOutput {
  Wire<8> value;
};

struct CounterInner {
  Register<8> count;
};

struct Counter : dark::Module<CounterInput, CounterOutput, CounterInner> {
  Counter() {
    value = [this] { return +count; }; // Output Wire 只接一次
  }

  void work() override {
    if (static_cast<bool>(enable))
      count <= count + 1;
  }
};
```

`Module::sync()` 是 final，会自动同步继承的 Input、Output 和 Inner，不应手工逐成员提交。
Input/Output 是 public 基类，Inner 是 protected 基类。顶层接线必须写到模块实例继承的 Input 上：

```cpp
Counter counter;
counter.enable = [] { return 1u; };
```

不要另建一个 `CounterInput input` 再给它接线；那是完全不同的对象，模块自己的 Wire 仍为空。

纯组合模块同样受支持。当前无状态仲裁器采用
`dark::Module<Input, Output>`，在构造函数调用一次项目自定义的 `wire_output()` 完成 Output 接线，
并提供空的 `work()`。`wire_output()` 是本仓库约定的普通成员函数，不是框架自动调用的 API。

### 8.3 本仓库的端口约定

- Input Wire 表示跨模块输入，Output Wire 或只读访问器表示模块对外组合视图，Inner Register
  表示模块私有状态。
- 单生产者信号直接从生产模块 Output 接到消费者 Input；仲裁类多输入组合云使用独立无状态
  Module。
- Wire 最大 32 bit；宽总线应拆成逐字段 Wire 聚合体，而不是塞进一个超宽 Wire。
- 每索引端口使用 `std::array<Wire<N>, CAP>`，避免 C 数组和隐藏的软件容器协议。
- checkpoint 是需要恢复的状态副本，应存入 Register 数组，不应伪装成 Wire。
- IMEM/DMEM 的大块 Memory 是模型外部存储，不应机械地变成逐字节 Register 并参与同步。

## 9. `dark::CPU`：所有权与运行

`dark::CPU` 保存模块调用列表，并可选择拥有模块：

```cpp
dark::CPU cpu;
Counter counter;

cpu.add_module(&counter); // 非 owning
cpu.run_once();
```

裸指针重载不接管所有权，模块必须比 `cpu` 中保存的指针活得更久。也可交出 `unique_ptr`：

```cpp
auto owned = std::make_unique<Counter>();
cpu.add_module(owned);    // 当前 API 会 move，调用后 owned 为空
```

运行 API 的准确语义如下：

- `run_once()`：`cycles` 加一，按注册顺序调用全部 `work()`，然后同步全部模块。
- `run_once_shuffle()`：每拍随机排列 `work()` 调用顺序，然后同步；用于暴露错误的软件顺序依赖。
- `run(max_cycles, shuffle)`：反复选择上述一种单拍函数，直到总 `cycles` 达到 `max_cycles`。
  `max_cycles == 0` 表示不设上限。

通用 `dark::CPU::run()` 没有 finish predicate，也不会识别 HALT。当前模拟器外层的 `::CPU::run(bool)`
自行在每拍开始采样退休状态，调用 `dcpu.run_once()` 或 `run_once_shuffle()` 后检查HALT；这是应用逻辑，
不是框架 `run()` 的隐藏功能。

正确设计中，所有跨模块读都来自 Register old view 或当拍组合 Wire，模块 `work()` 顺序不应影响
结果。`run_once_shuffle()` 是检查这一约束的工具，不是修复顺序依赖的机制。

## 10. RTL 数据通路约束

数据通路代码禁止直接使用宿主 `*`、`/`、`%`。允许的基本运算是加减、移位、按位运算、比较、
位选取和拼接。

- `Bit`/`Register`/`Wire` 参与的 `operator*`、`operator/` 已在框架中删除，误用会编译失败。
- 普通 C++ 整数仍可能让这些运算通过编译，但在 `src/` 数据通路中同样违规。
- 乘 2 的幂用左移，除 2 的幂用右移，2 的幂取模用掩码并配二次幂 `static_assert`。
- 真乘法只能由 MUL 的 Booth + CSA 数据通路产生；真除法和余数只能由 DIV 的 SRT 递推产生。
- host-only 输入解析、结束统计、编译期常量和明确包在断言条件中的参考模型不属于综合数据通路，
  但应保持清晰标注。

不要因为代码是 C++ 就假设编译器会替你保持目标 RTL 结构。SRAM 化的端口、周期边界与
验证约束见根级 [process.md](../../docs/process.md)。

## 11. 常见错误速查

| 现象 | 原因与处理 |
| --- | --- |
| 位宽不匹配编译失败 | 两个框架位类型必须同宽；明确切片、扩展或转换，不要靠隐式猜测 |
| `Register is double assigned` | 同拍存在两个写口；合并分支并在一个位置写一次，包含“同值双写” |
| `Wire is assigned twice` | 把接线放进了 `work()` 或重复调用 `wire_output()`；接线只做一次 |
| `Empty wire is called` | 模块实例自己的 Input/Output 未接；检查是否误接到独立 Input 对象 |
| Wire 下一拍仍是旧值 | Wire 没有位于注册模块中，因而缓存未被 `sync()` 清除 |
| lambda 报复制构造已删除 | `Register`/`Wire` 被按值返回或捕获；改用引用、`wire = reg` 或返回 `+reg` |
| 写寄存器后立即读取还是旧值 | 这是正确的 old/new 语义；新值只在周期末 `sync()` 后可见 |
| `a <= cond ? x : y` 编译异常 | 运算符优先级错误，必须写 `a <= (cond ? x : y)` |
| 随机模块顺序结果变化 | 存在 plain mutable 跨模块状态、未同步缓存或拍内先写后读依赖 |
| 组合求值递归/栈溢出 | Wire 连接形成组合环；用 Register 切断反馈路径 |
| 聚合同步编译失败 | 使用了 C 数组、非聚合成员或超过 64 个直接成员；改为 `std::array`/嵌套聚合 |
| Release 正常、`_DEBUG` 失败 | Release 隐藏了双写、重复接线或越界；应修正模型，不要关闭检查规避 |

另外遵守以下仓库约定：

- `work()` 按 RTL `always_ff` 风格组织，避免提前 `return` 跳过本拍其他状态更新，使用明确的
  `if/else` 或 enable 条件。
- 不复制、移动、`memcpy` 或 `memset` 含 Register/Wire 的模块。
- 循环接线时按值捕获索引，例如 `[this, i]`，不要捕获循环变量引用。
- Wire lambda 内先做 valid/ready 门控，再访问可能越界或会抛错的载荷。
- 裸指针注册模块时保证生命周期；`unique_ptr` 注册后不要继续使用已被 move 的指针。

## 12. VS Code IntelliSense

不要手写一套容易过期的 `includePath` 和宏。让 C/C++ 扩展读取 CMake 生成的
`compile_commands.json`：

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

本目录已有：

- `.vscode/settings.json`：CMake Configure 时自动加入 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`。
- `.vscode/c_cpp_properties.json`：指向 `${workspaceFolder}/build/compile_commands.json`。

在 VS Code 中打开包含本目录 `CMakeLists.txt` 的 `RISC-V-Simulator-Template/`，不要只打开
`src/`。安装 C/C++ 与 CMake Tools，选择和命令行构建相同的 compiler kit，然后执行
`CMake: Configure`。若改用其他 build 目录，同步修改 `compileCommands` 路径。

如果编译数据库由 WSL 编译器生成并包含 `/mnt/...` 路径，应使用 VS Code Remote WSL 打开目录，
并把扩展安装到 WSL 侧。配置更新后仍报红时，执行 `C/C++: Reset IntelliSense Database` 或
`Developer: Reload Window`。
