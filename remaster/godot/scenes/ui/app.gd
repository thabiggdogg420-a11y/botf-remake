class_name App
extends Control
## Root of the strategic client: application state, the LCARS shell (top bar, rail), screen
## switching, overlays, toasts and keyboard shortcuts. Port of remaster/web/src/App.tsx and
## state/store.ts. Screens are plain Controls with `app` and `refresh()`.

signal view_changed
signal selection_changed
signal screen_changed(id: String)

static var i: App

const GAME_SCREENS := [
	{"id": "galaxy", "label": "Galaxy", "key": "F1", "hint": "Star chart, fleets and orders", "script": "res://scenes/galaxy/galaxy_screen.gd"},
	{"id": "system", "label": "System", "key": "F2", "hint": "Colony management", "script": "res://scenes/ui/screens/system_screen.gd"},
	{"id": "empire", "label": "Empire", "key": "F3", "hint": "All colonies and fleets", "script": "res://scenes/ui/screens/empire_screen.gd"},
	{"id": "research", "label": "Research", "key": "F4", "hint": "Technology and allocation", "script": "res://scenes/ui/screens/research_screen.gd"},
	{"id": "fleets", "label": "Fleets", "key": "F5", "hint": "Ships and task forces", "script": "res://scenes/ui/screens/fleets_screen.gd"},
	{"id": "diplomacy", "label": "Diplomacy", "key": "F6", "hint": "Empires, minors and treaties", "script": "res://scenes/ui/screens/diplomacy_screen.gd"},
	{"id": "intel", "label": "Intel", "key": "F7", "hint": "Espionage and security", "script": "res://scenes/ui/screens/intel_screen.gd"},
	{"id": "database", "label": "Database", "key": "F8", "hint": "Ships, technologies, structures", "script": "res://scenes/ui/screens/database_screen.gd"},
]
const OUTSIDE_SCREENS := {
	"title": "res://scenes/ui/screens/title_screen.gd",
	"new": "res://scenes/ui/screens/new_campaign_screen.gd",
	"saves": "res://scenes/ui/screens/saves_screen.gd",
	"combat": "res://scenes/ui/screens/combat_screen.gd",
}
const SETTINGS_PATH := "user://botf_settings.json"

var core: CoreClient
var meta: Dictionary = {}
var view: Dictionary = {}
var encyclopedia: Dictionary = {}
var screen := ""
var history: Array[String] = []
var selection := {"cell": -1, "systemId": 0, "fleetId": 0, "taskForceId": 0}
var briefing: Variant = null
var encounter: Variant = null
var save_name := ""
var settings := {"uiScale": 1.0, "colorblind": false, "reducedMotion": false, "autosave": true, "confirmEndTurn": false, "volume": 0.7, "showGrid": true}
var map_layers := {"territory": true, "supply": true, "routes": true, "trade": true, "grid": true, "labels": true, "fog": true}

var _background: ColorRect
var _stage: Control
var _chrome: Control
var _topbar: Control
var _rail: VBoxContainer
var _overlay: Control
var _toasts: VBoxContainer
var _current: Control
var _modals: Array[Control] = []
var _theme_empire := -1

func _init() -> void:
	i = self

func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	_load_settings()
	core = CoreClient.create()
	var m: Variant = core.request("meta")
	meta = m if m is Dictionary else {}
	_apply_theme(1)
	_build_layers()
	goto("title")

