# Sourced by the scripts in remaster/godot: resolves $GODOT (a Godot 4.5-stable linux binary),
# downloading it into ~/.cache/botf-godot when none is found.
GODOT_VERSION="4.5-stable"
if [[ -z "${GODOT:-}" ]]; then
  for candidate in godot4 godot /tmp/claude-0/Godot_v${GODOT_VERSION}_linux.x86_64 "$HOME/.cache/botf-godot/Godot_v${GODOT_VERSION}_linux.x86_64"; do
    if command -v "$candidate" >/dev/null 2>&1; then GODOT="$(command -v "$candidate")"; break; fi
  done
fi
if [[ -z "${GODOT:-}" ]]; then
  cache="$HOME/.cache/botf-godot"; mkdir -p "$cache"
  echo "[godot] downloading Godot $GODOT_VERSION" >&2
  curl -fsSL -o "$cache/godot.zip" "https://github.com/godotengine/godot/releases/download/${GODOT_VERSION}/Godot_v${GODOT_VERSION}_linux.x86_64.zip"
  (cd "$cache" && unzip -oq godot.zip && rm godot.zip)
  GODOT="$cache/Godot_v${GODOT_VERSION}_linux.x86_64"
fi
export GODOT GODOT_VERSION
