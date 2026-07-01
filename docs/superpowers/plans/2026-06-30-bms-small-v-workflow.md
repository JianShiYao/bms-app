# BMS 小 V 自动编排工作流 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建命名工作流 `bms-small-v`：一次调用自动跑完一个 BMS 特性的敏捷-V 小 V（编排→需求→架构→设计→TDD→CICD→评审→追溯校验），含分层锚定真值的阶段门与 tester↔coder 重试循环。

**Architecture:** 一个 Workflow JS 脚本（一等编排者，绕开 subagent 不能嵌套）。各阶段用 `agent()` 派发子代理，子代理 Read `.claude/agents/bms-<role>.md` 并扮演该角色，经 `docs/features/<slug>/` 文件交接。阶段门以"门即数据"声明：能跑的阶段让 agent 跑 `west`/twister 返回真值 verdict，文档阶段用独立验证者 agent 返回 `{passed,gaps}`；JS 据 verdict 做 pass/重试分支。**不碰 git**。

**Tech Stack:** Claude Code Workflow 工具（JS，沙箱无 fs/Node API）、现有 7 个 `bms-*` subagent、Windows venv（`..\.venv\Scripts\python.exe -m west`）、Twister/gcovr。

**参考规格：** [docs/superpowers/specs/2026-06-30-bms-small-v-workflow-design.md](../specs/2026-06-30-bms-small-v-workflow-design.md)

---

## 介质约束与验证模型（必读）

- Workflow JS **无 fs / 无 Node API**；脚本不能直接读写文件或跑命令。所有文件检查 / 构建 / 测试都由
  `agent()` 子代理执行，并通过 **schema 结构化返回**把结果回传给脚本；脚本只对返回的布尔/数组做判定与重试。
- `meta` 必须是**纯字面量**（无变量/函数/拼接）。
- `Date.now()`/`Math.random()`/无参 `new Date()` 在脚本中会抛错——禁用。
- **每个任务的验证**：① 读回确认本段已写入；② 若本机有 node，`node --check`（仅查语法，未定义全局是运行期不报错）；
  ③ 最终 Task 6 用 Workflow 工具端到端跑一个极小特性做集成验证（支持 resume 缓存已过阶段、迭代修错）。
- 文件位置：`bms-app/.claude/workflows/bms-small-v.js`（命名工作流，Workflow `name:'bms-small-v'` 可解析）。

---

## File Structure

- Create: `bms-app/.claude/workflows/bms-small-v.js` — 整个编排脚本（唯一交付物，分 5 段建成）。
- 运行时产出（由 workflow 写）：`bms-app/docs/features/<slug>/{00-iteration-plan,01-requirements,02-architecture,03-design,05-test-report,06-cicd,traceability,gate-log}.md` + `app/`、`tests/` 下的代码/测试。

---

## Task 1: 脚手架 —— meta + 输入 + 角色 prompt + 门即数据

**Files:**
- Create: `bms-app/.claude/workflows/bms-small-v.js`

- [ ] **Step 1: 写入脚本骨架**

```javascript
export const meta = {
  name: 'bms-small-v',
  description: '自动跑完一个 BMS 特性的敏捷-V 小 V：编排→需求→架构→设计→TDD→CICD→评审→追溯校验',
  phases: [
    { title: '编排' },
    { title: '左腿设计' },
    { title: 'TDD实现' },
    { title: '收尾' },
    { title: '验收' },
  ],
}

// ---------- 输入 ----------
const slug = args && args.slug
const feature = args && args.feature
const riskTier = (args && args.riskTier) || 'normal'
if (!slug || !feature) {
  throw new Error('bms-small-v 需要 args = { slug, feature, riskTier? }')
}
const FEAT = `docs/features/${slug}`
const REPO_HINT = '项目根=bms-app；构建/测试用 Windows venv：在 bms-app/ 下 `..\\.venv\\Scripts\\python.exe -m west ...`，测试用 `..\\run-tests-coverage.ps1 -Board mps2/an386`（见 docs/process-agents.md §4）。'

// ---------- 角色扮演 prompt（读 .claude/agents 单一事实源）----------
function rolePrompt(role, task) {
  return [
    `你要严格扮演一个已定义好的 Claude Code subagent。先用 Read 读取`,
    `\`bms-app/.claude/agents/bms-${role}.md\`，把 frontmatter 之后的正文当作你的系统提示/角色设定，`,
    `完全按其职责/边界/输入输出契约/工作准则工作。`,
    REPO_HINT,
    `特性：${feature}`,
    `特性目录：bms-app/${FEAT}`,
    ``,
    `本次任务：`,
    task,
  ].join('\n')
}

