#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
case "$(uname -s):$(uname -m)" in Linux:x86_64) ;; *) echo 'Set PXA_BINARYEN_DIR to a Binaryen 130 installation on this platform.' >&2; exit 1;; esac
mkdir -p "$root/.tools"
archive="$root/.tools/binaryen-130.tar.gz"
curl --fail --location --retry 3 https://github.com/WebAssembly/binaryen/releases/download/version_130/binaryen-version_130-x86_64-linux.tar.gz --output "$archive"
printf '%s  %s\n' '0a18362361ad05465118cd8eeb72edaeec89de6894bc283576ef4e07aa3babcc' "$archive" | sha256sum --check --status
tar -xzf "$archive" -C "$root/.tools"
printf 'Binaryen installed at %s\n' "$root/.tools/binaryen-version_130"
