# 构建指南（Build Guide）

> 本文是 BMS 固件的**构建专题权威文档**，面向团队开发者。覆盖从零到跑通的全链路：
> 工具链/workspace **环境搭建** → **构建**（增量/全新/多板）→ QEMU **运行** → 单元**测试**，
> 外加命令语法、构建产物、配置覆盖、清理与排错。
>
> **不在本文范围**：分支/提交/PR/发布等开发流程，以及 clang-format/clang-tidy/cppcheck 等
> **质量门禁工具的安装与使用** —— 见 [workflow.md](../process/workflow.md) 与
> [README.md](../../README.md)「代码格式化」「静态分析工具依赖」。各项约定的唯一来源是 [../CLAUDE.md](../../CLAUDE.md)。

---

## 0. 环境准备

包含两部分：**首次一次性**的工具链安装 + workspace 初始化（0.1 / 0.2），以及**每个新终端**都要做的 venv 激活（0.3）。
本文所有命令默认在**本仓库目录 `bms-app/`** 下、**已激活 venv** 的 PowerShell 中执行。

### 0.1 首次：安装工具链

> 官方指南：https://docs.zephyrproject.org/4.4.0/develop/getting_started/index.html

1. 安装 **Python 3.10–3.12**（勾选 Add to PATH），以及 **CMake ≥ 3.20**、**Ninja**、**Git**。
2. 创建虚拟环境并安装 west（venv 建在 **workspace 根**，即本仓库的上一级）：
   ```powershell
   # 在 workspace 根目录下
   python -m venv .venv
   .\.venv\Scripts\Activate.ps1
   pip install west
   ```
3. 安装 **Zephyr SDK**（版本由 `zephyr/SDK_VERSION` 决定，当前为 **1.0.1**）。
   需先完成 0.2 的 `west update` 才能用 west 自动安装：
   ```powershell
   # 仅装 ARM 工具链 + host-tools（含 QEMU），版本自动匹配 SDK_VERSION
   west sdk install --install-base D:\zephyr-sdk -t arm-zephyr-eabi
   west sdk list        # 确认 arm-zephyr-eabi 与 hosttools 已安装
   ```

### 0.2 首次：初始化 workspace（T2 拓扑）

在**本仓库目录 `bms-app/`** 下执行：

```powershell
west init -l .                              # 以本仓库为 manifest 初始化（workspace 根=上一级）
west update                                 # 拉取 zephyr v4.4.0 与所需模块
west zephyr-export                          # 导出 Zephyr CMake 包
pip install -r ..\zephyr\scripts\requirements.txt   # 安装 Zephyr 的 Python 依赖
```

### 0.3 每个新终端：激活 venv

`west`、`clang-format` 等都装在 workspace 根的 `.venv` 里。**每开一个新终端**、运行任何 `west` 命令前，先激活：

```powershell
# 在 bms-app/ 下
& ..\.venv\Scripts\Activate.ps1
west --version            # 验证：能打印版本即 OK
```

> 没激活会报 `west: The term 'west' is not recognized...`。

### 0.4 可选：在 WSL2 下编译 `native_sim`

`native_sim` 是 POSIX（host）架构，**Windows 原生编不了**，须在 **WSL2（Ubuntu）** 下跑。它编译快、跑 ztest 无需 QEMU，是本地复刻 CI 覆盖率口径的途径。

> WSL 是独立的 Linux 环境：Windows 侧的 `..\.venv`（Windows Python）在 WSL 里**用不了**，需在 WSL 内单独建 venv 装 west。下面命令在 **WSL 的 bash** 里执行（非 PowerShell）。

1. **确认构建依赖**（Ubuntu 通常已具备；缺啥按提示 `sudo apt install`）：
   ```bash
   for t in gcc cmake ninja dtc gperf git; do command -v $t || echo "MISSING: $t"; done
   cmake --version    # 需 ≥ 3.20
   ```
2. **建 WSL 专属 venv 装 west**（Ubuntu 24.04 有 PEP668，必须用 venv，勿直接 `pip install`）：
   ```bash
   python3 -m venv ~/.venv-zephyr
   source ~/.venv-zephyr/bin/activate          # 每个新 WSL 终端都要先激活
   pip install west
   pip install -r /mnt/d/__00_WorkSpace/__06_Study/bms-workspace/zephyr/scripts/requirements.txt
   ```
