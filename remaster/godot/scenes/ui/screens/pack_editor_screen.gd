extends Control
## Pack editor (lane MOD-10, docs/design/MOD-TOOLS.md), reached from the title menu. Create, edit and save a user pack: an overlay
## (ship classes, structures, techs, races and minors, balance values, era windows) and a preset (rules, lineup, new-game
## defaults) in the user data folder. Every edit goes to the core's own validators (PackEditorVM.validate) and a refusal is shown
## inline, on the tab it is about and in the status line. The shipped packs are never edited. Data and rules live in PackEditorVM.

var app: App
var vm: PackEditorVM:
	get:
		if vm == null:
			vm = PackEditorVM.new(app.session if app else null)
			vm.new_pack()
		return vm
var tab := "identity"
var filter := ""
var selected := {}              ## section -> id
var note := ""                  ## the last field-level message (a value that is not a number)
var _content: VBoxContainer
var _status: VBoxContainer
var _tabs_row: HBoxContainer
var _saved_note := ""

func refresh() -> void:
	for c in get_children():
		remove_child(c)   # out of the tree now, so a deferred find_child never reaches the old copy
		c.queue_free()
	UiTokens.era = str(app.settings.get("uiEra", "tng")) if app else "tng"
	var margin := Lcars.screen_margin(self, Vector4(48, 48, 28, 24))
	var col := Lcars.vbox(12)
	margin.add_child(col)
	var head := Lcars.hbox(18)
	head.add_child(Lcars.pill("Back", "ghost", _back, "", "", "Esc"))
	head.add_child(Lcars.heading("Pack editor", 46))
	var sub := Lcars.caption("Make your own pack: data, rules and powers. Shipped packs are never changed.")
	sub.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	head.add_child(sub)
	col.add_child(head)
	if not vm.supported():
		col.add_child(Lcars.para("This build's simulation core cannot take user packs, so the pack editor is switched off.", Lcars.WARN, 18))
		return
	col.add_child(_toolbar())
	_tabs_row = HBoxContainer.new()
	col.add_child(_tabs_row)
	_build_tabs()
	var scroll := ScrollContainer.new()
	scroll.name = "EditorScroll"
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	col.add_child(scroll)
	_content = Lcars.vbox(10)
	_content.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(_content)
	_status = Lcars.vbox(4)
	_status.name = "Status"
	col.add_child(_status)
	_rebuild_content()
	_update_status()

func _back() -> void:
	if vm.dirty:
		app.toast("Unsaved changes to the pack were discarded.", "warn")
	app.goto("title", false)

# ---- toolbar, tabs, status ------------------------------------------------------------------------------------------------------
func _toolbar() -> Control:
	var row := Lcars.hbox(10)
	row.name = "Toolbar"
	var saved := vm.saved_packs()
	var pick := OptionButton.new()
	pick.name = "OpenPack"
	pick.custom_minimum_size.x = 300
	pick.add_item("Open a saved pack (%d)" % saved.size())
	for p in saved:
		pick.add_item("%s  (%s)" % [p["name"], p["id"]])
	pick.item_selected.connect(_open_saved.bind(saved))
	row.add_child(pick)
	var np := Lcars.pill("New pack", "secondary", func(): vm.new_pack(); selected = {}; _saved_note = ""; _build_on = ""; tab = "identity"; refresh(), "", "Start an empty pack")
	np.name = "NewPack"
	row.add_child(np)
	var sv := Lcars.pill("Save pack", "primary", _save, "", "Check the pack with the core, then write it to your data folder")
	sv.name = "SavePack"
	row.add_child(sv)
	var play := Lcars.pill("New game with this pack", "good", _play, "" if vm.pack_id != "" and not vm.dirty and UserPacks.ids().has(vm.pack_id) else "Save the pack first", "Open the new campaign screen with this pack picked")
	play.name = "PlayPack"
	row.add_child(play)
	var del := Lcars.pill("Delete pack", "alert", _delete, "" if UserPacks.ids().has(vm.pack_id) else "Not saved yet", "Remove this pack's files from your data folder (saves made with it will not load)")
	del.name = "DeletePack"
	row.add_child(del)
	return row

func _open_saved(i: int, saved: Array) -> void:
	if i > 0 and vm.load_pack(str(saved[i - 1]["id"])):
		selected = {}
		_saved_note = ""
		_build_on = ""
	refresh()

