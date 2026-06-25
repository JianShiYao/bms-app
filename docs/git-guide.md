# Git 管理制度与操作手册

本文是 bms-app 的 **Git 制度与操作细则**：仓库边界、分支/提交/合并规范、行尾与忽略策略、
钩子、标准工作流、并行隔离、发布打标签，以及**常见场景的命令示例与恢复操作**。

> **定位**：流程的"为什么/制度定义"以 [development-workflow.md](development-workflow.md) 为**单一事实源**；
> 本文是其 **Git 机制细化 + 操作手册**，不重复定义制度，只把"怎么做、出错怎么救"讲透并举例。
> 仓库：`github.com/JianShiYao/bms-app`（公开）。

---

## 1. 仓库拓扑与边界（什么进 git，什么不进）

本项目用 west **T2 拓扑**：**只有 `bms-app/` 是 git 仓库**（既是应用，又是 west manifest）。
Zephyr 与 HAL 模块由 `west update` 按 [west.yml](../west.yml) 拉到 workspace 上一级目录，**不属于本仓库**。

```
bms-workspace/            # 非 git 仓库（仅工作区容器）
├─ bms-app/               # ★ 唯一 git 仓库（所有 git 操作在此目录执行）
├─ zephyr/  modules/      # west 拉取的依赖 —— 不在 git 内，勿手改
└─ .venv/                 # Python venv —— 不在 git 内
```

**进 git 的**：`app/`、`boards/`、`drivers/`、`tests/`、`docs/`、`scripts/`、`.github/`、
`west.yml`、`VERSION`、`CHANGELOG.md` 及各配置文件。
**不进 git 的**（见 [.gitignore](../.gitignore)）：构建产物（`build*/`、`*.o/*.elf/*.bin/*.hex/*.map`）、
west 拉取的 `zephyr/*`·`modules/`·`tools/`、`twister-out/`、`.venv/`、cppcheck addon（`scripts/.cppcheck-addons/`）、
Doxygen 输出（`doxygen-out/`）、硬件大二进制（`docs/hardware/**/*.pdf|*.xlsx|*.ioc` → 另存网盘/LFS）。

> 例外：`zephyr/module.yml`（模块注册文件）**必须入库**，故 `.gitignore` 用 `/zephyr/*` + `!/zephyr/module.yml` 放行。

---

## 2. 分支模型

- **`master`**：唯一受保护主干，始终可构建、CI 全绿。**禁止直接 push**（分支保护拦截）。
- **工作分支**：`<type>/<kebab-描述>`，`type` 对齐提交规范：
  `feat/ fix/ docs/ ci/ style/ chore/ refactor/ test/ build/ perf/`。
- **一律从最新 `master` 切出**，不要从其他工作分支派生。

```bash
# 切前先同步 master，保证基线最新
git switch master && git pull
git switch -c feat/soc-coulomb-counting      # 新特性分支
```

真实例（本仓库历史）：`ci/local-quality-layering`、`docs/rollout-plan`、`chore/release-v0.1.1`、`fix/protection-uv-threshold`。

---

## 3. 提交信息规范（Conventional Commits）

```
<type>(<scope>): <祈使句摘要>

<可选正文：为什么这么改，而非改了什么>
<footer：BREAKING CHANGE: ... / Refs #n>
```

- **type**：`feat fix docs style refactor test chore ci build perf`
- **scope**：模块名 —— `soc protection afe balancing comm board ci docs`
- **版本影响**：`feat`→次版本，`fix`→修订，`BREAKING CHANGE`→主版本（0.x 阶段见 §9）。

**✅ 好例子**（取自真实提交）：
```
ci(quality): layer local checks (pre-push, check.ps1, cppcheck/MISRA)
fix(protection): clamp UV threshold to cell spec
docs(todo): consolidate unimplemented items with source/priority/necessity
```

**❌ 反例与问题**：
```
update                     # 无 type/scope，摘要无信息
fixed bug                  # 过去式、不具体
feat: 改了好多东西          # scope 缺失、摘要笼统
```

