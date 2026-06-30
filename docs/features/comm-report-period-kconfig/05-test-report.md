<!--
  测试报告（敏捷-V 右腿验证）：comm 模块 CAN 上报周期 Kconfig 可配。
  阶段：⑤ 测试（bms-tester）。输入：01-requirements.md / 03-design.md / 已实现代码。
  内容：用例清单 + 通过情况 + 覆盖率 + 每用例回溯需求/设计。交付物语言：中文。
-->
# comm 模块 CAN 上报周期 测试报告

| 字段 | 值 |
|---|---|
| 特性 slug | comm-report-period-kconfig |
| 套件 | `tests/bms/comm/`（套件名 `bms_comm`） |
| 被测产物 | `app/src/bms/comm/comm_period.c`（纯函数 `bms_comm_clamp_period_ms`） |
| 关联需求 | REQ-COMM-001 … REQ-COMM-007 |
| 关联设计 | DES-COMM-001 … DES-COMM-006（重点 DES-COMM-002/006） |
| 板 / 运行环境 | `mps2/an386`（Cortex-M4F + QEMU），命令 `..\run-tests-coverage.ps1 -Board mps2/an386` |
| 结果 | 全部通过（6/6 comm 用例；全仓 57/57 用例通过） |

---

## 1. 测试设计依据

- 设计 §7.2 给出套件 `bms_comm` 用例清单；本报告据此实现并补强「越上界 / 不变式」两类。
- 被测为纯函数 `bms_comm_clamp_period_ms(requested, lo, hi)`，无副作用、无全局/硬件/Kconfig 依赖、边界以入参注入，host 可直接断言（对齐 afe `bms_afe_validate` 范式）。
- 套件 `CMakeLists.txt` **只链 `comm_period.c`**，不链 `comm.c`（避免把 `K_THREAD_DEFINE`/桩 TX/`LOG_MODULE_REGISTER` 拖进 host 单测，符合 DES-COMM-006 / ADR-COMM-08）。
- 测试侧本地常量 `TEST_P_MIN=10`、`TEST_P_MAX=60000` 镜像 `app/Kconfig` 的 `range 10 60000`（= `comm.c` 内 `BMS_COMM_PERIOD_MIN/MAX_MS`），供边界与 range 自洽断言。

## 2. 用例清单与通过情况

| 用例（`ZTEST(bms_comm, …)`） | 验证场景（正常 / 边界 / 失效安全） | 关键断言 | 回链需求 | 结果 |
|---|---|---|---|---|
| `test_default_period_is_200` | 正常 | `clamp(200)==200`；`clamp(100)==100`、`clamp(1000)==1000` 且二者不相等（配置可观察改变周期） | REQ-COMM-001 / 002 | PASS |
| `test_period_range_bounds` | 边界 | 端点 `clamp(10)==10`、`clamp(60000)==60000`；`P_min>0`、`P_max>=200`、`P_min<=200<=P_max` | REQ-COMM-004 | PASS |
| `test_clamp_below_lower_bound` | 失效安全（恰好越限） | `clamp(1)==10`、`clamp(9)==10`；同输入两次结果一致且唯一（确定性，>0） | REQ-COMM-005 / 007 | PASS |
| `test_clamp_zero_or_negative` | 失效安全（≤0 / 远超限负向） | `clamp(0/-1/-1000/INT32_MIN)==10`；结果恒 `>0`（不退化为忙等） | REQ-COMM-005 | PASS |
| `test_clamp_above_upper_bound` | 失效安全（远超限正向） | `clamp(60001)==60000`、`clamp(INT32_MAX)==60000` | REQ-COMM-004 / 005 | PASS |
| `test_clamp_invariant_holds` | 失效安全（不变式） | 覆盖样本 `{INT32_MIN,-1000,-1,0,1,9,10,11,200,60000,60001,INT32_MAX}` 上恒满足 `P_min<=ret<=P_max ∧ ret>0` | REQ-COMM-005 | PASS |

- 失效安全红线对齐：「恰好越限」（`clamp(9)`、`clamp(60001)`）与「远超限」（`INT32_MIN`、`INT32_MAX`）两类均覆盖；核心不变式 `ret>0`（comm 线程睡眠周期恒 > 0、不忙等）专门断言。

## 3. 运行结果（mps2/an386 + QEMU）

命令（在 `bms-app/` 下）：`..\run-tests-coverage.ps1 -Board mps2/an386`

本次复跑（2026-06-30）comm 套件用例级结果（取自 `twister.json`）：

```
SUITE: bms.comm  status=passed
  passed  bms.comm.bms_comm.default_period_is_200
  passed  bms.comm.bms_comm.period_range_bounds
  passed  bms.comm.bms_comm.clamp_below_lower_bound
  passed  bms.comm.bms_comm.clamp_zero_or_negative
  passed  bms.comm.bms_comm.clamp_above_upper_bound
  passed  bms.comm.bms_comm.clamp_invariant_holds
```

