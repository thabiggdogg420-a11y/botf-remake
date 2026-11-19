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
const SETTINGS_PATH := "user://botf_settings.json"   ## the default; AppSaves.SETTINGS_PATH is the live path (a capture sandbox moves it)

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
## MOD-9: main.gd sets this in a real run; the opening, race intros and Borg cutscenes play only then (settings "cinematics").
var cinematics_live := false
var settings := {"cinematics": true, "uiScale": 1.0, "colorblind": false, "reducedMotion": false, "autosave": true, "confirmEndTurn": false, "showGrid": true, "uiEra": "tng"}  ## plus vol_* (BotfAudioMix)
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
var advisor_panel: AdvisorPanel   ## F3 advisor and first-run tutorial
var _theme_empire := -1

func _init() -> void:
	i = self

func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	var t := LoadTrace.start()
	_load_settings()
	DisplaySettings.fill_defaults(settings)
	DisplaySettings.apply(settings, get_tree())   # window mode, resolution, vsync, FPS cap, render scale, AA, shadows
	t = LoadTrace.done("boot: settings", t)
	AppLoader.mount_user_packs()   # desktop asset bundles (planets, hero ships); the web fetches its own after the title
	PlanetTextures.reset()
	t = LoadTrace.done("boot: packs", t)
	var web := OS.has_feature("web")
	if web:
		# Web smoothness pass (19 Nov): the shell first, then one start-up step a frame behind the panel, never one long frame.
		_apply_theme(1)
		_build_layers()
		_show_loading("Starting", [])
		_loading_label.text = "Starting"
		await get_tree().process_frame
		t = LoadTrace.start()
	BotfAudioSystem.attach(self)   # audio: shares settings, listens to screen changes (music, screen cue)
	t = LoadTrace.done("boot: audio", t)
	if web:
		await get_tree().process_frame
		t = LoadTrace.start()
	session.connect_core(CoreClient.create("fixture" if forced_backend == "fixture" else "auto"))
	t = LoadTrace.done("boot: core", t)
	session.view_changed.connect(_on_view_changed)
	session.selection_changed.connect(func(): selection_changed.emit())
	session.notified.connect(func(text, tone): toast(text, tone))
	session.failed.connect(report_error)
	if web:
		await get_tree().process_frame
		t = LoadTrace.start()
		_hide_loading()
	else:
		_apply_theme(1)
		_build_layers()
	t = LoadTrace.done("boot: theme+layers", t)
	goto("title")
	LoadTrace.done("boot: title", t)
	if cinematics_on():
		Cinema.play_movie(self, "open")   # the disc's opening over the title; any key or click skips it
	_warm_battle.call_deferred()
	_warm_screens.call_deferred()
	if OS.has_feature("web"):
		_fetch_background.call_deferred()

## F5 performance pass: the eight powers' portraits are decoded off the main thread first (AppLoader.warm_art). The first
## visit to each screen used to compile its script on the main thread (New campaign 565 ms,
## Galaxy 273 ms, System 198 ms, Empire 147 ms, Diplomacy 119 ms on the desktop). After the title has drawn, the desktop
## build loads every screen script on a worker thread, one at a time, in the order a player meets them; goto() then finds
## them compiled. A screen opened before its turn simply waits for its load, as before. The web build has no worker
## threads and keeps loading on first visit.
const WARM_SCREENS := ["new", "galaxy", "system", "saves", "empire", "research", "fleets", "diplomacy", "intel", "database", "packs"]
var screens_warmed := 0   ## how many screen scripts the warm-up has loaded (tests read it)

func _warm_screens() -> void:
	if not AppLoader.threads() or DisplayServer.get_name() == "headless":
		return
	var tree := get_tree()
	for _i in 20:
		await tree.process_frame   # let the title draw first
	await AppLoader.warm_art(tree, PortraitSlot.MAJOR_KEYS.values(), "portrait")   # the New campaign screen's powers
	for id: String in WARM_SCREENS:
		var path := AppRoutes.script_for(id)
		if path == "" or not is_inside_tree():
			continue
		if AppLoader.cached(path):
			screens_warmed += 1
			continue
		if not AppLoader.threaded_start(path):
			continue
		while AppLoader.threaded_status(path) == 0:
			await tree.process_frame
			if not is_inside_tree():
				return
		var res := AppLoader.threaded_take(path)
		if res != null:
			_warm_refs.append(res)
			screens_warmed += 1

