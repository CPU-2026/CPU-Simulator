# RV32IM 软件模板

采用`Register/Wire`建模的周期级RV32IM乱序核，所有模块work后统一sync。
2026-10-27已在生产源码采用真实 Tournament BPU（LHT256×1/local3/global2/PC chooser2、预测时上下文）
及 D$8 KiB（128组×2路×32 B）；2026-10-29主树已同步这些生产功能与r01/r02及多路分配。
隔离验收及生产复验均已通过，见[采用记录](../../sync/ADOPT-BPU-DCACHE-20261027.md)。
2026-10-30已同步当前生产RTL：**默认2取指/4译码/4分配/4提交，双ALU、三路写回**。
2026-11-01已同步正式采用第17轮：PRF 63×1×32 SRAM＋上一拍写入旁路、BPU置信度选择、D$同组其他way命中/返回字捕获及脏victim写回期间提前返回。两树Release及断言四构建原六perf答案、退休、课程周期全匹配；411373拍完整PRF状态及374104条退休PC/周期三端一致，见[当前基线](docs/hardware-baseline-prf-microbank.md)。

## 构建与测试

默认即2/4/4/4，双ALU各保留单槽结果，独立CDB0/1；Load/MUL/DIV按年龄共享CDB2。
`CPU_RENAME_WIDTH`现在是`CPU_ISSUE_WIDTH`的兼容别名（1/2/4），冲突配置编译失败。
四宽自动使用ROB32/PRF64/checkpoint64，FQ/IQ默认均为DECODE_WIDTH×2（各8）；AGU/Cache/AXI端口不变。
PRF保留四路分配写和三路CDB写；无写bank持续读，退出冻结时仍work嵌套SRAM并统一sync。所有分配都保存PC+4，包括未ready项；高序号写回优先，P0恒零。
[旧模板表](docs/benchmarks.md)与[旧主树表](../docs/benchmarks.md)保留历史1/1/1/2数据，不用于新默认验收。
四宽历史验收边界见[同步记录](../../sync/TEMPLATE-WIDE4-20261026.md)，不能套用单宽周期或宣称持续4 IPC。

双ALU独立双CDB要求issue4；历史单ALU档须显式设置`CPU_ALU_UNITS=1`与`CPU_ALU_CDB_PORTS=1`。历史配置验收不能扩用于本轮新源码或PPA。

使用支持C++20的编译器，按[共同全新构建命令](../docs/hardware-baseline-prf-microbank.md#证据与复验)
设置F盘输出及进程临时目录。已有两树独立构建时，从工作区根目录验收：

```bash
python3 sync/check_current_production_software.py \
  --main .cpu2026-synthesis/software-production-recheck-new/main/bin/code \
  --template .cpu2026-synthesis/software-production-recheck-new/template/bin/code \
  --out .cpu2026-synthesis/software-production-recheck-new/report-new
```

断言版另建目录，追加`-DCMAKE_CXX_FLAGS=-D_DEBUG -DCMAKE_CXX_FLAGS_RELEASE='-O2 -UNDEBUG'`。
旧`test.sh`/`test_IPC.sh`只读历史黄金表，不能套新默认；使用上述六例入口，不跑额外19例或全回归。
25个课程镜像保持不变。软件旧`timing.total`不含独立boot，课程总周期须显式比较`init+core+drain`。
其他配置不能套用当前六例结果；
旧版2/1/2与2/2/2三端周期对齐只属于[当时配置](../../sync/WIDTH-DECODE-SYNC-20261024.md)。
程序SW写`0x80000000`，对应AXI B握手后退出；`RESULT_FULL=1`输出完整32-bit结果。

## 入口

- [共同配置与周期](../../CPU-SYNC-SPEC.md) · [共享架构](../README.md#设计与验收)
- [框架使用与纪律](docs/help.md) · [进度](docs/progress.md)
- [当前验收](docs/hardware-baseline-prf-microbank.md)：原六perf限定验收；[第10轮记录](docs/hardware-baseline-word-production.md)及旧25例报告保留历史。
- [历史文档](docs/_archive/20261018/INDEX.md)：旧参数和迁移细节，仅作追溯。
