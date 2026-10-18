# 模板框架指南

构建/课程测试见[README](../README.md)，公共架构见[主树](../../README.md)。
长用例使用Release；断言版务必同时启用`_DEBUG`并取消`NDEBUG`，不要覆盖已有仓库`code`。

## 基本类型

| 类型 | 语义 |
|---|---|
| `Register<N>` | 读旧值，`<=`写下一值；每拍最多写一次，sync提交 |
| `Wire<N>` | 只接线一次；本拍懒求值并缓存，sync清缓存 |
| `Bit<N>` | 定宽组合中间值，不是跨周期状态 |
| `Module<In, Out, Inner>` | 输入、输出与内部状态；work求下一状态，sync递归同步成员 |
| `dark::CPU` | 调度模块work，随后统一sync |

位宽上限32 bit；宽SRT状态拆lo/hi。同步数组使用`std::array`，聚合反射上限64个直接成员。
Register零初始化；非零复位状态通过隔离boot建立，不能靠构造赋新值绕过同步边界。

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
