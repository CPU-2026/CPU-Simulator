# 模板框架指南

**2026-10-29**：主树已同步模板生产功能与独立1/2/4宽度；默认两树25例严格周期一致，主树首表与本模板表一致。
FQ默认取指宽度两倍、IQ默认译码宽度两倍；2/4/4/2为FQ4/IQ8，下文旧FQ8说明为历史。
当前同步/验证范围见[记录](../../docs/tree-sync-20261029.md)；历史独立采用证书不重标为本轮验收。

2026-10-27双向同步r02已写入三个模板功能文件，生产路径复验通过；状态与二进制位置以
[双向记录](../../../sync/MUTUAL-OPT-SYNC-20261027.md)为准。新默认表已按生产普通入口实测确认，
不混用r01旧code/旧表；仓库已有code及参考主树表不覆盖。

2026-10-27双向同步r01已写入模板DCache两文件，生产路径复验通过；当前状态及二进制位置以
[双向记录](../../../sync/MUTUAL-OPT-SYNC-20261027.md)为准。已按普通入口实测更新本模板表，
不修改主树表，不覆盖仓库已有`code`，不把旧二进制与新周期表混用。

2026-10-27：生产源码已采用真实 Tournament BPU＋D$8 KiB，默认宽度仍1/1/1/2。
参考主树未采用，本模板课程模式使用自己的[`benchmarks.md`](benchmarks.md)，不再默认读取主树周期表。
`BP_BIN=/path/to/new/code ./test.sh`与`./test_IPC.sh`会自动选此表；直接运行公共脚本时使用
`python3 ../data/course.py test --binary /path/to/new/code --data data/course-testcases --golden docs/benchmarks.md`。
仓库已有code未覆盖，必须重新构建后通过BP_BIN指定新二进制；生产复验状态见[采用记录](../../../sync/ADOPT-BPU-DCACHE-20261027.md)。

构建/课程测试见[README](../README.md)，公共架构见[主树](../../README.md)。
长用例使用Release；断言版务必同时启用`_DEBUG`并取消`NDEBUG`，不要覆盖已有仓库`code`。
默认双提交；`-DCPU_COMMIT_WIDTH=1`可构建单提交，但新RS配置须单独验证周期，不能套旧RS单提交golden。参数既可作为C++编译宏，也支持同名CMake cache。
本轮新表仅覆盖默认双提交。单提交尚无本轮实测周期表，不能套用历史单提交表或仅改测试参数冒充当前配置验收；
测试参数不会修改二进制的编译宽度。

## 基本类型

2026-10-26起`CPU_ISSUE_WIDTH`是后端顺序分配宽度，允许1/2/4；`CPU_RENAME_WIDTH`保留为完全同义别名，
同时指定但不相等会编译失败。此规则覆盖下方历史“旧CPU_ISSUE_WIDTH仅前端”的说明。
四宽独立构建示例：

```bash
cmake -S . -B /tmp/opencode/template-four -DCMAKE_BUILD_TYPE=Release \
  -DCPU_FETCH_WIDTH=2 -DCPU_DECODE_WIDTH=4 -DCPU_ISSUE_WIDTH=4 -DCPU_COMMIT_WIDTH=2 \
  -DCPU_RUNTIME_OUTPUT_DIRECTORY=/tmp/opencode/template-four \
  -DCMAKE_CXX_FLAGS=-D_DEBUG -DCMAKE_CXX_FLAGS_RELEASE='-O3 -UNDEBUG'
cmake --build /tmp/opencode/template-four -j2
```

四宽默认ROB32/PRF64/checkpoint64；FQ/IQ按fetch/decode分别派生，默认单发射仍ROB16。
decode4/issue1或2须显式缩小FQ/IQ（例如各4项）以满足checkpoint32预算，不能截断CID或放宽预算断言。
四宽不能套单发射golden周期，结果和独立退休数必须校验；可复用`sync/check_template_wide4.py`和
`sync/check_template_wide4_cpu.py`，报告均输出到仓库外，真实观察器只打开外部副本CPU访问标签。

后端双重命名使用独立配置`-DCPU_RENAME_WIDTH=2`（默认1），不是旧前端`CPU_ISSUE_WIDTH`。例如独立构建：

```bash
cmake -S . -B /tmp/opencode/rename-build -DCMAKE_BUILD_TYPE=Release \
  -DCPU_FETCH_WIDTH=2 -DCPU_DECODE_WIDTH=2 -DCPU_RENAME_WIDTH=2 \
  -DCPU_RUNTIME_OUTPUT_DIRECTORY=/tmp/opencode/rename-build/bin \
  -DCMAKE_CXX_FLAGS=-D_DEBUG -DCMAKE_CXX_FLAGS_RELEASE='-O3 -UNDEBUG'
cmake --build /tmp/opencode/rename-build -j4
```

