#!/usr/bin/env bash
# Installs the Godot 4.5-stable export templates needed for the Windows and Linux x86_64 exports
# into ~/.local/share/godot/export_templates/4.5.stable/ (the full .tpz is ~1 GB; only four
# templates are kept).
set -euo pipefail
source "$(dirname "$0")/godot-env.sh"
dest="${XDG_DATA_HOME:-$HOME/.local/share}/godot/export_templates/${GODOT_VERSION/-/.}"
need=(windows_release_x86_64.exe windows_debug_x86_64.exe linux_release.x86_64 linux_debug.x86_64)
missing=0; for f in "${need[@]}"; do [[ -f "$dest/$f" ]] || missing=1; done
[[ $missing == 0 ]] && exit 0
cache="$HOME/.cache/botf-godot"; mkdir -p "$cache" "$dest"
tpz="$cache/Godot_v${GODOT_VERSION}_export_templates.tpz"
if [[ ! -f "$tpz" ]]; then
  echo "[export] downloading export templates (~1 GB)" >&2
  curl -fL --retry 3 -o "$tpz.part" "https://github.com/godotengine/godot/releases/download/${GODOT_VERSION}/Godot_v${GODOT_VERSION}_export_templates.tpz"
  mv "$tpz.part" "$tpz"
fi
unzip -oqj "$tpz" "${need[@]/#/templates/}" templates/version.txt -d "$dest"
chmod +x "$dest"/linux_*.x86_64
echo "[export] templates installed in $dest" >&2
