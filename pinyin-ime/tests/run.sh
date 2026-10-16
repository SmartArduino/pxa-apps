#!/usr/bin/env bash
set -euo pipefail
app_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/pxa-ime-engine.XXXXXX")"
trap 'rm -rf "$test_dir"' EXIT
bash "$app_dir/tests/run_t9.sh"
python3 "$app_dir/tests/benchmark.py" --sanitize --check --samples 1 --iterations 10 --output "$test_dir"
