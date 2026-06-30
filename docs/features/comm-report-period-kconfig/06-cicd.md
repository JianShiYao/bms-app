<!--
  CI/CD 验证说明（敏捷-V 右腿：持续验证，bms-cicd 贯穿全程）。
  特性：comm 模块 CAN 上报周期改为 Kconfig (CONFIG_BMS_COMM_REPORT_PERIOD_MS) 可配，默认 200ms。
  阶段：⑥ 持续验证。输入：.github/workflows/ci.yml、release.yml；tests/bms/comm/；05-test-report.md。
  范围：仅 CI/CD 与构建配置，不涉及产品逻辑代码。交付物语言：中文。
-->
# CI/CD 验证说明：comm 模块 CAN 上报周期 Kconfig 可配

> 特性 slug：`comm-report-period-kconfig`
> 阶段：敏捷-V 右腿 —— 持续验证（`bms-cicd`，贯穿全程）
> 输入：`.github/workflows/ci.yml`、`release.yml`；`tests/bms/comm/`；`05-test-report.md`、`traceability.md`
> 范围：仅 CI/CD 与构建配置，不涉及产品逻辑代码。
> 交付物语言：中文。

---

## 1. 结论摘要

- **本特性的测试（testcase `bms.comm`，6 用例）已被现有 CI 自动覆盖，`ci.yml` 无需任何改动。**
- 已实证：`west twister -T tests -p native_sim --list-tests` 列出全部 6 条 `bms.comm.bms_comm.*` 用例（见 §2.1），即 CI 的 `test-coverage` job 用同一命令会自动构建并运行该套件。
- 覆盖率门禁保持原值（行 ≥ 55%、分支 ≥ 30%），**未降低**。
- 已知限制（QEMU gcov 串口转储截断）**不影响 CI**：CI 走 `native_sim`（原生 `.gcda` 文件），不经 QEMU 串口转储路径，覆盖率在 CI 上可靠产出。

---

## 2. CI 如何验证本特性（构建 + 测试 + 覆盖率）

现有 `ci.yml` 在每次 `push`/`pull_request`/手动触发时跑以下 job，本特性被相关 job 直接覆盖：

| Gate | Job | 与本特性的关系 |
|---|---|---|
| 1 | `format` | clang-format 校验 `app/ drivers/ tests/` 下 `*.c/*.h`，含 `tests/bms/comm/src/main.c` 与新增 `app/src/bms/comm/comm_period.c`、`comm.c`。 |
| 2 | `build` (matrix: `mps2/an386`, `native_sim`, `qmxx_f407zg`) | 编译应用，comm 模块（含新增 Kconfig `BMS_COMM_REPORT_PERIOD_MS` 与 `comm_period.c`）随 `app` 一并构建。 |
| 3 | `test-coverage` (`native_sim`) | **核心**：跑 Twister + 覆盖率，自动发现并执行 `bms.comm`（见 §2.1）。 |
| 4 | `sca-gcc` | gcc `-fanalyzer` 静态分析，含 `comm.c` / `comm_period.c`。 |
| 5 | `clang-tidy` | CERT/可读性硬门禁，含 `app/src` 下 comm 源（按 `.clang-tidy` 仅分析 `app/include/bms/*.h`，编译单元含 comm）。 |
| 6/7 | `editorconfig` / `yamllint` | 卫生门：编码/行尾/换行（含本特性新文件）、仓库 YAML 严格校验（含 `testcase.yaml`）。 |

> `cppcheck-misra` 为非阻断观察 job（`continue-on-error`），同样会扫到 comm 源，仅报告不阻断。

### 2.1 测试发现机制（为何无需改 workflow）

`test-coverage` job 的关键命令：

```yaml
west twister -T bms-app/tests -p native_sim --inline-logs \
  --coverage --coverage-tool gcovr -O twister-out
```

- `-T bms-app/tests` 让 Twister **递归扫描** `tests/` 下所有 `testcase.yaml`，自动纳入新测试，无需在 workflow 里逐个登记。
- 本特性新增 `tests/bms/comm/testcase.yaml`，声明 testcase `bms.comm`，`platform_allow` 含 `native_sim`：

  ```yaml
  tests:
    bms.comm:
      platform_allow:
        - mps2/an386
        - native_sim
      tags: [bms, comm]
  ```

- **实证（2026-06-30，本仓库 `native_sim`）**：执行
  `..\.venv\Scripts\python.exe -m west twister -T tests -p native_sim --list-tests`
  列出本套件全部 6 条用例，确认随目录自动纳入：

  ```
  - bms.comm.bms_comm.clamp_above_upper_bound
  - bms.comm.bms_comm.clamp_below_lower_bound
  - bms.comm.bms_comm.clamp_invariant_holds
  - bms.comm.bms_comm.clamp_zero_or_negative
  - bms.comm.bms_comm.default_period_is_200
  - bms.comm.bms_comm.period_range_bounds
  ```

  同次扫描同时拾取了 `bms.afe` / `bms.protection` / `bms.soc`，旁证「新测试随目录自动纳入」的机制对本套件同样成立。
