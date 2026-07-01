# TODO

本文件是**未实现项的单一归集处**。每项标注**出处**（可追溯到提出该缺口的文档）、**优先级**、**必要性**。
现状全景见 [docs/quality/management.md](docs/quality/management.md)；流程见 [docs/process/workflow.md](docs/process/workflow.md)。

出处缩写：**QM**=[management.md](docs/quality/management.md)、**WF**=[workflow.md](docs/process/workflow.md)、
**RP**=[quality-system-rollout-plan.md](docs/archive/plans/quality-system-rollout-plan.md)、**CL**=[ci-checklist.md](docs/quality/ci-checklist.md)、
**ARCH**=[architecture.md](docs/concept/architecture.md)、**CLAUDE**=[CLAUDE.md](CLAUDE.md)、
**DOCSYS**=[documentation-system.md](docs/concept/documentation-system.md)、**STD**=[module-interface.md](docs/standard/module-interface.md)、
**DR**=[design-review.md](docs/process/design-review.md)、**INT**=[integration-test-strategy.md](docs/quality/integration-test-strategy.md)。

## 已完成（里程碑）

- [x] **P0-CI**：`format → build → twister` 接入 GitHub Actions。
- [x] **CI 覆盖率**：`test-coverage`(native_sim + gcovr) 在 CI 跑通。
- [x] **gcc SCA 门禁**：`sca-gcc` 按路径只拦 `bms-app/app` 的 `-Wanalyzer`（[scripts/sca-check.sh](scripts/sca-check.sh)）。
- [x] **CD**：`release.yml`（tag `v*` → 固件制品 + SHA256 + Release）；已发 v0.1.0 / v0.1.1。
- [x] **clang-tidy 硬门禁**：`WarningsAsErrors` 开启、0 告警、进必过列。
- [x] **分支保护**：master 要求 PR + CI 必过；仅 squash + 自动删分支；仓库公开。当前门禁见 [docs/quality/gates.md](docs/quality/gates.md)。
- [x] **app 覆盖率门禁**：自跑 gcovr（root=workspace、过滤 `app/`），line ≥ 60% / branch ≥ 38%。
- [x] **分层本地质量网**：pre-push 钩子 + `scripts/check.ps1`（Windows 本地预检）+ cppcheck/MISRA 本地 warn-only；CI 为权威硬门。
- [x] **ztest 测试基础**：当前源码含 AFE 20、COMM 6、PROT 15、SOC 21、集成 4 个 `ZTEST()`；afe 采样后端可切换（stub/sim/adc + validate）。
- [x] **Doxygen API 文档 → GitHub Pages**：[docs/Doxyfile](docs/Doxyfile)（仅公共 API）+ [docs.yml](.github/workflows/docs.yml) 自动构建发布；站点 <https://jianshiyao.github.io/bms-app/>。
- [x] **文档/证据体系骨架**：[docs/concept/documentation-system.md](docs/concept/documentation-system.md) 明确 `concept/process/guide/standard/quality` 五类文档、权威链、小 V 证据包与变更治理。
- [x] **模块接口标准**：[docs/standard/module-interface.md](docs/standard/module-interface.md) 固化 `bms_task/bms_db/bms_diag/bms_bms` 架构下的模块接口、数据 owner、任务 owner 与安全默认态。
- [x] **设计评审门槛**：[docs/process/design-review.md](docs/process/design-review.md) 补齐架构/安全/任务/DB/诊断改动的评审触发条件、checklist 与结论类型。
- [x] **集成测试策略骨架**：[docs/quality/integration-test-strategy.md](docs/quality/integration-test-strategy.md) 明确 DB/DIAG/BMS/TASK 多模块集成验证路线。
- [x] **Engine Core 架构证据链初版**：[docs/work/features/engine-core-architecture/](docs/work/features/engine-core-architecture) 补齐 `bms_task/bms_db/bms_diag/bms_bms` 的 `REQ → DES → TEST` 追溯框架，并同步到全局 [docs/work/traceability.md](docs/work/traceability.md)。
- [x] **首批 Engine Core 集成测试**：[tests/integration/db_diag_bms/](tests/integration/db_diag_bms) 覆盖 DB snapshot、DIAG error blocking、BMS fail-safe OPEN、protection fault → contactor OPEN；`mps2/an386` Twister 构建通过。
- [x] **cppcheck/MISRA CI 硬门**：`cppcheck-misra` 已在 CI 以 `CPPCHECK_FAIL=1` 阻断；本地仍 warn-only 预检。
- [x] **卫生类 CI 门禁**：`editorconfig`、`yamllint`、`test-files`、`file-headers` 已进入 CI。
- [x] **Release SBOM + 签名**：`release.yml` 生成 SPDX SBOM，并用 cosign keyless 签名 `SHA256SUMS`。