// ---------- 结构化 verdict（门用）----------
const VERDICT = {
  type: 'object',
  additionalProperties: false,
  required: ['passed', 'gaps', 'evidence'],
  properties: {
    passed: { type: 'boolean' },
    gaps: { type: 'array', items: { type: 'string' } },
    evidence: { type: 'string' },
  },
}

log(`bms-small-v 启动：slug=${slug} riskTier=${riskTier}`)
```

- [ ] **Step 2: 验证语法（若有 node）**

Run: `cd bms-app && node --check .claude/workflows/bms-small-v.js && echo SYNTAX_OK || echo "node 不可用，跳过（不影响）"`
Expected: 输出 `SYNTAX_OK`（或 node 缺失提示）。

- [ ] **Step 3: 结构核对**

Run: `cd bms-app && grep -c "export const meta" .claude/workflows/bms-small-v.js && grep -c "function rolePrompt" .claude/workflows/bms-small-v.js`
Expected: 各为 `1`。

---

## Task 2: 文档阶段门工厂 + Phase 0 编排 + Phase 1 左腿

**Files:**
- Modify: `bms-app/.claude/workflows/bms-small-v.js`（追加）

- [ ] **Step 1: 追加 docGate 工厂与左腿流水线**

```javascript
// ---------- 文档阶段门：独立验证者（与生产者不同的 agent 调用）----------
// criteria 为机械可核对 + 语义判据的文字；对抗式：拿不准判 passed=false。
async function docGate(stage, artifact, criteria) {
  return agent(
    [
      `你是独立验证者（不是该产物的生产者）。只读不改。`,
      `审查 bms-app/${FEAT}/${artifact} 是否达标——对抗式：主动找缺漏，任何拿不准都判 passed=false。`,
      `判据：`,
      criteria,
      `先 Read 该文件（不存在则 passed=false, gaps=["文件缺失"]）。返回 {passed, gaps[], evidence}。`,
    ].join('\n'),
    { label: `gate:${stage}`, phase: '左腿设计', schema: VERDICT }
  )
}

// 带重试的"阶段=生产+过门"（文档阶段，≤2 次）
async function stageWithDocGate(stage, role, produceTask, artifact, criteria) {
  let last
  for (let attempt = 1; attempt <= 2; attempt++) {
    const extra = last ? `\n\n上一轮验证未过，请修正这些缺口后重做：\n- ${last.gaps.join('\n- ')}` : ''
    await agent(rolePrompt(role, produceTask + extra), { label: `${stage}#${attempt}`, phase: stage === 'orchestrator' ? '编排' : '左腿设计' })
    const v = await docGate(stage, artifact, criteria)
    log(`门[${stage}] 第${attempt}次：passed=${v && v.passed}`)
    if (v && v.passed) return { ok: true, attempts: attempt }
    last = v || { gaps: ['验证者无返回'] }
  }
  return { ok: false, gaps: last.gaps }
}