全仓汇总：`4 of 4 executed test configurations passed (100.00%)` / `57 of 57 executed test cases passed (100.00%)`（4 套件 = afe 13 + soc 21 + protection 17 + comm 6），`configurations: 4, failures: 0, errors: 0`。
- 本次全量并发跑（`..\run-tests-coverage.ps1 -Board mps2/an386`，含 `--coverage` 插桩）四套件全部 PASS：`bms.comm`（qemu 47.9s）、`bms.afe`（qemu 49.4s）、`bms.protection`（qemu 49.4s）、`bms.soc`（qemu 117.2s）；总墙钟 172.4s。本轮 soc 未触发 60s 超时（先前一轮曾因并发+插桩争用使 soc 报环境性 Timeout，本轮已不复现，确认为 flaky 非逻辑失败）。
- 运行前置：本机首跑曾因 (a) twister 在 PATH 找不到 `gcovr`（venv 内 `gcovr.exe` 存在，临时将 `..\.venv\Scripts` 前置到 PATH 解决）、(b) 上次中断遗留的 `qemu-system-arm`/`ninja` 进程占用 `twister-out` 句柄导致清理失败（kill 残留进程后重试）而中止；二者均为环境问题，非测试或产品缺陷。

## 4. 覆盖率

- 门槛（CI）：行 ≥ 55% / 分支 ≥ 30%。
- 被测新源 `comm_period.c` 仅含一个纯函数、3 条返回路径 + 2 个分支判定，按测试输入分析：
  - `if (requested < lo) return lo;` —— 由 `clamp_below_lower_bound`(1/9/5)、`clamp_zero_or_negative`(0/-1/-1000/INT32_MIN) 命中（真分支）；
  - `if (requested > hi) return hi;` —— 由 `clamp_above_upper_bound`(60001/INT32_MAX) 命中（真分支）；
  - `return requested;` —— 由 `default_period_is_200`(200/100/1000)、`period_range_bounds`(10/60000) 命中（两个 if 的假分支）。
  - 结论：**comm_period.c 行覆盖与分支覆盖均为 100%（按测试输入静态分析）**。
- 工具侧说明：本次在 Windows + QEMU(`mps2/an386`) 上 gcovr 报 `Gcov data capture incomplete / GCOVR failed with 64`，**未能产出覆盖率数值报告**。这是 `run-tests-coverage.ps1` 与项目 CLAUDE.md 明确记录的已知限制——QEMU 串口 gcov dump 常被截断，可靠覆盖率须在 WSL2/Linux 下用 `native_sim` 链路获取；该限制**不影响上面的测试通过结论**。
- 覆盖率门槛达成判定：因 Windows/QEMU 工具链未产出机器可读的行覆盖率数值（环境限制，非测试缺陷），本次以 `covLine = -1` 表示「未测得」；CI（Linux + native_sim）将给出权威覆盖率。被测纯函数的逻辑覆盖经静态分析为 100%。

## 5. 替代验证（非「测试」方法的需求）

| 需求 | 方法 | 检视结论 |
|---|---|---|
| REQ-COMM-003 | 检视 | `app/Kconfig` 的 `config BMS_COMM_REPORT_PERIOD_MS` 含 `depends on BMS_COMM`；`CONFIG_BMS_COMM=n` 时配置项不可见。已满足。 |
| REQ-COMM-004（编译期侧） | 检视 | `app/Kconfig` 含 `range 10 60000`、`default 200`，`10>0`、`10<=200<=60000`。已满足（测试侧 `period_range_bounds` 另以本地常量复核 range 自洽）。 |
| REQ-COMM-006 | 检视 / 分析 | `comm.c` 中 `COMM_THREAD_PRIO=8`（最低，未抬升）；三处 `zbus_chan_read(..., K_MSEC(50))` 维持有限超时，无 `K_FOREVER`。已满足。 |
| REQ-COMM-007（日志侧） | 检视 | `bms_comm_init` 的 `LOG_INF` 同时打印生效周期（`comm_effective_period_ms()` 结果）与原始 `CONFIG_*`，使「配了却被钳制」可见。已满足；测试侧另以 `clamp_below_lower_bound` 断言钳制结果确定唯一。 |

## 6. 缺口与结论

- **缺口**：覆盖率数值未由工具产出（Windows/QEMU 已知限制），需 CI（Linux/native_sim）或 WSL2 链路补测得权威数值。功能/逻辑层无缺口。
- **结论**：comm 套件 6/6 通过、全仓 57/57 用例通过、4/4 配置 0 失败 0 错误（本轮 soc 亦在并发+插桩下 PASS，未触发 Timeout）；REQ-COMM-001/002/004/005/007 由测试覆盖，REQ-COMM-003/006 由检视/分析覆盖。被测纯函数逻辑覆盖 100%。除覆盖率数值这一环境性缺口外，本特性验证通过。
