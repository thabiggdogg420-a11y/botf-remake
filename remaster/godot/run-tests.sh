#!/usr/bin/env bash
# Builds the linux extension (incremental), imports the project and runs the headless GDScript tests.
#   GODOT=/path/to/godot ./run-tests.sh      (default: $GODOT, then godot4/godot on PATH,
#                                             then /tmp/claude-0/Godot_v4.5-stable_linux.x86_64)
set -euo pipefail
cd "$(dirname "$0")"
source ./tools/godot-env.sh
[[ "${SKIP_BUILD:-0}" == 1 ]] || ./core/build.sh linux >/dev/null
# First import registers the extension (.godot/extension_list.cfg) and imports resources.
"$GODOT" --headless --path . --import >/dev/null 2>&1 || true
timeout "${TEST_TIMEOUT:-600}" "$GODOT" --headless --path . -s res://tests/run_tests.gd
