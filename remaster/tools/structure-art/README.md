Generates structure and governor illustrations with Codex CLI's image tool (OpenAI image generation via Codex CLI).
Run from a work folder holding raw/: `./runbatch.sh batchN.txt` (3 at once; lines are `slug|subject`; slug = ShipIcons.slug of the structure name).
gen.sh adds the shared style sentence and redirects stdin (codex waits on stdin otherwise). Then scale with
`godot --headless --path remaster/godot -s res://tools/scale_art.gd -- <raw> remaster/godot/assets/structures 384 <sheet.png>`.
