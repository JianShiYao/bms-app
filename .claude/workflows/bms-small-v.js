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
let _args = args
if (typeof _args === 'string') {
  try { _args = JSON.parse(_args) } catch (e) { _args = {} }
}
const slug = _args && _args.slug
const feature = _args && _args.feature
const riskTier = (_args && _args.riskTier) || 'normal'
if (!slug || !feature) {
  throw new Error('bms-small-v 需要 args = { slug, feature, riskTier? }；实际收到 typeof=' + (typeof args) + ' value=' + JSON.stringify(args))
}
const FEAT = `docs/features/${slug}`
const REPO_HINT = '项目根=bms-app；构建/测试用 Windows venv：在 bms-app/ 下 `..\\.venv\\Scripts\\python.exe -m west ...`，测试用 `..\\run-tests-coverage.ps1 -Board mps2/an386`（见 docs/agents-guide.md §4）。'

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

// ---------- 文档阶段门：独立验证者（与生产者不同的 agent 调用）----------
// criteria 为机械可核对 + 语义判据的文字；对抗式：拿不准判 passed=false。
async function docGate(stage, artifact, criteria, phaseTag) {
  return agent(
    [
      `你是独立验证者（不是该产物的生产者）。只读被审产物，不修改它。`,
      `审查 bms-app/${FEAT}/${artifact} 是否达标——对抗式：主动找缺漏，任何拿不准都判 passed=false。`,
      `判据：`,
      criteria,
      `先 Read 该文件（不存在则 passed=false, gaps=["文件缺失"]）。`,
      `最后把本次判定一行追加到 bms-app/${FEAT}/gate-log.md（格式 \`[${stage}] passed=<true|false> gaps=<条数> — <evidence首句>\`；用 Edit 追加，文件不存在则先创建）。`,
      `返回 {passed, gaps[], evidence}。`,
    ].join('\n'),
    { label: `gate:${stage}`, phase: phaseTag || '左腿设计', schema: VERDICT }
  )
}

// 带重试的"阶段=生产+过门"（文档阶段，≤2 次）
async function stageWithDocGate(stage, role, produceTask, artifact, criteria) {
  const ph = stage === 'orchestrator' ? '编排' : '左腿设计'
  let last
  for (let attempt = 1; attempt <= 2; attempt++) {
    const extra = last ? `\n\n上一轮验证未过，请修正这些缺口后重做：\n- ${last.gaps.join('\n- ')}` : ''
    await agent(rolePrompt(role, produceTask + extra), { label: `${stage}#${attempt}`, phase: ph })
    const v = await docGate(stage, artifact, criteria, ph)
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
    passed: { type: 'boolean' },          // 真实断言失败==0（flaky 超时不计、覆盖率不据此门控）
    failures: { type: 'integer' },        // 真实断言失败数（不含 QEMU flaky 超时）
    covLine: { type: 'number' },          // 行覆盖率%；QEMU 常拿不到→-1，覆盖率以 CI(native_sim) 为准
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
    `返回 {passed=构建退出码0, gaps[], evidence=构建末尾关键行}。把本次结果一行追加到 ${FEAT}/gate-log.md（[coder#${attempt}] build passed=..）。` + fix),
    { label: `coder#${attempt}`, phase: 'TDD实现', schema: BUILD_VERDICT })
  if (!buildV || !buildV.passed) {
    testV = { passed: false, failures: -1, covLine: -1, gaps: (buildV && buildV.gaps) || ['构建失败/无返回'] }
    log(`TDD 第${attempt}次：构建未过`)
    continue
  }
  // tester：写/补测试 + 跑 twister + 覆盖率（返回真值）
  testV = await agent(rolePrompt('tester',
    `按 ${FEAT}/01-requirements.md、03-design.md 与已实现代码补/改 ztest（覆盖正常/边界/失效安全；命名与 /* Verifies REQ */ 注释）。\n` +
    `在 bms-app/ 下跑【全量】套件：\`..\\run-tests-coverage.ps1 -Board mps2/an386\`（该脚本会跑 tests/ 下所有套件，用于发现跨套件回归——不要只跑本模块套件）。\n` +
    `QEMU 偶发超时容错：若某套件仅因 "Timeout"（非断言失败）而 FAILED，单独重跑该套件一次确认；仍只超时则视为基础设施 flaky，在 evidence 注明、不计入 failures。\n` +
    `覆盖率：QEMU(mps2/an386) 路线 gcov 常截断、covLine 可能为 -1——覆盖率以 CI(native_sim) 为准，本门不据 covLine 判 fail。\n` +
    `写 ${FEAT}/05-test-report.md 并回填 traceability.md 测试用例/状态列；把本次结果一行追加到 ${FEAT}/gate-log.md（[tester#${attempt}] passed=.. failures=.. cov=..）。\n` +
    `返回 {passed=(真实断言失败数==0), failures=真实断言失败数(不含flaky超时), covLine(拿不到填-1), gaps[], evidence}。`),
    { label: `tester#${attempt}`, phase: 'TDD实现', schema: TEST_VERDICT })
  log(`TDD 第${attempt}次：failures=${testV && testV.failures} cov=${testV && testV.covLine}`)
  if (testV && testV.passed) break
}
if (!testV || !testV.passed) {
  return { status: 'BLOCKED', stage: 'tdd', gaps: (testV && testV.gaps) || ['TDD 3 次未过'] }
}

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
    `返回 {passed=无Blocker, gaps[]=Blocker与重要项, evidence}。把评审结论一行追加到 bms-app/${FEAT}/gate-log.md（[review] passed=.. gaps=..）。`,
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
    `任何空单元或找不到的用例都计入 gaps。把校验结论一行追加到 bms-app/${FEAT}/gate-log.md（[traceability] passed=.. gaps=..）。返回 {passed=无断链且用例存在, gaps[], evidence}。`,
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
