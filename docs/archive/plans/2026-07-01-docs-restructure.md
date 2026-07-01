# docs/ 类别重构 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `docs/` 顶层由"带前缀散落文件"重构为"按类别的自导航目录树"（9 个语义单一子目录，去前缀，各带三段式 README），并修正全仓所有引用。

**Architecture:** 先 `git mv` 建新树（含去前缀、`design/` 解散、`work/`·`archive/`·`templates/`·`reference/` 分层）；再用一个 Python 迁移脚本按"源新位置 × 目标新位置"重算 markdown 相对链接、并映射代码/脚本/CI 里的 `docs/` 路径式引用与裸前缀文件名；最后改写元文档、补 9 份 README、逐链校验、PR 走 CI 后 squash 合并。

**Tech Stack:** git、Python 3、bash（git-bash）、gh CLI。权威依据：[../specs/2026-07-01-docs-restructure-design.md](../specs/2026-07-01-docs-restructure-design.md)（迁移期该 spec 会随 `superpowers/specs → archive/specs` 移动）。

**分支：** `docs/restructure-by-category`（已存在，spec 已在其上）。

---

## Task 0：Pre-flight 基线（已完成侦察，记录结论）

**结论（无需再查）：**
- `docs/Doxyfile` **留原地**；`.github/workflows/docs.yml` 只引用 `docs/Doxyfile` 与 `INPUT=app/include/bms`，**无需改**。
- 需纳入改引用面的非文档文件：`scripts/check-file-headers.py`、`scripts/check-test-files.py`、`.github/workflows/ci.yml`、根 `README.md`、`TODO.md`、`CLAUDE.md`、`app/**/*.{c,h}`、`tests/**/*.c`、`app/Kconfig`。
- 既存无关死链（如 `quality-verification.md`、`superpowers/plans` 内快照相对链接）**不在本次范围**。

- [ ] **Step 0.1：确认在分支上、工作树干净**

Run:
```bash
git rev-parse --abbrev-ref HEAD   # 期望 docs/restructure-by-category
git status --short                # 期望干净（spec/plan 已提交）
```

- [ ] **Step 0.2：记录迁移前 broken-link 基线（用于最后排除既存死链）**

Run（保存基线到 scratchpad）：
```bash
python - <<'PY' > "$SCRATCH/baseline-broken.txt"
import os,re,glob
for f in glob.glob('docs/**/*.md',recursive=True)+['CLAUDE.md','README.md','TODO.md']:
    if not os.path.exists(f): continue
    d=os.path.dirname(f); t=open(f,encoding='utf-8').read()
    for m in re.finditer(r'\]\(([^)]+)\)',t):
        g=m.group(1).split('#')[0].strip()
        if not g or g.startswith(('http','#','mailto:')) or not(g.endswith('.md') or '/' in g): continue
        if not os.path.exists(os.path.normpath(os.path.join(d,g))): print(f,'->',g)
PY
```
(`$SCRATCH` = 会话 scratchpad 目录。此清单在 Task 4 校验时用来剔除"迁移前就断"的链接。)

---

## Task 1：建新树（git mv + 去前缀 + design/ 解散）

**Files:** 移动全部文档；新建目录 `concept/ process/ standard/ quality/ guide/ work/ archive/ reference/`。

- [ ] **Step 1.1：建目录并 git mv（逐条按映射）**