3. **Zephyr SDK**：放在 `~/zephyr-sdk-<ver>`（标准位置，会被自动发现）即可。`native_sim` 实际用**宿主 gcc**，SDK 仅 ARM 目标/gcov 用。
4. **构建并运行**（用独立 build 目录，避免与 Windows 的 `build/` 冲突）：
   ```bash
   cd /mnt/d/__00_WorkSpace/__06_Study/bms-workspace/bms-app
   west build -p always -b native_sim app -d build_native_wsl
   ./build_native_wsl/zephyr/zephyr.exe        # 直接运行（Ctrl-C 退出）；不需要 QEMU
   ```
5. **跑测试 / 覆盖率**（native_sim 下无 QEMU 超时困扰）：
   ```bash
   west twister -T tests -p native_sim -c
   ```

> ⚠️ **`/mnt/d` 跨盘编译走 9p 协议、较慢**；高频使用建议把工程 `git clone` 到 WSL 家目录（`~/`）的 Linux 文件系统再 `west init -l . && west update`，速度提升明显。

---

## 1. 目标板（board）一览

| board | 用途 | 架构 | Windows 原生可编译 | 说明 |
|-------|------|------|:--:|------|
| `mps2/an386` | **当前主力**（QEMU 仿真） | Cortex-M4F（带 FPU） | ✅ | 与目标 STM32F405 同核，架构忠实；可在 QEMU 直接 `run` |
| `native_sim` | Linux 下的快速本地仿真 + 覆盖率 | POSIX（host） | ❌ | POSIX 架构仅 Linux；Windows 须在 **WSL2** 下编译（步骤见 §0.4）。CI 用它跑测试/覆盖率 |
| `bms_f405` | **第二步**：自定义 STM32F405 真机 | Cortex-M4F | ✅（待完善后） | 位于 `boards/enervenue/bms_f405/`，当前为模板，dts/defconfig 待补 |

> 选 `mps2/an386` 而非 `native_sim` 作为 Windows 主力，是因为它与 STM32F405 同为 Cortex-M4F，
> 能真实暴露 FPU/对齐/中断等架构相关问题；`native_sim` 跑得快但架构不忠实。

---

## 2. 一条命令编译（最常用）

```powershell
west build -p always -b mps2/an386 app
```

逐段解释：

| 片段 | 含义 |
|------|------|
| `west build` | 调用 Zephyr 构建（内部 CMake + Ninja） |
| `-p always` | **pristine**：先清空 build 目录再全新配置+编译（见 §3） |
| `-b mps2/an386` | 目标 board |
| `app` | 源码目录（应用根，含 `CMakeLists.txt` / `prj.conf`） |

成功结尾会打印内存占用，例如：

```
Memory region     Used Size  Region Size  %age Used
       FLASH:       27872 B         4 MB      0.66%
         RAM:       14280 B         4 MB      0.34%
```

构建产物在 `build/zephyr/`（见 §6）。

---

## 3. 全新构建 vs 增量构建（`-p`）

`west build` 默认**增量**：只重编改动的文件，最快。改了 C 源码时直接：

```powershell
west build              # 复用上次的 board/源码目录，增量编译
```

什么时候需要 **pristine（`-p always`）全新构建**：

- 改了 `prj.conf` / `Kconfig` / `*.overlay` / `CMakeLists.txt`（配置类改动，增量可能不重新生成）
- 换了 board（不同 board 必须分目录或 pristine，见 §4）
- 构建出现莫名其妙的「上次残留」问题，先 pristine 排除

```powershell
west build -p always -b mps2/an386 app    # 强制全新（首次约 1–2 分钟，含整个 Zephyr）
west build -p auto   -b mps2/an386 app    # 让 west 自行判断是否需要 pristine（折中）
```

> 经验：**改代码用增量，改配置/换板用 `-p always`**。不确定就 `-p always`，安全但慢。

---

## 4. 多板并存：用 `-d` 指定独立 build 目录

默认 build 目录是 `build/`，一次只能装一个 board。要同时保留多板产物（或避免反复 pristine），用 `-d`：

```powershell
west build -b mps2/an386 app -d build\an386      # M4F/QEMU
west build -b native_sim  app -d build\nsim       # 仅 WSL2/Linux
west build -b bms_f405    app -d build\f405       # 真机（待板完善）
```

