# RV32IM 软件模板

基于 C++20 `Wire/Register` 的 RV32IM 周期级乱序模拟器。各模块先执行 `work()`，再统一 `sync()`。另见 [拍初快照版](https://github.com/Bigherta/RV32IM-Simulator/tree/Tournament)。

## 构建与运行

需要 CMake 和支持 C++20 的编译器。在本仓库根目录运行，产物不会覆盖已有的 `code`：

```bash
cmake -S . -B build/quickstart -DCMAKE_BUILD_TYPE=Release \
  -DCPU_RUNTIME_OUTPUT_DIRECTORY="$PWD/build/quickstart/bin"
cmake --build build/quickstart -j4
RESULT_FULL=1 VERBOSE=clock ./build/quickstart/bin/code \
  < data/course-testcases/perf_median/program.data
```

从标准输入读取 Verilog-hex 字节镜像。`RESULT_FULL=1` 输出完整 32 位结果（默认低 8 位），`VERBOSE=clock` 将周期和退休统计输出到标准错误。

## 文档

- [框架指南](docs/help.md) · [验证记录](docs/progress.md)
- [前端](docs/frontend.md) · [后端](docs/backend.md) · [访存](docs/memory.md) · [缓存与 AXI](docs/cache.md)