双宽支持双Load/双Store入队，但不增加AGU/Cache/CDB带宽。周期会变化，不能拿单发射golden验收双发射周期；默认单发射必须保持改前结果/退休/分段周期。记录见[双重命名接线](../../../sync/DUAL-RENAME-WIRING-20261025.md)。

该逻辑随后已移植RTL：固定fetch/decode/commit=2时，单/双rename各25课程＋40定向独立ISA通过，课程core/total/退休数与模板相等；双rename不是主树支持项。RTL运行方式和既有失败边界见[移植记录](../../../sync/RTL-DUAL-RENAME-20261025.md)。

前端配置可直接用CMake cache：`-DCPU_FETCH_WIDTH=2 -DCPU_DECODE_WIDTH=2 -DCPU_COMMIT_WIDTH=2`，并可设置`CPU_FQ_ENTRIES`/`CPU_IQ_ENTRIES`。默认容量各为译码宽度两倍；仍单发射，旧`CPU_ISSUE_WIDTH`仅在未提供新宽度时兼容原前端设置。双取已同步RTL packet/逐指令CID/恢复期安全hit提前取指，2/1/2与2/2/2均25例三端周期相等；默认保持1/1/2，其他容量组合不能套用该结论，见[实施记录](../../../sync/WIDTH-DECODE-SYNC-20261024.md)。

Integer/Branch默认各4项；当前Integer/Branch/Load允许授权WB当拍派发及实际离站复用，StoreAddr/StoreValue/MUL/DIV不能借此读取新PRF值。

当前默认单取指档已与RTL 1/1/2完成25例结果/退休/core/total对齐，六perf IPC0.641865180802；旧RS/Load A/ABC整表保留。I$请求/AXIFetch/BTB/RAS容量参数分别是`CPU_FETCH_REQUEST_ENTRIES`/`CPU_AXI_FETCH_ENTRIES`/`CPU_BTB_ENTRIES`/`CPU_RAS_ENTRIES`，默认4/4/32/8。其他宽度须分别验收，不能套用本档golden；Slave服务和退出口径不变。

| 类型 | 语义 |
|---|---|
| `Register<N>` | 读旧值，`<=`写下一值；每拍最多写一次，sync提交 |
| `Wire<N>` | 只接线一次；本拍懒求值并缓存，sync清缓存 |
| `Bit<N>` | 定宽组合中间值，不是跨周期状态 |
| `Module<In, Out, Inner>` | 输入、输出与内部状态；work求下一状态，sync递归同步成员 |
| `dark::CPU` | 调度模块work，随后统一sync |

位宽上限32 bit；宽SRT状态拆lo/hi。同步数组使用`std::array`，聚合反射上限64个直接成员。
Register零初始化；非零复位状态通过隔离boot建立，不能靠构造赋新值绕过同步边界。
批量退休也不能对同一Register重复赋值：RAT/RAS先合并掩码和年轻写优先级，
PRF批量回收用局部FIFO游标，LQ/ROB分别计算最终head后写一次。

## 接线与更新纪律

```text
读取旧Register/Input Wire → 组合谓词与写意图 → work单点写Register
所有模块work结束 → sync所有Register/Wire → 下一拍
```

- 模块继承自己的Input，接线写在模块实例上；不要另建同名Input对象。
- 输出Wire在构造/`wire_output()`中接一次；不要在work里每拍重新接线。
- 多源更新先算最终写意图，再对每个Register写一次；相同值双写也违规。
- 禁止模块整对象复制、memcpy Register、读取其他模块本拍next-state作旁路。
- 无状态仲裁器使用Wire和空work，不添加寄存级。CPU裸Wire不受模块sync管理，不用于组合缓存。
- work用明确if/else表达优先级；helper读旧态，不用早退出隐藏写冲突。
- `Bit/Register`禁裸乘除；MUL用Booth/CSA，DIV用SRT，不能用宿主结果代替数据通路。

`_DEBUG`检查Register双写、Wire重复接线和未接线读取。需要测试观察时使用仓库外副本，不恢复生产Probe/Tester。

## 恢复与存储模块

四拍恢复只暂停前端/RS/FU/ROB/FlushArbiter的work；RAT/PRF/RAS继续重放，Cache/AXI/LSQ继续推进，
**全部模块仍sync**。不能跳过整个`run_once()`或只清运输模块的Wire。

ROB拥有共享四槽恢复窗的输出Wire；CPU只接旧恢复游标。D$组合缓存只存端口/事件，不能充当模块next-state。
嵌套SRAM由父模块每拍work一次，父sync递归提交输出并清Wire。
LQ内部value为raw，只有`getValue()`/CDB读出才扩展。

## 调试与参考

`VERBOSE=clock,branch,icache,dcache`输出到stderr。`reorder_test`已退役，不作为日常入口。
更改时序前先核对[共同规范](../../../CPU-SYNC-SPEC.md)，再跑[验证入口](../../../sync/README.md)。

完整API示例、历史迁移陷阱见[原指南](_archive/20261018/docs/help.md)；其中旧容量、HALT和过时命令不作为当前规则。
