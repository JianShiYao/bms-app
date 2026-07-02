# BMS 研发流程 Agent 体系 · 使用指南

本文说明 `.claude/agents/` 下这套覆盖**需求→架构→详细设计→编码→测试→CICD**全流程的
Claude Code subagent 怎么用。流程模型为**敏捷-V 混合**——其方法论依据见根基文档
[methodology.md](../concept/methodology.md);本文是该方法论"小 V 各阶段"的 agent 执行载体。

- 设计依据：[docs/archive/specs/2026-06-19-bms-agile-v-agents-design.md](../archive/specs/2026-06-19-bms-agile-v-agents-design.md)
- 实施计划：[docs/archive/plans/2026-06-19-bms-agile-v-agents.md](../archive/plans/2026-06-19-bms-agile-v-agents.md)
- 端到端样例：[docs/work/features/soc-coulomb/](../work/features/soc-coulomb)（一个完整"小 V"的全部交付物）

---

## 1. Agent 速查

| Agent | 角色 | 输入 | 产出 | 是否能跑命令(Bash) |
|-------|------|------|------|--------|
| `bms-orchestrator` | 迭代编排 | 特性描述 | `00-iteration-plan.md`（派发清单+追溯骨架） | 否 |
| `bms-requirements` | 需求分析 | 迭代计划 | `01-requirements.md`（EARS 需求+验收准则） | 否 |
| `bms-architect` | 架构设计 | 需求 | `02-architecture.md`（ADR、`bms_db` entry/任务模型/失效安全；zbus 过渡） | 否 |
| `bms-designer` | 详细设计 | 架构 | `03-design.md`（签名/状态机/Kconfig/dts） | 否 |
| `bms-coder` | 编码实现 | 详细设计 | 产品代码 + 构建自检 | 是 |
| `bms-tester` | 测试验证 | 需求/设计/代码 | 测试代码 + `05-test-report.md` | 是 |
| `bms-cicd` | CI/CD | 测试与流水线 | `06-cicd.md` + 必要的 workflow 改动 | 是 |

代码评审复用环境内置的 `code-reviewer`，不单独造。

## 2. 小 V 流程

```
① 需求 ───────────────────────验证──▶ ⑤ 测试·系统/集成级（合并）
② 架构 ──────────────────验证──────▶ （集成验证并入系统/集成级）
③ 详细设计 ──────验证────────────────▶ ⑤ 测试·单元级
        └──────▶ ④ 编码实现 ──────────────┘
   贯穿：⑥ CICD 持续验证 · 可追溯链(需求→架构→设计→代码→测试) · 代码评审
```

## 3. 怎么用

### 3.1 前提：先重启 Claude Code 会话
自定义 agent 文件是在某次会话中新建的；**新建后需重启 Claude Code 会话**，它们才会被注册为
可调用的 agent 类型。重启后可用 `@bms-requirements` 之类直接调用，或让主会话用 Agent 工具派发。

### 3.2 编排机制（方案 A）
`bms-orchestrator` **只产出迭代计划与派发清单**，不亲自实现，也**不能**直接调用其它 agent
（subagent 不可嵌套）。真正"按顺序调用 ①~⑥"由**主会话**照计划执行。典型一轮：

1. 把一个特性交给 `bms-orchestrator` → 得到 `docs/work/features/<slug>/00-iteration-plan.md`
2. 主会话照清单**依次**派发 `bms-requirements` → `bms-architect` → `bms-designer`（左腿，串行依赖）
3. 根据风险选择 TDD 握手：
   - 安全/核心逻辑改动：先派 `bms-tester` 写红灯用例（确认能失败）→ 再派 `bms-coder` 实现 → 再派 `bms-tester` 复验并出报告
   - 普通小改动：可派 `bms-coder` 补最小测试并实现 → 再派 `bms-tester` 做最终验证、覆盖率与追溯回填
4. 派发 `bms-cicd`，再过 `code-reviewer` 评审门
5. 核对 `<slug>` 下追溯链无断链 → 提交

### 3.3 主会话阶段门

主会话是方案 A 的"总集成者"：每个 agent 结束后先过对应阶段门，再进入下一阶段；不通过则回到产生该问题的最左侧阶段修正。

