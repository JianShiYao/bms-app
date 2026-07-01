# bms-app CI / 质量门禁清单

> 目标：bms-app（Zephyr 工程）的静态/动态质量门禁实践清单。
> 原则：**严格门禁理念 + 完整检查项 + `.clang-tidy` 配置；实现走 Zephyr 原生
> twister + SCA 钩子，而非手写 CMake target。**

**状态（2026-06）**：P0 / P1 已全部落地，质量门禁体系已成型。本清单从「待办计划」转为
**「已完成对照 + 剩余可选项」**。门禁的权威说明见 [../CLAUDE.md](../CLAUDE.md)「质量门禁」与
[process-workflow.md](process-workflow.md)；本文只记录「门禁做了什么、怎么落地的、还差什么」。

## 门禁能力现状

| 能力 | bms-app 现状 | 状态 |
|---|---|---|
| 动态测试 + 覆盖率 | twister + ztest + gcovr（CI 门槛 行≥55%/分支≥30%） | ✅ 已落地 |
| 代码格式化 | Zephyr `.clang-format` + CI 检查 + pre-commit 钩子 | ✅ 已落地 |
| clang-tidy（零告警） | `.clang-tidy`（cert-*/readability-*，WarningsAsErrors）+ CI 硬门 | ✅ 已落地 |
| cppcheck + MISRA | cppcheck + MISRA addon（**warn-only**，pre-push + check.ps1） | ✅ 已落地（未入 CI，见 P3） |
| editorconfig/yaml/py 风格 | `.editorconfig` 有；editorconfig-checker/yamllint/flake8 未入 CI | ⚠️ 部分（见 P2） |
| CI 流水线（门禁） | `.github/workflows/ci.yml`（format→build×2 / test-cov / sca / tidy） | ✅ 已落地 |
| 多配置/多板编译矩阵 | twister/build 矩阵 mps2/an386 + native_sim（bms_f405 暂注释） | ✅ 已落地 |
| Doxygen + Pages | Doxygen 公共 API + GitHub Pages | ✅ 已落地 |
| 供应链/产物安全（SBOM·签名·provenance） | 无 | 可立即采纳（见 P2） |

## 门禁清单（按优先级）

### P0 — CI 门禁 ✅ 已落地
- [x] 建 `.github/workflows/ci.yml`：format 检查先行；后续 build/test/sca/tidy 各为独立 job。
- [x] CI 用 `zephyrproject-rtos/action-zephyr-setup@v1`（自动装 SDK + west）。
      → 与原计划的 `ghcr.io/.../ci` 镜像等价，官方 action 维护更省心。
- [x] 核心步骤：`west twister`（测试+覆盖率，native_sim）→ 多板编译 → 静态分析（sca-gcc + clang-tidy）。
- [x] Job 结构：`format`（前置门）→ 并行 `build / test-coverage / sca-gcc / clang-tidy`。
- 另有 `release.yml`：打 `vX.Y.Z` tag 触发，校验 tag==`VERSION` → 构建 → 发布固件 + SHA256SUMS。

### P0 — 代码格式化 ✅ 已落地
- [x] 直接用 Zephyr 风格 `.clang-format`（避免与外部风格冲突）。
- [x] CI 加格式检查（clang-format，pin 到与本地一致的版本）。
- [x] 格式化双入口：`scripts\format.ps1 -Check`（只查）/ `format.ps1`（自动改）。
- [x] pre-commit 钩子对暂存的 app/drivers/tests 下 `.c/.h` 跑 clang-format（硬拒绝）。

### P1 — 静态分析 ✅ 已落地（实现选型与原计划有调整）
原计划走 `codechecker` 伞变体（含 clang-tidy + cppcheck）；**实际拆成三条独立门**，便于单独控制阻断强度：

- [x] **SCA = gcc 分析器**：`-DZEPHYR_SCA_VARIANT=gcc`，CI job `sca-gcc`，用 `scripts/sca-check.sh` 把告警范围
      收敛到 app 代码后判定（CI 硬门）。本地 check.ps1 同步跑。