func _build_tabs() -> void:
	for c in _tabs_row.get_children():
		c.queue_free()
	_tabs_row.add_theme_constant_override("separation", 6)
	for t in PackEditorVM.TABS:
		var id: String = t[0]
		var bad: bool = vm.error != "" and vm.error_tab == id
		var tg := Lcars.toggle(str(t[1]) + ("  !" if bad else ""), tab == id, func(_on): tab = id; filter = ""; note = ""; _build_tabs(); _rebuild_content())
		tg.name = "Tab_" + id
		if bad:
			tg.add_theme_color_override("font_color", Lcars.ALERT)
		_tabs_row.add_child(tg)

func _update_status() -> void:
	if _status == null:
		return
	for c in _status.get_children():
		c.queue_free()
	var line := Lcars.hbox(12)
	if vm.error == "":
		line.add_child(Lcars.badge("Valid", "good"))
		line.add_child(Lcars.label("Stack %s · checksum %s" % [vm.status.get("stackId", ""), str(vm.status.get("checksum", ""))], "Mono", 14, Lcars.TEXT2))
	else:
		line.add_child(Lcars.badge("Refused", "alert"))
		var msg := Lcars.para(vm.error, Lcars.ALERT, 15)
		msg.name = "ErrorText"
		msg.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		line.add_child(msg)
	_status.add_child(line)
	var s := vm.summary()
	var info := (" · ".join(s) if not s.is_empty() else "No changes yet") + ("   ·   " + _saved_note if _saved_note != "" else "")
	var l := Lcars.label(info, "Muted", 13)
	l.name = "Summary"
	_status.add_child(l)

func _save() -> void:
	var err := vm.save()
	if err != "":
		app.toast("Pack not saved: " + err, "alert", 6.0)
		_saved_note = ""
	else:
		_saved_note = "Saved to %s" % ProjectSettings.globalize_path(UserPacks.dir)
		app.toast("Pack \"%s\" saved. It is in the preset picker now." % vm.pack_name, "good")
	refresh()

func _play() -> void:
	var id := vm.pack_id
	app.goto("new")
	var nc := app.current_screen()
	if nc != null and nc.get("vm") != null:
		nc.vm.pick_preset(id)
		nc.refresh()

func _delete() -> void:
	var id := vm.pack_id
	vm.delete_pack()
	selected = {}
	_saved_note = ""
	app.toast("Pack \"%s\" removed." % id, "warn")
	refresh()

## A field changed: re-validate happened in the model; redraw the status and the tab marks without rebuilding the editor under the cursor.
func _edited(msg: String = "") -> void:
	note = msg
	_build_tabs.call_deferred()
	_update_status.call_deferred()
	if msg != "":
		_rebuild_content.call_deferred()

# ---- content ------------------------------------------------------------------------------------------------------------------------
func _rebuild_content() -> void:
	if _content == null:
		return
	for c in _content.get_children():
		_content.remove_child(c)
		c.queue_free()
	if vm.error != "" and vm.error_tab == tab:
		var inline := Lcars.para("The core refused this part of the pack: " + vm.error, Lcars.ALERT, 15)
		inline.name = "InlineError"
		_content.add_child(inline)
	if note != "":
		_content.add_child(Lcars.para(note, Lcars.WARN, 14))
	match tab:
		"identity": _content.add_child(_identity())
		"balance": _content.add_child(_balance())
		"eras": _content.add_child(_eras())
		"preset": _content.add_child(_preset())
		_: _content.add_child(_records(tab))

## A text box that commits on Enter or when it loses focus.
func _line(text: String, on_commit: Callable, width: int = 160, name_: String = "") -> LineEdit:
	var le := LineEdit.new()
	le.text = text
	le.custom_minimum_size.x = width
	if name_ != "":
		le.name = name_
	var last := [text]
	var commit := func(t: String):
		if t != last[0]:
			last[0] = t
			on_commit.call(t)
	le.text_submitted.connect(commit)
	le.focus_exited.connect(func(): commit.call(le.text))
	return le

func _check(text: String, on: bool, on_toggle: Callable, name_: String = "", hint: String = "") -> CheckBox:
	var c := CheckBox.new()
	c.text = text
	if text != "":
		c.custom_minimum_size.x = 36 + text.length() * 9   # the box plus the text; a flow or grid cell never clips the label
	c.button_pressed = on
	c.tooltip_text = hint
	if name_ != "":
		c.name = name_
	c.toggled.connect(on_toggle)
	return c

