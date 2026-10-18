# 模板实现进度

## 2026-10-18：当前RTL优化等价移植

- AXIFetch16→8并派生指针宽度；AXIData描述符/payload16→8，保留16项顺序环、Slave16项不改。
- ROB共享四槽窄窗、恢复消费者不再各自索引全ROB；RAS保留软件原有已解码hint表示。
- D$字选/字节旋转和单读grant共享；LQ全来源raw、读出扩展；SQ字节重叠含非对齐/回绕保护。
- 无新增流水级或Wire缓存所有权改变，Register单写/统一sync保持；四构建25例严格golden及
  8定向通过，golden不改。当前RTL逐拍/独立参考的最终范围见父工作区`sync/RTL-TO-SW-SYNC-20261018.md`。

## 2026-10-17：四拍恢复与AXI/Cache结构同步

- CPU持有固定四拍游标，模块拥有恢复Input Wires，不新增CPU裸Wire缓存。
- RAT/RAS已提交基线使用掩码宽字同步SRAM和valid，R+1首拍直接使用读输出；完整栈/映射重放含head/边界。
- PRF逐拍回收原取消窗口，保持完整tag与FIFO顺序；广播和四拍都禁止退休/分配。
- 恢复期选择性work，所有模块仍sync；Cache/AXI/LSQ继续，Load保存等待CDB、已提交Store前进。
- AXIData共享16槽payload改为32-bit word银行；DCache宽payload独立、候选/授权解耦、局部SRAM地址。
- 当前验收及最终golden更新状态以父工作区`sync/RECOVERY4-SYNC-20261017.md`为准，不沿用旧单拍恢复周期。

本文件为当前轮次新增记录，不虚构此前未保留的迁移历史正文。