- 因此 CI 在 `native_sim` 上会自动构建并运行 `bms.comm`（6 用例：默认值 / range 边界 / 越下界钳制 / ≤0 钳制 / 越上界钳制 / 不变式）。

### 2.2 覆盖率门禁

`test-coverage` job 在 Twister 跑完后，用项目自管的 gcovr 重新统计并门禁（job 内第二步 `App-scoped coverage gate`）：

```yaml
gcovr --root "$GITHUB_WORKSPACE" \
  --filter 'bms-app/app/(src|include)/.*' \
  --gcov-ignore-parse-errors=all \
  --print-summary \
  --fail-under-line 55 \
  --fail-under-branch 30 \
  twister-out
```

- **门禁现状**：行覆盖率 **≥ 55%**、分支覆盖率 **≥ 30%**，未达标即 fail（阻断合并）。
- 基线（2026-06-18）：行 61.0%、分支 39.1%；阈值留有余量以避免抖动误报。
- 被测新源 `app/src/bms/comm/comm_period.c`（纯函数 `bms_comm_clamp_period_ms`）位于 filter `bms-app/app/(src|include)/.*` 覆盖范围内，其覆盖数据自动计入。该函数仅 3 条返回路径 + 2 处分支判定，6 条用例的输入对全部真/假分支均有命中（详见 `05-test-report.md` §4），只会**提高** `app/src` 覆盖率，对现有门禁有利。
- **本次未改动门禁阈值，未降低既有覆盖率要求。**

---

## 3. 当前 CI 是否需要改动

**不需要。** 理由：

1. 测试发现：`-T bms-app/tests` 已递归纳入 `tests/bms/comm/`，无需新增 job 或修改命令（§2.1 已实证）。
2. 平台：`bms.comm` 的 `platform_allow` 含 `native_sim`，与 `test-coverage` job 的 `-p native_sim` 匹配，会被实际执行。
3. 覆盖率：gcovr filter `bms-app/app/(src|include)/.*` 已覆盖 `app/src/bms/comm/comm_period.c`、`comm.c` 与 `app/include/bms/`，新测试产生的覆盖数据自动计入。
4. 构建/静态分析/格式/卫生：均按目录通配，自动覆盖 comm 新增源文件与 `testcase.yaml`。

故本特性的持续验证完全复用既有 workflow 结构，**零改动**，避免了重复 job。

---

## 4. 已知限制：QEMU gcov 覆盖率截断对 CI 的影响与建议

### 4.1 现象与根因

- 本地 `run-tests-coverage.ps1`（板 `mps2/an386`，QEMU）跑覆盖率时报 `GCOVR failed with 64 / Gcov data capture incomplete`（详见 `05-test-report.md` §4）。
- 根因：QEMU 目标的 gcov 数据通过**串口转储**回主机，长输出经常被截断，导致 gcovr 拿不到完整数据。
- 注意：**这是覆盖率采集链路问题，不影响测试 verdict**——本地（comm 6/6、全仓 57/57）与 CI 的测试结论均有效。

### 4.2 对 CI 的影响：无

- CI 的 `test-coverage` job 使用 **`native_sim`**，覆盖率数据由本机原生进程直接写 `.gcda` 文件，**不经过 QEMU 串口转储路径**，因此不受该截断问题影响。
- 即 CI 上的覆盖率统计可靠；QEMU gcov 截断仅是 Windows 本地 `mps2/an386` 链路的限制。

### 4.3 建议

1. **本地可靠覆盖率**：在 WSL2/Linux 下用 `native_sim` 链路跑覆盖率（`bms.comm` 已 `platform_allow: native_sim`），与 CI 口径一致。
2. **保持 CI 覆盖率口径在 `native_sim`**：当前设计正确，不要把覆盖率门禁切到 QEMU/`mps2/an386` 链路，否则会引入串口截断导致的抖动/误判。
3. **QEMU 仅作功能回归**：`mps2/an386` 继续用于 `build` job 的编译验证与本地功能跑测，不承担覆盖率采集职责。
4. **门禁演进**：随 comm 等模块测试增长，可逐步上调 `--fail-under-line` / `--fail-under-branch`（只升不降），收紧回归防护。

---

## 5. 状态

- `ci.yml`：**无需改动**（新测试 `bms.comm` 已被 `-T tests` 自动纳入，已实证）。
- 覆盖率门禁：行 55% / 分支 30%，**保持不变，未降低**。
- 本文档：记录 CI 验证方式、改动结论、门禁现状、QEMU gcov 截断的影响与建议。

_状态：DONE（⑥ 持续验证说明已产出；workflow 零改动）_
