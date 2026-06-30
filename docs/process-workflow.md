# 开发流程与质量审查

本文是 bms-app 的**开发流程单一事实源**,覆盖**完整研发生命周期**——既包括「如何开发一个特性」(敏捷-V 小 V:需求→架构→设计→编码→测试),
也包括「如何协作交付」(分支→提交→PR→CI→发版)。环境安装/构建/运行/测试的命令见 [README](../README.md)。

> 小 V 的**执行细节**(怎么调 agent 链、各阶段产出模板)见 [process-agents.md](process-agents.md) 与 [templates/](templates/);
> 本文只定义**流程骨架与质量门**,不复制那些细节。

---

## 1. 特性开发生命周期(敏捷-V 小 V)

本节是**敏捷+V 方法论在本项目的操作落地**;方法论的"为什么/是什么"(模型、五条原则、可追溯性原理、DoR/DoD 的理由)见根基文档 [concept-methodology.md](concept-methodology.md),此处不再复述。下图为小 V 速查:

```
① 需求 ───────────────────────验证──▶ ⑤ 测试·系统/集成级（合并）
② 架构 ──────────────────验证──────▶ （集成验证并入系统/集成级）
③ 详细设计 ──────验证────────────────▶ ⑤ 测试·单元级
        └──────▶ ④ 编码实现 ──────────────┘
   贯穿：⑥ CICD 持续验证 · 可追溯链(需求→架构→设计→代码→测试) · 代码评审
```

- **方法论依据**：[concept-methodology.md](concept-methodology.md)（敏捷+V 研发方法论根基；本节为其在本项目的操作落地）
- **agent 体系设计**：[superpowers/specs/2026-06-19-bms-agile-v-agents-design.md](superpowers/specs/2026-06-19-bms-agile-v-agents-design.md)
- **怎么调 agent 链**：[process-agents.md](process-agents.md)(orchestrator → requirements → architect → designer → coder/tester → cicd)
- **端到端样例**：[features/soc-coulomb/](features/soc-coulomb/)

### 1.1 可追溯性(本项目落地)

> 为什么可追溯性是 V 模型的灵魂,见 [concept-methodology.md §4 原则3](concept-methodology.md)。本小节只给本项目的 ID 链与格式规则:

```
REQ-<域>-NNN  →  DES-<域>-NNN  →  代码位置(file:行/函数)  →  ztest 用例
```

- **ID 规范**:需求 `REQ-<域>-NNN`、设计 `DES-<域>-NNN`,域 = `SYS/AFE/SOC/PROT/BAL/COMM/BOARD`(例 `REQ-PROT-001`、`DES-SOC-002`)。
  详见 [templates/README.md](templates/README.md)。
- **ztest 注释回链**:用例顶部标 `/* Verifies REQ-<域>-NNN: ... */`,把测试和需求显式绑定。
- **追溯矩阵独立成文**:每个特性维护 `docs/features/<slug>/traceability.md`(用
  [templates/traceability-matrix-template.md](templates/traceability-matrix-template.md)),**不要**塞进 `00-iteration-plan.md`。
- **原则**:每条需求至少有一个验证手段(测试/分析/检视/演示);**安全相关需求优先自动化测试**。

### 1.2 命名与文件约定(明文规则)

- 需求/设计 ID 一律用 **`REQ-<域>-NNN` / `DES-<域>-NNN`** 形式;**不使用** `REQ-SOC-Cxx` 之类的临时式编号。
- 特性过程交付物落在 `docs/features/<slug>/`:`00-iteration-plan.md`(计划+派发清单)、`01-requirements.md`、
  `02-architecture.md`、`03-design.md`、`05-test-report.md`、`06-cicd.md`、`traceability.md`(独立追溯矩阵)。
- 产品代码与测试仍写入既有 `app/`、`tests/`;`docs/features/<slug>/` 只放过程交付物。

### 1.3 迭代准入 / 准出(DoR / DoD)

> DoR/DoD 的设立理由见 [concept-methodology.md §5](concept-methodology.md)。下为本项目落地清单:

每个特性默认继承以下通用判据(单特性可在其 `00-iteration-plan.md` 细化,但不得削弱)。