func _choice(cap: String, items: Array, current: String, on_pick: Callable) -> Control:
	var box := Lcars.vbox(4)
	box.add_child(Lcars.caption(cap))
	var row := HFlowContainer.new()
	row.add_theme_constant_override("h_separation", 6)
	row.add_theme_constant_override("v_separation", 6)
	for it in items:
		var id := str(it[0])
		var t := Lcars.toggle(str(it[1]), current == id, func(_on): on_pick.call(id); _rebuild_content())
		t.name = "Pick_" + id
		row.add_child(t)
	box.add_child(row)
	return box

func _identity() -> Control:
	var frame := Lcars.frame("Pack", "who it is and what it builds on", "secondary", "settings")
	var b := Lcars.body(frame)
	b.add_theme_constant_override("separation", 10)
	var grid := Lcars.grid(2)
	grid.add_theme_constant_override("h_separation", 14)
	grid.add_theme_constant_override("v_separation", 8)
	grid.add_child(Lcars.caption("Pack id"))
	grid.add_child(_line(vm.pack_id, func(t): vm.set_identity(t, vm.pack_name, vm.blurb); _edited(), 360, "PackId"))
	grid.add_child(Lcars.caption("Name"))
	grid.add_child(_line(vm.pack_name, func(t): vm.set_identity(vm.pack_id, t, vm.blurb); _edited(), 360, "PackName"))
	grid.add_child(Lcars.caption("Blurb"))
	grid.add_child(_line(vm.blurb, func(t): vm.set_identity(vm.pack_id, vm.pack_name, t); _edited(), 760, "PackBlurb"))
	b.add_child(grid)
	b.add_child(Lcars.para("The id names the files in your data folder (%s). Saving under another id makes a second pack. The ids of shipped packs are taken." % ProjectSettings.globalize_path(UserPacks.dir), Lcars.TEXT3, 13))
	if not UserPacks.ids().has(vm.pack_id):
		var items := [["", "Nothing (the base game)"]]
		items.append_array(vm.build_on_choices())
		b.add_child(_choice("Build on", items, _build_on, func(id):
			_build_on = id
			var keep_id := vm.pack_id; var keep_name := vm.pack_name; var keep_blurb := vm.blurb
			vm.new_pack(id)
			vm.set_identity(keep_id, keep_name, keep_blurb)
			selected = {}))
		b.add_child(Lcars.para("Building on a preset puts its packs under yours and starts your rules, powers and options from it. Your changes sit on top.", Lcars.TEXT3, 13))
	else:
		b.add_child(Lcars.para("Built on: " + (", ".join(vm.under) if not vm.under.is_empty() else "the base game"), Lcars.TEXT2, 14))
	return frame

var _build_on := ""

# Records: a filter and list on the left, the selected record's fields on the right.
func _records(section: String) -> Control:
	var def := PackEditorVM.section_def(section)
	var frame := Lcars.frame(str(def["label"]), "pick one to edit; changed values are marked", "primary", "database")
	var b := Lcars.body(frame)
	var split := Lcars.hbox(16)
	b.add_child(split)
	var left := Lcars.vbox(8)
	left.custom_minimum_size.x = 380
	split.add_child(left)
	var f := LineEdit.new()
	f.placeholder_text = "Filter by name or id"
	f.name = "Filter"
	f.text = filter
	left.add_child(f)
	var list := ItemList.new()
	list.name = "RecordList"
	list.custom_minimum_size = Vector2(380, 520)
	list.size_flags_vertical = Control.SIZE_EXPAND_FILL
	left.add_child(list)
	f.text_changed.connect(func(t): filter = t; _fill_list(section, list))
	list.item_selected.connect(func(i):
		selected[section] = int(list.get_item_metadata(i))
		note = ""
		_rebuild_content())
	var right := Lcars.vbox(8)
	right.name = "Detail"
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	split.add_child(right)
	_fill_list(section, list)
	_fill_detail(section, right)
	return frame

func _fill_list(section: String, list: ItemList = null) -> void:
	if list == null:
		list = find_child("RecordList", true, false) as ItemList
	if list == null or not is_instance_valid(list):
		return
	list.clear()
	var want := int(selected.get(section, -1))
	var rows := vm.rows(section, filter)
	if want < 0 and not rows.is_empty():
		want = int(rows[0]["id"])
		selected[section] = want
	for r in rows:
		var mark := ("+ " if r["added"] else ("* " if r["changed"] else ""))
		list.add_item("%s%d  %s" % [mark, r["id"], r["name"]])
		list.set_item_metadata(list.item_count - 1, int(r["id"]))
		if int(r["id"]) == want:
			list.select(list.item_count - 1)
			list.ensure_current_is_visible()

