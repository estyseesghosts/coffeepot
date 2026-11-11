#!/usr/bin/env bash
# Build the tile-renderer suite (ctr/benchmarks/tile-renderer/suite.txt): one .3dsx per line,
# each with BENCH_AUTO=1, its index in the chain, and the repository commit. Fails on any warning
# or error. Usage: tools/build_tile_suite.sh <output-dir> [n3ds]
# Writes <output-dir>/t01.3dsx ... tNN.3dsx, <output-dir>/coffeepot-tiles.3dsx (a copy of t01,
# the one entry the Homebrew Menu shows) and a copy of suite.txt. Requires DEVKITPRO/DEVKITARM.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
proj="$root/ctr/benchmarks/tile-renderer"
out="${1:?usage: build_tile_suite.sh <output-dir> [n3ds]}"
# "n3ds": the New 3DS suite. Each binary sets osSetSpeedupEnable(true) (804 MHz + L2) before it measures,
# chains within /3ds/coffeepot-tiles-n3ds, and the Homebrew Menu entry is coffeepot-tiles-n3ds.3dsx.
extra=""; suffix=""; entry="coffeepot-tiles"
if [ "${2:-}" = "n3ds" ]; then extra="-DBENCH_SPEEDUP=1 -DSUITE_N3DS=1"; suffix="_n3ds"; entry="coffeepot-tiles-n3ds"; fi
mkdir -p "$out"

commit="$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo unknown)"
if [ -n "$(git -C "$root" -c core.autocrlf=true status --porcelain -- ctr tools 2>/dev/null)" ]; then commit="${commit}_dirty"; fi

mapfile -t lines < <(grep -v '^\s*#' "$proj/suite.txt" | grep -v '^\s*$')
total=${#lines[@]}
status=0
i=0
for line in "${lines[@]}"; do
    i=$((i + 1))
    id="${line%%|*}"
    flags="${line#*|}"
    n="$(printf '%02d' "$i")"
    echo "== t$n $id"
    make -C "$proj" clean >/dev/null 2>&1
    if ! log="$(make -C "$proj" EXTRA_CFLAGS="-DBENCH_AUTO=1 -DSUITE_INDEX=$i -DSUITE_TOTAL=$total -DSUITE_ID=t${n}_${id#*_}${suffix} -DBENCH_COMMIT=$commit $extra $flags" 2>&1)"; then
        echo "$log"; echo "FAILED: $id"; status=1; continue
    fi
    if grep -qi "warning" <<<"$log"; then echo "$log"; echo "WARNINGS: $id"; status=1; continue; fi
    cp "$proj/tile-renderer.3dsx" "$out/t$n.3dsx"
done
make -C "$proj" clean >/dev/null 2>&1
[ -f "$out/t01.3dsx" ] && cp "$out/t01.3dsx" "$out/$entry.3dsx"
cp "$proj/suite.txt" "$out/suite.txt"
echo "built $total variants (commit $commit) into $out"
exit $status