**准入(DoR,进入迭代前满足)**
- 依赖就绪(所需 zbus 通道/数据结构/配置项已存在或本迭代显式纳入);
- 基线可构建可测(`mps2/an386` 构建 + `run-tests-coverage.ps1` 当前通过);
- 迭代计划(`00-iteration-plan.md`)已评审,**非目标范围已界定**。

**准出(DoD,迭代完成判据)**
- 小 V 各阶段产出齐备(需求/架构/设计/代码/测试/cicd 交付物);
- **追溯链无断链**:每条 `REQ-<域>-NNN` 贯通 需求→架构→设计→代码→测试,`traceability.md` 无空链;
- **变更已再基线**:本迭代若改动既有需求/安全项,其追溯链与受影响的右腿验证已同步更新(否则视为断链;依据 [concept-methodology.md §4 原则6](concept-methodology.md));
- 所有 ⚠️ **失效安全**项均有对应需求与测试用例并通过;
- 构建 + `twister` + 覆盖率达门限,无回归;**CI 6 门全绿**;
- 范围受控:非目标未被夹带实现。

### 1.4 小 V 的分支 / PR 粒度

- **一个特性 = 一条 `feat/<kebab>` 分支 = 一个 Squash PR**(分支/提交/PR 规则见 §3–§7)。
- `docs/features/<slug>/*` 过程交付物与 `app/`、`tests/` 代码**在同一个 PR** 提交——过程文档随实现一起评审、一起进 `master`。

## 2. 安全相关改动的额外要求

protection / 阈值 / 接触器 / 采样等**安全关键**改动,除常规流程外须额外满足(对齐项目失效安全红线:
默认接触器 **OPEN**,仅判定 NORMAL 才 CLOSED;另见 [quality-management.md 第四节](quality-management.md)):

- **必关联安全需求**:改动须对应一条 `REQ-PROT-*`(等)安全需求;若无,先补需求再改。
- **测试先行(TDD)**:先写覆盖该安全场景的 ztest,再实现;失效安全分支(默认 OPEN、仅 NORMAL 才 CLOSE)**必须**有显式用例。
- **显式验证失效安全默认态**,并在 PR 的「风险与回滚」中说明影响面与回滚办法。
- **加强自审**:单人项目无第二 reviewer,安全改动尤须对照需求逐条核 diff(必要时请 `code-reviewer` 评审)。

## 3. 分支模型

- `master`：唯一受保护主干，始终可构建、CI 全绿。**不可直接 push**（受分支保护）。
- 工作分支：`<type>/<kebab-描述>`，type 对齐提交规范：
  `feat/ fix/ docs/ ci/ style/ chore/ refactor/ test/ build/ perf/`
  例：`feat/soc-coulomb-counting`、`fix/protection-uv-threshold`、`ci/add-release-workflow`。
- 特性分支一律从**最新 `master`** 切出(勿从其他工作分支派生)。

### 3.1 并行工作:worktree 隔离(默认约定)

> 方法论依据:[concept-methodology.md §4 原则5](concept-methodology.md)(持续合规)——**用隔离让冲突无从发生;实在要交汇,交给 git 在合并点显式裁决(可控集成)**。

**约定:同时推进多个不相关任务时,默认用 git worktree 物理隔离——「一任务 = 一 worktree = 一分支 = 一 PR」,`master` 为唯一汇入点。**

- **默认单工作树串行**即可;**仅在真有并行需求时**才开 worktree(避免过度工程)。
- **禁止**两个 session/agent 指向**同一工作树**改同一批文件——这是丢更新与状态错乱的根源。
- 开/收:

  ```powershell
  git worktree add ..\bms-app-<topic> -b <type>/<topic>   # 从 master 切隔离工作区 + 分支
  # …在该目录开 session 工作 → 本地自检(§6) → PR(§7) → Squash 合 master…
  git worktree remove ..\bms-app-<topic>                   # 合并后清理
  ```

- Claude Code 并行子任务用 `isolation: "worktree"` 的 subagent(详见 superpowers `using-git-worktrees`)。
- 冲突处理:隔离后**文件级冲突不会发生**;唯一交汇点是分支→`master` 合并,由 git 显式处理(冲突可见、可审查),与 §7 / §9 一致。

## 4. 提交信息规范（Conventional Commits）

