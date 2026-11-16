#!/usr/bin/env bash
# usage: gen.sh <name> "<subject>"   (run from raw/)
name="$1"; subject="$2"
STYLE="Small cinematic Star Trek concept-art render, square composition, a single subject centred and large with a clear readable silhouette, dark near-black space or night-sky background, dramatic rim lighting with soft blue and amber glow, painterly matte-painting finish in the same style as a Star Trek Federation colony illustration, no text, no letters, no numbers, no logos, no user-interface, no people close up."
codex exec --skip-git-repo-check -s workspace-write "Use your image generation tool to create one image saved as ${name}.png in the current folder: ${subject}. ${STYLE} Report the saved path." < /dev/null > "${name}.log" 2>&1
echo "$name done: $(ls -la ${name}.png 2>&1 | tail -1)"