# ---------------------------------------------------------------------------------------------
# Layers and chrome
# ---------------------------------------------------------------------------------------------
func _build_layers() -> void:
	var base := ColorRect.new()
	base.color = Lcars.BG
	base.set_anchors_preset(Control.PRESET_FULL_RECT)
	base.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(base)
	_background = ColorRect.new()
	_background.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mat := ShaderMaterial.new()
	mat.shader = load("res://scenes/galaxy/starfield.gdshader")
	_background.material = mat
	_background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_background)
	_stage = Control.new()
	_stage.name = "Stage"
	_stage.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(_stage)
	_chrome = Control.new()
	_chrome.name = "Chrome"
	_chrome.set_anchors_preset(Control.PRESET_FULL_RECT)
	_chrome.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_chrome)
	_overlay = Control.new()
	_overlay.name = "Overlay"
	_overlay.set_anchors_preset(Control.PRESET_FULL_RECT)
	_overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_overlay)
	_toasts = VBoxContainer.new()
	_toasts.name = "Toasts"
	_toasts.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
	_toasts.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_toasts.grow_vertical = Control.GROW_DIRECTION_BEGIN
	_toasts.offset_bottom = -90
	_toasts.alignment = BoxContainer.ALIGNMENT_END
	_toasts.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_toasts)

func _apply_theme(empire: int) -> void:
	Lcars.colorblind = settings.get("colorblind", false)
	if empire == _theme_empire:
		return
	_theme_empire = empire
	theme = Lcars.build_theme(empire)
	if _background:
		(_background.material as ShaderMaterial).set_shader_parameter("accent", Lcars.accent())

const TOP_H := 74.0
const RAIL_W := 150.0