## Web only, after the title is up (the first playable load is the engine and the game's own pck alone). Only the small
## bundles come now: the planet tier, the portraits and the shared music (about 15 MB together). Hero hulls, empire audio and
## the full-resolution planets are lazy (F1 download budget): the hulls of the empires in a campaign start downloading in the
## background when it begins (_fetch_game_packs), any other faction's hulls are fetched when a battle needs them
## (_missing_ship_packs), and the big planet tier waits for the first galaxy. A pack already in the player's IndexedDB cache
## is mounted with no request. `web_packs_done` is set and emitted when the title's bundles are in.
signal web_packs_changed(name: String)
var web_packs_done := false

func _fetch_background() -> void:
	await AppLoader.fetch_web_packs(self)
	var t := LoadTrace.start()
	PlanetTextures.reset()
	web_packs_changed.emit("planets_web.pck")
	LoadTrace.done("title packs: planets+art in", t)
	if await AppLoader.fetch_web_pack(self, AppLoader.WEB_MUSIC):
		t = LoadTrace.start()
		BotfAudioSystem.music_pack_ready()
		LoadTrace.done("title packs: music ready", t)
	if await AppLoader.fetch_web_pack(self, AppLoader.WEB_SHIP_ICONS):
		t = LoadTrace.start()
		ShipIcons.reset()
		web_packs_changed.emit(AppLoader.WEB_SHIP_ICONS)
		LoadTrace.done("title packs: ship icons in", t)
	web_packs_done = true
	print("web packs: %.1f MB downloaded, %.1f MB from cache" % [AppLoader.bytes_downloaded / 1048576.0, AppLoader.bytes_cached / 1048576.0])
	web_packs_changed.emit("")
	_warm_service_worker()
	_idle_prefetch()

## Web: tells the service worker (web-host/sw.js) the title is up, so it caches the engine files for the next visit.
func _warm_service_worker() -> void:
	if OS.has_feature("web"):
		JavaScriptBridge.eval("(function(){var c=navigator.serviceWorker&&navigator.serviceWorker.controller;if(c)c.postMessage({type:'warm'});})()")

# ---- Idle prefetch (F1 audit, 17 Nov): while the player is on the title or the galaxy, the next packs come down quietly ----
const _IDLE_SCREENS := ["title", "new", "galaxy"]
var _idle_running := false
var _idle_failed := {}
var _game_packs_busy := false
var idle_fetched: Array = []   ## logical names the idle prefetch has brought in this session (for the verify report)

## True when nothing else wants the connection: a calm screen, no battle or preload download running, no campaign-start fetch.
func _idle_ok() -> bool:
	return _IDLE_SCREENS.has(screen) and (_loading_layer == null or not _loading_layer.visible) and not AppLoader.preload_running and not _game_packs_busy

## One pack at a time, only while `_idle_ok`. Leaves the preload button as the way to take everything else. Ends when the
## queue is empty and a campaign is running (the title keeps waiting: the empire is not known there).
func _idle_prefetch() -> void:
	if not OS.has_feature("web") or _idle_running:
		return
	_idle_running = true
	AppLoader.batch_sync = true
	await AppLoader.load_manifest(self)
	while true:
		var tok := ""
		var others: Array = []
		if not view.is_empty():
			tok = str(_EMPIRE_TOKENS.get(int(view.get("meta", {}).get("player", 0)), ""))
		for k in _EMPIRE_TOKENS:
			if _EMPIRE_TOKENS[k] != tok:
				others.append(_EMPIRE_TOKENS[k])
		var next := ""
		for n: String in AppLoader.idle_queue(tok, others):
			if not _idle_failed.has(n):
				next = n
				break
		if next == "":
			if not view.is_empty():
				break
			await get_tree().create_timer(2.0).timeout
			continue
		if not _idle_ok():
			await get_tree().create_timer(1.0).timeout
			continue
		if await AppLoader.fetch_web_pack(self, next):
			var t := LoadTrace.start()
			idle_fetched.append(next)
			if next == AppLoader.WEB_PLANETS_HI:
				PlanetTextures.reset()
			web_packs_changed.emit(next)
			LoadTrace.done("idle pack in: %s" % next, t)
		else:
			_idle_failed[next] = true
	AppLoader.batch_sync = false
	await AppLoader.flush_pending(self)
	_idle_running = false
	print("idle prefetch done: %d packs (%.1f MB downloaded, %.1f MB from cache so far)" % [idle_fetched.size(), AppLoader.bytes_downloaded / 1048576.0, AppLoader.bytes_cached / 1048576.0])

