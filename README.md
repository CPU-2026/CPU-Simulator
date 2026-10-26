# RV32IM 软件模板

与[主树](../README.md)实现同一周期级RV32IM乱序核；区别是用`Register/Wire`建模，所有模块work后统一sync。
架构说明统一维护在主树，避免两份文档漂移。

## 构建与测试

四译码／四路顺序发射：追加`-DCPU_FETCH_WIDTH=2 -DCPU_DECODE_WIDTH=4 -DCPU_ISSUE_WIDTH=4 -DCPU_COMMIT_WIDTH=2`。
`CPU_RENAME_WIDTH`现在是`CPU_ISSUE_WIDTH`的兼容别名（1/2/4），冲突配置编译失败。
四宽自动使用ROB32/PRF64/checkpoint64、默认FQ8/IQ8；不增加取指、执行、提交或访存端口。
默认仍1/1/1/2，旧golden保持；四宽验收边界见[同步记录](../../sync/TEMPLATE-WIDE4-20261026.md)，不能套用历史双宽周期或宣称持续4 IPC。

可选双发射：追加`-DCPU_FETCH_WIDTH=2 -DCPU_DECODE_WIDTH=2 -DCPU_RENAME_WIDTH=2 -DCPU_COMMIT_WIDTH=2`，默认rename仍1。双Load/双Store、RAW/WAW分配逻辑已移植RTL；对应单/双rename各25课程＋40定向独立ISA通过，课程逐例core/total/退休数与模板相等。主树仍单发射，默认golden不改，未新增第二ALU；见[RTL移植记录](../../sync/RTL-DUAL-RENAME-20261025.md)。

在本目录执行，使用支持C++20的编译器：

```bash
cmake -S . -B /tmp/opencode/rv32-template -DCMAKE_BUILD_TYPE=Release -DCPU_RUNTIME_OUTPUT_DIRECTORY=/tmp/opencode/rv32-template
cmake --build /tmp/opencode/rv32-template -j2
BP_BIN=/tmp/opencode/rv32-template/code ./test.sh
BP_BIN=/tmp/opencode/rv32-template/code ./test_IPC.sh
python3 ../data/course.py smoke --binary /tmp/opencode/rv32-template/code
```

断言版另建目录，追加`-DCMAKE_CXX_FLAGS=-D_DEBUG -DCMAKE_CXX_FLAGS_RELEASE='-O3 -UNDEBUG'`。
golden只读[主树首张课程表](../docs/benchmarks.md)；25个课程镜像在`data/course-testcases/`。
默认仍单取/单译码/双提交。可选`-DCPU_FETCH_WIDTH=2 -DCPU_DECODE_WIDTH=1/2`；2/1/2与2/2/2各25例已与主树及对应RTL结果/退休/core/total精确一致，但不能套用默认单取指golden。配置和边界见[宽度同步记录](../../sync/WIDTH-DECODE-SYNC-20261024.md)。
程序SW写`0x80000000`，对应AXI B握手后退出；`RESULT_FULL=1`输出完整32-bit结果。

## 入口

- [共同配置与周期](../../CPU-SYNC-SPEC.md) · [共享架构](../README.md#设计与验收)
- [框架使用与纪律](docs/help.md) · [进度](docs/progress.md)
- [当前验收](../../sync/RTL-TO-SW-SYNC-20261018.md)：四软件构建及两软件分别对RTL的25例均通过。
- [历史文档](docs/_archive/20261018/INDEX.md)：旧参数和迁移细节，仅作追溯。