func _build_chrome() -> void:
	for c in _chrome.get_children():
		c.queue_free()
	if view.is_empty() or screen in ["title", "new", "saves"]:
		return
	var a := Lcars.accent()
	# Elbow (top-left LCARS sweep) with the faction emblem.
	var elbow := ElbowView.new()
	elbow.position = Vector2.ZERO
	elbow.size = Vector2(RAIL_W + 40, TOP_H + 40)
	elbow.empire = int(view["meta"]["player"])
	_chrome.add_child(elbow)
	# Top bar
	var top := PanelContainer.new()
	top.add_theme_stylebox_override("panel", Lcars.box(Color(Lcars.BG, 0.82), 0, Color.TRANSPARENT, 0, Vector4(14, 6, 18, 6)))
	top.position = Vector2(RAIL_W + 46, 0)
	top.size = Vector2(size.x - RAIL_W - 46, TOP_H - 14)
	top.mouse_filter = Control.MOUSE_FILTER_STOP
	_chrome.add_child(top)
	_topbar = top
	var row := Lcars.hbox(26)
	top.add_child(row)
	var e: Dictionary = view["empire"]
	var who := Lcars.vbox(0)
	who.add_child(Lcars.label(str(view["meta"]["playerName"]).to_upper(), "Heading", 26))
	who.add_child(Lcars.caption("%s era · %s rules · stardate %d.%d" % [view["meta"].get("eraName", ""), view["meta"].get("rules", {}).get("presetName", ""), 2360 + int(view["meta"]["turn"]) / 4, (int(view["meta"]["turn"]) % 4) * 25]))
	row.add_child(who)
	row.add_child(_top_stat("Turn", str(int(view["meta"]["turn"])), "", "Current turn. Enter ends the turn."))
	var net := int(e.get("income", {}).get("net", 0))
	row.add_child(_top_stat("Credits", "%s  %s" % [Lcars.fmt(e.get("credits", 0)), Lcars.signed(net)], "alert" if net < 0 else "secondary",
		"Treasury ¢%s. Per turn: colonies %s, trade %s, ships -%s. Drafted spending ¢%s." % [Lcars.fmt(e.get("credits", 0)), e.get("income", {}).get("colonies", 0), e.get("income", {}).get("trade", 0), e.get("income", {}).get("ships", 0), e.get("draftCredits", 0)]))
	row.add_child(_top_stat("Research", "%d/t" % int(e.get("researchOutput", 0)), "", "Research points per turn across the empire. Reserve %s." % e.get("researchReserve", 0)))
	row.add_child(_top_stat("Industry", str(int(e.get("industry", 0))), "", "Total industry per turn of all colonies."))
	row.add_child(_top_stat("Food", str(int(e.get("food", 0))), "good", "Total food produced per turn."))
	row.add_child(_top_stat("Pop", str(int(e.get("population", 0))), "", "Total population."))
	row.add_child(_top_stat("Intel", str(int(e.get("intelligence", 0))), "tertiary", "Intelligence points per turn."))
	row.add_child(Lcars.spacer())
	var pending: Array = view.get("pending", [])
	row.add_child(_top_stat("Orders", str(pending.size()), "", "Orders drafted this turn. They resolve when the turn ends."))
	var menu := Lcars.pill("Menu", "tertiary", func(): open_menu(), "", "Save, load, settings and help", "Esc")
	menu.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	row.add_child(menu)
	var end := Lcars.pill("End turn", "secondary", func(): request_end_turn(), "Finish the battle in progress first" if view.get("session", {}).get("active", false) else "", "Resolve all drafted orders", "Enter")
	end.name = "EndTurn"
	end.custom_minimum_size = Vector2(170, 44)
	end.add_theme_font_size_override("font_size", 21)
	end.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	row.add_child(end)
	# Rail
	var rail_bg := ColorRect.new()
	rail_bg.color = Color(Lcars.BG, 0.82)
	rail_bg.position = Vector2(0, TOP_H + 36)
	rail_bg.size = Vector2(RAIL_W + 8, size.y - TOP_H - 36)
	_chrome.add_child(rail_bg)
	_rail = Lcars.vbox(5)
	_rail.position = Vector2(0, TOP_H + 44)
	_rail.custom_minimum_size = Vector2(RAIL_W, 0)
	_chrome.add_child(_rail)
	for s in GAME_SCREENS:
		var b := Button.new()
		b.theme_type_variation = "RailButton"
		b.text = "%s" % s["label"].to_upper()
		b.alignment = HORIZONTAL_ALIGNMENT_RIGHT
		b.custom_minimum_size = Vector2(RAIL_W, 50)
		b.tooltip_text = "%s  [%s]" % [s["hint"], s["key"]]
		b.name = "Rail_" + s["id"]
		var sid: String = s["id"]
		b.pressed.connect(func(): goto(sid))
		if s["id"] == screen:
			var sb := Lcars.box(Lcars.accent2(), 0, Color.TRANSPARENT, 0, Vector4(14, 8, 14, 8))
			sb.corner_radius_top_right = 18
			sb.corner_radius_bottom_right = 18
			b.add_theme_stylebox_override("normal", sb)
			b.add_theme_stylebox_override("hover", sb)
		var keyl := Lcars.label(s["key"], "Mono", 11, Color(Lcars.ink(), 0.7))
		keyl.position = Vector2(8, 4)
		b.add_child(keyl)
		_rail.add_child(b)
	var fill := ColorRect.new()
	fill.color = a.darkened(0.45)
	fill.custom_minimum_size = Vector2(RAIL_W, 8)
	_rail.add_child(fill)
	var extras := [
		["Events", func(): open_overlay("events"), "Galactic events and decisions"],
		["Victory", func(): open_overlay("victory"), "Victory conditions and megaprojects"],
		["Chronicle", func(): open_overlay("briefing"), "Last turn briefing and chronicle"],
	]
	if view.get("session", {}).get("active", false):
		extras.push_front(["Battle", func(): goto("combat"), "Tactical battle in progress"])
	for ex in extras:
		var b2 := Button.new()
		b2.theme_type_variation = "RailButton"
		b2.text = ex[0].to_upper()
		b2.alignment = HORIZONTAL_ALIGNMENT_RIGHT
		b2.custom_minimum_size = Vector2(RAIL_W, 38)
		b2.add_theme_font_size_override("font_size", 16)
		var sb2 := Lcars.box(Lcars.accent3().darkened(0.2), 0, Color.TRANSPARENT, 0, Vector4(14, 4, 14, 4))
		sb2.corner_radius_top_right = 14
		sb2.corner_radius_bottom_right = 14
		b2.add_theme_stylebox_override("normal", sb2)
		b2.tooltip_text = ex[2]
		b2.pressed.connect(ex[1])
		_rail.add_child(b2)

