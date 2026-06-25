#!/bin/sh
# cppcheck-run.sh -- run cppcheck (general checks + MISRA addon). Single source
# of truth for cppcheck flags; called by both scripts/hooks/pre-push and
# scripts/check.ps1. Two modes (two layers of the quality net):
#
#   cppcheck-run.sh <file.c> [file.c ...]               # STANDALONE (fast)
#   cppcheck-run.sh --project <compile_commands.json>   # PROJECT (accurate)
#
# STANDALONE has no build context (no -I/-D from the build), so on Zephyr code
# it yields systematic false positives (implicit-decl 17.3, unused-return 17.7,
# misra-config). It is the fast pre-push "rough pass". PROJECT mode reuses a
# compile_commands.json (e.g. the one check.ps1/CI build for clang-tidy) so
# cppcheck sees real includes/macros -> accurate MISRA with few false positives.
#
# Policy: WARN-ONLY by default (prints findings, exits 0). Once the noise is
# tuned (suppressions/baseline), set CPPCHECK_FAIL=1 to make findings fail.
#
# Requires: cppcheck (PATH or C:/Program Files/Cppcheck) + a Python interpreter
#           for the MISRA addon (see scripts/setup-cppcheck-misra.sh).

MODE="files"
PROJECT=""
if [ "${1:-}" = "--project" ]; then
    MODE="project"
    PROJECT="${2:-}"
    [ -z "$PROJECT" ] && { echo "cppcheck-run: --project requires a path." >&2; exit 2; }
    if [ ! -f "$PROJECT" ]; then
        echo "cppcheck-run: compile db not found: $PROJECT; skipping." >&2
        exit 0
    fi
elif [ "$#" -eq 0 ]; then
    echo "cppcheck-run: no files; nothing to do."
    exit 0
fi

# Locate cppcheck: PATH first, then the standard Windows install dir (the MSI /
# winget package installs here but does not always refresh PATH for open shells).
CC="$(command -v cppcheck 2>/dev/null)"
if [ -z "$CC" ] && [ -x "/c/Program Files/Cppcheck/cppcheck.exe" ]; then
    CC="/c/Program Files/Cppcheck/cppcheck.exe"
fi
if [ -z "$CC" ]; then
    echo "cppcheck-run: cppcheck not found (PATH or C:/Program Files/Cppcheck); skipping." >&2
    echo "cppcheck-run: install via 'winget install Cppcheck.Cppcheck' (and ensure python is on PATH for MISRA)." >&2
    exit 0
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"   # bms-app root (this script is in scripts/)

# Project-wide suppression list (include-resolution noise + accepted deviations).
SUPPR_ARG=""
[ -f "$ROOT/.cppcheck-suppressions" ] && SUPPR_ARG="--suppressions-list=$ROOT/.cppcheck-suppressions"

# --- MISRA addon ----------------------------------------------------------
# The Windows cppcheck install does NOT bundle the Python addons, so misra.py
# must come from a local copy. Look in: $CPPCHECK_MISRA, our gitignored addon
# dir (populated by scripts/setup-cppcheck-misra.sh), then any addons/ beside
# the cppcheck binary. The addon also needs a python interpreter -- if none is
# on PATH, fall back to the workspace venv so cppcheck can run misra.py.
ADDON=""
MISRA=""
for cand in "${CPPCHECK_MISRA:-}" "$ROOT/scripts/.cppcheck-addons/misra.py" "$(dirname "$CC")/addons/misra.py"; do
    if [ -n "$cand" ] && [ -f "$cand" ]; then MISRA="$cand"; break; fi
done

# Optional MISRA rule-texts -> print rule descriptions instead of bare IDs.
# The text is MISRA copyright (CC BY-NC-ND); keep it LOCAL & gitignored, NEVER
# commit it. See scripts/setup-cppcheck-misra.sh for how to provide it.
RULETEXTS=""
for rt in "${CPPCHECK_MISRA_RULETEXTS:-}" "$ROOT/scripts/.cppcheck-addons/misra-rule-texts.txt"; do
    if [ -n "$rt" ] && [ -f "$rt" ]; then RULETEXTS="$rt"; break; fi