## 未实现项（归集 · 按优先级）

| 优先级 | 项 | 出处 | 必要性（为什么需要） |
|---|---|---|---|
| 🔴 高 | **补 `task_pipeline` 集成烟测 + 让 engine 集成测试在可执行平台跑通** | INT / QM §三 | DB→DIAG→BMS 首批集成测试已补并能在 `mps2/an386` 构建；仍需补任务框架调度烟测，并在 CI/WSL `native_sim` 等可执行环境中实际运行 ztest。 |
| 🔴 高 | **补齐 balancing 单测 + comm DB/TX 集成测试**，提高分支覆盖 | QM §三 / INT / CLAUDE 测试节 | balancing 仍缺专门单测；comm 已有周期纯函数测试但缺 DB 快照上报/状态不干扰安全链的集成验证。 |
| 🔴 高 | **按模块填充需求/设计文档 + 建立需求↔测试追溯矩阵** | QM §一·§三 / [docs/templates/](docs/templates) / WF | 模板已就绪、内容待写。BMS 属安全相关系统，需求→设计→代码→测试可追溯是**功能安全基础**与验收依据。 |
| 🟡 中 | **bms_f405 板完善 → 进 CI 编译矩阵 + release 多板** | QM §三 / RP 第5步 / ci.yml 注释 | dts/defconfig 仍为模板。接真实 STM32F405 的必经步骤；完成后 build 矩阵与 release 各加一行。 |
| 🟡 中 | **补配置 / 标定治理标准**（保护阈值、周期、容量、诊断参数） | DOCSYS / methodology §6.5 | 当前 Kconfig 有范围但缺统一参数治理；安全参数需要单位、来源、合法范围、变更审批与测试要求。 |
| 🟡 中 | **圈复杂度 / 代码度量** | QM §一(静态分析待补) | 控制复杂度、辅助重构判断。 |
| 🟢 低 | **固件签名 / 安全启动（MCUboot）** | QM §三·§7 / RP 不在本轮 | 防篡改；量产 / OTA 阶段必需。 |
| 🟢 低 | **Build provenance / attestation** | QM §三·§7 | SBOM 与 SHA256SUMS 签名已落地；仍可补 SLSA provenance。 |
| 🟢 低 | **pip 依赖（clang-format/gcovr）自动更新 + 依赖漏洞扫描** | QM §7 | 当前 dependabot 仅管 GitHub Actions；工具链与 Python 依赖未纳入。 |
| 🟢 低 | **真机 HIL / 冒烟测试** | QM §测试·§构建 / RP 不在本轮 | 需自托管 runner + 硬件；接真板后验证端到端行为。 |
| 🟢 低 | **OTA 升级通道** | QM §7 | 接真板后的现场升级能力。 |
| 🟢 低 | **FMEA / 危害分析（IEC 61508 / ISO 26262）** | QM §三·§四 | 功能安全认证路径；视项目安全目标启动。 |
| 🟢 低 | **多人代码评审**（reviewer ≥ 1、`require_code_owner_reviews`） | QM §三·流程治理 / WF §7 | 单人项目现 reviewer=0，以「PR + CI + 自审」替代；团队化后启用。 |
| 🟢 低 | **需求 / 缺陷跟踪系统（issue 流程）** | QM §流程治理 | 规模化协作的事项管理。 |

## 调研 / 选型

| 项 | 出处 | 必要性 |
|---|---|---|
| **renode 与 QEMU 对比** —— 模拟硬件环境选型与适配 | 本文 | 评估更贴近外设的仿真，为接真板前的 HIL 仿真铺路。 |
| **本地 Windows 覆盖率（WSL2 + native_sim）** | TODO / CLAUDE | QEMU 路线覆盖率不可靠；CI 已满足日常，本地按需用 WSL2。 |
| **clang-tidy 本地对齐（WSL2）** | WF §4 / CLAUDE | Windows 原生对 Zephyr 不可靠（native_sim 不可配 + 版本漂移）；以 CI 为准，要本地跑用 WSL2。 |

> 说明：🔴高=直接影响可靠性/安全基础，应优先；🟡中=工程化与合规增强；🟢低=量产/认证/规模化阶段再做。
> 多数「低」项与「接真实硬件」绑定（见 RP「不在本轮范围」），属阶段性推迟而非遗漏。
