# 模板实现与验证

模板迁移已完成。模块用 `Wire` 接线、用 `Register` 保存状态，按 `work()` → `sync()` 更新。建模约定见[框架指南](help.md)。

## 六项课程用例

模板 `a9b409f` 与主树 `403071c` 对应同一 RTL 配置：四路写回、ROB16/PRF48，每拍重放 8 个 ROB 槽。

两套软件的 Release 与保断言构建均通过六项课程性能用例的结果、退休数和总周期校验。两个外部观察副本各有 374,104 条退休记录，PC、周期和提交通道与 RTL 逐条一致。结果仅覆盖这六项用例，逐项数据和复验方法见[主树性能文档](https://github.com/Bigherta/RV32IM-Simulator/blob/Tournament/docs/ipc_benchmarks.md)。

## 历史记录

- [跨 16 B 四取指](hardware-baseline-fetch4-cross16.md) · [ROB16/PRF48](hardware-baseline-rob16-prf48.md) · [旧行 RS 信用](hardware-baseline-rs-onehot-credit.md)
- [早期模板迁移记录](_archive/20261018/docs/progress.md) · [旧配置黄金表](benchmarks.md)
