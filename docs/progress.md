# 模板实现与验证

模块用 `Wire` 接线、用 `Register` 保存状态，先 `work()`，再统一 `sync()`。更新约定见[框架指南](help.md)。

本提交的默认取指 / 译码 / 分配 / 提交宽度为 4 / 4 / 4 / 4。验证数据仅适用于各记录注明的版本。

[ROB16/PRF48](hardware-baseline-rob16-prf48.md) · [跨 16 B 四取指](hardware-baseline-fetch4-cross16.md) · [IQ 整组旁路](hardware-baseline-iq-whole-group.md) · [PRF 单行 SRAM](hardware-baseline-prf-microbank.md) · [用例基准](benchmarks.md) · [迁移归档](_archive/20261018/docs/progress.md)
