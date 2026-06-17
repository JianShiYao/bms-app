# CI / 代码审查 借鉴清单

> 来源参考项目：`D:\__00_WorkSpace\__06_Study\stm32-project-template`
> 目标：把该模板成熟的「静态质量门禁」理念移植到 bms-app（Zephyr 工程）。
> 原则：**借鉴其严格门禁理念 + 检查项清单 + `.clang-tidy` 配置，但实现走 Zephyr 原生
> twister + SCA 钩子，而非手写 CMake target。**

## 现状差距（bms-app vs stm32-template）

| 能力 | stm32-template | bms-app 现状 | 处理 |
|---|---|---|---|
| 动态测试 + 覆盖率 | ❌ 无 | ✅ twister + ztest + gcovr | bms-app 已领先，**保留** |
| 代码格式化 | ✅ clang-format | ⚠️ 未配置（Zephyr 自带 `.clang-format`） | 借鉴 |
| clang-tidy（零告警） | ✅ | ❌ | 借鉴（用 SCA） |
| cppcheck + MISRA | ✅ MISRA-C:2023 | ❌ | 借鉴（分免费/商业两档） |
| editorconfig/yaml/py 风格 | ✅ | ❌ | 借鉴（低成本） |
| CI 流水线（门禁） | ✅ GitHub Actions | ❌ 无 CI | 借鉴（重点） |
| 多配置/多板编译矩阵 | ✅ 4 build types | ⚠️ 手动单板 | 借鉴（twister 天然支持） |
| Doxygen + Pages | ✅ | ❌ | 可选 |

## 借鉴清单（按优先级）

### P0 — CI 门禁（最该补，bms-app 完全没有）
- [ ] 建 `.github/workflows/ci.yml`，借鉴 stm32「门禁前置」思路：格式/风格检查先行，不过则不跑后续。
- [ ] CI 用 Zephyr 官方镜像 `ghcr.io/zephyrproject-rtos/ci`（已含 SDK + west），避免自建环境。
- [ ] 核心步骤：`west twister`（测试+覆盖率）→ 多板编译 → 静态分析。
- [ ] Job 依赖 DAG 仿照 stm32：`lint → {build, test, sca} → (report)`。

### P0 — 代码格式化（成本极低，Zephyr 已自带配置）
- [ ] 直接用 Zephyr 自带 `zephyr/.clang-format`，**不要从 stm32 拷**（风格不同，bms-app 跟 Zephyr 风格）。
- [ ] CI 加格式检查：`git clang-format --diff`，或 Zephyr 官方 `scripts/checkpatch.pl`（更全面）。
- [ ] 借鉴 stm32 的 `check-format`（只检查）+ `run-format`（自动改）双入口 → bms-app 对应「CI 检查 / 本地 `clang-format -i`」。

### P1 — 静态分析（借鉴 stm32 的 clang-tidy + cppcheck，但走 Zephyr SCA）
本地 Zephyr 4.4 实测可用的 SCA 变体（`zephyr/cmake/sca/`）：
`gcc`、`clang`、`codechecker`（含 clang-tidy + cppcheck）、`sparse`；
MISRA 类：`eclair`、`polyspace`、`coverity`、`iar_c_stat`、`cpptest`（均商业）。

- [ ] 免费档，本地/CI 直接可跑：
  ```
  west build -b mps2/an386 bms-app/app -- -DZEPHYR_SCA_VARIANT=gcc          # GCC 分析器，零依赖
  west build -b mps2/an386 bms-app/app -- -DZEPHYR_SCA_VARIANT=codechecker  # 含 clang-tidy + cppcheck
  ```
- [ ] 借鉴 stm32 的 `.clang-tidy` 规则集（cert-*/readability-* + 命名规范 + warnings-as-error）——
      该配置与构建系统无关，**可整份拷到 bms-app 根目录复用**。
- [ ] 借鉴 stm32 的 cppcheck 严格参数（`--enable=all --inconclusive --safety` 等），在 codechecker/cppcheck 配置中对齐。

### P1 — 多板编译矩阵（比 stm32 更适合 bms-app）
- [ ] stm32 是「4 种 build type 矩阵」；bms-app 对应「多目标板矩阵」，用 twister 一条命令覆盖：
  ```
  west twister -T bms-app/tests -p mps2/an386 -p native_sim -p bms_f405
  ```
- [ ] 借鉴「每种配置都必须编过」的强制思想。

### P2 — 通用风格检查（低成本，直接照搬）
- [ ] `.editorconfig` 可几乎原样拷贝。
- [ ] CI 加 `editorconfig-checker`、`yamllint --strict`（west.yml / CI yml 受益）、`flake8`（若有 Python 脚本）。

### P2 — 文档（可选）
- [ ] Doxygen + GitHub Pages 自动发布——按需，可后置。

### P3 — MISRA（看是否做功能安全，有许可证门槛）
- [ ] BMS 属安全相关领域，MISRA 价值高，但注意：
  - Zephyr 原生 MISRA 走 `eclair` / `polyspace` / `coverity`——**均为商业工具，需许可证**。
  - 免费替代：cppcheck 的 misra addon（stm32 就是这么做），但 **MISRA 规则全文受版权保护**，
    stm32 仅放了 `misra-c-2023-headlines.txt`（标题行），借鉴时需自备规则文本。
- [ ] 建议：先上免费的 `gcc`/`codechecker`，确有合规需求再评估商业工具。

## 落地顺序建议
1. 先补 **CI 骨架 + twister + 格式检查**（P0）——投入最小，立刻拦低级问题。
2. 再加 **SCA（gcc/codechecker）+ 借用 stm32 的 `.clang-tidy`**（P1）。
3. 最后按需考虑 **MISRA / Doxygen**（P2/P3）。

## 参考文件（stm32-project-template）
- CI 流水线：`.github/workflows/ci-pipeline.yml`
- clang-tidy 配置：`.clang-tidy`
- cppcheck/MISRA：`CMakeLists.txt`（cppcheck target）、`lint/misra.json`
- 通用风格：`.editorconfig`
- 文档：`docs/doxygen/Doxyfile`