func _top_stat(cap: String, value: String, tone: String, tip: String) -> Control:
	var v := Lcars.vbox(-2)
	v.add_child(Lcars.caption(cap))
	var l := Lcars.label(value, "", 24, Lcars.tone_color(tone) if tone != "" else Lcars.TEXT)
	l.add_theme_font_override("font", Lcars.font("display", 560))
	v.add_child(l)
	v.tooltip_text = tip
	v.mouse_filter = Control.MOUSE_FILTER_STOP
	v.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	return v

## Area available to in-game screens (right of the rail, below the top bar).
func stage_rect() -> Rect2:
	if view.is_empty() or screen in ["title", "new", "saves"]:
		return Rect2(Vector2.ZERO, size)
	return Rect2(Vector2(RAIL_W + 14, TOP_H + 4), size - Vector2(RAIL_W + 14, TOP_H + 4))

class ElbowView extends Control:
	var empire := 1
	func _ready() -> void:
		mouse_filter = Control.MOUSE_FILTER_IGNORE
	func _draw() -> void:
		var a := Lcars.accent()
		var w := App.RAIL_W
		var h := App.TOP_H - 14
		# LCARS elbow: rail column sweeping into the top bar with an inner radius.
		draw_rect(Rect2(0, 0, w, h + 30), a)
		draw_rect(Rect2(w, 0, 40, h), a)
		draw_rect(Rect2(w, h, 30, 30), a)
		# inner corner cut
		var pts := PackedVector2Array()
		for k in range(0, 17):
			var ang := PI + PI / 2.0 * k / 16.0
			pts.append(Vector2(w + 30, h + 30) + Vector2(cos(ang), sin(ang)) * 30)
		pts.append(Vector2(w + 30, h + 30))
		draw_colored_polygon(pts, Lcars.BG)
		Lcars.draw_emblem(self, empire, Vector2(34, 34), 40, Lcars.ink())
		draw_string(Lcars.font("display", 600), Vector2(62, 44), "LCARS", HORIZONTAL_ALIGNMENT_LEFT, -1, 22, Lcars.ink())

func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED and _chrome:
		_build_chrome()
		if _current:
			_place_current()

# ---------------------------------------------------------------------------------------------
# Navigation
# ---------------------------------------------------------------------------------------------
func _script_for(id: String) -> String:
	for s in GAME_SCREENS:
		if s["id"] == id:
			return s["script"]
	return OUTSIDE_SCREENS.get(id, "")

func goto(id: String, remember: bool = true) -> void:
	if id == screen and _current:
		return
	if id == "system" and not view.is_empty():
		var cols: Array = view.get("colonies", [])
		var ok := false
		for c in cols:
			if int(c["systemId"]) == int(selection["systemId"]):
				ok = true
		if not ok and cols.size() > 0:
			select_system(int(cols[0]["systemId"]))
	if remember and screen != "":
		history.append(screen)
		if history.size() > 12:
			history.pop_front()
	screen = id
	close_all_modals()
	if _current:
		_current.queue_free()
		_current = null
	var path := _script_for(id)
	if path == "":
		return
	var s: Control = load(path).new()
	s.name = "Screen_" + id
	s.set("app", self)
	_current = s
	_stage.add_child(s)
	_place_current()
	_background.visible = id != "galaxy"
	_build_chrome()
	if s.has_method("refresh"):
		s.refresh()
	screen_changed.emit(id)

func _place_current() -> void:
	var r := stage_rect()
	_current.position = r.position
	_current.size = r.size

func current_screen() -> Control:
	return _current

func back() -> void:
	if history.is_empty():
		if screen != "galaxy" and not view.is_empty():
			goto("galaxy", false)
		return
	var prev: String = history.pop_back()
	goto(prev, false)

