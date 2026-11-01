# 模板历史黄金表

下表为旧默认配置的黄金数据。不用它校验本提交的四路分配周期，各项指标只适用于表中对应版本。

| case | result | cycles | retired | core | drain |
| --- | ---: | ---: | ---: | ---: | ---: |
| correctness_add_to_100 | 5050 | 641 | 421 | 627 | 14 |
| correctness_array_test1 | 123 | 561 | 131 | 547 | 14 |
| correctness_array_test2 | 43 | 592 | 149 | 578 | 14 |
| correctness_basicopt1 | 88 | 178280 | 142059 | 178266 | 14 |
| correctness_bulgarian | 159 | 364461 | 265654 | 364447 | 14 |
| correctness_expr | 136 | 763 | 291 | 749 | 14 |
| correctness_gcd | 178 | 519 | 82 | 505 | 14 |
| correctness_hanoi | 184 | 6730 | 3674 | 6716 | 14 |
| correctness_lvalue2 | 175 | 308 | 53 | 294 | 14 |
| correctness_magic | 216 | 622715 | 441938 | 622701 | 14 |
| correctness_manyarguments | 40 | 337 | 37 | 323 | 14 |
| correctness_multiarray | 115 | 2663 | 1543 | 2649 | 14 |
| correctness_naive | 97 | 286 | 37 | 272 | 14 |
| correctness_pi | 112 | 7894549 | 3117658 | 7894535 | 14 |
| correctness_qsort | 81 | 1432131 | 1097111 | 1432117 | 14 |
| correctness_queens | 144 | 400694 | 265167 | 400680 | 14 |
| correctness_statement_test | 50 | 1376 | 546 | 1362 | 14 |
| correctness_superloop | 134 | 516685 | 371607 | 516671 | 14 |
| correctness_tak | 186 | 1769880 | 1221227 | 1769866 | 14 |
| perf_median | 0 | 12103 | 6961 | 12089 | 14 |
| perf_multiply | 0 | 27847 | 21722 | 27833 | 14 |
| perf_qsort | 0 | 236748 | 139900 | 236734 | 14 |
| perf_rsort | 0 | 246612 | 195719 | 246598 | 14 |
| perf_towers | 0 | 8249 | 5278 | 8235 | 14 |
| perf_vvadd | 0 | 6257 | 4524 | 6243 | 14 |

25例总 cycles=13731987，retired=7303489。
六 perf 几何 IPC=0.678879614277。
total不含boot1拍；退出Store计入退休，IPC以core为分母。

范围及状态：`../../../sync/MUTUAL-OPT-SYNC-20261027.md`。
生产原始实测及严格校验：`../../../build/mutual-sync-20261027/r02-load-rs/post-report.json`。
