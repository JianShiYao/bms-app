# 质量管控全景（需求 → 编码 → 测试 → CI/CD）

> **定位**：本文仅为质量全景**总览**。门禁与阈值的事实以 [gates.md](gates.md) 为准，**不在此复制**。

本文梳理 bms-app 从需求到交付各阶段**已落地的质量管控**与**待补齐项**，便于评估当前成熟度与规划下一步。
相关文档：开发流程见 [workflow.md](../process/workflow.md)；当前门禁事实表见 [gates.md](gates.md)；CI 对照清单见 [ci-checklist.md](ci-checklist.md)；待办见 [../TODO.md](../../TODO.md)。

> 当前状态（2026-07-01）：QEMU/native_sim 阶段骨架 + 完整 CI/CD 质量门禁；cppcheck/MISRA、editorconfig、yamllint、测试存在性、文件头已进入 CI 阻断门。
> 当前源码含 5 个 ztest 套件、66 个 `ZTEST()` 用例；尚未接真实硬件（STM32F405）。

## 一、全景总览

| 阶段 | 已落地管控 | 阻断强度 | 主要待补齐 |
|---|---|---|---|
| 需求 | 设计/规格文档（`docs/`） | 软 | 需求管理、验收标准、需求↔测试追溯 |
| 设计 | foxBMS 2 inspired 架构文档 + 安全概念 + 模块接口标准 + 设计评审门 | 软 | engine 模块 REQ/DES/TEST 链、参数/标定治理、FMEA/安全分析 |
| 编码 | clang-format（pre-commit/CI 强制）、.editorconfig、文件头检查、命名规范、**app `-Werror`**（编译告警即错，限 app 目标） | clang-format=提交前拦截；`-Werror`/editorconfig/file-headers=CI 构建拦截 | — |
| 静态分析 | gcc `-fanalyzer`、clang-tidy(CERT/可读性，硬)、cppcheck+MISRA(CI 硬门，本地 warn-only 预检) | CI 必过 | 圈复杂度等度量 |
| 测试 | ztest 单测/集成测试（当前源码 66 个 `ZTEST()`）、twister、app 覆盖率门禁(line>=60% / branch>=38%)、测试存在性门 | CI 必过 | HIL 测试、balancing 单测、comm DB/TX 集成、engine 执行型验证 |
| 构建 | 多目标编译矩阵(mps2/an386 + native_sim + qmxx_f407zg)、app `-Werror`、CI 工具版本已 pin | CI 必过 | bms_f405 板、Release 多板 |
| 发布(CD) | tag→固件制品+SHA256+SPDX SBOM+cosign keyless 签名+Release、tag↔VERSION 校验 | 失败即无 Release | Release 多板、build provenance、MCUboot 安全启动 |
| 依赖/供应链 | west manifest pin、dependabot(actions)、Release SBOM/签名 | 软/发布阻断 | pip 依赖、依赖漏洞扫描、build provenance |
| 流程治理 | PR 流 + CI 必过门禁、Conventional Commits、CODEOWNERS、CHANGELOG | 合并拦截 | 多人评审(单人=0)、需求/缺陷跟踪系统 |

> 流程门补充：特性开发的迭代准入/准出(DoR/DoD)、安全改动路径、追溯 DoD 门见 [workflow.md §1.3 / §2 / §7](../process/workflow.md)；PR 模板含「追溯链无断链」「安全相关改动」勾选项。本文对应的方法论依据见 [methodology.md](../concept/methodology.md)。

## 二、分阶段详述

### 1. 需求与设计
**现状**：目标架构见 [architecture.md](../concept/architecture.md)（foxBMS 2 inspired：`bms_task` / `bms_db` / `bms_diag` / `bms_bms`，Zephyr 原生落地）；安全概念见 [safety.md](../concept/safety.md)；文档/证据体系见 [documentation-system.md](../concept/documentation-system.md)。模块化由 Kconfig 开关表达（`CONFIG_BMS_*`）。**已提供需求工程模板** [templates/](../templates)（需求规格 + 设计规格 + 需求↔测试追溯，EARS 句式、ID 规范、与 ztest 关联约定）。模块接口标准见 [module-interface.md](../standard/module-interface.md)，设计评审门见 [design-review.md](../process/design-review.md)。
**待补齐**：
- 按模块**填充**需求/设计文档并建立追溯矩阵（模板已就绪，内容待写）；
- 为 `bms_task` / `bms_db` / `bms_diag` / `bms_bms` 补齐 `REQ/DES/TEST` 链；
- 参数/标定治理标准（保护阈值、周期、容量、诊断参数）待补。

