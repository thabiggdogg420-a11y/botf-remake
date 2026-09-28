#!/usr/bin/env bash
# Stand-in for the parts of a real Unreal compile that do not need the engine (Linux, GCC or Clang).
#
# 1. SimulationBridge.cpp compiles every core .cpp as ONE translation unit, as UBT does. Two core files with the same
#    file-static helper, or a core local shadowing another, only fail there. UE turns MSVC shadowing warnings
#    (C4456-C4459) into errors, so -Wshadow -Werror stands in for them.
# 2. Engine and <windows.h> headers define macros with ordinary names (near, far, IN, OUT, check, verify, TEXT, PI,
#    interface, small, ...). The core is compiled with those names predefined the hostile way, so a core identifier
#    that an engine macro would rewrite fails here first.
# 3. The engine-independent client view models (BotfViewModel.h, BotfGameplayViewModel.h) get the same treatment.
#
# This does not prove any Slate/Engine API exists; only a UE 5.8 build does (Package-Windows.ps1).
# Usage: remaster/game/Tools/check-unreal-tu.sh [compiler...]   (default: g++ and clang++ when present)
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
core="$here/Source/BotfSim"
client="$here/Source/BOTFRemake/Private"

hostile=(
  -Dnear= -Dfar= -DNEAR= -DFAR= -DIN= -DOUT= -DOPTIONAL= -DCONST=const -DVOID=void
  '-Dcheck(x)=((void)0)' '-Dverify(x)=((void)0)' '-Densure(x)=(!!(x))' '-DTEXT(x)=L##x'
  -DPI=3.1415926535897932f -DDELTA=0.00001f -DSMALL_NUMBER=1.e-8f -DKINDA_SMALL_NUMBER=1.e-4f -DBIG_NUMBER=3.4e+38f
  -DINDEX_NONE=-1 -Dinterface=struct -Dsmall=char -Dhyper=__int64_unused
  -DDELETE=0x00010000L -DERROR=0 -DNO_ERROR=0 -DTRANSPARENT=1 -DOPAQUE=2 -DABSOLUTE=1 -DRELATIVE=2
  -DDIFFERENCE=11 -DIGNORE=0 -DINFINITE=0xFFFFFFFF '-DYield()=' -DCALLBACK= -DWINAPI= -DPASCAL=
)
flags=(-std=c++20 -fsyntax-only -Wall -Wextra -Wshadow -Werror -I"$core")

compilers=("$@")
if [ ${#compilers[@]} -eq 0 ]; then
  for c in g++ clang++; do command -v "$c" >/dev/null 2>&1 && compilers+=("$c"); done
fi
[ ${#compilers[@]} -gt 0 ] || { echo "no C++ compiler found" >&2; exit 2; }

scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
cat > "$scratch/viewmodels.cpp" <<EOF
#include "$client/BotfViewModel.h"
#include "$client/BotfGameplayViewModel.h"
EOF

status=0
for cxx in "${compilers[@]}"; do
  echo "== $cxx: SimulationBridge.cpp as one translation unit"
  "$cxx" "${flags[@]}" "$client/SimulationBridge.cpp" || status=1
  echo "== $cxx: SimulationBridge.cpp with engine/Windows macro names predefined"
  "$cxx" "${flags[@]}" "${hostile[@]}" "$client/SimulationBridge.cpp" || status=1
  echo "== $cxx: client view models with engine/Windows macro names predefined"
  "$cxx" "${flags[@]}" "${hostile[@]}" "$scratch/viewmodels.cpp" || status=1
done
[ $status -eq 0 ] && echo "Unreal translation-unit stand-in checks passed."
exit $status
