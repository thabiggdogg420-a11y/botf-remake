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
	if AppLoader.diag():
		var t := Timer.new()
		t.wait_time = 5.0
		t.autostart = true
		t.timeout.connect(func() -> void:
			var ri := RenderingServer
			print("DIAG gpu tex=%.0f MB buf=%.0f MB video=%.0f MB draw=%d objs=%d nodes=%d" % [
				ri.get_rendering_info(RenderingServer.RENDERING_INFO_TEXTURE_MEM_USED) / 1048576.0,
				ri.get_rendering_info(RenderingServer.RENDERING_INFO_BUFFER_MEM_USED) / 1048576.0,
				ri.get_rendering_info(RenderingServer.RENDERING_INFO_VIDEO_MEM_USED) / 1048576.0,
				ri.get_rendering_info(RenderingServer.RENDERING_INFO_TOTAL_DRAW_CALLS_IN_FRAME),
				ri.get_rendering_info(RenderingServer.RENDERING_INFO_TOTAL_OBJECTS_IN_FRAME),
				get_tree().get_node_count()]))
		add_child(t)