```
<type>(<scope>): <祈使句摘要>

<可选正文：为什么这么改>
<footer：BREAKING CHANGE: ... / Refs #n>
```
- `type`：`feat fix docs style refactor test chore ci build perf`
- `scope`：模块名 —— `soc protection afe balancing comm board ci docs`
- 版本影响：`feat`→次版本，`fix`→修订，`BREAKING CHANGE`→主版本（0.x 见 §10）。

## 5. 克隆后一次性设置

```powershell
git config core.hooksPath scripts/hooks   # 一次启用 pre-commit(格式) + pre-push(推前自检)
```
详见 [README 第二节](../README.md#二代码格式化与提交检查)。

## 6. 提交前自检（开 PR 前跑 check.ps1）

开 PR 前在**已激活 venv 的 PowerShell** 里本地复现 CI 全套门禁，避免 push 后才在 CI 发现问题：
```powershell
powershell -ExecutionPolicy Bypass -File scripts\check.ps1          # 全量：format/build×2/twister/sca/clang-tidy
powershell -ExecutionPolicy Bypass -File scripts\check.ps1 -Fast    # 快跑：仅 format/build×2/twister(跳过较慢的 sca/clang-tidy)
```
- 缺工具（如未装 clang-tidy）的门会标 `SKIP`（不计失败，CI 会补跑）；任一 `FAIL` 退出码非 0。
- SCA / clang-tidy 做 `-p always` 干净构建，全量约数分钟；日常迭代可先 `-Fast`，开 PR 前再跑一次全量。
- **clang-tidy 与覆盖率同属"Linux 可靠"项**：clang-tidy 的 parity 需 `native_sim`（提供 host flags），而 native_sim 在 Windows **配置失败**，且本地新版本 clang-tidy 与 CI 不一致——故 clang-tidy 以 **CI(Linux) 为准**，Windows 上 check.ps1 标 `SKIP`；要本地对齐用 **WSL2**（Zephyr 即装在 WSL）。
- **cppcheck** 则不受此限：check.ps1 用 **mps2/an386 的 `compile_commands.json`**（Windows 可编）走 project 精查；覆盖率本地走 `..\run-tests-coverage.ps1`（QEMU 路线覆盖率不可靠，可靠覆盖率见 CI / WSL2+native_sim）。
- **QEMU 测试超时 flake**：本地用 QEMU 跑 twister 时，`bms.soc`（21 例）较易触发 harness 超时（用例本身全过），按需加 `--timeout-multiplier 4`；CI 走 native_sim 不受此限。

## 7. PR 流程

```
git switch -c feat/xxx          # 从最新 master 切分支
# ... 编码 + 本地自检 ...
git push -u origin feat/xxx
gh pr create --base master      # 开 PR（填模板）
# CI 自动跑 → 5 个必过检查全绿 → Squash 合并 → 自动删分支
```
- 合并策略：**仅 Squash**（master 线性、每 PR 一条规范提交）。
- master 受保护：CI 必过才能合并；禁直推、禁强推、要求线性历史。
- **小 V 即一个 PR**：一个特性的全部小 V 交付物(`docs/features/<slug>/*`)+ `app/`/`tests/` 改动同 PR 提交;
  合并前须满足 §1.3 的 **DoD**(尤其追溯链无断链);PR 模板的「追溯链无断链」「安全相关改动」勾选项即此门的检查点。

## 8. 分层质量关（按触发时机递进）

**设计原则：越靠前的关越要快，越靠后的关越全。** 检查按成本放到合适的触发点，
而不是一股脑塞进 `pre-commit`（那样会慢到被 `--no-verify` 绕过，门形同虚设）。

| 关 | 时机 | 工具 | 速度 | 阻断点 |
|---|---|---|---|---|
| ① 编辑时 | 写码/保存 | `.clang-format` + `.editorconfig`（编辑器即时，editorconfig 依赖插件） | 实时 | 软约束（自动套用） |
| ② 提交前 | `git commit` | `scripts/hooks/pre-commit`（暂存 .c/.h 格式校验） | 秒级 | 本地拒绝提交 |
| ③ 推送前 | `git push` | `scripts/hooks/pre-push`（push 范围 format + 机会性增量 clang-tidy + **cppcheck/MISRA 告警**） | 秒级~十几秒 | format/tidy 拒推；cppcheck/MISRA 仅告警 |
| ④ 开 PR 前 | 手动 | `scripts/check.ps1`（本地全量镜像 CI） | 数分钟 | 自检（暴露 CI 必失项） |
| ⑤ CI / PR | push / PR | `.github/workflows/ci.yml`（6 门，权威） | — | 分支保护拦合并 |
| ⑥ 发布 | 打 tag | `.github/workflows/release.yml`（tag `v*`） | — | 发布失败即无 Release |
| ⑦ 审计 | 持续 | `dependabot` / `CODEOWNERS` / `CHANGELOG` / PR 模板 | — | 软约束 |

**上表是「静态/集成检查」视角;从 V 模型「右腿验证」视角,各层对应小 V 的验证阶段:**

| 小 V 右腿 | 对应左腿 | 落地手段 | 在上表/CI 的位置 |
|---|---|---|---|
| ⑤ 测试·单元级 | ③ 详细设计 | Twister ztest(纯逻辑函数,`/* Verifies REQ-... */` 回链) | CI 门 `test-coverage`(native_sim) |
| ⑤ 测试·系统/集成级 | ① 需求 | native_sim 多模块 + 验收准则回归(当前集成/系统合并) | CI 构建 + 覆盖率;DoD 准出核验 |
| 失效安全需求 | ① 需求(⚠️ 项) | **优先自动化测试**;结构性约束(线程优先级/不阻塞)由评审+集成确认 | §2 安全路径 + DoD |

> 即:§8 的「关」回答「改动够不够干净/能不能合」;小 V 右腿回答「需求/设计有没有被验证到」。两者经 **CI test-coverage 门**与 **DoD 追溯门**交汇。

**为什么重检查（clang-tidy / SCA / cppcheck/MISRA）不放 `pre-commit`：**
它们依赖完整构建生成的 `compile_commands.json`，首轮要编一遍 Zephyr（数十秒~分钟），
每次 commit 都做不现实。因此：

- `pre-commit`（每次提交）只做**秒级**的 clang-format —— 保证提交粒度的快。
- `pre-push`（每次推送一次）做**便宜网**：对本次 push 的改动做 format 校验（挡 `--no-verify` 漏过的）、
  在本地已有 `build/compile_commands.json` 时**复用它**增量跑 clang-tidy（绝不从零构建）、
  并用 [`scripts/cppcheck-run.sh`](../scripts/cppcheck-run.sh) 对改动的 `.c` 跑 **cppcheck + MISRA**
  （**独立粗筛**：无需构建、秒级；但无 Zephyr 头/宏上下文，会有已知假阳性如 MISRA 17.3/17.7，故 warn-only）。
- `check.ps1`（开 PR 前手动）才做**全量镜像**：两板构建 + twister + SCA + clang-tidy + cppcheck/MISRA，对齐 CI。
  其中 cppcheck/MISRA 走 **project 模式**（`cppcheck --project build/check-tidy/compile_commands.json`，复用 clang-tidy
  那步的编译库），有真实 `-I/-D` 上下文，**假阳性基本消除**（实测 17→0）。

> **cppcheck/MISRA 双层**：pre-push 用独立模式图快（容忍假阳性、只告警）；check.ps1/CI 用 project 模式图准。
> MISRA addon（`misra.py`，GPLv3）不入库，按机器跑 [`scripts/setup-cppcheck-misra.sh`](../scripts/setup-cppcheck-misra.sh)
> 下载到 gitignore 的 `scripts/.cppcheck-addons/`；噪声与 deviation 在 [`.cppcheck-suppressions`](../.cppcheck-suppressions) 集中维护。

**新静态分析工具（cppcheck / MISRA）的引入按「阻断强度」递进，而非按位置：**
直接把还很吵的工具设成 CI 必过门，会让每个 PR 都被误报卡死、且在 CI 上来回调参极慢。正确阶梯：

| 阶段 | 在哪 | 阻断强度 | 目的 | 本项目现状 |
|---|---|---|---|---|
| ① 试跑调参 | 本地（反馈最快） | 不阻断 | 看噪声量、选规则子集、建 suppression/基线 | — |
| ② 观察期 | pre-push（本地） / CI | **非阻断（告警）** | 跑若干轮，确认基线稳 | **当前在此**：pre-push + check.ps1（本地）+ CI `cppcheck-misra` job（`continue-on-error`，非必过）均 warn-only |
| ③ 升门禁 | CI | **必过**（加入 required checks） | 噪声归零后才阻断合并 | 待噪声调稳后做 |
| ④ 收紧本地 | pre-push | 本地拦截（`CPPCHECK_FAIL=1`） | 低噪后给开发者硬反馈 | 一个开关即可启用 |

> 说明：②"非阻断观察"同时在**本地（pre-push / check.ps1 的 warn-only）**与 **CI（`cppcheck-misra` job，`continue-on-error`、不计入必过门、产出报告制品）**进行——
> 本地反馈快、调 suppression 不必来回 push;CI 则给每个 PR 一份跨平台(Linux)的稳定基线观察。[`.cppcheck-suppressions`](../.cppcheck-suppressions)
> 集中维护豁免与 MISRA deviation；噪声调稳后,去掉 `continue-on-error` 并把该 check 加入分支保护(③ 升必过),pre-push 设 `CPPCHECK_FAIL=1`(④ 收紧本地)。

要点：**调参在本地做**（CI 来回 push 太慢）；**进 CI 第一步必须非阻断**；用 **baseline/suppression 只对新增代码报错**，不必先清零历史问题即可上线。

CI（⑤）当前 6 道**必过**门禁：`format` → `build (mps2/an386)` + `build (native_sim)` + `test-coverage`(native_sim 覆盖率) + `sca-gcc`(gcc 静态分析) + `clang-tidy`(CERT/可读性)。
另有 `editorconfig`、`yamllint` 两道**阻断作业**(卫生门:尾随空格/末行换行/LF/charset/YAML lint;缩进交由 clang-format 与编辑器,见 `.ecrc`/`.editorconfig`),已去 `continue-on-error`,**待本 PR 合并后加入分支保护必过列 → 8 道**。
另有 1 个**非阻断观察** job `cppcheck-misra`(②阶梯,`continue-on-error`,不计入必过门、产出报告制品),待噪声稳后升必过。
各阶段质量管控现状与待补齐的全景见 [quality-management.md](quality-management.md)；SCA/clang-tidy/覆盖率的路线图见 [quality-ci-checklist.md](quality-ci-checklist.md)。

## 9. 分支保护说明

master 受保护，必过检查（6 项）：`format`、`build (mps2/an386)`、`build (native_sim)`、`test-coverage`、`sca-gcc`、`clang-tidy`。
- **待加入（本 PR 合并后执行）**：`editorconfig`、`yamllint`（已是阻断作业，加入后共 8 项）。命令：
  `gh api -X POST repos/JianShiYao/bms-app/branches/master/protection/required_status_checks/contexts -f 'contexts[]=editorconfig' -f 'contexts[]=yamllint'`。
  **须合并后执行**——否则未含这两作业的在途 PR 会因必过检查永不上报而被卡。
- `clang-tidy` 已**硬门禁**（`.clang-tidy` 开启 `WarningsAsErrors`，当前 0 告警）并加入必过列。
- **单人项目说明**：必需 reviewer = 0（无法要求他人评审），用「PR + CI 必过 + 自审 diff」替代第二双眼；
  团队化后改 reviewer ≥ 1、启用 `require_code_owner_reviews`、考虑 `enforce_admins`。

## 10. 发布流程（CD）

`VERSION` 文件 + git tag + `release.yml` 联动：
1. 开 `chore(release): bump vX.Y.Z` 分支；
2. 改 [VERSION](../VERSION)（`PATCHLEVEL` 或 `VERSION_MINOR` +1），把 `CHANGELOG.md` 的 `[Unreleased]` 落为 `## [X.Y.Z] - 日期`；
3. PR → CI 过 → squash 合并；
4. master 上 `git tag -a vX.Y.Z -m "release vX.Y.Z"` → `git push origin vX.Y.Z`；
5. tag push 触发 `release.yml`：校验 tag == VERSION → Release 构建 → 发布固件制品 + `SHA256SUMS`。

**SemVer（0.x 固件约定）**：`0.MINOR.PATCH` —— MINOR 进位 = 新功能或可能破坏性变更，PATCH = 修复。
上真实 STM32F405 板量产稳定后再升 `1.0.0`。