- [x] **clang-tidy** 独立硬门（CI job `clang-tidy`，`.clang-tidy` 内 `WarningsAsErrors`）。
      → `.clang-tidy` 规则集：cert-*/readability-* + warnings-as-error。
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

### P2 — 文档 ✅ 已落地
- [x] Doxygen + GitHub Pages 自动发布（公共 API）。

### P2 — 供应链 / 产物安全（固件签名 · SBOM · attestation）

> 出自固件供应链最佳实践 + 架构讨论。
> 三项均为**纯 CI 步骤、与 MCUboot 无关**，可立即采纳；与设备端验签的边界见末尾。

- [ ] **SBOM（SPDX）**：用 Zephyr 原生 `west spdx`（无需新依赖）。
      流程：构建前 `west spdx --init -d build` → `west build` → `west spdx -d build`，
      产出 `build/spdx/*.spdx` 作为 release 资产上传。
- [ ] **Release 产物签名**：在 `release.yml` 现有 SHA256SUMS 基础上，对 `.bin` + SHA256SUMS 签名
      （`cosign sign-blob` keyless/OIDC，或 minisign/GPG 用存储密钥），签名随 release 发布；
      下游 `cosign verify-blob` 验来源。**注意：这是「产物出处签名」，验证者是人/下游，区别于 MCUboot 的启动验签。**
- [ ] **Build provenance（SLSA attestation）**：CI/release 用 GitHub `actions/attest-build-provenance@v1`
      （需 `permissions: id-token: write` + `attestations: write`）对构建产物生成出处证明，
      `gh attestation verify <artifact>` 可校验「此产物由本仓库此 commit 此流水线构建」。

**边界（不在本节，留待真机阶段）**：
- **设备端启动验签 / secure boot** 需 **MCUboot**（imgtool 签名 + 启动验签 + 回滚），涉及 dual-slot 分区、
  密钥管理、升级通道，且须与「上电默认安全态」一并设计——接 `bms_f405` 真机阶段再评估。当前 `west.yml` allowlist 未含 mcuboot。
- **设备远程证明（remote attestation, TF-M/PSA）** 需硬件 RoT；**F405（Cortex-M4 无 TrustZone）不支持**，不做。

### P3 — MISRA（已上免费档，商业档按需）
- [x] 已上免费 cppcheck misra addon（warn-only），符合下面「非阻断」阶梯的起步档。
- [ ] 若做功能安全合规，再评估商业工具（`eclair`/`polyspace`/`coverity`，均需许可证）。
  - 免费 addon 的 **MISRA 规则全文受版权保护**，仓库不含规则文本，本地自备（如仅保留标题行 `misra-c-2023-headlines.txt`）。
- [ ] **把 cppcheck/MISRA 升级进 CI**——按阻断强度递进：
      ① 本地试跑调参 + 建 suppression/基线（**已完成**）→ ② 进 CI 非阻断（`cppcheck-misra` job，`continue-on-error`，**已完成**）
      → ③ 噪声归零后升必过门禁（待办，需在 GitHub 分支保护加该 check）→ ④ 证明低噪后下放 `pre-push`（`CPPCHECK_FAIL=1`）。
  - 切忌一上来把吵的工具设成 CI 必过门；用 **baseline 只对新增代码报错**。
  - 详见 [process-workflow.md §8](process-workflow.md) 的「按阻断强度递进」阶梯表。

## 进度小结
1. ✅ **CI 骨架 + twister + 格式检查（P0）** —— 已完成。
2. ✅ **SCA（gcc）+ clang-tidy + cppcheck/MISRA（P1）** —— 已完成。
3. ⏳ **剩余可选项**：
   - P2：editorconfig-checker / yamllint / flake8 进 CI；供应链/产物安全（SBOM · 产物签名 · build provenance，纯 CI 可立即采纳）。
   - P3：cppcheck/MISRA 升级进 CI（非阻断→基线→门禁）；商业 MISRA 工具按合规需求评估。
   - 板：`bms_f405` dts/defconfig 完善后加入编译矩阵。
