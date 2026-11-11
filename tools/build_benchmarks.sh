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
# The 20-binary tile-renderer suite (also fails on any warning).
echo "== tile-renderer suite"
if bash "$root/tools/build_tile_suite.sh" "$(mktemp -d)" >/dev/null 2>&1; then echo "ok"; else echo "FAILED: tile-renderer suite (run tools/build_tile_suite.sh <dir> to see why)"; status=1; fi
echo "== tile-renderer suite (New 3DS, 804 MHz)"
if bash "$root/tools/build_tile_suite.sh" "$(mktemp -d)" n3ds >/dev/null 2>&1; then echo "ok"; else echo "FAILED: New 3DS tile suite"; status=1; fi
echo "== coffeebench-n3ds"
if make -C "$root/ctr/benchmarks/suite" N3DS=1 clean >/dev/null 2>&1 && log="$(make -C "$root/ctr/benchmarks/suite" N3DS=1 2>&1)" && ! grep -qi warning <<<"$log"; then echo "ok"; else echo "FAILED: coffeebench-n3ds"; status=1; fi
make -C "$root/ctr/benchmarks/suite" N3DS=1 clean >/dev/null 2>&1
exit $status
