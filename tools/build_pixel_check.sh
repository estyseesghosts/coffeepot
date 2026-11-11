#!/usr/bin/env bash
# Build the pixel-equivalence variants of the tile renderer (ctr/benchmarks/tile-renderer/pixel-check.txt) into
# <output-dir>/<name>.3dsx. Fails on any warning. Needs DEVKITPRO/DEVKITARM. Usage: tools/build_pixel_check.sh <output-dir>
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
proj="$root/ctr/benchmarks/tile-renderer"
out="${1:?usage: build_pixel_check.sh <output-dir>}"
mkdir -p "$out"
status=0
while IFS='|' read -r name flags ref; do
    case "$name" in ''|\#*) continue;; esac
    make -C "$proj" clean >/dev/null 2>&1
    if ! log="$(make -C "$proj" EXTRA_CFLAGS="-DBENCH_AUTO=1 -DBENCH_DUMP_FRAME=300 -DSUITE_ID=$name $flags" 2>&1)"; then echo "$log"; echo "FAILED: $name"; status=1; continue; fi
    if grep -qi warning <<<"$log"; then echo "$log"; echo "WARNINGS: $name"; status=1; continue; fi
    cp "$proj/tile-renderer.3dsx" "$out/$name.3dsx"
    echo "built $name"
done < "$proj/pixel-check.txt"
make -C "$proj" clean >/dev/null 2>&1
cp "$proj/pixel-check.txt" "$out/pixel-check.txt"
exit $status
