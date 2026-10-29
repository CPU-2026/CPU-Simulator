# RV32IM 软件模板

采用`Register/Wire`建模的周期级RV32IM乱序核，所有模块work后统一sync。
2026-10-27已在生产源码采用真实 Tournament BPU（LHT256×1/local3/global2/PC chooser2、预测时上下文）
及 D$8 KiB（128组×2路×32 B）；2026-10-29主树已同步这些生产功能与r01/r02及多路分配。
隔离验收及生产复验均已通过，见[采用记录](../../sync/ADOPT-BPU-DCACHE-20261027.md)。
没有顺带移植独立最佳候选的双ALU/四提交或 critical-word 早返回，既有宽度选项保持。

## 构建与测试

四译码／四路顺序发射：追加`-DCPU_FETCH_WIDTH=2 -DCPU_DECODE_WIDTH=4 -DCPU_ISSUE_WIDTH=4 -DCPU_COMMIT_WIDTH=2`。
`CPU_RENAME_WIDTH`现在是`CPU_ISSUE_WIDTH`的兼容别名（1/2/4），冲突配置编译失败。
四宽自动使用ROB32/PRF64/checkpoint64，FQ默认FETCH_WIDTH×2、IQ默认DECODE_WIDTH×2（上述2/4为4/8）；不增加执行或访存端口。
默认仍1/1/1/2；[模板实测表](docs/benchmarks.md)与[主树首表](../docs/benchmarks.md)已核对一致，旧主树表保留历史。
四宽历史验收边界见[同步记录](../../sync/TEMPLATE-WIDE4-20261026.md)，不能套用单宽周期或宣称持续4 IPC。

可选双发射：追加`-DCPU_FETCH_WIDTH=2 -DCPU_DECODE_WIDTH=2 -DCPU_RENAME_WIDTH=2 -DCPU_COMMIT_WIDTH=2`，默认rename仍1。主树现也支持；当前软件对照见[同步记录](../docs/tree-sync-20261029.md)。历史RTL验收仍见[RTL移植记录](../../sync/RTL-DUAL-RENAME-20261025.md)，不扩作本轮新RTL或PPA结论。

在本目录执行，使用支持C++20的编译器：

```bash
cmake -S . -B /tmp/opencode/rv32-template -DCMAKE_BUILD_TYPE=Release -DCPU_RUNTIME_OUTPUT_DIRECTORY=/tmp/opencode/rv32-template
cmake --build /tmp/opencode/rv32-template -j2
BP_BIN=/tmp/opencode/rv32-template/code ./test.sh
BP_BIN=/tmp/opencode/rv32-template/code ./test_IPC.sh
python3 ../data/course.py smoke --binary /tmp/opencode/rv32-template/code
```

断言版另建目录，追加`-DCMAKE_CXX_FLAGS=-D_DEBUG -DCMAKE_CXX_FLAGS_RELEASE='-O3 -UNDEBUG'`。
课程模式的`test.sh`/`test_IPC.sh`默认只读[模板周期表](docs/benchmarks.md)，可用`GOLDEN_MD`覆盖；
直接调用`../data/course.py test`时必须显式加`--golden docs/benchmarks.md`。25个课程镜像保持不变。
默认仍单取/单译码/双提交。双取指或其它译码/发射宽度不能套用本表；
旧版2/1/2与2/2/2三端周期对齐只属于[当时配置](../../sync/WIDTH-DECODE-SYNC-20261024.md)。
程序SW写`0x80000000`，对应AXI B握手后退出；`RESULT_FULL=1`输出完整32-bit结果。

## 入口

- [共同配置与周期](../../CPU-SYNC-SPEC.md) · [共享架构](../README.md#设计与验收)
- [框架使用与纪律](docs/help.md) · [进度](docs/progress.md)
- [当前验收](../../sync/RTL-TO-SW-SYNC-20261018.md)：四软件构建及两软件分别对RTL的25例均通过。
- [历史文档](docs/_archive/20261018/INDEX.md)：旧参数和迁移细节，仅作追溯。
