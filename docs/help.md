# 模板框架指南

构建/课程测试见[README](../README.md)，公共架构见[主树](../../README.md)。
长用例使用Release；断言版务必同时启用`_DEBUG`并取消`NDEBUG`，不要覆盖已有仓库`code`。
默认双提交；`-DCPU_COMMIT_WIDTH=1`可构建单提交，但新RS配置须单独验证周期，不能套旧RS单提交golden。参数既可作为C++编译宏，也支持同名CMake cache。
单提交测试同时给`data/course.py test --commit-width 1`，选择`benchmarks.md`中的历史单提交表；
该测试参数不会修改二进制宽度。

## 基本类型

前端配置可直接用CMake cache：`-DCPU_FETCH_WIDTH=1 -DCPU_DECODE_WIDTH=2`，并可设置`CPU_FQ_ENTRIES`/`CPU_IQ_ENTRIES`。默认容量各为译码宽度两倍；仍单发射，旧`CPU_ISSUE_WIDTH`仅在未提供新宽度时兼容原前端设置。可选双取不是RTL等价实现，验收边界见[实施记录](../../../sync/WIDTH-DECODE-SYNC-20261024.md)。

Integer/Branch默认各4项，容量参数为`CPU_INT_RS_ENTRIES`/`CPU_BRANCH_RS_ENTRIES`。只有这两池允许授权WB当拍派发及满站实际离站复用，其他池不能借此读取新的PRF值。

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
