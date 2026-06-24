# 敏捷+V 研发方法论(根基文档)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增 `docs/development-methodology.md`(通用敏捷+V 方法论母文档),作为开发工作流/agent/模板的唯一依据,并把下游文档收敛为"由它衍生"。

**Architecture:** 母文档承载方法论概念(通用 + BMS 注脚);development-workflow §1 瘦身为"指针 + 本项目落地规则";agents-guide / templates/README / CLAUDE.md 各加一句指向母文档为依据,形成派生闭环。

**Tech Stack:** 纯 Markdown 文档(中文);相对链接相对各文件所在目录;无代码/构建。

**依据 spec:** `docs/superpowers/specs/2026-06-24-agile-v-development-methodology-design.md`

> **提交约定**:依项目流程(master 受保护、从最新 master 切分支、Squash PR)。执行前先建分支 `docs/agile-v-methodology`;各任务 commit,最后开 PR。**仅在用户许可后执行 git 操作。**

---

## Task 0: 准备分支

**Files:** 无(仅 git)

- [ ] **Step 1: 从最新 master 切分支**

Run:
```bash
git switch master && git pull --ff-only && git switch -c docs/agile-v-methodology
```
Expected: 切到新分支 `docs/agile-v-methodology`。

---

## Task 1: 创建母文档 `docs/development-methodology.md`

**Files:**
- Create: `docs/development-methodology.md`

- [ ] **Step 1: 写入母文档完整内容**

将以下内容**原样**写入 `docs/development-methodology.md`:

````markdown
# 敏捷+V 研发方法论

本文是本项目研发方法论的**根基(母文档)**:一份**通用的敏捷+V 研发方法论**,BMS 项目相关的具体落地以「BMS 注脚」形式给出。

> **定位**:开发工作流、agent 体系、需求/追溯模板、CI 门禁等一切下游流程,**均以本方法论为依据并由其衍生**(派生关系见 §6)。
> **读者**:开发者、参与研发的 agent、新人——用于**统一对方法论的认知**。
> **边界**:本文只讲"为什么 / 是什么";**具体操作**(门禁命令、分支/PR、agent 调用)见各下游文档,本文不复制。

---

## 1. 目的与适用范围

- **是什么**:把"为什么用敏捷+V、小 V 是什么、哪些原则不可违背"集中成唯一出处,作为下游流程的共同依据。
- **解决什么**:避免方法论散落各文档导致理解不一、维护脱节;让下游有可引用的单一依据。
- **怎么用**:做任何研发决策(定流程、设门禁、写 agent、建模板)时,应能回指本文的某条模型或原则;若回指不到,要么决策有问题,要么本文需补充。

派生关系总览见 §6。

## 2. 为什么是敏捷+V(统一认知的核心)

软件工程的成熟实践(敏捷、CI/CD)多诞生于互联网领域,其隐含前提——运行环境随处可得、部署近零成本、错误可快速回滚——在**安全相关 + 长周期 + 资源/实时约束**的嵌入式系统里大多不成立。于是两种纯粹方法各有死角:

- **纯敏捷的死角**:轻文档、轻追溯、拥抱变化。快,但**无法举证"每条(安全)需求都被设计覆盖、被测试验证"**——撞上功能安全的墙。
- **纯 V 模型的死角**:需求前置、逐级验证、文档与追溯完备。严谨可举证,但大批量、阶段门、**人工**验证与追溯使其**慢、不适应迭代**。

**调和洞察(本方法论的支点)**:敏捷与 V 并非二选一,而是分层共存——
> **敏捷管节奏,V 管完备性与可追溯;而 V 模型的"慢"主要慢在人工(人工写验证、人工维护追溯)。一旦把验证与追溯自动化,V 的完备性就能以敏捷的节奏被持续满足——自动化是让两者共存的桥。**

这条洞察贯穿后面的模型(§3)、原则(§4 尤其原则5)与门(§5)。

> **BMS 注脚**:本项目是安全相关系统(过压/过流/过温→接触器),V 的"可追溯 + 完备验证"不可省;同时以迭代方式快速演进骨架,需要敏捷的快。两者靠 CI 与分层门禁的自动化共存。

## 3. 核心模型:敏捷外壳 + 小 V 内核

**迭代层(敏捷外壳)**:以 backlog 驱动,每个迭代交付一个**特性增量**。

**每个特性走一个「小 V」**(左腿分解·设计,逐层对应右腿验证·集成):