### 2. 编码（提交前）
**现状**（提交前即拦截）。规则文件本身不"触发"检查，触发靠读取它的工具，分三层：
- 格式：
  - [.clang-format](../../.clang-format)（Zephyr 风格 tab/8）——**权威**。触发链：保存时由 C/C++ 扩展套用 → 提交时 pre-commit 用 `clang-format --dry-run --Werror` 硬拦截 → CI 设 format gate 兜底。
  - [.editorconfig](../../.editorconfig)（缩进/charset/去尾空格/结尾换行；C 段已对齐 tab/8 以避免与 clang-format 冲突）——**仅编辑器层，依赖插件**：需安装 `EditorConfig.EditorConfig`（已列入 [.vscode/extensions.json](../../../.vscode/extensions.json) 推荐）才生效，**不装则该文件不起作用**；它无独立提交门禁，非 C 文件（yaml/json/ps1/md）的排版仅靠它。
  - 行尾 LF 由 [.gitattributes](../../.gitattributes) 在提交时强制（与是否装插件无关）。
- 命名/规范：[.clang-tidy](../../.clang-tidy)（C/snake_case，cert-*/readability-*）。
- 本地门禁：[scripts/hooks/pre-commit](../../scripts/hooks/pre-commit)（需 `git config core.hooksPath scripts/hooks` 注册一次）校验暂存 `.c/.h` 格式；`scripts/format.ps1` 一键格式化/检查。
**待补齐**：无（编码层管控已较完整）。

### 3. 静态分析（SCA）
**现状**：
- `sca-gcc`：`-DZEPHYR_SCA_VARIANT=gcc`（`-fanalyzer`），经 [scripts/sca-check.sh](../../scripts/sca-check.sh) 只拦 `app/` 的 `-Wanalyzer` 告警。**CI 必过**。
- `clang-tidy`：硬门禁（`WarningsAsErrors`，当前 0 告警），CERT + 可读性 + snake_case 命名。**CI 必过**。
- `cppcheck + MISRA`（misra addon）：[scripts/cppcheck-run.sh](../../scripts/cppcheck-run.sh)，CI `cppcheck-misra` job 以 `CPPCHECK_FAIL=1` 阻断合并；pre-push / `check.ps1` 仍为本地 warn-only 预检，便于快速调 suppression/deviation。
**待补齐**：圈复杂度等度量。

### 4. 测试与覆盖率（CI 必过）
**现状**：
- 测试：当前源码含 5 个 ztest 套件（AFE 20、COMM 6、PROT 15、SOC 21、集成 4），共 66 个 `ZTEST()`；实际执行数量以 Twister/CI 报告为准。
- 执行：CI 用 `west twister -p native_sim`；本地可用 QEMU（`../run-tests-coverage.ps1`）。
  - ⚠️ QEMU 下 `bms.soc`（21 例）较易触发 harness **超时 flake**（用例本身全过），本地按需加 `--timeout-multiplier 4`；CI 走 native_sim 不受此限。
- 覆盖率门禁：自跑 gcovr（root=workspace、过滤 `app/`），**line >= 60% / branch >= 38%**。
**待补齐**：
- **未被充分测试的模块**：comm 已有周期纯函数测试，但仍缺 DB/TX 集成验证；**balancing/main/engine 执行路径仍需补测试**；
- **分支覆盖偏低**（39%），init/线程路径未覆盖；
- 集成/系统验证当前**合并为一个环节**（对齐 [workflow.md §8](../process/workflow.md) V 腿表）；多模块集成测试路线已在 [integration-test-strategy.md](integration-test-strategy.md) 成文，首批 `DB→DIAG→BMS→contactor` 套件已补到 `tests/integration/db_diag_bms/`，当前本地 `mps2/an386` 为构建验证，执行型验证待 CI/WSL `native_sim`。无 **HIL（硬件在环）**、无 fuzz/属性测试。

### 5. 构建 / CI（必过门禁）
**现状**：[.github/workflows/ci.yml](../../.github/workflows/ci.yml) 的阻断门见 [gates.md](gates.md)。分支保护要求 CI 必过 + PR + 线性历史 + 禁直推/强推。
**待补齐**：`bms_f405` 板完善后进编译矩阵；真机冒烟（需自托管 runner）。

### 6. 发布 / CD
**现状**：[.github/workflows/release.yml](../../.github/workflows/release.yml)：tag `v*` → 校验 tag==[VERSION](../../VERSION) → Release 构建 → 固件制品(.elf/.bin/.map) + `SHA256SUMS` + GitHub Release（已发布 v0.1.0、v0.1.1）。
**待补齐**：Release 多板、build provenance、设备端安全启动（MCUboot）、OTA 通道（接真板后）。