var _game_packs_started := false

## Web: a campaign has started. Fetches in the background, one at a time: the hull manifests, then the hull of each ship the
## player starts with (a few MB each), then the full-resolution planet tier (the galaxy and system screens swap to it when it
## lands). Every other hull comes when a battle needs it (_missing_ship_packs).
func _fetch_game_packs(v: Dictionary) -> void:
	if not OS.has_feature("web") or _game_packs_started:
		return
	_game_packs_started = true
	_game_packs_busy = true
	var player := int(v.get("meta", {}).get("player", 1))
	await AppLoader.load_manifest(self)
	var need: Array = []
	for f in v.get("fleets", []):
		if int(f.get("owner", 0)) == player:
			_add_hull_pack(need, {"owner": player, "name": str(f.get("name", "")), "assetKey": str(f.get("assetKey", ""))})
	if not need.is_empty() and await AppLoader.fetch_web_pack(self, AppLoader.WEB_SHIPS_BASE):
		web_packs_changed.emit(AppLoader.WEB_SHIPS_BASE)
	for name: String in need:
		if await AppLoader.fetch_web_pack(self, name):
			web_packs_changed.emit(name)
	if await AppLoader.fetch_web_pack(self, AppLoader.WEB_PLANETS_HI):
		PlanetTextures.reset()
		web_packs_changed.emit(AppLoader.WEB_PLANETS_HI)
	_game_packs_busy = false
	print("web packs: %.1f MB downloaded, %.1f MB from cache" % [AppLoader.bytes_downloaded / 1048576.0, AppLoader.bytes_cached / 1048576.0])
	web_packs_changed.emit("")

## Web: the ship packs the open encounter needs that are not mounted yet (logical names). Faction tokens come from the ships'
## asset keys (ship.<faction>.<class>, study.<faction>), so a minor race's or a threat's hulls are found too.
func _missing_ship_packs() -> Array:
	var out: Array = []
	if not OS.has_feature("web"):
		return out
	var enc: Variant = session.encounter
	if not (enc is Dictionary):
		return out
	for sd in enc.get("ships", []):
		if sd is Dictionary:
			_add_hull_pack(out, sd)
	if not out.is_empty() and not AppLoader.web_pack_mounted(AppLoader.WEB_SHIPS_BASE):
		out.push_front(AppLoader.WEB_SHIPS_BASE)
	return out

## Adds the hull pack of ship dictionary `sd` (owner, name, assetKey) to `out` unless mounted or already listed. The first asset
## key with a pack is the one BattleAssets.hero() would draw, so one pack per ship is enough.
func _add_hull_pack(out: Array, sd: Dictionary) -> void:
	for key in BattleShip.asset_keys_for(sd):
		var name := AppLoader.hull_pack_for(str(key))
		if name != "":
			if not AppLoader.web_pack_mounted(name) and not out.has(name):
				out.append(name)
			return

var _loading_layer: Control = null
var _loading_label: Label = null
var _loading_bar: ColorRect = null
var _loading_fill: ColorRect = null
var _loading_total := 1.0
var _combat_ready := false

## Web: opens the battle screen once its ship packs are in, with a modal progress panel so the player sees why it waits
## (the hulls are the only big files a battle can need). A failed download still opens the battle: those hulls are drawn
## as plain procedural ones.
func _open_combat_when_ready(remember: bool, need: Array) -> void:
	_show_loading("Loading ship models", need)
	for name: String in need:
		await AppLoader.fetch_web_pack(self, name)
		web_packs_changed.emit(name)
	_hide_loading()
	_combat_ready = true
	goto("combat", remember)
	_combat_ready = false

var _loading_title := ""

