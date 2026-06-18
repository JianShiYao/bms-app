#!/usr/bin/env python3
"""App-scoped coverage gate.

Reads the gcovr JSON report that twister generates under twister-out/ and computes
line/branch coverage for THIS project's own code only (app/src, app/include) — the
whole-tree number is meaningless because it's dominated by uncovered Zephyr kernel.

Usage:
  coverage-gate.py [--report PATH] [--min-line PCT] [--min-branch PCT]

Exit 1 if coverage is below a threshold. With the default thresholds (0) it only
reports, which is how you discover the baseline before setting a real threshold.
"""
import argparse
import glob
import json
import sys


def find_report(explicit):
    if explicit:
        return explicit
    cands = glob.glob("twister-out/**/coverage.json", recursive=True)
    cands += glob.glob("twister-out/coverage.json")
    return cands[0] if cands else None


def is_app_file(path):
    p = path.replace("\\", "/")
    return "/app/src/" in p or "/app/include/" in p \
        or p.startswith("app/src/") or p.startswith("app/include/")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--report")
    ap.add_argument("--min-line", type=float, default=0.0)
    ap.add_argument("--min-branch", type=float, default=0.0)
    args = ap.parse_args()

    report = find_report(args.report)
    if not report:
        print("::error::no gcovr coverage.json found under twister-out/")
        return 2
    with open(report) as fh:
        data = json.load(fh)

    line_tot = line_cov = br_tot = br_cov = 0
    files = []
    for f in data.get("files", []):
        if not is_app_file(f.get("file", "")):
            continue
        lt = lc = bt = bc = 0
        for ln in f.get("lines", []):
            lt += 1
            if ln.get("count", 0) > 0:
                lc += 1
            for br in ln.get("branches", []):
                bt += 1
                if br.get("count", 0) > 0:
                    bc += 1
        files.append((f["file"], lc, lt))
        line_tot += lt
        line_cov += lc
        br_tot += bt
        br_cov += bc

    if line_tot == 0:
        all_files = [f.get("file", "") for f in data.get("files", [])]
        print(f"WARNING: no app files matched. report has {len(all_files)} files; sample:")
        for p in all_files[:20]:
            print(f"  {p}")
        # In report-only mode (no thresholds) don't fail — this surfaces the path
        # format so the filter can be fixed. With a real threshold set, this is fatal.
        if args.min_line == 0.0 and args.min_branch == 0.0:
            return 0
        print("::error::no app files found in coverage report (check filter / paths)")
        return 2

    line_pct = 100.0 * line_cov / line_tot
    br_pct = (100.0 * br_cov / br_tot) if br_tot else 100.0

    print(f"report: {report}")
    for name, lc, lt in sorted(files):
        print(f"  {lc:4d}/{lt:<4d}  {name}")
    print(f"App line coverage:   {line_cov}/{line_tot} = {line_pct:.1f}%  (min {args.min_line}%)")
    print(f"App branch coverage: {br_cov}/{br_tot} = {br_pct:.1f}%  (min {args.min_branch}%)")

    failed = False
    if line_pct + 1e-9 < args.min_line:
        print(f"::error::app line coverage {line_pct:.1f}% below threshold {args.min_line}%")
        failed = True
    if br_pct + 1e-9 < args.min_branch:
        print(f"::error::app branch coverage {br_pct:.1f}% below threshold {args.min_branch}%")
        failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