Run（在 `bms-app/`）：
```bash
mkdir -p docs/concept docs/process docs/standard docs/quality docs/guide docs/work docs/archive docs/reference

# concept/（去前缀 + design 解散）
git mv docs/concept-methodology.md               docs/concept/methodology.md
git mv docs/concept-documentation-system.md      docs/concept/documentation-system.md
git mv docs/design/concept-architecture.md       docs/concept/architecture.md
git mv docs/design/concept-runtime-model.md      docs/concept/runtime-model.md
git mv docs/design/concept-data-model.md         docs/concept/data-model.md
git mv docs/design/concept-diagnostics-fault-model.md docs/concept/diagnostics-fault-model.md
git mv docs/design/concept-safety.md             docs/concept/safety.md
# standard/
git mv docs/design/standard-module-interface.md  docs/standard/module-interface.md
git mv docs/coding-style.md                       docs/standard/coding-style.md
# process/
git mv docs/process-workflow.md                  docs/process/workflow.md
git mv docs/process-git.md                        docs/process/git.md
git mv docs/process-design-review.md             docs/process/design-review.md
git mv docs/process-agents.md                     docs/process/agents.md
git mv docs/process-small-v-workflow.md          docs/process/small-v-workflow.md
# quality/
git mv docs/quality-gates.md                      docs/quality/gates.md
git mv docs/quality-ci-checklist.md               docs/quality/ci-checklist.md
git mv docs/quality-integration-test-strategy.md docs/quality/integration-test-strategy.md
git mv docs/quality-management.md                 docs/quality/management.md
# guide/
git mv docs/guide-build.md                        docs/guide/build.md
# work/（活产物）
git mv docs/requirements                          docs/work/requirements
git mv docs/features                              docs/work/features
git mv docs/traceability.md                       docs/work/traceability.md
# archive/（历史归档）
git mv docs/superpowers/specs                     docs/archive/specs
git mv docs/superpowers/plans                     docs/archive/plans
# reference/
git mv docs/hardware                              docs/reference/hardware
# 清理空的 design/ 与 superpowers/
rmdir docs/design docs/superpowers 2>/dev/null || true
```
（`templates/` 保持 `docs/templates/`，不动。）

- [ ] **Step 1.2：核对新树**

Run:
```bash
find docs -maxdepth 2 -type d | sort
ls docs/concept docs/process docs/standard docs/quality docs/guide docs/work docs/archive docs/reference
```
Expected: 8 个新目录各含预期文件；无 `docs/design`、`docs/superpowers`。

- [ ] **Step 1.3：提交移动（引用随后修）**

```bash
git add -A
git commit -m "docs: 移动到按类别的目录结构（git mv，引用下一步修）

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2：引用重算脚本

**Files:** Create `"$SCRATCH/migrate_refs.py"`；Modify 全仓 md/代码/脚本/CI 引用。

- [ ] **Step 2.1：写迁移脚本**

写入 `"$SCRATCH/migrate_refs.py"`：
```python
import os, re, glob

# ---- 映射（唯一事实源，来自 spec §3）----
FILE_MAP = {
 "docs/concept-methodology.md":"docs/concept/methodology.md",
 "docs/concept-documentation-system.md":"docs/concept/documentation-system.md",
 "docs/design/concept-architecture.md":"docs/concept/architecture.md",
 "docs/design/concept-runtime-model.md":"docs/concept/runtime-model.md",
 "docs/design/concept-data-model.md":"docs/concept/data-model.md",
 "docs/design/concept-diagnostics-fault-model.md":"docs/concept/diagnostics-fault-model.md",
 "docs/design/concept-safety.md":"docs/concept/safety.md",
 "docs/design/standard-module-interface.md":"docs/standard/module-interface.md",
 "docs/coding-style.md":"docs/standard/coding-style.md",
 "docs/process-workflow.md":"docs/process/workflow.md",
 "docs/process-git.md":"docs/process/git.md",
 "docs/process-design-review.md":"docs/process/design-review.md",
 "docs/process-agents.md":"docs/process/agents.md",
 "docs/process-small-v-workflow.md":"docs/process/small-v-workflow.md",
 "docs/quality-gates.md":"docs/quality/gates.md",
 "docs/quality-ci-checklist.md":"docs/quality/ci-checklist.md",
 "docs/quality-integration-test-strategy.md":"docs/quality/integration-test-strategy.md",
 "docs/quality-management.md":"docs/quality/management.md",
 "docs/guide-build.md":"docs/guide/build.md",
 "docs/traceability.md":"docs/work/traceability.md",
}
DIR_MAP = {  # 长前缀在前
 "docs/superpowers/specs/":"docs/archive/specs/",
 "docs/superpowers/plans/":"docs/archive/plans/",
 "docs/superpowers/":"docs/archive/",
 "docs/requirements/":"docs/work/requirements/",
 "docs/features/":"docs/work/features/",
 "docs/hardware/":"docs/reference/hardware/",
}
# 裸前缀文件名（prose 提及）→ 新裸名（仅去前缀者；coding-style/traceability 名未变，不列）
BARE = {
 "concept-methodology.md":"methodology.md","concept-documentation-system.md":"documentation-system.md",
 "concept-architecture.md":"architecture.md","concept-runtime-model.md":"runtime-model.md",
 "concept-data-model.md":"data-model.md","concept-diagnostics-fault-model.md":"diagnostics-fault-model.md",
 "concept-safety.md":"safety.md","standard-module-interface.md":"module-interface.md",
 "process-workflow.md":"workflow.md","process-git.md":"git.md","process-design-review.md":"design-review.md",
 "process-agents.md":"agents.md","process-small-v-workflow.md":"small-v-workflow.md",
 "quality-gates.md":"gates.md","quality-ci-checklist.md":"ci-checklist.md",
 "quality-integration-test-strategy.md":"integration-test-strategy.md","quality-management.md":"management.md",
 "guide-build.md":"build.md",
}

