#!/usr/bin/env bash
# Records botf_call JSON responses into remaster/godot/fixtures/ (see README "Fixtures").
set -euo pipefail
cd "$(dirname "$0")"
exec node dump-fixtures.mjs
