# RV32IM 乱序处理器模拟器

> **当前默认（2026-10-14，按RTL同步）**：D$为4 KiB（128组×1路×32 B），8个
> 128×32bit data SRAM＋128×21bit tag/dirty SRAM＋独立valid。缺失Load首个合格
> 回填拍从完整256bit回复返回一次；整行4拍填完才释放Cache队首与消费回复。
> I$保持1 KiB/64 B，Register单写/统一sync、boot1拍和立即HALT保持；最新验收见sync/README.md。
> 当前源码三CPU24镜像逐拍通过，活动golden已刷新；两软件四构建24镜像strict复核通过。

> **历史2026-10-13 D$同步完成**：4项请求队列、队首串行查询，data为128×512bit字节写
> SRAM，metadata为128×21bit SRAM；DMEM回复保持至回填消费。HALT提交立即结束，
> 不排空。Release/`_DEBUG`各18 ISA＋6 IPC与主树结果/周期/退休数/保留统计一致，
> 严格匹配共同golden；总clock分别12,987,988/759,991。RTL尚未同步此次时序。

这是一个 C++20 编写的周期级 RV32IM 处理器模型。核心采用 Tomasulo 风格设计：
按序取指和发射、乱序执行、按序提交。

项目同时提供一套简单的硬件建模框架，用 `Register` 表示时序状态，用 `Wire`
表示组合连线。它用于在编写 RTL 前验证指令行为和流水线时序，不是可直接综合的 C++。

## 构建

需要支持 C++20 的编译器，推荐 GCC 12 及以上版本。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

可执行文件默认生成在当前目录：

```bash
./code < data/testcases/gcd.data
# 178
```

模拟器从标准输入读取 Verilog hexadecimal 字节镜像。标准输出是程序结束时
`x10` 的低 8 位，调试信息写入标准错误。

```bash
VERBOSE=clock,icache ./code < data/testcases/gcd.data
```

`0x0ff00513` 被用作 HALT 标记。HALT 提交后立即结束，不排空 SQ、DCache 和 DMEM。
内存大小为 128 KiB；停机时可能仍有已提交但未写入缓存的 Store。

## 核心结构

```text
 Tournament / BTB / RAS
         |
 Fetch -> ICache -> FQ(2) -> Decode / IQ(2)
   |                                  |
 IMEM                           Issue + Rename
                               RAT / PRF / ROB
                                      |
                         Reservation Stations
                         |                 |
                 ALU / MUL / DIV / BRU    AGU -> LQ / SQ -> DCache / DMEM

             ALU CDB | Load CDB | MUL CDB | DIV CDB
                              |
                         PRF / ROB
```

| 项目 | 配置 |
| --- | --- |
| ISA | RV32I 整数核心指令 + RV32M |
| 取指 / 发射 / 提交宽度 | 1 / 1 / 1 |
| ROB / PRF / RAT | 16 / 48 / 32 |
| FQ / IQ | 2 / 2 |
| Integer RS | 2 |
| MUL / DIV / Load / Branch RS | 1 / 1 / 2 / 2 |
| Store Address / Store Value RS | 各 2 |
| LQ / SQ | 8 / 8 |
| 结果总线 | ALU、Load、MUL、DIV 四路独立 CDB |
| ICache | 1 KiB，16×64 B，单端口同步 SRAM；4 项请求队列，队首命中最早 n+1 拍旁路交付 FQ |
| DCache | 4 KiB，128×32 B，data/metadata 同步 SRAM，4 项请求队列，首拍Load早返回/4拍回填，write-back + write-allocate |
| 主存延迟 | 20 周期 |

分支预测器的方向侧由 local/global/selector 各 256×2-bit 的 Tournament 表和 8-bit GHR
组成；目标侧保留 BTB64、specRAS/archRAS 各 16（按提交基线与 ROB 重放恢复）。分支误预测与 JALR 目标错误统一
交给 `FlushArbiter` 恢复；load 采用保守准入，更老未提交 store 地址未知或同址时停发
cache 请求，不再依赖 store-load 顺序违例恢复。

MUL 使用 radix-4 Booth 部分积和 CSA 压缩树；DIV 使用 SRT radix-4 递推。
数据通路不使用宿主 `*`、`/` 或 `%` 计算乘除法结果。

未实现 `fence`、CSR、`ecall/ebreak`、特权级、异常、中断、原子、压缩、浮点和
向量扩展。

## 周期模型

每个周期先调用所有模块的 `work()`，再统一调用 `sync()`：

```text
Module::work() -> sync all Register/Wire -> next cycle
```

| 类型 | 用途 |
| --- | --- |
| `Register<N>` | 读取当前值，`<=` 写入下一周期值 |
| `Wire<N>` | 组合连线，每周期求值一次 |
| `Bit<N>` | 定宽组合中间值 |
| `Module<In, Out, Inner>` | 模块端口和内部状态 |
| `dark::CPU` | 模块调度和统一同步 |