```
左腿（分解·设计）                                右腿（验证·集成）
① 需求 ─────────────────────────验证──────▶ ⑥ 系统/集成验证
② 架构 ────────────────────验证──────────▶ （集成验证并入⑥）
③ 详细设计 ──────────验证──────────────────▶ ④ 单元测试
            └──────────▶ 编码实现 ──────────────┘
   贯穿：可追溯链(需求→架构→设计→代码→测试) · 代码评审 · CI 持续验证
```

- **左右腿逐层对应**:右腿每一层验证回溯到左腿同层的需求/设计——这是"完备性可举证"的结构来源。
- **编码在谷底**:设计向下收敛到实现,再由测试向上逐层验证。
- **双轨(可选实践)**:高不确定的工作(架构选型、算法探索、危险分析)走**探索轨**,想清楚后再喂给**交付轨**走严格小 V;不要硬塞进固定迭代假装有进度。

> **BMS 注脚**:一个"特性"通常是一个模块增量(如"补全 SOC 库仑计数""接触器 GPIO 驱动")。当前仅单一 `native_sim`/QEMU 目标且模块化,**集成测试与系统测试合并**为一个验证环节(YAGNI);将来真机目标增多再拆。

## 4. 五条指导原则(不可违背)

每条先给通用陈述,再附 BMS 注脚。下游的任何流程/门禁/agent 职责,都应能回指其中一条或多条。

1. **软件优先 / 可仿真** — 软件独立于硬件演进;仿真目标是一等公民,使大部分开发与测试脱离硬件。
   > BMS:`native_sim`/QEMU(`mps2/an386`)为主力验证目标,真机 `bms_f405` 只是众多目标之一;zbus 解耦五模块,逻辑不绑死硬件。

2. **测试左移 / 测试金字塔** — 逻辑与硬件解耦;能在低层(单元/PC)拦截的缺陷绝不上推;上硬件只验证真正硬件相关项。
   > BMS:核心逻辑放纯函数(`bms_protection_evaluate`、`bms_soc_coulomb_step` 等)+ 薄线程包装,Twister ztest 直测纯函数。

3. **可追溯性是灵魂(V 模型核心)** — 每条需求贯通 `需求→架构→设计→代码→测试`,右腿验证可回溯左腿;**无追溯即无完备性证据**;安全相关需求优先自动化测试。
   > BMS:`REQ-<域>-NNN → DES-<域>-NNN → 代码位置 → ztest 用例`;域 = SYS/AFE/SOC/PROT/BAL/COMM/BOARD;ztest 用 `/* Verifies REQ-<域>-NNN: ... */` 回链。
   >
   > **立场(原则即刻生效,不悬空)**:本原则对**新特性即刻强制**——小 V 的 DoD 要求追溯链无断链,无追溯不得合并。历史代码(逆向得到的既有需求)的追溯矩阵作为**独立的增量 backfill** 推进,不因历史欠账而削弱对新工作的强制。

4. **失效安全 / 安全红线先行** — 安全相关改动必须关联安全需求、默认安全态、测试先行、显式验证。
   > BMS:接触器**默认 OPEN**,仅判定 NORMAL 才 CLOSED;保护线程最高优先级;SOC 等信息流不得参与保护决策。
   >
   > **立场(纪律不分阶段)**:安全**纪律**——关联安全需求、测试先行、验证失效安全默认态(见 [development-workflow.md §2](development-workflow.md))——**现行生效,不分项目阶段**。仅 FMEA / 危害分析 / ISO 26262 工作产物全集等**重型**项可随"接真板、明确安全目标"后置。不给安全纪律留"待真板再说"的口子。

5. **持续合规(自动化是桥)** — 验证与追溯靠自动化**持续**满足,而非阶段末人工补;门要前移、高频、自动。这是 §2 调和洞察的落地。
   > BMS:CI 6 门(format / build×2 / test-coverage / sca-gcc / clang-tidy)+ pre-commit/pre-push 分层门禁 + DoD 追溯门。

## 5. 迭代生命周期与门

- **为什么要 DoR/DoD**:迭代是流动的,需要明确"什么时候可以进、什么时候算完",否则容易把没想清楚的工作塞进迭代、或在追溯/验证不全时就宣称完成。
  - **准入(DoR)原则**:依赖就绪、基线可构建可测、范围(含非目标)已界定。
  - **准出(DoD)原则**:小 V 各阶段产出齐备;**追溯链无断链**;安全相关项有对应测试并通过;自动化门全绿;范围受控。
