# BMS 固件项目（Zephyr v4.4.0）

基于 **Zephyr RTOS v4.4.0** 的电池管理系统（BMS）固件。采用 west **T2 拓扑**（本仓库即 manifest 仓库），分层架构 + zbus 消息总线解耦。

> 参与开发请先读 **[开发流程与质量审查](docs/process-workflow.md)**（分支模型 / 提交规范 / PR 流程 / 质量门禁 / 发布）；
> Git 操作细则、命令示例与出错恢复见 **[Git 管理制度与操作手册](docs/process-git.md)**。

- 第一步：在 PC 上用 **QEMU（`mps2/an386`，Cortex-M4F）** 跑通架构与业务骨架。
  - 选 `mps2/an386` 是因为它与目标 STM32F405 同为 **Cortex-M4F（带硬件 FPU）**，架构忠实。
  - 注：`native_sim` 的 POSIX 架构仅支持 Linux（见 `zephyr/arch/posix/CMakeLists.txt`），
    Windows 上无法编译；如需 native_sim 请在 WSL2/Linux 下使用。
- 第二步：真机 bring-up 用 **`qmxx_f407zg`**（启明欣欣 STM32F407ZGT6 开发板，已入 CI 构建矩阵）。
- 目标：接入自定义 STM32F405 板（`boards/enervenue/bms_f405/`，dts/defconfig 待完善）。

## 目录结构

```
bms-app/                     # 本仓库（west manifest 仓库）
├─ west.yml                  # manifest：pin zephyr v4.4.0 + HAL
├─ app/
│  ├─ include/bms/           # 模块公共接口（头文件）
│  └─ src/bms/
│     ├─ afe/                # 采样 + 合理性校验（后端可切换：sim/stub/adc）
│     ├─ soc/ protection/ balancing/ comm/   # 业务模块
│     ├─ engine/             # db（数据库）/ diag（诊断）/ task（集中周期调度）
│     └─ application/        # bms 主状态机（纯函数）
├─ boards/
│  ├─ alientek/qmxx_f407zg/  # 真机 bring-up 板（STM32F407ZGT6，已入 CI）
│  └─ enervenue/bms_f405/    # 目标自定义板（模板，dts 待完善）
├─ drivers/                  # out-of-tree 驱动占位（如 AFE 芯片）
├─ tests/bms/{soc,protection,afe,comm}/   # ztest 单测套件
└─ docs/                     # 架构/流程/质量文档（总索引见 docs/README.md）
```

## 一、环境搭建 / 构建 / 运行 / 测试

> **完整步骤见 [构建指南 docs/guide-build.md](docs/guide-build.md)** —— 工具链与 SDK 安装、
> workspace 初始化、构建（增量/全新/多板）、QEMU 运行、单元测试、构建产物、配置覆盖、清理、排错速查。
>
> 首次搭建（装工具链 / `west init` + `west update` / 装 SDK）务必照
> [构建指南 §0 环境准备](docs/guide-build.md) 一步步来。下面是跑通后的常用命令速览：

```powershell
& ..\.venv\Scripts\Activate.ps1                       # 每个新终端先激活 venv
west build -p always -b mps2/an386 app                # 构建（首次约 1–2 分钟）
west build -t run                                     # QEMU 运行（Ctrl-A X 退出）

$env:QEMU_BIN_PATH = "D:\zephyr-sdk\zephyr-sdk-1.0.1\hosttools\qemu"
west twister -T tests -p mps2/an386 -c               # 单元测试（预期 57/57）
```

> 运行后启动日志示例：`*** Booting Zephyr OS build v4.4.0 ***` →
> `bms_main: ==== BMS firmware v0.1.1 (git v0.1.1-…-dirty) on mps2/an386 ====`
> （**版本 + git 描述编进固件**，供现场/日志追溯）→ engine/各模块 init → bms_task 周期调度。
> 真机板：`west build -b qmxx_f407zg app`（已入 CI）；目标板 `bms_f405` 待 dts 完善。

## 二、代码格式化与提交检查

代码风格沿用 **Zephyr 官方 clang-format**（配置见仓库根 `.clang-format`，pin 22.1.5）。

**克隆后首次** 需激活本地提交钩子。两种方式（二选一）：

```powershell
# 推荐：pre-commit 框架（.pre-commit-config.yaml，第三方 hook 版本锁定、可复现）
pip install pre-commit ; pre-commit install
# 或 fallback：项目自带 git hooks（core.hooksPath 是仓库本地配置，不随提交携带）
git config core.hooksPath scripts/hooks
```

激活后，每次 `git commit` 会自动检查本次暂存的 `app/`、`drivers/`、`tests/` 下
`.c/.h` 是否符合格式；不符合则拒绝提交。

```powershell
# 本地一键格式化（修正所有文件）
powershell -ExecutionPolicy Bypass -File scripts\format.ps1
# 只检查不修改（CI / 手动验证用）
powershell -ExecutionPolicy Bypass -File scripts\format.ps1 -Check
# 应急跳过钩子（不推荐）
git commit --no-verify
```

> 行尾由 `.gitattributes` 统一为 LF，确保 Windows 与 Linux/CI 一致。
>
> 注：两种钩子方式都同时启用 **pre-commit**（格式等）与 **pre-push**
> （format + 增量 clang-tidy + cppcheck/MISRA 告警）；后两者依赖下节的工具，未装则自动跳过。

