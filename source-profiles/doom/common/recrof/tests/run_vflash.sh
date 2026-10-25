#!/usr/bin/env bash
set -euo pipefail
tests="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
flags=(-std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror
       -fsanitize=address,undefined -fno-omit-frame-pointer -g -I"$tests/..")
for capacity in 4 16 48 128; do
    "${CC:-cc}" "${flags[@]}" -DPORT_VFLASH_CACHE_PAGES="$capacity" \
        "$tests/../vflash.c" "$tests/vflash_index_test.c" -o "$build/index"
    "$build/index"
done
for paged in 0 1; do
    extra=()
    if ((paged)); then extra+=(-DPORT_VFLASH_PAGED_NVM=1); fi
    "${CC:-cc}" "${flags[@]}" "${extra[@]}" -DPORT_VFLASH_COOPERATE=1 \
        -DPORT_COOPERATE_BUDGET_US=8000 "$tests/../vflash.c" \
        "$tests/vflash_test.c" -o "$build/nvm"
    "$build/nvm"
done
