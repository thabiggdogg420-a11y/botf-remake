#!/usr/bin/env bash
# Fetches godot-cpp (pinned to the Godot 4.5 release tag) into core/godot-cpp (gitignored).
set -euo pipefail
cd "$(dirname "$0")"
TAG="${GODOT_CPP_TAG:-godot-4.5-stable}"
if [[ -f godot-cpp/.fetched-$TAG ]]; then exit 0; fi
rm -rf godot-cpp
git clone --quiet --depth 1 --branch "$TAG" https://github.com/godotengine/godot-cpp godot-cpp
touch "godot-cpp/.fetched-$TAG" godot-cpp/.gdignore
echo "[core] godot-cpp $TAG fetched"