## 三、静态分析工具依赖（cppcheck / clang-tidy）

pre-push 钩子与 `scripts\check.ps1` 会调用 **cppcheck**（含 MISRA）与 **clang-tidy**。
两者均为**可选本地依赖**：未安装时对应检查自动标 `SKIP`（不阻断本地操作），**CI 会权威地补跑**。
分层触发与各门细节见 [开发流程与质量审查](docs/process-workflow.md)。

### cppcheck（+ MISRA）

```powershell
winget install Cppcheck.Cppcheck      # 或：scoop install cppcheck
```

- **MISRA addon 不随 Windows 安装包附带**，需额外下载（GPLv3，不入库，放在 gitignore 的本地目录）：
  ```powershell
  # 需 git-bash；按已装 cppcheck 版本拉取 misra.py 等到 scripts\.cppcheck-addons\
  bash scripts/setup-cppcheck-misra.sh
  ```
- MISRA addon 运行需 **Python 在 PATH**（项目 venv 里即有）。
- **规则描述（可选）**：MISRA 规则文本受版权保护、**不入库**。默认只报规则号（如 `misra-c2012-13.4`）；
  若想同时打印规则描述，按机自备一份 rule-texts 文件放到 `scripts\.cppcheck-addons\misra-rule-texts.txt`
  （cppcheck `--rule-texts` 的「Appendix A」格式，来源：MISRA 官方 cppcheck headlines（CC BY-NC-ND，**商业项目用需注意授权**）或你授权的 MISRA C:2012 PDF）。
  脚本检测到即自动启用；详见 `bash scripts/setup-cppcheck-misra.sh` 的提示。
- 运行方式：pre-push 对改动的 `.c` 跑**独立模式**（无需构建、秒级，有已知假阳性，**仅告警**）；
  `check.ps1` / CI 用 **project 模式**（mps2/an386 的 `compile_commands.json`，准确，假阳性基本消除）。
  **CI 的 cppcheck + MISRA 已是必过硬门**（`CPPCHECK_FAIL=1`）；噪声抑制与 MISRA deviation 集中在 `.cppcheck-suppressions` 维护。

### clang-tidy

随 **LLVM** 发布：

```powershell
scoop install llvm                    # 或：winget install LLVM.LLVM
```

- 安装后需**新开终端 / Reload VS Code**，`clang-tidy` 才在 PATH 上。
- **Windows 原生对 Zephyr 不可靠**：clang-tidy 需 `native_sim`（提供 host flags，Windows 无法配置），
  且本地 LLVM 版本常比 CI 新、结果不一致。故 clang-tidy **以 CI（Linux）为准**，
  `check.ps1` 在 Windows 上会标 `SKIP`；要本地对齐请在 **WSL2**（Zephyr 即装在 WSL）下运行。
- CI clang-tidy 含 **CS-16 接口文档门**（`-Wdocumentation`，随 `WarningsAsErrors='*'` 硬门）。

## 四、API 文档（Doxygen → GitHub Pages）

公共接口（[app/include/bms/](app/include/bms/) 头文件）的 API 参考由 **Doxygen** 自动生成并发布到 GitHub Pages：

- **在线文档**：<https://jianshiyao.github.io/bms-app/>
- **自动发布**：`master` 上改动头文件 / `docs/Doxyfile` 时，[.github/workflows/docs.yml](.github/workflows/docs.yml) 自动构建并部署（也可手动 `workflow_dispatch`）。
- **本地预览**（需装 `doxygen` + `graphviz`，在仓库根执行）：
  ```powershell
  doxygen docs/Doxyfile          # 生成 doxygen-out/html/index.html
  ```
- 配置见 [docs/Doxyfile](docs/Doxyfile)（仅文档公共 API，C 模式，Graphviz 画数据结构/协作图）。

## 五、质量门禁与验证

分层门禁（编辑器 → pre-commit → pre-push → `check.ps1` → CI，**CI 为权威**）。完整说明见
[质量管控全景 docs/quality-management.md](docs/quality-management.md) 与
[验证策略与工具关键性分级 docs/quality-verification.md](docs/quality-verification.md)。

CI 必过门（[.github/workflows/ci.yml](.github/workflows/ci.yml)）：

- **格式**：clang-format（pin 22.1.5）
- **构建**：`mps2/an386` + `native_sim` + `qmxx_f407zg`
- **单测 + 覆盖率**：twister；门槛 **行 ≥ 60% / 分支 ≥ 38%**（阶梯上调；安全关键纯函数目标 100% 分支）
- **静态分析**：gcc `-fanalyzer`、clang-tidy（CERT/可读性 + CS-16 文档门）、**cppcheck + MISRA（硬门）**
- **测试存在性**：每个 `app/src/bms/<module>` 须有 `tests/bms/<module>`（否则在 `scripts/check-test-files.py` 登记豁免）
- **卫生**：editorconfig、yamllint

发布（[release.yml](.github/workflows/release.yml)，tag `vX.Y.Z`）：tag↔`VERSION` 校验 → 固件、`SHA256SUMS`、**SPDX SBOM**（`west spdx`）、**cosign keyless 签名**；版本（git describe）编进固件。

## 架构

详见 [docs/concept-architecture.md](docs/concept-architecture.md)。设计文档见 `docs/superpowers/specs/`。
