# TODO

## 已完成（里程碑）

- [x] **P0-CI**：`format → build → twister` 已接入 GitHub Actions（详见 [docs/development-workflow.md](docs/development-workflow.md)）。
- [x] **CI 覆盖率**：`test-coverage`(native_sim + gcovr) 已在 CI 跑通 —— Windows/QEMU 的覆盖率遗留在 Linux CI 上解决。
- [x] **gcc SCA 门禁**：`sca-gcc` 已接入，按路径只拦 `bms-app/app` 的 `-Wanalyzer`（[scripts/sca-check.sh](scripts/sca-check.sh)）。
- [x] **CD**：`release.yml`（tag `v*` → 固件制品 + SHA256 + Release）。
- [x] **分支保护**：master 要求 PR + 5 项 CI 必过。

## 进行中 / 待办

- [ ] **clang-tidy 转硬门禁**：现为软门禁（`continue-on-error`）。需先整改首轮告警：
  - 编译参数兼容（clang 不认 `-fno-reorder-functions` 等 gcc flag）—— 过滤 compile_commands.json 或换 LLVM 工具链构建分析库；
  - 头文件保护宏 `BMS_*_H_` 命名误报 —— 加 `MacroDefinitionIgnoredRegexp`；
  - `memset` 安全检查噪声 —— 嵌入式可禁用该 check。
  调好后开 `.clang-tidy` 的 `WarningsAsErrors` 并把 `clang-tidy` 加入分支保护必过列。
- [ ] **覆盖率阈值**：观察 CI 基线后给 twister 加 `--coverage-threshold`。
- [ ] **本地 Windows 覆盖率**：QEMU 路线不可靠；如需本地出覆盖率用 **WSL2 + native_sim**（`west twister -p native_sim --coverage`）。CI 已可替代日常需求。
- [ ] **bms_f405 进矩阵**：自定义板 dts/defconfig 完善后，加入 CI build 矩阵与 release.yml。
- [ ] **后续增强**：codechecker（封装 clang-tidy/clang-analyzer + cppcheck）做更深 SCA；MISRA 视功能安全需求评估（商业工具）。见 [docs/ci-borrow-checklist.md](docs/ci-borrow-checklist.md)。