done

if [ -n "$MISRA" ]; then
    if ! command -v python >/dev/null 2>&1 && ! command -v python3 >/dev/null 2>&1; then
        VENV_SCRIPTS="/d/__00_WorkSpace/__06_Study/bms-workspace/.venv/Scripts"
        [ -x "$VENV_SCRIPTS/python.exe" ] && PATH="$VENV_SCRIPTS:$PATH"
    fi
    if command -v python >/dev/null 2>&1 || command -v python3 >/dev/null 2>&1; then
        if [ -n "$RULETEXTS" ]; then
            # cppcheck passes addon args only via an addon-config JSON. Its paths
            # are read by cppcheck (not MSYS-translated), so convert to native.
            _w() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else printf '%s' "$1"; fi; }
            ADDON_JSON="$ROOT/scripts/.cppcheck-addons/misra-addon.json"
            printf '{"script":"%s","args":["--rule-texts=%s"]}\n' \
                "$(_w "$MISRA")" "$(_w "$RULETEXTS")" > "$ADDON_JSON"
            ADDON="--addon=$ADDON_JSON"
        else
            ADDON="--addon=$MISRA"
        fi
    else
        echo "cppcheck-run: python not found for MISRA addon -- running cppcheck general checks only." >&2
    fi
else
    echo "cppcheck-run: MISRA addon (misra.py) not found -- running cppcheck general checks only." >&2
    echo "cppcheck-run: enable MISRA with: sh scripts/setup-cppcheck-misra.sh" >&2
fi

# --enable: 'style' is REQUIRED -- MISRA addon violations are emitted at style
#   severity, so without it cppcheck silently drops every MISRA finding. We skip
#   only 'information'/'unusedFunction' (need whole-program; noisy per-file).
#   Tune cppcheck's own style noise via .cppcheck-suppressions, not by dropping style.
# --addon: MISRA C:2012. Reports rule IDs; with a local rule-texts file (see
#   above) it also prints rule descriptions. Rule text is MISRA copyright.
# --inline-suppr: honour '// cppcheck-suppress <id>' annotations in code.
# Note: $ADDON / $SUPPR_ARG are intentionally unquoted (token-split); the paths
# they hold contain no spaces (repo + venv live under space-free directories).
if [ "$MODE" = "project" ]; then
    # The compile db supplies -I/-D/--std. The Zephyr build db also contains
    # kernel files we do not own, so scope analysis to our app sources.
    out="$("$CC" \
        --enable=warning,style,performance,portability \
        $ADDON \
        --inline-suppr \
        $SUPPR_ARG \
        --project="$PROJECT" \
        --file-filter='*/app/*' \
        --template=gcc \
        --quiet 2>&1)"
else
    # Standalone: no build context. -I app/include resolves our own headers;
    # Zephyr headers/macros stay unresolved (hence the known 17.3/17.7 noise).
    out="$("$CC" \
        --enable=warning,style,performance,portability \
        $ADDON \
        --std=c11 --language=c \
        --inline-suppr \
        -I "$ROOT/app/include" \
        $SUPPR_ARG \
        --template=gcc \
        --quiet \
        "$@" 2>&1)"
fi

[ -n "$out" ] && printf '%s\n' "$out"

# Count real findings (severity-tagged lines); MISRA addon reports as 'style'.
n="$(printf '%s\n' "$out" | grep -Ec ': (error|warning|style|performance|portability):')"
[ -z "$n" ] && n=0

if [ "$n" -gt 0 ]; then
    echo ""
    echo "cppcheck-run: $n finding(s) above (cppcheck + MISRA)."
    if [ "${CPPCHECK_FAIL:-0}" = "1" ]; then
        echo "cppcheck-run: CPPCHECK_FAIL=1 -> failing."
        exit 1
    fi
    echo "cppcheck-run: WARN-ONLY (non-blocking). Set CPPCHECK_FAIL=1 to enforce once tuned."
else
    echo "cppcheck-run: 0 findings."
fi
exit 0
