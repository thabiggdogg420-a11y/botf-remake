extends SceneTree
## Prints sample log lines per empire for the common event kinds (headless): godot --headless -s res://scenes/ui/tools/trektext_samples.gd
func _initialize() -> void:
	var lines := ["Colony founded at Vega. Production begins next turn.", "Battle at sector 7,10: 2 ships lost, 1 withdrew.", "Klingon Empire is now at war with us.",
		"Weapons advanced to level 4. Benefits apply next turn.", "Diplomacy: Ferengi Alliance proposes a Trade Pact (proposal 4, respond by turn 14).", "Sol captured by Romulan Star Empire."]
	for e in range(1, 7):
		print("--- ", TrekText.log_header(e, 10))
		for i in lines.size():
			print("  ", TrekText.voice_line(e, lines[i], 10, i, {"own_name": "Federation"}))
	quit(0)
