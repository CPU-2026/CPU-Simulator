# 模板框架指南

## 构建配置

基本命令见 [README](../README.md)。保留断言时用独立构建及输出目录，配置为 Release，并追加 `-DCMAKE_CXX_FLAGS=-D_DEBUG` 与 `-DCMAKE_CXX_FLAGS_RELEASE='-O2 -UNDEBUG'`。`_DEBUG` 检查框架单写和接线，`-UNDEBUG` 保留标准 `assert`。

取指、译码、分配和提交宽度由 `CPU_FETCH_WIDTH`、`CPU_DECODE_WIDTH`、`CPU_ISSUE_WIDTH`、`CPU_COMMIT_WIDTH` 设置，默认均为 4。`CPU_RENAME_WIDTH` 是 `CPU_ISSUE_WIDTH` 的别名，两者不能冲突。改用单路或双路分配时还须把 `CPU_ALU_UNITS` 和 `CPU_ALU_CDB_PORTS` 均设为 1，并满足队列和检查点约束。

## 每拍如何更新

| 类型 | 用法 |
|---|---|
| `Bit<N>` | 定宽组合值，不保存跨拍状态 |
| `Register<N>` | 读旧值，`<=` 写下一值；每拍最多写一次，由 `sync()` 提交 |
| `Wire<N>` | 只接线一次；本拍首次读取时求值，`sync()` 清缓存 |
| `Module<In, Out, Inner>` | 承载端口和状态；`work()` 写下一值，`sync()` 同步成员 |
| `dark::CPU` | 所有模块先 `work()`，再统一 `sync()` |

- 在模块实例上接 Input，输出 Wire 在构造时接好；无状态仲裁器用 Wire，不新增 Register。
- 组合逻辑只读旧状态。多个写源先算最终值再写一次；不能复制模块下一状态，也不能靠 `work()` 顺序做旁路。
- Register 初值为零，非零初值由初始化拍写入。同步数组用 `std::array`，聚合反射最多 64 个直接成员。
- 恢复期间被暂停的模块仍要 `sync()` 清 Wire 缓存；Cache 和 AXI 继续推进。嵌套 SRAM 由父模块驱动，并随父模块同步。
- 数据通路不直接写乘、除、取模；乘除法实现见[后端](backend.md#执行单元)。

## 调试与验证

`VERBOSE=clock,branch,icache,dcache` 将统计写到标准错误。做周期对拍时，按 `init + core + drain` 比较，不套用[历史黄金表](benchmarks.md)。范围和结果见[验证记录](progress.md)。

[完整 API 示例（历史指南）](_archive/20261018/docs/help.md)