> 摘要用**祈使句**（"add/fix/clamp"而非"added/fixes"）；正文解释**为什么**，diff 已说明"改了什么"。

---

## 4. 行尾（.gitattributes）与忽略（.gitignore）

### 4.1 行尾统一 LF
[.gitattributes](../.gitattributes) 强制**仓库内一律 LF**（`* text=auto eol=lf`），Windows 工作副本可为 CRLF。
这解决两类坑：① clang-format/diff 因行尾"假改动"churn；② `.sh` 脚本带 CRLF 在 Linux/CI 跑不起来。

> 历史教训：曾因 CRLF 导致 clang-format 把 `K_THREAD_DEFINE` 改坏；`.gitattributes` 落地后根治。
> 二进制资产（`*.bin/*.elf/*.hex/*.png/*.pdf` 等）标 `binary`，永不转换。

### 4.2 忽略策略
新增"不该入库"的东西时，**改 [.gitignore](../.gitignore) 而不是靠自觉**。分类见 §1。
临时/本地文件放到忽略目录，避免 `git add -A` 误带入。

---

## 5. Git 钩子（克隆后必做一次）

钩子目录是仓库本地配置，**不随提交携带**，每个克隆要手动启用一次：

```powershell
git config core.hooksPath scripts/hooks    # 在 bms-app/ 下执行一次
```

启用后两道本地关（详见 [development-workflow.md §8](development-workflow.md)）：

| 钩子 | 时机 | 做什么 | 阻断性 |
|---|---|---|---|
| [pre-commit](../scripts/hooks/pre-commit) | `git commit` | 对暂存的 `app/drivers/tests` 下 `.c/.h` 跑 clang-format 校验 | **硬拒绝**（不符合则提交失败） |
| [pre-push](../scripts/hooks/pre-push) | `git push` | push 范围 format + 机会性增量 clang-tidy + cppcheck/MISRA | format/tidy 失败**拒推**；cppcheck/MISRA **仅告警** |

应急绕过（**仅限紧急、需说明理由**）：`git commit --no-verify` / `git push --no-verify`。

---

## 6. 标准工作流（端到端命令示例）

以"给 soc 加库仑计"为例，从零到合并：

```bash
# 0) 克隆后一次性设置（每个克隆做一次）
git config core.hooksPath scripts/hooks

# 1) 从最新 master 切分支
git switch master && git pull
git switch -c feat/soc-coulomb-counting

# 2) 编码……（纯函数 + ztest，遵循可测试性约定）

# 3) 本地自检（开 PR 前镜像 CI，见 development-workflow §6）
powershell -ExecutionPolicy Bypass -File scripts/check.ps1        # 全量
#   或 -Fast 只跑 format+build+test

# 4) 提交（pre-commit 钩子自动校验格式）
git add app/ tests/
git commit -m "feat(soc): add coulomb-counting SOC estimator"

# 5) 推送（pre-push 钩子跑 format/tidy/cppcheck）
git push -u origin feat/soc-coulomb-counting

# 6) 开 PR（填模板）
gh pr create --base master --fill

# 7) CI 6 门全绿 → Squash 合并 → 自动删分支
gh pr merge --squash --auto --delete-branch
```

> `gh` 用已认证的 GitHub CLI（`gh auth login`）；**全程不涉及账号密码**——git 走 token/SSH，不接受密码。

---

## 7. 并行工作：worktree 物理隔离

同时推进多个**不相关**任务时，用 git worktree 让冲突无从发生——"一任务 = 一 worktree = 一分支 = 一 PR"。

```bash
git worktree add ../bms-app-soc  -b feat/soc-coulomb     # 从 master 切隔离工作区
# …在 ../bms-app-soc 里独立开发 / 自检 / PR…
git worktree remove ../bms-app-soc                        # 合并后清理
```

