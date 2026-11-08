class_name App
extends Control
## The shell of the strategic client: the LCARS chrome (top bar, rail), screen switching, overlays, toasts and
## keyboard shortcuts. Game state lives in an AppSession (scripts/app_session.gd); the fields below forward to it so
## older callers (galaxy, battle, tests) keep working. Screens are presentation scripts with `app` and `refresh()`,
## each fed by a view model (scripts/viewmodels). Layering: docs/CLIENT-ARCHITECTURE.md.

signal view_changed
signal selection_changed
signal screen_changed(id: String)

static var i: App
## Test hook: "fixture" forces the fixture backend, "" picks BotfCore when present.
static var forced_backend := ""

const GAME_SCREENS := AppRoutes.GAME_SCREENS
const OUTSIDE_SCREENS := AppRoutes.OUTSIDE_SCREENS
const SETTINGS_PATH := AppSaves.SETTINGS_PATH

## Game state: core, meta, view, selection, briefing, encounter and the order API.
var session := AppSession.new()

var core: CoreClient:
	get: return session.core
	set(v): session.core = v
var meta: Dictionary:
	get: return session.meta
	set(v): session.meta = v
var view: Dictionary:
	get: return session.view
	set(v): session.view = v
var encyclopedia: Dictionary:
	get: return session.encyclopedia
	set(v): session.encyclopedia = v
var selection: Dictionary:
	get: return session.selection
	set(v): session.selection = v
var briefing: Variant:
	get: return session.briefing
	set(v): session.briefing = v
var encounter: Variant:
	get: return session.encounter
	set(v): session.encounter = v

