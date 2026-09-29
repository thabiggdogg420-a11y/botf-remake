#!/usr/bin/env bash
# Renders a scene on a virtual display (Xvfb + Mesa software rendering) and saves PNGs.
#   tools/screenshot.sh [res://scene.tscn] [out.png] [extra screenshot.gd options...]
# Examples:
#   tools/screenshot.sh res://scenes/galaxy/galaxy.tscn /tmp/galaxy.png
#   tools/screenshot.sh res://scenes/battle/battle.tscn /tmp/battle.png --frames=120 --count=4 --interval=60
# Env: RENDERER=vulkan (default; Forward+ on lavapipe) | opengl3 (Compatibility on llvmpipe)
#      SIZE=1920x1080  SHOT_TIMEOUT=300 (seconds; a script error otherwise leaves Godot running)  GODOT=...
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
source "$here/tools/godot-env.sh"
scene="${1:-res://scenes/main.tscn}"; out="${2:-$PWD/screenshot.png}"; shift $(( $# > 2 ? 2 : $# )) || true
[[ "$out" = /* ]] || out="$PWD/$out"
mkdir -p "$(dirname "$out")"
RENDERER="${RENDERER:-vulkan}"
if [[ "$RENDERER" == vulkan ]]; then
  export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json   # lavapipe
  args=(--rendering-driver vulkan)
else
  export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
  args=(--rendering-method gl_compatibility --rendering-driver opengl3)
fi
[[ -d "$here/.godot" ]] || "$GODOT" --headless --path "$here" --import >/dev/null 2>&1 || true
size="${SIZE:-1920x1080}"
exec timeout "${SHOT_TIMEOUT:-300}" xvfb-run -a -s "-screen 0 ${size}x24" \
  "$GODOT" --path "$here" "${args[@]}" --display-driver x11 --audio-driver Dummy \
  --resolution "$size" -s res://tools/screenshot.gd -- --scene="$scene" --out="$out" --size="$size" "$@"