// ---------- Phase 0：编排 ----------
phase('编排')
const orch = await stageWithDocGate(
  'orchestrator', 'orchestrator',
  `产出本特性迭代计划 ${FEAT}/00-iteration-plan.md，并初始化独立追溯矩阵 ${FEAT}/traceability.md` +
  `（套 traceability-matrix-template：需求ID|需求摘要|设计|验证方法|测试用例|状态）。`,
  '00-iteration-plan.md',
  '- 含特性目标/价值、小 V 派发清单(复选框)、失效安全考量、DoR/DoD。\n' +
  '- 同目录 traceability.md 已创建且为模板规定的 6 列表头。'
)
if (!orch.ok) return { status: 'BLOCKED', stage: 'orchestrator', gaps: orch.gaps }

// ---------- Phase 1：左腿 requirements → architect → designer（串行 + 各自过门）----------
phase('左腿设计')
const reqs = await stageWithDocGate(
  'requirements', 'requirements',
  `读 ${FEAT}/00-iteration-plan.md，写 ${FEAT}/01-requirements.md（EARS、REQ-<域>-NNN、可度量验收、验证方法、安全需求标注），并回填 traceability.md 需求列。`,
  '01-requirements.md',
  '- 每条需求有唯一 REQ-<域>-NNN（域∈SYS/AFE/SOC/PROT/BAL/COMM/BOARD，无额外前后缀）。\n' +
  '- 每条含 EARS 句式 + 可度量验收准则 + 验证方法；安全相关需求显式标注。\n' +
  '- traceability.md 的"需求ID/需求摘要"列已逐条回填。'
)
if (!reqs.ok) return { status: 'BLOCKED', stage: 'requirements', gaps: reqs.gaps }

const arch = await stageWithDocGate(
  'architect', 'architect',
  `读 ${FEAT}/01-requirements.md，写 ${FEAT}/02-architecture.md（ADR：zbus通道/线程优先级/模块边界/失效安全；每条 ADR 标注服务的 REQ-ID）。`,
  '02-architecture.md',
  '- 每条架构决策(ADR)标注其服务的 REQ-ID。\n' +
  '- 明确 zbus 通道/数据结构变更、线程与优先级（安全线程更高）、失效安全影响。'
)
if (!arch.ok) return { status: 'BLOCKED', stage: 'architect', gaps: arch.gaps }

const des = await stageWithDocGate(
  'designer', 'designer',
  `读 ${FEAT}/02-architecture.md，写 ${FEAT}/03-design.md（DES-<域>-NNN、函数签名/契约/错误码、状态机、Kconfig/dts、纯逻辑测试目标），并回填 traceability.md 设计列。`,
  '03-design.md',
  '- 给本设计分配 DES-<域>-NNN，并写明"满足需求"(覆盖哪些 REQ-*)。\n' +
  '- 每条需求至少被一个 DES 覆盖；函数契约/状态机/Kconfig 可直接编码。\n' +
  '- traceability.md 的"设计"列已回填 DES-ID。'
)
if (!des.ok) return { status: 'BLOCKED', stage: 'designer', gaps: des.gaps }
```

- [ ] **Step 2: 语法 + 结构核对**

Run: `cd bms-app && (node --check .claude/workflows/bms-small-v.js && echo SYNTAX_OK); grep -c "stageWithDocGate" .claude/workflows/bms-small-v.js`
Expected: `SYNTAX_OK`（或 node 缺失）；`stageWithDocGate` 计数 ≥ 4（定义 1 + 调用 ≥3，实际 4 个调用 → ≥5）。

---

## Task 3: Phase 2 —— TDD 实现（tester↔coder 真值门 + 重试 ≤3）

**Files:**
- Modify: `bms-app/.claude/workflows/bms-small-v.js`（追加）

- [ ] **Step 1: 追加真值构建/测试门与 TDD 循环**

```javascript
// ---------- 真值门：让子代理跑构建/测试，返回真值 verdict ----------
const BUILD_VERDICT = {
  type: 'object',
  additionalProperties: false,
  required: ['passed', 'gaps', 'evidence'],
  properties: {
    passed: { type: 'boolean' },          // west build 退出码 0
    gaps: { type: 'array', items: { type: 'string' } },
    evidence: { type: 'string' },         // 贴构建末尾关键行
  },
}
const TEST_VERDICT = {
  type: 'object',
  additionalProperties: false,
  required: ['passed', 'failures', 'covLine', 'gaps', 'evidence'],
  properties: {
    passed: { type: 'boolean' },          // failures==0 且 覆盖率达门槛
    failures: { type: 'integer' },
    covLine: { type: 'number' },          // 行覆盖率%（拿不到填 -1）
    gaps: { type: 'array', items: { type: 'string' } },
    evidence: { type: 'string' },
  },
}

