#!/usr/bin/env bash
# Build everything and lay it out as an SD-card image under <output-dir>/sdmc/3ds/:
#   coffeebench/coffeebench.3dsx          the four micro-benchmarks in one binary
#   coffeepot-tiles/coffeepot-tiles.3dsx  the first of the 20 chained tile-renderer binaries
#   coffeepot-tiles/t01.3dsx ... t20.3dsx all of them (the chain launches t02 onward by name)
# Copy <output-dir>/sdmc/* to the SD card root. Usage: tools/stage_benchmarks.sh [output-dir]
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$root/dist}"
commit="$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo unknown)"
if [ -n "$(git -C "$root" status --porcelain -- ctr tools 2>/dev/null)" ]; then commit="${commit}_dirty"; fi

rm -rf "$out/sdmc"
mkdir -p "$out/sdmc/3ds/coffeebench" "$out/sdmc/3ds/coffeepot-tiles"
make -C "$root/ctr/benchmarks/suite" clean >/dev/null 2>&1
log="$(make -C "$root/ctr/benchmarks/suite" EXTRA_CFLAGS="-DBENCH_COMMIT=$commit" 2>&1)" || { echo "$log"; exit 1; }
if grep -qi warning <<<"$log"; then echo "$log"; echo "WARNINGS: suite"; exit 1; fi
cp "$root/ctr/benchmarks/suite/coffeebench.3dsx" "$out/sdmc/3ds/coffeebench/"
bash "$root/tools/build_tile_suite.sh" "$out/sdmc/3ds/coffeepot-tiles"
echo "staged commit $commit in $out/sdmc"
