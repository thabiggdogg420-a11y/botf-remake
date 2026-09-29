#!/usr/bin/env bash
# Renders battle screenshots headlessly (Xvfb + Vulkan lavapipe, Forward+).
#   scenes/battle/tools/capture.sh OUT_DIR [SHOTS] [extra user args...]
# SHOTS is round:seconds[,round:seconds...] (round = index into the fixture's rounds).
# Needs: xvfb-run, mesa-vulkan-drivers (lavapipe). GODOT defaults to godot on PATH.
set -euo pipefail
here="$(cd "$(dirname "$0")/../../.." && pwd)"
out="${1:?output dir}"
shots="${2:-4:2.8,4:3.7,5:3.2,0:2.3}"
shift $(( $# >= 2 ? 2 : 1 ))
godot="${GODOT:-godot}"
export VK_ICD_FILENAMES="${VK_ICD_FILENAMES:-/usr/share/vulkan/icd.d/lvp_icd.json}"
xvfb-run -a -s "-screen 0 1920x1080x24" "$godot" --path "$here" --rendering-driver vulkan --fixed-fps 30 \
  res://scenes/battle/battle_demo.tscn -- --capture="$out" --shots="$shots" --size="${SIZE:-1280x720}" "$@"
