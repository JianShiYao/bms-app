# TODO

本文件是**未实现项的单一归集处**。每项标注**出处**（可追溯到提出该缺口的文档）、**优先级**、**必要性**。
现状全景见 [docs/quality-management.md](docs/quality-management.md)；流程见 [docs/development-workflow.md](docs/development-workflow.md)。

出处缩写：**QM**=[quality-management.md](docs/quality-management.md)、**WF**=[development-workflow.md](docs/development-workflow.md)、
**RP**=[quality-system-rollout-plan.md](docs/quality-system-rollout-plan.md)、**CL**=[ci-borrow-checklist.md](docs/ci-borrow-checklist.md)、
**ARCH**=[architecture.md](docs/architecture.md)、**CLAUDE**=[CLAUDE.md](CLAUDE.md)。

## 已完成（里程碑）

- [x] **P0-CI**：`format → build → twister` 接入 GitHub Actions。
- [x] **CI 覆盖率**：`test-coverage`(native_sim + gcovr) 在 CI 跑通。
- [x] **gcc SCA 门禁**：`sca-gcc` 按路径只拦 `bms-app/app` 的 `-Wanalyzer`（[scripts/sca-check.sh](scripts/sca-check.sh)）。
- [x] **CD**：`release.yml`（tag `v*` → 固件制品 + SHA256 + Release）；已发 v0.1.0 / v0.1.1。
- [x] **clang-tidy 硬门禁**：`WarningsAsErrors` 开启、0 告警、进必过列。
- [x] **分支保护**：master 要求 PR + **6 项** CI 必过；仅 squash + 自动删分支；仓库公开。
- [x] **app 覆盖率门禁**：自跑 gcovr（root=workspace、过滤 `app/`），line ≥ 55% / branch ≥ 30%（基线 61.0% / 39.1%）。
- [x] **分层本地质量网**：pre-push 钩子 + `scripts/check.ps1`（镜像 CI）+ cppcheck/MISRA 本地 warn-only（独立粗筛 / project 精查）。
- [x] **afe/soc 单测**：afe 20 例、soc 21 例、protection 6 例（共 47）；afe 采样后端可切换（stub/sim/adc + validate）。
- [x] **Doxygen API 文档 → GitHub Pages**：[docs/Doxyfile](docs/Doxyfile)（仅公共 API）+ [docs.yml](.github/workflows/docs.yml) 自动构建发布；站点 <https://jianshiyao.github.io/bms-app/>。

## 未实现项（归集 · 按优先级）

| 优先级 | 项 | 出处 | 必要性（为什么需要） |
|---|---|---|---|
| 🔴 高 | **补齐 balancing / comm 单测**，提高分支覆盖 | QM §三 / CLAUDE 测试节 | balancing、comm 目前**零测试**；分支覆盖仅 ~39%。直接关系固件可靠性，且随覆盖率阈值逐步调高的前置。 |
| 🔴 高 | **按模块填充需求/设计文档 + 建立需求↔测试追溯矩阵** | QM §一·§三 / [docs/templates/](docs/templates/) / WF | 模板已就绪、内容待写。BMS 属安全相关系统，需求→设计→代码→测试可追溯是**功能安全基础**与验收依据。 |
| 🟡 中 | **cppcheck / MISRA 升级进 CI** | QM §三 / WF §6 引入阶梯 / CL P3 | 本地已接（warn-only）。按**非阻断→基线→必过**阶梯进 CI，才能成为团队级强约束（嵌入式编码合规）。 |
| 🟡 中 | **bms_f405 板完善 → 进 CI 编译矩阵 + release 多板** | QM §三 / RP 第5步 / ci.yml 注释 | dts/defconfig 仍为模板。接真实 STM32F405 的必经步骤；完成后 build 矩阵与 release 各加一行。 |
| 🟡 中 | **成文设计评审门槛** | QM §一(设计) | 目前设计层无评审流程；变更（尤其保护/阈值）缺第二双眼。 |
| 🟡 中 | **圈复杂度 / 代码度量** | QM §一(静态分析待补) | 控制复杂度、辅助重构判断。 |
| 🟢 低 | **固件签名 / 安全启动（MCUboot）** | QM §三·§7 / RP 不在本轮 | 防篡改；量产 / OTA 阶段必需。 |
| 🟢 低 | **SBOM + 制品 attestation / provenance** | QM §三·§7 | 供应链安全与可追溯；合规交付。 |
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
