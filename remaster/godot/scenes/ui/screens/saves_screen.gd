extends Control
## Save / load presentation. Slots come from SavesVM (AppSaves: core `save` blobs as JSON in user://saves).

var app: App
var vm: SavesVM:
	get:
		if vm == null:
			vm = SavesVM.new(app.session)
		return vm

func refresh() -> void:
	for c in get_children():
		c.queue_free()
	var m := Lcars.screen_margin(self, Vector4(56, 56, 40, 40))
	var col := Lcars.vbox(18)
	m.add_child(col)
	var head := Lcars.hbox(16)
	head.add_child(Lcars.back_pill(func(): app.back() if not app.view.is_empty() else app.goto("title", false)))
	head.add_child(Lcars.heading("Saved campaigns", 44))
	head.add_child(Lcars.spacer())
	if not app.view.is_empty():
		var name_edit := LineEdit.new()
		name_edit.text = app.save_name
		name_edit.custom_minimum_size.x = 320
		head.add_child(name_edit)
		head.add_child(Lcars.pill("Save current", "secondary", func():
			if app.save_game(name_edit.text.strip_edges()):
				app.toast("Saved.", "good")
				refresh()))
	col.add_child(head)
	var frame := Lcars.frame("Slots", "stored in user://saves · F9 in game", "secondary", "save")
	frame.size_flags_vertical = Control.SIZE_EXPAND_FILL
	col.add_child(frame)
	var list := Lcars.vbox(8)
	vm.fetch()
	var saves := vm.saves
	if saves.is_empty():
		list.add_child(Lcars.empty_note("No saved campaigns yet. Campaigns autosave at the end of every turn."))
	var grid := HFlowContainer.new()
	grid.add_theme_constant_override("h_separation", 12)
	grid.add_theme_constant_override("v_separation", 12)
	list.add_child(grid)
	for s in saves:
		var row := Lcars.card()
		row.custom_minimum_size = Vector2(620, 0)
		var h := Lcars.hbox(14)
		row.add_child(h)
		var tex := AppSaves.thumb_texture(s)
		if tex != null:
			# F5: the screen at the save, framed like the portrait it replaces.
			var tr0 := TextureRect.new()
			tr0.name = "SaveThumb"
			tr0.texture = tex
			tr0.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
			tr0.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
			tr0.custom_minimum_size = Vector2(171, 96)
			h.add_child(tr0)
		else:
			h.add_child(PortraitSlot.make(int(s.get("player", 0)), Vector2(96, 96)))
		var t := Lcars.vbox(2)
		t.add_child(Lcars.label(str(s.get("name", "?")), "SubHeading", 22, Lcars.TEXT))
		var tr := Lcars.hbox(8)
		tr.add_child(UiIcon.make("turn", 16, Lcars.accent()))
		tr.add_child(Lcars.label("Stardate %s" % TrekText.stardate_text(int(s.get("turn", 0))), "", 18, Lcars.accent()))
		t.add_child(tr)
		t.add_child(Lcars.caption(SavesVM.describe(s)))
		t.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		h.add_child(t)
		var blob: Dictionary = s
		h.add_child(Lcars.pill("Load", "primary", func(): app.load_game(blob)))
		var file: String = s["file"]
		h.add_child(Lcars.pill("Delete", "alert", func(): app.delete_save(file); refresh()))
		grid.add_child(row)
	Lcars.body(frame).add_child(Lcars.scroll(list))
