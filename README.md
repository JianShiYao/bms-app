# BMS 固件项目（Zephyr v4.4.0）

基于 **Zephyr RTOS v4.4.0** 的电池管理系统（BMS）固件。采用 west **T2 拓扑**（本仓库即 manifest 仓库），分层架构 + zbus 消息总线解耦。

> 参与开发请先读 **[开发流程与质量审查](docs/development-workflow.md)**（分支模型 / 提交规范 / PR 流程 / 质量门禁 / 发布）；
> Git 操作细则、命令示例与出错恢复见 **[Git 管理制度与操作手册](docs/git-guide.md)**。

- 第一步：在 PC 上用 **QEMU（`mps2/an386`，Cortex-M4F）** 跑通架构与业务骨架。
  - 选 `mps2/an386` 是因为它与目标 STM32F405 同为 **Cortex-M4F（带硬件 FPU）**，架构忠实。
  - 注：`native_sim` 的 POSIX 架构仅支持 Linux（见 `zephyr/arch/posix/CMakeLists.txt`），
    Windows 上无法编译；如需 native_sim 请在 WSL2/Linux 下使用。
- 第二步：接入自定义 STM32F405 板（`boards/enervenue/bms_f405/`）。

## 目录结构

```
bms-app/                     # 本仓库（west manifest 仓库）
├─ west.yml                  # manifest：pin zephyr v4.4.0 + HAL
├─ app/                      # BMS 应用
│  ├─ include/bms/           # 模块公共接口（头文件）
│  └─ src/bms/{afe,soc,protection,balancing,comm}/  # 模块桩实现
├─ boards/enervenue/bms_f405/   # 第二步：自定义 STM32F405 板（模板）
├─ drivers/                  # out-of-tree 驱动占位（如 AFE 芯片）
├─ tests/bms/{soc,protection,afe}/  # ztest 单测套件
└─ docs/architecture.md      # 架构与数据流说明
```

## 一、环境搭建 / 构建 / 运行 / 测试

> **完整步骤见 [构建指南 docs/build-guide.md](docs/build-guide.md)** —— 工具链与 SDK 安装、
> workspace 初始化、构建（增量/全新/多板）、QEMU 运行、单元测试、构建产物、配置覆盖、清理、排错速查。
>
> 首次搭建（装工具链 / `west init` + `west update` / 装 SDK）务必照
> [构建指南 §0 环境准备](docs/build-guide.md) 一步步来。下面是跑通后的常用命令速览：

```powershell
& ..\.venv\Scripts\Activate.ps1                       # 每个新终端先激活 venv
west build -p always -b mps2/an386 app                # 构建（首次约 1–2 分钟）
west build -t run                                     # QEMU 运行（Ctrl-A X 退出）

$env:QEMU_BIN_PATH = "D:\zephyr-sdk\zephyr-sdk-1.0.1\hosttools\qemu"
west twister -T tests -p mps2/an386 -c               # 单元测试（预期 47/47）
```

> 运行后启动日志示例：`*** Booting Zephyr OS build v4.4.0 ***` →
> `bms_main: ==== BMS firmware starting on mps2/an386 ====` → 各模块 init → 每 5s 心跳。
> STM32F405 真机板（`bms_f405`）待 dts/defconfig 完善后用 `west build -b bms_f405 app`。

## 二、代码格式化与提交检查

代码风格沿用 **Zephyr 官方 clang-format**（配置见仓库根 `.clang-format`）。

**克隆后首次** 需激活本地 git 提交钩子（`core.hooksPath` 是仓库本地配置，不随提交携带）：

```powershell
# 在本仓库目录下执行一次
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
> 注：`git config core.hooksPath scripts/hooks` 同时启用 **pre-commit**（格式）与 **pre-push**
> （format + 增量 clang-tidy + cppcheck/MISRA 告警）；后两者依赖下节的工具，未装则自动跳过。

## 三、静态分析工具依赖（cppcheck / clang-tidy）

pre-push 钩子与 `scripts\check.ps1` 会调用 **cppcheck**（含 MISRA）与 **clang-tidy**。
两者均为**可选本地依赖**：未安装时对应检查自动标 `SKIP`（不阻断本地操作），**CI 会权威地补跑**。
分层触发与各门细节见 [开发流程与质量审查](docs/development-workflow.md)。

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
- 运行方式：pre-push 对改动的 `.c` 跑**独立模式**（无需构建、秒级，但无构建上下文会有已知假阳性，**仅告警**）；
  `check.ps1` 用 **mps2/an386 的 `compile_commands.json`** 跑 **project 模式**（准确，假阳性基本消除）。
- 噪声抑制与 MISRA deviation 集中在 `.cppcheck-suppressions` 维护。

### clang-tidy

随 **LLVM** 发布：

```powershell
scoop install llvm                    # 或：winget install LLVM.LLVM
```

- 安装后需**新开终端 / Reload VS Code**，`clang-tidy` 才在 PATH 上。
- **Windows 原生对 Zephyr 不可靠**：clang-tidy 需 `native_sim`（提供 host flags，Windows 无法配置），
  且本地 LLVM 版本常比 CI 新、结果不一致。故 clang-tidy **以 CI（Linux）为准**，
  `check.ps1` 在 Windows 上会标 `SKIP`；要本地对齐请在 **WSL2**（Zephyr 即装在 WSL）下运行。

## 架构

详见 [docs/architecture.md](docs/architecture.md)。设计文档见 `docs/superpowers/specs/`。
