#!/usr/bin/env bash
set -euo pipefail

# Compatibility entry point. Music is now shared by all targets and both games.
# An upstream checkout is required so an existing Opus file is never re-encoded.
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${PYTHON:-python3}" "$script_dir/generate_music.py" "$@"
