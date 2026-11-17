#!/usr/bin/env bash
set -euo pipefail
app_dir="$(cd "$(dirname "$0")/.." && pwd)"
workspace_dir="$(cd "$app_dir/../../.." && pwd)"
artifact_dir="${1:-$workspace_dir/local/artifacts/temple-runner/$(date +%Y%m%d-%H%M%S)}"
mkdir -p "$artifact_dir"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -fsanitize=address,undefined \
  "$app_dir/core.cpp" "$app_dir/tests/core_test.cpp" -o "$artifact_dir/core_test"
"$artifact_dir/core_test" | tee "$artifact_dir/core-test.log"

"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -fsanitize=address,undefined \
  "$app_dir/core.cpp" "$app_dir/tests/advanced_test.cpp" -o "$artifact_dir/advanced_test"
"$artifact_dir/advanced_test" | tee "$artifact_dir/advanced-test.log"

"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -fsanitize=address,undefined \
  "$app_dir/core.cpp" "$app_dir/tests/player_motion_test.cpp" -o "$artifact_dir/player_motion_test"
"$artifact_dir/player_motion_test" | tee "$artifact_dir/player-motion.log"

"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -fsanitize=address,undefined \
  "$app_dir/core.cpp" "$app_dir/tests/soundscape_test.cpp" -o "$artifact_dir/soundscape_test"
"$artifact_dir/soundscape_test" | tee "$artifact_dir/soundscape.log"