func _fill_detail(section: String, box: VBoxContainer) -> void:
	var def := PackEditorVM.section_def(section)
	var rows := vm.rows(section, "")
	var want := int(selected.get(section, -1))
	if want < 0 and not rows.is_empty():
		want = int(rows[0]["id"])
		selected[section] = want
	if want < 0:
		box.add_child(Lcars.empty_note("Nothing to edit here."))
		return
	var is_added := rows.any(func(r): return int(r["id"]) == want and r["added"])
	var head := Lcars.hbox(10)
	head.add_child(Lcars.heading("%s  #%d" % [vm.field_value(section, want, str(def["name"])), want], 26))
	if is_added:
		head.add_child(Lcars.badge("added by this pack", "good"))
	elif vm.patches.get(section, {}).has(want):
		head.add_child(Lcars.badge("changed", "warn"))
	box.add_child(head)
	var grid := Lcars.grid(3)
	grid.add_theme_constant_override("h_separation", 14)
	grid.add_theme_constant_override("v_separation", 6)
	for fd in def["fields"]:
		var key := str(fd["k"])
		var cur: Variant = vm.field_value(section, want, key)
		var label_text: String = key.get_slice(".", key.get_slice_count(".") - 1)
		grid.add_child(Lcars.caption(label_text))
		var nm := "F_" + key.replace(".", "_")
		var base: Variant = vm.base_value(section, want, key)
		if str(fd["t"]) == "bool":
			grid.add_child(_check("", bool(cur), func(on): _commit(section, want, key, on), nm))
		else:
			grid.add_child(_line("" if cur == null else str(cur), func(t): _commit(section, want, key, t), 520 if str(fd["t"]) == "str" else 160, nm))
		var was := Lcars.label("was %s" % str(base) if vm.is_changed(section, want, key) else "", "Muted", 13)
		grid.add_child(was)
	box.add_child(grid)
	var actions := Lcars.hbox(10)
	if vm.patches.get(section, {}).has(want):
		actions.add_child(Lcars.pill("Revert this record", "ghost", func(): vm.revert(section, want); _edited(); _rebuild_content()))
	if bool(def.get("clone", false)):
		var nn := LineEdit.new()
		nn.name = "CopyName"
		nn.placeholder_text = "Name for the copy"
		nn.custom_minimum_size.x = 240
		actions.add_child(nn)
		actions.add_child(Lcars.pill("Add as a new record", "secondary", _add_copy.bind(section, want, nn), "", "Copy this record under a new id and edit the copy"))
		if (vm.appends.get(section, []) as Array).size() > 0:
			actions.add_child(Lcars.pill("Remove the last added", "ghost", func(): vm.remove_last_added(section); selected.erase(section); _edited(); _rebuild_content()))
	box.add_child(actions)

func _add_copy(section: String, source: int, name_edit: LineEdit) -> void:
	var def := PackEditorVM.section_def(section)
	var nm := name_edit.text if name_edit.text.strip_edges() != "" else "New %s" % str(def["label"]).to_lower()
	var id := vm.add_record(section, source, nm)
	if id >= 0:
		selected[section] = id
	_edited()
	_rebuild_content()

func _commit(section: String, id: int, key: String, value: Variant) -> void:
	var msg := vm.set_field(section, id, key, value)
	_edited(msg)
	_fill_list.call_deferred(section)
	if msg == "":
		_refresh_detail.call_deferred(section)

func _refresh_detail(section: String) -> void:
	var right := find_child("Detail", true, false) as VBoxContainer
	if right == null:
		return
	for c in right.get_children():
		right.remove_child(c)
		c.queue_free()
	_fill_detail(section, right)

func _balance() -> Control:
	var frame := Lcars.frame("Balance", "pack-wide values; 100 is the base game", "tertiary", "settings")
	var b := Lcars.body(frame)
	var grid := Lcars.grid(3)
	grid.add_theme_constant_override("h_separation", 14)
	grid.add_theme_constant_override("v_separation", 6)
	for item in PackEditorVM.BALANCE:
		var key: String = item["k"]
		grid.add_child(Lcars.label(str(item["label"]), "", 15))
		grid.add_child(_line(str(vm.balance_value(key)), func(t): _edit_balance(key, t), 120, "B_" + key))
		grid.add_child(Lcars.label("set here" if vm.balance.has(key) else "base %d" % vm.balance_default(key), "Muted", 13))
	b.add_child(grid)
	return frame

