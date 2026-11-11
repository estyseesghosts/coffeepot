#!/usr/bin/env bash
# Build and stage the New 3DS (full speed) benchmarks as an SD-card image under <output-dir>/sdmc/3ds/:
#   coffeebench-n3ds/coffeebench-n3ds.3dsx          micro suite + corebench, each module at 268 MHz and at 804 MHz + L2
#   coffeepot-tiles-n3ds/coffeepot-tiles-n3ds.3dsx  first of the 20 chained tile-renderer binaries, all at 804 MHz + L2
#   coffeepot-tiles-n3ds/t01.3dsx ... t20.3dsx
# These refuse (micro suite) or are pointless (tile suite) on an Old 3DS. Copy <output-dir>/sdmc/* to the SD card root.
# Usage: tools/stage_n3ds.sh [output-dir]
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$root/dist-n3ds}"
commit="$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo unknown)"
if [ -n "$(git -C "$root" status --porcelain -- ctr tools 2>/dev/null)" ]; then commit="${commit}_dirty"; fi

rm -rf "$out/sdmc"
mkdir -p "$out/sdmc/3ds/coffeebench-n3ds" "$out/sdmc/3ds/coffeepot-tiles-n3ds"
suite="$root/ctr/benchmarks/suite"
make -C "$suite" N3DS=1 clean >/dev/null 2>&1
log="$(make -C "$suite" N3DS=1 EXTRA_CFLAGS="-DBENCH_COMMIT=$commit" 2>&1)" || { echo "$log"; exit 1; }
if grep -qi warning <<<"$log"; then echo "$log"; echo "WARNINGS: suite N3DS"; exit 1; fi
cp "$suite/coffeebench-n3ds.3dsx" "$out/sdmc/3ds/coffeebench-n3ds/"
make -C "$suite" N3DS=1 clean >/dev/null 2>&1
bash "$root/tools/build_tile_suite.sh" "$out/sdmc/3ds/coffeepot-tiles-n3ds" n3ds
echo "staged New 3DS benchmarks (commit $commit) in $out/sdmc"
