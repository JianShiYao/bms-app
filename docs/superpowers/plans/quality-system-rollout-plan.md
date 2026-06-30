<!--
  开发流程与质量审查体系的实施计划（历史/参考）。
  本计划已基本执行完毕，落地结果见下列「当前状态」链接；正文保留为决策与次序记录。
-->
# 开发流程与质量审查体系化 —— 实施计划

> **状态（2026-06-18）**：本计划已执行完成。当前实况见
> [质量管控全景](quality-management.md) 与 [开发流程](process-workflow.md)；
> 本文保留为**实施记录**（设计决策、次序、风险规避）。已通过 PR #1–#14 落地，
> 含 6 道 CI 门禁、分支保护、CD（v0.1.0 / v0.1.1）、clang-tidy 硬化、app 覆盖率门禁。
>
> **后续进展（超出本轮计划，2026-06）**：本文「不在本轮范围」中的部分项已陆续落地——
> cppcheck/MISRA（本地 warn-only，见 [process-workflow.md §8](process-workflow.md)）、
> afe 单测（20 例）与可切换采样后端（stub/sim/adc）。最新状态以上述两份当前实况文档为准。

## Context（为什么做这件事）
bms-app（Zephyr v4.4.0 BMS 固件，github.com/JianShiYao/bms-app）当时底座扎实但流程零散：
clang-format/pre-commit 钩子、最小 CI（`format` → `build-test`）、11/11 测试、VERSION 0.1.0 都在，
但**没有贯穿的开发流程主线、没有分支保护、没有 CD、没有成文规范**，提交仍直推 master。

用户决定：**PR 分支流 + 分支保护**；**全范围含 CD**；CI 四道门禁全上（覆盖率/SCA/多板/clang-tidy）。
目标产出：一套"单人也按团队级走"的开发流程 + 质量审查体系，并成文为单一事实源文档。

## 目标终态（一图看懂）
```
本地: 编辑(.clang-format/.editorconfig) → git commit(pre-commit 格式钩子)
   → push 到 feature 分支 → 开 PR
CI(GitHub Actions, push+PR): format → [build(mps2/an386), build(native_sim),
                                        test-coverage, sca-gcc, clang-tidy]
   → 分支保护要求必过 → squash 合并 master → 自动删分支
发布: 改 VERSION+CHANGELOG → tag vX.Y.Z → release.yml 构建固件制品 + 校验和 → GitHub Release
```

五道质量关 → 本仓库工具：①编辑时 `.clang-format/.editorconfig` ②提交前 `scripts/hooks/pre-commit`
③CI/PR `ci.yml`(分支保护拦合并) ④发布 `release.yml`(tag 触发) ⑤审计 `dependabot/CODEOWNERS/CHANGELOG/PR模板`。

---

## 要新建 / 修改的文件

**新建**
- `.clang-tidy` — 从 stm32-project-template 改编。**关键：必须改成 snake_case**（stm32 那份是 C++ CamelCase + `WarningsAsErrors:'*'`，原样会对每个 C 函数/变量报错全红）。保留 `cert-*`/`readability-*`，命名改 `FunctionCase/VariableCase/ParameterCase: lower_case`、`MacroDefinitionCase: UPPER_CASE`；`HeaderFilterRegex: '.*/app/include/bms/.*\.h$'`。**首轮 `WarningsAsErrors` 关闭**（先盘点再硬化）。
- `scripts/sca-check.sh` — 便携 grep 门禁：从构建日志里只挑 `bms-app/app` 路径下的 `-Wanalyzer` 告警，命中则 exit 1（忽略 `zephyr/*`/`modules/*` 噪声）。CI 与本地共用。
- `scripts/check-version-tag.sh` — 校验 git tag `vX.Y.Z` 与 `VERSION` 文件解析出的 `MAJOR.MINOR.PATCHLEVEL` 一致，不一致 fail。
- `.github/workflows/release.yml` — CD，见下。
- `.github/pull_request_template.md` — 变更说明 + 类型 + 自检清单（format/build/twister/规范提交/必要时改 CHANGELOG·VERSION）+ BMS「风险与回滚」。
- `.github/CODEOWNERS` — `* @JianShiYao`（注释示范日后按目录细分，如 protection 给 safety 团队）。
- `.github/dependabot.yml` — `github-actions` 生态、weekly、commit 前缀 `ci:`。
- `CHANGELOG.md` — Keep a Changelog（中文），`[Unreleased]` + `[0.1.0]` 回填现有提交。
- `docs/process-workflow.md` — **单一事实源**：分支模型/提交规范/克隆设置(链 README §6)/pre-PR 自检三连/PR 流程/五道质量关表/发布流程/分支保护与 solo 说明。
- `CONTRIBUTING.md` — 极简转发，仅指向 `docs/process-workflow.md`（GitHub 在 PR 界面会识别它）。

