# 构建指南（Build Guide）

> 本文是 BMS 固件的**构建专题权威文档**，面向团队开发者。聚焦「如何编译」的全部细节：
> 命令语法、多板构建、增量 vs 全新、构建产物、配置覆盖、清理与排错。
>
> **不在本文范围**：工具链/SDK 安装、workspace 首次初始化 —— 见 [README.md](../README.md) 的「一、安装工具链」「二、初始化 workspace」。
> 分支/提交/PR/质量门禁流程见 [development-workflow.md](development-workflow.md)。

---

## 0. 前置：每个新终端都要激活 venv

`west`、`clang-format` 等都装在 workspace 根的 `.venv` 里。**每开一个新终端**、运行任何 `west` 命令前，先激活：

```powershell
# 在 bms-app/ 下
& ..\.venv\Scripts\Activate.ps1
west --version            # 验证：能打印版本即 OK
```

> 没激活会报 `west: The term 'west' is not recognized...`。

本文所有命令默认在**本仓库目录 `bms-app/`** 下、**已激活 venv** 的 PowerShell 中执行。

---

## 1. 目标板（board）一览

| board | 用途 | 架构 | Windows 原生可编译 | 说明 |
|-------|------|------|:--:|------|
| `mps2/an386` | **当前主力**（QEMU 仿真） | Cortex-M4F（带 FPU） | ✅ | 与目标 STM32F405 同核，架构忠实；可在 QEMU 直接 `run` |
| `native_sim` | Linux 下的快速本地仿真 + 覆盖率 | POSIX（host） | ❌ | POSIX 架构仅 Linux；Windows 须在 **WSL2** 下编译。CI 用它跑测试/覆盖率 |
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
> 含义见 [../CLAUDE.md](../CLAUDE.md) 的「Kconfig 关键项」。永久改动写进 `prj.conf`，**别只在 menuconfig 里改**（不持久）。

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

## 9. 测试构建与质量门禁构建

这两类「构建」有专属入口，不要用裸 `west build`：

```powershell
# 单元测试（twister）：Windows 必须先设 QEMU_BIN_PATH，否则只编译不运行
$env:QEMU_BIN_PATH = "D:\zephyr-sdk\zephyr-sdk-1.0.1\hosttools\qemu"
west twister -T tests -p mps2/an386 -c        # 预期 11/11 通过

# 提交前本地全量镜像 CI（format→build×2→test→SCA→tidy→cppcheck）
powershell -ExecutionPolicy Bypass -File scripts\check.ps1
powershell -ExecutionPolicy Bypass -File scripts\check.ps1 -Fast   # 仅 format+build+test（跳过两个重门）
```

细节见 [README.md](../README.md)「四/六/七」与 [development-workflow.md](development-workflow.md)。

---

## 10. 常见错误排查（Troubleshooting）

| 现象 | 原因 | 处理 |
|------|------|------|
| `west: ... is not recognized` | 没激活 venv | `& ..\.venv\Scripts\Activate.ps1`（见 §0） |
| `ZEPHYR_BASE ... not found` / 找不到 zephyr | workspace 未初始化或 `west update` 没跑 | 见 README「二、初始化 workspace」；确认上一级有 `zephyr/` |
| 找不到 SDK / 工具链 / `arm-zephyr-eabi-gcc` | Zephyr SDK 未装或未被发现 | `west sdk list` 确认；缺则按 README「一」装 SDK（`west sdk install ...`） |
| 改了 `prj.conf`/`Kconfig` 但行为没变 | 增量构建未重新配置 | 用 `-p always` 全新构建；核对 `build/zephyr/.config` |
| twister 显示 `built (not run)`，测试没真正执行 | Windows 没设 `QEMU_BIN_PATH` | 设环境变量后重跑（§9） |
| `native_sim` 在 Windows 上配置/编译失败 | POSIX 架构不支持 Windows 原生 | 改用 `mps2/an386`，或在 **WSL2** 下编译 `native_sim` |
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
> 若发现不一致，以本仓库 [CLAUDE.md](../CLAUDE.md)（唯一来源）为准并同步本文。