- **门即持续合规**:把 V 的"阶段门"从"开会才过"变成"每次提交都过"——前移、高频、自动(原则5)。
- **具体门禁、命令、DoR/DoD 的本项目落地清单**见 [development-workflow.md](development-workflow.md),本文不复制。

## 6. 派生关系:本方法论如何落地

每个下游产物都"依据本方法论的某一部分"。这张表是本文作为"依据"的支点:

| 下游产物 | 依据本方法论的 | 承载(落地了什么) |
|---|---|---|
| [development-workflow.md](development-workflow.md) | §3 模型 / §4 原则3·4·5 / §5 DoR-DoD | 分支/提交/PR、分层质量门、DoR/DoD 本项目清单、安全改动路径、发布 |
| [agents-guide.md](agents-guide.md) + `.claude/agents/*` | §3 小 V 各阶段 / §4 全部 | 把小 V 各阶段实现为 subagent 与编排 |
| [templates/](templates/) | §4 原则3(可追溯性) | REQ/DES ID 规范、追溯矩阵、EARS 模板 |
| CI(`.github/workflows/ci.yml`) | §4 原则2·5 | 自动化验证门 |
| [superpowers/specs/2026-06-19-bms-agile-v-agents-design.md](superpowers/specs/2026-06-19-bms-agile-v-agents-design.md) | §3 模型 / §4 | agent 体系的设计取舍 |
| [quality-management.md](quality-management.md) | §4 原则2·3·5 | 各阶段质量管控现状与缺口、对五原则的符合性 |

## 7. 术语表

| 术语 | 含义 |
|---|---|
| 小 V | 单个特性走的微缩 V:左腿设计逐层对应右腿验证 |
| 左腿 / 右腿 | 左腿=需求/架构/详细设计(分解);右腿=系统-集成/单元测试(验证) |
| DoR / DoD | Definition of Ready / Done——迭代准入 / 准出判据 |
| 可追溯链 | `需求→架构→设计→代码→测试` 的 ID 贯通链 |
| 失效安全 | 故障时回到安全默认态(BMS:接触器 OPEN) |
| 持续合规 | 验证与追溯靠自动化持续满足,而非阶段末人工补 |
| 测试金字塔 | 单元多而快、集成次之、系统/硬件少而精的测试结构 |
| 双轨 | 探索性工作与交付性工作分轨推进 |
````

- [ ] **Step 2: 验证文件结构自检(无需命令)**

人工核对母文档满足:
- 能独立回答"为什么敏捷+V / 小 V 是什么 / 五条原则 / 谁从它衍生";
- 七节齐全(§1–§7);
- 每条原则都有"通用陈述 + BMS 注脚"两部分。
Expected: 全部满足。

- [ ] **Step 3: 验证"抽掉 BMS 注脚后仍成立"**

通读时在脑中删去所有 `> BMS:` 注脚与 §3 末 BMS 注脚段:正文仍是一份可读的通用敏捷+V 方法论。
Expected: 成立(注脚是增量,不是主干依赖)。

- [ ] **Step 4: Commit**

```bash
git add docs/development-methodology.md
git commit -m "docs(methodology): add agile+V development methodology as the root doc"
```

---

## Task 2: development-workflow.md §1 瘦身为指针 + 本项目落地规则

**Files:**
- Modify: `docs/development-workflow.md`(§1 区块,以及文件顶部"设计依据"类链接)

**背景:** 当前 §1 含小 V 模型完整阐述、可追溯性原理、DoR/DoD。按决策 A:方法论原理上移母文档;§1 只留**本项目落地规则**(分支/PR 粒度、ztest 注释格式、`docs/features/<slug>/` 文件约定、ID 形式)+ DoR/DoD 的**本项目具体清单**(清单留下,"为什么"归母文档)。

- [ ] **Step 1: 改文件顶部小 V"设计依据"指向**

将顶部引用块里指向 `superpowers/specs/2026-06-19-...` 作为"设计依据"的那句,改为指向母文档:

把(现状,§1 引用块内):
```markdown
- **设计依据**：[superpowers/specs/2026-06-19-bms-agile-v-agents-design.md](superpowers/specs/2026-06-19-bms-agile-v-agents-design.md)
```
改为:
```markdown
- **方法论依据**：[development-methodology.md](development-methodology.md)（敏捷+V 研发方法论根基；本节为其在本项目的操作落地）
- **agent 体系设计**：[superpowers/specs/2026-06-19-bms-agile-v-agents-design.md](superpowers/specs/2026-06-19-bms-agile-v-agents-design.md)
```