// ---------- Phase 2：TDD ----------
phase('TDD实现')

// safety：先让 tester 写红灯用例（确认实现前失败）
if (riskTier === 'safety') {
  await agent(rolePrompt('tester',
    `按 ${FEAT}/01-requirements.md 与 03-design.md，为安全/核心纯逻辑写 ztest 红灯用例（注释 /* Verifies REQ-<域>-NNN */），` +
    `运行确认其在实现前失败，把"红灯已确认"写入 ${FEAT}/05-test-report.md 顶部。`),
    { label: 'tester:red', phase: 'TDD实现' })
}

let testV
for (let attempt = 1; attempt <= 3; attempt++) {
  const fix = (testV && !testV.passed)
    ? `\n\n上一轮测试未过（failures=${testV.failures}, 覆盖率=${testV.covLine}）。请按以下修正后重做：\n- ${testV.gaps.join('\n- ')}`
    : ''
  // coder：实现 + 构建自检（返回真值）
  const buildV = await agent(rolePrompt('coder',
    `按 ${FEAT}/03-design.md 实现代码（遵循 zbus/K_THREAD_DEFINE 范式、失效安全默认态、注释回链 REQ-/DES-ID）。` +
    `实现后在 bms-app/ 下运行 \`..\\.venv\\Scripts\\python.exe -m west build -b mps2/an386 app -p always\` 自检。` +
    `返回 {passed=构建退出码0, gaps[], evidence=构建末尾关键行}。` + fix),
    { label: `coder#${attempt}`, phase: 'TDD实现', schema: BUILD_VERDICT })
  if (!buildV || !buildV.passed) {
    testV = { passed: false, failures: -1, covLine: -1, gaps: (buildV && buildV.gaps) || ['构建失败/无返回'] }
    log(`TDD 第${attempt}次：构建未过`)
    continue
  }
  // tester：写/补测试 + 跑 twister + 覆盖率（返回真值）
  testV = await agent(rolePrompt('tester',
    `按 ${FEAT}/01-requirements.md、03-design.md 与已实现代码补/改 ztest（覆盖正常/边界/失效安全；命名与 /* Verifies REQ */ 注释），` +
    `在 bms-app/ 下运行 \`..\\run-tests-coverage.ps1 -Board mps2/an386\`，写 ${FEAT}/05-test-report.md 并回填 traceability.md 测试用例/状态列。` +
    `返回 {passed=(failures==0 且 覆盖率达门槛), failures, covLine, gaps[], evidence}。`),
    { label: `tester#${attempt}`, phase: 'TDD实现', schema: TEST_VERDICT })
  log(`TDD 第${attempt}次：failures=${testV && testV.failures} cov=${testV && testV.covLine}`)
  if (testV && testV.passed) break
}
if (!testV || !testV.passed) {
  return { status: 'BLOCKED', stage: 'tdd', gaps: (testV && testV.gaps) || ['TDD 3 次未过'] }
}
```

- [ ] **Step 2: 语法 + 结构核对**

Run: `cd bms-app && (node --check .claude/workflows/bms-small-v.js && echo SYNTAX_OK); grep -c "attempt <= 3" .claude/workflows/bms-small-v.js`
Expected: `SYNTAX_OK`（或 node 缺失）；`attempt <= 3` 计数 `1`。

---

## Task 4: Phase 3 收尾（CICD + 评审）+ Phase 4 验收（追溯断言 + 总结）

**Files:**
- Modify: `bms-app/.claude/workflows/bms-small-v.js`（追加）

- [ ] **Step 1: 追加收尾与验收**

```javascript
// ---------- Phase 3：收尾（cicd + 评审）----------
phase('收尾')
await agent(rolePrompt('cicd',
  `确认本特性新测试被 .github/workflows/ci.yml 的 twister 覆盖（通常 -T tests 自动纳入）；若需最小改动则改之；写 ${FEAT}/06-cicd.md 说明。`),
  { label: 'cicd', phase: '收尾' })

