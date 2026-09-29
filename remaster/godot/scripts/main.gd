extends Node
## Entry point: checks the core extension, then mounts the strategic UI shell.

func _ready() -> void:
	if Engine.has_singleton("BotfCore"):
		var meta: Dictionary = BotfCore.request("meta")
		print("BOTF Remastered (Godot) booting: core ok=%s, %d empires" % [meta.ok, meta.result.empires.size()])
	else:
		push_warning("BotfCore extension not loaded (build it with core/build.sh); using recorded fixtures")
	if DisplayServer.get_name() == "headless" and OS.get_cmdline_user_args().has("--boot-check"):
		get_tree().quit()
		return
	add_child(load("res://scenes/ui/app.tscn").instantiate())
