#!/bin/sh
# setup-cppcheck-misra.sh -- fetch the cppcheck MISRA addon (misra.py +
# cppcheckdata.py) into a LOCAL, gitignored dir so scripts/cppcheck-run.sh can
# run MISRA C:2012 checks. The Windows cppcheck installer does NOT ship the
# Python addons, so they must be obtained separately.
#
# These files are part of cppcheck (GPLv3); we deliberately do NOT commit them.
# They are downloaded per-machine into scripts/.cppcheck-addons/ (gitignored),
# pinned to the installed cppcheck version so the addon matches the binary.
#
# Usage: sh scripts/setup-cppcheck-misra.sh [version]
#        version defaults to the installed cppcheck's version (e.g. 2.21.0).

set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/scripts/.cppcheck-addons"

# Resolve cppcheck (PATH, then standard Windows install) to read its version.
CC="$(command -v cppcheck 2>/dev/null || true)"
if [ -z "$CC" ] && [ -x "/c/Program Files/Cppcheck/cppcheck.exe" ]; then
    CC="/c/Program Files/Cppcheck/cppcheck.exe"
fi

ver="${1:-}"
if [ -z "$ver" ] && [ -n "$CC" ]; then
    ver="$("$CC" --version 2>/dev/null | sed -E 's/[^0-9]*([0-9]+\.[0-9]+(\.[0-9]+)?).*/\1/')"
fi
[ -z "$ver" ] && ver="2.21.0"

base="https://raw.githubusercontent.com/danmar/cppcheck/$ver/addons"
mkdir -p "$DEST"

fetch() {
    _url="$1"; _out="$2"
    echo "downloading $_url"
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "$_url" -o "$_out"
    elif command -v wget >/dev/null 2>&1; then
        wget -qO "$_out" "$_url"
    else
        echo "setup: need curl or wget on PATH" >&2
        exit 1
    fi
}

# misra.py imports cppcheckdata + misra_9; all must sit in the same dir.
fetch "$base/cppcheckdata.py" "$DEST/cppcheckdata.py"
fetch "$base/misra_9.py"      "$DEST/misra_9.py"
fetch "$base/misra.py"        "$DEST/misra.py"

echo ""
echo "OK: MISRA addon installed in scripts/.cppcheck-addons/ (cppcheck $ver)."
echo "verify: sh scripts/cppcheck-run.sh app/src/main.c"

# --- Optional: rule-texts (print rule descriptions, not just IDs) ----------
RULETEXTS="$DEST/misra-rule-texts.txt"
if [ -f "$RULETEXTS" ]; then
    echo "rule-texts: present -> cppcheck-run.sh will print MISRA descriptions."
else
    echo ""
    echo "NOTE: MISRA rule text is copyright and is NOT shipped/downloaded here."
    echo "      Without it cppcheck prints rule IDs only (e.g. misra-c2012-13.4)."
    echo "      To also print descriptions, place a rule-texts file at:"
    echo "          $RULETEXTS"
    echo "      Format = cppcheck '--rule-texts' (the 'Appendix A Summary of"
    echo "      guidelines' layout). Sources (pick one you are licensed for):"
    echo "        * MISRA's official cppcheck headlines file (CC BY-NC-ND 4.0;"
    echo "          NonCommercial -- a commercial project may need MISRA licensing)."
    echo "        * Appendix A of your licensed MISRA C:2012 PDF."
    echo "      It stays LOCAL (gitignored); never commit MISRA text to the repo."
fi
