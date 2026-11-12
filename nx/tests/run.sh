#!/usr/bin/env bash
# Build and run the host-side logic tests for the NX examples. Needs only a host C compiler (gcc or clang).
# Pass criteria: zero warnings with -Wall -Wextra and "N checks, 0 failed".
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
common="$here/../engineering/examples/common"
out="${1:-$(mktemp -d)}"
cc="${CC:-$(command -v gcc || command -v cc || command -v clang)}"
sanitize=""
# Undefined-behaviour checking where the compiler supports it (it catches the int16_t*/uint32_t* class of bug).
if echo 'int main(void){return 0;}' | "$cc" -x c -fsanitize=undefined -o "$out/probe" - 2>/dev/null; then
    sanitize="-fsanitize=undefined -fno-sanitize-recover=undefined"
fi
"$cc" -std=c11 -O1 -g -Wall -Wextra -Werror $sanitize -I"$common" \
    "$here/test_logic.c" "$common/save_format.c" "$common/world.c" "$common/pcm_pad.c" -o "$out/test_logic"
"$out/test_logic"