# ---------------------------------------------------------------------------------------------
# State
# ---------------------------------------------------------------------------------------------
func refresh() -> void:
	var v: Variant = core.request("view")
	if v is Dictionary:
		view = v
		_apply_theme(int(view["meta"]["player"]))
		_build_chrome()
		view_changed.emit()
		if _current and _current.has_method("refresh"):
			_current.refresh()
	else:
		report_error(core.last_error)

func player() -> int:
	return int(view.get("meta", {}).get("player", 0))

func width() -> int:
	return int(view.get("meta", {}).get("width", 1))

func get_encyclopedia() -> Dictionary:
	if encyclopedia.is_empty():
		var e: Variant = core.request("encyclopedia")
		encyclopedia = e if e is Dictionary else {}
	return encyclopedia

func find_by_id(list_key: String, id: int, key: String = "id") -> Dictionary:
	for item in view.get(list_key, []):
		if int(item.get(key, -1)) == id:
			return item
	return {}

func system_at(x: int, y: int) -> Dictionary:
	for s in view.get("systems", []):
		if int(s["x"]) == x and int(s["y"]) == y:
			return s
	return {}

func empire_name(id: int) -> String:
	if id == player():
		return str(view["meta"]["playerName"])
	for e in meta.get("empires", []):
		if int(e["id"]) == id:
			return str(e["name"])
	return "Independent"

func select_cell(cell: int) -> void:
	if cell < 0 or view.is_empty():
		selection = {"cell": -1, "systemId": 0, "fleetId": 0, "taskForceId": 0}
	else:
		var s := system_at(cell % width(), cell / width())
		selection = {"cell": cell, "systemId": int(s.get("id", 0)), "fleetId": 0, "taskForceId": 0}
	selection_changed.emit()

func select_system(system_id: int) -> void:
	var s := find_by_id("systems", system_id)
	if s.is_empty():
		return
	selection = {"cell": int(s["y"]) * width() + int(s["x"]), "systemId": system_id, "fleetId": 0, "taskForceId": 0}
	selection_changed.emit()

func select_fleet(fleet_id: int) -> void:
	var f := find_by_id("fleets", fleet_id)
	if f.is_empty():
		f = find_by_id("contacts", fleet_id)
	if f.is_empty():
		return
	var s := system_at(int(f["x"]), int(f["y"]))
	selection = {"cell": int(f["y"]) * width() + int(f["x"]), "systemId": int(s.get("id", 0)), "fleetId": fleet_id, "taskForceId": int(f.get("taskForceId", 0))}
	selection_changed.emit()

## Submits an order through the core; on rejection shows the core's reason.
func issue(command: Dictionary, cancel_ids: Array = [], success: String = "") -> bool:
	var r := core.invoke("submit", {"command": command, "cancelIds": cancel_ids})
	if not r["ok"]:
		report_error(r["error"])
		return false
	refresh()
	if success != "":
		toast(success, "good")
	return true

func cancel_orders(ids: Array, message: String = "") -> void:
	if ids.is_empty():
		return
	var r := core.invoke("cancel", {"ids": ids})
	if not r["ok"]:
		report_error(r["error"])
		return
	refresh()
	if message != "":
		toast(message, "info")

## The core's reason a command would be rejected, or "" when it is valid.
func dry_run(command: Dictionary, cancel_ids: Array = []) -> String:
	var r := core.invoke("dryRun", {"command": command, "cancelIds": cancel_ids})
	if not r["ok"]:
		return str(r["error"])
	var res: Variant = r["result"]
	if res is Dictionary:
		return "" if res.get("ok", true) else str(res.get("error", "Rejected"))
	return ""

func unit_order_ids(fleet_id: int, task_force_id: int, types: Array = []) -> Array:
	var out := []
	for p in view.get("pending", []):
		if not types.is_empty() and not types.has(p["type"]):
			continue
		if task_force_id != 0:
			if int(p.get("taskForceId", 0)) == task_force_id and p["type"] == "SetTaskForceOrder":
				out.append(int(p["id"]))
		elif int(p.get("fleetId", -1)) == fleet_id and p["type"] != "CreateTaskForce":
			out.append(int(p["id"]))
	return out

