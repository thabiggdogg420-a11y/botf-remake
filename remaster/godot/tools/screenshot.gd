extends SceneTree
## Renders a scene and saves PNG screenshots. Driven by tools/screenshot.sh; arguments after "--":
##   --scene=res://scenes/main.tscn   scene to instantiate (default: the main scene)
##   --out=/abs/path/shot.png         output PNG (with --count > 1: shot-1.png, shot-2.png, ...)
##   --frames=30                      frames to wait before the (first) capture
##   --count=1 --interval=30          extra captures every N frames (animations, battle playback)
##   --size=1920x1080                 window size
##   --call=method                    call this method on the scene root before waiting (optional)

func _initialize() -> void:
	var opts := {"scene": ProjectSettings.get_setting("application/run/main_scene"), "out": "user://screenshot.png",
		"frames": "30", "count": "1", "interval": "30", "size": "1920x1080", "call": ""}
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--") and "=" in arg:
			var kv := arg.substr(2).split("=", true, 1)
			opts[kv[0]] = kv[1]
	var dims: PackedStringArray = String(opts.size).split("x")
	root.size = Vector2i(int(dims[0]), int(dims[1]))
	DisplayServer.window_set_size(root.size)
	var packed: PackedScene = load(opts.scene)
	if packed == null:
		printerr("screenshot: cannot load scene ", opts.scene)
		quit(2)
		return
	var node := packed.instantiate()
	root.add_child(node)
	if opts.call != "" and node.has_method(opts.call):
		node.call(opts.call)
	_capture.call_deferred(opts, node)

func _capture(opts: Dictionary, _node: Node) -> void:
	var count := int(opts.count)
	for i in count:
		var wait: int = int(opts.frames) if i == 0 else int(opts.interval)
		for f in wait:
			await process_frame
		await RenderingServer.frame_post_draw
		var image := root.get_texture().get_image()
		var path: String = opts.out
		if count > 1:
			path = "%s-%d.%s" % [path.get_basename(), i + 1, path.get_extension()]
		var err := image.save_png(path)
		if err != OK:
			printerr("screenshot: cannot write ", path, " (", error_string(err), ")")
			quit(1)
			return
		print("screenshot: ", ProjectSettings.globalize_path(path), " ", image.get_width(), "x", image.get_height())
	quit(0)