func _show_loading(title: String, need: Array) -> void:
	_loading_title = title
	if _loading_layer == null:
		_loading_layer = ColorRect.new()
		(_loading_layer as ColorRect).color = Color(0, 0, 0, 0.72)
		_loading_layer.set_anchors_preset(Control.PRESET_FULL_RECT)
		_loading_layer.mouse_filter = Control.MOUSE_FILTER_STOP
		var box := VBoxContainer.new()
		box.set_anchors_preset(Control.PRESET_CENTER)
		box.grow_horizontal = Control.GROW_DIRECTION_BOTH
		box.grow_vertical = Control.GROW_DIRECTION_BOTH
		box.custom_minimum_size = Vector2(460, 0)
		_loading_label = Label.new()
		_loading_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		_loading_bar = ColorRect.new()   # drawn by hand: the themed ProgressBar is invisible on the dark panel
		_loading_bar.custom_minimum_size = Vector2(460, 14)
		_loading_bar.color = Color(1, 1, 1, 0.18)
		_loading_fill = ColorRect.new()
		_loading_fill.color = Lcars.accent()
		_loading_fill.size = Vector2(0, 14)
		_loading_bar.add_child(_loading_fill)
		box.add_child(_loading_label)
		box.add_child(_loading_bar)
		_loading_layer.add_child(box)
		add_child(_loading_layer)
	_loading_layer.visible = true
	var total := 0
	for name: String in need:
		total += AppLoader.pack_bytes(name)
	_loading_label.text = "%s (%.0f MB)" % [title, total / 1048576.0]
	_loading_total = maxf(total, 1.0)
	_loading_fill.size.x = 0.0
	_loading_bar.set_meta("need", need)
	_loading_bar.set_meta("done_before", 0)
	set_process(true)

func _hide_loading() -> void:
	if _loading_layer != null:
		_loading_layer.visible = false

func _process(_delta: float) -> void:
	if _loading_layer == null or not _loading_layer.visible:
		set_process(false)
		return
	var need: Array = _loading_bar.get_meta("need", [])
	var done := 0
	for name: String in need:
		if AppLoader.web_pack_mounted(name):
			done += AppLoader.pack_bytes(name)
	if not AppLoader.progress.is_empty() and need.has(str(AppLoader.progress.get("name", ""))):
		done += int(AppLoader.progress.get("bytes", 0))
	_loading_fill.size.x = _loading_bar.size.x * clampf(done / _loading_total, 0.0, 1.0)
	_loading_label.text = "%s (%.1f of %.1f MB)" % [_loading_title, done / 1048576.0, _loading_total / 1048576.0]

func _title_empire() -> int:
	var t := current_screen()
	return int(t.get("fleet_empire")) if t != null and t.get("fleet_empire") != null else 1

## Loads the battle screen's scripts, scene, planet textures and effect materials one step per frame while the title and
## galaxy sit idle, so the first battle opens in a frame or two instead of a 300-500 ms stall (M1-BETAFIX; it was 1.5-4 s
## with hero models, see BattleAssets.mip_scene_images). The references are kept for the life of the process.
static var _warm_refs: Array = []
static var _warmed_battle := false

func _warm_battle() -> void:
	if _warmed_battle or DisplayServer.get_name() == "headless":
		return
	_warmed_battle = true
	var tree := get_tree()
	for _i in 30:
		await tree.process_frame # let the title draw first
	if not is_inside_tree():
		_warmed_battle = false
		return
	_warm_refs.append(AppLoader.resource(AppRoutes.script_for("combat")))
	await tree.process_frame
	var packed: PackedScene = AppLoader.resource(BattleHost.BATTLE_SCENE)
	_warm_refs.append(packed)
	await tree.process_frame
	# An off-screen battle view: compiles the sky, planet and effect shaders and uploads the backdrop planet's textures.
	var sv := SubViewport.new()
	sv.size = Vector2i(256, 144)
	sv.own_world_3d = true
	sv.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	# F2: the battle host's own viewport settings (anti-aliasing, render scale), so the pipelines compiled here are the
	# ones the battle draws with (a different MSAA mode compiled them again on the first battle frame: 90-100 ms).
	DisplaySettings.apply_viewport(sv, settings)
	add_child(sv)
	var view := packed.instantiate()
	sv.add_child(view)
	await tree.process_frame
	if "_env" in view:
		BattleEnvironment.set_planet_class(view._env, "M")
	await tree.process_frame
	if "fx" in view and view.fx != null:
		view.fx.prewarm()
	# Ship shaders (hero hull, plating, hull, shield, cloak, blink): a sphere each, in front of the warm-up camera, so
	# their pipelines compile now and not when the first hull of a battle is drawn.
	var warm_meshes: Array = []
	for sh in [HeroLook.HULL_SHADER, HeroLook.PLATED_SHADER, BattleShip.HULL_SHADER, BattleShip.SHIELD_SHADER, BattleShip.CLOAK_SHADER, BattleShip.BLINK_SHADER]:
		var mi := MeshInstance3D.new()
		warm_meshes.append(mi)
		mi.mesh = SphereMesh.new()
		var mat := ShaderMaterial.new()
		mat.shader = sh
		mi.material_override = mat
		view.add_child(mi)
		if "camera" in view and view.camera != null:
			mi.global_position = view.camera.global_position + (view.camera.global_transform.basis * Vector3(0, 0, -3.0))
	BattleShip.flame_flipbook()
	BattleShip.puff_texture()
	for _i in 6:
		await tree.process_frame
	# F2: keep the warmed view for the first battle (BattleHost takes it): instancing a fresh one when the battle opened
	# cost 50-60 ms on that frame, and its first draw compiled pipelines again.
	for mi in warm_meshes:
		if is_instance_valid(mi):
			mi.queue_free()
	await tree.process_frame
	if is_instance_valid(view) and _spare_battle_view == null:
		sv.remove_child(view)
		_spare_battle_view = view
	elif is_instance_valid(view):
		view.queue_free()
	# F2: rasterise the UI fonts' glyphs at the sizes the screens use, one font a frame, in the same off-screen viewport:
	# the battle screen's first draw rendered them (60-70 ms on the frame it opened).
	if not _glyphs_warmed:
		_glyphs_warmed = true
		var canvas := Control.new()
		canvas.size = Vector2(sv.size)
		sv.add_child(canvas)
		var text := ""
		for c in range(32, 127):
			text += char(c)
		text += "●◇▸▲▼✓✗✕→·—"
		for f: Font in Lcars._fonts.values():
			if f == null:
				continue
			var cb := func():
				for px in [10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 24, 26, 28, 30]:
					canvas.draw_string(f, Vector2(0, 40), text, HORIZONTAL_ALIGNMENT_LEFT, -1, px)
			canvas.draw.connect(cb)
			canvas.queue_redraw()
			await tree.process_frame
			await tree.process_frame
			canvas.draw.disconnect(cb)
	sv.queue_free()