# ---------------------------------------------------------------------------------------------
# Campaign lifecycle
# ---------------------------------------------------------------------------------------------
func _enter_game(v: Dictionary, name_: String) -> void:
	view = v
	briefing = null
	encounter = null
	history.clear()
	save_name = name_
	_apply_theme(int(v["meta"]["player"]))
	var cols: Array = v.get("colonies", [])
	if cols.size() > 0:
		select_system(int(cols[0]["systemId"]))
	screen = ""
	goto("galaxy", false)

func start_campaign(options: Dictionary) -> bool:
	var r := core.invoke("newCampaign", options)
	if not r["ok"]:
		report_error(r["error"])
		return false
	_enter_game(r["result"], "%s campaign %s" % [r["result"]["meta"]["playerName"], str(r["result"]["meta"]["seed"])])
	toast("Campaign begins. %s, turn %d." % [view["meta"]["playerName"], int(view["meta"]["turn"])], "good")
	autosave()
	return true

func start_drill(method: String, label_: String) -> void:
	var r := core.invoke(method, {})
	if not r["ok"]:
		report_error(r["error"])
		return
	_enter_game(r["result"], label_)
	toast(label_ + " loaded.", "warn")

func request_end_turn() -> void:
	if view.is_empty() or view.get("session", {}).get("active", false) and not view.get("session", {}).get("battleFinished", false):
		if screen != "combat" and view.get("session", {}).get("active", false):
			goto("combat")
		return
	end_turn()

## Ends the turn. Returns the status ("battle" or "complete").
func end_turn() -> String:
	var r := core.invoke("endTurn", {})
	if not r["ok"]:
		report_error(r["error"])
		return "error"
	var res: Dictionary = r["result"]
	if res.get("status", "") == "battle":
		refresh()
		var e: Variant = core.request("tactical")
		encounter = e
		goto("combat")
		toast("Battle joined. Issue maneuvers or let the tactical AI command.", "warn")
		return "battle"
	briefing = res.get("briefing")
	refresh()
	if screen == "combat":
		goto("galaxy", false)
	autosave()
	var outcome := str(view.get("meta", {}).get("outcome", "ongoing"))
	if outcome != "ongoing":
		open_overlay("end")
	elif briefing is Dictionary:
		open_overlay("briefing")
	return "complete"

# ---------------------------------------------------------------------------------------------
# Saves and settings
# ---------------------------------------------------------------------------------------------
func save_game(name_: String) -> bool:
	var r := core.invoke("save", {})
	if not r["ok"]:
		report_error(r["error"])
		return false
	DirAccess.make_dir_recursive_absolute("user://saves")
	var blob: Dictionary = r["result"]
	blob["name"] = name_
	blob["when"] = Time.get_datetime_string_from_system()
	blob["playerName"] = view.get("meta", {}).get("playerName", "")
	var f := FileAccess.open("user://saves/%s.json" % name_.validate_filename(), FileAccess.WRITE)
	if f == null:
		report_error("Could not write the save file")
		return false
	f.store_string(JSON.stringify(blob))
	f.close()
	save_name = name_
	return true

func list_saves() -> Array:
	var out := []
	var d := DirAccess.open("user://saves")
	if d == null:
		return out
	for fn in d.get_files():
		if fn.ends_with(".json"):
			var data: Variant = JSON.parse_string(FileAccess.get_file_as_string("user://saves/" + fn))
			if data is Dictionary:
				data["file"] = fn
				out.append(data)
	out.sort_custom(func(a, b): return str(a.get("when", "")) > str(b.get("when", "")))
	return out

func load_game(blob: Dictionary) -> bool:
	var r := core.invoke("load", {"data": blob.get("data", ""), "scenario": blob.get("scenario", "campaign")})
	if not r["ok"]:
		report_error(r["error"])
		return false
	_enter_game(r["result"], str(blob.get("name", "Loaded campaign")))
	toast("Loaded %s, turn %d." % [blob.get("name", ""), int(view["meta"]["turn"])], "good")
	if view.get("session", {}).get("active", false):
		encounter = core.request("tactical")
		goto("combat")
	return true

