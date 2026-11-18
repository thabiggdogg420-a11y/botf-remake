#!/usr/bin/env bash
# Builds the botf_core GDExtension into core/bin/.
#   ./build.sh [linux|windows|all] [debug|release|both]
# Defaults: linux debug (what the editor and the headless tests load).
# Release exports need the release library: the release engine template crashes on a
# template_debug godot-cpp build.
set -euo pipefail
cd "$(dirname "$0")"
./fetch-godot-cpp.sh
JOBS="${JOBS:-$(nproc)}"
build() {
  local platform="$1" target="template_$2" dir="build-$1-$2" extra=()
  [[ "$platform" == windows ]] && extra=(-DCMAKE_TOOLCHAIN_FILE="$PWD/mingw-w64.cmake")
  mkdir -p "$dir" && touch "$dir/.gdignore"   # keep Godot from importing .obj files as meshes
  cmake -S . -B "$dir" -DCMAKE_BUILD_TYPE=Release -DBOTF_GODOT_TARGET="$target" "${extra[@]}" >/dev/null
  # sed succeeds even when all progress lines are filtered; pipefail still
  # propagates a failed compiler/build instead of accepting an old library.
  cmake --build "$dir" -j "$JOBS" | sed -E '/^\[ *[0-9]+%\] Building CXX object godot-cpp/d'
  test -n "$(ls bin/libbotf_core."$platform"."$target".x86_64.* 2>/dev/null)"
  echo "[core] built $platform $target"
}
platforms=(linux); targets=(debug)
case "${1:-linux}" in linux) ;; windows) platforms=(windows) ;; all) platforms=(linux windows) ;;
  *) echo "usage: $0 [linux|windows|all] [debug|release|both]" >&2; exit 2 ;; esac
case "${2:-debug}" in debug) ;; release) targets=(release) ;; both) targets=(debug release) ;;
  *) echo "usage: $0 [linux|windows|all] [debug|release|both]" >&2; exit 2 ;; esac
for p in "${platforms[@]}"; do for t in "${targets[@]}"; do build "$p" "$t"; done; done