- **默认单工作树串行即可**，仅在真有并行需求时才开 worktree（避免过度工程）。
- **禁止**两个 session 指向同一工作树改同一批文件（丢更新、状态错乱的根源）。
- 详见 [development-workflow.md §3.1](development-workflow.md)。

---

## 8. PR 与合并制度

`master` 受 **分支保护**，规则（确切设置）：

- **必过检查 6 项**（`required_status_checks`）：`format`、`build (mps2/an386)`、`build (native_sim)`、
  `test-coverage`、`sca-gcc`、`clang-tidy`——不全绿**合不进**。
- **仅允许 Squash 合并**（`allow_merge_commit=false`、`allow_rebase_merge=false`）→ master 线性、每 PR 一条规范提交。
- **合并后自动删分支**（`delete_branch_on_merge=true`）。
- **要求线性历史**（`required_linear_history=true`）、**禁强推/禁删 master**、**要求对话已解决**。
- **必需评审 = 0**（单人项目；以"PR + CI 必过 + 自审 diff"替代第二双眼）。团队化后改 reviewer ≥ 1 并开 `require_code_owner_reviews`。
- 已开启 **auto-merge**：`gh pr merge <n> --squash --auto --delete-branch` 可设"全绿即自动合并"。

设置分支保护（参考，已落地，需仓库公开或 Pro）：
```bash
gh api -X PUT repos/JianShiYao/bms-app/branches/master/protection --input protection.json
gh api -X PATCH repos/JianShiYao/bms-app \
  -F allow_squash_merge=true -F allow_merge_commit=false \
  -F allow_rebase_merge=false -F delete_branch_on_merge=true
```

> 注：**免费账户的私有仓不支持分支保护**——本项目据此将仓库设为**公开**后才启用。

---

## 9. 发布与标签管理（CD）

版本由 [VERSION](../VERSION) 文件 + git tag + [release.yml](../.github/workflows/release.yml) 联动；遵循 **SemVer 0.x**（`0.MINOR.PATCH`）。

```bash
# 1) 开发布分支，改 VERSION（PATCHLEVEL 或 VERSION_MINOR +1）+ CHANGELOG
git switch -c chore/release-v0.1.2
#    编辑 VERSION 与 CHANGELOG.md（把 [Unreleased] 落为 ## [0.1.2] - 日期）
git commit -am "chore(release): bump v0.1.2"
gh pr create --base master --fill          # 走完整受保护 PR 流程合并

# 2) master 上打 tag（tag 必须与 VERSION 一致，否则 release.yml 在 check-version-tag.sh 处 fail）
git switch master && git pull
git tag -a v0.1.2 -m "release v0.1.2"
git push origin v0.1.2                      # 触发 release.yml：构建固件 + SHA256SUMS + GitHub Release
```

- tag 命名 **`vX.Y.Z`**；[scripts/check-version-tag.sh](../scripts/check-version-tag.sh) 校验 `tag == VERSION`。
- 已发布：`v0.1.0`、`v0.1.1`（制品含 `mps2-an386-zephyr.{elf,bin,map}` + `SHA256SUMS`）。
- MINOR 进位 = 新功能/可能破坏；PATCH = 修复。上真实 STM32F405 板量产稳定后再升 `1.0.0`。

---

## 10. 常见场景与恢复操作（举例）

> 原则：**未推送的提交可自由改写**（reset/amend/rebase）；**已推送到共享分支的别强推改写**。
> master 永不强推。

### 10.1 提交到了错误的分支
把当前分支顶端那个提交挪到正确分支（本会话真实救过一次）：
```bash
git log --oneline -1                       # 记下错放的提交哈希，如 00d1800
git switch -c correct/branch origin/master # 建正确分支
git cherry-pick 00d1800                    # 把提交摘过来
git switch wrong-branch
git reset --hard HEAD~1                     # 从错分支移除该提交（顶端时安全）
```

### 10.2 撤销"最后一次提交"但保留改动
```bash
git reset --soft HEAD~1     # 撤提交，改动回到暂存区（未推送时用）
git reset --mixed HEAD~1    # 撤提交，改动回到工作区（默认）
```