- [ ] **Step 2: §1 开头加一句"依据"并精简模型阐述**

在 `## 1. 特性开发生命周期(敏捷-V 小 V)` 标题下、小 V 图之前,把"研发流程模型为敏捷-V 混合……"那段说明,改为一句指针 + 保留图作为速查:

把(现状):
```markdown
研发流程模型为**敏捷-V 混合**:以 backlog 驱动的**迭代节奏**(敏捷)推进,每个特性走一个**「小 V」**(左腿设计 ↔ 右腿验证,逐层对应)。
```
改为:
```markdown
本节是**敏捷+V 方法论在本项目的操作落地**;方法论的"为什么/是什么"(模型、五条原则、可追溯性原理、DoR/DoD 的理由)见根基文档 [development-methodology.md](development-methodology.md),此处不再复述。下图为小 V 速查:
```
(小 V 图保留不动。)

- [ ] **Step 3: §1.1 可追溯性——删原理、留本项目规则**

`### 1.1 可追溯性` 小节:删去"V 模型的灵魂"等**原理性**表述(已在母文档 §4 原则3),保留**本项目具体规则**。

把该小节首句(现状):
```markdown
### 1.1 可追溯性(V 模型的灵魂)

每个特性保留一条完整 ID 链,右腿测试可回溯到左腿对应层的需求/设计:
```
改为:
```markdown
### 1.1 可追溯性(本项目落地)

> 为什么可追溯性是 V 模型的灵魂,见 [development-methodology.md §4 原则3](development-methodology.md)。本小节只给本项目的 ID 链与格式规则:
```
(其下 ID 链代码块、ID 规范、ztest 注释、traceability.md、原则三条 bullet 全部保留不动。)

- [ ] **Step 4: §1.3 DoR/DoD——加一句指针,保留本项目清单**

`### 1.3 迭代准入 / 准出(DoR / DoD)` 小节:在小节标题下、"每个特性默认继承……"之前,加一句指针;**清单本体保留**(决策:清单留 §1,"为什么"归母文档)。

在(现状):
```markdown
### 1.3 迭代准入 / 准出(DoR / DoD)

每个特性默认继承以下通用判据(单特性可在其 `00-iteration-plan.md` 细化,但不得削弱)。
```
之间插入一句,变为:
```markdown
### 1.3 迭代准入 / 准出(DoR / DoD)

> DoR/DoD 的设立理由见 [development-methodology.md §5](development-methodology.md)。下为本项目落地清单:

每个特性默认继承以下通用判据(单特性可在其 `00-iteration-plan.md` 细化,但不得削弱)。
```
(准入/准出清单保留不动。)

- [ ] **Step 5: 验证 §1 无方法论原理重复 + 链接有效**

人工核对:§1 不再完整阐述"为什么敏捷+V/小 V 完整模型/可追溯性原理/DoR-DoD 理由"(只剩本项目规则 + 指针);新增的 `development-methodology.md` 相对链接(同在 docs/,直接文件名)可点开。
Expected: 满足,无与母文档重复的原理段落。

- [ ] **Step 6: Commit**

```bash
git add docs/development-workflow.md
git commit -m "docs(workflow): slim §1 to project landing + point to methodology root"
```

---

## Task 3: agents-guide.md 指向母文档为依据

**Files:**
- Modify: `docs/agents-guide.md`(开头说明 + §1 上方的引用列表)

- [ ] **Step 1: 开头"流程模型"一句加方法论依据**

把(现状,第 3–4 行附近):
```markdown
本文说明 `.claude/agents/` 下这套覆盖**需求→架构→详细设计→编码→测试→CICD**全流程的
Claude Code subagent 怎么用。流程模型为**敏捷-V 混合**（迭代节奏 + 每个特性走 V 模型的设计↔验证）。
```
改为:
```markdown
本文说明 `.claude/agents/` 下这套覆盖**需求→架构→详细设计→编码→测试→CICD**全流程的
Claude Code subagent 怎么用。流程模型为**敏捷-V 混合**——其方法论依据见根基文档
[development-methodology.md](development-methodology.md);本文是该方法论"小 V 各阶段"的 agent 执行载体。
```

- [ ] **Step 2: Commit**

```bash
git add docs/agents-guide.md
git commit -m "docs(agents): point agents-guide to methodology root as rationale"
```

---

## Task 4: templates/README.md 标注派生自原则3