**修改**
- `.github/workflows/ci.yml` — 由 2 job 扩成 5 道门禁 DAG（见下）。
- `README.md` — 顶部加一行链到 `docs/process-workflow.md`（其余不动，§6 仍是权威）。
- `TODO.md` — 勾掉 P0-CI，更新 SCA/覆盖率状态。
- 顺带纳入版本控制：当时 untracked 的 `.editorconfig`、`TODO.md`。

---

## ci.yml 增强（5 道门禁 DAG）
```
format ──> build (matrix: mps2/an386, native_sim) ──┐
       └─> test-coverage / sca-gcc / clang-tidy ─────┘  (均 needs: format, 并行)
```
所有 job 复用 `zephyrproject-rtos/action-zephyr-setup@v1`（含 SDK + west + 缓存）。check 名（供分支保护引用）：`format`、`build (mps2/an386)`、`build (native_sim)`、`test-coverage`、`sca-gcc`、`clang-tidy`。matrix job 用 `name: build (${{ matrix.board }})` 固定检查名。

- **format**：保持现状（pip clang-format==22.1.5 + dry-run）。
- **build**：`strategy.matrix.board: [mps2/an386, native_sim]`，`fail-fast:false`。**不含 bms_f405**（模板未完成会编不过），留注释待其 dts/defconfig 完成后加一行。
- **test-coverage**：`west twister -T bms-app/tests -p native_sim --coverage --coverage-tool gcovr`；`upload-artifact`（`if: always()`）传覆盖率报告。**覆盖率阈值先关**（待基线确定后启用）。
- **sca-gcc**：`west build -b mps2/an386 bms-app/app -- -DZEPHYR_SCA_VARIANT=gcc 2>&1 | tee sca-build.log` → `bash bms-app/scripts/sca-check.sh sca-build.log`。
- **clang-tidy**：`apt-get install -y clang-tidy` → `west build -b native_sim bms-app/app -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` → `clang-tidy -p build $(find bms-app/app/src -name '*.c')`。**首轮 `continue-on-error: true`（软门禁）**，盘点并调好 `.clang-tidy` 后再硬化。用 native_sim 生成 compile_commands.json（host clang 认得编译参数，噪声远小于 arm 交叉参数）。

## release.yml（CD，tag `v*` 触发）
`permissions: contents: write` → checkout(fetch-depth:0) → `check-version-tag.sh` 校验 tag==VERSION → zephyr-setup →
`west build -b mps2/an386 bms-app/app -- -DCMAKE_BUILD_TYPE=Release` → 收集 `build/zephyr/zephyr.{elf,bin,hex,map}`（前缀 `mps2-an386-`，`|| true` 容错 + `sha256sum > SHA256SUMS`）→ `softprops/action-gh-release`（`generate_release_notes: true`，附件 `release/**`）。bms_f405 后续加第二构建块即可。