const review = await agent(
  [
    `你是资深嵌入式 C/Zephyr 代码评审员（独立评审门）。`,
    `评审本特性的工作区改动（用 git diff/status 看 app/、tests/、.github 改动；只评审不改）。`,
    `依据 bms-app/${FEAT}/03-design.md 与 01-requirements.md。重点：失效安全默认态、是否偏离设计契约、`,
    `是否夹带非目标范围改动、是否降低 CI/覆盖率门槛、每条需求是否能在 traceability.md 找到验证证据。`,
    `返回 {passed=无Blocker, gaps[]=Blocker与重要项, evidence}。`,
  ].join('\n'),
  { label: 'code-review', phase: '收尾', schema: VERDICT }
)
if (!review || !review.passed) {
  return { status: 'REVIEW_BLOCKED', stage: 'review', gaps: (review && review.gaps) || ['评审无返回'] }
}

// ---------- Phase 4：验收（追溯断言 + 总结）----------
phase('验收')
const trace = await agent(
  [
    `只读校验 bms-app/${FEAT}/traceability.md：`,
    `逐行检查 6 列(需求ID|需求摘要|设计|验证方法|测试用例|状态)是否有空单元；`,
    `并抽查"测试用例"列中的用例名是否能在 bms-app/tests/ 下真实找到（Grep）。`,
    `任何空单元或找不到的用例都计入 gaps。返回 {passed=无断链且用例存在, gaps[], evidence}。`,
  ].join('\n'),
  { label: 'gate:traceability', phase: '验收', schema: VERDICT }
)

