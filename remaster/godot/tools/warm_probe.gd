extends SceneTree
## WebGPU trial: runs App._warm_rounds_webgpu on desktop so its script errors show (the web release swallows them).
func _initialize() -> void:
	var app: Node = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_go.call_deferred(app)

func _go(app: Node) -> void:
	for i in 20:
		await process_frame
	var sv := SubViewport.new()
	sv.size = Vector2i(256, 144)
	sv.own_world_3d = true
	sv.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	app.add_child(sv)
	await app._warm_rounds_webgpu(sv, load("res://scenes/battle/battle_view.tscn"), self)
	print("WARM_PROBE done")
	quit(0)