**Files:**
- Modify: `docs/templates/README.md`

- [ ] **Step 1: 在 README 顶部说明处加一句依据**

在 `docs/templates/README.md` 开头概述段落后,加一行(若已有类似句则合并,不重复):
```markdown
> 本套 ID / 追溯规范是研发方法论**原则3(可追溯性)**的落地;方法论根基见 [../development-methodology.md](../development-methodology.md)。
```
(注意相对路径:templates/ 在 docs/ 下,故用 `../development-methodology.md`。)

- [ ] **Step 2: Commit**

```bash
git add docs/templates/README.md
git commit -m "docs(templates): note ID/traceability derives from methodology principle 3"
```

---

## Task 5: CLAUDE.md「开发流程与提交规范」节加依据指针

**Files:**
- Modify: `CLAUDE.md`(`## 开发流程与提交规范` 节)

- [ ] **Step 1: 在该节"权威文档"句旁加方法论依据**

把(现状):
```markdown
## 开发流程与提交规范

权威文档：`docs/development-workflow.md`（务必先读）。
```
改为:
```markdown
## 开发流程与提交规范

方法论根基：`docs/development-methodology.md`（敏捷+V 研发方法论,一切流程由其衍生）。
操作权威：`docs/development-workflow.md`（务必先读）。
```

- [ ] **Step 2: 验证 CLAUDE.md 链接/路径正确**

人工核对:`docs/development-methodology.md` 路径相对仓库根正确(CLAUDE.md 在 `bms-app/` 根,文档在 `bms-app/docs/`)。
Expected: 正确。

- [ ] **Step 3: Commit**

```bash
git add CLAUDE.md
git commit -m "docs(claude): reference methodology root as workflow rationale"
```

---

## Task 6: quality-management 对齐方法论

**Files:**
- Modify: `docs/quality-management.md`(加「五原则符合性」节、改 §四末句、修 §二.4 不一致、补交叉引用)

**背景:** quality-management 写于方法论+工作流重构前,需对齐:加符合性映射并声明依据、把安全纪律从"待真板"上移为现行、修「无集成测试」与 development-workflow §8 的口径冲突。

- [ ] **Step 1: 新增「五、对方法论五原则的符合性」节**

在现有「## 四、功能安全视角(BMS 特别提示)」节**之后**,追加新节:

````markdown
## 五、对方法论五原则的符合性

> 依据 [development-methodology.md](development-methodology.md) §4 的五条原则,评估本项目质量管控的符合度(✅满足 / ⚠️部分 / ❌差距)。本文是该方法论原则2·3·5 的落地与现状映射(见方法论 §6 派生表)。

| 方法论原则 | 结论 | 说明 |
|---|---|---|
| ①软件优先/可仿真 | ✅ 满足 | 多目标编译矩阵(native_sim + mps2/an386)、zbus 解耦、afe 后端可切换 |
| ②测试左移/金字塔 | ⚠️ 部分 | 单元层好(47 例 + 纯函数 + 覆盖率门);集成层尚无专门套件、balancing/comm/main 无单测、分支覆盖 39% 偏低 |
| ③可追溯性是灵魂 | ⚠️ 推进中 | 新特性经小 V 的 DoD **强制**追溯链(已生效);历史需求(逆向 231 条)追溯矩阵**增量 backfill**(待补) |
| ④失效安全/红线先行 | ✅ 纪律满足 / ⚠️ 重型项待补 | 默认 OPEN 红线 + 安全改动纪律(development-workflow §2)现行;FMEA/认证类待接真板 |
| ⑤持续合规/自动化为桥 | ✅ 代码维度 / ⚠️ 追溯维度 | CI 6 门 + 分层门禁扎实;追溯维护暂靠 DoD/PR 模板(未自动化),CI 追溯校验留作后续 |

> 待补齐项详见第三节优先级表;可追溯性的"新工作强制/历史增量"边界以方法论 §4 原则3 立场为准。
````

- [ ] **Step 2: 改 §四末句——安全纪律上移为现行**

把「四、功能安全视角」节末句:
```
这些超出当前 QEMU 骨架阶段，待接真实硬件并明确安全目标后纳入规划。
```
替换为:
```
其中**安全纪律**——关联安全需求、测试先行、验证失效安全默认态——按 [development-workflow.md §2](development-workflow.md) **现行生效,不分阶段**;**重型工作产物**(FMEA / 危害分析 / ISO 26262 工作产物全集、保护路径 MC-DC 高覆盖、固件签名+安全启动)则待接真实硬件并明确安全目标后纳入规划。
```

