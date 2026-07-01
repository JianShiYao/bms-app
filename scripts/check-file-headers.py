#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Lightweight file-header hygiene check (coding-style C:002 + C:004).

Mirrors foxBMS 2's pre-commit header check, scoped to this project's own C
code. For every ``.c``/``.h`` under the given roots (default: ``app``) it
verifies:

  * C:002 - the first line is the SPDX license identifier
            ``/* SPDX-License-Identifier: Apache-2.0 */``.
  * C:004 - a file-level doxygen block exists with:
              - ``@file <name>`` whose argument equals the actual file name,
              - ``@brief`` (non-empty), and
              - ``@ingroup <DOMAIN>`` with DOMAIN in the allowed module-domain
                set (kept in sync with docs/work/traceability.md / CLAUDE.md).

Scope is intentionally ``app/`` only: tests/ and drivers/ follow different
conventions (see docs/standard/coding-style.md). Pass explicit roots to widen it.

Exit code: 0 if all files pass, 1 otherwise. Pure stdlib; runs identically on
Windows (check.ps1) and Linux (CI).
"""
from __future__ import annotations

import os
import re
import sys

# Module domains, kept in sync with CLAUDE.md / docs/work/traceability.md.
ALLOWED_INGROUPS = {"SYS", "AFE", "SOC", "PROT", "BAL", "COMM", "BOARD"}

SPDX_FIRST_LINE = "/* SPDX-License-Identifier: Apache-2.0 */"

FILE_RE = re.compile(r"@file\s+(\S+)")
BRIEF_RE = re.compile(r"@brief\s+(\S.*\S|\S)")
INGROUP_RE = re.compile(r"@ingroup\s+(\S+)")


def check_file(path: str) -> list[str]:
    """Return a list of human-readable problems for one file (empty == ok)."""
    problems: list[str] = []
    with open(path, encoding="utf-8") as fh:
        text = fh.read()

    lines = text.splitlines()
    first = lines[0] if lines else ""
    if first.rstrip() != SPDX_FIRST_LINE:
        problems.append(f"first line must be '{SPDX_FIRST_LINE}' (C:002), got: {first!r}")

    m_file = FILE_RE.search(text)
    if not m_file:
        problems.append("missing '@file' (C:004)")
    else:
        basename = os.path.basename(path)
        if m_file.group(1) != basename:
            problems.append(f"@file is '{m_file.group(1)}' but file name is '{basename}' (C:004)")

    if not BRIEF_RE.search(text):
        problems.append("missing or empty '@brief' (C:004)")

    m_ingroup = INGROUP_RE.search(text)
    if not m_ingroup:
        problems.append("missing '@ingroup' (C:004)")
    elif m_ingroup.group(1) not in ALLOWED_INGROUPS:
        allowed = ", ".join(sorted(ALLOWED_INGROUPS))
        problems.append(f"@ingroup '{m_ingroup.group(1)}' not in allowed domains: {allowed} (C:004)")

    return problems


def collect_files(roots: list[str]) -> list[str]:
    found: list[str] = []
    for root in roots:
        if os.path.isfile(root):
            found.append(root)
            continue
        for dirpath, _dirs, names in os.walk(root):
            for name in names:
                if name.endswith((".c", ".h")):
                    found.append(os.path.join(dirpath, name))
    return sorted(found)


def main(argv: list[str]) -> int:
    roots = argv[1:] or ["app"]
    files = collect_files(roots)
    if not files:
        print(f"check-file-headers: no .c/.h files under {roots}")
        return 0

    failed = 0
    for path in files:
        rel = path.replace(os.sep, "/")
        problems = check_file(path)
        if problems:
            failed += 1
            for problem in problems:
                print(f"{rel}: {problem}")

    total = len(files)
    if failed:
        print(f"\ncheck-file-headers: {failed}/{total} file(s) FAILED the header check")
        return 1
    print(f"check-file-headers: {total} file(s) OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
