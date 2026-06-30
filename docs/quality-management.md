# 质量管控全景（需求 → 编码 → 测试 → CI/CD）

本文梳理 bms-app 从需求到交付各阶段**已落地的质量管控**与**待补齐项**，便于评估当前成熟度与规划下一步。
相关文档：开发流程见 [process-workflow.md](process-workflow.md)；CI 借鉴路线图见 [quality-ci-checklist.md](quality-ci-checklist.md)；待办见 [../TODO.md](../TODO.md)。

> 当前状态（2026-06-24）：QEMU 阶段骨架 + 完整 CI/CD 质量门禁；cppcheck/MISRA 已上（本地 warn-only）；
> afe 已补单测（20 例）且采样后端可切换（stub/sim/adc）；尚未接真实硬件（STM32F405）。

## 一、全景总览

| 阶段 | 已落地管控 | 阻断强度 | 主要待补齐 |
|---|---|---|---|
| 需求 | 设计/规格文档（`docs/`） | 软 | 需求管理、验收标准、需求↔测试追溯 |
| 设计 | 架构文档 + 分层/zbus 解耦 | 软 | 设计评审流程、API 文档(Doxygen)、FMEA/安全分析 |
| 编码 | clang-format（pre-commit/CI 强制）、.editorconfig（编辑器实时，**依赖插件**）、命名规范、**app `-Werror`**（编译告警即错，限 app 目标） | clang-format=提交前拦截；`-Werror`=CI 构建拦截；editorconfig=仅编辑器层 | — |
| 静态分析 | gcc `-fanalyzer`、clang-tidy(CERT/可读性，硬)、cppcheck+MISRA(本地 + CI 非阻断观察) | CI 必过；cppcheck/MISRA=非阻断(`continue-on-error`) | cppcheck/MISRA 升必过门、复杂度度量 |
| 测试 | ztest 单测(soc/protection/afe，47 例)、twister、app 覆盖率门禁(line≥55%) | CI 必过 | 集成/HIL 测试、balancing/comm 单测、分支覆盖偏低 |
| 构建 | 多目标编译矩阵(mps2/an386 + native_sim)、app `-Werror`、**CI 工具版本已 pin**(clang-format 22.1.5 / gcovr 7.2 / runner 镜像 ubuntu-24.04 锁 apt cppcheck·clang-tidy) | CI 必过 | bms_f405 板、Release 多板 |
| 发布(CD) | tag→固件制品+SHA256+Release、tag↔VERSION 校验 | 失败即无 Release | 固件签名、SBOM、制品 attestation |
| 依赖/供应链 | west manifest pin、dependabot(actions) | 软 | pip 依赖、SBOM、依赖漏洞扫描 |
| 流程治理 | PR 流 + 6 门禁分支保护、Conventional Commits、CODEOWNERS、CHANGELOG | 合并拦截 | 多人评审(单人=0)、需求/缺陷跟踪系统 |

> 流程门补充：特性开发的迭代准入/准出(DoR/DoD)、安全改动路径、追溯 DoD 门见 [process-workflow.md §1.3 / §2 / §7](process-workflow.md)；PR 模板含「追溯链无断链」「安全相关改动」勾选项。本文对应的方法论依据见 [concept-methodology.md](concept-methodology.md)。

## 二、分阶段详述

### 1. 需求与设计
**现状**：系统设计见 [concept-architecture.md](concept-architecture.md)（三层架构、zbus 数据流、保护优先级、fail-safe 原则）；功能规格散见 `docs/`。模块化由 Kconfig 开关表达（`CONFIG_BMS_*`）。**已提供需求工程模板** [templates/](templates/)（需求规格 + 设计规格 + 需求↔测试追溯，EARS 句式、ID 规范、与 ztest 关联约定）。
**待补齐**：
- 按模块**填充**需求/设计文档并建立追溯矩阵（模板已就绪，内容待写）；
- 无成文**设计评审**门槛；API 文档（Doxygen）未搭建。

