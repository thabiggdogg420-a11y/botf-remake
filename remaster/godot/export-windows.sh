#!/usr/bin/env bash
# Exports the game from Linux.
#   ./export-windows.sh           build/BOTFRemastered.exe + libbotf_core.windows.template_release.x86_64.dll
#   ./export-windows.sh --linux   also build/linux/BOTFRemastered.x86_64 (+ .so) and boot it headless
# Needs mingw-w64 (apt install g++-mingw-w64-x86-64-posix); export templates are downloaded on demand.
set -euo pipefail
cd "$(dirname "$0")"
source ./tools/godot-env.sh
./tools/install-export-templates.sh
MODE="${EXPORT_MODE:-release}"   # release | debug
./core/build.sh windows "$MODE"
mkdir -p build
"$GODOT" --headless --path . --import >/dev/null 2>&1 || true
"$GODOT" --headless --path . --export-$MODE "Windows Desktop" build/BOTFRemastered.exe
ls -l build/BOTFRemastered.exe build/libbotf_core.windows.template_$MODE.x86_64.dll
if [[ "${1:-}" == "--linux" ]]; then
  ./core/build.sh linux "$MODE"
  mkdir -p build/linux
  "$GODOT" --headless --path . --export-$MODE "Linux" build/linux/BOTFRemastered.x86_64
  echo "[export] booting the linux export headless"
  out="$(build/linux/BOTFRemastered.x86_64 --headless --quit-after 5 2>&1)"
  echo "$out"
  grep -q "core ok=true" <<<"$out"
  echo "[export] linux export boots"
fi