return {
  status: trace && trace.passed ? 'DONE' : 'TRACE_GAP',
  slug,
  riskTier,
  tdd: { failures: testV.failures, covLine: testV.covLine },
  traceability: trace,
  deliverables: `bms-app/${FEAT}/`,
  note: '未执行任何 git 操作；请人工审阅 docs/features/' + slug + ' 与 app/、tests/ 改动后再提交。',
}
```

- [ ] **Step 2: 语法 + 结构核对**

Run: `cd bms-app && (node --check .claude/workflows/bms-small-v.js && echo SYNTAX_OK); grep -cE "status: trace|REVIEW_BLOCKED|gate:traceability" .claude/workflows/bms-small-v.js`
Expected: `SYNTAX_OK`（或 node 缺失）；计数 ≥ 3。

---

## Task 5: 选一个极小验证特性 + 准备运行前提

**Files:**
- 无新增文件（仅确认前提）

- [ ] **Step 1: 确认 7 个 agent 文件齐全（脚本依赖它们）**

Run: `cd bms-app && ls .claude/agents/bms-*.md | wc -l`
Expected: `7`

- [ ] **Step 2: 确认 venv 工具链可用（coder/tester 真值门依赖）**

Run: `powershell -ExecutionPolicy Bypass -Command "& 'D:\__00_WorkSpace\__06_Study\bms-workspace\.venv\Scripts\python.exe' -m west --version"`
Expected: 打印 west 版本（如 `West version: v1.5.0`）。

- [ ] **Step 3: 选定验证特性（normal 风险、改动极小）**

记录于本计划：`slug=comm-report-period-kconfig`，`feature="把 comm 模块 CAN 上报周期改为由 Kconfig (CONFIG_BMS_COMM_REPORT_PERIOD_MS) 可配，默认 200ms"`，`riskTier=normal`。
理由：触及 Kconfig + 一个模块 + 一个简单单测，能走完所有阶段但代价最小，不涉及安全红线。

---

## Task 6: 端到端集成验证（真跑一次 + 迭代）

**Files:**
- 运行时产出：`bms-app/docs/features/comm-report-period-kconfig/*`、`app/`/`tests/` 改动

- [ ] **Step 1: 用 Workflow 工具运行 `bms-small-v`**

用 Workflow 工具（scriptPath 指向 `bms-app/.claude/workflows/bms-small-v.js`）调用，传：
`args = { slug: "comm-report-period-kconfig", feature: "把 comm 模块 CAN 上报周期改为由 Kconfig CONFIG_BMS_COMM_REPORT_PERIOD_MS 可配，默认 200ms", riskTier: "normal" }`
Expected: 工作流按 5 个 phase 推进；`/workflows` 可见进度树；最终返回对象 `status` 字段。

- [ ] **Step 2: 校验返回与产出**

Run: `cd bms-app && ls docs/features/comm-report-period-kconfig/ && echo "--- 追溯 ---" && cat docs/features/comm-report-period-kconfig/traceability.md | head -20`
Expected: 见 `00-iteration-plan.md`、`01-requirements.md`、`02-architecture.md`、`03-design.md`、`05-test-report.md`、`06-cicd.md`、`traceability.md`、`gate-log.md`；返回 `status: "DONE"`。

- [ ] **Step 3: 校验真值门真的生效（测试真跑通过）**

Run: `powershell -ExecutionPolicy Bypass -File run-tests-coverage.ps1 -Board mps2/an386 -NoCoverage`（在 workspace 根）
Expected: `TESTS PASSED`，包含新 comm 测试，0 failures。

- [ ] **Step 4: 失败则迭代（resume 缓存已过阶段）**

若某阶段 BLOCKED 或返回 gaps：读 workflow 返回的 `stage`/`gaps`，修脚本（如门判据过严/prompt 不清），用 Workflow `resumeFromRunId` 重跑——未改动的前缀阶段命中缓存、只重跑出错段。重复至 `status: "DONE"`。

- [ ] **Step 5: 记录验证结论**

在 `bms-app/docs/features/comm-report-period-kconfig/` 旁或 PR 描述记录：workflow 一次调用端到端跑通、各门生效、追溯无断链、测试真通过。**不提交 git**（按 spec §6，git 阶段后期补）。

---

## 自查记录（计划编写者）

- **规格覆盖**：spec §3 输入 ↔ Task1；§4 管线 ↔ Task2/3/4；§5 阶段门(分层+门即数据+gate-log+重试) ↔ Task2 docGate/stageWithDocGate + Task3 真值门/重试≤3 + Task4 追溯断言；§2 agency-agents 映射(编排者=脚本/QA重试=TDD循环/文件交接) ↔ Task2-4；§6 不碰 git ↔ 全程无 git 命令、返回 note 提示人工提交；§7 成本 ↔ Task6 真跑；§8 扩展(git/agentType) ↔ 未做、留注释。全部覆盖。
- **gate-log**：Task2/3 的门循环应把每次 {stage,pass,gaps,attempt} 由对应 agent 追加写入 `${FEAT}/gate-log.md`（已在各 produce/gate prompt 的职责中隐含；执行时若发现未落盘，在对应 prompt 显式加"把本次门结果追加到 gate-log.md"）。
- **占位符**：无 TBD/TODO；各 Task 给出完整 JS 段与可跑校验命令。
- **介质约束**：脚本无 fs/Node API → 所有文件/构建/测试检查都经 agent 返回 schema，JS 只做布尔判定（Task2-4 一致）。
- **命名一致**：`rolePrompt`/`docGate`/`stageWithDocGate`/`VERDICT`/`BUILD_VERDICT`/`TEST_VERDICT`/`FEAT`/`testV` 跨任务一致。