| 阶段 | 调用前必须存在 | 调用后必须检查 | 不通过时 |
|------|----------------|----------------|----------|
| `bms-orchestrator` | 特性目标/范围草案 | `00-iteration-plan.md` 与 `traceability.md` 已创建；DoR/DoD、非目标、风险点清楚 | 补清范围或拆小特性 |
| `bms-requirements` | 迭代计划 | 每条需求有 `REQ-<域>-NNN`、EARS 句式、验收准则、验证方法；安全需求已标注 | 回需求阶段重写，不进入架构 |
| `bms-architect` | `01-requirements.md` | 每条架构决策标注服务的 REQ；模块/`bms_db` entry/任务与优先级（阻塞边界）与失效安全影响明确（对齐 concept 契约；zbus 过渡） | 回架构阶段，必要时同步修需求 |
| `bms-designer` | `02-architecture.md` | 每条需求至少有 `DES-<域>-NNN` 覆盖；函数契约、状态机、Kconfig/dts、纯逻辑测试目标明确；`traceability.md` 设计列已回填 | 回设计阶段，接口不稳不得编码 |
| `bms-tester` 红灯阶段 | `01-requirements.md` + `03-design.md` | 安全/核心逻辑的关键用例已写且可证明在实现前失败 | 用例不完整则补测试；需求含混则回需求 |
| `bms-coder` | `03-design.md` + 必要红灯用例 | 实现不超设计范围；代码能回到 REQ/DES；构建通过 | 修实现；若必须改接口，回设计再基线 |
| `bms-tester` 复验阶段 | 代码 + 测试 | 测试通过、覆盖率达标、`05-test-report.md` 完成；`traceability.md` 测试列/状态列无空链 | 缺陷回 coder；漏测回 tester；需求变化按 §3.4 |
| `bms-cicd` / `code-reviewer` | 测试报告与 diff | CI 不降级；workflow 覆盖新测试；评审无阻断问题 | 修 CI/代码/测试后重跑相关门 |

### 3.4 变更再基线

只要迭代中途或跨迭代改动了既有需求、安全项、接口、阈值或验收准则，就必须同步传播到右腿验证；否则按"追溯断链"处理。

1. 改 `REQ-*` 或验收准则 → 回到 `bms-requirements`，同步 `01-requirements.md` 与 `traceability.md`
2. 需求影响模块边界、`bms_db` entry、任务/优先级（阻塞边界）、安全路径 → 重新派 `bms-architect`
3. 架构或接口变化 → 重新派 `bms-designer`，更新 `DES-*`、函数契约、Kconfig/dts 与纯逻辑测试目标
4. 设计变化 → 重新派 `bms-tester` 补/改红灯用例，再派 `bms-coder` 实现
5. 实现后 → `bms-tester` 重跑受影响用例并更新 `05-test-report.md`，注明"再基线范围"与重跑结果

安全相关变更（protection / 阈值 / 接触器 / 采样）不能只改代码；若没有对应安全需求，先补需求再实现。

### 3.5 评审门（code-reviewer）

`code-reviewer` 不单独建 agent，但作为小 V 完成前的固定评审门。主会话在 `bms-tester` 复验与
`bms-cicd` 检查后调用它，重点审以下内容：

- diff 是否夹带非目标范围，尤其是 `app/`、`tests/`、`.github/workflows/` 外的意外改动
- 实现是否偏离 `03-design.md` 的函数契约、状态机、Kconfig/dts 设计
- 每条新增/变更需求是否能在 `traceability.md` 中找到设计与验证证据
- 安全相关路径是否满足默认安全态、测试先行、显式失效安全用例
- 是否降低 CI、覆盖率、SCA、clang-tidy、cppcheck/MISRA 的既有门槛

发现阻断问题时，不直接"评审通过后补"；应回到对应阶段修正，并重跑受影响测试/门禁。

### 3.6 并行与 worktree 约束

默认在单一工作树中串行推进一个小 V。确需并行推进多个不相关特性时，遵循
[workflow.md §3.1](workflow.md)：

- 一任务 = 一 worktree = 一分支 = 一 PR，最终只通过 `master` 汇合
- 禁止两个会话/agent 在同一工作树同时改同一批文件
- `coder` 与 `tester` 若要并行，只能在各自隔离 worktree 中工作；主会话负责合并与冲突裁决
- 并行分支合并前，必须重新核对 `traceability.md`、测试报告与 CI 门，避免一边的变更使另一边断链

常用开/收命令见 [workflow.md §3.1](workflow.md)；不要为普通单特性工作强行开 worktree。

### 3.7 什么时候不用完整小 V

完整小 V 是新功能、安全相关改动、架构/接口变更的默认路径。其他任务可按下表裁剪，但不得削弱追溯与安全纪律。

| 任务类型 | 推荐流程 | 不可省略 |
|----------|----------|----------|
| 新功能或新模块 | 完整小 V：orchestrator → requirements → architect → designer → tester/coder → tester → cicd/review | 全部过程交付物、追溯矩阵、测试报告 |
| 安全相关 bugfix（protection/阈值/接触器/采样） | 从既有/新增 `REQ-*` 开始，按 §3.4 做再基线；先红灯用例再实现 | 安全需求、失效安全用例、受影响验证重跑 |
| 普通 bugfix | 定位既有 `REQ-*`/`DES-*`，必要时只更新受影响的需求/设计/测试 | 修复用例、追溯状态、回归测试 |
| 纯 docs | 可不走完整小 V；按 docs 分支/PR 走评审 | 不得改动流程根文档后忘记同步下游 |
| 纯 CI/工具链 | 可直接派 `bms-cicd`，必要时补 `06-cicd.md` | 不降低既有门槛；workflow 校验 |
| 历史追溯 backfill | 更新 requirements/traceability，不夹带产品代码 | ID 规范、映射说明、状态标记 |
| 探索/调研 spike | 走探索轨，产出结论后再决定是否进入交付轨小 V | 不把探索代码直接当作完成实现合入 |