## 分支保护（master，确切命令）
```bash
gh api -X PUT /repos/JianShiYao/bms-app/branches/master/protection --input - <<'JSON'
{
  "required_status_checks": { "strict": true,
    "contexts": ["format","build (mps2/an386)","build (native_sim)","test-coverage","sca-gcc"] },
  "enforce_admins": false,
  "required_pull_request_reviews": { "required_approving_review_count": 0,
    "dismiss_stale_reviews": true, "require_code_owner_reviews": false },
  "restrictions": null, "required_linear_history": true,
  "allow_force_pushes": false, "allow_deletions": false,
  "required_conversation_resolution": true
}
JSON
```

- `contexts` **只放已存在且变绿的 check**；`clang-tidy` 软门禁阶段**不**放入（否则 PR 永久 pending），硬化后再加。
- 合并策略「仅 Squash + 自动删分支」：`gh api -X PATCH /repos/JianShiYao/bms-app -F allow_squash_merge=true -F allow_merge_commit=false -F allow_rebase_merge=false -F delete_branch_on_merge=true`。
- **注意**：免费账户私有仓不支持分支保护——本项目据此将仓库改为公开（或升级 Pro）后才启用。
- solo 说明：必需 reviewer 只能 0（用「PR + CI 必过 + 自审 diff」替代第二双眼）；团队化后改 reviewer≥1、`require_code_owner_reviews:true`。

---

## 实施次序（全程走 PR，注意 bootstrap 次序）
1. **PR #1**：增强 ci.yml + `.clang-tidy`(软) + `scripts/sca-check.sh` + `scripts/check-version-tag.sh` + `release.yml` + 纳入 `.editorconfig`。**此时 master 尚无保护**,合并后新 check 名才存在并变绿。
2. **开启分支保护**：跑上面 `gh api` 两条（protection + squash 策略）。此后 master 不可直推。
3. **PR #2**：`docs/process-workflow.md` + `CONTRIBUTING.md` + PR 模板 + CODEOWNERS + dependabot + `CHANGELOG.md` + README 链接 + 更新 TODO。**走完整受保护 PR 流程**。
4. **首发 tag**：`git tag -a v0.1.0` → push → 确认 release.yml 产出 Release + 固件 + SHA256SUMS。
5. **后续硬化**：盘点后 `.clang-tidy` 翻硬 + 加入必需检查；定覆盖率阈值；bms_f405 完成后进 build 矩阵与 release。

> 每一步动手前先说明做什么/为什么；改动类操作等确认。GitHub 建仓/推送/tag 用已认证的 `gh`/`git`，**不涉及任何密码**。

## 验证
- **CI 各门禁**：PR 推上后 `gh run watch`，确认 6 个 check 出现并按预期变绿；clang-tidy 软门禁产出告警清单。
- **分支保护**：试 `git push` 直推 master 应被拒；开小 PR 验证「CI 必过才能合并 + squash + 自动删分支」。
- **CD**：`v0.1.0`(匹配 VERSION) 触发 release.yml → 生成含固件 + SHA256SUMS 的 Release；故意打错 tag 应在 `check-version-tag.sh` 处 fail。

## 已知首轮风险（已规避）
1. `.clang-tidy` 原样照搬 → 命名规则全红：**已改 snake_case + 首轮软门禁**。
2. clang-tidy "unknown argument" 噪声：**用 native_sim 生成 compile_commands.json**（实测仍需 sed 过滤个别 gcc-only flag）。
3. `.hex` 某些配置不生成：release 用 `|| true` + 对实际产物做 SHA。
4. 覆盖率阈值过早开 → 误红：**首轮不设阈值**；后续自跑 gcovr（root=workspace、过滤 app/）再设 line≥55%/branch≥30%。
5. 分支保护 contexts 写入未存在的 check → PR 永久 pending：**严格按"先有绿 check 再开保护、clang-tidy 暂不入列"次序**。

## 不在本轮范围
固件签名/MCUboot、OTA/HIL 真机部署（需自托管 runner + 硬件）、MISRA 商业工具、bms_f405 板完善 —— 记入 [../TODO.md](../TODO.md)，待接真板阶段再做。