func delete_save(file: String) -> void:
	DirAccess.remove_absolute("user://saves/" + file)

func autosave() -> void:
	if settings.get("autosave", true) and not view.is_empty():
		save_game("Autosave")

func _load_settings() -> void:
	if FileAccess.file_exists(SETTINGS_PATH):
		var s: Variant = JSON.parse_string(FileAccess.get_file_as_string(SETTINGS_PATH))
		if s is Dictionary:
			settings.merge(s, true)
	_apply_scale()

func save_settings() -> void:
	var f := FileAccess.open(SETTINGS_PATH, FileAccess.WRITE)
	if f:
		f.store_string(JSON.stringify(settings))
	Lcars.colorblind = settings.get("colorblind", false)
	_apply_scale()
	_theme_empire = -1
	_apply_theme(player() if not view.is_empty() else 1)
	_build_chrome()
	view_changed.emit()

func _apply_scale() -> void:
	if is_inside_tree():
		get_window().content_scale_factor = float(settings.get("uiScale", 1.0))

# ---------------------------------------------------------------------------------------------
# Toasts, modals and overlays
# ---------------------------------------------------------------------------------------------
func toast(text: String, tone: String = "info", seconds: float = 3.4) -> void:
	if _toasts == null:
		return
	var p := PanelContainer.new()
	var c := Lcars.tone_color(tone)
	var sb := Lcars.box(Color(Lcars.PANEL_SOLID, 0.96), 12, c, 1, Vector4(16, 10, 16, 10))
	sb.border_width_left = 6
	p.add_theme_stylebox_override("panel", sb)
	var l := Lcars.para(text, Lcars.TEXT, 15)
	l.custom_minimum_size.x = 380
	p.add_child(l)
	_toasts.add_child(p)
	while _toasts.get_child_count() > 5:
		_toasts.get_child(0).free()
	get_tree().create_timer(seconds).timeout.connect(func():
		if is_instance_valid(p):
			var tw := p.create_tween()
			tw.tween_property(p, "modulate:a", 0.0, 0.3)
			tw.tween_callback(p.queue_free))

func report_error(message: String) -> void:
	toast(message if message != "" else "Unknown error", "alert", 6.0)

## Shows `content` in a centred modal panel over a dimmed backdrop. Esc closes it.
func open_modal(content: Control, title: String = "", min_size: Vector2 = Vector2(760, 0), tone: String = "primary") -> Control:
	var backdrop := ColorRect.new()
	backdrop.color = Color(0.01, 0.02, 0.05, 0.72)
	backdrop.set_anchors_preset(Control.PRESET_FULL_RECT)
	backdrop.mouse_filter = Control.MOUSE_FILTER_STOP
	var center := CenterContainer.new()
	center.set_anchors_preset(Control.PRESET_FULL_RECT)
	backdrop.add_child(center)
	var panel := Lcars.frame(title, "", tone) if title != "" else PanelContainer.new()
	panel.add_theme_stylebox_override("panel", Lcars.box(Color(Lcars.PANEL_SOLID, 0.98), 20, Color(Lcars.tone_color(tone), 0.6), 1, Vector4(22, 18, 22, 18)))
	panel.custom_minimum_size = min_size
	center.add_child(panel)
	var host: Control = Lcars.body(panel) if title != "" else panel
	host.add_child(content)
	_overlay.add_child(backdrop)
	_modals.append(backdrop)
	backdrop.gui_input.connect(func(ev):
		if ev is InputEventMouseButton and ev.pressed and ev.button_index == MOUSE_BUTTON_LEFT:
			close_modal(backdrop))
	return backdrop