def old_to_new(p):
    p=p.replace("\\","/")
    if p in FILE_MAP: return FILE_MAP[p]
    for o,n in sorted(DIR_MAP.items(),key=lambda kv:-len(kv[0])):
        if p.startswith(o): return n+p[len(o):]
    return p
def new_to_old(p):
    p=p.replace("\\","/")
    for o,n in FILE_MAP.items():
        if n==p: return o
    for o,n in sorted(DIR_MAP.items(),key=lambda kv:-len(kv[1])):
        if p.startswith(n): return o+p[len(n):]
    return p

MD=[f.replace("\\","/") for f in glob.glob("docs/**/*.md",recursive=True)]+["CLAUDE.md","README.md","TODO.md"]
CODE=(glob.glob("app/**/*.c",recursive=True)+glob.glob("app/**/*.h",recursive=True)
      +glob.glob("tests/**/*.c",recursive=True)+["app/Kconfig"]
      +glob.glob("scripts/**/*.py",recursive=True)
      +glob.glob(".github/workflows/*.yml",recursive=True)+glob.glob(".github/workflows/*.yaml",recursive=True))
CODE=[c.replace("\\","/") for c in CODE if os.path.isfile(c)]

# Pass A: markdown 相对链接 resolve→map→relativize
linkre=re.compile(r'\]\(([^)]+)\)')
for f in MD:
    if not os.path.exists(f): continue
    s_old=new_to_old(f)
    t=open(f,encoding="utf-8").read()
    def repl(m):
        tgt=m.group(1); raw=tgt.split("#")[0]; anc=tgt[len(raw):]
        if not raw or raw.startswith(("http://","https://","mailto:")): return m.group(0)
        ao=os.path.normpath(os.path.join(os.path.dirname(s_old),raw)).replace("\\","/")
        an=old_to_new(ao)
        nr=os.path.relpath(an,os.path.dirname(f)).replace("\\","/")
        return "]("+nr+anc+")"
    t2=linkre.sub(repl,t)
    if t2!=t: open(f,"w",encoding="utf-8").write(t2)

# Pass B: 路径式 docs/ 引用（含 .md 与目录路径）——字符串替换，长者优先
PAIRS=sorted(list(FILE_MAP.items())+list(DIR_MAP.items()),key=lambda kv:-len(kv[0]))
for f in set(MD+CODE):
    if not os.path.exists(f): continue
    t=open(f,encoding="utf-8").read(); t0=t
    for o,n in PAIRS: t=t.replace(o,n)
    if t!=t0: open(f,"w",encoding="utf-8").write(t)