func _edit_balance(key: String, text: String) -> void:
	_edited(vm.set_balance(key, text))
	_rebuild_content.call_deferred()

func _eras() -> Control:
	var frame := Lcars.frame("Eras", "timeline, tech levels and class windows", "secondary", "galaxy")
	var b := Lcars.body(frame)
	var on := vm.has_timeline()
	var sw := _check("This pack has an era timeline", on, func(v): vm.set_timeline(v); _edited(); _rebuild_content(), "TimelineOn",
		"Needs the expanded roster and the eras rule; switching it on sets both on the Rules tab" if not vm.timeline_inherited() else "The timeline comes from a pack under this one; edit its values here")
	if vm.timeline_inherited():
		sw.disabled = true
	b.add_child(sw)
	if not on:
		b.add_child(Lcars.para("Without a timeline every ship is available from the start era. Switch it on to give each class years in service.", Lcars.TEXT3, 13))
		return frame
	var block := vm.era_block()
	var grid := Lcars.grid(6)
	grid.add_theme_constant_override("h_separation", 12)
	grid.add_theme_constant_override("v_separation", 6)
	grid.add_child(Lcars.caption("Turns per year"))
	grid.add_child(_line(str(block.get("turnsPerYear", 4)), func(t): _edit_era("turnsPerYear", 0, t), 80, "E_turnsPerYear"))
	grid.add_child(Lcars.caption("Tech cap"))
	grid.add_child(_line(str(block.get("techCap", 10)), func(t): _edit_era("techCap", 0, t), 80, "E_techCap"))
	grid.add_child(Control.new())
	grid.add_child(Control.new())
	b.add_child(grid)
	var years := Lcars.grid(6)
	years.add_theme_constant_override("h_separation", 12)
	years.add_theme_constant_override("v_separation", 6)
	years.add_child(Lcars.caption("Start era"))
	for n in PackEditorVM.ERA_LABELS:
		years.add_child(Lcars.caption(n))
	years.add_child(Lcars.caption("Start year"))
	for i in 5:
		years.add_child(_line(str((block.get("years", PackEditorVM.ERA_DEFAULT["years"]) as Array)[i]), func(t): _edit_era("years", i, t), 80, "E_years_%d" % i))
	years.add_child(Lcars.caption("Start tech"))
	for i in 5:
		years.add_child(_line(str((block.get("startTech", PackEditorVM.ERA_DEFAULT["startTech"]) as Array)[i]), func(t): _edit_era("startTech", i, t), 80, "E_startTech_%d" % i))
	b.add_child(years)
	b.add_child(Lcars.caption("Class windows: first and last year in service (0 = no limit)"))
	var wins := vm.era_windows()
	var wg := Lcars.grid(5)
	wg.add_theme_constant_override("h_separation", 12)
	wg.add_theme_constant_override("v_separation", 4)
	for w in wins:
		var wid := int(w["id"])
		wg.add_child(Lcars.label("%d  %s" % [wid, str(vm.field_value("roster", wid, "className") if wid >= 1000 else vm.field_value("ships", wid, "name"))], "", 14))
		wg.add_child(_line(str(w["from"]), func(t): _edit_window(wid, t, str(w["until"])), 80))
		wg.add_child(_line(str(w["until"]), func(t): _edit_window(wid, str(w["from"]), t), 80))
		wg.add_child(Lcars.pill("Remove", "ghost", func(): vm.remove_era_window(wid); _edited(); _rebuild_content()))
		wg.add_child(Control.new())
	b.add_child(wg)
	var add := Lcars.hbox(8)
	var cls := OptionButton.new()
	cls.name = "WindowClass"
	cls.custom_minimum_size.x = 320
	for r in vm.rows("roster"):
		cls.add_item("%d  %s" % [r["id"], r["name"]])
		cls.set_item_metadata(cls.item_count - 1, int(r["id"]))
	add.add_child(cls)
	var fr := _line("2300", func(_t): pass, 80, "WinFrom")
	var un := _line("0", func(_t): pass, 80, "WinUntil")
	add.add_child(fr)
	add.add_child(un)
	add.add_child(Lcars.pill("Add window", "secondary", _add_window.bind(cls, fr, un)))
	b.add_child(add)
	return frame