### 10.3 改最后一次提交（信息或漏加文件）—— 仅限未推送
```bash
git add 漏掉的文件
git commit --amend -m "feat(soc): add coulomb-counting SOC estimator"
```
> 已 push 的提交别 amend（会与远程分叉）；改用新提交修正。

### 10.4 同步最新 master 到工作分支
```bash
git fetch origin master
git merge origin/master      # 安全、保留历史（推荐用于已推送的分支）
#   或（仅本地未推送时，想要线性历史）：git rebase origin/master
```

### 10.5 解决合并/变基冲突
```bash
# 出现冲突后：编辑标 <<<<<<< ======= >>>>>>> 的文件择优合并
git add <冲突文件>
git merge --continue        # 或 git rebase --continue
git merge --abort           # 想放弃整个合并、回到合并前
```

### 10.6 误删文件 / 想丢弃本地改动
```bash
git restore docs/architecture.md         # 丢弃该文件的未暂存改动（恢复到 HEAD）
git checkout -- .claude/CLAUDE.md         # 旧写法，恢复被删文件（本会话真实用过）
git restore --staged file                 # 仅取消暂存，保留改动
git clean -nd                             # 先预览将删除哪些未跟踪文件（-n 只看不删）
git clean -fd                             # 确认后真正删除未跟踪文件/目录
```

### 10.7 临时搁置改动去做别的
```bash
git stash push -m "wip: soc 半成品"
git switch fix/urgent  ...                # 处理急活
git switch feat/soc && git stash pop      # 回来恢复
```

### 10.8 查看/对比
```bash
git status -sb                 # 简洁状态 + 跟踪关系
git log --oneline -10          # 近 10 条
git diff origin/master...HEAD  # 本分支相对 master 的净改动（PR 将合入的内容）
git show <hash>                # 某次提交的完整 diff
```

---

## 11. 禁止事项（红线）

- ❌ **直接 push `master`** / 对 master 强推（`--force`）——分支保护已拦，也不要尝试绕过。
- ❌ **滥用 `--no-verify`** 绕过钩子——仅限紧急且须说明；常态必须过钩子。
- ❌ **提交密钥/令牌/密码**（`.env`、token、私钥）——一旦推送即视为泄露，须改密 + 清历史。
- ❌ **提交大二进制**（数据手册 PDF、固件镜像、抓包）——走网盘/LFS，仅留文本说明。
- ❌ **提交构建产物 / west 依赖**（`build*/`、`zephyr/`、`modules/`）——已 gitignore，勿 `-f` 强加。
- ❌ **混合无关改动进一个 PR**——一个 PR 一件事，便于评审与回滚。

---

## 12. 速查表

| 目的 | 命令 |
|---|---|
| 克隆后启用钩子 | `git config core.hooksPath scripts/hooks` |
| 从最新 master 切分支 | `git switch master && git pull && git switch -c <type>/<desc>` |
| 本地自检 | `powershell -File scripts/check.ps1`（`-Fast` 快跑） |
| 提交 | `git add … && git commit -m "<type>(<scope>): …"` |
| 推送并建追踪 | `git push -u origin <branch>` |
| 开 PR | `gh pr create --base master --fill` |
| 全绿自动合并并删支 | `gh pr merge <n> --squash --auto --delete-branch` |
| 同步 master | `git fetch origin master && git merge origin/master` |
| 撤未推送的提交（留改动） | `git reset --soft HEAD~1` |
| 恢复误删文件 | `git restore <file>` |
| 发版打标签 | `git tag -a vX.Y.Z -m "release vX.Y.Z" && git push origin vX.Y.Z` |

---

> 相关文档：[development-workflow.md](development-workflow.md)（流程权威）、
> [build-guide.md](build-guide.md)（构建/运行/测试）、[quality-management.md](quality-management.md)（质量管控全景）。