之后对某个目录增量编译时也要带上 `-d`：

```powershell
west build -d build\an386      # 继续增量编译 an386 那份
```

> `scripts\check.ps1` 正是用 `build\check-an386` / `build\check-nsim` 等独立目录并行镜像 CI（见 §9）。

---

## 5. 在 QEMU 中运行（仅 `mps2/an386`）

```powershell
west build -t run        # 复用当前 build/，在 QEMU 启动；Ctrl-A 然后 X 退出
```

预期启动日志：

```
*** Booting Zephyr OS build v4.4.0 ***
bms_main: ==== BMS firmware starting on mps2/an386 ====
... 各模块 init（afe/soc/protection/balancing/comm）...
（之后每 5s 一次心跳）
```

上列模块（`afe/soc/protection/balancing/comm`）为**当前过渡实现**；目标 engine core 分层见 [../concept/architecture.md](../concept/architecture.md) §3。

> `-t run` 是「构建目标（target）」而非编译；它会先确保已编译再拉起 QEMU。
> 若用了 `-d`，运行也要带：`west build -t run -d build\an386`。

---

## 6. 构建产物与目录结构

全新构建后 `build/` 关键内容：

```
build/
├─ zephyr/
│  ├─ zephyr.elf        # ★ 主产物：带符号的可执行（QEMU run / 调试 / gdb 用）
│  ├─ zephyr.bin        # 裸二进制（烧录真机用）
│  ├─ zephyr.hex        # Intel HEX（部分烧录器用；视 board 而定）
│  ├─ zephyr.map        # 链接 map（排查体积/符号位置）
│  └─ .config           # 本次构建最终生效的 Kconfig（所有 CONFIG_* 的真值来源）
├─ compile_commands.json  # 编译数据库（带 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON 时生成；clangd/cppcheck 用）
└─ CMakeCache.txt         # CMake 缓存（记录 board 等；删它≈轻量 pristine）
```

排查「我的配置到底有没有生效」时，**看 `build/zephyr/.config`** 是最可靠的——它是 `prj.conf` + board defconfig + 命令行覆盖合并后的最终结果。

---

## 7. 覆盖/自定义构建配置

配置优先级（低 → 高）：board defconfig → `app/prj.conf` → `*.overlay`/额外 conf → 命令行 `-D`。

```powershell
# 交互式调配置（改完保存，会写入 build 的 .config；持久化请落到 prj.conf）
west build -t menuconfig

# 命令行临时覆盖一个 Kconfig（-- 之后的参数原样传给 CMake）
west build -p always -b mps2/an386 app -- -DCONFIG_BMS_BALANCING=n

# 叠加额外的 conf 片段（不改 prj.conf 的前提下试配置）
west build -b mps2/an386 app -- -DEXTRA_CONF_FILE=debug.conf

# 生成编译数据库（给 clangd / cppcheck project 模式用）
west build -b mps2/an386 app -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

> BMS 关键 Kconfig（`BMS_CELL_COUNT`、各 `BMS_*_PERIOD_MS`、SOC 容量等）默认值见 `app/prj.conf` 与 `app/Kconfig`，
> 含义见 [../CLAUDE.md](../../CLAUDE.md) 的「Kconfig 关键项」。永久改动写进 `prj.conf`，**别只在 menuconfig 里改**（不持久）。

---

## 8. 清理

```powershell
# 最干净：删整个 build 目录
Remove-Item -Recurse -Force build

# 等价于下次构建加 -p always（不手动删目录）
west build -p always -b mps2/an386 app