static var _glyphs_warmed := false

static var _spare_battle_view: Node = null

## The warmed, never played battle view, once (null when there is none): BattleHost mounts it instead of a new instance.
static func take_spare_battle_view() -> Node:
	var v := _spare_battle_view
	_spare_battle_view = null
	if v != null and not is_instance_valid(v):
		return null
	if v != null:
		_warmed_battle = false # the next idle stretch on the galaxy warms another for the next battle
	return v

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
	advisor_panel = AdvisorPanel.make(self)   # F3: above modals so "Close log" can be pressed while the log is open
	add_child(advisor_panel)
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
		b.text = TrekText.rail_label(player(), str(s["id"]), str(s["label"])).to_upper()
		b.alignment = HORIZONTAL_ALIGNMENT_RIGHT
		b.custom_minimum_size = Vector2(RAIL_W, 50)
		b.tooltip_text = "%s  [%s]" % [s["hint"], s["key"]]
		b.name = "Rail_" + s["id"]
		var sid: String = s["id"]
		b.pressed.connect(func(): goto(sid))
		_rail_era_style(b, GAME_SCREENS.find(s), s["id"] == screen, 50)
		if s["id"] == screen and UiTokens.era in ["tng", "voy", "late"]:
			var sb := Lcars.box(Lcars.accent2(), 0, Color.TRANSPARENT, 0, Vector4(14, 8, 14, 8))
			sb.corner_radius_top_right = 18
			sb.corner_radius_bottom_right = 18
			b.add_theme_stylebox_override("normal", sb)
			b.add_theme_stylebox_override("hover", sb)
		var keyl := Lcars.label(s["key"], "Mono", 11, Color(Lcars.ink(), 0.7))
		keyl.position = Vector2(8, 4)
		keyl.mouse_filter = Control.MOUSE_FILTER_IGNORE
		b.add_child(keyl)
		_rail_icon(b, str(ShellVM.SCREEN_ICONS.get(s["id"], "arrow_right")), 18, Vector2(5, 21))
		_rail.add_child(b)
	var fill := ColorRect.new()
	fill.color = a.darkened(0.45)
	fill.custom_minimum_size = Vector2(RAIL_W, 8)
	_rail.add_child(fill)
	for ex in shell.rail_extras():
		var b2 := Button.new()
		b2.theme_type_variation = "RailButton"
		b2.text = TrekText.rail_label(player(), str(ex["id"]), str(ex["label"])).to_upper()
		b2.alignment = HORIZONTAL_ALIGNMENT_RIGHT
		b2.custom_minimum_size = Vector2(RAIL_W, 38)
		b2.add_theme_font_size_override("font_size", 16)
		var sb2 := Lcars.box(Lcars.accent3().darkened(0.2), 0, Color.TRANSPARENT, 0, Vector4(14, 4, 14, 4))
		sb2.corner_radius_top_right = 14
		sb2.corner_radius_bottom_right = 14
		b2.add_theme_stylebox_override("normal", sb2)
		_rail_era_style(b2, _rail.get_child_count() + 2, false, 38)
		b2.tooltip_text = ex["hint"] + ("  [%s]" % ex["key"] if str(ex.get("key", "")) != "" else "")
		_rail_icon(b2, str(ex.get("icon", "arrow_right")), 18, Vector2(5, 7))
		var xid: String = ex["id"]
		b2.pressed.connect(func(): goto(xid) if xid == "combat" else open_overlay(xid))
		_rail.add_child(b2)

