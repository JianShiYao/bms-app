# TODO

## 已完成（里程碑）

- [x] **P0-CI**：`format → build → twister` 已接入 GitHub Actions（详见 [docs/development-workflow.md](docs/development-workflow.md)）。
- [x] **CI 覆盖率**：`test-coverage`(native_sim + gcovr) 已在 CI 跑通 —— Windows/QEMU 的覆盖率遗留在 Linux CI 上解决。
- [x] **gcc SCA 门禁**：`sca-gcc` 已接入，按路径只拦 `bms-app/app` 的 `-Wanalyzer`（[scripts/sca-check.sh](scripts/sca-check.sh)）。
- [x] **CD**：`release.yml`（tag `v*` → 固件制品 + SHA256 + Release）。
- [x] **clang-tidy 硬门禁**：调优后 0 告警，开启 `WarningsAsErrors` 并加入必过列。
- [x] **分支保护**：master 要求 PR + **6 项** CI 必过（含 clang-tidy）。
- [x] **app 覆盖率门禁**：自跑 gcovr（root=workspace、过滤 `app/`）并设阈值 line ≥ 55% / branch ≥ 30%（基线 lines 61.0% / branches 39.1%）。

## 进行中

- [ ] **本地 Windows 覆盖率**：QEMU 路线不可靠；如需本地出覆盖率用 **WSL2 + native_sim**（`west twister -p native_sim --coverage`）。CI 已可替代日常需求。
- [ ] **bms_f405 进矩阵**：自定义板 dts/defconfig 完善后，加入 CI build 矩阵与 release.yml。
- [ ] **后续增强**：codechecker（封装 clang-tidy/clang-analyzer + cppcheck）做更深 SCA；MISRA 视功能安全需求评估（商业工具）。见 [docs/ci-borrow-checklist.md](docs/ci-borrow-checklist.md)。

## 待办

- [ ] **renode与qemu对比** ：通过对比二者进行模拟硬件环境的选型、适配。
- [ ] **codereview** ：多人开发还需要增加代码审查环节。