- [ ] **Step 3: 修「无集成测试」与 §8 的口径不一致**

把「二、4. 测试与覆盖率」节"待补齐"中的:
```
- 无**集成测试**、无**HIL（硬件在环）**、无 fuzz/属性测试。
```
替换为:
```
- 集成/系统验证当前**合并为一个环节**(对齐 [development-workflow.md §8](development-workflow.md) V 腿表),但**尚无专门的多模块集成测试套件**——现仅靠各模块单测 + `native_sim` 整机运行覆盖;专门集成测试待补。无 **HIL(硬件在环)**、无 fuzz/属性测试。
```

- [ ] **Step 4: 补 DoR/DoD、§2、追溯门交叉引用**

在「一、全景总览」表后插入一句:
```markdown
> 流程门补充:特性开发的迭代准入/准出(DoR/DoD)、安全改动路径、追溯 DoD 门见 [development-workflow.md §1.3 / §2 / §7](development-workflow.md);PR 模板含「追溯链无断链」「安全相关改动」勾选项。
```

- [ ] **Step 5: 校验改动一致**

Run: `grep -n "development-methodology.md\|尚无专门的多模块集成测试\|现行生效" docs/quality-management.md`
Expected: 新节引用 `development-methodology.md`;不再有孤立的"无集成测试"绝对表述;§四末句出现"现行生效"。

- [ ] **Step 6: Commit**

```bash
git add docs/quality-management.md
git commit -m "docs(quality): align with methodology (principle conformance, safety timing, integration-test wording)"
```

---

## Task 7: 全局验证(派生闭环 + 链接 + 无重复)

**Files:** 无(只读核对)

- [ ] **Step 1: 派生闭环核对**

母文档 §6 表中每个下游(development-workflow / agents-guide / templates / quality-management / 2026-06-19 spec),逐一确认其文中已有指回母文档的"依据"链接(本计划 Task 2–6 已加 development-workflow / agents-guide / templates / CLAUDE.md / quality-management;2026-06-19 spec 为被引用方,无需反向链接)。
Expected: 无孤链。

- [ ] **Step 2: 链接有效性**

核对所有新增相对链接目标存在:
- 母文档内:`development-workflow.md`、`agents-guide.md`、`templates/`、`superpowers/specs/2026-06-19-bms-agile-v-agents-design.md`、`quality-management.md`(均相对 docs/);
- development-workflow §1 内:`development-methodology.md`;
- templates/README.md 内:`../development-methodology.md`。
Expected: 全部可点开。

- [ ] **Step 3: 无重复/无矛盾**

确认方法论概念(小 V 完整模型、可追溯性原理、DoR/DoD 理由)此后**只在母文档**有完整表述;development-workflow §1 仅剩本项目规则 + 指针。
Expected: 无重复。

- [ ] **Step 4: 开 PR**

```bash
git push -u origin docs/agile-v-methodology
gh pr create --base master --title "docs: 敏捷+V 研发方法论根基文档" --fill
```
Expected: PR 创建,CI 6 门触发(文档改动应全绿)。

---

## Self-Review(本计划对 spec 的覆盖)

> 依据 spec `2026-06-24-agile-v-development-methodology-design.md`(§1 目的 / §2 决策 / §3 母文档结构含五原则 / §4 配套回改 / §5 范围 / §6 验证)。

- spec §3 母文档七节 → Task 1 全文覆盖 ✓
- spec §3 §4 原则3/原则4 的两条"立场"(可追溯性新工作强制·历史增量;安全纪律不分阶段)→ Task 1 原则3/4 已注入,且 Task 6 quality-management 用同口径 ✓
- spec §4 配套回改(development-workflow §1 / agents-guide / templates/README / CLAUDE.md / **quality-management**)→ Task 2/3/4/5/**6** ✓
- spec §3 派生关系表 → Task 1 §6 ✓
- spec §6 验证项(含 quality-management 对齐验收)→ Task 1 Step2–3 + Task 6 Step5 + Task 7 ✓
- 占位符扫描:无 TBD/TODO;母文档与 quality-management 新节全文已嵌入,非占位 ✓
- 一致性:ID 形式 `REQ-<域>-NNN`、域集合、ztest 注释格式在母文档与 §1 一致;母文档"立场"与 quality-management 结论口径一致(新工作强制/历史增量;纪律现行/重型后置)✓
