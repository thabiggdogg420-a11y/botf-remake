extends "res://scenes/battle/battle_scene.gd"
## Adapter between the UI's BattleHost contract (load_encounter / play_round(round, encounter))
## and BattleScene.play_round(encounter, round_index).

func load_encounter(e: Dictionary) -> void:
	var rounds: Array = e.get("rounds", [])
	if not rounds.is_empty() and not is_playing():
		super.play_round(e, rounds.size() - 1)

func play_round(r: Variant, e: Variant = null) -> void:
	# BattleScene-style call: play_round(encounter, index).
	if r is Dictionary and (e is int or e is float):
		super.play_round(r, int(e))
		return
	var enc: Dictionary = e if e is Dictionary else {}
	var rounds: Array = enc.get("rounds", [])
	var idx := rounds.size() - 1
	for i in rounds.size():
		if rounds[i] == r or (r is Dictionary and rounds[i].get("index") == r.get("index")):
			idx = i
			break
	if idx >= 0:
		super.play_round(enc, idx)