# 多目录时按需删
Remove-Item -Recurse -Force build\an386, build\nsim
```

> `build/` 已在 `.gitignore`，不会误提交。磁盘紧张时整份删掉即可，下次构建自动重建（首次较慢）。

---

## 9. 测试构建与运行（ztest / twister）

单元测试用 **twister** 跑，不要用裸 `west build`：

```powershell
# Windows 必须先设 QEMU_BIN_PATH，否则 twister 只编译不运行（显示 "built (not run)"）
$env:QEMU_BIN_PATH = "D:\zephyr-sdk\zephyr-sdk-1.0.1\hosttools\qemu"
west twister -T tests -p mps2/an386 -c        # 预期 47/47 通过
```

- `QEMU_BIN_PATH` 指向 SDK 自带 QEMU 目录（每个新终端都要设；也可在系统环境变量里永久设置）。
- 测试规模不在文档写死，以 `west twister` / CI 报告为准（CLAUDE.md §3）。
- ⚠️ **QEMU soc 超时 flake**：`bms.soc` 套件在 QEMU 下较易触发 harness 超时（用例本身全过），
  按需加 `--timeout-multiplier 4`；CI 走 `native_sim` 不受此限。
- 跑单个套件：限定 `-T` 路径，如 `west twister -T tests/bms/soc -p mps2/an386 -c`。
- 覆盖率：用 workspace 根的 `..\run-tests-coverage.ps1`（QEMU 覆盖率不稳，可靠覆盖率见 CI / WSL2+native_sim）。

**提交前本地全量镜像 CI**（format→build×2→test→SCA→tidy→cppcheck）：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\check.ps1
powershell -ExecutionPolicy Bypass -File scripts\check.ps1 -Fast   # 仅 format+build+test（跳过两个重门）
```

> 质量门禁的分层触发、各门含义、工具安装见 [workflow.md](../process/workflow.md) 与
> [README.md](../../README.md)「静态分析工具依赖」。

---

## 10. 常见错误排查（Troubleshooting）

| 现象 | 原因 | 处理 |
|------|------|------|
| `west: ... is not recognized` | 没激活 venv | `& ..\.venv\Scripts\Activate.ps1`（见 §0） |
| `ZEPHYR_BASE ... not found` / 找不到 zephyr | workspace 未初始化或 `west update` 没跑 | 见 §0.2 初始化 workspace；确认上一级有 `zephyr/` |
| 找不到 SDK / 工具链 / `arm-zephyr-eabi-gcc` | Zephyr SDK 未装或未被发现 | `west sdk list` 确认；缺则按 §0.1 装 SDK（`west sdk install ...`） |
| 改了 `prj.conf`/`Kconfig` 但行为没变 | 增量构建未重新配置 | 用 `-p always` 全新构建；核对 `build/zephyr/.config` |
| twister 显示 `built (not run)`，测试没真正执行 | Windows 没设 `QEMU_BIN_PATH` | 设环境变量后重跑（§9） |
| `native_sim` 在 Windows 上配置/编译失败 | POSIX 架构不支持 Windows 原生 | 改用 `mps2/an386`，或在 **WSL2** 下编译 `native_sim`（步骤见 §0.4） |
| 构建中途换了 board 报 board 不一致 | 同一 build 目录混用多个 board | `-p always` 重建，或用 `-d` 分目录（§4） |
| 链接报 region overflow（FLASH/RAM 不够） | 配置开太多/栈太大 | 看 `zephyr.map` 与 `.config`；关无关 `CONFIG_BMS_*` 或调小栈 |
| 编译卡很久 | 首次/`-p always` 要编整个 Zephyr（约 1–2 分钟） | 正常；日常改代码用增量 `west build` |

排查仍无果时：删 `build/` 全新构建（§8）排除残留，再贴**完整报错首段**（CMake/Ninja 的第一条 error 才是根因）。

---

## 11. 速查表（Cheat Sheet）

```powershell
# --- 准备（每个新终端一次）---
& ..\.venv\Scripts\Activate.ps1

# --- 日常 ---
west build -p always -b mps2/an386 app     # 全新构建（换板/改配置/首次）
west build                                 # 增量构建（只改了 C 代码）
west build -t run                          # QEMU 运行（Ctrl-A X 退出）

# --- 多板（独立目录）---
west build -b mps2/an386 app -d build\an386
west build -b native_sim  app -d build\nsim     # 仅 WSL2/Linux

# --- 测试 / 门禁 ---
$env:QEMU_BIN_PATH = "D:\zephyr-sdk\zephyr-sdk-1.0.1\hosttools\qemu"
west twister -T tests -p mps2/an386 -c
powershell -ExecutionPolicy Bypass -File scripts\check.ps1 -Fast

# --- 配置 / 清理 ---
west build -t menuconfig
Remove-Item -Recurse -Force build
```

---

> 当前版本：`VERSION` = **0.1.1**（SemVer 0.x）。本文与 README/CLAUDE.md 的命令保持一致；
> 若发现不一致，以本仓库 [CLAUDE.md](../../CLAUDE.md)（唯一来源）为准并同步本文。