基本约束：

- 跨周期状态使用 `Register`，跨模块通信使用 `Wire`。
- `Wire` 只接线一次，一个 `Register` 每周期最多写一次。
- 同步数组使用 `std::array`。
- `work()` 中用明确的 `if/else` 表示写优先级。
- `Register`、`Wire` 和 `Bit` 的最大位宽为 32 bit。

定义 `_DEBUG` 后，框架会检查 Register 双写、Wire 重复接线和未接线读取。
仓库的构建、运行、测试、框架 API 与常见错误统一见
[`docs/help.md`](docs/help.md)。

## 目录

```text
include/             Register/Wire/Bit/Module 框架
src/CPU/             顶层接线与运行循环
src/include/         CPU 模块声明和公共常量
src/                 各流水线模块实现
data/testcases/      RV32IM 测试源、镜像与反汇编
data/testcases_ipc/  RV32IM IPC 工作负载
docs/                使用、架构、迁移回顾和 fmax 文档
```

主要设计文档：

- [`docs/frontend.md`](docs/frontend.md)：取指、译码和分支预测。
- [`docs/backend.md`](docs/backend.md)：发射、执行、写回、提交、MUL/DIV。
- [`docs/memory.md`](docs/memory.md)：LQ/SQ、转发和访存顺序。
- [`docs/cache.md`](docs/cache.md)：ICache、DCache、IMEM、DMEM。
- [`docs/progress.md`](docs/progress.md)：模板迁移回顾、验证政策和关键经验。
- [`docs/fmax-critical-path-analysis.md`](docs/fmax-critical-path-analysis.md)：结构性关键路径分析。
- [`../docs/benchmarks.md`](../docs/benchmarks.md)：测试结果和性能数据的根级 SSOT。
- [`../docs/process.md`](../docs/process.md)：大数组 SRAM 化阶段、端口契约与验证结果。

当前参数以 `include/common.h` 和 `src/include/` 中的定义为准。

## 测试

RV32IM 行为与周期回归：

```bash
./test.sh gcd
./test.sh
```

IPC 工作负载与报告更新：

```bash
./test_IPC.sh
```

`test.sh` 同时严格检查 x10 和 `../docs/benchmarks.md` 中的 cycles；缺失镜像、缺失
golden 或零用例都会失败。历史扩展收益数据只保留在根级 benchmark 文档中，不再提供
对应的可运行资产。`test_IPC.sh` 要求每个 IPC 镜像的自校验结果 `x10 == 0`，任一失败
都不会覆盖根级 `../docs/ipc_benchmarks.md`。

**历史容量基线（2026-10-01）**：两树当时使用 1 KB ICache 与 **16 KiB、64 B 行、直接映射 DCache**；
对应18/18总clock为12,209,929，独立六例几何IPC为0.720539。该组数字不是当前容量结果。
本树保留 `refillValid/refillSlot`，接线与验收见
[`docs/cache.md`](docs/cache.md)、[`docs/progress.md`](docs/progress.md) 和
[`../docs/process.md`](../docs/process.md)。

`reorder_test` 已随旧测试脚手架退役。框架仍提供 `run_once_shuffle()`，但它不再是仓库的日常回归入口。

## 调试

`VERBOSE` 支持以下主题：

```text
clock,branch,icache,dcache
```

多个主题用逗号分隔，`VERBOSE=all` 启用全部活动指标汇总。逐指令事件日志与额外画像已清理。

## 限制

- 不支持 ELF 直接加载。
- IMEM 和 DMEM 是独立副本，不支持自修改代码。
- `0x0ff00513` 不能作为普通 `addi` 使用。
- 顶层没有最大周期限制，缺少 HALT 的程序不会自动结束。
- 模型使用 lambda、虚函数和动态对象，不能直接综合。
## 当前紧凑容量配置（2026-10-11）

主树、模板树及 RTL 同步采用 ROB16 / PRF48 / FQ2 / IQ2 / LQ8 / SQ8，七池 RS
（Integer/Multiply/Divide/Branch/Load/StoreAddr/StoreValue）=2/1/1/2/2/2/2，共12槽，
RAS16；D$为8 KiB/64 B直映。Release与`_DEBUG`两树18 ISA＋6 IPC严格回归通过，六项IPC
几何平均 **0.612677678542**。活动逐例结果与周期见根级 [`../docs/benchmarks.md`](../docs/benchmarks.md)
和 [`../docs/ipc_benchmarks.md`](../docs/ipc_benchmarks.md)。对应 RTL 当前容量综合/STA 汇总见
[`../../RISC-V-CPU-2026/README.md`](../../RISC-V-CPU-2026/README.md)。