### 3.8 交付物布局
```
docs/work/features/<feature-slug>/
├─ 00-iteration-plan.md   # orchestrator：计划+派发清单
├─ 01-requirements.md     # requirements
├─ 02-architecture.md     # architect
├─ 03-design.md           # designer
├─ 05-test-report.md      # tester
├─ 06-cicd.md             # cicd
└─ traceability.md        # 独立追溯矩阵（orchestrator 初始化，各阶段回填）
```
产品代码与测试仍写入既有 `app/`、`tests/`；`docs/work/features/<slug>/` 只放过程交付物。

## 4. 工具链速记（以 Windows venv 为准）

本项目实际构建/测试链路是 **Windows 侧 venv**，不是 WSL：

在 workspace 根 `bms-workspace/` 执行：

```powershell
# 构建（QEMU/ARM 目标，默认板）
.venv\Scripts\python.exe -m west build -b mps2/an386 bms-app\app -p always
# 测试（自动设好 QEMU_BIN_PATH）
powershell -ExecutionPolicy Bypass -File run-tests-coverage.ps1 -Board mps2/an386
# 开 PR 前本地镜像 CI 全门
powershell -ExecutionPolicy Bypass -File bms-app\scripts\check.ps1        # 或 -Fast
```

在仓库根 `bms-app/` 执行：

```powershell
# 构建（QEMU/ARM 目标，默认板）
..\.venv\Scripts\python.exe -m west build -b mps2/an386 app -p always
# 测试（自动设好 QEMU_BIN_PATH）
powershell -ExecutionPolicy Bypass -File ..\run-tests-coverage.ps1 -Board mps2/an386
# 开 PR 前本地镜像 CI 全门
powershell -ExecutionPolicy Bypass -File scripts\check.ps1        # 或 -Fast
```

WSL + `native_sim` 仅用于更可靠的覆盖率（CI 即走 native_sim）。

## 5. 与现有项目规范的关系（重要）

项目已有一套需求工程脚手架，**这套 agent 应当对齐它**：

- **ID 规范**（见 [docs/templates/README.md](../templates/README.md)）：需求 `REQ-<域>-<NNN>`、设计 `DES-<域>-<NNN>`；
  域 = `SYS/AFE/SOC/PROT/BAL/COMM/BOARD`。例：`REQ-SOC-001`、`DES-SOC-002`。
- **模板**：[requirements-template.md](../templates/requirements-template.md) /
  [design-spec-template.md](../templates/design-spec-template.md) /
  [traceability-matrix-template.md](../templates/traceability-matrix-template.md)。
- **追溯**：每条需求可追溯到一个验证手段；安全相关需求优先自动化测试；ztest 用
  `/* Verifies REQ-XXX-NNN: ... */` 注释标注。
- **分支/PR**（见 [workflow.md](workflow.md)）：从最新 **master** 切
  `feat/<kebab>` 分支，PR `--base master`，仅 Squash 合并，master 受 6 道 CI 门保护。

> ⚠️ 已知偏差（仅限历史样例 `soc-coulomb`，规则面向后续新特性）：该样例用了 `REQ-SOC-Cxx`、
> 把追溯表塞进 `00-iteration-plan.md`、从 `ci/local-quality-layering` 切分支。这三点的**明文规则**现已固化在
> [workflow.md §1.1–1.2、§3](workflow.md)（ID 用 `REQ-<域>-NNN`、追溯矩阵独立成 `traceability.md`、
> 特性分支从 `master` 切）——后续 agent 的 CKB/契约以该处为准；历史样例不回改。
>
> **ID 规范化进展（2026-06-24）**：`REQ-SOC-Cxx` 已规范为 **`REQ-SOC-025..036`**（接续遗留 `soc.md` 001-024，`C0x→0(x+24)`）。
> **活代码** `tests/bms/application/soc/` 已用规范 ID，权威矩阵见 [docs/work/traceability.md](../work/traceability.md)；`docs/work/features/soc-coulomb/` 过程文档因
> REQ/ADR/裸码交织且属历史记录，**保留原始 `Cxx`**（不回改，映射见 traceability.md）。protection/afe 测试的 REQ 注释为独立后续项。

## 6. 扩展点（YAGNI 之外）
- 将来真机目标增多 → 把"集成测试 / 系统测试"从合并状态拆开。
- 想要"一条命令自动跑完整个小 V" → 把编排升级为 slash 命令/skill（方案 B）。
