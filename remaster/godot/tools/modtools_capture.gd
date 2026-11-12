extends SceneTree
## MOD-10 captures at 1920x1080 on a real GPU window with the native core: the pack editor (a ship class edit, a core validation error
## shown inline, the eras and rules tabs) and the new-campaign preset picker listing the saved user pack. Uses a scratch pack folder.
##   godot --path remaster/godot --resolution 1920x1080 --audio-driver Dummy -s res://tools/modtools_capture.gd -- <out_dir>
var app: App

func wait(k := 2) -> void:
	for i in k:
		await process_frame

func _initialize() -> void:
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func _shot(path: String) -> void:
	await wait(10)
	for t in app.get_node("Toasts").get_children():
		t.queue_free()
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png(path)
	print("MT wrote ", path)

func _find_text(node: Node, needle: String) -> Control:
	if node is Control and (node.name == needle or (node is Label and needle in (node as Label).text)):
		return node as Control
	for c in node.get_children():
		var r := _find_text(c, needle)
		if r != null:
			return r
	return null

func _reveal(node: Node, needle: String) -> bool:
	var hit := _find_text(node, needle)
	if hit == null:
		return false
	var p: Node = hit.get_parent()
	while p != null:
		if p is ScrollContainer:
			(p as ScrollContainer).ensure_control_visible(hit)
		p = p.get_parent()
	return true

func _run() -> void:
	var a := OS.get_cmdline_user_args()
	var out := a[0] if a.size() > 0 else "user://modtools"
	DirAccess.make_dir_recursive_absolute(out)
	UserPacks.dir = "user://mods-capture"
	for id in UserPacks.ids():
		UserPacks.remove(id)
	await wait(10)
	print("MT backend: %s, user packs supported: %s" % [app.session.core.backend, str(UserPacks.supported(app.session.core))])
	# Title: the Pack editor entry.
	app.goto("title", false)
	await wait(20)
	await _shot("%s/00-title.png" % out)
	app.goto("packs")
	await wait(8)
	var ed := app.current_screen()
	var vm: PackEditorVM = ed.vm
	vm.set_identity("tholian-refit", "Tholian Refit", "A cheaper, tougher NX line, a faster research curve and a Klingon-Romulan-Federation war.")
	ed.refresh()
	await wait(8)
	await _shot("%s/01-pack-tab.png" % out)
	# A ship class edit.
	vm.set_field("roster", 1000, "industryCost", "1500")
	vm.set_field("roster", 1000, "hull", "170")
	vm.set_field("roster", 1000, "attack", "125")
	vm.set_balance("researchCostPercent", "85")
	ed.tab = "roster"
	ed.selected["roster"] = 1000
	ed.refresh()
	await wait(10)
	print("MT ship class edit: error='%s' summary=%s" % [vm.error, str(vm.summary())])
	await _shot("%s/02-ship-class-edit.png" % out)
	# A validation error from the core, inline.
	vm.set_field("roster", 1000, "hull", "-5")
	ed.refresh()
	await wait(10)
	print("MT validation error: tab=%s '%s'" % [vm.error_tab, vm.error])
	await _shot("%s/03-validation-error.png" % out)
	vm.set_field("roster", 1000, "hull", "170")
	# Another error, on the balance tab.
	vm.set_balance("weaponOutputPercent", "5")
	ed.tab = "balance"
	ed.refresh()
	await wait(10)
	await _shot("%s/04-balance-error.png" % out)
	vm.set_balance("weaponOutputPercent", "90")
	# Eras and rules.
	vm.set_timeline(true)
	vm.set_era_window(1000, "2250", "0")
	ed.tab = "eras"
	ed.refresh()
	await wait(10)
	await _shot("%s/05-eras.png" % out)
	ed.tab = "preset"
	ed.refresh()
	await wait(10)
	await _shot("%s/06-rules-lineup.png" % out)
	# Save, then the preset picker.
	print("MT save: '%s' ids=%s" % [vm.save(), str(UserPacks.ids())])
	ed.refresh()
	await wait(6)
	app.goto("new")
	await wait(6)
	var nc := app.current_screen()
	nc.vm.pick_preset("tholian-refit")
	nc.refresh()
	await wait(10)
	print("MT picker: %s" % str(nc.vm.presets().map(func(p): return str(p["id"]) + ("*" if p.get("user", false) else ""))))
	_reveal(nc, "PresetNote")
	await wait(6)
	await _shot("%s/07-preset-picker.png" % out)
	# Clean up the scratch pack from the core and the disk.
	app.session.core.invoke("removeUserPack", {"id": "tholian-refit"})
	for id in UserPacks.ids():
		UserPacks.remove(id)
	DirAccess.remove_absolute("user://mods-capture")
	quit(0)