### 2. 编码（提交前）
**现状**（提交前即拦截）。规则文件本身不"触发"检查，触发靠读取它的工具，分三层：
- 格式：
  - [.clang-format](../.clang-format)（Zephyr 风格 tab/8）——**权威**。触发链：保存时由 C/C++ 扩展套用 → 提交时 pre-commit 用 `clang-format --dry-run --Werror` 硬拦截 → CI 设 format gate 兜底。
  - [.editorconfig](../.editorconfig)（缩进/charset/去尾空格/结尾换行；C 段已对齐 tab/8 以避免与 clang-format 冲突）——**仅编辑器层，依赖插件**：需安装 `EditorConfig.EditorConfig`（已列入 [.vscode/extensions.json](../../.vscode/extensions.json) 推荐）才生效，**不装则该文件不起作用**；它无独立提交门禁，非 C 文件（yaml/json/ps1/md）的排版仅靠它。
  - 行尾 LF 由 [.gitattributes](../.gitattributes) 在提交时强制（与是否装插件无关）。
- 命名/规范：[.clang-tidy](../.clang-tidy)（C/snake_case，cert-*/readability-*）。
- 本地门禁：[scripts/hooks/pre-commit](../scripts/hooks/pre-commit)（需 `git config core.hooksPath scripts/hooks` 注册一次）校验暂存 `.c/.h` 格式；`scripts/format.ps1` 一键格式化/检查。
**待补齐**：无（编码层管控已较完整）。

### 3. 静态分析（SCA）
**现状**：
- `sca-gcc`：`-DZEPHYR_SCA_VARIANT=gcc`（`-fanalyzer`），经 [scripts/sca-check.sh](../scripts/sca-check.sh) 只拦 `app/` 的 `-Wanalyzer` 告警。**CI 必过**。
- `clang-tidy`：硬门禁（`WarningsAsErrors`，当前 0 告警），CERT + 可读性 + snake_case 命名。**CI 必过**。
- `cppcheck + MISRA`（misra addon）：[scripts/cppcheck-run.sh](../scripts/cppcheck-run.sh)，**warn-only**——pre-push 独立模式粗筛、check.ps1 project 模式精查、**CI `cppcheck-misra` job（`continue-on-error`，非必过，产出报告制品）**;豁免/deviation 在 [.cppcheck-suppressions](../.cppcheck-suppressions) 维护。
**待补齐**：cppcheck/MISRA **已进 CI（非阻断观察，②阶梯）**，待噪声稳后**升必过门**（③，需在 GitHub 分支保护加该 check；见 [process-workflow.md §8](process-workflow.md)）；圈复杂度等度量。

### 4. 测试与覆盖率（CI 必过）
**现状**：
- 单元测试：ztest 3 套（`tests/bms/soc` 21 例、`tests/bms/protection` 6 例、`tests/bms/afe` 20 例），共 **47/47**；平台 `native_sim` + `mps2/an386`。
- 执行：CI 用 `west twister -p native_sim`；本地可用 QEMU（`../run-tests-coverage.ps1`）。
  - ⚠️ QEMU 下 `bms.soc`（21 例）较易触发 harness **超时 flake**（用例本身全过），本地按需加 `--timeout-multiplier 4`；CI 走 native_sim 不受此限。
- 覆盖率门禁：自跑 gcovr（root=workspace、过滤 `app/`），**line ≥ 55% / branch ≥ 30%**（基线 lines 61.0% / branches 39.1%）。
**待补齐**：
- **未被测试的模块**：当前 soc/protection/afe/channels 进入测试构建；**balancing/comm/main 无专门单测**；
- **分支覆盖偏低**（39%），init/线程路径未覆盖；
- 集成/系统验证当前**合并为一个环节**（对齐 [process-workflow.md §8](process-workflow.md) V 腿表），但**尚无专门的多模块集成测试套件**——现仅靠各模块单测 + `native_sim` 整机运行覆盖；专门集成测试待补。无 **HIL（硬件在环）**、无 fuzz/属性测试。

### 5. 构建 / CI（必过门禁）
**现状**：[.github/workflows/ci.yml](../.github/workflows/ci.yml) 6 道门禁 DAG：
`format` → `build (mps2/an386)` + `build (native_sim)` + `test-coverage` + `sca-gcc` + `clang-tidy`。
分支保护要求 6 项全过 + PR + 线性历史 + 禁直推/强推。
**待补齐**：`bms_f405` 板完善后进编译矩阵；真机冒烟（需自托管 runner）。

### 6. 发布 / CD
**现状**：[.github/workflows/release.yml](../.github/workflows/release.yml)：tag `v*` → 校验 tag==[VERSION](../VERSION) → Release 构建 → 固件制品(.elf/.bin/.map) + `SHA256SUMS` + GitHub Release（已发布 v0.1.0、v0.1.1）。
**待补齐**：固件**签名 / 安全启动（MCUboot）**、**SBOM**、制品 **attestation/provenance**、OTA 通道（接真板后）。