var screen := ""
var history: Array[String] = []
var save_name := ""
var settings := {"uiScale": 1.0, "colorblind": false, "reducedMotion": false, "autosave": true, "confirmEndTurn": false, "showGrid": true, "uiEra": "tng"}  ## plus vol_* (BotfAudioMix)
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
	DisplaySettings.fill_defaults(settings)
	DisplaySettings.apply(settings, get_tree())   # window mode, resolution, vsync, FPS cap, render scale, AA, shadows
	BotfAudioSystem.attach(self)   # audio: shares settings, listens to screen changes (music, screen cue)
	session.connect_core(CoreClient.create("fixture" if forced_backend == "fixture" else "auto"))
	session.view_changed.connect(_on_view_changed)
	session.selection_changed.connect(func(): selection_changed.emit())
	session.notified.connect(func(text, tone): toast(text, tone))
	session.failed.connect(report_error)
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
	mat.shader = AppLoader.resource("res://scenes/galaxy/starfield.gdshader")
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
	UiTokens.reduced_motion = settings.get("reducedMotion", false)
	var era_id := str(settings.get("uiEra", "tng"))
	if era_id != UiTokens.era:
		UiTokens.era = era_id if UiTokens.ERAS.has(era_id) else "tng"
		_theme_empire = -1
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
	if view.is_empty() or AppRoutes.is_bare(screen):
		return
	var a := Lcars.accent()
	# Elbow (top-left LCARS sweep) with the faction emblem.
	var elbow := LcarsElbow.new()
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
	var shell := ShellVM.new(session)
	var head := shell.heading()
	var who := Lcars.vbox(0)
	who.add_child(Lcars.label(head[0], "Heading", 26))
	who.add_child(Lcars.caption(head[1]))
	row.add_child(who)
	for st in shell.stats():
		row.add_child(Lcars.kpi(st["icon"], st["cap"], st["value"], st["delta"], st["tone"], st["explain"], true))
	row.add_child(Lcars.spacer())
	var os_ := shell.orders_stat()
	row.add_child(Lcars.kpi(os_["icon"], os_["cap"], os_["value"], "", os_["tone"], os_["explain"], true))
	var pal := Lcars.icon_button("", "Command palette: jump to any screen, colony, fleet or command  [Ctrl+K]", "", func(): open_palette())
	pal.name = "PaletteButton"
	pal.icon = null
	pal.custom_minimum_size = Vector2(40, 40)
	pal.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	pal.add_child(UiIcon.make("search", 22, Lcars.TEXT2))
	(pal.get_child(0) as Control).position = Vector2(9, 9)
	row.add_child(pal)
	var menu := Lcars.pill("Menu", "tertiary", func(): open_menu(), "", "Save, load, settings and help", "Esc")
	menu.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	row.add_child(menu)
	var end := Lcars.pill("End turn", "secondary", func(): request_end_turn(), shell.end_turn_block(), "Resolve all drafted orders", "Enter")
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
		keyl.mouse_filter = Control.MOUSE_FILTER_IGNORE
		b.add_child(keyl)
		var ic := UiIcon.make(ShellVM.SCREEN_ICONS.get(s["id"], "arrow_right"), 18, Color(Lcars.ink(), 0.8))
		ic.position = Vector2(10, 22)
		ic.size = Vector2(18, 18)
		b.add_child(ic)
		_rail.add_child(b)
	var fill := ColorRect.new()
	fill.color = a.darkened(0.45)
	fill.custom_minimum_size = Vector2(RAIL_W, 8)
	_rail.add_child(fill)
	for ex in shell.rail_extras():
		var b2 := Button.new()
		b2.theme_type_variation = "RailButton"
		b2.text = str(ex["label"]).to_upper()
		b2.alignment = HORIZONTAL_ALIGNMENT_RIGHT
		b2.custom_minimum_size = Vector2(RAIL_W, 38)
		b2.add_theme_font_size_override("font_size", 16)
		var sb2 := Lcars.box(Lcars.accent3().darkened(0.2), 0, Color.TRANSPARENT, 0, Vector4(14, 4, 14, 4))
		sb2.corner_radius_top_right = 14
		sb2.corner_radius_bottom_right = 14
		b2.add_theme_stylebox_override("normal", sb2)
		b2.tooltip_text = ex["hint"] + ("  [%s]" % ex["key"] if str(ex.get("key", "")) != "" else "")
		var ic2 := UiIcon.make(str(ex.get("icon", "arrow_right")), 16, Color(Lcars.ink(), 0.8))
		ic2.position = Vector2(10, 11)
		ic2.size = Vector2(16, 16)
		b2.add_child(ic2)
		var xid: String = ex["id"]
		b2.pressed.connect(func(): goto(xid) if xid == "combat" else open_overlay(xid))
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
	if view.is_empty() or AppRoutes.is_bare(screen):
		return Rect2(Vector2.ZERO, size)
	return Rect2(Vector2(RAIL_W + 14, TOP_H + 4), size - Vector2(RAIL_W + 14, TOP_H + 4))

func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED and _chrome:
		_build_chrome()
		if _current:
			_place_current()

# ---------------------------------------------------------------------------------------------
# Navigation
# ---------------------------------------------------------------------------------------------
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
	var path := AppRoutes.script_for(id)
	if path == "":
		return
	var s: Control = (AppLoader.resource(path) as GDScript).new()
	s.name = "Screen_" + id
	s.set("app", self)
	_current = s
	_stage.add_child(s)
	_place_current()
	_background.visible = id != "galaxy"
	_build_chrome()
	if s.has_method("refresh"):
		s.refresh()
	Motion.enter(s, 0, "screen")
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
	session.refresh()

func _on_view_changed() -> void:
	_apply_theme(int(view["meta"]["player"]))
	_build_chrome()
	view_changed.emit()
	if _current and _current.has_method("refresh"):
		_current.refresh()

func player() -> int:
	return session.player()

func width() -> int:
	return session.width()

func get_encyclopedia() -> Dictionary:
	return session.get_encyclopedia()

func find_by_id(list_key: String, id: int, key: String = "id") -> Dictionary:
	return session.find_by_id(list_key, id, key)

func system_at(x: int, y: int) -> Dictionary:
	return session.system_at(x, y)

func empire_name(id: int) -> String:
	return session.empire_name(id)

