#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""测试存在性门禁：每个 BMS 业务模块都应有对应的 ztest 套件目录。

仿 foxBMS-2 的 check_test_files：凡 ``app/src/bms/<module>`` 下含 ``.c`` 源文件，
即要求存在 ``tests/bms/<module>/``；否则必须在下方 ``EXEMPT`` 显式登记（技术债，
附理由）。新增模块若既无测试、又未登记豁免，则本检查失败——逼迫"新代码带测试
或显式记债"，避免静默漏测（历史上 afe/balancing/comm 曾无单测）。

无第三方依赖，纯标准库。本地：``python scripts/check-test-files.py``；CI 同此。
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent  # bms-app/
SRC = ROOT / "app" / "src" / "bms"
TESTS = ROOT / "tests" / "bms"

# 已知缺测、显式豁免的模块（技术债登记；补上 tests/bms/<name> 后从此处删除）。
# 对齐 docs/quality-management.md「待补齐清单」。
EXEMPT = {
    "balancing": "纯函数 bms_balancing_compute 待补单测",
    "engine": "db/diag/task（集中调度/数据库/诊断）单测待补",
    "application": "bms 主状态机单测待补",
}


def main() -> int:
    if not SRC.is_dir():
        print(f"ERROR: source dir not found: {SRC}")
        return 1

    missing, exempt_used = [], []
    for mod_dir in sorted(p for p in SRC.iterdir() if p.is_dir()):
        if not any(mod_dir.glob("*.c")):
            continue  # 无 .c 的目录不算模块
        name = mod_dir.name
        if (TESTS / name).is_dir():
            continue
        (exempt_used if name in EXEMPT else missing).append(name)

    for name in exempt_used:
        print(f"WARN  exempt (no tests yet): {name}  -- {EXEMPT[name]}")
    for name in missing:
        print(f"FAIL  module without tests: app/src/bms/{name} -> expected tests/bms/{name}/")

    if missing:
        print(
            f"\n{len(missing)} module(s) lack tests and are not registered in EXEMPT.\n"
            "Add tests/bms/<module>/ or register the module in scripts/check-test-files.py "
            "with a reason."
        )
        return 1

    print(f"OK  test-presence check passed ({len(exempt_used)} registered exemption(s)).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