### 7. 依赖与供应链
**现状**：west manifest 固定 Zephyr v4.4.0 + HAL 版本；[dependabot.yml](../.github/dependabot.yml) 周更 GitHub Actions。
**待补齐**：pip 依赖（clang-format/gcovr）自动更新；SBOM 生成；依赖漏洞扫描。

### 8. 流程治理
**现状**：PR 分支流 + 6 门禁分支保护；Conventional Commits；[CODEOWNERS](../.github/CODEOWNERS)；[PR 模板](../.github/pull_request_template.md)；[CHANGELOG](../CHANGELOG.md)；单一事实源 [process-workflow.md](process-workflow.md)。
**待补齐**：多人代码评审（单人项目必需 reviewer=0）；接入需求/缺陷跟踪系统（issue 流程）。

## 三、待补齐清单（按优先级）

| 优先级 | 项 | 价值 | 备注 |
|---|---|---|---|
| 高 | 补齐 balancing/comm 单测、提高分支覆盖（afe 已补） | 直接提升固件可靠性 | 随阈值逐步调高 |
| 高 | 需求↔测试追溯 + 验收标准 | 功能安全基础 | BMS 安全相关必备 |
| 中 | cppcheck/MISRA 升必过门（已进 CI 非阻断观察 ②，待升 ③） | 嵌入式编码合规 | 噪声稳后去 `continue-on-error` + 加分支保护 |
| 中 | Doxygen API 文档 + GitHub Pages | 可维护性 | 见 ci-borrow-checklist |
| 中 | bms_f405 进编译矩阵 + 真机 HIL | 接真板必需 | 需自托管 runner + 硬件 |
| 低 | 固件签名 / SBOM / attestation | 供应链与安全启动 | 量产/OTA 阶段 |
| 低 | FMEA / 安全分析（IEC 61508 / ISO 26262） | 功能安全认证 | 视项目目标 |

## 四、功能安全视角（BMS 特别提示）

BMS 属安全相关系统，保护逻辑（过压/过流/过温→接触器）是安全核心。除上述通用管控外，量产路径还应考虑：
- **需求追溯**：每条安全需求 → 设计 → 代码 → 测试用例可追溯；
- **MISRA-C 合规**（强制编码规范）；
- **保护路径的高覆盖率**（含分支/MC-DC，视安全等级）；
- **FMEA / 危害分析** 与对应的诊断/降级策略；
- **固件签名 + 安全启动**，防篡改。

其中**安全纪律**——关联安全需求、测试先行、验证失效安全默认态——按 [process-workflow.md §2](process-workflow.md) **现行生效，不分阶段**；**重型工作产物**（FMEA / 危害分析 / ISO 26262 工作产物全集、保护路径 MC-DC 高覆盖、固件签名+安全启动）则待接真实硬件并明确安全目标后纳入规划。

## 五、对方法论五原则的符合性

> 依据 [concept-methodology.md](concept-methodology.md) §4 的五条原则，评估本项目质量管控的符合度（✅满足 / ⚠️部分 / ❌差距）。本文是该方法论原则2·3·5 的落地与现状映射（见方法论 §6 派生表）。

| 方法论原则 | 结论 | 说明 |
|---|---|---|
| ①软件优先/可仿真 | ✅ 满足 | 多目标编译矩阵(native_sim + mps2/an386)、zbus 解耦、afe 后端可切换 |
| ②测试左移/金字塔 | ⚠️ 部分 | 单元层好(47 例 + 纯函数 + 覆盖率门)；集成层尚无专门套件、balancing/comm/main 无单测、分支覆盖 39% 偏低 |
| ③可追溯性是灵魂 | ⚠️ 推进中 | 新特性经小 V 的 DoD **强制**追溯链(已生效)；历史需求(逆向 231 条)追溯矩阵**增量 backfill**(待补) |
| ④失效安全/红线先行 | ✅ 纪律满足 / ⚠️ 重型项待补 | 默认 OPEN 红线 + 安全改动纪律(development-workflow §2)现行；FMEA/认证类待接真板 |
| ⑤持续合规/自动化为桥 | ✅ 代码维度 / ⚠️ 追溯维度 | CI 6 门 + 分层门禁扎实；追溯维护暂靠 DoD/PR 模板(未自动化)，CI 追溯校验留作后续 |

> 待补齐项详见第三节优先级表；可追溯性的"新工作强制/历史增量"边界以方法论 §4 原则3 立场为准。