func select_cell(cell: int) -> void:
	session.select_cell(cell)

func select_system(system_id: int) -> void:
	session.select_system(system_id)

func select_fleet(fleet_id: int) -> void:
	session.select_fleet(fleet_id)

## Submits an order through the core; on rejection shows the core's reason.
func issue(command: Dictionary, cancel_ids: Array = [], success: String = "") -> bool:
	return session.issue(command, cancel_ids, success)

func cancel_orders(ids: Array, message: String = "") -> void:
	session.cancel_orders(ids, message)

## The core's reason a command would be rejected, or "" when it is valid.
func dry_run(command: Dictionary, cancel_ids: Array = []) -> String:
	return session.dry_run(command, cancel_ids)

func unit_order_ids(fleet_id: int, task_force_id: int, types: Array = []) -> Array:
	return session.unit_order_ids(fleet_id, task_force_id, types)

# ---------------------------------------------------------------------------------------------
# Campaign lifecycle
# ---------------------------------------------------------------------------------------------
func _enter_game(v: Dictionary, name_: String) -> void:
	history.clear()
	save_name = name_
	_apply_theme(int(v["meta"]["player"]))
	session.enter(v)
	screen = ""
	goto("galaxy", false)

## Starts a campaign. The player's power is options.faction; options.empire is accepted as an alias
## (AppSession.campaign_options).
func start_campaign(options: Dictionary) -> bool:
	var v := session.start("newCampaign", AppSession.campaign_options(options))
	if v.is_empty():
		return false
	_enter_game(v, "%s campaign %s" % [v["meta"]["playerName"], str(v["meta"]["seed"])])
	toast("Campaign begins. %s, turn %d." % [view["meta"]["playerName"], int(view["meta"]["turn"])], "good")
	autosave()
	return true

func start_drill(method: String, label_: String, args: Dictionary = {}) -> void:
	var v := session.start(method, args)
	if v.is_empty():
		return
	_enter_game(v, label_)
	toast(label_ + " loaded.", "warn")

func request_end_turn() -> void:
	if view.is_empty() or view.get("session", {}).get("active", false) and not view.get("session", {}).get("battleFinished", false):
		if screen != "combat" and view.get("session", {}).get("active", false):
			goto("combat")
		return
	var check := EndTurnCheck.build(self)
	if EndTurnCheck.should_prompt(self, check):
		EndTurnCheck.open(self, check)
		return
	BotfAudioSystem.cue_now("end_turn")
	end_turn()

## Shows the end-turn check without ending the turn (what the turn will do, and what is left idle).
func preview_turn() -> void:
	if view.is_empty():
		return
	EndTurnCheck.open(self, EndTurnCheck.build(self), true)

## Ends the turn. Returns the status ("battle" or "complete").
func end_turn() -> String:
	var res := session.end_turn()
	if res.is_empty():
		return "error"
	if res.get("status", "") == "battle":
		refresh()
		session.fetch_encounter()
		goto("combat")
		BotfAudioSystem.on_battle_joined()
		toast("Battle joined. Issue maneuvers or let the tactical AI command.", "warn")
		return "battle"
	briefing = res.get("briefing")
	refresh()
	BotfAudioSystem.on_turn_briefing(briefing, view)
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
	var blob := session.save_blob()
	if blob.is_empty():
		return false
	var err := AppSaves.write(blob, name_, str(view.get("meta", {}).get("playerName", "")))
	if err != "":
		report_error(err)
		return false
	save_name = name_
	return true

func list_saves() -> Array:
	return AppSaves.list()

func load_game(blob: Dictionary) -> bool:
	var v := session.start("load", {"data": blob.get("data", ""), "scenario": blob.get("scenario", "campaign")})
	if v.is_empty():
		return false
	_enter_game(v, str(blob.get("name", "Loaded campaign")))
	toast("Loaded %s, turn %d." % [blob.get("name", ""), int(view["meta"]["turn"])], "good")
	if view.get("session", {}).get("active", false):
		session.fetch_encounter()
		goto("combat")
	return true

