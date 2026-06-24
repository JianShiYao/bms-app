# CI / 代码审查 借鉴清单

> 来源参考项目：`D:\__00_WorkSpace\__06_Study\stm32-project-template`
> 目标：把该模板成熟的「静态质量门禁」理念移植到 bms-app（Zephyr 工程）。
> 原则：**借鉴其严格门禁理念 + 检查项清单 + `.clang-tidy` 配置，但实现走 Zephyr 原生
> twister + SCA 钩子，而非手写 CMake target。**

**状态（2026-06）**：P0 / P1 已全部落地，质量门禁体系已成型。本清单从「待办计划」转为
**「已完成对照 + 剩余可选项」**。门禁的权威说明见 [../CLAUDE.md](../CLAUDE.md)「质量门禁」与
[development-workflow.md](development-workflow.md)；本文只记录「相对 stm32 模板借了什么、怎么落地的、还差什么」。

## 现状差距（bms-app vs stm32-template）

| 能力 | stm32-template | bms-app 现状 | 处理 |
|---|---|---|---|
| 动态测试 + 覆盖率 | ❌ 无 | ✅ twister + ztest + gcovr（CI 门槛 行≥55%/分支≥30%） | bms-app 领先，**已保留** |
| 代码格式化 | ✅ clang-format | ✅ Zephyr `.clang-format` + CI 检查 + pre-commit 钩子 | ✅ 已落地 |
| clang-tidy（零告警） | ✅ | ✅ `.clang-tidy`（cert-*/readability-*，WarningsAsErrors）+ CI 硬门 | ✅ 已落地 |
| cppcheck + MISRA | ✅ MISRA-C:2023 | ✅ cppcheck + MISRA addon（**warn-only**，pre-push + check.ps1） | ✅ 已落地（未入 CI，见 P3） |
| editorconfig/yaml/py 风格 | ✅ | ⚠️ `.editorconfig` 有；editorconfig-checker/yamllint/flake8 未入 CI | 部分（见 P2） |
| CI 流水线（门禁） | ✅ GitHub Actions | ✅ `.github/workflows/ci.yml`（format→build×2 / test-cov / sca / tidy） | ✅ 已落地 |
| 多配置/多板编译矩阵 | ✅ 4 build types | ✅ twister/build 矩阵 mps2/an386 + native_sim（bms_f405 暂注释） | ✅ 已落地 |
| Doxygen + Pages | ✅ | ❌ | 可选（见 P2） |

## 借鉴清单（按优先级）

### P0 — CI 门禁 ✅ 已落地
- [x] 建 `.github/workflows/ci.yml`：format 检查先行；后续 build/test/sca/tidy 各为独立 job。
- [x] CI 用 `zephyrproject-rtos/action-zephyr-setup@v1`（自动装 SDK + west）。
      → 与原计划的 `ghcr.io/.../ci` 镜像等价，官方 action 维护更省心。
- [x] 核心步骤：`west twister`（测试+覆盖率，native_sim）→ 多板编译 → 静态分析（sca-gcc + clang-tidy）。
- [x] Job 结构仿 stm32：`format`（前置门）→ 并行 `build / test-coverage / sca-gcc / clang-tidy`。
- 另有 `release.yml`：打 `vX.Y.Z` tag 触发，校验 tag==`VERSION` → 构建 → 发布固件 + SHA256SUMS。

### P0 — 代码格式化 ✅ 已落地
- [x] 直接用 Zephyr 风格 `.clang-format`（**未从 stm32 拷**，避免风格冲突）。
- [x] CI 加格式检查（clang-format，pin 到与本地一致的版本）。
- [x] stm32「check-format / run-format」双入口 → bms-app 对应 `scripts\format.ps1 -Check`（只查）/ `format.ps1`（自动改）。
- [x] pre-commit 钩子对暂存的 app/drivers/tests 下 `.c/.h` 跑 clang-format（硬拒绝）。

### P1 — 静态分析 ✅ 已落地（实现选型与原计划有调整）
原计划走 `codechecker` 伞变体（含 clang-tidy + cppcheck）；**实际拆成三条独立门**，便于单独控制阻断强度：