## A rail icon on its pill (Simon, 18 Nov: the navy icons were hard to read on the blue and lavender pills). The two-tone icon
## is drawn light on a dark ink chip, so both tones read against any pill colour, gold (active) or blue, lavender (idle).
func _rail_icon(b: Button, icon: String, px: float, at: Vector2) -> void:
	var chip := Panel.new()
	var cs := px + 6.0
	chip.position = at
	chip.size = Vector2(cs, cs)
	chip.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var sb := Lcars.box(Color(Lcars.ink(), 0.92), 0, Color.TRANSPARENT, 0, Vector4(0, 0, 0, 0))
	sb.set_corner_radius_all(8)
	chip.add_theme_stylebox_override("panel", sb)
	chip.name = "RailChip"
	b.add_child(chip)
	var ic := UiIcon.make(icon, px, Color(0.94, 0.97, 1.0))
	ic.position = Vector2(3, 3)
	ic.size = Vector2(px, px)
	ic.mouse_filter = Control.MOUSE_FILTER_IGNORE
	chip.add_child(ic)

## The rail button's shape per interface look: TOS gets square push-button blocks in the console's gold, red, blue and green with a
## black gap; the movies a flat grey-blue plate with a hairline; the later looks keep the LCARS tab from the theme.
func _rail_era_style(b: Button, index: int, active: bool, h: int) -> void:
	var era := str(UiTokens.era)
	if era != "tos" and era != "movie":
		return
	var base: Color
	if era == "tos":
		base = [UiTokens.era_hi(), UiTokens.era_lo(), Lcars.accent(), Color("#6fd98f"), Lcars.accent2(), UiTokens.era_hi(), Color("#c98ad8"), UiTokens.era_lo()][maxi(0, index) % 8]
	else:
		base = UiTokens.era_hi().darkened(0.55)
	var make := func(c: Color) -> StyleBoxFlat:
		var sbx := Lcars.box(c, 0, Color.TRANSPARENT, 0, Vector4(14, 8, 14, 8))
		if era == "movie":
			sbx.border_color = UiTokens.era_hi()
			sbx.set_border_width_all(1)
		return sbx
	var lit: Color = base if era == "tos" else UiTokens.era_hi().darkened(0.2)
	b.add_theme_stylebox_override("normal", make.call(lit if active else (base.darkened(0.2) if era == "tos" else base)))
	b.add_theme_stylebox_override("hover", make.call(base.lightened(0.15)))
	b.add_theme_stylebox_override("pressed", make.call(base.lightened(0.3)))

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
	if view.is_empty() or AppRoutes.is_bare(screen) or _chrome_folded:
		return Rect2(Vector2.ZERO, size)
	return Rect2(Vector2(RAIL_W + 14, TOP_H + 4), size - Vector2(RAIL_W + 14, TOP_H + 4))

## F2: while a battle round plays the top bar and the rail fold away and the battle fills the window; they come back
## for the plan phase (and whenever the screen changes).
var _chrome_folded := false
var _chrome_tween: Tween

func set_chrome_folded(on: bool) -> void:
	if on == _chrome_folded or _chrome == null:
		return
	_chrome_folded = on
	if _chrome_tween != null and _chrome_tween.is_valid():
		_chrome_tween.kill()
	if _current:
		_place_current()
	if on:
		_chrome_tween = create_tween()
		_chrome_tween.tween_property(_chrome, "modulate:a", 0.0, 0.2)
		_chrome_tween.tween_callback(func(): _chrome.visible = not _chrome_folded)
	else:
		_chrome.visible = true
		_chrome_tween = create_tween()
		_chrome_tween.tween_property(_chrome, "modulate:a", 1.0, 0.25)