func _add_window(cls: OptionButton, from_edit: LineEdit, until_edit: LineEdit) -> void:
	if cls.item_count > 0:
		_edit_window(int(cls.get_item_metadata(cls.selected)), from_edit.text, until_edit.text)

func _edit_era(key: String, index: int, text: String) -> void:
	_edited(vm.set_era(key, index, text))
	_rebuild_content.call_deferred()

func _edit_window(id: int, from_t: String, until_t: String) -> void:
	_edited(vm.set_era_window(id, from_t, until_t))
	_rebuild_content.call_deferred()

func _preset() -> Control:
	var frame := Lcars.frame("Rules and lineup", "what the preset switches on and who plays", "tertiary", "settings")
	var b := Lcars.body(frame)
	b.add_theme_constant_override("separation", 12)
	b.add_child(_choice("Rule base", [["classic", "Classic"], ["remastered", "Remastered"]], vm.rules_base, func(id): vm.set_rules_base(id); _edited()))
	var fl := GridContainer.new()
	fl.columns = 5
	fl.add_theme_constant_override("h_separation", 24)
	fl.add_theme_constant_override("v_separation", 4)
	for name_ in PackEditorVM.FLAGS:
		fl.add_child(_check(name_, vm.flag_value(name_), func(on): vm.set_flag(name_, on); vm.validate(); _edited(), "Flag_" + name_))
	b.add_child(Lcars.caption("Rule flags"))
	b.add_child(fl)
	b.add_child(Lcars.caption("Powers in play, in seating order"))
	var lr := GridContainer.new()
	lr.columns = 4
	lr.add_theme_constant_override("h_separation", 24)
	lr.add_theme_constant_override("v_separation", 6)
	for e in PackEditorVM.EMPIRES:
		var cell := Lcars.hbox(2)
		cell.add_child(_check(e, vm.lineup.has(e), func(on): vm.toggle_empire(e, on); _edited(); _rebuild_content(), "Lineup_" + e))
		if vm.lineup.has(e):
			cell.add_child(Lcars.pill("<", "ghost", func(): vm.move_empire(e, -1); _edited(); _rebuild_content()))
			cell.add_child(Lcars.pill(">", "ghost", func(): vm.move_empire(e, 1); _edited(); _rebuild_content()))
		lr.add_child(cell)
	b.add_child(lr)
	b.add_child(Lcars.para("Seating order: " + " · ".join(vm.lineup), Lcars.TEXT2, 13))
	var o := vm.options
	b.add_child(_choice("Powers at start", range(2, 9).map(func(n): return [str(n), str(n)]), str(o.get("empires", 5)), func(id): vm.set_option("empires", int(id)); _edited()))
	b.add_child(_choice("Start era", range(1, 6).map(func(n): return [str(n), PackEditorVM.ERA_LABELS[n - 1]]), str(o.get("era", 1)), func(id): vm.set_option("era", int(id)); _edited()))
	b.add_child(_choice("Galaxy size", PackEditorVM.SIZES.map(func(s): return [s, s.capitalize()]), str(o.get("size", "medium")), func(id): vm.set_option("size", id); _edited()))
	b.add_child(_choice("Galaxy shape", PackEditorVM.SHAPES.map(func(s): return [s, s.capitalize()]), str(o.get("shape", "spiral")), func(id): vm.set_option("shape", id); _edited()))
	b.add_child(_choice("Minor races", PackEditorVM.DENSITIES.map(func(s): return [s, s.capitalize()]), str(o.get("minorDensity", "some")), func(id): vm.set_option("minorDensity", id); _edited()))
	b.add_child(_choice("Difficulty", PackEditorVM.DIFFICULTIES.map(func(s): return [s, s.capitalize()]), str(o.get("difficulty", "normal")), func(id): vm.set_option("difficulty", id); _edited()))
	var sw := Lcars.hbox(18)
	sw.add_child(_check("Galactic events", bool(o.get("events", true)), func(on): vm.set_option("events", on); _edited(), "OptEvents"))
	sw.add_child(_check("Rival aggression", bool(o.get("aiAggression", true)), func(on): vm.set_option("aiAggression", on); _edited(), "OptAggression"))
	b.add_child(sw)
	return frame
