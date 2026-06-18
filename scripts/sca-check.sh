#!/usr/bin/env bash
# sca-check.sh -- gate a Zephyr SCA (gcc -fanalyzer) build log.
#
# Zephyr's `-DZEPHYR_SCA_VARIANT=gcc` analyzes the WHOLE tree (kernel + app),
# so the raw log contains known noise from zephyr/* and modules/*. This script
# fails ONLY when an -Wanalyzer warning points at this project's own code
# (app/src or app/include). Used by CI and locally.
#
# Usage: sca-check.sh <build-log>
set -uo pipefail

log="${1:?usage: sca-check.sh <build-log>}"
[ -f "$log" ] || { echo "sca-check: log not found: $log" >&2; exit 2; }

# Keep -Wanalyzer lines under app/{src,include}, drop zephyr/ and modules/ noise.
# Path regex tolerates both absolute CI paths and relative local paths.
hits=$(grep -E '\[-Wanalyzer' "$log" \
       | grep -E '(^|[/\\])app[/\\](src|include)[/\\]' \
       | grep -vE '[/\\](zephyr|modules)[/\\]' || true)

if [ -n "$hits" ]; then
    echo "::error::gcc analyzer (-Wanalyzer) warnings in app code:"
    echo "$hits"
    exit 1
fi

echo "sca-check: 0 analyzer warnings in app code."
exit 0