func chrome_folded() -> bool:
	return _chrome_folded

func _exit_tree() -> void:
	if _spare_battle_view != null and is_instance_valid(_spare_battle_view):
		_spare_battle_view.free() # F2: the warmed view lives outside the tree; free it before the engine shuts down
	_spare_battle_view = null
	BattleAssets.wait_for_loads() # a hull still converting on a worker thread hangs the process at exit

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
	if id == "combat" and not _combat_ready:
		var need := _missing_ship_packs()   # web only: hulls of a faction not downloaded yet
		if not need.is_empty():
			_open_combat_when_ready(remember, need)
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
	if _chrome_folded:
		_chrome_folded = false
		_chrome.visible = true
		_chrome.modulate.a = 1.0
	close_all_modals()
	if _current:
		_current.queue_free()
		_current = null
	var path := AppRoutes.script_for(id)
	if path == "":
		return
	var _tg := Time.get_ticks_usec()
	var s: Control = (AppLoader.resource(path) as GDScript).new()
	var _tl := LoadTrace.done("goto %s: script+new" % id, _tg)
	s.name = "Screen_" + id
	s.set("app", self)
	_current = s
	_stage.add_child(s)
	_tl = LoadTrace.done("goto %s: add_child" % id, _tl)
	_place_current()
	_background.visible = id != "galaxy"
	if id == "galaxy" and not _warmed_battle:
		_warm_battle.call_deferred() # F2: a spare battle view for the next battle
	_build_chrome()
	_tl = LoadTrace.done("goto %s: chrome" % id, _tl)
	if s.has_method("refresh"):
		s.refresh()
	_tl = LoadTrace.done("goto %s: refresh" % id, _tl)
	Motion.enter(s, 0, "screen")
	screen_changed.emit(id)
	LoadTrace.done("goto %s: signals" % id, _tl)
	if OS.get_environment("ROUND_TIMING") != "":
		print("ROUND_TIMING goto %s frame %d: %d ms" % [id, Engine.get_process_frames(), (Time.get_ticks_usec() - _tg) / 1000])

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
const _EMPIRE_TOKENS := {1: "fed", 2: "kli", 3: "rom", 4: "car", 5: "fer"}
var _empire_audio := {}

## Web only: downloads and reads one empire's audio pack (shared audio is already in the main pck). Idempotent.
func _fetch_empire_audio(player: int) -> void:
	var tok: String = BotfAudioCues.faction_prefix(player)   # the five and the Dominion (MOD-9 voice pack)
	if tok == "" or _empire_audio.has(tok) or not OS.has_feature("web"):
		return
	_empire_audio[tok] = true
	if await AppLoader.fetch_empire_audio(self, tok):
		BotfAudioSystem.load_empire_pack(tok)

func _enter_game(v: Dictionary, name_: String) -> void:
	history.clear()
	save_name = name_
	session._adopt_pack_look(v)   # before the theme: the pack may recolour the empires (schema 25)
	_theme_empire = -1
	_apply_theme(int(v["meta"]["player"]))
	_fetch_empire_audio(int(v["meta"]["player"]))   # web: not awaited, the campaign starts while the pack downloads
	_fetch_game_packs(v)   # web: the empires' hulls in the background (not awaited)
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
	if cinematics_on():
		Cinema.race_intro(self, int(v["meta"]["player"]))   # MOD-9: the race's intro movie (the Dominion's drawn in-engine)
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
		# The screen we are leaving is not refreshed (its map rebuild cost ~30 ms of the battle-open frame).
		var leaving := _current
		_current = null
		refresh()
		_current = leaving
		session.fetch_encounter()
		if DisplayServer.get_name() != "headless":
			# F2: the battle screen is built on the frame after the core's end of turn (both on one frame were the
			# battle-open hitch); the galaxy stays on screen, unchanged, for that one frame.
			_open_combat_next_frame.call_deferred()
		else:
			goto("combat")
		get_tree().create_timer(0.25).timeout.connect(BotfAudioSystem.on_battle_joined) # off the frame that opens the screen
		get_tree().create_timer(0.3).timeout.connect(func(): toast("Battle joined. Issue maneuvers or let the tactical AI command.", "warn"))
		return "battle"
	briefing = res.get("briefing")
	refresh()
	BotfAudioSystem.on_turn_briefing(briefing, view, str(view.get("meta", {}).get("outcome", "ongoing")) != "ongoing")
	if screen == "combat":
		goto("galaxy", false)
	autosave()
	var outcome := str(view.get("meta", {}).get("outcome", "ongoing"))
	_borg_cutscene(briefing)
	if outcome != "ongoing":
		open_overlay("end")
	elif not view.get("aftermaths", []).is_empty():
		open_overlay("aftermath") # A conquest awaits its decision; the modal links to the briefing.
	elif briefing is Dictionary:
		open_overlay("briefing")
	return "complete"

