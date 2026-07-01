# 质量门禁事实表

本文记录当前 CI / 本地检查的实际门禁状态。流程原则见
[workflow.md](../process/workflow.md)，质量全景见
[management.md](management.md)。当 `.github/workflows/ci.yml`
变化时，优先同步本文，再更新其他文档引用。

## CI 门禁

| Gate | CI job | 阻断 | 本地入口 | 覆盖范围 |
|---|---|---|---|---|
| 格式 | `format` | 是 | `scripts\format.ps1 -Check` | `app/`、`drivers/`、`tests/` 下 `.c/.h` |
| 多板构建 | `build (mps2/an386)` | 是 | `west build -b mps2/an386 app` | QEMU Cortex-M4F 构建 |
| 多板构建 | `build (native_sim)` | 是 | WSL2/Linux 上 `west build -b native_sim app` | Linux native 仿真构建 |
| 多板构建 | `build (qmxx_f407zg)` | 是 | `west build -b qmxx_f407zg app` | STM32F407ZGT6 开发板构建 |
| 测试与覆盖率 | `test-coverage` | 是 | CI/WSL2 权威；Windows 用 QEMU 预检 | `west twister -T tests -p native_sim`，覆盖率行 >= 60%、分支 >= 38% |
| GCC 静态分析 | `sca-gcc` | 是 | `scripts\check.ps1` 全量模式 | `-fanalyzer`，只拦 app 代码告警 |
| clang-tidy | `clang-tidy` | 是 | WSL2/Linux；Windows 通常 `SKIP` | `app/src/**/*.c`，`WarningsAsErrors` |
| cppcheck + MISRA | `cppcheck-misra` | 是 | `scripts\check.ps1` 预检为 warn-only；CI 权威 | project mode，`CPPCHECK_FAIL=1` |
| EditorConfig 卫生 | `editorconfig` | 是 | CI 权威 | 尾随空格、末行换行、LF、charset |
| YAML lint | `yamllint` | 是 | CI 权威 | 仓库 YAML |
| 测试存在性 | `test-files` | 是 | `python scripts/check-test-files.py` | 每个 `app/src/bms/<module>` 须有测试或登记豁免 |
| 文件头 | `file-headers` | 是 | `python scripts/check-file-headers.py` | `app/` 中 SPDX 与 Doxygen 文件头 |

## 本地检查定位

`scripts\check.ps1` 是 Windows 本地预检，不承诺完全复刻 Linux CI。它用于尽早暴露常见失败项；
`native_sim`、clang-tidy parity、覆盖率和若干卫生门以 CI/WSL2 为权威。

本地 `cppcheck+MISRA` 仍保持 warn-only，用于快速反馈和 suppression 调整；CI 的
`cppcheck-misra` 是合并阻断门。

## 测试数量口径

文档和 PR 模板不固定写测试总数。测试用例数量随新增 `ZTEST()` 变化，以 `west twister`
和 CI 报告为准。