# Pass C: 裸前缀文件名 prose 提及（长者优先，防子串）
BPAIRS=sorted(BARE.items(),key=lambda kv:-len(kv[0]))
for f in set(MD+CODE):
    if not os.path.exists(f): continue
    t=open(f,encoding="utf-8").read(); t0=t
    for o,n in BPAIRS: t=t.replace(o,n)
    if t!=t0: open(f,"w",encoding="utf-8").write(t)
print("done")
```

- [ ] **Step 2.2：运行脚本**

Run: `python "$SCRATCH/migrate_refs.py"`
Expected: 打印 `done`。

- [ ] **Step 2.3：校验无遗留旧引用**

Run:
```bash
grep -rn 'docs/design/\|docs/superpowers/\|docs/features/\|docs/requirements/\|docs/hardware/' \
  --include=*.md --include=*.c --include=*.h --include=*.py --include=*.yml . 2>/dev/null | grep -v '^./docs/archive/' | tr -d '\000'
grep -rn 'concept-architecture.md\|process-workflow.md\|quality-gates.md\|standard-module-interface.md' \
  --include=*.md --include=*.c --include=*.h --include=*.py . 2>/dev/null | tr -d '\000'
```
Expected: 空（历史快照 `docs/archive/**` 内的自指链接除外——它们是快照，允许保留旧措辞）。

- [ ] **Step 2.4：逐链解析校验（broken == baseline）**

Run:
```bash
python - <<'PY'
import os,re,glob
bad=0
for f in glob.glob('docs/**/*.md',recursive=True)+['CLAUDE.md','README.md','TODO.md']:
    if not os.path.exists(f): continue
    d=os.path.dirname(f); t=open(f,encoding='utf-8').read()
    for m in re.finditer(r'\]\(([^)]+)\)',t):
        g=m.group(1).split('#')[0].strip()
        if not g or g.startswith(('http','#','mailto:')) or not(g.endswith('.md') or '/' in g): continue
        if not os.path.exists(os.path.normpath(os.path.join(d,g))): print('BROKEN',f,'->',g); bad+=1
print('broken:',bad)
PY
```
Expected: 仅剩 `$SCRATCH/baseline-broken.txt` 里那些既存死链（逐条比对确认无新增）。若有新增 BROKEN → 手工修该链接。

- [ ] **Step 2.5：提交**

```bash
git add -A
git commit -m "docs: 迁移全部引用到新目录结构（相对链接重算 + 路径/裸名映射）

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3：补 9 份三段式子目录 README

**Files:** Create `docs/{concept,process,standard,quality,guide,work,archive,reference}/README.md`；Modify `docs/templates/README.md`（加三段式头）。

- [ ] **Step 3.1：写 8 份新 README + 改 templates/README.md**

每份严格三段：**这里放什么 / 不要放什么 / 权威文件**。内容按 spec §4：

`docs/concept/README.md`：
```markdown
# concept/ —— 概念与设计契约

- **这里放什么**：为什么这么做、目标模型是什么。方法论、文档体系，以及架构/运行时/数据/诊断/安全的设计契约（agent 据此实现代码）。
- **不要放什么**：操作步骤（→ `guide/`）、流程门（→ `process/`）、可执行的接口/编码约束（→ `standard/`）、活的产物（→ `work/`）。
- **权威文件**：`methodology.md`（方法论母文档，一切下游由它衍生）。
```

`docs/process/README.md`：
```markdown
# process/ —— 研发流程

- **这里放什么**：研发活动怎么走、门在哪。生命周期、Git 制度、设计评审、agent 编排。
- **不要放什么**：门禁阈值事实表（→ `quality/gates.md`）、"为什么"的原则（→ `concept/`）。
- **权威文件**：`workflow.md`（开发流程单一事实源）。
```

`docs/standard/README.md`：
```markdown
# standard/ —— 工程契约标准

- **这里放什么**：必须遵守的工程契约。模块接口标准、编码风格。
- **不要放什么**：目标模型讨论（→ `concept/`）、如何证明质量（→ `quality/`）。
- **权威文件**：`module-interface.md`（模块接口/数据 owner/任务 owner/安全默认态）。
```