## MOD-9 cinematics gate: a real run (main.gd) with the setting on.
func cinematics_on() -> bool:
	return cinematics_live and bool(settings.get("cinematics", true))

## MOD-9 Borg cutscenes from the turn's briefing: a Borg vessel entering the quadrant, or the Borg assimilating one of the
## player's worlds. At most one a turn (the arrival first), each over the screens the turn opens. Returns the kind played.
func _borg_cutscene(b: Variant) -> String:
	var pick := borg_cutscene_for(b, str(view.get("meta", {}).get("playerName", "")))
	if pick.is_empty() or not cinematics_on():
		return str(pick.get("kind", ""))
	Cinema.borg(self, pick.kind, pick.asset, pick.caption)
	return pick.kind

## Pure: {kind, asset, caption} for the briefing's Borg moment, {} when none. player_name: the player's empire name.
static func borg_cutscene_for(b: Variant, player_name: String) -> Dictionary:
	if not (b is Dictionary):
		return {}
	var arrival := {}
	var lost := {}
	for e in b.get("events", []):
		var text := str(e.get("text", ""))
		if arrival.is_empty() and text.contains("Borg") and text.ends_with("detected entering the quadrant."):
			var sphere := text.contains("Borg sphere")
			arrival = {"kind": "arrival", "asset": "ship.borg.sphere" if sphere else "ship.borg.tactical_cube",
				"caption": ("Borg sphere" if sphere else "Borg cube") + " detected entering the quadrant"}
		elif lost.is_empty() and text.begins_with("The Borg have assimilated ") and player_name != "" and text.contains("(" + player_name + ")"):
			var place := text.trim_prefix("The Borg have assimilated ").get_slice(" (", 0)
			lost = {"kind": "assimilation", "asset": "ship.borg.tactical_cube", "caption": "The Borg have assimilated " + place}
	return arrival if not arrival.is_empty() else lost

func _open_combat_next_frame() -> void:
	await get_tree().process_frame
	if screen != "combat" and encounter is Dictionary and not encounter.is_empty():
		goto("combat")

# ---------------------------------------------------------------------------------------------
# Saves and settings
# ---------------------------------------------------------------------------------------------
func save_game(name_: String) -> bool:
	var blob := session.save_blob()
	if blob.is_empty():
		return false
	var thumb := AppSaves.thumbnail(get_viewport())
	if thumb != "":
		blob["thumb"] = thumb
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

## F5 (20 Nov 2026): three rolling autosave slots, one per turn in turn (Autosave 1-3), so a bad turn can be stepped back
## from; each carries a thumbnail of the screen at the save.
func autosave() -> void:
	if settings.get("autosave", true) and not view.is_empty():
		save_game(AppSaves.autosave_name(int(view.get("meta", {}).get("turn", 0))))

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
	var sb := Lcars.box_lcars(Color(Lcars.PANEL_SOLID, 0.96), 12, c, 1, Vector4(16, 10, 16, 10))
	sb.border_width_left = 6
	p.add_theme_stylebox_override("panel", sb)
	var l := Lcars.para(text, Lcars.TEXT, 15)
	l.custom_minimum_size.x = 380
	var trow := Lcars.hbox(10)
	var ticon: String = {"alert": "alert", "warn": "warn", "good": "good", "info": "info"}.get(tone, "padd")
	trow.add_child(UiIcon.make(ticon, 20, c))
	trow.add_child(l)
	p.add_child(trow)
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
	panel.add_theme_stylebox_override("panel", Lcars.box_lcars(Color(Lcars.PANEL_SOLID, 0.98), 20, Color(Lcars.tone_color(tone), 0.6), 1, Vector4(22, 18, 22, 18)))
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
	autosave()
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
		elif (screen in ["new", "saves"] and view.is_empty()) or screen == "packs":
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
