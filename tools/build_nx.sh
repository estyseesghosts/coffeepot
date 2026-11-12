#!/usr/bin/env bash
# Build every NX example and benchmark with devkitA64 and fail on any compiler warning or error.
# Requires DEVKITPRO/DEVKITA64 to be set (a devkitPro shell or the devkitpro/devkita64 container image).
# The host-side logic tests are separate (nx/tests/run.sh): they need only a host C compiler.
# Usage: tools/build_nx.sh [output-dir]   (the .nro and .elf files are copied there when given)
#
# Each project is built from a copy, with nx/engineering/examples/common next to it, so the source tree stays clean
# and a path with spaces cannot break the devkitPro makefiles.
set -uo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-}"
[ -n "$out" ] && mkdir -p "$out"
status=0
work="$(mktemp -d)"
mkdir -p "$work/examples" "$work/benchmarks"
cp -r "$root/nx/engineering/examples/common" "$work/examples/"
[ -d "$root/nx/benchmarks/common" ] && cp -r "$root/nx/benchmarks/common" "$work/benchmarks/"

build() {   # build <label> <source-dir> <copy-into> [make args...]
    local label="$1" src="$2" dst="$3"; shift 3
    local name; name="$(basename "$src")"
    echo "== $label"
    rm -rf "$dst/$name"; cp -r "$src" "$dst/$name"
    local log
    if ! log="$(make -C "$dst/$name" "$@" 2>&1)"; then echo "$log"; echo "FAILED: $label"; status=1; return; fi
    if grep -qi "warning" <<<"$log"; then echo "$log"; echo "WARNINGS: $label"; status=1; return; fi
    echo ok
    if [ -n "$out" ] && [ -z "${NOCOPY:-}" ]; then cp "$dst/$name"/*.nro "$dst/$name"/*.elf "$out/" 2>/dev/null || true; fi
}

for dir in "$root"/nx/engineering/examples/*/; do
    [ "$(basename "$dir")" = "common" ] && continue
    [ -f "$dir/Makefile" ] || continue
    build "example $(basename "$dir")" "${dir%/}" "$work/examples"
done
# Scripted acceptance builds: the same programs compiled with a self-test flag that writes report.csv to the SD card.
for spec in first-game:FG_SELFTEST audout-stream:AS_SELFTEST deko-renderer:DKR_SELFTEST; do
    name="${spec%%:*}"; flag="${spec##*:}"
    [ -f "$root/nx/engineering/examples/$name/Makefile" ] || continue
    NOCOPY=1 build "example $name (self-test build)" "$root/nx/engineering/examples/$name" "$work/examples" CFLAGS_EXTRA=-D$flag
    [ -n "$out" ] && [ -f "$work/examples/$name/$name.nro" ] && cp "$work/examples/$name/$name.nro" "$out/$name-selftest.nro"
done
for dir in "$root"/nx/benchmarks/*/; do
    [ -f "$dir/Makefile" ] || continue
    build "benchmark $(basename "$dir")" "${dir%/}" "$work/benchmarks"
done

rm -rf "$work"
exit $status
