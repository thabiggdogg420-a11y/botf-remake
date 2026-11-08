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
	head.add_child(Lcars.pill("Back", "ghost", func(): app.back() if not app.view.is_empty() else app.goto("title", false), "", "", "Esc"))
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
	var frame := Lcars.frame("Slots", "stored in user://saves", "secondary")
	frame.size_flags_vertical = Control.SIZE_EXPAND_FILL
	col.add_child(frame)
	var list := Lcars.vbox(8)
	vm.load()
	var saves := vm.saves
	if saves.is_empty():
		list.add_child(Lcars.empty_note("No saved campaigns yet. Campaigns autosave at the end of every turn."))
	for s in saves:
		var row := Lcars.card()
		var h := Lcars.hbox(14)
		row.add_child(h)
		h.add_child(Lcars.emblem(int(s.get("player", 0)), 34))
		var t := Lcars.vbox(0)
		t.add_child(Lcars.label(str(s.get("name", "?")), "SubHeading", 22, Lcars.TEXT))
		t.add_child(Lcars.caption(SavesVM.describe(s)))
		t.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		h.add_child(t)
		var blob: Dictionary = s
		h.add_child(Lcars.pill("Load", "primary", func(): app.load_game(blob)))
		var file: String = s["file"]
		h.add_child(Lcars.pill("Delete", "alert", func(): app.delete_save(file); refresh()))
		list.add_child(row)
	Lcars.body(frame).add_child(Lcars.scroll(list))