`docs/quality/README.md`：
```markdown
# quality/ —— 质量与证明

- **这里放什么**：如何证明做得够好。门禁事实表、CI 借鉴清单、集成测试策略、质量全景总览。
- **不要放什么**：活的证据/追溯矩阵（→ `work/`）、流程步骤（→ `process/`）。
- **权威文件**：`gates.md`（门与阈值唯一事实源）。`management.md` 仅总览，不抢权威。
```

`docs/guide/README.md`：
```markdown
# guide/ —— 操作指南

- **这里放什么**：具体操作怎么做。环境搭建、构建、QEMU/WSL、测试运行。
- **不要放什么**：规则与约定（→ `concept/`、`standard/`）、门禁细节（→ `quality/`）。
- **权威文件**：`build.md`（环境/构建/测试命令权威，回指 CLAUDE.md）。
```

`docs/work/README.md`：
```markdown
# work/ —— 活的工程产物

- **这里放什么**：随产品演进的活产物与证据。需求基线（`requirements/`）、每个小 V 的交付物（`features/<slug>/`）、需求↔测试活矩阵（`traceability.md`）。
- **不要放什么**：按日期沉淀的历史记录（→ `archive/`）、常青规范文档（→ `concept/`·`process/`·`standard/`·`quality/`）。
- **权威文件**：`traceability.md`（需求验证到哪的活矩阵）；每特性看 `features/<slug>/`。
```

`docs/archive/README.md`：
```markdown
# archive/ —— 历史归档

- **这里放什么**：按日期沉淀的过程记录。设计 spec（`specs/`）、实施 plan（`plans/`），命名 `YYYY-MM-DD-…`。
- **不要放什么**：活产物（→ `work/`）、常青规范（→ 类别文件夹）。
- **权威文件**：无单一权威——历史快照按文件名日期为序；某主题的最新结论以对应常青文档为准（如架构以 `concept/architecture.md`）。
```

`docs/reference/README.md`：
```markdown
# reference/ —— 参考阅读资料

- **这里放什么**：参考阅读用的外部/硬件资料。硬件原理图、BOM、数据手册（`hardware/`）。
- **不要放什么**：可复制的产出骨架（→ `templates/`）、常青规范、活产物。
- **权威文件**：`hardware/__00_readme.md`（硬件资料索引）。
```

`docs/templates/README.md`（在现有内容顶部插入三段式头，保留其余模板索引内容）：
```markdown
# templates/ —— 可复制的产出骨架

- **这里放什么**：复制后填写的**骨架**。需求规格、设计规格、追溯矩阵模板。
- **不要放什么**：参考阅读资料（→ `reference/`）、真实产物（→ `work/`）。
- **注意**：这不是"参考阅读"，是**复制去用**的模板。
```

- [ ] **Step 3.2：校验并提交**

