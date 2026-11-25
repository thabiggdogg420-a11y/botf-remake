extends SceneTree
## Prints the resolved klaxon file for the four alert cues on a COLD start (no cue table loaded: the web's first frames) and
## after the shared pack loads. Silent: run with --audio-driver Dummy. Exit 1 when any cue resolves to a generated tone or nothing.
##   godot --headless --audio-driver Dummy --path remaster/godot -s res://scripts/audio/tests/klaxon_dump.gd
## The exported exe runs the same script (scripts are in the pck): BOTFRemastered.exe --headless --audio-driver Dummy -s res://scripts/audio/tests/klaxon_dump.gd

func _initialize() -> void:
	var bad := 0
	var lib := BotfAudioLibrary.new()
	for cue_name in ["red_alert", "encounter_opens", "war_declared", "system_under_attack"]:
		var c := BotfAudioCues.cue_for(cue_name, "fed", "kli", "")
		var keys := PackedStringArray()
		for k in c.sfx:
			if str(k).begins_with("sfx.alert"):
				keys.append(k)
		var r := lib.resolve_files(keys)
		var tone := lib.tone_for(keys[0]) if not keys.is_empty() else null
		var s := lib.stream_of(r) if not r.is_empty() else null
		var ok := not r.is_empty() and s is AudioStreamOggVorbis and tone == null
		bad += 0 if ok else 1
		print("KLAXON %s key=%s file=%s stream=%s tone=%s %s" % [cue_name, str(keys), str(r.get("files", PackedStringArray([""]))[0]), s.get_class() if s else "none", str(tone), "ok" if ok else "BAD"])
	quit(1 if bad > 0 else 0)
