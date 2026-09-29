extends Node
## Entry point. Screens are loaded by the shell once the core extension is available.

func _ready() -> void:
	if not Engine.has_singleton("BotfCore"):
		push_error("BotfCore extension not loaded (build it with core/build.sh)")
		return
	var meta: Dictionary = BotfCore.request("meta")
	print("BOTF Remastered (Godot) booting: core ok=%s, %d empires" % [meta.ok, meta.result.empires.size()])