## Confirmation dialog: runs `on_confirm` when accepted.
func confirm(title: String, body: String, confirm_label: String, on_confirm: Callable, tone: String = "alert") -> Control:
	var col := Lcars.vbox(14)
	col.name = "Confirm"
	col.add_child(Lcars.para(body, Lcars.TEXT2, 15))
	var row := Lcars.hbox(10)
	row.alignment = BoxContainer.ALIGNMENT_END
	row.add_child(Lcars.pill("Cancel", "ghost", func(): close_modal(), "", "", "Esc"))
	var ok := Lcars.pill(confirm_label, tone, func():
		close_modal()
		on_confirm.call())
	ok.name = "ConfirmButton"
	row.add_child(ok)
	col.add_child(row)
	var m := open_modal(col, title, Vector2(520, 0), tone)
	ok.grab_focus.call_deferred()
	return m

func close_modal(m: Control = null) -> void:
	if _modals.is_empty():
		return
	var target: Control = m if m else _modals.back()
	_modals.erase(target)
	target.queue_free()

func close_all_modals() -> void:
	for m in _modals:
		m.queue_free()
	_modals.clear()

func has_modal() -> bool:
	return not _modals.is_empty()

## Named overlays: briefing, events, victory, end, menu, settings, help.
func open_overlay(kind: String) -> Control:
	var o := preload("res://scenes/ui/screens/overlays.gd")
	return o.open(self, kind)

func open_menu() -> void:
	open_overlay("menu")

# ---------------------------------------------------------------------------------------------
# Keyboard
# ---------------------------------------------------------------------------------------------
func _unhandled_key_input(event: InputEvent) -> void:
	var k := event as InputEventKey
	if k == null or not k.pressed or k.echo:
		return
	var in_game := not view.is_empty() and not (screen in ["title", "new", "saves"])
	if k.keycode == KEY_ESCAPE:
		get_viewport().set_input_as_handled()
		if has_modal():
			close_modal()
		elif screen == "galaxy" and int(selection["fleetId"]) != 0:
			select_cell(int(selection["cell"]))
		elif screen == "galaxy":
			open_menu()
		elif screen == "combat" and encounter is Dictionary and not encounter.get("finished", false):
			return
		elif screen in ["new", "saves"] and view.is_empty():
			goto("title", false)
		else:
			back()
		return
	if has_modal():
		if k.keycode == KEY_ENTER and screen != "combat":
			close_modal()
			get_viewport().set_input_as_handled()
		return
	if not in_game:
		return
	if k.keycode >= KEY_F1 and k.keycode <= KEY_F8:
		goto(GAME_SCREENS[k.keycode - KEY_F1]["id"])
		get_viewport().set_input_as_handled()
	elif k.keycode == KEY_ENTER or k.keycode == KEY_KP_ENTER:
		if screen != "combat":
			request_end_turn()
			get_viewport().set_input_as_handled()
	elif k.unicode == 63: # '?'
		open_overlay("help")
		get_viewport().set_input_as_handled()
	elif k.keycode == KEY_S and k.ctrl_pressed:
		if save_game(save_name if save_name != "" else "Quicksave"):
			toast("Saved.", "good")
		get_viewport().set_input_as_handled()
	elif k.keycode == KEY_TAB and screen == "galaxy":
		_cycle_unit(-1 if k.shift_pressed else 1)
		get_viewport().set_input_as_handled()

func _cycle_unit(dir: int) -> void:
	var units := []
	for f in view.get("fleets", []):
		var tf := int(f.get("taskForceId", 0))
		if tf == 0:
			units.append(f)
		else:
			var t := find_by_id("taskForces", tf)
			if not t.is_empty() and int(t["fleetIds"][0]) == int(f["id"]):
				units.append(f)
	if units.is_empty():
		return
	var idx := -1
	for n in units.size():
		if int(units[n]["id"]) == int(selection["fleetId"]):
			idx = n
	var nxt: Dictionary = units[(idx + dir + units.size()) % units.size()]
	select_fleet(int(nxt["id"]))
	if _current and _current.has_method("focus_cell"):
		_current.focus_cell(int(nxt["x"]), int(nxt["y"]))
