#!/usr/bin/env bash
# check-version-tag.sh -- assert a release tag matches the Zephyr VERSION file.
#
# Guards against tagging a release without bumping VERSION. The VERSION file uses
# `KEY = value` (with spaces), e.g. `VERSION_MAJOR = 0`.
#
# Usage: check-version-tag.sh <tag>   (tag like v0.1.0 or 0.1.0)
set -euo pipefail

tag="${1:?usage: check-version-tag.sh <tag>}"
tag="${tag#v}"   # strip leading v

# Locate VERSION relative to this script (scripts/ -> repo root).
here="$(cd "$(dirname "$0")" && pwd)"
version_file="$here/../VERSION"
[ -f "$version_file" ] || { echo "::error::VERSION not found at $version_file"; exit 2; }

get() { grep -E "^$1[[:space:]]*=" "$version_file" | head -n1 | sed -E 's/.*=[[:space:]]*//; s/[[:space:]]*$//'; }
major="$(get VERSION_MAJOR)"
minor="$(get VERSION_MINOR)"
patch="$(get PATCHLEVEL)"
ver="${major}.${minor}.${patch}"

if [ "$tag" != "$ver" ]; then
    echo "::error::tag v$tag does not match VERSION $ver — bump VERSION before tagging."
    exit 1
fi
echo "check-version-tag: tag v$tag matches VERSION ($ver)."
exit 0
