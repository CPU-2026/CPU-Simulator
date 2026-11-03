# RV32IM 软件模板

基于C++20 `Wire/Register` 框架的周期级RV32IM乱序处理器模型，用于与主树模拟器及配套RTL对拍。

## 默认配置

| 项目 | 配置 |
|---|---|
| 取指 / 译码 / 分配 / 提交 | 2 / 4 / 4 / 4；取指可跨8 B，不跨64 B缓存行 |
| FQ / IQ / ROB / PRF | 8 / 8 / 32 / 64 项 |
| 执行与写回 | 双ALU、单AGU、三路CDB；Load/MUL/DIV共享一路CDB |
| 分支预测与恢复 | Tournament、BTB32、RAS8、checkpoint64；每拍重放8个ROB槽 |
| I$ / D$ | 1 KiB直接映射、64 B行 / 8 KiB两路、32 B行 |
| LQ / SQ / D$请求槽 / MSHR | 8 / 8 / 8 / 4 项 |

## 建模约定

- `Wire` 在构造时一次接线，按周期求值并清理缓存。
- `Register` 读取旧值，每周期只允许写入一次。
- 所有模块完成 `work()` 后统一 `sync()`，不得通过调用顺序产生隐含旁路。

## 构建与运行

需要CMake及支持C++20的编译器。以下命令在包含两份模拟器的 **CPU-2026工作区根目录**执行，不覆盖仓库已有 `code`：

```bash
export TMPDIR="$PWD/.cpu2026-synthesis/tmp"
mkdir -p "$TMPDIR"
export TMP="$TMPDIR" TEMP="$TMPDIR"
OUT="$PWD/.cpu2026-synthesis/software-readme"
cmake -S RV32IM_Simulator/RISC-V-Simulator-Template -B "$OUT/template" -DCMAKE_BUILD_TYPE=Release -DCPU_RUNTIME_OUTPUT_DIRECTORY="$OUT/template/bin"
cmake --build "$OUT/template" -j4
RESULT_FULL=1 VERBOSE=clock "$OUT/template/bin/code" < RV32IM_Simulator/data/course-testcases/perf_median/program.data
```

输入为Verilog-hex字节镜像。程序向 `0x80000000` 全字写入结果，在对应AXI B握手后退出。`RESULT_FULL=1` 输出完整32-bit结果，统计信息写入stderr。

## 验证与文档

原六perf验证仅适用于对应版本与配置，不是全输入形式证明；课程总周期按 `init+core+drain` 比较，不套用历史单宽黄金周期。两树对拍依赖工作区的 `sync/check_current_production_software.py` 及匹配的基线证据，不随模板仓库单独克隆提供。

断言版使用独立构建目录，追加 `-DCMAKE_CXX_FLAGS=-D_DEBUG -DCMAKE_CXX_FLAGS_RELEASE='-O2 -UNDEBUG'`，保留Register单写检查。软件统计不能替代硬件面积／Fmax测量。

- [框架使用说明](docs/help.md)
- [前端](docs/frontend.md) · [后端与乘除法](docs/backend.md)
- [访存](docs/memory.md) · [缓存](docs/cache.md)