### 7. 依赖与供应链
**现状**：west manifest 固定 Zephyr v4.4.0 + HAL 版本；[dependabot.yml](../../.github/dependabot.yml) 周更 GitHub Actions。
**待补齐**：pip 依赖（clang-format/gcovr）自动更新；依赖漏洞扫描；build provenance。

### 8. 流程治理
**现状**：PR 分支流 + CI 必过门禁；Conventional Commits；[CODEOWNERS](../../.github/CODEOWNERS)；[PR 模板](../../.github/pull_request_template.md)；[CHANGELOG](../../CHANGELOG.md)；流程骨架见 [workflow.md](../process/workflow.md)，门禁事实见 [gates.md](gates.md)。
**待补齐**：多人代码评审（单人项目必需 reviewer=0）；接入需求/缺陷跟踪系统（issue 流程）。

## 三、待补齐清单（按优先级）

| 优先级 | 项 | 价值 | 备注 |
|---|---|---|---|
| 高 | 补齐 balancing 单测 + comm DB/TX 集成测试，提高分支覆盖 | 直接提升固件可靠性 | 随阈值逐步调高 |
| 高 | 需求↔测试追溯 + 验收标准 | 功能安全基础 | Engine Core 初版已补；其余模块继续 backfill |
| 高 | `task_pipeline` 集成烟测 + engine 集成测试执行型验证 | 新架构关键交互验证 | DB→DIAG→BMS 已构建通过，仍需可执行平台运行 |
| 中 | 圈复杂度 / 代码度量 | 控制复杂度、辅助重构判断 | 可后续引入 lizard 或 clang-tidy 度量 |
| 中 | 参数/标定治理标准 | 安全参数可控 | 阈值、周期、容量、诊断参数 |
| 中 | bms_f405 进编译矩阵 + 真机 HIL | 接真板必需 | 需自托管 runner + 硬件 |
| 低 | build provenance / MCUboot 安全启动 | 供应链与设备端安全启动 | Release SBOM 与校验和签名已落地，设备端启动验签留待真板阶段 |
| 低 | FMEA / 安全分析（IEC 61508 / ISO 26262） | 功能安全认证 | 视项目目标 |

## 四、功能安全视角（BMS 特别提示）

BMS 属安全相关系统，保护逻辑（过压/过流/过温→接触器）是安全核心。除上述通用管控外，量产路径还应考虑：
- **需求追溯**：每条安全需求 → 设计 → 代码 → 测试用例可追溯；
- **MISRA-C 合规**（强制编码规范）；
- **保护路径的高覆盖率**（含分支/MC-DC，视安全等级）；
- **FMEA / 危害分析** 与对应的诊断/降级策略；
- **固件签名 + 安全启动**，防篡改。

其中**安全纪律**——关联安全需求、测试先行、验证失效安全默认态——按 [workflow.md §2](../process/workflow.md) **现行生效，不分阶段**；**重型工作产物**（FMEA / 危害分析 / ISO 26262 工作产物全集、保护路径 MC-DC 高覆盖、固件签名+安全启动）则待接真实硬件并明确安全目标后纳入规划。

## 五、对方法论五原则的符合性

> 依据 [methodology.md](../concept/methodology.md) §4 的五条原则，评估本项目质量管控的符合度（✅满足 / ⚠️部分 / ❌差距）。本文是该方法论原则2·3·5 的落地与现状映射（见方法论 §6 派生表）。

| 方法论原则 | 结论 | 说明 |
|---|---|---|
| ①软件优先/可仿真 | ✅ 满足 | 多目标编译矩阵(native_sim + mps2/an386)、zbus 解耦、afe 后端可切换 |
| ②测试左移/金字塔 | ⚠️ 部分 | 单元层较好；已有首批集成套件，但 balancing、comm DB/TX、task pipeline 仍需补齐 |
| ③可追溯性是灵魂 | ⚠️ 推进中 | 新特性经小 V 的 DoD **强制**追溯链(已生效)；历史需求(逆向 231 条)追溯矩阵**增量 backfill**(待补) |
| ④失效安全/红线先行 | ✅ 纪律满足 / ⚠️ 重型项待补 | 默认 OPEN 红线 + 安全改动纪律(development-workflow §2)现行；FMEA/认证类待接真板 |
| ⑤持续合规/自动化为桥 | ✅ 代码维度 / ⚠️ 追溯维度 | CI 门禁扎实；追溯维护暂靠 DoD/PR 模板(未自动化)，CI 追溯校验留作后续 |

> 待补齐项详见第三节优先级表；可追溯性的"新工作强制/历史增量"边界以方法论 §4 原则3 立场为准。
