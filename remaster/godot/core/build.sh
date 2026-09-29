#!/usr/bin/env bash
# Builds the botf_core GDExtension into core/bin/.
#   ./build.sh            linux x86_64
#   ./build.sh windows    windows x86_64 (mingw-w64 cross-compile)
#   ./build.sh all        both
set -euo pipefail
cd "$(dirname "$0")"
./fetch-godot-cpp.sh
JOBS="${JOBS:-$(nproc)}"
build() {
  local platform="$1" extra=()
  [[ "$platform" == windows ]] && extra=(-DCMAKE_TOOLCHAIN_FILE="$PWD/mingw-w64.cmake")
  mkdir -p "build-$platform" && touch "build-$platform/.gdignore"
  cmake -S . -B "build-$platform" -DCMAKE_BUILD_TYPE=Release "${extra[@]}" >/dev/null
  cmake --build "build-$platform" -j "$JOBS"
  echo "[core] built $platform"
}
case "${1:-linux}" in
  linux) build linux ;;
  windows) build windows ;;
  all) build linux; build windows ;;
  *) echo "usage: $0 [linux|windows|all]" >&2; exit 2 ;;
esac