func delete_save(file: String) -> void:
	AppSaves.delete(file)

func autosave() -> void:
	if settings.get("autosave", true) and not view.is_empty():
		save_game("Autosave")

func _load_settings() -> void:
	AppSaves.load_settings(settings)
	_apply_scale()

func save_settings() -> void:
	AppSaves.store_settings(settings)
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
	# The wait runs on the toast's own tween, so it dies with the toast. A SceneTree timer whose lambda captured `p`
	# logged "Lambda capture at index 0 was freed" whenever the overflow above freed that toast first.
	var tw := p.create_tween()
	tw.tween_interval(seconds)
	tw.tween_property(p, "modulate:a", 0.0, 0.3)
	tw.tween_callback(p.queue_free)

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
	BotfAudioSystem.cue_now("ui_open")
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

## Quits the program (title Quit, menu Quit game). Tests set quit_hook to observe it instead.
var quit_hook: Callable
func quit_game() -> void:
	if quit_hook.is_valid():
		quit_hook.call()
		return
	if settings.get("autosave", true) and not view.is_empty():
		save_game("Autosave")
	get_tree().quit()

## Named overlays: briefing, events, victory, end, menu, settings, help.
func open_overlay(kind: String) -> Control:
	return Overlays.open(self, kind)

func open_menu() -> void:
	open_overlay("menu")

## Ctrl+K command palette (docs/UI-DESIGN-SYSTEM.md §9). Items come from ShellVM.palette_items.
func open_palette() -> Control:
	if view.is_empty():
		return null
	close_all_modals()
	var pal := CommandPalette.make(ShellVM.new(session).palette_items())
	pal.ran.connect(run_palette)
	var m := open_modal(pal, "Command", Vector2(700, 0))
	pal.field.grab_focus.call_deferred()
	return m

## Runs a palette item id: screen:<id>, overlay:<kind>, colony:<systemId>, fleet:<fleetId>, cmd:<name>.
func run_palette(id: String) -> void:
	close_all_modals()
	var kind := id.get_slice(":", 0)
	var arg := id.get_slice(":", 1)
	match kind:
		"screen": goto(arg)
		"overlay": open_overlay(arg)
		"colony":
			select_system(int(arg))
			if screen == "system" and _current:
				_current.refresh()
			goto("system")
		"fleet":
			var f := find_by_id("fleets", int(arg))
			select_fleet(int(arg))
			goto("galaxy")
			if not f.is_empty() and _current and _current.has_method("focus_cell"):
				_current.focus_cell(int(f["x"]), int(f["y"]))
		"cmd":
			match arg:
				"saves": goto("saves")
				"quicksave":
					if save_game(save_name if save_name != "" else "Quicksave"):
						toast("Saved.", "good")
				"preview": preview_turn()
				"endturn": request_end_turn()

# ---------------------------------------------------------------------------------------------
# Keyboard
# ---------------------------------------------------------------------------------------------
func _unhandled_key_input(event: InputEvent) -> void:
	var k := event as InputEventKey
	if k == null or not k.pressed or k.echo:
		return
	var in_game := not view.is_empty() and not AppRoutes.is_bare(screen)
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
	var handled := true
	if k.keycode == KEY_K and k.ctrl_pressed:
		open_palette()
	elif k.keycode == KEY_F9:
		goto("saves")
	elif k.keycode == KEY_F10:
		open_overlay("settings")
	elif k.keycode == KEY_LEFT and k.alt_pressed:
		back()
	elif not k.ctrl_pressed and not k.alt_pressed and k.keycode == KEY_E:
		open_overlay("events")
	elif not k.ctrl_pressed and not k.alt_pressed and k.keycode == KEY_V:
		open_overlay("victory")
	elif not k.ctrl_pressed and not k.alt_pressed and k.keycode == KEY_C and screen != "combat":
		open_overlay("briefing")
	else:
		handled = false
	if handled:
		get_viewport().set_input_as_handled()
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