- [x] **SCA = gcc 分析器**：`-DZEPHYR_SCA_VARIANT=gcc`，CI job `sca-gcc`，用 `scripts/sca-check.sh` 把告警范围
      收敛到 app 代码后判定（CI 硬门）。本地 check.ps1 同步跑。
- [x] **clang-tidy** 独立硬门（CI job `clang-tidy`，`.clang-tidy` 内 `WarningsAsErrors`）。
      → 借鉴了 stm32 的 `.clang-tidy` 规则集思路（cert-*/readability-* + warnings-as-error）。
      → **Windows 本地标 SKIP**（需 native_sim host flags，Win 配不了），以 CI(Linux)/WSL2 为准。
- [x] **cppcheck + MISRA**（warn-only）：`scripts/cppcheck-run.sh`，pre-push 跑独立模式、check.ps1 跑 project 模式
      （用 mps2 的 `compile_commands.json`，假阳性少）；噪声/deviation 在 `.cppcheck-suppressions` 维护。
      MISRA addon 不入库，本地 `bash scripts/setup-cppcheck-misra.sh` 拉取。
- 说明：未采用 `codechecker` 是因为三条独立门能各自设阻断强度（gcc/tidy 硬门、cppcheck warn-only），
  且免去 codechecker 额外依赖。

### P1 — 多板编译矩阵 ✅ 已落地
- [x] 用 build/twister 矩阵覆盖多板：CI `build` job 矩阵 = `mps2/an386` + `native_sim`；
      test-coverage 在 `native_sim` 跑。
- [x] 「每种配置都必须编过」的强制思想已体现（矩阵任一板失败即红）。
- [ ] `bms_f405` 暂从矩阵注释掉（板模板 dts/defconfig 未完善）；待板就绪后加入矩阵。

### P2 — 通用风格检查（部分落地）
- [x] `.editorconfig` 已就位（编辑器实时生效）。
- [ ] CI 加 `editorconfig-checker`、`yamllint --strict`（west.yml / *.yml 受益）、`flake8`（Python 脚本）。
      → 低成本、可随时补；当前未进 CI。

### P2 — 文档（可选，未做）
- [ ] Doxygen + GitHub Pages 自动发布——按需后置。

### P3 — MISRA（已上免费档，商业档按需）
- [x] 已上免费 cppcheck misra addon（warn-only），符合下面「非阻断」阶梯的起步档。
- [ ] 若做功能安全合规，再评估商业工具（`eclair`/`polyspace`/`coverity`，均需许可证）。
  - 免费 addon 的 **MISRA 规则全文受版权保护**，仓库不含规则文本，本地自备（参考 stm32 仅放标题行 `misra-c-2023-headlines.txt`）。
- [ ] **把 cppcheck/MISRA 升级进 CI**（当前仅本地 pre-push + check.ps1）——按阻断强度递进：
      ① 本地试跑调参 + 建 suppression/基线（**已在此档**）→ ② 进 CI 非阻断（`continue-on-error`/只出报告或 PR 注释）
      → ③ 噪声归零后升必过门禁 → ④ 证明低噪后下放 `pre-push`。
  - 切忌一上来把吵的工具设成 CI 必过门；用 **baseline 只对新增代码报错**。
  - 详见 [development-workflow.md §8](development-workflow.md) 的「按阻断强度递进」阶梯表。

## 进度小结
1. ✅ **CI 骨架 + twister + 格式检查（P0）** —— 已完成。
2. ✅ **SCA（gcc）+ clang-tidy（借 stm32 `.clang-tidy`）+ cppcheck/MISRA（P1）** —— 已完成。
3. ⏳ **剩余可选项**：
   - P2：editorconfig-checker / yamllint / flake8 进 CI；Doxygen + Pages。
   - P3：cppcheck/MISRA 升级进 CI（非阻断→基线→门禁）；商业 MISRA 工具按合规需求评估。
   - 板：`bms_f405` dts/defconfig 完善后加入编译矩阵。

## 参考文件（stm32-project-template）
- CI 流水线：`.github/workflows/ci-pipeline.yml`
- clang-tidy 配置：`.clang-tidy`
- cppcheck/MISRA：`CMakeLists.txt`（cppcheck target）、`lint/misra.json`
- 通用风格：`.editorconfig`
- 文档：`docs/doxygen/Doxyfile`