Run:
```bash
for d in concept process standard quality guide work archive reference templates; do
  [ -f "docs/$d/README.md" ] && echo "OK $d" || echo "MISS $d"
done
git add -A && git commit -m "docs: 为每个子目录补三段式 README（放什么/不放什么/权威文件）

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4：改写元文档

**Files:** Modify `docs/README.md`、`docs/concept/documentation-system.md`、`CLAUDE.md`、`TODO.md`、根 `README.md`。

- [ ] **Step 4.1：`docs/README.md`（顶层索引）重写**

改为反映新结构：
- 命名约定段：改为"**目录即类别**（`concept/process/standard/quality/guide`）+ 产物/参考四类（`work/archive/templates/reference`）；文件名**去前缀**"。
- 关系图：以 9 个目录为骨架（不再是 `<category>-topic` 前缀展开）。
- 文档清单：按目录分组列出（链接用新相对路径，如 `concept/architecture.md`）。
- 速查：更新为按目录导航。
- 强调"每个子目录有自己的 README，先看目录 README 再进文件"。

- [ ] **Step 4.2：`docs/concept/documentation-system.md`**

- §2「五类顶层文档（前缀）」→「五类**目录** + 四类产物/参考目录」；示例文件用新裸名。
- 删除上一版加的「2b. 学科目录（design/）」（design/ 已解散）。
- §3 权威链树：文件名用新裸名。

- [ ] **Step 4.3：`CLAUDE.md`**

- §2 必读文档链接改新路径（`docs/concept/architecture.md`、`docs/concept/data-model.md`、`docs/concept/safety.md`、`docs/process/workflow.md`、`docs/quality/gates.md` 等）。
- §8 文档命名约定：改为"目录即类别、文件名去前缀；见 docs/README.md"。

- [ ] **Step 4.4：`TODO.md` 与根 `README.md`**

- 更新其中文档链接到新路径（脚本 Pass A/B 已处理大部分；此步人工复核 §图例/锚点行是否通顺，如 `TODO.md` 顶部 `ARCH=`/`STD=`/`DOCSYS=` 图例）。
- 根 `README.md:158` 的 `docs/archive/specs/` 措辞复核。

- [ ] **Step 4.5：`quality/management.md` 定位 banner（受控例外）**

在 `docs/quality/management.md` 顶部（标题下）插入：
```markdown
> **定位**：本文仅为质量全景**总览**。门禁与阈值的事实以 [gates.md](gates.md) 为准，**不在此复制**。
```

- [ ] **Step 4.6：校验链接 + 提交**

Run: 重跑 Step 2.4 的 broken 校验（期望仍只剩基线死链）。
```bash
git add -A && git commit -m "docs: 改写元文档以对齐类别目录结构（README/文档体系/CLAUDE/TODO/management 定位）

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5：最终校验 + PR + 合并

- [ ] **Step 5.1：卫生校验（editorconfig 相关）**

Run:
```bash
# 末行换行
for f in $(git ls-files 'docs/**/*.md'); do
  [ "$(tail -c1 "$f" | xxd -p)" = "0a" ] || echo "MISS newline: $f"
done
# CRLF
grep -rlU $'\r' docs/ 2>/dev/null | tr -d '\000' || true
# 无 docs/docs 双写
grep -rn 'docs/docs' --include=*.md --include=*.c --include=*.h . 2>/dev/null | tr -d '\000' || true
```
Expected: 无 MISS、无 CRLF、无 docs/docs。有则修。

- [ ] **Step 5.2：推送 + 开 PR**

```bash
git push -u origin docs/restructure-by-category
gh pr create --base master --title "docs: docs/ 按类别重构为自导航目录树（去前缀 + 子目录 README）" --body "见 docs/archive/specs/2026-07-01-docs-restructure-design.md。移动+去前缀+补 9 份三段式 README+全量引用修正；Doxyfile 留原地；既存无关死链不在本次范围。"
```

- [ ] **Step 5.3：盯 CI**

Run: `gh pr checks <PR#> --watch --interval 20`
Expected: 全绿（尤其 editorconfig、3 板 build、tidy/misra/coverage）。红则修对应问题、重推。

- [ ] **Step 5.4：squash 合并 + 同步**

```bash
gh pr merge <PR#> --squash --delete-branch
git checkout master && git pull origin master
```

---

## 自检（spec 覆盖 / 占位 / 一致性）

- **spec §2 目标树** → Task 1 建树全覆盖（含 templates 留顶层、Doxyfile 留根）。✓
- **spec §3 映射** → Task 1 git mv + Task 2 FILE_MAP/DIR_MAP 一一对应。✓
- **spec §4 READMEs（9 份三段式）** → Task 3 给出全文。✓
- **spec §5 Doxyfile/脚本 CI** → Task 0 结论（Doxyfile 留、docs.yml 不改）+ Task 2 CODE 集含 scripts/*.py、.github/workflows/*.yml。✓
- **spec §6 引用策略（相对重算+路径+裸名）** → Task 2 Pass A/B/C。✓
- **spec §7 元文档改写** → Task 4。✓
- **spec §8/§9（受控例外 management banner）** → Task 4.5。✓
- 无占位；映射键名与 spec §3 逐条一致。
```
