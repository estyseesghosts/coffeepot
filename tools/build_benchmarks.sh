#!/usr/bin/env bash
# Build every project under ctr/benchmarks/ with devkitARM and fail on any
# compiler warning or error. Requires DEVKITPRO/DEVKITARM to be set (as in a
# devkitPro shell or the devkitpro/devkitarm container image).
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
status=0
for dir in "$root"/ctr/benchmarks/*/; do
    [ -f "$dir/Makefile" ] || continue
    name="$(basename "$dir")"
    echo "== $name"
    log="$(make -C "$dir" 2>&1)" || { echo "$log"; echo "FAILED: $name"; status=1; continue; }
    if grep -qi "warning" <<<"$log"; then
        echo "$log"; echo "WARNINGS: $name"; status=1
    else
        echo "ok"
    fi
done
exit $status
